#pragma once

#include "raylib.h"

namespace water {

class Water {
public:
    void Load();
    void Unload();
    void Update(float totalTime, Vector3 viewPos);
    void Draw() const;

    Shader GetShader() const { return shader_; }

private:
    Model model_ = { 0 };
    Shader shader_ = { 0 };
    int locTime_ = 0;
    int locViewPos_ = 0;
    bool loaded_ = false;
};

} // namespace water
