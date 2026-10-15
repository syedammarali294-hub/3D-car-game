#include "raylib.h"
#include "raymath.h"
#include <algorithm>
#include <vector>

#include "AiTraffic.hpp"
#include "Camera.hpp"
#include "Car.hpp"
#include "CarDatabase.hpp"
#include "Config.hpp"
#include "Engine.hpp"
#include "Fence.hpp"
#include "Input.hpp"
#include "Lighting.hpp"
#include "Menu.hpp"
#include "Road.hpp"
#include "Settings.hpp"
#include "Terrain.hpp"
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
void DrawMinimap(const car::Car &player, int screenW, const traffic::AiTraffic *aiTraffic)
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

void DrawHud(const car::Car &player, const cam::GameCamera &camera, int visibleProps,
             bool showStats, const input::Input &input, int lapCount, float lapTime, float bestLap,
             float hintTimer, float driftPopupTimer, float driftPopupScore, const traffic::AiTraffic *aiTraffic)
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
    DrawText(TextFormat("Lap %d   %s", lapCount + 1, FormatLapTime(lapTime)),
             config::SCREEN_WIDTH - 220, 24, 20, Fade(RAYWHITE, 0.9f));
    if (bestLap >= 0.0f) {
        DrawText(TextFormat("Best   %s", FormatLapTime(bestLap)),
                 config::SCREEN_WIDTH - 220, 50, 18, Fade(Color{ 255, 210, 90, 255 }, 0.9f));
    }

    DrawMinimap(player, config::SCREEN_WIDTH, aiTraffic);

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
}

} // namespace

int main(void)
{
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

    road::BuildCentreline();

    lighting::SceneShader sceneShader;
    sceneShader.Load("shaders/scene.vs", "shaders/scene.fs");

    lighting::SceneShader terrainShader;
    terrainShader.Load("shaders/scene.vs", "shaders/terrain.fs");

    terrain::Terrain ground;
    ground.Load(terrainShader.Get());

    road::RoadMesh roadMesh;
    roadMesh.Load(sceneShader.Get());

    fence::Fence fences;
    fences.Load(sceneShader.Get());

    water::Water lake;
    lake.Load();

    world::World scenery;
    scenery.Load(sceneShader.Get());

    menu::Menu ui;

    car::Car player;
    bool carLoaded = false;
    int loadedCarIndex = -1;
    int loadedColorIndex = -1;
    terrain::Style loadedWorldStyle = terrain::STYLE_HILLS;

    // Traffic mode: a handful of AI cars looping the same circuit. Only
    // loaded (and only costs anything) while actually in that mode.
    traffic::AiTraffic aiTraffic;
    bool trafficLoaded = false;
    constexpr int TRAFFIC_CAR_COUNT = 8;

    cam::GameCamera camera;

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

    // Lap tracking: nearest-centreline-index crossing the start/finish line.
    // "Armed" means the car has gotten roughly halfway round since the last
    // crossing, so slow back-and-forth wiggling right at the line can't
    // rack up laps.
    int lapCount = 0;
    float lapTime = 0.0f;
    float bestLap = -1.0f;
    int lastRoadIdx = -1;
    bool lapArmed = false;

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

    car::Car previewCar;
    bool previewLoaded = false;
    int previewCarIndex = -1;
    int previewColorIndex = -1;

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
            // A changed World style also forces a car reload below (its
            // start position/respawn depend on the new road), same as
            // switching cars already does.
            bool worldChanged = (loadedWorldStyle != ui.SelectedWorldStyle());
            if (worldChanged) {
                terrain::SetStyle(ui.SelectedWorldStyle());
                terrainShader.SetDesertMode(terrain::IsDesert());

                ground.Unload();
                ground.Load(terrainShader.Get());

                road::BuildCentreline();
                roadMesh.Unload();
                roadMesh.Load(sceneShader.Get());

                fences.Unload();
                fences.Load(sceneShader.Get());

                scenery.Unload();
                scenery.Load(sceneShader.Get());

                loadedWorldStyle = ui.SelectedWorldStyle();
            }

            if (!carLoaded || worldChanged || loadedCarIndex != ui.SelectedCar() ||
                loadedColorIndex != ui.SelectedColorIndex()) {
                if (carLoaded) player.Unload();
                player.Load(sceneShader.Get(), ui.SelectedCar(), ResolveColor(ui.SelectedCar(), ui.SelectedColorIndex()));
                road::Sample start = road::StartPoint();
                player.Respawn(start.position, atan2f(start.forward.x, start.forward.z));
                camera.Init(player);
                loadedCarIndex = ui.SelectedCar();
                loadedColorIndex = ui.SelectedColorIndex();
                carLoaded = true;

                if (!everPlayed) {
                    everPlayed = true;
                    hintTimer = 6.0f;
                }

                lapCount = 0;
                lapTime = 0.0f;
                bestLap = -1.0f;
                lastRoadIdx = -1;
                lapArmed = false;
                ghostRecording.clear();
                bestGhost.clear();
            }

            // Traffic mode loads/unloads alongside the car above -- a world
            // change already forces everything else to reload here too, so
            // traffic rebuilds on the new circuit at the same time.
            bool wantTraffic = ui.IsTrafficMode();
            if (wantTraffic && (!trafficLoaded || worldChanged)) {
                if (trafficLoaded) aiTraffic.Unload();
                aiTraffic.Load(sceneShader.Get(), TRAFFIC_CAR_COUNT);
                trafficLoaded = true;
            } else if (!wantTraffic && trafficLoaded) {
                aiTraffic.Unload();
                trafficLoaded = false;
            }

            if (IsKeyPressed(KEY_F1)) showStats = !showStats;

            if (hintTimer > 0.0f) {
                hintTimer -= dt;
                if (input.Down(input::ACTION_ACCELERATE) || input.Down(input::ACTION_BRAKE) ||
                    input.Down(input::ACTION_STEER_LEFT) || input.Down(input::ACTION_STEER_RIGHT) ||
                    input.Down(input::ACTION_HANDBRAKE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    hintTimer = 0.0f;
                }
            }

            if (trafficLoaded) aiTraffic.Update(dt);
            player.Update(dt, input, scenery, fences, trafficLoaded ? &aiTraffic : nullptr);
            camera.Update(player, input, dt);

            {
                const cardb::CarSpec &curSpec = cardb::Roster()[ui.SelectedCar()];
                float speedFactor = fabsf(player.GetSpeed()) / curSpec.maxSpeed;
                bool slip = input.Down(input::ACTION_HANDBRAKE) && fabsf(player.GetSpeed()) > 5.0f;
                engineSound.Update(player.GetRpm(), speedFactor, slip, dt);
                if (player.JustShifted()) audio::PlayShiftClunk();

                if (player.JustHitObstacle()) {
                    audio::PlayCollisionThump(Clamp(speedFactor, 0.0f, 1.0f));
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
                    if (!lapArmed && idx > N / 2) lapArmed = true;
                    if (lapArmed && lastRoadIdx > N * 9 / 10 && idx < N / 10) {
                        bool newBest = (bestLap < 0.0f || lapTime < bestLap);
                        if (newBest) {
                            bestLap = lapTime;
                            bestGhost = ghostRecording; // this lap's recording is the one that just finished
                        }
                        ghostRecording.clear();
                        lapCount++;
                        lapTime = 0.0f;
                        lapArmed = false;

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
                // Session top-speed record.
                float curSpeed = fabsf(player.GetSpeed());
                if (curSpeed > topSpeedRecord + 0.01f) {
                    topSpeedRecord = curSpeed;
                    topSpeedPopupTimer = 1.6f;
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
            }

            Vector3 viewPos = camera.Get().position;
            sceneShader.UpdateView(viewPos);
            terrainShader.UpdateView(viewPos);
            lake.Update((float)GetTime(), viewPos);
        }

        // Garage preview: rendered to its own off-screen target before the
        // real frame starts, then handed to the menu as a texture to draw
        // like any other 2D element.
        bool inGarage = (ui.CurrentScreen() == menu::SCREEN_GARAGE);
        if (inGarage) {
            if (!previewLoaded || previewCarIndex != ui.SelectedCar() || previewColorIndex != ui.SelectedColorIndex()) {
                if (previewLoaded) previewCar.Unload();
                previewCar.Load(previewShader.Get(), ui.SelectedCar(),
                                ResolveColor(ui.SelectedCar(), ui.SelectedColorIndex()));
                previewCar.Respawn(Vector3{ 0.0f, 0.0f, 0.0f }, 0.5f);
                previewCarIndex = ui.SelectedCar();
                previewColorIndex = ui.SelectedColorIndex();
                previewLoaded = true;
            }

            // Menu owns the angle now -- TickGarage auto-orbits it when idle
            // and drag-rotates it when the player holds the mouse down over
            // the preview, so it has to run before this reads it.
            float previewAngle = ui.GaragePreviewAngle();

            const cardb::CarSpec &pspec = cardb::Roster()[ui.SelectedCar()];
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
                Vector3 viewPos = camera.Get().position;
                DrawSky();

                BeginMode3D(camera.Get());
                    ground.Draw();
                    roadMesh.Draw();
                    fences.Draw();
                    scenery.Draw(viewPos, config::DRAW_DISTANCE);
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
                EndMode3D();

                if (playing) {
                    DrawHud(player, camera, scenery.GetVisibleCount(), showStats, input, lapCount, lapTime, bestLap,
                           hintTimer, lastDriftPopupTimer, lastDriftPopupScore, trafficLoaded ? &aiTraffic : nullptr);
                    DrawFirstDriveHint(input, hintTimer, config::SCREEN_WIDTH, config::SCREEN_HEIGHT);

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
            ui.Tick(input, config::SCREEN_WIDTH, config::SCREEN_HEIGHT, carLoaded);
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
    if (previewLoaded) previewCar.Unload();
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
