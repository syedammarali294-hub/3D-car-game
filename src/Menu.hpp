#pragma once

#include "CarDatabase.hpp"
#include "Input.hpp"
#include "Terrain.hpp"
#include "Track.hpp"
#include "raylib.h"
#include <string>
#include <vector>

namespace menu {

enum Screen {
    SCREEN_MAIN = 0,
    SCREEN_MODESELECT, // PLAY opens this instead of jumping straight to Free Drive
    SCREEN_GARAGE,
    SCREEN_SETTINGS,
    SCREEN_CREDITS,
    SCREEN_PLAYING,
    SCREEN_PAUSED
};

// Drives the main menu, garage, key-rebind settings and credits screens.
// Tick() both reads input AND draws, and must be called once per frame
// from inside BeginDrawing/EndDrawing -- raylib's immediate-mode 2D calls
// only land in the frame that is actually being presented, so splitting
// "handle clicks" and "draw the result" across two separate calls (one of
// them outside the frame bracket) silently drops everything that was drawn
// outside it.
class Menu {
public:
    Screen Tick(input::Input &input, int screenW, int screenH, bool hasSceneBehind);

    int SelectedCar() const { return selectedCar_; }
    int SelectedColorIndex() const { return selectedColorIndex_; }
    terrain::Style SelectedWorldStyle() const { return worldStyle_; }
    // TRACK_CUSTOM whenever a .cgmap is the active selection (its data is
    // already loaded into track::Get(TRACK_CUSTOM) by the time this is
    // read -- see TickModeSelect's cycle logic), otherwise one of the 3
    // built-in tracks.
    track::TrackId SelectedTrack() const { return (customMapIndex_ >= 0) ? track::TRACK_CUSTOM : selectedTrack_; }
    bool IsLive() const { return screen_ == SCREEN_PLAYING; }
    bool IsTrafficMode() const { return trafficMode_; }
    Screen CurrentScreen() const { return screen_; }

    // main.cpp renders the selected car into this each frame the garage is
    // open (it owns the GPU resources; Menu just draws the result).
    void SetGaragePreview(Texture2D tex) { garagePreviewTex_ = tex; garagePreviewReady_ = true; }

    // Double-click-a-.cgmap-file entry point: called once from main() if a
    // map path was passed on the command line. Loads it as the active
    // custom track and jumps straight to the mode-select screen (skipping
    // the main menu), same as the plan's "open a .map file -> straight to
    // car select -> drive" asked for. Returns false if the file didn't
    // load (main() still shows the normal main menu in that case).
    bool StartWithCustomMap(const std::string &path);

    // Garage preview camera angle -- auto-orbits when idle, drag-rotated by
    // the player when the mouse is held down over the preview pane. Read by
    // main.cpp to place the preview camera; driven by TickGarage.
    float GaragePreviewAngle() const { return previewAngle_; }

private:
    void TickMain(int w, int h);
    void TickModeSelect(int w, int h);
    void TickGarage(int w, int h);
    void TickSettings(input::Input &input, int w, int h);
    void TickCredits(int w, int h);
    void TickPause(int w, int h);

    Screen screen_ = SCREEN_MAIN;
    int selectedCar_ = 0;
    int selectedColorIndex_ = 0; // 0 = that car's factory paint
    terrain::Style worldStyle_ = terrain::STYLE_HILLS;
    track::TrackId selectedTrack_ = track::TRACK_LONG; // defaults to the original circuit

    // Custom .cgmap cycling: the picker in TickModeSelect walks
    // SMALL -> LONG -> LONGEST -> each file in customMapPaths_ -> back to
    // SMALL. -1 means "on one of the 3 built-in tracks, not a custom one".
    std::string fileAssocMessage_; // feedback shown under the Settings > Car association button
    std::string exportMessage_;    // feedback shown under the mode-select EXPORT button
    std::vector<std::string> customMapPaths_;
    bool customMapsScanned_ = false; // ListMapFiles() is a directory read -- do it once, not every frame
    int customMapIndex_ = -1;
    bool trafficMode_ = false;

    float previewAngle_ = 0.4f;
    bool previewDragging_ = false;

    int rebindingAction_ = -1; // -1 when not waiting for a key press

    // Settings screen is split into tabs so it doesn't become one long list.
    enum SettingsTab { TAB_KEYS = 0, TAB_SOUND, TAB_CAR, TAB_GEAR, TAB_HOWTO, TAB_COUNT };
    int settingsTab_ = TAB_KEYS;
    bool optionsDirty_ = false; // saved once the mouse is released, not every drag frame
    float keysScroll_ = 0.0f; // pixels scrolled down the Keybinds list; clamped each frame to content height
    void SettingsKeys(input::Input &input, float top, float bottom, int w);
    void SettingsSound(float top, int w);
    void SettingsCar(float top, int w);
    void SettingsGear(const input::Input &input, float top, int w);
    void SettingsHowTo(float top, int w);

    Texture2D garagePreviewTex_{};
    bool garagePreviewReady_ = false;

    // Drives the fade-in whenever screen_ changes, so switching menus
    // reads as a soft cut instead of an instant swap.
    Screen fadeFromScreen_ = SCREEN_MAIN;
    float fadeT_ = 1.0f;
};

} // namespace menu
