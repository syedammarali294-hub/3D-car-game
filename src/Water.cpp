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
}

void Water::Draw() const
{
    if (!loaded_) return;

    BeginBlendMode(BLEND_ALPHA);
    DrawModel(model_, Vector3{ 0.0f, terrain::WATER_LEVEL, 0.0f }, 1.0f, WHITE);
    EndBlendMode();
}

} // namespace water
