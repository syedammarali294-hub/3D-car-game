// Standalone top-down track editor -- a separate .exe from the game itself
// (same pattern as Unity/Unreal's own editor vs. the built player). Lays
// out a track's road control points and fence gaps on a flat 2D canvas and
// saves/loads the exact same .cgmap files the game's Maps picker reads
// (see MapFile.hpp), so nothing round-trips through a different format.
//
// This is deliberately scoped to what .cgmap can actually hold today:
// road shape, fence gaps, and world style. It does NOT place trees, water,
// or sculpt terrain -- those are driven by Terrain.cpp's procedural style
// and World.cpp's scattering, not per-map data, so there is nothing in the
// file format yet for a 3D prop/terrain editor to write to. Extending
// .cgmap to carry that is its own follow-up, not something to half-wire
// here.

#include "raylib.h"
#include "raymath.h"
#include "MapFile.hpp"
#include "Track.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

enum Mode { MODE_ROAD = 0, MODE_GAPS };

const char *StyleName(int s)
{
    switch (s) {
        case 0: return "Hills";
        case 1: return "Slopes";
        case 2: return "Flat";
        case 3: return "Desert";
        default: return "?";
    }
}

// Same centripetal Catmull-Rom curve Road.cpp's BuildCentreline() actually
// drives the game from (see its own comment for why centripetal, not
// uniform, matters) -- the editor's straight-segment polyline is a
// placement aid, not what the track will really look like, so the preview
// re-derives the exact real curve rather than approximating it.
constexpr int PREVIEW_STEPS_PER_SEGMENT = 16;

Vector2 CentripetalCatmullRom(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float u)
{
    constexpr float ALPHA = 0.5f;
    auto knotDelta = [ALPHA](Vector2 a, Vector2 b) {
        float d = Vector2Distance(a, b);
        return (d < 1e-4f) ? 1.0f : powf(d, ALPHA);
    };

    float t0 = 0.0f;
    float t1 = t0 + knotDelta(p0, p1);
    float t2 = t1 + knotDelta(p1, p2);
    float t3 = t2 + knotDelta(p2, p3);

    float t = Lerp(t1, t2, u);

    Vector2 a1 = Vector2Lerp(p0, p1, (t - t0) / (t1 - t0));
    Vector2 a2 = Vector2Lerp(p1, p2, (t - t1) / (t2 - t1));
    Vector2 a3 = Vector2Lerp(p2, p3, (t - t2) / (t3 - t2));

    Vector2 b1 = Vector2Lerp(a1, a2, (t - t0) / (t2 - t0));
    Vector2 b2 = Vector2Lerp(a2, a3, (t - t1) / (t3 - t1));

    return Vector2Lerp(b1, b2, (t - t1) / (t2 - t1));
}

std::vector<Vector2> BuildPreviewCurve(const std::vector<Vector2> &ctrl)
{
    std::vector<Vector2> out;
    int n = (int)ctrl.size();
    if (n < 4) return out;
    for (int i = 0; i < n; i++) {
        Vector2 p0 = ctrl[(i - 1 + n) % n];
        Vector2 p1 = ctrl[i];
        Vector2 p2 = ctrl[(i + 1) % n];
        Vector2 p3 = ctrl[(i + 2) % n];
        for (int s = 0; s < PREVIEW_STEPS_PER_SEGMENT; s++) {
            out.push_back(CentripetalCatmullRom(p0, p1, p2, p3, (float)s / (float)PREVIEW_STEPS_PER_SEGMENT));
        }
    }
    return out;
}

// World units <-> screen pixels. Pan/zoom live here so every draw/pick
// call goes through the same transform instead of two drifting copies.
struct View {
    Vector2 pan{ 0.0f, 0.0f }; // world-space point currently at screen centre
    float zoom = 0.35f;

    Vector2 ToScreen(Vector2 w, int screenW, int screenH) const
    {
        return Vector2{ (w.x - pan.x) * zoom + screenW * 0.5f, (w.y - pan.y) * zoom + screenH * 0.5f };
    }
    Vector2 ToWorld(Vector2 s, int screenW, int screenH) const
    {
        return Vector2{ (s.x - screenW * 0.5f) / zoom + pan.x, (s.y - screenH * 0.5f) / zoom + pan.y };
    }
};

// Nearest point on the closed control-point polyline to a world position,
// expressed as a fraction 0..1 around the loop -- same fenceGaps convention
// Track.hpp uses (fraction of the lap), just approximated with straight
// segments between control points rather than the game's Catmull-Rom
// curve. Good enough to place a gap in roughly the right spot; the curve
// itself is still built from the control points exactly as before.
float NearestFraction(const std::vector<Vector2> &ctrl, Vector2 w, float *outDist)
{
    int n = (int)ctrl.size();
    if (n < 2) { if (outDist) *outDist = 1e9f; return 0.0f; }

    float bestDist = 1e9f;
    float bestFrac = 0.0f;
    for (int i = 0; i < n; i++) {
        Vector2 a = ctrl[i];
        Vector2 b = ctrl[(i + 1) % n];
        Vector2 ab = Vector2Subtract(b, a);
        float len2 = Vector2DotProduct(ab, ab);
        float t = len2 > 0.0001f ? Clamp(Vector2DotProduct(Vector2Subtract(w, a), ab) / len2, 0.0f, 1.0f) : 0.0f;
        Vector2 proj = Vector2Add(a, Vector2Scale(ab, t));
        float d = Vector2Distance(w, proj);
        if (d < bestDist) {
            bestDist = d;
            bestFrac = ((float)i + t) / (float)n;
        }
    }
    if (outDist) *outDist = bestDist;
    return bestFrac;
}

// Simple blocking text prompt -- captures keystrokes into `out` until
// Enter/Escape. Draws over whatever's already on screen each frame so it
// doesn't need its own window or a second render pass.
bool TextPrompt(const char *title, std::string &out, int screenW, int screenH)
{
    out.clear();
    while (!WindowShouldClose()) {
        int key = GetCharPressed();
        while (key > 0) {
            if (key >= 32 && key < 127 && out.size() < 48) out += (char)key;
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE) && !out.empty()) out.pop_back();
        if (IsKeyPressed(KEY_ENTER)) return !out.empty();
        if (IsKeyPressed(KEY_ESCAPE)) return false;

        BeginDrawing();
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.75f));
        int tw = MeasureText(title, 28);
        DrawText(title, screenW / 2 - tw / 2, screenH / 2 - 60, 28, RAYWHITE);
        DrawRectangle(screenW / 2 - 260, screenH / 2 - 10, 520, 44, Color{ 30, 34, 44, 255 });
        DrawRectangleLines(screenW / 2 - 260, screenH / 2 - 10, 520, 44, Color{ 120, 150, 210, 255 });
        DrawText((out + "_").c_str(), screenW / 2 - 248, screenH / 2 + 2, 22, RAYWHITE);
        DrawText("Enter to confirm, Esc to cancel", screenW / 2 - 140, screenH / 2 + 50, 16, Fade(RAYWHITE, 0.6f));
        EndDrawing();
    }
    return false;
}

} // namespace

int main()
{
    // Same fix as CarGame3D's main() -- see its comment. This editor has
    // no relative asset loads today, but MapsDirectory() falling back to a
    // relative "maps\" folder (see MapFile.cpp) would land in the wrong
    // place if launched from a shortcut whose "Start in" differs from the
    // exe's own folder, so pin it down the same way regardless.
    ChangeDirectory(GetApplicationDirectory());

    const int screenW = 1280, screenH = 800;
    InitWindow(screenW, screenH, "3D Car Game - Map Editor");
    SetTargetFPS(60);

    std::vector<Vector2> control; // road control points, in order, closed loop
    std::vector<Vector2> gaps;    // fence gaps as (startFraction, endFraction)
    int worldStyle = 0;
    std::string mapName = "untitled";
    std::string status = "New map. 1: fence gaps  2: road points  N: new  O: open  S: save  Esc: quit";

    View view;
    Mode mode = MODE_ROAD;
    bool showPreview = true;  // smooth Catmull-Rom curve overlay, see BuildPreviewCurve above
    int dragIndex = -1;       // control point currently being dragged, -1 = none
    const float pickRadius = 14.0f; // screen pixels
    const float gapWidth = 0.04f;   // fraction of the lap a placed gap spans

    std::vector<std::string> mapPaths;
    int openCycle = -1;

    while (!WindowShouldClose()) {
        int w = GetScreenWidth(), h = GetScreenHeight();
        Vector2 mouse = GetMousePosition();
        bool overHud = mouse.y < 96.0f;

        if (IsKeyPressed(KEY_ONE)) mode = MODE_GAPS;
        if (IsKeyPressed(KEY_TWO)) mode = MODE_ROAD;
        if (IsKeyPressed(KEY_THREE)) worldStyle = (worldStyle + 1) % 4;
        if (IsKeyPressed(KEY_P)) showPreview = !showPreview;

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f && !overHud) {
            view.zoom = Clamp(view.zoom * (1.0f + wheel * 0.1f), 0.03f, 3.0f);
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 d = GetMouseDelta();
            view.pan.x -= d.x / view.zoom;
            view.pan.y -= d.y / view.zoom;
        }
        float panSpeed = 600.0f * GetFrameTime() / view.zoom * view.zoom; // screen-speed regardless of zoom
        panSpeed = 500.0f * GetFrameTime() / Clamp(view.zoom, 0.1f, 3.0f);
        if (IsKeyDown(KEY_LEFT))  view.pan.x -= panSpeed;
        if (IsKeyDown(KEY_RIGHT)) view.pan.x += panSpeed;
        if (IsKeyDown(KEY_UP))    view.pan.y -= panSpeed;
        if (IsKeyDown(KEY_DOWN))  view.pan.y += panSpeed;

        Vector2 worldMouse = view.ToWorld(mouse, w, h);

        if (!overHud && mode == MODE_ROAD) {
            // Pick an existing point under the cursor first -- drag it if
            // the mouse is held, delete it on right-click. Otherwise a
            // left-click appends a brand new point to the end of the loop.
            int hit = -1;
            for (int i = 0; i < (int)control.size(); i++) {
                if (Vector2Distance(view.ToScreen(control[i], w, h), mouse) <= pickRadius) { hit = i; break; }
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (hit >= 0) dragIndex = hit;
                else { control.push_back(worldMouse); status = TextFormat("%d control points", (int)control.size()); }
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && dragIndex >= 0) {
                control[dragIndex] = worldMouse;
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) dragIndex = -1;

            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hit >= 0) {
                control.erase(control.begin() + hit);
                status = TextFormat("%d control points", (int)control.size());
            }
        } else if (!overHud && mode == MODE_GAPS) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && control.size() >= 4) {
                float dist;
                float frac = NearestFraction(control, worldMouse, &dist);
                float start = frac - gapWidth * 0.5f;
                float end = frac + gapWidth * 0.5f;
                if (start < 0.0f) start += 1.0f;
                if (end > 1.0f) end -= 1.0f;
                gaps.push_back(Vector2{ start, end });
                status = TextFormat("%d fence gaps", (int)gaps.size());
            }
            if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !gaps.empty()) {
                // Remove whichever gap's centre fraction is closest to the
                // click, same nearest-point logic used to place one.
                float clickDist;
                float clickFrac = NearestFraction(control, worldMouse, &clickDist);
                int best = -1; float bestD = 1e9f;
                for (int i = 0; i < (int)gaps.size(); i++) {
                    float mid = (gaps[i].x + gaps[i].y) * 0.5f;
                    float d = fabsf(mid - clickFrac);
                    if (d < bestD) { bestD = d; best = i; }
                }
                if (best >= 0) { gaps.erase(gaps.begin() + best); status = TextFormat("%d fence gaps", (int)gaps.size()); }
            }
        }

        if (IsKeyPressed(KEY_N)) {
            control.clear();
            gaps.clear();
            worldStyle = 0;
            mapName = "untitled";
            status = "New map.";
        }

        if (IsKeyPressed(KEY_O)) {
            mapPaths = mapfile::ListMapFiles();
            if (mapPaths.empty()) {
                status = "No .cgmap files found in " + mapfile::MapsDirectory();
            } else {
                openCycle = (openCycle + 1) % (int)mapPaths.size();
                mapfile::LoadResult r = mapfile::Load(mapPaths[openCycle]);
                if (r.ok) {
                    control = r.trackDef.control;
                    gaps = r.trackDef.fenceGaps;
                    worldStyle = r.worldStyle;
                    mapName = r.trackDef.name;
                    status = "Opened " + mapName + " (" + TextFormat("%d/%d", openCycle + 1, (int)mapPaths.size()) + ") -- press O again to cycle";
                } else {
                    status = "Failed to open: " + r.error;
                }
            }
        }

        if (IsKeyPressed(KEY_S)) {
            if (control.size() < 4) {
                status = "Need at least 4 road points before saving.";
            } else {
                std::string typed = mapName;
                if (TextPrompt("Save as (name):", typed, w, h)) {
                    mapName = typed;
                    track::TrackDef def;
                    def.name = mapName.c_str();
                    def.description = "Custom map";
                    def.control = control;
                    def.fenceGaps = gaps;
                    status = mapfile::Save(mapName, def, worldStyle)
                                 ? ("Saved to " + mapfile::MapsDirectory() + "\\" + mapName + ".cgmap")
                                 : "Save failed.";
                }
            }
        }

        // ---- draw ----
        BeginDrawing();
        ClearBackground(Color{ 18, 20, 26, 255 });

        // World-space grid, 100-unit spacing, so scale is readable while panning/zooming.
        {
            Vector2 topLeft = view.ToWorld(Vector2{ 0, 96 }, w, h);
            Vector2 botRight = view.ToWorld(Vector2{ (float)w, (float)h }, w, h);
            float spacing = 100.0f;
            int x0 = (int)(floorf(topLeft.x / spacing) * spacing);
            int x1 = (int)(ceilf(botRight.x / spacing) * spacing);
            int z0 = (int)(floorf(topLeft.y / spacing) * spacing);
            int z1 = (int)(ceilf(botRight.y / spacing) * spacing);
            for (int x = x0; x <= x1; x += (int)spacing) {
                Vector2 a = view.ToScreen(Vector2{ (float)x, (float)z0 }, w, h);
                Vector2 b = view.ToScreen(Vector2{ (float)x, (float)z1 }, w, h);
                DrawLineV(a, b, x == 0 ? Fade(SKYBLUE, 0.35f) : Fade(RAYWHITE, 0.07f));
            }
            for (int z = z0; z <= z1; z += (int)spacing) {
                Vector2 a = view.ToScreen(Vector2{ (float)x0, (float)z }, w, h);
                Vector2 b = view.ToScreen(Vector2{ (float)x1, (float)z }, w, h);
                DrawLineV(a, b, z == 0 ? Fade(SKYBLUE, 0.35f) : Fade(RAYWHITE, 0.07f));
            }
        }

        // Road loop + control points.
        int n = (int)control.size();
        for (int i = 0; i < n; i++) {
            Vector2 a = view.ToScreen(control[i], w, h);
            Vector2 b = view.ToScreen(control[(i + 1) % n], w, h);
            float frac0 = (float)i / (float)n;
            float frac1 = (float)(i + 1) / (float)n;
            bool inGap = false;
            for (const Vector2 &g : gaps) {
                bool wraps = g.x > g.y;
                bool overlap = wraps ? (frac0 >= g.x || frac1 <= g.y) : (frac0 >= g.x && frac1 <= g.y);
                if (overlap) { inGap = true; break; }
            }
            DrawLineEx(a, b, 4.0f, inGap ? Fade(RED, 0.3f) : Fade(Color{ 90, 150, 235, 220 }, showPreview ? 0.35f : 1.0f));
        }

        // Preview: the actual road curve the game builds, not the straight-
        // segment polyline above -- that one's just a placement aid between
        // control points.
        if (showPreview && n >= 4) {
            std::vector<Vector2> curve = BuildPreviewCurve(control);
            int cn = (int)curve.size();
            for (int i = 0; i < cn; i++) {
                Vector2 a = view.ToScreen(curve[i], w, h);
                Vector2 b = view.ToScreen(curve[(i + 1) % cn], w, h);
                DrawLineEx(a, b, 5.0f, Color{ 120, 210, 140, 235 });
            }
        }

        for (int i = 0; i < n; i++) {
            Vector2 p = view.ToScreen(control[i], w, h);
            bool hovered = Vector2Distance(p, mouse) <= pickRadius;
            DrawCircleV(p, hovered ? 9.0f : 7.0f, mode == MODE_ROAD ? Color{ 150, 200, 255, 255 } : Fade(Color{ 150, 200, 255, 255 }, 0.5f));
            DrawCircleLines((int)p.x, (int)p.y, hovered ? 9.0f : 7.0f, RAYWHITE);
            DrawText(TextFormat("%d", i), (int)p.x + 10, (int)p.y - 8, 14, Fade(RAYWHITE, 0.7f));
        }

        // Gap midpoint markers, drawn along the same straight-segment
        // approximation NearestFraction uses for placement.
        for (const Vector2 &g : gaps) {
            float mid = g.x + (g.y >= g.x ? (g.y - g.x) : (1.0f - g.x + g.y)) * 0.5f;
            if (mid > 1.0f) mid -= 1.0f;
            float idxF = mid * n;
            int i0 = (int)idxF % n;
            float t = idxF - floorf(idxF);
            Vector2 wpos = Vector2Lerp(control[i0], control[(i0 + 1) % n], t);
            Vector2 spos = view.ToScreen(wpos, w, h);
            DrawCircleV(spos, 10.0f, Fade(RED, 0.8f));
            DrawText("GAP", (int)spos.x - 16, (int)spos.y - 26, 14, RED);
        }

        if (mode == MODE_ROAD && !overHud) {
            DrawCircleLines((int)mouse.x, (int)mouse.y, pickRadius, Fade(RAYWHITE, 0.3f));
        }

        // ---- HUD ----
        DrawRectangle(0, 0, w, 96, Fade(Color{ 10, 12, 16, 255 }, 0.9f));
        DrawText(TextFormat("MAP EDITOR -- %s  [%s]", mapName.c_str(), StyleName(worldStyle)), 16, 8, 22, RAYWHITE);
        DrawText(TextFormat("Mode: %s (1: fence gaps  2: road points  3: cycle world style: %s  P: preview curve %s)",
                            mode == MODE_ROAD ? "ROAD" : "FENCE GAPS", StyleName(worldStyle),
                            showPreview ? "ON" : "OFF"),
                 16, 34, 16, Fade(RAYWHITE, 0.8f));
        DrawText("LClick: add/drag  RClick: delete  Wheel: zoom  MMB/Arrows: pan  N: new  O: open next  S: save  Esc: quit",
                 16, 54, 14, Fade(RAYWHITE, 0.6f));
        DrawText(status.c_str(), 16, 74, 14, Color{ 150, 220, 150, 255 });

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
