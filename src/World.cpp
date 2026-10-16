#include "World.hpp"

#include "Road.hpp"
#include "Terrain.hpp"
#include "raymath.h"
#include <cmath>

namespace world {

namespace {

const char *MODEL_FILES[] = {
    "assets/models/nature/tree_default.obj",
    "assets/models/nature/tree_oak.obj",
    "assets/models/nature/tree_pineDefaultA.obj",
    "assets/models/nature/tree_detailed.obj",
    "assets/models/nature/rock_largeA.obj",
    "assets/models/nature/rock_smallA.obj",
    // Downloaded, licensed (CC BY 4.0) real oak models -- much higher
    // triangle cost (~6.2k faces each vs a few hundred for the Kenney
    // kit trees above), so these only get picked for a small fraction of
    // tree spawns rather than replacing the kit trees outright. With up
    // to ~800 tree instances scattered across the map, giving all of
    // them this model would multiply the scenery triangle budget by
    // roughly 20x for comparatively little visible benefit at the
    // draw distances most of them are seen from.
    "assets/models/nature/oak_big.obj",
    "assets/models/nature/oak_large.obj",
    "assets/models/nature/oak_medium.obj",
    "assets/models/nature/oak_sapling.obj",
};
constexpr int MODEL_COUNT = 10;
constexpr int ROCK_FIRST_INDEX = 4;
constexpr int REALISTIC_OAK_FIRST_INDEX = 6;
constexpr int REALISTIC_OAK_COUNT = 4;
constexpr int PROP_TARGET = 1100;

// Deterministic generator so the world is identical every run.
struct Rng {
    unsigned int state = 0x9E3779B9u;
    float Next()
    {
        state = state * 1664525u + 1013904223u;
        return (float)((state >> 8) & 0xFFFFFF) / (float)0xFFFFFF;
    }
    float Range(float lo, float hi) { return lo + Next() * (hi - lo); }
};

} // namespace

void World::Load(Shader shader)
{
    models_.reserve(MODEL_COUNT);
    for (int i = 0; i < MODEL_COUNT; i++) {
        Model m = LoadModel(MODEL_FILES[i]);
        for (int j = 0; j < m.materialCount; j++) m.materials[j].shader = shader;
        models_.push_back(m);
    }

    Rng rng;
    const float half = terrain::WORLD_SIZE * 0.5f - 20.0f;
    // Desert scattering skews heavily toward rocks and is much sparser --
    // open ground is the whole point of the style, and this kit's only
    // tree models are temperate-climate trees that would look wrong
    // scattered across dunes. (A real desert prop set -- cacti, dead
    // scrub -- is a separate ask; see TODO.md.)
    const bool desert = (terrain::GetStyle() == terrain::STYLE_DESERT);

    for (int i = 0; i < PROP_TARGET; i++) {
        if (desert && rng.Next() < 0.6f) continue; // thin the field out
        float x = rng.Range(-half, half);
        float z = rng.Range(-half, half);

        float h = terrain::HeightAt(x, z);
        if (h < terrain::WATER_LEVEL + 1.5f) continue;   // no trees in the lake
        if (sqrtf(x * x + z * z) < 45.0f) continue;      // keep the spawn pad clear
        if (road::DistanceToRoad(x, z) < 19.0f) continue; // keep clear of the circuit

        Vector3 n = terrain::NormalAt(x, z);
        if (n.y < 0.80f) continue;                        // nothing clings to steep mountainside

        bool rock = (rng.Next() < (desert ? 0.9f : 0.22f));
        // A small fraction of tree spawns use one of the realistic,
        // downloaded oak models instead of a kit tree -- see the comment
        // by REALISTIC_OAK_FIRST_INDEX for why this stays a minority
        // rather than replacing the kit trees outright.
        bool realisticOak = (!rock && !desert && rng.Next() < 0.12f);
        int modelIndex = rock
            ? ROCK_FIRST_INDEX + (int)(rng.Next() * 2.0f) % 2
            : realisticOak
                ? REALISTIC_OAK_FIRST_INDEX + (int)(rng.Next() * REALISTIC_OAK_COUNT) % REALISTIC_OAK_COUNT
                : (int)(rng.Next() * 4.0f) % 4;

        Prop p;
        p.position = Vector3{ x, h - 0.1f, z };
        p.rotation = rng.Range(0.0f, 360.0f);
        p.scale = rock ? rng.Range(1.5f, 4.0f) : rng.Range(3.5f, 7.0f);
        p.modelIndex = modelIndex;
        // Only trees block the car; rocks are small enough to treat as decoration.
        p.colliderRadius = rock ? 0.0f : p.scale * 0.17f;
        props_.push_back(p);
    }

    loaded_ = true;
}

void World::Unload()
{
    if (!loaded_) return;
    for (Model &m : models_) UnloadModel(m);
    models_.clear();
    props_.clear();
    loaded_ = false;
}

void World::Draw(Vector3 viewPos, float drawDistance, Vector3 viewForward, float fovyDeg, float aspect) const
{
    visibleCount_ = 0;
    if (!loaded_) return;

    const float maxDistSq = drawDistance * drawDistance;

    // Cone cull: horizontal FOV is wider than the vertical fovy whenever
    // aspect > 1, so derive it from fovy rather than reusing fovy directly
    // -- using the vertical angle alone would clip props still visible at
    // the left/right edge of a widescreen view. +18 degrees of margin over
    // the true half-angle: this is a cone, not the camera's real frustum,
    // and props have their own size, so cutting it exactly at the FOV edge
    // would pop trees in/out right at the edge of the screen.
    float vHalf = fovyDeg * 0.5f * DEG2RAD;
    float hHalf = atanf(tanf(vHalf) * aspect);
    float cosThreshold = cosf(hHalf + 18.0f * DEG2RAD);
    Vector3 fwd = Vector3Normalize(viewForward);
    const float nearRadius = 18.0f; // always draw anything this close regardless of angle
    const float nearRadiusSq = nearRadius * nearRadius;

    // Bucket visible props by model type, then issue one instanced draw per
    // type instead of one DrawModelEx per prop. With up to PROP_TARGET
    // (1100) props scattered across the map, that was up to 1100 individual
    // draw calls a frame -- each one its own GPU state change -- for meshes
    // that (unlike the rest of this project's procedural geometry) come
    // from real imported OBJ files. Grouping into MODEL_COUNT (6) instanced
    // draws is the single biggest render-side cost this project had.
    static thread_local std::vector<Matrix> transforms[MODEL_COUNT];
    for (auto &v : transforms) v.clear();

    for (const Prop &p : props_) {
        float dx = p.position.x - viewPos.x;
        float dz = p.position.z - viewPos.z;
        float distSq = dx * dx + dz * dz;
        if (distSq > maxDistSq) continue;

        if (distSq > nearRadiusSq) {
            Vector3 toProp = Vector3Normalize(Vector3{ dx, 0.0f, dz });
            if (Vector3DotProduct(toProp, fwd) < cosThreshold) continue;
        }

        Matrix scale = MatrixScale(p.scale, p.scale, p.scale);
        Matrix rotate = MatrixRotateY(p.rotation * DEG2RAD);
        Matrix translate = MatrixTranslate(p.position.x, p.position.y, p.position.z);
        transforms[p.modelIndex].push_back(MatrixMultiply(MatrixMultiply(scale, rotate), translate));
        visibleCount_++;
    }

    for (int m = 0; m < MODEL_COUNT; m++) {
        if (transforms[m].empty()) continue;
        const Model &model = models_[m];
        for (int mesh = 0; mesh < model.meshCount; mesh++) {
            Material material = model.materials[model.meshMaterial[mesh]];
            DrawMeshInstanced(model.meshes[mesh], material, transforms[m].data(), (int)transforms[m].size());
        }
    }
}

bool World::ResolveCollisions(Vector3 &position, float radius) const
{
    bool hit = false;

    for (const Prop &p : props_) {
        if (p.colliderRadius <= 0.0f) continue;

        float dx = position.x - p.position.x;
        float dz = position.z - p.position.z;
        float distSq = dx * dx + dz * dz;
        float minDist = radius + p.colliderRadius;

        if (distSq < minDist * minDist && distSq > 0.00001f) {
            float dist = sqrtf(distSq);
            float push = (minDist - dist);
            position.x += (dx / dist) * push;
            position.z += (dz / dist) * push;
            hit = true;
        }
    }

    return hit;
}

} // namespace world
