#pragma once

// Player options that aren't key bindings (those live in Input). Persisted to
// options.cfg as simple "name value" lines, so a missing or older file just
// falls back to the defaults below.
namespace settings {

struct Options {
    // Sound
    bool soundOn = true;
    float master = 0.8f;   // 0..1, everything
    float engine = 0.8f;   // 0..1, engine drone + tyre squeal
    float effects = 0.8f;  // 0..1, collision thumps + gear clunks
    bool shiftSound = true;

    // Gearbox
    bool manual = false;       // false = automatic
    bool showGearHud = true;

    // Car / display
    bool useMph = false;

    // Graphics quality. Low is the only tier the project's stated target
    // (Intel HD 4000 class, see TODO.md) is sized for. Ultra exists
    // specifically to gate the handful of downloaded, high-poly licensed
    // car models (tens of thousands of triangles each, vs ~2-3k for every
    // procedural/Kenney car) behind an explicit opt-in, rather than having
    // them always loaded and quietly tanking frame rate on the low-end
    // hardware this project otherwise targets.
    bool ultraGraphics = false;
};

Options &Get();
void Load();
void Save();
void ResetSound();
void ResetGearbox();
void ResetCar();

} // namespace settings
