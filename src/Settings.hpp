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

    // Admin mode: gates things that would otherwise undermine normal
    // play -- right now, just minimap click-to-teleport (which is also
    // the one thing that still invalidates a lap; see Phase 1 in
    // TODO.md). Off by default so lap times/ghosts earned in normal play
    // can't be casually cheated by someone who doesn't know what the
    // toggle does.
    bool adminMode = false;

    // Quality preset: High is the default (the project's stated low-end
    // target was explicitly dropped -- see TODO.md). Low scales back prop
    // density and draw distance for anyone who still wants a lighter-
    // weight option, without the codebase being all-or-nothing about it.
    bool highQuality = true;
};

Options &Get();
void Load();
void Save();
void ResetSound();
void ResetGearbox();
void ResetCar();

} // namespace settings
