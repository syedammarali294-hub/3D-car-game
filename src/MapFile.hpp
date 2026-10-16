#pragma once

#include "Track.hpp"
#include <string>
#include <vector>

// .cgmap file format: plain text, name-keyed lines (never positional --
// see Input.cpp's history for exactly why positional save formats are
// fragile), holding one track's control points + fence gaps + world
// style + spawn heading. Saved to Documents\3D Car Game\Maps\ by default.
namespace mapfile {

constexpr int FORMAT_VERSION = 1;

struct LoadResult {
    bool ok = false;
    std::string error;        // set when ok == false
    track::TrackDef trackDef; // set when ok == true
    int worldStyle = 0;       // terrain::Style, as an int to avoid a Terrain.hpp dependency here
};

// Documents\3D Car Game\Maps\ (created if it doesn't exist yet). Falls
// back to a local "maps\" folder next to the executable if the user's
// Documents folder can't be resolved, so saving never just silently fails.
std::string MapsDirectory();

// LoadResult::trackDef.name/description point into static storage internal
// to Load() -- valid until the NEXT Load() call, which is long enough for
// the intended use (call Load(), then immediately track::SetCustom(result.
// trackDef) to copy the control points/gaps data out), but don't hold onto
// a LoadResult across a second Load() call expecting the name to stay put.
LoadResult Load(const std::string &path);

// name becomes both the track's display name and (sanitised: spaces and
// anything that isn't alnum/-/_ replaced with '_') the filename.
bool Save(const std::string &name, const track::TrackDef &def, int worldStyle);

// Full paths of every .cgmap file in MapsDirectory(), newest first.
std::vector<std::string> ListMapFiles();

} // namespace mapfile
