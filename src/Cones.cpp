#include "Cones.hpp"

#include "Config.hpp"
#include "Road.hpp"
#include "raymath.h"

namespace cones {

namespace {
constexpr int SAMPLE_SPACING = 6;  // road samples between cones -- denser than Fence.cpp's posts (4), since these are meant to be woven through closely
constexpr float OFFSET = config::ROAD_WIDTH * 0.35f; // inside the drivable road, not out past the shoulder like a fence post
constexpr float CONE_RADIUS = 0.22f;
constexpr float CONE_HEIGHT = 0.55f;
constexpr float HIT_RADIUS = CONE_RADIUS + 0.35f; // a bit more forgiving than the visual cone itself
}

void Cones::Load()
{
    cones_.clear();
    hits_ = 0;

    const auto &samples = road::Centreline();
    const int count = (int)samples.size();
    if (count < 2) { loaded_ = true; return; }

    int side = 1;
    for (int i = 0; i < count; i += SAMPLE_SPACING) {
        const road::Sample &s = samples[i];
        Cone c;
        c.position = Vector3Add(s.position, Vector3Scale(s.right, (float)side * OFFSET));
        cones_.push_back(c);
        side = -side; // alternate left/right -- the actual slalom weave
    }
    loaded_ = true;
}

void Cones::Unload()
{
    cones_.clear();
    hits_ = 0;
    loaded_ = false;
}

void Cones::Draw() const
{
    if (!loaded_) return;
    for (const Cone &c : cones_) {
        Color body = c.hit ? Fade(Color{ 235, 110, 40, 255 }, 0.5f) : Color{ 235, 110, 40, 255 };
        float h = c.hit ? CONE_HEIGHT * 0.35f : CONE_HEIGHT; // knocked cones draw squashed, a cheap "fallen over" read
        DrawCylinder(c.position, CONE_RADIUS, 0.0f, h, 10, body);
        // White base ring, same real-traffic-cone look the fence posts'
        // dark/light wood pairing was going for.
        DrawCylinder(Vector3{ c.position.x, c.position.y, c.position.z }, CONE_RADIUS * 1.05f, CONE_RADIUS * 1.05f,
                     0.05f, 10, RAYWHITE);
    }
}

bool Cones::ResolveCollisions(Vector3 &position, float radius) const
{
    if (!loaded_) return false;
    bool any = false;
    for (const Cone &c : cones_) {
        float dx = position.x - c.position.x;
        float dz = position.z - c.position.z;
        float minDist = HIT_RADIUS + radius * 0.5f;
        float d2 = dx * dx + dz * dz;
        if (d2 < minDist * minDist) {
            if (!c.hit) { c.hit = true; hits_++; }
            // Gentle push rather than a hard wall -- a knocked cone is a
            // mistake you see and keep driving through, not a collision
            // that stops the car the way a fence/tree does.
            float d = sqrtf(fmaxf(d2, 0.0001f));
            float push = (minDist - d);
            position.x += (dx / d) * push;
            position.z += (dz / d) * push;
            any = true;
        }
    }
    return any;
}

} // namespace cones
