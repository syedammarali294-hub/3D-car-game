#include "Input.hpp"

#include "Config.hpp"
#include "raylib.h"
#include <cstdio>
#include <cstring>

namespace input {

void Input::ResetToDefaults()
{
    keys_[ACTION_ACCELERATE] = KEY_W;
    keys_[ACTION_BRAKE] = KEY_S;
    keys_[ACTION_STEER_LEFT] = KEY_A;
    keys_[ACTION_STEER_RIGHT] = KEY_D;
    keys_[ACTION_HANDBRAKE] = KEY_SPACE;
    keys_[ACTION_RESET] = KEY_BACKSPACE;
    keys_[ACTION_CAMERA] = KEY_C;
    keys_[ACTION_GEAR_UP] = KEY_LEFT_SHIFT;
    keys_[ACTION_GEAR_DOWN] = KEY_LEFT_CONTROL;
    keys_[ACTION_REVERSE] = KEY_T;
    keys_[ACTION_BOOST] = KEY_N;
    keys_[ACTION_HORN] = KEY_H;
    keys_[ACTION_AUTODRIVE] = KEY_F;
}

void Input::Load()
{
    ResetToDefaults();

    if (!FileExists(config::SETTINGS_FILE)) return;

    char *text = LoadFileText(config::SETTINGS_FILE);
    if (text == nullptr) return;

    // Space-separated key codes in Action order. Older files only have the
    // first 7 (before the gear keys existed) -- those still load, and the
    // new actions keep their defaults.
    int values[ACTION_COUNT] = { 0 };
    int parsed = 0;
    const char *p = text;
    while (parsed < ACTION_COUNT) {
        int consumed = 0;
        if (sscanf(p, "%d%n", &values[parsed], &consumed) != 1) break;
        p += consumed;
        parsed++;
    }

    if (parsed >= 7) {
        for (int i = 0; i < parsed; i++) {
            if (values[i] > 0) keys_[i] = values[i];
        }
    }

    UnloadFileText(text);
}

void Input::Save() const
{
    char buffer[160];
    int n = 0;
    for (int i = 0; i < ACTION_COUNT; i++) {
        n += snprintf(buffer + n, sizeof(buffer) - n, i ? " %d" : "%d", keys_[i]);
    }
    SaveFileText(config::SETTINGS_FILE, buffer);
}

bool Input::Down(Action action) const
{
    if (IsKeyDown(keys_[action])) return true;

    // Arrow keys always mirror the driving actions as a fallback.
    switch (action) {
        case ACTION_ACCELERATE:  return IsKeyDown(KEY_UP);
        case ACTION_BRAKE:       return IsKeyDown(KEY_DOWN);
        case ACTION_STEER_LEFT:  return IsKeyDown(KEY_LEFT);
        case ACTION_STEER_RIGHT: return IsKeyDown(KEY_RIGHT);
        default: return false;
    }
}

bool Input::Pressed(Action action) const
{
    return IsKeyPressed(keys_[action]);
}

const char *Input::ActionLabel(Action action)
{
    switch (action) {
        case ACTION_ACCELERATE:  return "Accelerate";
        case ACTION_BRAKE:       return "Brake / Reverse";
        case ACTION_STEER_LEFT:  return "Steer Left";
        case ACTION_STEER_RIGHT: return "Steer Right";
        case ACTION_HANDBRAKE:   return "Handbrake";
        case ACTION_RESET:       return "Respawn Car";
        case ACTION_CAMERA:      return "Change Camera";
        case ACTION_GEAR_UP:     return "Gear Up (manual)";
        case ACTION_GEAR_DOWN:   return "Gear Down (manual)";
        case ACTION_REVERSE:     return "Reverse (manual)";
        case ACTION_BOOST:       return "Boost (NOS)";
        case ACTION_HORN:        return "Horn";
        case ACTION_AUTODRIVE:   return "Auto-Drive Toggle";
        default: return "?";
    }
}

const char *Input::KeyLabel(int key)
{
    // A ring of buffers, not one shared static -- a single DrawText/
    // TextFormat line routinely calls KeyLabel several times as arguments
    // to the same call (e.g. the "W/A/S/D: drive" line), and C++ does not
    // guarantee an evaluation order for those arguments. With one shared
    // buffer, every call aliased the same memory, so all of them ended up
    // showing whichever key happened to be formatted last (every letter on
    // the HUD read as the same one). Eight slots is comfortably more than
    // any single line here uses.
    static char buffers[8][16];
    static int slot = 0;
    char *buffer = buffers[slot];
    slot = (slot + 1) % 8;

    switch (key) {
        case KEY_SPACE:     return "SPACE";
        case KEY_BACKSPACE: return "BKSP";
        case KEY_ENTER:     return "ENTER";
        case KEY_TAB:       return "TAB";
        case KEY_LEFT_SHIFT:  return "LSHIFT";
        case KEY_RIGHT_SHIFT: return "RSHIFT";
        case KEY_LEFT_CONTROL:  return "LCTRL";
        case KEY_RIGHT_CONTROL: return "RCTRL";
        case KEY_UP:    return "UP";
        case KEY_DOWN:  return "DOWN";
        case KEY_LEFT:  return "LEFT";
        case KEY_RIGHT: return "RIGHT";
        default: break;
    }

    if ((key >= KEY_A && key <= KEY_Z) || (key >= KEY_ZERO && key <= KEY_NINE)) {
        buffer[0] = (char)key;
        buffer[1] = '\0';
        return buffer;
    }

    snprintf(buffer, sizeof(buffer), "#%d", key);
    return buffer;
}

} // namespace input
