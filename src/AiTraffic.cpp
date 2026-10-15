#include "AiTraffic.hpp"

#include "CarDatabase.hpp"
#include "Road.hpp"
#include "raymath.h"
#include <cmath>
#include <cstdlib>

namespace traffic {

namespace {

// Plain, livery-free kit bodies only -- the same subset CarDatabase.cpp
// uses for the player's own roster, so a passing AI car never looks like it
// wandered out of a mismatched art style.
const char *KIT_FILES[] = {
    "assets/models/car/sedan-sports.obj",
    "assets/models/car/hatchback-sports.obj",
    "assets/models/car/race.obj",
    "assets/models/car/suv.obj",
    "assets/models/car/race-future.obj",
};
constexpr int KIT_COUNT = 5;

const Color PALETTE[] = {
    Color{ 210, 210, 216, 255 }, Color{ 40, 44, 50, 255 }, Color{ 150, 30, 36, 255 },
    Color{ 30, 70, 150, 255 }, Color{ 230, 200, 40, 255 }, Color{ 60, 110, 70, 255 },
};
constexpr int PALETTE_COUNT = 6;

float Frand(unsigned int &seed)
{
    seed = seed * 1664525u + 1013904223u;
    return (float)((seed >> 8) & 0xFFFFFF) / (float)0xFFFFFF;
}

} // namespace

void AiTraffic::Load(Shader shader, int count)
{
    cars_.clear();
    cars_.reserve(count);

    unsigned int seed = 0xC0FFEEu;
    const auto &samples = road::Centreline();
    int sampleCount = (int)samples.size();

    for (int i = 0; i < count; i++) {
        Car c;
        const char *file = KIT_FILES[(int)(Frand(seed) * KIT_COUNT) % KIT_COUNT];
        c.body = LoadModel(file);
        for (int m = 0; m < c.body.materialCount; m++) c.body.materials[m].shader = shader;

        BoundingBox bb = GetModelBoundingBox(c.body);
        float kitLength = bb.max.z - bb.min.z;
        const float targetLength = 4.4f; // roughly matches the player roster's average
        c.bodyScale = (kitLength > 0.01f) ? targetLength / kitLength : 1.0f;
        c.radius = targetLength * 0.55f;

        c.paint = PALETTE[(int)(Frand(seed) * PALETTE_COUNT) % PALETTE_COUNT];

        // Spread cars evenly around the lap rather than clumped at the
        // start, each offset to one side or the other of the centreline so
        // they read as two lanes of opposing-ish traffic.
        c.progress = (sampleCount > 0) ? (float)i / (float)count * (float)sampleCount : 0.0f;
        c.speed = 8.0f + Frand(seed) * 6.0f; // slower than the player's own top speed on purpose
        c.laneOffset = (i % 2 == 0) ? 3.0f : -3.0f;

        cars_.push_back(c);
    }

    loaded_ = true;
}

void AiTraffic::Unload()
{
    if (!loaded_) return;
    for (Car &c : cars_) UnloadModel(c.body);
    cars_.clear();
    loaded_ = false;
}

void AiTraffic::Update(float dt)
{
    if (!loaded_) return;

    const auto &samples = road::Centreline();
    int n = (int)samples.size();
    if (n < 2) return;

    for (Car &c : cars_) {
        // Advance progress using the REAL distance to the next sample, not
        // a guessed constant -- the circuit's samples average ~11.8 world
        // units apart (checked directly against Road.cpp's control points),
        // not the ~2.2 this used to assume. That guess being off by ~5x was
        // why every AI car -- and later, auto-drive, copied from this same
        // code -- moved along the track several times faster than their
        // own speed value implied.
        int i0now = (int)c.progress;
        int i1now = (i0now + 1) % n;
        float segLen = Vector3Distance(samples[i0now].position, samples[i1now].position);
        if (segLen < 0.01f) segLen = 0.01f;
        c.progress += (c.speed * dt) / segLen;
        while (c.progress >= (float)n) c.progress -= (float)n;
        while (c.progress < 0.0f) c.progress += (float)n;

        int i0 = (int)c.progress;
        int i1 = (i0 + 1) % n;
        float t = c.progress - (float)i0;

        const road::Sample &a = samples[i0];
        const road::Sample &b = samples[i1];

        Vector3 base = Vector3Lerp(a.position, b.position, t);
        Vector3 fwd = Vector3Normalize(Vector3Lerp(a.forward, b.forward, t));
        Vector3 right = Vector3Normalize(Vector3Lerp(a.right, b.right, t));

        c.position = Vector3Add(base, Vector3Scale(right, c.laneOffset));
        c.heading = atan2f(fwd.x, fwd.z);
    }
}

void AiTraffic::Draw() const
{
    if (!loaded_) return;

    for (const Car &c : cars_) {
        Matrix scale = MatrixScale(c.bodyScale, c.bodyScale, c.bodyScale);
        Matrix rotate = MatrixRotateY(c.heading);
        Matrix translate = MatrixTranslate(c.position.x, c.position.y, c.position.z);

        Model body = c.body;
        body.transform = MatrixMultiply(scale, MatrixMultiply(rotate, translate));
        DrawModel(body, Vector3Zero(), 1.0f, c.paint);
    }
}

bool AiTraffic::ResolveCollisions(Vector3 &playerPosition, float playerRadius) const
{
    bool hit = false;
    for (const Car &c : cars_) {
        float dx = playerPosition.x - c.position.x;
        float dz = playerPosition.z - c.position.z;
        float distSq = dx * dx + dz * dz;
        float minDist = playerRadius + c.radius;

        if (distSq < minDist * minDist && distSq > 0.00001f) {
            float dist = sqrtf(distSq);
            float push = minDist - dist;
            playerPosition.x += (dx / dist) * push;
            playerPosition.z += (dz / dist) * push;
            hit = true;
        }
    }
    return hit;
}

} // namespace traffic
