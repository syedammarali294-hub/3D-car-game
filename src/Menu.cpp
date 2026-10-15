#include "Menu.hpp"

#include "raymath.h"
#include "Config.hpp"
#include "Lighting.hpp"
#include "Engine.hpp"
#include "Gearbox.hpp"
#include "Settings.hpp"
#include "Upgrades.hpp"
#include <cstdio>
#include <string>
#include <unordered_map>

namespace menu {

namespace {

// Per-button hover progress, 0..1, keyed by label text. Persisting this
// across frames (rather than deriving everything from the instantaneous
// hover test) is what makes the grow/colour change read as an eased
// transition instead of an instant snap.
float &HoverProgress(const char *label)
{
    static std::unordered_map<std::string, float> state;
    return state[label];
}

bool ButtonAt(Rectangle r, const char *label, int fontSize)
{
    Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, r);
    bool clicked = hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    float &t = HoverProgress(label);
    t = Clamp(t + (hover ? 1.0f : -1.0f) * GetFrameTime() * 9.0f, 0.0f, 1.0f);
    // Ease rather than a straight lerp, so the motion settles instead of
    // moving at a constant rate right up to the stop.
    float eased = t * t * (3.0f - 2.0f * t);

    Color fill = ColorLerp(Color{ 40, 48, 64, 230 }, Color{ 70, 110, 180, 255 }, eased);
    Color border = ColorLerp(Color{ 90, 100, 120, 255 }, Color{ 150, 190, 255, 255 }, eased);

    // A slight grow-and-lift on hover, plus a quick outward flash on click.
    float grow = eased * 4.0f;
    Rectangle grown = Rectangle{ r.x - grow, r.y - grow * 0.6f, r.width + grow * 2.0f, r.height + grow * 1.2f };

    DrawRectangleRec(grown, fill);
    DrawRectangleLinesEx(grown, 2.0f + eased * 1.5f, border);
    if (clicked) {
        DrawRectangleLinesEx(Rectangle{ grown.x - 4.0f, grown.y - 4.0f, grown.width + 8.0f, grown.height + 8.0f },
                             2.0f, Fade(RAYWHITE, 0.8f));
    }

    int tw = MeasureText(label, fontSize);
    DrawText(label, (int)(r.x + (r.width - tw) * 0.5f), (int)(r.y + (r.height - fontSize) * 0.5f - eased * 2.0f),
             fontSize, RAYWHITE);

    return clicked;
}


// Horizontal 0..1 slider. Dragging keeps tracking the mouse even if it
// strays off the track, and ends on release. Returns true on change.
bool SliderAt(Rectangle r, float &value)
{
    static const float *active = nullptr;
    Vector2 m = GetMousePosition();
    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) active = nullptr;

    Rectangle hit = Rectangle{ r.x - 10.0f, r.y - 12.0f, r.width + 20.0f, r.height + 24.0f };
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(m, hit)) active = &value;

    bool changed = false;
    if (active == &value) {
        float nv = Clamp((m.x - r.x) / r.width, 0.0f, 1.0f);
        if (nv != value) { value = nv; changed = true; }
    }

    bool hover = CheckCollisionPointRec(m, hit) || active == &value;
    float cy = r.y + r.height * 0.5f;
    DrawRectangleRec(Rectangle{ r.x, cy - 3.0f, r.width, 6.0f }, Color{ 40, 48, 64, 255 });
    DrawRectangleRec(Rectangle{ r.x, cy - 3.0f, r.width * value, 6.0f }, Color{ 90, 150, 235, 255 });
    DrawCircle((int)(r.x + r.width * value), (int)cy, hover ? 11.0f : 9.0f,
               hover ? Color{ 190, 220, 255, 255 } : Color{ 150, 190, 255, 255 });
    return changed;
}

// Small helper so every tab lays its rows out the same way.
void RowLabel(const char *text, float x, float y)
{
    DrawText(text, (int)x, (int)y + 10, 20, RAYWHITE);
}

} // namespace

Screen Menu::Tick(input::Input &input, int screenW, int screenH, bool hasSceneBehind)
{
    if (screen_ == SCREEN_PLAYING) {
        // ESC and P both pause; neither quits. (main() clears raylib's default
        // ESC-closes-the-window binding so this can own the key.)
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) screen_ = SCREEN_PAUSED;
        return screen_;
    }

    if (screen_ == SCREEN_PAUSED) {
        if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_P)) { screen_ = SCREEN_PLAYING; return screen_; }
        if (IsKeyPressed(KEY_M)) { screen_ = SCREEN_MAIN; return screen_; }
    }

    if (hasSceneBehind) {
        // The 3D world is already drawn behind this call -- just dim it
        // enough to read text over, instead of blacking it out completely.
        DrawRectangle(0, 0, screenW, screenH, Color{ 10, 14, 20, 150 });
    } else {
        // Nothing behind yet (first launch, before any DRIVE): use a sky
        // gradient instead of a flat near-black screen.
        DrawRectangleGradientV(0, 0, screenW, screenH, lighting::SkyColorTop(), lighting::SkyColorHorizon());
        DrawRectangle(0, 0, screenW, screenH, Color{ 10, 14, 20, 90 });
    }

    if (screen_ != fadeFromScreen_) {
        fadeFromScreen_ = screen_;
        fadeT_ = 0.0f;
    }
    fadeT_ = Clamp(fadeT_ + GetFrameTime() * 5.0f, 0.0f, 1.0f);

    switch (screen_) {
        case SCREEN_MAIN:       TickMain(screenW, screenH); break;
        case SCREEN_MODESELECT: TickModeSelect(screenW, screenH); break;
        case SCREEN_GARAGE:     TickGarage(screenW, screenH); break;
        case SCREEN_SETTINGS:   TickSettings(input, screenW, screenH); break;
        case SCREEN_CREDITS:    TickCredits(screenW, screenH); break;
        case SCREEN_PAUSED:     TickPause(screenW, screenH); break;
        case SCREEN_PLAYING:    break;
    }

    // Fades the whole screen in from black over the switch instead of a
    // hard cut; drawn last so it sits over everything TickX just drew.
    if (fadeT_ < 1.0f) {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 1.0f - fadeT_));
    }

    return screen_;
}

void Menu::TickMain(int w, int h)
{
    DrawText("3D CAR GAME", w / 2 - MeasureText("3D CAR GAME", 46) / 2, (int)(h * 0.16f), 46, RAYWHITE);
    DrawText("An original low-poly open world", w / 2 - MeasureText("An original low-poly open world", 16) / 2,
             (int)(h * 0.16f) + 56, 16, Fade(RAYWHITE, 0.7f));

    // Back to four buttons -- PLAY now opens a mode-select screen instead
    // of the old DRIVE/TRAFFIC MODE/HOW TO PLAY stack living here directly.
    // How To Play moved into Settings as its own tab.
    float cx = w * 0.5f - 130.0f;
    float startY = h * 0.38f;
    float btnH = 54.0f, gap = 68.0f;

    if (ButtonAt(Rectangle{ cx, startY, 260.0f, btnH }, "PLAY", 26)) {
        screen_ = SCREEN_MODESELECT;
    }
    if (ButtonAt(Rectangle{ cx, startY + gap, 260.0f, btnH }, "GARAGE", 26)) {
        screen_ = SCREEN_GARAGE;
    }
    if (ButtonAt(Rectangle{ cx, startY + gap * 2.0f, 260.0f, btnH }, "SETTINGS", 26)) {
        screen_ = SCREEN_SETTINGS;
    }
    if (ButtonAt(Rectangle{ cx, startY + gap * 3.0f, 260.0f, btnH }, "CREDITS", 26)) {
        screen_ = SCREEN_CREDITS;
    }

    // Shortcut letters as their own small badges to the left of each
    // button, rather than parenthesised text stuffed inside the label.
    const char *letters[4] = { "P", "G", "S", "C" };
    for (int i = 0; i < 4; i++) {
        Rectangle badge = Rectangle{ cx - 44.0f, startY + gap * (float)i + 7.0f, 40.0f, 40.0f };
        DrawRectangleRec(badge, Color{ 28, 34, 46, 220 });
        DrawRectangleLinesEx(badge, 1.5f, Color{ 120, 150, 200, 200 });
        int tw = MeasureText(letters[i], 22);
        DrawText(letters[i], (int)(badge.x + (badge.width - tw) * 0.5f), (int)badge.y + 9, 22, RAYWHITE);
    }

    const char *hint = "F11: fullscreen   -   How To Play is a Settings tab";
    DrawText(hint, w / 2 - MeasureText(hint, 15) / 2, h - 30, 15, Fade(RAYWHITE, 0.5f));

    const char *verTxt = TextFormat("v%s", config::GAME_VERSION);
    DrawText(verTxt, w - MeasureText(verTxt, 14) - 14, h - 24, 14, Fade(RAYWHITE, 0.35f));

    // Letter shortcuts mirroring the buttons, only live on this screen --
    // once driving, P/G/S/C belong to other actions instead.
    if (IsKeyPressed(KEY_P)) screen_ = SCREEN_MODESELECT;
    else if (IsKeyPressed(KEY_G)) screen_ = SCREEN_GARAGE;
    else if (IsKeyPressed(KEY_S)) screen_ = SCREEN_SETTINGS;
    else if (IsKeyPressed(KEY_C)) screen_ = SCREEN_CREDITS;
}

void Menu::TickModeSelect(int w, int h)
{
    DrawText("SELECT MODE", w / 2 - MeasureText("SELECT MODE", 36) / 2, (int)(h * 0.22f), 36, RAYWHITE);

    float cx = w * 0.5f - 170.0f;
    float startY = h * 0.40f;

    Rectangle freeDriveBtn = Rectangle{ cx, startY, 340.0f, 70.0f };
    if (ButtonAt(freeDriveBtn, "FREE DRIVE", 26)) {
        trafficMode_ = false;
        screen_ = SCREEN_PLAYING;
    }
    DrawText("The circuit to yourself -- explore and lap freely.",
             w / 2 - MeasureText("The circuit to yourself -- explore and lap freely.", 14) / 2,
             (int)(startY + 78.0f), 14, Fade(RAYWHITE, 0.6f));

    float y2 = startY + 120.0f;
    Rectangle trafficBtn = Rectangle{ cx, y2, 340.0f, 70.0f };
    if (ButtonAt(trafficBtn, "TRAFFIC MODE", 26)) {
        trafficMode_ = true;
        screen_ = SCREEN_PLAYING;
    }
    DrawText("Same circuit, shared with AI traffic -- weave through it.",
             w / 2 - MeasureText("Same circuit, shared with AI traffic -- weave through it.", 14) / 2,
             (int)(y2 + 78.0f), 14, Fade(RAYWHITE, 0.6f));

    if (ButtonAt(Rectangle{ w * 0.5f - 90.0f, y2 + 130.0f, 180.0f, 46.0f }, "BACK", 20)) {
        screen_ = SCREEN_MAIN;
    }

    if (IsKeyPressed(KEY_ESCAPE)) screen_ = SCREEN_MAIN;
}

void Menu::TickGarage(int w, int h)
{
    // Every number below is budgeted against a fixed 720px-tall canvas (the
    // whole frame renders at config::SCREEN_WIDTH/HEIGHT regardless of the
    // real window -- see main.cpp) -- nothing here uses h as a fraction the
    // way the rest of the menu does, specifically so adding the upgrades
    // column later doesn't quietly push the BACK button off the bottom
    // edge again the way the old swatches-then-bars-then-back stack did.
    DrawText("GARAGE", w / 2 - MeasureText("GARAGE", 32) / 2, 18, 32, RAYWHITE);

    const auto &roster = cardb::Roster();
    const int rosterSize = (int)roster.size();
    const cardb::CarSpec &spec = roster[selectedCar_];

    // One big preview instead of a card strip -- a strip of thirteen cards
    // is wider than the screen, and a live 3D view sells the shape far
    // better than a text card ever could.
    float previewW = 400.0f, previewH = 210.0f;
    float previewX = w * 0.5f - previewW * 0.5f;
    float previewY = 58.0f;
    Rectangle previewRect = Rectangle{ previewX, previewY, previewW, previewH };

    DrawRectangleRec(previewRect, Color{ 14, 18, 26, 255 });
    if (garagePreviewReady_) {
        // Render textures are stored upside-down relative to the screen.
        Rectangle src = Rectangle{ 0.0f, 0.0f, (float)garagePreviewTex_.width, -(float)garagePreviewTex_.height };
        DrawTexturePro(garagePreviewTex_, src, previewRect, Vector2{ 0.0f, 0.0f }, 0.0f, WHITE);
    }
    DrawRectangleLinesEx(previewRect, 2.0f, Color{ 90, 98, 115, 255 });

    // Click-drag over the preview rotates it manually; otherwise it
    // auto-orbits so the shape is never shown from one static angle.
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, previewRect) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        previewDragging_ = true;
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        previewDragging_ = false;
    }
    if (previewDragging_) {
        previewAngle_ -= GetMouseDelta().x * 0.012f;
    } else {
        previewAngle_ += GetFrameTime() * 0.45f;
    }
    if (CheckCollisionPointRec(mouse, previewRect)) {
        const char *dragHint = "drag to rotate";
        DrawText(dragHint, (int)(previewX + previewW - MeasureText(dragHint, 14) - 10.0f),
                 (int)(previewY + previewH - 22.0f), 14, Fade(RAYWHITE, 0.6f));
    }

    // Ultra-only cars (the downloaded, high-poly licensed models) are
    // skipped while cycling unless Ultra Graphics is on in Settings > Car --
    // see CarSpec::ultraOnly. If Ultra was just turned off while one of
    // them was selected, this also walks forward off it immediately rather
    // than leaving an unreachable car loaded.
    auto carAllowed = [&](int idx) {
        return settings::Get().ultraGraphics || !roster[idx].ultraOnly;
    };
    if (!carAllowed(selectedCar_)) {
        for (int i = 0; i < rosterSize; i++) {
            int idx = (selectedCar_ + i) % rosterSize;
            if (carAllowed(idx)) { selectedCar_ = idx; break; }
        }
    }

    // Prev/next arrows flank the preview and cycle with wraparound.
    Rectangle prevBtn = Rectangle{ previewX - 76.0f, previewY + previewH * 0.5f - 28.0f, 56.0f, 56.0f };
    Rectangle nextBtn = Rectangle{ previewX + previewW + 20.0f, previewY + previewH * 0.5f - 28.0f, 56.0f, 56.0f };
    if (ButtonAt(prevBtn, "<", 30)) {
        do { selectedCar_ = (selectedCar_ - 1 + rosterSize) % rosterSize; } while (!carAllowed(selectedCar_));
        selectedColorIndex_ = 0;
    }
    if (ButtonAt(nextBtn, ">", 30)) {
        do { selectedCar_ = (selectedCar_ + 1) % rosterSize; } while (!carAllowed(selectedCar_));
        selectedColorIndex_ = 0;
    }

    float infoY = previewY + previewH + 14.0f;
    const char *name = spec.name;
    DrawText(name, w / 2 - MeasureText(name, 24) / 2, (int)infoY, 24, RAYWHITE);
    DrawText(spec.subtitle, w / 2 - MeasureText(spec.subtitle, 15) / 2, (int)infoY + 29, 15, Fade(RAYWHITE, 0.75f));
    // Upgrade-adjusted stats, same numbers the car itself actually drives
    // with -- so maxing out Engine visibly moves the Top Speed/Accel bars.
    upgrades::CarUpgrade &upg = upgrades::Get(selectedCar_);

    // Combined onto one line (car index + lifetime odometer) rather than
    // its own row, so the persistent-odometer addition doesn't need any
    // more of the vertical budget this screen is already tight on.
    float totalKm = upg.totalMeters / 1000.0f;
    const char *countTxt = TextFormat("%d / %d   -   %.1f km driven", selectedCar_ + 1, rosterSize, totalKm);
    DrawText(countTxt, w / 2 - MeasureText(countTxt, 14) / 2, (int)infoY + 49, 14, Fade(RAYWHITE, 0.55f));

    float dispMaxSpeed = spec.maxSpeed * upgrades::EngineMultiplier(upg.engine);
    float dispAccel = spec.acceleration * upgrades::EngineMultiplier(upg.engine);
    float dispTurnRate = spec.turnRate * upgrades::HandlingMultiplier(upg.handling);

    // Stat bars (left) and upgrade controls (right) share a row grid so
    // adding the upgrade column didn't need its own vertical space.
    struct StatBar { const char *label; float t; };
    StatBar bars[3] = {
        { "Top speed", Clamp(dispMaxSpeed / 60.0f, 0.0f, 1.0f) },
        { "Accel",     Clamp(dispAccel / 26.0f, 0.0f, 1.0f) },
        { "Handling",  Clamp(dispTurnRate / 3.2f, 0.0f, 1.0f) },
    };
    float rowsY = infoY + 70.0f;
    float rowH = 30.0f;
    float barW = 150.0f, barH = 10.0f;
    float barsX = w * 0.5f - 260.0f;
    for (int i = 0; i < 3; i++) {
        float by = rowsY + rowH * (float)i;
        DrawText(bars[i].label, (int)barsX, (int)by - 2, 14, Fade(RAYWHITE, 0.8f));
        DrawRectangle((int)(barsX + 86.0f), (int)by, (int)barW, (int)barH, Color{ 40, 44, 54, 255 });
        DrawRectangle((int)(barsX + 86.0f), (int)by, (int)(barW * bars[i].t), (int)barH, Color{ 120, 170, 240, 255 });
    }

    // Upgrades: three tracks (Engine/Handling/Nitro), 0..MAX_LEVEL each,
    // persisted per car (see Upgrades.hpp). "-"/"+" either side of a row of
    // filled pips showing the current level.
    struct UpgradeRow { const char *label; int *level; };
    UpgradeRow upgradeRows[3] = {
        { "Engine",   &upg.engine },
        { "Handling", &upg.handling },
        { "Nitro",    &upg.nitro },
    };
    float upgX = w * 0.5f + 40.0f;
    float pipSize = 16.0f, pipGap = 5.0f;
    for (int i = 0; i < 3; i++) {
        float uy = rowsY + rowH * (float)i;
        DrawText(upgradeRows[i].label, (int)upgX, (int)uy - 2, 14, Fade(RAYWHITE, 0.8f));

        Rectangle minusBtn = Rectangle{ upgX + 70.0f, uy - 3.0f, 22.0f, 22.0f };
        Rectangle plusBtn = Rectangle{ upgX + 70.0f + 22.0f + (float)upgrades::MAX_LEVEL * (pipSize + pipGap) + 6.0f,
                                       uy - 3.0f, 22.0f, 22.0f };
        if (ButtonAt(minusBtn, "-", 16) && *upgradeRows[i].level > 0) {
            (*upgradeRows[i].level)--;
            upgrades::Save();
        }
        for (int p = 0; p < upgrades::MAX_LEVEL; p++) {
            Rectangle pip = Rectangle{ minusBtn.x + 28.0f + (float)p * (pipSize + pipGap), uy - 3.0f, pipSize, pipSize };
            bool filled = p < *upgradeRows[i].level;
            DrawRectangleRec(pip, filled ? Color{ 120, 220, 160, 255 } : Color{ 40, 44, 54, 255 });
            DrawRectangleLinesEx(pip, 1.0f, Color{ 90, 98, 115, 200 });
        }
        if (ButtonAt(plusBtn, "+", 16) && *upgradeRows[i].level < upgrades::MAX_LEVEL) {
            (*upgradeRows[i].level)++;
            upgrades::Save();
        }
    }

    // Colour swatches -- index 0 is that car's own factory paint.
    const auto &palette = cardb::PaintPalette();
    float swY = rowsY + rowH * 3.0f + 14.0f;
    float swSize = 30.0f, swGap = 8.0f;
    float swTotal = (float)palette.size() * swSize + (float)(palette.size() - 1) * swGap;
    float swX = w * 0.5f - swTotal * 0.5f;
    for (int i = 0; i < (int)palette.size(); i++) {
        Rectangle r = Rectangle{ swX + (swSize + swGap) * (float)i, swY, swSize, swSize };
        Color c = (i == 0) ? spec.design.paint : palette[i];
        bool selected = (i == selectedColorIndex_);

        DrawRectangleRec(r, c);
        DrawRectangleLinesEx(r, selected ? 3.0f : 1.0f,
                             selected ? Color{ 255, 255, 255, 255 } : Color{ 20, 20, 24, 200 });

        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            selectedColorIndex_ = i;
        }
    }
    const char *colorLabel = cardb::PaintLabel(selectedColorIndex_);
    DrawText(colorLabel, w / 2 - MeasureText(colorLabel, 16) / 2, (int)(swY + swSize + 8.0f), 16, Fade(RAYWHITE, 0.75f));

    if (ButtonAt(Rectangle{ w * 0.5f - 90.0f, swY + swSize + 42.0f, 180.0f, 48.0f }, "BACK", 22)) {
        screen_ = SCREEN_MAIN;
    }
}

void Menu::TickSettings(input::Input &input, int w, int h)
{
    DrawText("SETTINGS", w / 2 - MeasureText("SETTINGS", 40) / 2, (int)(h * 0.07f), 40, RAYWHITE);

    if (rebindingAction_ >= 0) {
        const char *msg = "Press any key... (ESC cancels)";
        DrawText(msg, w / 2 - MeasureText(msg, 22) / 2, (int)(h * 0.16f), 22, Color{ 255, 210, 90, 255 });

        if (IsKeyPressed(KEY_ESCAPE)) {
            rebindingAction_ = -1;
            return;
        }
        int key = GetKeyPressed();
        if (key != 0) {
            input.SetKey((input::Action)rebindingAction_, key);
            input.Save();
            rebindingAction_ = -1;
        }
        return;
    }

    // ---- tab strip ----
    static const char *tabNames[TAB_COUNT] = { "KEYBINDS", "SOUND", "CAR", "GEAR", "HOW TO PLAY" };
    const float tabW = 150.0f, tabH = 40.0f, tabGap = 10.0f;
    float tabsX = w * 0.5f - (TAB_COUNT * tabW + (TAB_COUNT - 1) * tabGap) * 0.5f;
    float tabsY = h * 0.16f;
    for (int t = 0; t < TAB_COUNT; t++) {
        Rectangle r = Rectangle{ tabsX + t * (tabW + tabGap), tabsY, tabW, tabH };
        if (ButtonAt(r, tabNames[t], 18)) settingsTab_ = t;
        if (t == settingsTab_) {
            DrawRectangleRec(Rectangle{ r.x, r.y + r.height + 4.0f, r.width, 4.0f }, Color{ 150, 190, 255, 255 });
        }
    }
    DrawRectangleRec(Rectangle{ tabsX, tabsY + tabH + 7.0f, TAB_COUNT * tabW + (TAB_COUNT - 1) * tabGap, 1.0f },
                     Color{ 90, 100, 120, 160 });

    float top = tabsY + tabH + 26.0f;
    float bottom = h - 84.0f; // action buttons live below this

    switch (settingsTab_) {
        case TAB_KEYS:  SettingsKeys(input, top, bottom, w); break;
        case TAB_SOUND: SettingsSound(top, w); break;
        case TAB_CAR:   SettingsCar(top, w); break;
        case TAB_GEAR:  SettingsGear(input, top, w); break;
        case TAB_HOWTO: SettingsHowTo(top, w); break;
    }

    // ---- shared buttons: reset applies to the tab you're looking at ----
    // Nothing to reset on the How To Play tab, so its button is skipped
    // rather than left there doing nothing.
    if (settingsTab_ != TAB_HOWTO) {
        if (ButtonAt(Rectangle{ w * 0.5f - 280.0f, bottom + 20.0f, 220.0f, 44.0f }, "RESET THIS TAB", 18)) {
            switch (settingsTab_) {
                case TAB_KEYS:  input.ResetToDefaults(); input.Save(); break;
                case TAB_SOUND: settings::ResetSound(); optionsDirty_ = true; break;
                case TAB_CAR:   settings::ResetCar(); worldStyle_ = terrain::STYLE_HILLS; optionsDirty_ = true; break;
                case TAB_GEAR:  settings::ResetGearbox(); optionsDirty_ = true; break;
                default: break;
            }
        }
    }
    if (ButtonAt(Rectangle{ w * 0.5f + 60.0f, bottom + 20.0f, 220.0f, 44.0f }, "BACK", 20)) {
        settings::Save();
        optionsDirty_ = false;
        screen_ = SCREEN_MAIN;
    }

    if (optionsDirty_ && !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        settings::Save();
        optionsDirty_ = false;
    }
}

void Menu::SettingsKeys(input::Input &input, float top, float bottom, int w)
{
    (void)bottom;
    const float pitch = 44.0f;
    float y = top;
    for (int a = 0; a < input::ACTION_COUNT; a++) {
        Rectangle rowRect = Rectangle{ w * 0.5f - 280.0f, y, 560.0f, 40.0f };
        RowLabel(input::Input::ActionLabel((input::Action)a), rowRect.x, rowRect.y - 4.0f);

        Rectangle keyBtn = Rectangle{ rowRect.x + 380.0f, rowRect.y, 160.0f, 36.0f };
        const char *label = input::Input::KeyLabel(input.GetKey((input::Action)a));
        if (ButtonAt(keyBtn, label, 20)) {
            rebindingAction_ = a;
        }
        y += pitch;
    }
}

void Menu::SettingsSound(float top, int w)
{
    settings::Options &o = settings::Get();
    float x = w * 0.5f - 280.0f;
    float y = top;

    RowLabel("Sound", x, y);
    if (ButtonAt(Rectangle{ x + 380.0f, y, 160.0f, 40.0f }, o.soundOn ? "SOUND: ON" : "SOUND: OFF", 18)) {
        o.soundOn = !o.soundOn;
        optionsDirty_ = true;
    }
    y += 58.0f;

    struct Row { const char *label; float *value; } rows[] = {
        { "Master volume", &o.master },
        { "Engine volume", &o.engine },
        { "Effects volume", &o.effects },
    };
    for (const Row &row : rows) {
        RowLabel(row.label, x, y);
        if (SliderAt(Rectangle{ x + 230.0f, y + 6.0f, 230.0f, 28.0f }, *row.value)) optionsDirty_ = true;
        DrawText(TextFormat("%d%%", (int)(*row.value * 100.0f + 0.5f)), (int)(x + 490.0f), (int)y + 10, 20,
                 o.soundOn ? RAYWHITE : Fade(RAYWHITE, 0.4f));
        y += 52.0f;
    }

    y += 6.0f;
    RowLabel("Gear shift sound", x, y);
    if (ButtonAt(Rectangle{ x + 380.0f, y, 160.0f, 40.0f }, o.shiftSound ? "SHIFT: ON" : "SHIFT: OFF", 18)) {
        o.shiftSound = !o.shiftSound;
        optionsDirty_ = true;
    }
    y += 58.0f;

    if (ButtonAt(Rectangle{ x + 380.0f, y, 160.0f, 40.0f }, "TEST SOUND", 18)) {
        audio::PlayShiftClunk(true);
    }
    DrawText("Engine sound is heard while driving.", (int)x, (int)y + 11, 16, Fade(RAYWHITE, 0.6f));
}

void Menu::SettingsCar(float top, int w)
{
    settings::Options &o = settings::Get();
    float x = w * 0.5f - 280.0f;
    float y = top;

    const cardb::CarSpec &spec = cardb::Roster()[selectedCar_];
    DrawText(spec.name, (int)x, (int)y, 28, RAYWHITE);
    DrawText(TextFormat("%s  -  %d-speed + reverse", gearbox::ClassLabel(spec.gears), spec.gears),
             (int)x, (int)y + 36, 18, Fade(RAYWHITE, 0.8f));
    DrawText(TextFormat("Top speed %d km/h  (%d mph)", (int)(spec.maxSpeed * 3.6f), (int)(spec.maxSpeed * 2.237f)),
             (int)x, (int)y + 62, 18, Fade(RAYWHITE, 0.8f));
    DrawText("Pick a different car in the Garage.", (int)x, (int)y + 88, 16, Fade(RAYWHITE, 0.55f));
    y += 134.0f;

    RowLabel("Speed units", x, y);
    if (ButtonAt(Rectangle{ x + 380.0f, y, 160.0f, 40.0f }, o.useMph ? "UNITS: MPH" : "UNITS: KM/H", 18)) {
        o.useMph = !o.useMph;
        optionsDirty_ = true;
    }
    y += 58.0f;

    RowLabel("World", x, y);
    // Cycling here only changes the setting -- the circuit, fences and
    // scenery are all baked from terrain::HeightAt() once at load time, so
    // the new look applies the next time Drive is pressed, the same way a
    // car swap only takes effect on the next Drive.
    if (ButtonAt(Rectangle{ x + 380.0f, y, 160.0f, 40.0f }, terrain::StyleLabel(worldStyle_), 18)) {
        worldStyle_ = (terrain::Style)((worldStyle_ + 1) % terrain::STYLE_COUNT);
    }
    DrawText("Applies next time you press Drive.", (int)x, (int)y + 44, 14, Fade(RAYWHITE, 0.55f));
    y += 78.0f;

    RowLabel("Graphics", x, y);
    if (ButtonAt(Rectangle{ x + 380.0f, y, 160.0f, 40.0f }, o.ultraGraphics ? "ULTRA" : "LOW", 18)) {
        o.ultraGraphics = !o.ultraGraphics;
        optionsDirty_ = true;
    }
    DrawText("Ultra unlocks 3 downloaded, high-poly licensed cars in the",
             (int)x, (int)y + 44, 14, Fade(RAYWHITE, 0.55f));
    DrawText("Garage -- real meshes, 5-25x the triangle cost of the rest.",
             (int)x, (int)y + 62, 14, Fade(RAYWHITE, 0.55f));
}

void Menu::SettingsGear(const input::Input &input, float top, int w)
{
    settings::Options &o = settings::Get();
    float x = w * 0.5f - 280.0f;
    float y = top;

    RowLabel("Transmission", x, y);
    if (ButtonAt(Rectangle{ x + 330.0f, y, 210.0f, 40.0f }, o.manual ? "MANUAL" : "AUTOMATIC", 18)) {
        o.manual = !o.manual;
        optionsDirty_ = true;
    }
    y += 58.0f;

    RowLabel("Show gear on screen", x, y);
    if (ButtonAt(Rectangle{ x + 380.0f, y, 160.0f, 40.0f }, o.showGearHud ? "GEAR HUD: ON" : "GEAR HUD: OFF", 18)) {
        o.showGearHud = !o.showGearHud;
        optionsDirty_ = true;
    }
    y += 70.0f;

    DrawText(o.manual ? "MANUAL: you shift. Wrong gear = weak pull or the limiter."
                      : "AUTOMATIC: shifts up near the redline, down as you slow.",
             (int)x, (int)y, 17, Color{ 255, 210, 90, 255 });
    y += 30.0f;
    DrawText(TextFormat("Gear up: %s      Gear down: %s      (rebind in KEYBINDS)",
                        input::Input::KeyLabel(input.GetKey(input::ACTION_GEAR_UP)),
                        input::Input::KeyLabel(input.GetKey(input::ACTION_GEAR_DOWN))),
             (int)x, (int)y, 16, Fade(RAYWHITE, 0.85f));
    y += 34.0f;
    DrawText("Reverse: brake from a standstill.", (int)x, (int)y, 16, Fade(RAYWHITE, 0.85f));
    y += 40.0f;

    DrawText("Gears per car class", (int)x, (int)y, 18, RAYWHITE);
    y += 28.0f;
    DrawText("Everyday cars: 5 + reverse", (int)x, (int)y, 16, Fade(RAYWHITE, 0.8f));
    DrawText("Sports cars: 7 + reverse", (int)x, (int)y + 24, 16, Fade(RAYWHITE, 0.8f));
    DrawText("Hypercars: 8 + reverse", (int)x, (int)y + 48, 16, Fade(RAYWHITE, 0.8f));
}

void Menu::TickCredits(int w, int h)
{
    DrawText("CREDITS", w / 2 - MeasureText("CREDITS", 40) / 2, (int)(h * 0.14f), 40, RAYWHITE);
    const char *verTxt = TextFormat("Version %s", config::GAME_VERSION);
    DrawText(verTxt, w / 2 - MeasureText(verTxt, 16) / 2, (int)(h * 0.14f) + 46, 16, Fade(RAYWHITE, 0.6f));

    const char *lines[] = {
        "Engine: raylib (zlib license) - raysan5 and contributors",
        "https://www.raylib.com",
        "",
        "Scenery models (trees, rocks): Kenney Nature Kit",
        "https://kenney.nl  -  CC0 1.0, no attribution required",
        "",
        "Cars: original low-poly bodies generated for this game.",
        "Not modeled on, or copies of, any manufacturer's design.",
        "",
        "Built with raylib + a custom procedural mesh pipeline.",
    };

    int y = (int)(h * 0.24f);
    for (const char *line : lines) {
        if (line[0] != '\0') {
            DrawText(line, w / 2 - MeasureText(line, 18) / 2, y, 18, Fade(RAYWHITE, 0.85f));
        }
        y += 26;
    }

    // Three real, downloaded car models -- decimated and gated behind
    // Ultra Graphics (Settings > Car), but still shipped as part of the
    // game, so their exact required attribution goes here, verbatim.
    y += 6;
    const char *licensed[] = {
        "Downloaded models (Ultra Graphics only):",
        "\"BMW Modified Racing Car | BMW M3 E30\" by DevPoly3D",
        "skfb.ly/pFIqo - CC BY 4.0",
        "\"BMW M3 Competition\" by VTX - skfb.ly/pMrWu",
        "\"BMW S1000 RR\" by VTX - skfb.ly/pMyBS",
        "Both VTX models: CC BY-NC-SA 4.0 (NonCommercial -- this project",
        "must stay free/non-commercial for as long as they're included)",
    };
    for (const char *line : licensed) {
        DrawText(line, w / 2 - MeasureText(line, 14) / 2, y, 14, Fade(RAYWHITE, 0.7f));
        y += 20;
    }

    if (ButtonAt(Rectangle{ w * 0.5f - 90.0f, h - 110.0f, 180.0f, 48.0f }, "BACK", 22)) {
        screen_ = SCREEN_MAIN;
    }
}

// "How to play" -- a straightforward written guide rather than a literal AI
// assistant: this is an offline raylib game with no language model or
// network access built into it, so a chat-style "AI" that answers free-form
// questions about the game isn't something this codebase can do honestly.
// What it *can* do, and does here, is explain every mechanic clearly in one
// place, in plain language, the first time someone might want it.
// Lives in Settings now (its own tab) rather than as a screen reached
// straight from the main menu, so the home screen stays four buttons.
void Menu::SettingsHowTo(float top, int w)
{
    // Fixed-size arrays here previously (lines[3]) -- switched to an
    // explicit count after a 4th line added to DRIVING overflowed it; this
    // way each section can hold as many lines as it actually needs.
    struct Section { const char *title; const char *lines[4]; int lineCount; };
    Section sections[] = {
        { "DRIVING", {
            "W/A/S/D (or arrow keys) to accelerate, steer and brake.",
            "Space is the handbrake -- hold it into a turn to drift.",
            "Backspace respawns on the road if you get stuck.",
            "F toggles auto-drive -- touching the wheel hands it back." }, 4 },
        { "GEARS", {
            "Automatic by default. Manual below switches gear keys on.",
            "Manual: Left Shift shifts up, Left Ctrl shifts down.",
            "Everyday cars run 5 gears, sports/hyper cars run 7-8." }, 3 },
        { "BOOST & EXTRAS", {
            "N fires NOS boost -- a limited meter, refills when not used.",
            "H sounds the horn. C cycles camera. P or ESC pauses.",
            "F1 toggles stats, F11 toggles fullscreen." }, 3 },
        { "MODES", {
            "Free Drive: the circuit to yourself, explore and lap.",
            "Traffic Mode: the same circuit with AI cars sharing it --",
            "weave through them, or a bump costs you some speed." }, 3 },
        { "GARAGE", {
            "Pick from the full roster, repaint it, and spend upgrade",
            "points on Engine / Handling / Nitro -- saved per car, so",
            "a build sticks around for next time." }, 3 },
    };

    float y = top;
    for (const Section &s : sections) {
        DrawText(s.title, (int)(w * 0.5f - 280.0f), (int)y, 17, Color{ 150, 190, 255, 255 });
        y += 22.0f;
        for (int i = 0; i < s.lineCount; i++) {
            DrawText(s.lines[i], (int)(w * 0.5f - 262.0f), (int)y, 14, Fade(RAYWHITE, 0.85f));
            y += 18.0f;
        }
        y += 10.0f;
    }
}

void Menu::TickPause(int w, int h)
{
    DrawText("PAUSED", w / 2 - MeasureText("PAUSED", 48) / 2, (int)(h * 0.24f), 48, RAYWHITE);
    const char *hint = "P or ESC: resume     M: main menu";
    DrawText(hint, w / 2 - MeasureText(hint, 18) / 2, (int)(h * 0.24f) + 60, 18, Fade(RAYWHITE, 0.7f));

    float cx = w * 0.5f - 130.0f;
    if (ButtonAt(Rectangle{ cx, h * 0.44f, 260.0f, 56.0f }, "RESUME", 28)) {
        screen_ = SCREEN_PLAYING;
    }
    if (ButtonAt(Rectangle{ cx, h * 0.44f + 72.0f, 260.0f, 56.0f }, "SETTINGS", 28)) {
        screen_ = SCREEN_SETTINGS;
    }
    if (ButtonAt(Rectangle{ cx, h * 0.44f + 144.0f, 260.0f, 56.0f }, "MAIN MENU", 28)) {
        screen_ = SCREEN_MAIN;
    }
}

} // namespace menu
