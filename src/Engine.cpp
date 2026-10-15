#include "Engine.hpp"

#include "Settings.hpp"
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

namespace audio {

namespace {

constexpr int SAMPLE_RATE = 44100;
constexpr int CHANNELS = 1;
constexpr int FRAMES_PER_CHUNK = 1470; // 1/30s at 44.1kHz -- a few chunks queued per frame is plenty

float Saw(float phaseCycles)
{
    float p = phaseCycles - floorf(phaseCycles);
    return 2.0f * p - 1.0f;
}

float WhiteNoise()
{
    return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

} // namespace

struct EngineSound::Impl {
    AudioStream stream{};
    bool loaded = false;

    double phase1 = 0.0; // fundamental
    double phase2 = 0.0; // sub-octave, gives the drone body
    float smoothedFreq = 40.0f;
    float smoothedSlip = 0.0f;
    float windLP = 0.0f; // one-pole low-pass state, turns hiss into a wind "whoosh"
};

void EngineSound::Load()
{
    impl_ = new Impl();
    impl_->stream = LoadAudioStream(SAMPLE_RATE, 16, CHANNELS);
    SetAudioStreamVolume(impl_->stream, 0.0f);
    PlayAudioStream(impl_->stream);
    impl_->loaded = true;
}

void EngineSound::Unload()
{
    if (!impl_) return;
    if (impl_->loaded) UnloadAudioStream(impl_->stream);
    delete impl_;
    impl_ = nullptr;
}

void EngineSound::Update(float rpm, float speedFactor, bool slip, float dt)
{
    if (!impl_ || !impl_->loaded) return;

    // An idle note even at a standstill, sweeping up with engine rpm (which
    // the gearbox derives from speed in the current gear).
    float targetFreq = Lerp(42.0f, 150.0f, Clamp(rpm, 0.0f, 1.15f));
    impl_->smoothedFreq = Lerp(impl_->smoothedFreq, targetFreq, Clamp(dt * 6.0f, 0.0f, 1.0f));
    impl_->smoothedSlip = Lerp(impl_->smoothedSlip, slip ? 1.0f : 0.0f, Clamp(dt * 10.0f, 0.0f, 1.0f));

    SetAudioStreamVolume(impl_->stream,
                         Clamp(0.14f + speedFactor * 0.10f, 0.0f, 0.3f) * settings::Get().engine);

    // Keep the queue topped up rather than pushing exactly one chunk, so a
    // slow frame never starves the stream into audible silence.
    int queued = 0;
    while (IsAudioStreamProcessed(impl_->stream) && queued < 3) {
        short buffer[FRAMES_PER_CHUNK];
        double step1 = (double)impl_->smoothedFreq / SAMPLE_RATE;
        double step2 = (double)impl_->smoothedFreq * 0.5 / SAMPLE_RATE;

        for (int i = 0; i < FRAMES_PER_CHUNK; i++) {
            impl_->phase1 += step1;
            impl_->phase2 += step2;
            if (impl_->phase1 >= 1.0) impl_->phase1 -= 1.0;
            if (impl_->phase2 >= 1.0) impl_->phase2 -= 1.0;

            float tone = Saw((float)impl_->phase1) * 0.55f + Saw((float)impl_->phase2) * 0.30f;
            float rumble = WhiteNoise() * (0.10f + speedFactor * 0.08f);
            float squeal = WhiteNoise() * impl_->smoothedSlip * 0.35f;

            // Ambient wind: filtered noise (a one-pole low-pass turns flat
            // white noise into a wind-like whoosh), growing with the square
            // of speed so it's inaudible pottering around and genuinely
            // present at speed.
            impl_->windLP += (WhiteNoise() - impl_->windLP) * 0.06f;
            float wind = impl_->windLP * speedFactor * speedFactor * 0.30f;

            float s = tone * (0.55f + speedFactor * 0.25f) + rumble + squeal + wind;
            s = Clamp(s, -1.0f, 1.0f);
            buffer[i] = (short)(s * 22000.0f);
        }

        UpdateAudioStream(impl_->stream, buffer, FRAMES_PER_CHUNK);
        queued++;
    }
}

namespace {

Sound g_clunk{};
bool g_clunkLoaded = false;

// Baked once: a short low "thunk" plus a bright click on top, like a
// shifter seating into a gate.
void BakeClunk()
{
    const int frames = (int)(SAMPLE_RATE * 0.09f);
    Wave wave{};
    wave.frameCount = frames;
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    short *data = (short *)MemAlloc(frames * sizeof(short));

    double phase = 0.0;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)frames;
        float env = powf(1.0f - t, 2.5f);
        phase += (95.0 - 40.0 * t) / SAMPLE_RATE;   // pitch falls as it settles
        if (phase >= 1.0) phase -= 1.0;
        float thunk = sinf((float)phase * 6.2831853f) * 0.8f;
        float click = (i < 90) ? WhiteNoise() * (1.0f - i / 90.0f) * 0.7f : 0.0f;
        data[i] = (short)(Clamp((thunk + click) * env, -1.0f, 1.0f) * 26000.0f);
    }
    wave.data = data;
    g_clunk = LoadSoundFromWave(wave);
    g_clunkLoaded = true;
    UnloadWave(wave);
}

} // namespace

void Init()
{
    InitAudioDevice();
    BakeClunk();
}

void Shutdown()
{
    if (g_clunkLoaded) UnloadSound(g_clunk);
    g_clunkLoaded = false;
    CloseAudioDevice();
}

void ApplySettings()
{
    const settings::Options &o = settings::Get();
    SetMasterVolume(o.soundOn ? o.master : 0.0f);
}

void PlayShiftClunk(bool force)
{
    const settings::Options &o = settings::Get();
    if (!g_clunkLoaded || !o.soundOn || (!o.shiftSound && !force)) return;
    SetSoundVolume(g_clunk, o.effects);
    PlaySound(g_clunk);
}

void PlayCollisionThump(float impactFactor)
{
    impactFactor = Clamp(impactFactor, 0.0f, 1.0f);

    const int frames = (int)(SAMPLE_RATE * 0.22f);
    Wave wave{};
    wave.frameCount = frames;
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    short *data = (short *)MemAlloc(frames * sizeof(short));

    // Decaying filtered-noise thump, pitched a little higher for a harder hit.
    float freq = 60.0f + impactFactor * 40.0f;
    double phase = 0.0;
    double step = freq / SAMPLE_RATE;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)frames;
        float envelope = powf(1.0f - t, 3.0f);
        phase += step;
        if (phase >= 1.0) phase -= 1.0;
        float body = Saw((float)phase) * 0.6f;
        float noise = WhiteNoise() * 0.5f;
        float s = (body + noise) * envelope * (0.35f + impactFactor * 0.5f);
        s = Clamp(s, -1.0f, 1.0f);
        data[i] = (short)(s * 28000.0f);
    }
    wave.data = data;

    Sound sound = LoadSoundFromWave(wave);
    SetSoundVolume(sound, settings::Get().effects);
    PlaySound(sound);
    // Leaked intentionally for the lifetime of this one playback -- raylib
    // has no "unload after it finishes" hook, and these are rare, tiny
    // (under 20KB) allocations. A pooled set of pre-baked thump Sounds
    // would be the tidier fix if this ever needs to happen often.
    UnloadWave(wave);
}

void PlayHorn()
{
    const settings::Options &o = settings::Get();
    if (!o.soundOn) return;

    const int frames = (int)(SAMPLE_RATE * 0.35f);
    Wave wave{};
    wave.frameCount = frames;
    wave.sampleRate = SAMPLE_RATE;
    wave.sampleSize = 16;
    wave.channels = 1;
    short *data = (short *)MemAlloc(frames * sizeof(short));

    // Two slightly-detuned square-ish tones, the classic two-horn car honk.
    double phaseA = 0.0, phaseB = 0.0;
    double stepA = 310.0 / SAMPLE_RATE;
    double stepB = 415.0 / SAMPLE_RATE;
    for (int i = 0; i < frames; i++) {
        float t = (float)i / (float)frames;
        // Quick attack, held, short release -- a honk, not a siren.
        float env = (t < 0.04f) ? t / 0.04f : (t > 0.85f ? (1.0f - t) / 0.15f : 1.0f);
        phaseA += stepA; if (phaseA >= 1.0) phaseA -= 1.0;
        phaseB += stepB; if (phaseB >= 1.0) phaseB -= 1.0;
        float toneA = Saw((float)phaseA) * 0.5f;
        float toneB = Saw((float)phaseB) * 0.5f;
        float s = Clamp((toneA + toneB) * env, -1.0f, 1.0f);
        data[i] = (short)(s * 26000.0f);
    }
    wave.data = data;

    Sound sound = LoadSoundFromWave(wave);
    SetSoundVolume(sound, o.effects);
    PlaySound(sound);
    UnloadWave(wave); // see PlayCollisionThump -- same deliberate one-shot leak
}

} // namespace audio
