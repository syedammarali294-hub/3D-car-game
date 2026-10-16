#pragma once

#include "Config.hpp"
#include "raylib.h"
#include <vector>

namespace terrain {

// Mirrors of the central tuning values so terrain code reads naturally.
constexpr float WORLD_SIZE = config::WORLD_SIZE;
constexpr float WATER_LEVEL = config::WATER_LEVEL;
constexpr float LAKE_X = config::LAKE_X;
constexpr float LAKE_Z = config::LAKE_Z;
constexpr float LAKE_RADIUS = config::LAKE_RADIUS;

// World look presets, cycled from Settings. Changing it takes effect the
// next time Drive is pressed (same pattern as swapping cars), since the
// road/fences/props are all baked from HeightAt() once at load time rather
// than read live every frame.
enum Style { STYLE_HILLS = 0, STYLE_SLOPES, STYLE_FLAT, STYLE_DESERT, STYLE_COUNT };

void SetStyle(Style s);
Style GetStyle();
const char *StyleLabel(Style s);
bool IsDesert(); // lets the terrain shader pick a sand palette instead of grass

float HeightAt(float x, float z);
Vector3 NormalAt(float x, float z);

class Terrain {
public:
    void Load(Shader shader);
    void Unload();
    void Draw() const;

private:
    // raylib's Mesh::indices is a hardcoded unsigned short* (not something
    // this project's own MeshBuilder controls) -- a hard 65535-vertex cap
    // per mesh. A single (RES+1)^2 grid can't go much past RES=250 without
    // hitting it, so the terrain is tiled into a grid of smaller meshes
    // instead: each tile stays comfortably under the cap, and since every
    // tile computes its vertices from the same world-space HeightAt/
    // NormalAt functions, shared edges land on identical positions with no
    // extra seam-matching logic needed.
    std::vector<Model> tiles_;
    bool loaded_ = false;
};

} // namespace terrain
