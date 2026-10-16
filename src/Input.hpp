#pragma once

namespace input {

enum Action {
    ACTION_ACCELERATE = 0,
    ACTION_BRAKE,
    ACTION_STEER_LEFT,
    ACTION_STEER_RIGHT,
    ACTION_HANDBRAKE,
    ACTION_RESET,
    ACTION_CAMERA,
    ACTION_GEAR_UP,
    ACTION_GEAR_DOWN,
    ACTION_REVERSE,
    ACTION_BOOST,
    ACTION_HORN,
    ACTION_AUTODRIVE,
    ACTION_COUNT
};

// Rebindable key map, persisted to disk so settings survive a restart.
class Input {
public:
    void Load();
    void Save() const;
    void ResetToDefaults();

    bool Down(Action action) const;
    bool Pressed(Action action) const;

    int GetKey(Action action) const { return keys_[action]; }
    void SetKey(Action action, int key) { keys_[action] = key; }

    static const char *ActionLabel(Action action);
    static const char *KeyLabel(int key);

private:
    int keys_[ACTION_COUNT] = { 0 };
};

} // namespace input
