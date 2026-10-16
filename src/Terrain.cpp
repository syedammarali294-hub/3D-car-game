#include "Terrain.hpp"

#include "raymath.h"
#include <cmath>
#include <cstdlib>

namespace terrain {

namespace {

// WORLD_SIZE grew 1600 -> 4000 for the open-world expansion; RES and
// TILE_COUNT scale with it to keep the same ~4 world-units-per-quad
// density rather than the existing terrain just getting coarser over a
// bigger area. TILE_COUNT=5 (not 4) because RES/4 would land each tile at
// 63001 verts -- only ~4% under the 65535 cap, an uncomfortably thin
// margin. /5 keeps the same ~62%-of-cap margin the previous 4x4 grid had.
constexpr int RES = 1000;      // grid quads per side, across the whole terrain (was 400)
constexpr int TILE_COUNT = 5;  // per axis -- 25 tiles total, each (RES/TILE_COUNT+1)^2 = 40401 verts

float SmoothStep01(float t)
{
    t = Clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Style g_style = STYLE_HILLS;

} // namespace

void SetStyle(Style s) { g_style = s; }
Style GetStyle() { return g_style; }
bool IsDesert() { return g_style == STYLE_DESERT; }

const char *StyleLabel(Style s)
{
    switch (s) {
        case STYLE_HILLS:  return "Hills";
        case STYLE_SLOPES: return "Slopes";
        case STYLE_FLAT:   return "Flat";
        case STYLE_DESERT: return "Desert";
        default: return "?";
    }
}

float HeightAt(float x, float z)
{
    float h = 6.0f;
    switch (g_style) {
        case STYLE_SLOPES:
            // Long, rolling, mostly one-directional slopes rather than
            // small bumps -- big wavelength, no short-period noise term.
            h += sinf(x * 0.0021f + 0.4f) * 11.0f;
            h += sinf(z * 0.0027f) * 6.0f;
            h += sinf((x - z) * 0.0043f) * 3.0f;
            break;
        case STYLE_FLAT:
            // Barely-there undulation -- a plain rather than a bowl of hills.
            h += sinf(x * 0.0062f) * cosf(z * 0.0058f) * 1.1f;
            h += sinf((x + z) * 0.0270f) * 0.25f;
            break;
        case STYLE_DESERT:
            // Big, smooth, slow dune shapes -- lower frequency than the
            // default hills, no small-scale grass-field noise.
            h += sinf(x * 0.0040f + 0.9f) * cosf(z * 0.0036f) * 6.0f;
            h += sinf((x * 0.6f + z) * 0.0110f) * 2.2f;
            break;
        case STYLE_HILLS:
        default:
            h += sinf(x * 0.0062f) * cosf(z * 0.0058f) * 7.0f;
            h += sinf(x * 0.0150f + 1.7f) * cosf(z * 0.0131f + 0.6f) * 2.6f;
            h += sinf((x + z) * 0.0270f) * 0.9f;
            break;
    }

    // No lake in the desert -- an oasis-sized puddle would read as a
    // mistake rather than a feature at this scale.
    //
    // Water.cpp draws one flat plane across the WHOLE world at
    // WATER_LEVEL, not just under the lake -- it only ever becomes visible
    // where terrain dips below that, so the lake "exists" purely because
    // HeightAt() dips low near LAKE_X/LAKE_Z. The style noise above can
    // dip below WATER_LEVEL anywhere on the map, though (hills especially),
    // which used to flood random low points along tracks far from the
    // lake with no warning. Clamping everywhere outside the lake's own
    // falloff back above the waterline keeps the lake the only place that
    // floods, without having to touch Water.cpp's plane itself.
    const float floor = WATER_LEVEL + 1.0f;
    if (g_style != STYLE_DESERT) {
        float dx = x - LAKE_X;
        float dz = z - LAKE_Z;
        float lakeT = SmoothStep01(1.0f - sqrtf(dx * dx + dz * dz) / LAKE_RADIUS);
        h -= lakeT * 17.0f;
        h = Lerp(fmaxf(h, floor), h, lakeT);
    } else {
        h = fmaxf(h, floor);
    }

    // Keep the spawn area flat so the car always starts on dry, level ground.
    float spawnT = SmoothStep01(1.0f - sqrtf(x * x + z * z) / 70.0f);
    h = Lerp(h, 6.0f, spawnT * 0.9f);

    // Mountain ring: closes the world off well outside the circuit.
    float r = sqrtf(x * x + z * z);
    float rim = SmoothStep01((r - config::MOUNTAIN_START) /
                             (config::MOUNTAIN_FULL - config::MOUNTAIN_START));
    if (rim > 0.0f) {
        float angle = atan2f(z, x);
        // Ridges and saddles so the wall reads as peaks rather than a bowl rim.
        float ridge = sinf(angle * 5.0f) * 0.30f + sinf(angle * 11.0f + 2.1f) * 0.16f;
        h += rim * config::MOUNTAIN_HEIGHT * (1.0f + ridge);
        h += rim * sinf(x * 0.021f) * cosf(z * 0.019f) * 14.0f;
    }

    return h;
}

Vector3 NormalAt(float x, float z)
{
    const float e = 1.0f;
    float hL = HeightAt(x - e, z);
    float hR = HeightAt(x + e, z);
    float hD = HeightAt(x, z - e);
    float hU = HeightAt(x, z + e);

    Vector3 n = { hL - hR, 2.0f * e, hD - hU };
    return Vector3Normalize(n);
}

void Terrain::Load(Shader shader)
{
    static_assert(RES % TILE_COUNT == 0, "RES must divide evenly into TILE_COUNT tiles");

    const int quadsPerTile = RES / TILE_COUNT;
    const int vertsPerSide = quadsPerTile + 1;        // per tile
    const int vertexCount = vertsPerSide * vertsPerSide; // per tile -- stays well under 65535
    const int triangleCount = quadsPerTile * quadsPerTile * 2;

    const float step = WORLD_SIZE / (float)RES;
    const float origin = -WORLD_SIZE * 0.5f;

    tiles_.clear();
    tiles_.reserve(TILE_COUNT * TILE_COUNT);

    for (int tz = 0; tz < TILE_COUNT; tz++) {
        for (int tx = 0; tx < TILE_COUNT; tx++) {
            Mesh mesh = { 0 };
            mesh.vertexCount = vertexCount;
            mesh.triangleCount = triangleCount;
            mesh.vertices = (float *)MemAlloc(vertexCount * 3 * sizeof(float));
            mesh.normals = (float *)MemAlloc(vertexCount * 3 * sizeof(float));
            mesh.texcoords = (float *)MemAlloc(vertexCount * 2 * sizeof(float));
            mesh.indices = (unsigned short *)MemAlloc(triangleCount * 3 * sizeof(unsigned short));

            // Global grid index this tile starts at -- the +1 overlap with
            // the next tile is what makes adjacent tiles share an edge
            // (computed independently from the same world x/z, so the
            // positions land exactly on top of each other with no crack).
            int gx0 = tx * quadsPerTile;
            int gz0 = tz * quadsPerTile;

            for (int zi = 0; zi < vertsPerSide; zi++) {
                for (int xi = 0; xi < vertsPerSide; xi++) {
                    int i = zi * vertsPerSide + xi;
                    float x = origin + (gx0 + xi) * step;
                    float z = origin + (gz0 + zi) * step;

                    mesh.vertices[i * 3 + 0] = x;
                    mesh.vertices[i * 3 + 1] = HeightAt(x, z);
                    mesh.vertices[i * 3 + 2] = z;

                    Vector3 n = NormalAt(x, z);
                    mesh.normals[i * 3 + 0] = n.x;
                    mesh.normals[i * 3 + 1] = n.y;
                    mesh.normals[i * 3 + 2] = n.z;

                    mesh.texcoords[i * 2 + 0] = (float)(gx0 + xi) / (float)RES;
                    mesh.texcoords[i * 2 + 1] = (float)(gz0 + zi) / (float)RES;
                }
            }

            int t = 0;
            for (int zi = 0; zi < quadsPerTile; zi++) {
                for (int xi = 0; xi < quadsPerTile; xi++) {
                    unsigned short a = (unsigned short)(zi * vertsPerSide + xi);
                    unsigned short b = (unsigned short)(a + 1);
                    unsigned short c = (unsigned short)(a + vertsPerSide);
                    unsigned short d = (unsigned short)(c + 1);

                    mesh.indices[t++] = a;
                    mesh.indices[t++] = c;
                    mesh.indices[t++] = b;

                    mesh.indices[t++] = b;
                    mesh.indices[t++] = c;
                    mesh.indices[t++] = d;
                }
            }

            UploadMesh(&mesh, false);
            Model m = LoadModelFromMesh(mesh);
            m.materials[0].shader = shader;
            tiles_.push_back(m);
        }
    }

    loaded_ = true;
}

void Terrain::Unload()
{
    if (loaded_) {
        for (Model &m : tiles_) UnloadModel(m);
        tiles_.clear();
        loaded_ = false;
    }
}

void Terrain::Draw() const
{
    if (!loaded_) return;
    for (const Model &m : tiles_) DrawModel(m, Vector3Zero(), 1.0f, WHITE);
}

} // namespace terrain
