#include "MapFile.hpp"

#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace mapfile {

namespace {

std::string SanitizeFilename(const std::string &name)
{
    std::string out;
    for (char c : name) {
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '-' || c == '_';
        out += ok ? c : '_';
    }
    if (out.empty()) out = "untitled";
    return out;
}

} // namespace

std::string MapsDirectory()
{
    const char *userProfile = std::getenv("USERPROFILE");
    std::string dir;
    if (userProfile != nullptr) {
        dir = std::string(userProfile) + "\\Documents\\3D Car Game\\Maps";
        // MakeDirectory only creates one level at a time -- build the
        // tree top-down so a missing "3D Car Game" folder doesn't make
        // the whole thing fail.
        std::string docGame = std::string(userProfile) + "\\Documents\\3D Car Game";
        if (!DirectoryExists(docGame.c_str())) MakeDirectory(docGame.c_str());
        if (!DirectoryExists(dir.c_str())) MakeDirectory(dir.c_str());
        if (DirectoryExists(dir.c_str())) return dir;
    }

    // Fall back to a local folder next to the exe -- saving should never
    // just silently fail because USERPROFILE wasn't set or Documents
    // couldn't be created.
    dir = "maps";
    if (!DirectoryExists(dir.c_str())) MakeDirectory(dir.c_str());
    return dir;
}

bool Save(const std::string &name, const track::TrackDef &def, int worldStyle)
{
    std::string path = MapsDirectory() + "\\" + SanitizeFilename(name) + ".cgmap";

    std::string buffer;
    char line[256];
    snprintf(line, sizeof(line), "cgmap_version %d\n", FORMAT_VERSION);
    buffer += line;
    snprintf(line, sizeof(line), "name %s\n", name.c_str());
    buffer += line;
    snprintf(line, sizeof(line), "description %s\n", def.description[0] ? def.description : "Custom map");
    buffer += line;
    snprintf(line, sizeof(line), "world_style %d\n", worldStyle);
    buffer += line;
    for (const Vector2 &c : def.control) {
        snprintf(line, sizeof(line), "control %.3f %.3f\n", c.x, c.y);
        buffer += line;
    }
    for (const Vector2 &g : def.fenceGaps) {
        snprintf(line, sizeof(line), "gap %.4f %.4f\n", g.x, g.y);
        buffer += line;
    }

    return SaveFileText(path.c_str(), (char *)buffer.c_str());
}

LoadResult Load(const std::string &path)
{
    LoadResult result;

    if (!FileExists(path.c_str())) {
        result.error = "File not found";
        return result;
    }

    char *text = LoadFileText(path.c_str());
    if (text == nullptr) {
        result.error = "Could not read file";
        return result;
    }

    // Static strings (TrackDef::name/description are const char* -- same
    // convention as the other built-in tracks, so Get(TRACK_CUSTOM) can
    // keep returning a valid pointer after this function returns).
    static std::string s_name, s_description;
    s_name = "Custom Track";
    s_description = "";

    std::vector<Vector2> control;
    std::vector<Vector2> gaps;
    int worldStyle = 0;
    int version = 0;
    bool sawVersion = false;

    const char *p = text;
    while (*p) {
        char token[32];
        if (sscanf(p, "%31s", token) == 1) {
            if (strcmp(token, "cgmap_version") == 0) {
                sscanf(p, "%*s %d", &version);
                sawVersion = true;
            } else if (strcmp(token, "name") == 0) {
                const char *rest = p + strlen(token);
                while (*rest == ' ') rest++;
                const char *nl = strchr(rest, '\n');
                s_name = nl ? std::string(rest, nl - rest) : std::string(rest);
            } else if (strcmp(token, "description") == 0) {
                const char *rest = p + strlen(token);
                while (*rest == ' ') rest++;
                const char *nl = strchr(rest, '\n');
                s_description = nl ? std::string(rest, nl - rest) : std::string(rest);
            } else if (strcmp(token, "world_style") == 0) {
                sscanf(p, "%*s %d", &worldStyle);
            } else if (strcmp(token, "control") == 0) {
                Vector2 c;
                if (sscanf(p, "%*s %f %f", &c.x, &c.y) == 2) control.push_back(c);
            } else if (strcmp(token, "gap") == 0) {
                Vector2 g;
                if (sscanf(p, "%*s %f %f", &g.x, &g.y) == 2) gaps.push_back(g);
            }
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    UnloadFileText(text);

    if (!sawVersion) {
        result.error = "Not a .cgmap file (missing cgmap_version)";
        return result;
    }
    if (control.size() < 4) {
        result.error = "Too few control points (need at least 4)";
        return result;
    }

    result.ok = true;
    result.trackDef.name = s_name.c_str();
    result.trackDef.description = s_description.c_str();
    result.trackDef.control = control;
    result.trackDef.fenceGaps = gaps;
    result.worldStyle = worldStyle;
    return result;
}

std::vector<std::string> ListMapFiles()
{
    std::vector<std::string> out;
    std::string dir = MapsDirectory();

    FilePathList files = LoadDirectoryFilesEx(dir.c_str(), ".cgmap", false);
    for (unsigned int i = 0; i < files.count; i++) {
        out.push_back(files.paths[i]);
    }
    UnloadDirectoryFiles(files);

    std::sort(out.begin(), out.end(), [](const std::string &a, const std::string &b) {
        return GetFileModTime(a.c_str()) > GetFileModTime(b.c_str());
    });
    return out;
}

} // namespace mapfile
