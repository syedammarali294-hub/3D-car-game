#pragma once

namespace audio {

// All sound in this project is synthesised in code at runtime (a phase-
// accumulated sawtooth engine drone, filtered noise for tyre squeal and
// collision thumps) rather than sampled from any external recording, so
// there is nothing here to license or credit beyond raylib's own audio
// backend.

void Init();    // call once, after InitWindow
void Shutdown(); // call once, before CloseWindow

// Pushes the Sound settings (on/off + master volume) to the audio device.
// Cheap; call every frame so a slider drag is heard immediately.
void ApplySettings();

// Short mechanical clunk for a gear change, scaled by the Effects volume.
// Silent when gear-shift sound is turned off in Settings.
void PlayShiftClunk(bool force = false); // force: ignore the gear-shift-sound toggle (Settings test button)

// Continuous engine drone, fed a fresh buffer every frame via raylib's
// AudioStream so pitch changes smoothly with no restart clicks.
class EngineSound {
public:
    void Load();
    void Unload();

    // rpm: engine speed 0..~1.1 from the gearbox -- sets the pitch, so it
    // climbs through each gear and drops on an upshift. speedFactor:
    // |car speed| / car max speed, roughly 0..1, adds body to the note.
    // slip: true while the handbrake is scrubbing the tyres at speed, layers
    // in a squeal. Call every frame while driving; simply stop calling it
    // (menu/pause) to let the engine go silent.
    void Update(float rpm, float speedFactor, bool slip, float dt);

private:
    struct Impl;
    Impl *impl_ = nullptr;
};

// One-shot synthesised thump, played when the car hits a tree or fence.
// impactFactor (0..1) scales both volume and pitch -- a glancing bump at
// low speed sounds different from slamming in at full speed.
void PlayCollisionThump(float impactFactor);

// Two-tone synthesised horn honk.
void PlayHorn();

} // namespace audio
