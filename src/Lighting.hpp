#pragma once

#include "raylib.h"

namespace lighting {

// Shared sky/sun setup so the scene, terrain and water all agree on the lighting.
constexpr float FOG_DENSITY = 0.0028f;

Vector3 SunDirection();
Vector3 SkyColor();
Color SkyColorTop();
Color SkyColorHorizon();

class SceneShader {
public:
    void Load(const char *vsPath, const char *fsPath);
    void Unload();
    void UpdateView(Vector3 viewPos);
    void SetDesertMode(bool desert); // only meaningful on the terrain shader

    Shader Get() const { return shader_; }

private:
    Shader shader_ = { 0 };
    int locViewPos_ = 0;
    bool loaded_ = false;
};

} // namespace lighting
