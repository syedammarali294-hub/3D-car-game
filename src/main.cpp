#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "AiTraffic.hpp"
#include "Camera.hpp"
#include "Car.hpp"
#include "CarDatabase.hpp"
#include "Config.hpp"
#include "Console.hpp"
#include "Engine.hpp"
#include "Fence.hpp"
#include "Input.hpp"
#include "Lighting.hpp"
#include "Menu.hpp"
#include "Road.hpp"
#include "Settings.hpp"
#include "Spectator.hpp"
#include "Terrain.hpp"
#include "Track.hpp"
#include "Upgrades.hpp"
#include "Water.hpp"
#include "World.hpp"

namespace {

void DrawSky()
{
    DrawRectangleGradientV(0, 0, config::SCREEN_WIDTH, config::SCREEN_HEIGHT,
                           lighting::SkyColorTop(), lighting::SkyColorHorizon());
}

const char *FormatLapTime(float seconds)
{
    if (seconds < 0.0f) return "--:--.-";
    int m = (int)(seconds / 60.0f);
    float s = seconds - m * 60.0f;
    return TextFormat("%02d:%05.2f", m, s);
}

// Small top-down readout of the circuit, the car's position on it, and
// which way it's heading -- drawn from road::Centreline() samples, which
// already exist for fence/collision work, so this is pure 2D projection,
// no new world data.
// Click anywhere on the minimap to instantly teleport there (snapped to
// the nearest point on the circuit, so a click never drops the car off
// the road or into the lake/mountains). static locals persist the
// fading marker ring between frames -- fine since this is only ever
// called from the one place in main().
void DrawMinimap(car::Car &player, int screenW, const traffic::AiTraffic *aiTraffic, bool inputBlocked)
{
    const auto &samples = road::Centreline();
    if (samples.empty()) return;

    float minX = 1e9f, maxX = -1e9f, minZ = 1e9f, maxZ = -1e9f;
    for (const road::Sample &s : samples) {
        minX = fminf(minX, s.position.x); maxX = fmaxf(maxX, s.position.x);
        minZ = fminf(minZ, s.position.z); maxZ = fmaxf(maxZ, s.position.z);
    }
    float span = fmaxf(maxX - minX, maxZ - minZ) * 1.15f;
    float cx = (minX + maxX) * 0.5f, cz = (minZ + maxZ) * 0.5f;

    const float mapSize = 160.0f;
    const float mapX = (float)screenW - mapSize - 20.0f, mapY = 20.0f;

    auto toMap = [&](float x, float z) {
        return Vector2{
            mapX + ((x - cx) / span + 0.5f) * mapSize,
            mapY + ((z - cz) / span + 0.5f) * mapSize
        };
    };

    DrawRectangle((int)mapX - 6, (int)mapY - 6, (int)mapSize + 12, (int)mapSize + 12, Fade(BLACK, 0.45f));
    DrawRectangleLines((int)mapX - 6, (int)mapY - 6, (int)mapSize + 12, (int)mapSize + 12, Fade(RAYWHITE, 0.4f));

    for (size_t i = 0; i < samples.size(); i++) {
        Vector2 a = toMap(samples[i].position.x, samples[i].position.z);
        Vector2 b = toMap(samples[(i + 1) % samples.size()].position.x, samples[(i + 1) % samples.size()].position.z);
        DrawLineEx(a, b, 2.0f, Fade(RAYWHITE, 0.8f));
    }

    // Traffic cars as small dots -- drawn before the player's own dot so
    // the player always reads as the foreground marker, not buried in a
    // cluster of AI dots.
    if (aiTraffic) {
        for (int i = 0; i < aiTraffic->CarCount(); i++) {
            Vector3 tp = aiTraffic->CarPosition(i);
            Vector2 td = toMap(tp.x, tp.z);
            DrawCircleV(td, 3.0f, Fade(Color{ 230, 210, 120, 255 }, 0.85f));
        }
    }

    Vector3 pos = player.GetPosition();
    Vector2 dot = toMap(pos.x, pos.z);
    float h = player.GetHeading();
    Vector2 tip = Vector2{ dot.x + sinf(h) * 7.0f, dot.y + cosf(h) * 7.0f };
    DrawCircleV(dot, 4.0f, Color{ 120, 230, 140, 255 });
    DrawLineEx(dot, tip, 2.0f, Color{ 120, 230, 140, 255 });

    // Click-to-teleport: GetMousePosition() already comes back in this same
    // fixed logical space (main.cpp applies SetMouseScale/Offset once per
    // frame for exactly this reason -- see the fullscreen letterboxing
    // code), so no extra conversion is needed beyond inverting toMap().
    static Vector2 markerPos{};
    static float markerTimer = 0.0f;
    Rectangle mapRect{ mapX - 6.0f, mapY - 6.0f, mapSize + 12.0f, mapSize + 12.0f };
    Vector2 mouse = GetMousePosition();
    // Teleporting resets lastRoadIdx without warning and gets caught by the
    // large-jump lap-invalidation check (see Phase 1) -- that's intentional,
    // but it means this can't be left available in normal play without a
    // free way to fake a lap. Admin mode is the explicit opt-in for it.
    if (!inputBlocked && settings::Get().adminMode && CheckCollisionPointRec(mouse, mapRect) &&
        IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        float wx = cx + ((mouse.x - mapX) / mapSize - 0.5f) * span;
        float wz = cz + ((mouse.y - mapY) / mapSize - 0.5f) * span;
        const road::Sample &target = road::NearestSample(wx, wz);
        player.Respawn(target.position, atan2f(target.forward.x, target.forward.z));
        markerPos = toMap(target.position.x, target.position.z);
        markerTimer = 1.0f;
    }
    if (markerTimer > 0.0f) {
        markerTimer -= GetFrameTime();
        float ringR = (1.0f - markerTimer) * 14.0f + 3.0f;
        DrawCircleLines((int)markerPos.x, (int)markerPos.y, ringR, Fade(Color{ 255, 210, 90, 255 }, markerTimer));
    }
}

// Visible start/finish gate (checkpoint 0) and small flag markers
// (checkpoints 1..checkpointCount-1), all simple primitives computed
// straight from road::Centreline() -- no new assets, no caching needed
// for ~8 draws a frame.
void DrawCheckpointMarkers(int checkpointCount)
{
    const auto &samples = road::Centreline();
    int n = (int)samples.size();
    if (n == 0) return;

    for (int cp = 0; cp < checkpointCount; cp++) {
        const road::Sample &s = samples[(n * cp) / checkpointCount];
        float half = config::ROAD_WIDTH * 0.5f + 1.2f;
        Vector3 left = Vector3Add(s.position, Vector3Scale(s.right, -half));
        Vector3 right = Vector3Add(s.position, Vector3Scale(s.right, half));

        if (cp == 0) {
            // Finish gate: two taller pillars, alternating red/white bands
            // (a cheap stand-in for a chequered pattern), connected by a
            // banner bar overhead.
            float poleH = 5.5f;
            for (Vector3 base : { left, right }) {
                for (int band = 0; band < 5; band++) {
                    float by = base.y + band * (poleH / 5.0f);
                    Color c = (band % 2 == 0) ? Color{ 230, 230, 230, 255 } : Color{ 210, 40, 40, 255 };
                    DrawCube(Vector3{ base.x, by + poleH / 10.0f, base.z }, 0.35f, poleH / 5.0f, 0.35f, c);
                }
            }
            Vector3 bannerMid = Vector3Lerp(left, right, 0.5f);
            bannerMid.y += poleH;
            float bannerLen = Vector3Distance(left, right);
            DrawCube(bannerMid, bannerLen, 0.7f, 0.15f, Color{ 230, 230, 230, 255 });
            DrawCubeWires(bannerMid, bannerLen, 0.7f, 0.15f, Color{ 40, 40, 40, 255 });
        } else {
            // Checkpoint flag: one thin pole on the right-hand side, a
            // small coloured flag near the top.
            float poleH = 3.2f;
            Vector3 base = right;
            DrawCylinderEx(base, Vector3{ base.x, base.y + poleH, base.z }, 0.05f, 0.05f, 6,
                           Color{ 90, 90, 95, 255 });
            Vector3 flagCenter = Vector3{ base.x + s.forward.x * 0.35f, base.y + poleH - 0.3f, base.z + s.forward.z * 0.35f };
            DrawCube(flagCenter, 0.7f, 0.45f, 0.05f, Color{ 90, 200, 255, 255 });
        }
    }
}

void DrawFirstDriveHint(const input::Input &input, float hintTimer, int screenW, int screenH)
{
    if (hintTimer <= 0.0f) return;
    float alpha = fminf(1.0f, hintTimer); // fades out over its last second

    const char *lines[] = {
        "CONTROLS",
        TextFormat("%s / %s : accelerate / brake",
                  input::Input::KeyLabel(input.GetKey(input::ACTION_ACCELERATE)),
                  input::Input::KeyLabel(input.GetKey(input::ACTION_BRAKE))),
        TextFormat("%s / %s : steer",
                  input::Input::KeyLabel(input.GetKey(input::ACTION_STEER_LEFT)),
                  input::Input::KeyLabel(input.GetKey(input::ACTION_STEER_RIGHT))),
        TextFormat("%s : handbrake", input::Input::KeyLabel(input.GetKey(input::ACTION_HANDBRAKE))),
        TextFormat("%s : respawn", input::Input::KeyLabel(input.GetKey(input::ACTION_RESET))),
        "C : change camera      P / ESC : pause",
        settings::Get().manual
            ? TextFormat("%s / %s : gear up / down (manual)",
                         input::Input::KeyLabel(input.GetKey(input::ACTION_GEAR_UP)),
                         input::Input::KeyLabel(input.GetKey(input::ACTION_GEAR_DOWN)))
            : "Automatic gearbox (change in Settings > Gear)",
    };
    const int lineCount = (int)(sizeof(lines) / sizeof(lines[0]));

    int boxW = 360, boxH = 20 + lineCount * 26;
    int boxX = screenW / 2 - boxW / 2, boxY = screenH / 2 - boxH / 2 - 40;

    DrawRectangle(boxX, boxY, boxW, boxH, Fade(Color{ 12, 16, 22, 230 }, alpha));
    DrawRectangleLines(boxX, boxY, boxW, boxH, Fade(Color{ 120, 150, 200, 220 }, alpha));

    for (int i = 0; i < lineCount; i++) {
        int size = (i == 0) ? 20 : 16;
        Color c = (i == 0) ? Fade(RAYWHITE, alpha) : Fade(RAYWHITE, alpha * 0.85f);
        DrawText(lines[i], boxX + 18, boxY + 12 + i * 26, size, c);
    }
}

void DrawHud(car::Car &player, const cam::GameCamera &camera, int visibleProps,
             bool showStats, const input::Input &input, int lapCount, float lapTime, float bestLap,
             int nextCheckpoint, int checkpointCount, bool lapInvalid,
             float hintTimer, float driftPopupTimer, float driftPopupScore, const traffic::AiTraffic *aiTraffic,
             bool consoleOpen = false)
{
    // Shows direction rather than hiding it: a flipped sign while coasting
    // backwards used to read as a plain, still-positive speed.
    bool reversing = player.IsReversing();
    const settings::Options &opt = settings::Get();
    float unitScale = opt.useMph ? 2.237f : 3.6f; // world units/s -> mph or km/h
    int speedShown = (int)(fabsf(player.GetSpeed()) * unitScale);
    DrawText(TextFormat("%d", speedShown), 24, 24, 56, RAYWHITE);
    DrawText(opt.useMph ? "mph" : "km/h", 26, 84, 20, Fade(RAYWHITE, 0.75f));

    if (opt.showGearHud) {
        // R when backing up, otherwise the gear number out of how many this
        // car has; M/D says whether you are shifting or it is.
        const char *mode = opt.manual ? "M" : "D";
        if (reversing) {
            DrawText("R", 150, 24, 56, Color{ 240, 120, 110, 255 });
        } else {
            DrawText(TextFormat("%d", player.GetGear()), 150, 24, 56, RAYWHITE);
            DrawText(TextFormat("/%d  %s", player.GetGearCount(), mode), 190, 50, 22, Fade(RAYWHITE, 0.7f));
        }
    } else if (reversing) {
        DrawText("R", 150, 30, 32, Fade(Color{ 240, 120, 110, 255 }, 0.9f));
    }

    if (player.IsInWater()) {
        DrawText("IN WATER", config::SCREEN_WIDTH / 2 - 60, 30, 24, Fade(SKYBLUE, 0.9f));
    }

    // NOS meter -- a small bar under the odometer, same idea as a fuel
    // gauge. Glows and reads "BOOST" while actually being spent. Sat right
    // on top of the odometer text before (192 + its own label at -20 = 192,
    // dead level with "X.X km"); given real clearance now.
    {
        float barW = 140.0f, barH = 12.0f, bx = 24.0f, by = 248.0f;
        Color fill = player.IsBoosting() ? Color{ 90, 200, 255, 255 } : Color{ 60, 130, 190, 220 };
        DrawRectangle((int)bx, (int)by, (int)barW, (int)barH, Color{ 30, 34, 44, 220 });
        DrawRectangle((int)bx, (int)by, (int)(barW * player.GetBoost()), (int)barH, fill);
        DrawRectangleLines((int)bx, (int)by, (int)barW, (int)barH, Fade(RAYWHITE, 0.5f));
        DrawText(player.IsBoosting() ? "BOOST!" : "NOS", (int)bx, (int)by - 20, 16,
                 player.IsBoosting() ? Color{ 120, 220, 255, 255 } : Fade(RAYWHITE, 0.7f));
    }

    DrawText(TextFormat("Camera: %s  (%s to cycle)", camera.ModeLabel(),
                        input::Input::KeyLabel(input.GetKey(input::ACTION_CAMERA))),
             config::SCREEN_WIDTH - 320, config::SCREEN_HEIGHT - 32, 18, Fade(RAYWHITE, 0.75f));

    DrawText(TextFormat("%s/%s/%s/%s: drive   %s: handbrake   %s: respawn   P/ESC: pause   F1: stats   F11: fullscreen",
                        input::Input::KeyLabel(input.GetKey(input::ACTION_ACCELERATE)),
                        input::Input::KeyLabel(input.GetKey(input::ACTION_STEER_LEFT)),
                        input::Input::KeyLabel(input.GetKey(input::ACTION_BRAKE)),
                        input::Input::KeyLabel(input.GetKey(input::ACTION_STEER_RIGHT)),
                        input::Input::KeyLabel(input.GetKey(input::ACTION_HANDBRAKE)),
                        input::Input::KeyLabel(input.GetKey(input::ACTION_RESET))),
             20, config::SCREEN_HEIGHT - 32, 16, Fade(RAYWHITE, 0.8f));

    // FPS always sits with the speedo at top-left so it is never mistaken for
    // missing; F1 adds the extra diagnostics next to it.
    int fps = GetFPS();
    Color fpsColor = (fps >= 50) ? Color{ 120, 230, 140, 255 }
                   : (fps >= 30) ? Color{ 240, 210, 110, 255 }
                                 : Color{ 240, 120, 110, 255 };
    DrawText(TextFormat("%d FPS", fps), 24, 112, 24, fpsColor);

    // Odometer -- always visible, like the speedo, not gated behind F1.
    DrawText(TextFormat("%.1f km", player.GetTripKm()), 24, 192, 20, Fade(RAYWHITE, 0.85f));

    // Lap timer -- current time always shown while a lap is in progress;
    // best lap only once one has actually been completed.
    Color lapColor = lapInvalid ? Color{ 240, 120, 110, 255 } : RAYWHITE;
    DrawText(TextFormat("Lap %d   %s", lapCount + 1, FormatLapTime(lapTime)),
             config::SCREEN_WIDTH - 220, 24, 20, Fade(lapColor, 0.9f));
    if (lapInvalid) {
        DrawText("INVALID (teleported)", config::SCREEN_WIDTH - 220, 50, 14,
                 Fade(Color{ 240, 120, 110, 255 }, 0.9f));
    } else {
        // Checkpoints already hit out of the total -- index 0 is the
        // finish line itself, so "hit" count is nextCheckpoint - 1.
        int hit = nextCheckpoint - 1;
        DrawText(TextFormat("Checkpoint %d/%d", hit, checkpointCount - 1),
                 config::SCREEN_WIDTH - 220, 50, 14, Fade(RAYWHITE, 0.65f));
    }
    if (bestLap >= 0.0f) {
        DrawText(TextFormat("Best   %s", FormatLapTime(bestLap)),
                 config::SCREEN_WIDTH - 220, 70, 18, Fade(Color{ 255, 210, 90, 255 }, 0.9f));
    }

    DrawMinimap(player, config::SCREEN_WIDTH, aiTraffic, consoleOpen);

    // Drift score -- live while sliding, held briefly after it ends so the
    // final number is actually readable instead of vanishing instantly.
    if (player.IsDrifting()) {
        const char *txt = TextFormat("DRIFT  %d", (int)player.GetDriftScore());
        int tw = MeasureText(txt, 28);
        DrawText(txt, config::SCREEN_WIDTH / 2 - tw / 2, 100, 28, Color{ 255, 170, 60, 255 });
    } else if (driftPopupTimer > 0.0f) {
        float alpha = fminf(1.0f, driftPopupTimer);
        const char *txt = TextFormat("DRIFT  +%d", (int)driftPopupScore);
        int tw = MeasureText(txt, 30);
        DrawText(txt, config::SCREEN_WIDTH / 2 - tw / 2, 100, 30, Fade(Color{ 255, 190, 80, 255 }, alpha));
    }

    if (showStats) {
        DrawText(TextFormat("props drawn: %d", visibleProps), 24, 142, 18, Fade(RAYWHITE, 0.8f));
        DrawText("F1: hide stats", 24, 164, 16, Fade(RAYWHITE, 0.55f));
    }

    if (settings::Get().adminMode) {
        DrawText("ADMIN MODE", 24, config::SCREEN_HEIGHT - 70, 18, Color{ 255, 90, 90, 255 });
        DrawText("minimap click: teleport  F2: refill NOS  F3: unstick", 24, config::SCREEN_HEIGHT - 48, 14,
                 Fade(RAYWHITE, 0.7f));
    }
}

} // namespace

int main(int argc, char **argv)
{
    // Every asset/shader load in this project uses a path relative to the
    // exe (e.g. "shaders/scene.vs"). That's only valid if the current
    // working directory IS the exe's folder -- true when launched
    // normally, but Windows sets the cwd to wherever a double-clicked file
    // lives, not to the exe's folder, when the game is opened by
    // double-clicking a .cgmap (see FileAssoc.cpp). Without this, every
    // relative load below silently fails and falls back to raylib's
    // default white material/texture -- which is exactly why the ground,
    // trees, and cars went blank white rather than crashing outright.
    ChangeDirectory(GetApplicationDirectory());

    // No MSAA by default -- 4x antialiasing is a real cost on the Intel HD
    // 4000-class hardware this project targets, and the low-poly flat-shaded
    // look was chosen specifically so it does not need smoothing to read
    // well. F11 toggles fullscreen below; a Settings row to re-enable MSAA
    // on faster hardware would be a reasonable follow-up (see TODO.md).
    SetConfigFlags(FLAG_WINDOW_RESIZABLE); // needed for borderless-fullscreen below to behave
    InitWindow(config::SCREEN_WIDTH, config::SCREEN_HEIGHT, "3D Car Game");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL); // ESC pauses instead of closing the window

    // Everything in this project (every DrawText/DrawRectangle position,
    // every menu button) is positioned in a fixed config::SCREEN_WIDTH x
    // SCREEN_HEIGHT space. Rather than letting fullscreen actually resize
    // the real framebuffer to the monitor's resolution -- which would
    // squeeze all of that into one corner of the screen -- the whole frame
    // renders into this fixed-size target, then gets scaled up and
    // letterboxed into whatever the real window size is. This also sidesteps
    // the other half of the problem: ToggleFullscreen() does a genuine
    // Windows display-mode switch, which is what was flashing the screen
    // black on every press. ToggleBorderlessWindowed() (used below instead)
    // never changes the video mode, so there's nothing to flash.
    RenderTexture2D frameTarget = LoadRenderTexture(config::SCREEN_WIDTH, config::SCREEN_HEIGHT);
    SetTextureFilter(frameTarget.texture, TEXTURE_FILTER_BILINEAR);

    audio::Init();
    audio::EngineSound engineSound;
    engineSound.Load();

    // Autosaves the per-car odometer periodically rather than every frame
    // -- writing to disk 60 times a second for one float wouldn't be worth
    // it, so this ticks down independently of lapTime (which resets).
    float upgradesSaveTimer = 0.0f;

    input::Input input;
    input.Load();
    settings::Load();
    upgrades::Load();
    audio::ApplySettings();

    road::BuildCentreline(track::Get(track::TRACK_LONG).control);

    lighting::SceneShader sceneShader;
    sceneShader.Load("shaders/scene.vs", "shaders/scene.fs");

    lighting::SceneShader terrainShader;
    terrainShader.Load("shaders/scene.vs", "shaders/terrain.fs");

    terrain::Terrain ground;
    ground.Load(terrainShader.Get());

    road::RoadMesh roadMesh;
    roadMesh.Load(sceneShader.Get());

    fence::Fence fences;
    fences.Load(sceneShader.Get(), track::Get(track::TRACK_LONG).fenceGaps);

    water::Water lake;
    lake.Load();

    // Low cuts prop density and draw distance (settings::Options::highQuality,
    // Settings > Car > Quality) -- the one real lever for anyone who wants
    // a lighter-weight option now that the low-end-hardware target was
    // explicitly dropped (see TODO.md).
    auto PropDensityScale = []() { return settings::Get().highQuality ? 1.0f : 0.45f; };
    auto EffectiveDrawDistance = []() { return settings::Get().highQuality ? config::DRAW_DISTANCE : config::DRAW_DISTANCE * 0.65f; };

    world::World scenery;
    scenery.Load(sceneShader.Get(), PropDensityScale());

    menu::Menu ui;

    // Double-click a .cgmap file (Windows passes its full path as argv[1]
    // once file association is set up in Settings) -- load it straight in,
    // skip the main menu. Silently falls through to the normal main menu
    // if the path doesn't end in .cgmap or didn't load; this is a nice-to-
    // have shortcut, not something that should block launching the game.
    if (argc > 1) {
        std::string argPath = argv[1];
        if (argPath.size() > 6 && argPath.compare(argPath.size() - 6, 6, ".cgmap") == 0) {
            ui.StartWithCustomMap(argPath);
        }
    }

    car::Car player;
    bool carLoaded = false;
    int loadedCarIndex = -1;
    int loadedColorIndex = -1;
    terrain::Style loadedWorldStyle = terrain::STYLE_HILLS;
    track::TrackId loadedTrack = track::TRACK_LONG;
    bool loadedHighQuality = settings::Get().highQuality;

    // Traffic mode: a handful of AI cars looping the same circuit. Only
    // loaded (and only costs anything) while actually in that mode.
    traffic::AiTraffic aiTraffic;
    bool trafficLoaded = false;
    constexpr int TRAFFIC_CAR_COUNT = 8;

    cam::GameCamera camera;

    // Admin-only: free-fly spectator camera + dev console, same gating as
    // the minimap teleport/NOS refill/unstick above (see Settings.hpp).
    // /gamemode spectator and /gamemode drive toggle between this and the
    // normal car camera; the console itself only opens at all when admin
    // mode is on.
    spectator::FlyCam flycam;
    console::Console devConsole;
    bool spectating = false;

    bool showStats = true;

    // First-launch controls hint: shown once, the first time the car is
    // ever actually driven this session, dismissed by any input or after a
    // few seconds.
    bool everPlayed = false;
    float hintTimer = 0.0f;

    // Final drift score, held on screen for a couple of seconds after a
    // drift ends rather than disappearing the instant the handbrake lifts.
    float lastDriftPopupTimer = 0.0f;
    float lastDriftPopupScore = 0.0f;

    // Session top-speed record and a brief "NEW TOP SPEED" popup when beaten.
    float topSpeedRecord = 0.0f;
    float topSpeedPopupTimer = 0.0f;

    // Confetti burst on completing a lap -- small 2D squares launched from
    // the top speed/km readout, gravity-faded, pure screen-space fun with
    // no 3D cost.
    struct Confetti { Vector2 pos; Vector2 vel; float life; Color color; };
    std::vector<Confetti> confetti;

    // Visual pairing for the water-splash sound: a handful of small 3D
    // droplets launched from the entry point, gravity-faded, same spirit
    // as the confetti above but in world space instead of screen space.
    struct Splash { Vector3 pos; Vector3 vel; float life; };
    std::vector<Splash> splashes;

    // Lap tracking: nearest-centreline-index crossing the start/finish line.
    // Checkpoints (8, evenly spaced around the centreline -- index 0 is the
    // start/finish line, 1-7 the rest) must be crossed IN ORDER for a lap
    // to count, replacing the old "got roughly halfway round" heuristic --
    // that one didn't actually require you to drive the track, just be on
    // the far side of it when you crossed the line, and had no idea a
    // teleport had happened.
    constexpr int CHECKPOINT_COUNT = 8;
    int lapCount = 0;
    float lapTime = 0.0f;
    float bestLap = -1.0f;
    int lastRoadIdx = -1;
    int nextCheckpoint = 1; // 1..7 waiting to be crossed; 8 means all hit, only the finish line is left
    // Set whenever the car's road position jumps a large distance in one
    // frame -- minimap teleport, Backspace respawn-to-road, and the
    // internal auto-respawn-when-stuck all go through this same symptom
    // (a big discontinuity in nearest-centreline-index) without main.cpp
    // needing its own hook into every place a respawn can happen. A lap in
    // progress when that happens finishes as INVALID -- no time, no best,
    // no ghost -- and checkpoint progress restarts from wherever landed.
    bool lapInvalid = false;

    // Ghost of the best completed lap -- recorded every frame during a lap,
    // kept only if that lap turned out to beat the previous best, then
    // played back (re-drawn via Car::DrawGhost, no physics) on every later
    // lap until a faster one replaces it.
    struct GhostSample { Vector3 pos; float heading; float t; };
    std::vector<GhostSample> ghostRecording;
    std::vector<GhostSample> bestGhost;

    // A second, independent shader + a small off-screen target for the
    // garage's live 3D car preview, so it never competes with the real
    // scene's viewPos uniform or draws into the real backbuffer.
    lighting::SceneShader previewShader;
    previewShader.Load("shaders/scene.vs", "shaders/scene.fs");
    RenderTexture2D previewRT = LoadRenderTexture(480, 320);

    // Garage preview used to reload the full model on every single click --
    // including a plain colour change, which never actually needed to touch
    // the mesh at all (Car::SetPaint just updates a draw-time tint). Now:
    // each car is loaded once and kept resident for the rest of the
    // session (unique_ptr so the vector can resize without shallow-copying
    // raylib's raw GPU handles), and a colour-only change just repaints the
    // already-loaded model.
    std::vector<std::unique_ptr<car::Car>> previewCache(cardb::Roster().size());

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();
        audio::ApplySettings(); // live, so a volume slider is heard as it moves
        if (dt > 0.1f) dt = 0.1f; // stop a hitch from teleporting the car

        // Fullscreen toggle. Now that the whole frame renders into the fixed
        // -size frameTarget above and gets scaled into the real window at
        // the end of the loop, borderless-fullscreen no longer misaligns
        // anything -- and unlike ToggleFullscreen(), it never changes the
        // actual video mode, so there's no black flash while switching.
        // Checked every frame regardless of menu/driving state, same as the
        // menu's own ESC/P.
        if (IsKeyPressed(KEY_F11)) ToggleBorderlessWindowed();

        // Only the PREVIOUS frame's screen decides whether to step physics this
        // frame; a click that changes screens this frame takes effect next
        // frame. Everything that reads input AND draws (the menu itself) must
        // run inside BeginDrawing/EndDrawing below -- raylib only keeps 2D
        // draw calls made inside that bracket.
        bool playing = (ui.CurrentScreen() == menu::SCREEN_PLAYING);

        // index 0 in the palette means "that car's own factory paint" rather
        // than a real colour of its own.
        auto ResolveColor = [&](int carIndex, int colorIndex) {
            return (colorIndex == 0) ? cardb::Roster()[carIndex].design.paint
                                      : cardb::PaintPalette()[colorIndex];
        };

        if (playing) {
            // A changed World style OR track also forces a car reload below
            // (its start position/respawn depend on the new road), same as
            // switching cars already does.
            bool worldChanged = (loadedWorldStyle != ui.SelectedWorldStyle());
            bool trackChanged = (loadedTrack != ui.SelectedTrack());
            bool qualityChanged = (loadedHighQuality != settings::Get().highQuality);
            if (worldChanged) {
                terrain::SetStyle(ui.SelectedWorldStyle());
                terrainShader.SetDesertMode(terrain::IsDesert());

                ground.Unload();
                ground.Load(terrainShader.Get());
            }
            if (worldChanged || trackChanged) {
                road::BuildCentreline(track::Get(ui.SelectedTrack()).control);
                roadMesh.Unload();
                roadMesh.Load(sceneShader.Get());

                fences.Unload();
                fences.Load(sceneShader.Get(), track::Get(ui.SelectedTrack()).fenceGaps);
            }
            if (worldChanged || trackChanged || qualityChanged) {
                // Quality alone doesn't touch terrain/road/fences -- only
                // prop density, so it only needs scenery rebuilt, not the
                // full reload above.
                scenery.Unload();
                scenery.Load(sceneShader.Get(), PropDensityScale());

                loadedWorldStyle = ui.SelectedWorldStyle();
                loadedTrack = ui.SelectedTrack();
                loadedHighQuality = settings::Get().highQuality;
            }
            bool trackOrWorldChanged = worldChanged || trackChanged;

            if (!carLoaded || trackOrWorldChanged || loadedCarIndex != ui.SelectedCar() ||
                loadedColorIndex != ui.SelectedColorIndex()) {
                if (carLoaded) player.Unload();
                player.Load(sceneShader.Get(), ui.SelectedCar(), ResolveColor(ui.SelectedCar(), ui.SelectedColorIndex()));
                road::Sample start = road::StartPoint();
                player.Respawn(start.position, atan2f(start.forward.x, start.forward.z));
                camera.Init(player);
                loadedCarIndex = ui.SelectedCar();
                loadedColorIndex = ui.SelectedColorIndex();
                carLoaded = true;
                topSpeedRecord = upgrades::Get(loadedCarIndex).bestTopSpeed; // persists across sessions, per car

                if (!everPlayed) {
                    everPlayed = true;
                    hintTimer = 6.0f;
                }

                lapCount = 0;
                lapTime = 0.0f;
                bestLap = -1.0f;
                lastRoadIdx = -1;
                nextCheckpoint = 1;
                lapInvalid = false;
                ghostRecording.clear();
                bestGhost.clear();
            }

            // Traffic mode loads/unloads alongside the car above -- a world
            // or track change already forces everything else to reload
            // here too, so traffic rebuilds on the new circuit at the same
            // time.
            bool wantTraffic = ui.IsTrafficMode();
            if (wantTraffic && (!trafficLoaded || trackOrWorldChanged)) {
                if (trafficLoaded) aiTraffic.Unload();
                aiTraffic.Load(sceneShader.Get(), TRAFFIC_CAR_COUNT);
                trafficLoaded = true;
            } else if (!wantTraffic && trafficLoaded) {
                aiTraffic.Unload();
                trafficLoaded = false;
            }

            // Admin-mode debug utilities (see Settings.hpp for why these are
            // gated -- same reasoning as the minimap teleport above: they'd
            // undermine normal play if left always-on). All suppressed while
            // the console is open -- it owns the keyboard then, the same
            // way typing "spectator" shouldn't also hit F1/F2/F3 shortcuts
            // if any of those letters overlapped one.
            if (!devConsole.IsOpen()) {
                if (IsKeyPressed(KEY_F1)) showStats = !showStats;
                if (settings::Get().adminMode) {
                    if (IsKeyPressed(KEY_F2)) player.RefillBoost();
                    if (IsKeyPressed(KEY_F3)) player.RespawnNearestRoad();
                    // "/" opens the console, Minecraft-chat style -- it
                    // starts pre-filled with "/" so there's no ambiguity
                    // about whether you're typing a command.
                    if (IsKeyPressed(KEY_SLASH)) devConsole.Open();
                }
            }
            if (settings::Get().adminMode == false && devConsole.IsOpen()) {
                // adminMode got turned off (e.g. Settings > Car) while the
                // console was open -- don't leave an inaccessible console
                // capturing all keyboard input.
                devConsole.Close();
            }
            devConsole.Update();

            if (devConsole.HasPendingCommand()) {
                std::string cmd = devConsole.TakeCommand();
                if (cmd == "/gamemode spectator" || cmd == "/gamemode spec") {
                    if (!spectating) {
                        flycam.Init(Vector3Add(player.GetPosition(), Vector3{ 0.0f, 4.0f, 0.0f }),
                                    player.GetHeading());
                        spectating = true;
                    }
                    devConsole.Log("Gamemode: spectator");
                } else if (cmd == "/gamemode drive" || cmd == "/gamemode player" || cmd == "/gamemode survival") {
                    spectating = false;
                    devConsole.Log("Gamemode: drive");
                } else if (cmd == "/respawn") {
                    player.RespawnNearestRoad();
                    devConsole.Log("Respawned onto the nearest road point.");
                } else if (cmd == "/refillnos") {
                    player.RefillBoost();
                    devConsole.Log("NOS refilled.");
                } else if (cmd == "/pos") {
                    Vector3 p = spectating ? flycam.Get().position : player.GetPosition();
                    devConsole.Log(TextFormat("x=%.1f y=%.1f z=%.1f", p.x, p.y, p.z));
                } else if (cmd.rfind("/tp ", 0) == 0) {
                    float tx, ty, tz;
                    if (sscanf(cmd.c_str() + 4, "%f %f %f", &tx, &ty, &tz) == 3) {
                        Vector3 dest{ tx, ty, tz };
                        if (spectating) {
                            Vector3 lookDir = Vector3Subtract(flycam.Get().target, flycam.Get().position);
                            flycam.Init(dest, atan2f(lookDir.x, lookDir.z)); // keep facing the same way
                        } else {
                            player.Respawn(dest, player.GetHeading());
                        }
                        devConsole.Log(TextFormat("Teleported to %.1f %.1f %.1f", tx, ty, tz));
                    } else {
                        devConsole.Log("Usage: /tp <x> <y> <z>");
                    }
                } else if (cmd == "/help") {
                    devConsole.Log("Commands: /gamemode spectator, /gamemode drive, /respawn, /refillnos, /tp x y z, /pos, /help");
                } else {
                    devConsole.Log("Unknown command: " + cmd + " (try /help)");
                }
            }

            if (hintTimer > 0.0f) {
                hintTimer -= dt;
                if (input.Down(input::ACTION_ACCELERATE) || input.Down(input::ACTION_BRAKE) ||
                    input.Down(input::ACTION_STEER_LEFT) || input.Down(input::ACTION_STEER_RIGHT) ||
                    input.Down(input::ACTION_HANDBRAKE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    hintTimer = 0.0f;
                }
            }

            if (trafficLoaded) aiTraffic.Update(dt);

            // Console open freezes the world, same as a pause menu would --
            // typing a command shouldn't also be steering the car. Spectator
            // mode flies the admin camera instead of updating the car at
            // all; EngineSound::Update simply isn't called while spectating,
            // which is all it takes to go silent (see Engine.hpp).
            if (!devConsole.IsOpen()) {
                if (spectating) {
                    flycam.Update(dt, input);
                } else {
                    player.Update(dt, input, scenery, fences, trafficLoaded ? &aiTraffic : nullptr);
                    camera.Update(player, input, dt);
                }
            }

            if (!spectating) {
                const cardb::CarSpec &curSpec = cardb::Roster()[ui.SelectedCar()];
                float speedFactor = fabsf(player.GetSpeed()) / curSpec.maxSpeed;
                bool slip = input.Down(input::ACTION_HANDBRAKE) && fabsf(player.GetSpeed()) > 5.0f;
                engineSound.Update(player.GetRpm(), speedFactor, slip, dt);
                if (player.JustShifted()) audio::PlayShiftClunk();

                if (player.JustHitObstacle()) {
                    audio::PlayCollisionThump(Clamp(speedFactor, 0.0f, 1.0f));
                }

                if (player.JustEnteredWater()) {
                    audio::PlayWaterSplash(Clamp(speedFactor, 0.0f, 1.0f));

                    Vector3 entry = player.GetPosition();
                    entry.y = terrain::WATER_LEVEL;
                    unsigned int sseed = (unsigned int)(entry.x * 131.0f) ^ (unsigned int)(entry.z * 977.0f) ^ 0xBADF00Du;
                    for (int c = 0; c < 14; c++) {
                        sseed = sseed * 1664525u + 1013904223u;
                        float a = ((sseed >> 8) & 0xFFFFFF) / (float)0xFFFFFF * 2.0f * PI;
                        sseed = sseed * 1664525u + 1013904223u;
                        float speed = 2.0f + ((sseed >> 8) & 0xFFFFFF) / (float)0xFFFFFF * 3.0f * (0.4f + Clamp(speedFactor, 0.0f, 1.0f));
                        Splash s;
                        s.pos = entry;
                        s.vel = Vector3{ cosf(a) * speed, 3.5f + speed * 0.6f, sinf(a) * speed };
                        s.life = 1.0f;
                        splashes.push_back(s);
                    }
                }

                if (input.Pressed(input::ACTION_HORN)) {
                    audio::PlayHorn();
                }

                if (player.JustFinishedDrift() && player.GetLastDriftScore() > 15.0f) {
                    lastDriftPopupScore = player.GetLastDriftScore();
                    lastDriftPopupTimer = 2.0f;
                } else if (lastDriftPopupTimer > 0.0f) {
                    lastDriftPopupTimer -= dt;
                }
            }

            {
                int N = (int)road::Centreline().size();
                if (N > 0) {
                    Vector3 p = player.GetPosition();
                    int idx = road::NearestSampleIndex(p.x, p.z);
                    if (lastRoadIdx < 0) lastRoadIdx = idx;

                    // Teleport/respawn detection: a normal frame can't move
                    // more than a tiny fraction of the lap's sample count,
                    // even at top speed -- a jump bigger than one
                    // checkpoint-spacing is a discontinuity, not driving.
                    int rawDiff = idx - lastRoadIdx;
                    if (rawDiff > N / 2) rawDiff -= N;
                    if (rawDiff < -N / 2) rawDiff += N;
                    int jumpMagnitude = (rawDiff < 0) ? -rawDiff : rawDiff;
                    if (jumpMagnitude > N / CHECKPOINT_COUNT) {
                        lapInvalid = true;
                    }

                    // Checkpoints 1..7, crossed in order by advancing past
                    // each one's target sample index.
                    if (nextCheckpoint < CHECKPOINT_COUNT) {
                        int cpTarget = N * nextCheckpoint / CHECKPOINT_COUNT;
                        if (lastRoadIdx < cpTarget && idx >= cpTarget) {
                            nextCheckpoint++;
                        }
                    }

                    // Finish line: the same "crossed from the far end back
                    // to the start" wraparound the old code used, kept as
                    // the one case checkpoints 1-7 don't cover.
                    if (lastRoadIdx > N * 9 / 10 && idx < N / 10) {
                        bool completedAllCheckpoints = (nextCheckpoint >= CHECKPOINT_COUNT);
                        if (completedAllCheckpoints && !lapInvalid) {
                            bool newBest = (bestLap < 0.0f || lapTime < bestLap);
                            if (newBest) {
                                bestLap = lapTime;
                                bestGhost = ghostRecording; // this lap's recording is the one that just finished
                            }
                            lapCount++;

                            // A little reward for finishing a lap.
                            const Color palette[] = {
                                Color{ 255, 90, 90, 255 }, Color{ 90, 200, 255, 255 },
                                Color{ 255, 210, 90, 255 }, Color{ 120, 230, 140, 255 },
                            };
                            for (int c = 0; c < 36; c++) {
                                Confetti piece;
                                piece.pos = Vector2{ (float)config::SCREEN_WIDTH / 2.0f, 90.0f };
                                float ang = (float)c / 36.0f * 2.0f * PI;
                                float spd = 120.0f + (float)(c % 5) * 30.0f;
                                piece.vel = Vector2{ cosf(ang) * spd, sinf(ang) * spd - 60.0f };
                                piece.life = 1.0f;
                                piece.color = palette[c % 4];
                                confetti.push_back(piece);
                            }
                        }
                        // Reset for the next lap either way -- an invalid
                        // or incomplete attempt still ends here, it just
                        // doesn't count.
                        ghostRecording.clear();
                        lapTime = 0.0f;
                        nextCheckpoint = 1;
                        lapInvalid = false;
                    }
                    lastRoadIdx = idx;
                }
                lapTime += dt;

                // Record every frame -- a few minutes at 60fps is still a
                // trivial amount of memory (one Vector3 + two floats each).
                GhostSample gs;
                gs.pos = player.GetPosition();
                gs.heading = player.GetHeading();
                gs.t = lapTime;
                ghostRecording.push_back(gs);
            }

            upgradesSaveTimer += dt;
            if (upgradesSaveTimer > 10.0f) {
                upgradesSaveTimer = 0.0f;
                upgrades::Save();
            }

            {
                // Persistent (not just session) top-speed record, per car.
                float curSpeed = fabsf(player.GetSpeed());
                if (curSpeed > topSpeedRecord + 0.01f) {
                    topSpeedRecord = curSpeed;
                    topSpeedPopupTimer = 1.6f;
                    upgrades::ReportSpeed(loadedCarIndex, curSpeed);
                } else if (topSpeedPopupTimer > 0.0f) {
                    topSpeedPopupTimer -= dt;
                }

                // Confetti physics: simple gravity + fade, screen-space only.
                for (Confetti &c : confetti) {
                    c.vel.y += 260.0f * dt;
                    c.pos.x += c.vel.x * dt;
                    c.pos.y += c.vel.y * dt;
                    c.life -= dt * 0.8f;
                }
                confetti.erase(std::remove_if(confetti.begin(), confetti.end(),
                                              [](const Confetti &c) { return c.life <= 0.0f; }),
                               confetti.end());

                // Splash physics: same shape as confetti above, world-space
                // gravity instead of screen-space.
                for (Splash &s : splashes) {
                    s.vel.y -= 9.0f * dt;
                    s.pos = Vector3Add(s.pos, Vector3Scale(s.vel, dt));
                    s.life -= dt * 1.6f;
                }
                splashes.erase(std::remove_if(splashes.begin(), splashes.end(),
                                              [](const Splash &s) { return s.life <= 0.0f; }),
                               splashes.end());
            }

            Vector3 viewPos = spectating ? flycam.Get().position : camera.Get().position;
            sceneShader.UpdateView(viewPos);
            terrainShader.UpdateView(viewPos);
            lake.Update((float)GetTime(), viewPos);
        }

        // Whichever camera is actually driving the view this frame --
        // the normal chase/hood/etc. camera, or the admin flycam while
        // spectating. Every render-time camera.Get() below reads this
        // instead so spectator mode doesn't need its own copy of the
        // render block.
        const Camera3D &activeCam = spectating ? flycam.Get() : camera.Get();

        // Garage preview: rendered to its own off-screen target before the
        // real frame starts, then handed to the menu as a texture to draw
        // like any other 2D element.
        bool inGarage = (ui.CurrentScreen() == menu::SCREEN_GARAGE);
        if (inGarage) {
            int idx = ui.SelectedCar();
            if (!previewCache[idx]) {
                previewCache[idx] = std::make_unique<car::Car>();
                previewCache[idx]->Load(previewShader.Get(), idx, ResolveColor(idx, ui.SelectedColorIndex()));
                previewCache[idx]->Respawn(Vector3{ 0.0f, 0.0f, 0.0f }, 0.5f);
            }
            car::Car &previewCar = *previewCache[idx];
            // Colour-only change: just retint, no reload at all.
            previewCar.SetPaint(ResolveColor(idx, ui.SelectedColorIndex()));

            // Menu owns the angle now -- TickGarage auto-orbits it when idle
            // and drag-rotates it when the player holds the mouse down over
            // the preview, so it has to run before this reads it.
            float previewAngle = ui.GaragePreviewAngle();

            const cardb::CarSpec &pspec = cardb::Roster()[idx];
            Vector3 carPos = previewCar.GetPosition();
            float dist = pspec.design.length * 1.25f + 1.4f;
            Camera3D previewCam = { 0 };
            previewCam.position = Vector3Add(carPos, Vector3{
                sinf(previewAngle) * dist, pspec.design.beltHeight + 1.0f, cosf(previewAngle) * dist
            });
            previewCam.target = Vector3Add(carPos, Vector3{ 0.0f, pspec.design.beltHeight * 0.3f, 0.0f });
            previewCam.up = Vector3{ 0.0f, 1.0f, 0.0f };
            previewCam.fovy = 26.0f;
            previewCam.projection = CAMERA_PERSPECTIVE;

            previewShader.UpdateView(previewCam.position);

            BeginTextureMode(previewRT);
                ClearBackground(Color{ 22, 28, 40, 255 });
                BeginMode3D(previewCam);
                    previewCar.Draw();
                EndMode3D();
            EndTextureMode();

            ui.SetGaragePreview(previewRT.texture);
        }

        // Mouse coordinates need to land in the fixed logical space too --
        // every button rect in Menu.cpp is computed against config::
        // SCREEN_WIDTH/HEIGHT, but GetMousePosition() otherwise returns real
        // window pixels, which stop matching the moment the window isn't
        // exactly 1280x720 (i.e. the instant fullscreen is toggled on
        // anything but a 1280x720 monitor). SetMouseScale/Offset make
        // raylib do that translation for every GetMousePosition() call in
        // the project without touching each call site.
        {
            float scaleX = (float)GetScreenWidth() / (float)config::SCREEN_WIDTH;
            float scaleY = (float)GetScreenHeight() / (float)config::SCREEN_HEIGHT;
            float letterboxScale = fminf(scaleX, scaleY);
            float drawW = config::SCREEN_WIDTH * letterboxScale;
            float drawH = config::SCREEN_HEIGHT * letterboxScale;
            float offsetX = (GetScreenWidth() - drawW) * 0.5f;
            float offsetY = (GetScreenHeight() - drawH) * 0.5f;
            SetMouseOffset((int)-offsetX, (int)-offsetY);
            SetMouseScale(1.0f / letterboxScale, 1.0f / letterboxScale);
        }

        BeginTextureMode(frameTarget);
            ClearBackground(playing ? lighting::SkyColorHorizon() : Color{ 10, 12, 16, 255 });

            // Skip the full background world while the garage is open -- it
            // was previously drawn behind the menu even though the garage
            // already has its own live preview, which meant two full 3D
            // passes (this one plus the preview render above) every single
            // frame spent browsing cars. The garage draws its own dark
            // panel, so there is nothing lost by not rendering the world
            // behind it too.
            if (playing || (carLoaded && !inGarage)) {
                Vector3 viewPos = activeCam.position;
                DrawSky();

                BeginMode3D(activeCam);
                    ground.Draw();
                    roadMesh.Draw();
                    fences.Draw();
                    DrawCheckpointMarkers(CHECKPOINT_COUNT);
                    {
                        Vector3 viewFwd = Vector3Subtract(activeCam.target, activeCam.position);
                        float aspect = (float)config::SCREEN_WIDTH / (float)config::SCREEN_HEIGHT;
                        scenery.Draw(viewPos, EffectiveDrawDistance(), viewFwd, activeCam.fovy, aspect);
                    }
                    if (trafficLoaded) aiTraffic.Draw();
                    player.Draw();

                    // Ghost of the best lap -- interpolated to the current
                    // lap's own elapsed time, so it runs exactly alongside
                    // a real attempt at beating it, then simply stops once
                    // its own recorded lap finished.
                    if (!bestGhost.empty() && lapTime <= bestGhost.back().t) {
                        size_t gi = 0;
                        while (gi + 1 < bestGhost.size() && bestGhost[gi + 1].t < lapTime) gi++;
                        const GhostSample &ga = bestGhost[gi];
                        const GhostSample &gb = bestGhost[(gi + 1 < bestGhost.size()) ? gi + 1 : gi];
                        float span = gb.t - ga.t;
                        float gt = (span > 0.0001f) ? Clamp((lapTime - ga.t) / span, 0.0f, 1.0f) : 0.0f;
                        Vector3 gpos = Vector3Lerp(ga.pos, gb.pos, gt);
                        float gheading = ga.heading + Wrap(gb.heading - ga.heading, -PI, PI) * gt;
                        player.DrawGhost(gpos, gheading, Fade(SKYBLUE, 0.45f));
                    }
                    lake.Draw();

                    for (const Splash &s : splashes) {
                        DrawSphere(s.pos, 0.12f * s.life, Fade(Color{ 220, 235, 245, 255 }, s.life));
                    }
                EndMode3D();

                if (playing) {
                    if (!spectating) {
                        DrawHud(player, camera, scenery.GetVisibleCount(), showStats, input, lapCount, lapTime, bestLap,
                               nextCheckpoint, CHECKPOINT_COUNT, lapInvalid,
                               hintTimer, lastDriftPopupTimer, lastDriftPopupScore, trafficLoaded ? &aiTraffic : nullptr,
                               devConsole.IsOpen());
                        DrawFirstDriveHint(input, hintTimer, config::SCREEN_WIDTH, config::SCREEN_HEIGHT);
                    } else {
                        DrawText("SPECTATING", 24, 24, 26, Color{ 120, 220, 255, 255 });
                        DrawText("RMB: look  W/A/S/D: fly  SHIFT: up  CTRL: down  HANDBRAKE key: boost",
                                 24, 54, 16, Fade(RAYWHITE, 0.75f));
                        DrawText("console: /gamemode drive", 24, 74, 16, Fade(RAYWHITE, 0.6f));
                    }

                    devConsole.Draw(config::SCREEN_WIDTH, config::SCREEN_HEIGHT);

                    for (const Confetti &c : confetti) {
                        DrawRectangle((int)c.pos.x - 4, (int)c.pos.y - 4, 8, 8, Fade(c.color, Clamp(c.life, 0.0f, 1.0f)));
                    }

                    if (topSpeedPopupTimer > 0.0f) {
                        float unitScale2 = settings::Get().useMph ? 2.237f : 3.6f;
                        const char *unit2 = settings::Get().useMph ? "mph" : "km/h";
                        const char *txt = TextFormat("NEW TOP SPEED  %d %s", (int)(topSpeedRecord * unitScale2), unit2);
                        int tw = MeasureText(txt, 22);
                        DrawText(txt, config::SCREEN_WIDTH / 2 - tw / 2, 140,
                                22, Fade(Color{ 255, 230, 120, 255 }, fminf(1.0f, topSpeedPopupTimer)));
                    }
                }
            }

            // Draws the active menu screen (and handles its clicks) when not
            // playing; while playing it only watches for ESC.
            ui.Tick(input, config::SCREEN_WIDTH, config::SCREEN_HEIGHT, carLoaded, devConsole.IsOpen());
        EndTextureMode();

        // Blit the fixed-resolution frame into the real window, scaled up
        // and letterboxed (black bars) if the window's aspect ratio doesn't
        // match 1280x720 -- which it won't, on most monitors, once
        // fullscreen is on.
        BeginDrawing();
            ClearBackground(BLACK);
            float scaleX = (float)GetScreenWidth() / (float)config::SCREEN_WIDTH;
            float scaleY = (float)GetScreenHeight() / (float)config::SCREEN_HEIGHT;
            float letterboxScale = fminf(scaleX, scaleY);
            float drawW = config::SCREEN_WIDTH * letterboxScale;
            float drawH = config::SCREEN_HEIGHT * letterboxScale;
            float offsetX = (GetScreenWidth() - drawW) * 0.5f;
            float offsetY = (GetScreenHeight() - drawH) * 0.5f;
            Rectangle src = Rectangle{ 0.0f, 0.0f, (float)frameTarget.texture.width, -(float)frameTarget.texture.height };
            Rectangle dst = Rectangle{ offsetX, offsetY, drawW, drawH };
            DrawTexturePro(frameTarget.texture, src, dst, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
        EndDrawing();
    }

    if (trafficLoaded) aiTraffic.Unload();
    engineSound.Unload();
    audio::Shutdown();

    if (carLoaded) player.Unload();
    for (auto &c : previewCache) {
        if (c) c->Unload();
    }
    UnloadRenderTexture(previewRT);
    previewShader.Unload();
    scenery.Unload();
    lake.Unload();
    fences.Unload();
    roadMesh.Unload();
    ground.Unload();
    terrainShader.Unload();
    sceneShader.Unload();
    upgrades::Save();
    UnloadRenderTexture(frameTarget);
    CloseWindow();

    return 0;
}
