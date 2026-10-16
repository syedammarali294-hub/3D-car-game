#include "Console.hpp"

#include "raylib.h"

namespace console {

void Console::Update()
{
    if (!open_) return;

    int key = GetCharPressed();
    while (key > 0) {
        if (key >= 32 && key < 127 && input_.size() < 96) input_ += (char)key;
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !input_.empty()) input_.pop_back();

    if (IsKeyPressed(KEY_ENTER)) {
        if (!input_.empty()) {
            log_.push_back("> " + input_);
            pending_ = input_;
            input_.clear();
        }
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
        open_ = false;
        input_.clear();
    }

    if (log_.size() > 200) log_.erase(log_.begin(), log_.begin() + (log_.size() - 200));
}

void Console::Log(const std::string &line)
{
    log_.push_back(line);
}

std::string Console::TakeCommand()
{
    std::string c = pending_;
    pending_.clear();
    return c;
}

void Console::Draw(int screenW, int screenH) const
{
    if (!open_) return;

    const int panelH = screenH / 2;
    DrawRectangle(0, 0, screenW, panelH, Fade(BLACK, 0.82f));

    // Scrollback, newest line at the bottom just above the input box.
    const int lineH = 18;
    int visibleLines = (panelH - 50) / lineH;
    int start = (int)log_.size() > visibleLines ? (int)log_.size() - visibleLines : 0;
    int y = panelH - 50 - lineH;
    for (int i = (int)log_.size() - 1; i >= start; i--) {
        DrawText(log_[i].c_str(), 10, y, 16, Fade(RAYWHITE, 0.85f));
        y -= lineH;
    }

    DrawRectangle(0, panelH - 36, screenW, 36, Fade(Color{ 20, 24, 32, 255 }, 0.95f));
    DrawText((input_ + "_").c_str(), 10, panelH - 28, 18, Color{ 150, 220, 150, 255 });
    DrawLine(0, panelH, screenW, panelH, Fade(RAYWHITE, 0.3f));
}

} // namespace console
