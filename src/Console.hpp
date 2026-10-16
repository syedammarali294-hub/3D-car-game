#pragma once

#include <string>
#include <vector>

// Minecraft/Source-engine-style dev console: backtick toggles it open,
// typed text + Enter becomes a command main.cpp parses and acts on (this
// class only captures text and keeps a scrollback log -- it has no idea
// what "/gamemode spectator" means, same split as Menu.cpp drawing a
// button vs. main.cpp deciding what the click does).
namespace console {

class Console {
public:
    // Opens pre-filled with "/" (same as typing the key that opened it),
    // so the console always reads as "you're writing a command" the
    // moment it appears, the way Minecraft's chat does for "/".
    void Open() { open_ = true; input_ = "/"; }
    void Close() { open_ = false; input_.clear(); }
    bool IsOpen() const { return open_; }

    // Captures keystrokes into the input line when open; Enter queues the
    // typed line as a pending command (also logged, like a real console
    // echoing what you typed) and clears the line. Call once per frame.
    void Update();

    void Draw(int screenW, int screenH) const;

    // Appends a line to the scrollback without it being a command -- for
    // command results/errors main.cpp wants shown back to the user.
    void Log(const std::string &line);

    bool HasPendingCommand() const { return !pending_.empty(); }
    std::string TakeCommand();

private:
    bool open_ = false;
    std::string input_;
    std::string pending_;
    std::vector<std::string> log_;
};

} // namespace console
