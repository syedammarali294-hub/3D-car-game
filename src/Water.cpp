#include "Water.hpp"

#include "Terrain.hpp"
#include "raymath.h"

namespace water {

void Water::Load()
{
    shader_ = LoadShader("shaders/water.vs", "shaders/water.fs");
    locTime_ = GetShaderLocation(shader_, "time");
    locViewPos_ = GetShaderLocation(shader_, "viewPos");

    Mesh mesh = GenMeshPlane(terrain::WORLD_SIZE, terrain::WORLD_SIZE, 60, 60);
    model_ = LoadModelFromMesh(mesh);
    model_.materials[0].shader = shader_;

    // Downloaded water ripple detail (CC BY 4.0, see Credits) -- a
    // scrolling texture layer on top of the existing procedural wave
    // shading, not a replacement for it.
    rippleTex_ = LoadTexture("assets/textures/water/diffuse.png");
    rippleAlpha_ = LoadTexture("assets/textures/water/opacity.png");
    SetTextureWrap(rippleTex_, TEXTURE_WRAP_REPEAT);
    SetTextureWrap(rippleAlpha_, TEXTURE_WRAP_REPEAT);
    locRippleTex_ = GetShaderLocation(shader_, "rippleTex");
    locRippleAlpha_ = GetShaderLocation(shader_, "rippleAlpha");

    loaded_ = true;
}

void Water::Unload()
{
    if (loaded_) {
        UnloadModel(model_);
        UnloadShader(shader_);
        loaded_ = false;
    }
}

void Water::Update(float totalTime, Vector3 viewPos)
{
    SetShaderValue(shader_, locTime_, &totalTime, SHADER_UNIFORM_FLOAT);
    SetShaderValue(shader_, locViewPos_, &viewPos, SHADER_UNIFORM_VEC3);
    // Rebinding every frame rather than once at Load() -- other draw calls
    // in between can rebind their own textures to the same GL texture unit,
    // so a one-time bind isn't reliable.
    SetShaderValueTexture(shader_, locRippleTex_, rippleTex_);
    SetShaderValueTexture(shader_, locRippleAlpha_, rippleAlpha_);
}

void Water::Draw() const
{
    if (!loaded_) return;

    BeginBlendMode(BLEND_ALPHA);
    DrawModel(model_, Vector3{ 0.0f, terrain::WATER_LEVEL, 0.0f }, 1.0f, WHITE);
    EndBlendMode();
}

} // namespace water
