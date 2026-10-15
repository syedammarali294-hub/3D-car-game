#include "Lighting.hpp"

#include "raymath.h"

namespace lighting {

namespace {
const Vector3 kSun = { -0.45f, -0.78f, -0.42f };
const Vector3 kSky = { 0.62f, 0.74f, 0.88f };
}

Vector3 SunDirection() { return Vector3Normalize(kSun); }
Vector3 SkyColor() { return kSky; }

Color SkyColorTop() { return Color{ 92, 140, 206, 255 }; }
Color SkyColorHorizon() { return Color{ 182, 208, 226, 255 }; }

void SceneShader::Load(const char *vsPath, const char *fsPath)
{
    shader_ = LoadShader(vsPath, fsPath);
    locViewPos_ = GetShaderLocation(shader_, "viewPos");

    Vector3 sun = SunDirection();
    Vector3 sky = SkyColor();
    float fog = FOG_DENSITY;

    SetShaderValue(shader_, GetShaderLocation(shader_, "lightDir"), &sun, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, GetShaderLocation(shader_, "fogColor"), &sky, SHADER_UNIFORM_VEC3);
    SetShaderValue(shader_, GetShaderLocation(shader_, "fogDensity"), &fog, SHADER_UNIFORM_FLOAT);

    // Harmless on scene.fs, which doesn't declare this uniform -- raylib
    // just reports -1 for the location and SetShaderValue no-ops.
    int off = 0;
    SetShaderValue(shader_, GetShaderLocation(shader_, "desertMode"), &off, SHADER_UNIFORM_INT);

    loaded_ = true;
}

void SceneShader::SetDesertMode(bool desert)
{
    int v = desert ? 1 : 0;
    SetShaderValue(shader_, GetShaderLocation(shader_, "desertMode"), &v, SHADER_UNIFORM_INT);
}

void SceneShader::Unload()
{
    if (loaded_) {
        UnloadShader(shader_);
        loaded_ = false;
    }
}

void SceneShader::UpdateView(Vector3 viewPos)
{
    SetShaderValue(shader_, locViewPos_, &viewPos, SHADER_UNIFORM_VEC3);
}

} // namespace lighting
