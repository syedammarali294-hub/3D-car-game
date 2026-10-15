#include "Terrain.hpp"

#include "raymath.h"
#include <cmath>
#include <cstdlib>

namespace terrain {

namespace {

constexpr int RES = 200; // grid quads per side; (RES+1)^2 must stay under 65535

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
    if (g_style != STYLE_DESERT) {
        float dx = x - LAKE_X;
        float dz = z - LAKE_Z;
        float lakeT = SmoothStep01(1.0f - sqrtf(dx * dx + dz * dz) / LAKE_RADIUS);
        h -= lakeT * 17.0f;
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
    const int vertsPerSide = RES + 1;
    const int vertexCount = vertsPerSide * vertsPerSide;
    const int triangleCount = RES * RES * 2;

    Mesh mesh = { 0 };
    mesh.vertexCount = vertexCount;
    mesh.triangleCount = triangleCount;
    mesh.vertices = (float *)MemAlloc(vertexCount * 3 * sizeof(float));
    mesh.normals = (float *)MemAlloc(vertexCount * 3 * sizeof(float));
    mesh.texcoords = (float *)MemAlloc(vertexCount * 2 * sizeof(float));
    mesh.indices = (unsigned short *)MemAlloc(triangleCount * 3 * sizeof(unsigned short));

    const float step = WORLD_SIZE / (float)RES;
    const float origin = -WORLD_SIZE * 0.5f;

    for (int zi = 0; zi < vertsPerSide; zi++) {
        for (int xi = 0; xi < vertsPerSide; xi++) {
            int i = zi * vertsPerSide + xi;
            float x = origin + xi * step;
            float z = origin + zi * step;

            mesh.vertices[i * 3 + 0] = x;
            mesh.vertices[i * 3 + 1] = HeightAt(x, z);
            mesh.vertices[i * 3 + 2] = z;

            Vector3 n = NormalAt(x, z);
            mesh.normals[i * 3 + 0] = n.x;
            mesh.normals[i * 3 + 1] = n.y;
            mesh.normals[i * 3 + 2] = n.z;

            mesh.texcoords[i * 2 + 0] = (float)xi / (float)RES;
            mesh.texcoords[i * 2 + 1] = (float)zi / (float)RES;
        }
    }

    int t = 0;
    for (int zi = 0; zi < RES; zi++) {
        for (int xi = 0; xi < RES; xi++) {
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
    model_ = LoadModelFromMesh(mesh);
    model_.materials[0].shader = shader;
    loaded_ = true;
}

void Terrain::Unload()
{
    if (loaded_) {
        UnloadModel(model_);
        loaded_ = false;
    }
}

void Terrain::Draw() const
{
    if (loaded_) DrawModel(model_, Vector3Zero(), 1.0f, WHITE);
}

} // namespace terrain
