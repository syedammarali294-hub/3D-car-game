#include "Input.hpp"

#include "Config.hpp"
#include "raylib.h"
#include <cstdio>
#include <cstring>
#include <string>

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

namespace {

// Stable, machine-readable token per action -- separate from ActionLabel()
// (which has spaces/parens meant for display, not parsing) and, critically,
// independent of the action's position in the enum. The old format stored
// bare key codes in raw Action-enum order with no names at all; inserting
// ACTION_REVERSE into the middle of that enum for this session's gear
// feature silently shifted every binding after it in any save file written
// before that change (boost's key loaded into reverse's slot, horn's into
// boost's, and so on) -- a real, reproduced bug: NOS appeared to stop doing
// anything because it had silently picked up an unrelated/unused key.
// Keying by name instead means future enum insertions can never do this
// again -- an action simply keeps its own saved key, in any file order.
const char *ActionToken(Action action)
{
    switch (action) {
        case ACTION_ACCELERATE:  return "accelerate";
        case ACTION_BRAKE:       return "brake";
        case ACTION_STEER_LEFT:  return "steer_left";
        case ACTION_STEER_RIGHT: return "steer_right";
        case ACTION_HANDBRAKE:   return "handbrake";
        case ACTION_RESET:       return "reset";
        case ACTION_CAMERA:      return "camera";
        case ACTION_GEAR_UP:     return "gear_up";
        case ACTION_GEAR_DOWN:   return "gear_down";
        case ACTION_REVERSE:     return "reverse";
        case ACTION_BOOST:       return "boost";
        case ACTION_HORN:        return "horn";
        case ACTION_AUTODRIVE:   return "autodrive";
        default: return "unknown";
    }
}

} // namespace

void Input::Load()
{
    ResetToDefaults();

    if (!FileExists(config::SETTINGS_FILE)) return;

    char *text = LoadFileText(config::SETTINGS_FILE);
    if (text == nullptr) return;

    // "token value" per line. Also accepts the old bare-integer-per-line
    // format (no name) for one release, so an existing save isn't just
    // silently discarded -- read positionally same as before, but note
    // that format is exactly what caused the bug above, so Save() below
    // never writes it again; the first save after loading an old file
    // upgrades it to the name-keyed format.
    char token[32];
    int value;
    bool anyNamed = false;
    const char *p = text;
    while (*p) {
        if (sscanf(p, "%31s %d", token, &value) == 2) {
            bool matched = false;
            for (int i = 0; i < ACTION_COUNT; i++) {
                if (strcmp(token, ActionToken((Action)i)) == 0) {
                    keys_[i] = value;
                    matched = true;
                    anyNamed = true;
                    break;
                }
            }
            (void)matched;
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }

    if (!anyNamed) {
        // Old positional format: bare key codes in enum order, one line's
        // worth of space-separated ints. Only trustworthy for exactly the
        // enum order it was written against -- reload defaults and parse
        // positionally as a last resort rather than silently keep
        // whatever Load() partially wrote above from failed name matches.
        ResetToDefaults();
        int values[ACTION_COUNT] = { 0 };
        int parsed = 0;
        const char *q = text;
        while (parsed < ACTION_COUNT) {
            int consumed = 0;
            if (sscanf(q, "%d%n", &values[parsed], &consumed) != 1) break;
            q += consumed;
            parsed++;
        }
        if (parsed >= 7) {
            for (int i = 0; i < parsed; i++) {
                if (values[i] > 0) keys_[i] = values[i];
            }
        }
    }

    UnloadFileText(text);
}

void Input::Save() const
{
    std::string buffer;
    char line[48];
    for (int i = 0; i < ACTION_COUNT; i++) {
        snprintf(line, sizeof(line), "%s %d\n", ActionToken((Action)i), keys_[i]);
        buffer += line;
    }
    SaveFileText(config::SETTINGS_FILE, (char *)buffer.c_str());
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
