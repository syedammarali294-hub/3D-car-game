#include "SkidMarks.hpp"

#include "raymath.h"
#include "rlgl.h"
#include <vector>

namespace skidmarks {

namespace {

struct Point {
    Vector3 left, right;
    float age = 0.0f;
    bool startsNewStrip = false; // true for the first point after a Break()
};

constexpr float MAX_AGE = 6.0f;       // seconds before a mark is fully gone
constexpr int MAX_POINTS = 2400;      // bounds memory and worst-case draw cost

std::vector<Point> g_points;
bool g_pendingBreak = true; // starts broken so the very first Add() doesn't connect to nothing

} // namespace

void Add(Vector3 left, Vector3 right)
{
    Point p;
    p.left = left;
    p.right = right;
    p.age = 0.0f;
    p.startsNewStrip = g_pendingBreak;
    g_pendingBreak = false;

    g_points.push_back(p);
    if ((int)g_points.size() > MAX_POINTS) {
        g_points.erase(g_points.begin(), g_points.begin() + (g_points.size() - MAX_POINTS));
    }
}

void Break()
{
    g_pendingBreak = true;
}

void Update(float dt)
{
    for (Point &p : g_points) p.age += dt;
    // Drop fully-faded points from the front -- they're added in order, so
    // the oldest are always at the start.
    size_t drop = 0;
    while (drop < g_points.size() && g_points[drop].age > MAX_AGE) drop++;
    if (drop > 0) g_points.erase(g_points.begin(), g_points.begin() + drop);
}

void Draw()
{
    if (g_points.size() < 2) return;

    rlDisableBackfaceCulling(); // seen from any angle, not just from above
    rlBegin(RL_TRIANGLES);
    for (size_t i = 1; i < g_points.size(); i++) {
        const Point &a = g_points[i - 1];
        const Point &b = g_points[i];
        if (b.startsNewStrip) continue; // a and b belong to different drift events

        float alphaA = 1.0f - Clamp(a.age / MAX_AGE, 0.0f, 1.0f);
        float alphaB = 1.0f - Clamp(b.age / MAX_AGE, 0.0f, 1.0f);
        // Faded enough it wouldn't be visible anyway -- skip the draw call.
        if (alphaA <= 0.01f && alphaB <= 0.01f) continue;

        unsigned char ca = (unsigned char)(alphaA * 140.0f);
        unsigned char cb = (unsigned char)(alphaB * 140.0f);

        // Two triangles per quad (a.left, a.right, b.left, b.right), each
        // vertex using its own point's fade so a strip visibly tapers out.
        rlColor4ub(10, 10, 12, ca);
        rlVertex3f(a.left.x, a.left.y, a.left.z);
        rlColor4ub(10, 10, 12, cb);
        rlVertex3f(b.left.x, b.left.y, b.left.z);
        rlColor4ub(10, 10, 12, cb);
        rlVertex3f(b.right.x, b.right.y, b.right.z);

        rlColor4ub(10, 10, 12, ca);
        rlVertex3f(a.left.x, a.left.y, a.left.z);
        rlColor4ub(10, 10, 12, cb);
        rlVertex3f(b.right.x, b.right.y, b.right.z);
        rlColor4ub(10, 10, 12, ca);
        rlVertex3f(a.right.x, a.right.y, a.right.z);
    }
    rlEnd();
    rlEnableBackfaceCulling();
}

} // namespace skidmarks
