#include "Settings.hpp"

#include "Config.hpp"
#include "raylib.h"
#include <cstdio>
#include <cstring>

namespace settings {

namespace {
Options g_options;
float Clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
} // namespace

Options &Get() { return g_options; }

void ResetSound()
{
    Options d;
    g_options.soundOn = d.soundOn;
    g_options.master = d.master;
    g_options.engine = d.engine;
    g_options.effects = d.effects;
    g_options.shiftSound = d.shiftSound;
}

void ResetGearbox()
{
    Options d;
    g_options.manual = d.manual;
    g_options.showGearHud = d.showGearHud;
}

void ResetCar()
{
    Options d;
    g_options.useMph = d.useMph;
    g_options.ultraGraphics = d.ultraGraphics;
}

void Load()
{
    g_options = Options{};
    if (!FileExists(config::OPTIONS_FILE)) return;

    char *text = LoadFileText(config::OPTIONS_FILE);
    if (text == nullptr) return;

    char name[32];
    float value;
    const char *p = text;
    while (*p) {
        if (sscanf(p, "%31s %f", name, &value) == 2) {
            if      (!strcmp(name, "soundOn"))     g_options.soundOn = value != 0.0f;
            else if (!strcmp(name, "master"))      g_options.master = Clamp01(value);
            else if (!strcmp(name, "engine"))      g_options.engine = Clamp01(value);
            else if (!strcmp(name, "effects"))     g_options.effects = Clamp01(value);
            else if (!strcmp(name, "shiftSound"))  g_options.shiftSound = value != 0.0f;
            else if (!strcmp(name, "manual"))      g_options.manual = value != 0.0f;
            else if (!strcmp(name, "showGearHud")) g_options.showGearHud = value != 0.0f;
            else if (!strcmp(name, "useMph"))      g_options.useMph = value != 0.0f;
            else if (!strcmp(name, "ultraGraphics")) g_options.ultraGraphics = value != 0.0f;
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    UnloadFileText(text);
}

void Save()
{
    char buffer[448];
    snprintf(buffer, sizeof(buffer),
             "soundOn %d\nmaster %.2f\nengine %.2f\neffects %.2f\nshiftSound %d\n"
             "manual %d\nshowGearHud %d\nuseMph %d\nultraGraphics %d\n",
             g_options.soundOn, g_options.master, g_options.engine, g_options.effects,
             g_options.shiftSound, g_options.manual, g_options.showGearHud, g_options.useMph,
             g_options.ultraGraphics);
    SaveFileText(config::OPTIONS_FILE, buffer);
}

} // namespace settings
