#include "Road.hpp"

#include "Config.hpp"
#include "MeshBuilder.hpp"
#include "Terrain.hpp"
#include "raymath.h"
#include <cmath>

namespace road {

namespace {

std::vector<Sample> g_samples;

constexpr int STEPS_PER_SEGMENT = 24;

Vector2 CatmullRom(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3, float t)
{
    float t2 = t * t;
    float t3 = t2 * t;

    Vector2 r;
    r.x = 0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                  (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                  (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3);
    r.y = 0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                  (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                  (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3);
    return r;
}

} // namespace

void BuildCentreline(const std::vector<Vector2> &control)
{
    g_samples.clear();

    const int controlCount = (int)control.size();
    if (controlCount < 4) return; // Catmull-Rom needs 4 points per segment to evaluate

    std::vector<Vector2> flat;
    for (int i = 0; i < controlCount; i++) {
        Vector2 p0 = control[(i - 1 + controlCount) % controlCount];
        Vector2 p1 = control[i];
        Vector2 p2 = control[(i + 1) % controlCount];
        Vector2 p3 = control[(i + 2) % controlCount];

        for (int s = 0; s < STEPS_PER_SEGMENT; s++) {
            flat.push_back(CatmullRom(p0, p1, p2, p3, (float)s / (float)STEPS_PER_SEGMENT));
        }
    }

    const int count = (int)flat.size();
    g_samples.reserve(count);

    for (int i = 0; i < count; i++) {
        Vector2 here = flat[i];
        Vector2 next = flat[(i + 1) % count];

        Sample s;
        s.position = Vector3{ here.x, terrain::HeightAt(here.x, here.y), here.y };
        s.forward = Vector3Normalize(Vector3{ next.x - here.x, 0.0f, next.y - here.y });
        s.right = Vector3Normalize(Vector3CrossProduct(Vector3{ 0.0f, 1.0f, 0.0f }, s.forward));
        g_samples.push_back(s);
    }
}

const std::vector<Sample> &Centreline() { return g_samples; }

float DistanceToRoad(float x, float z)
{
    float best = 1e9f;
    for (const Sample &s : g_samples) {
        float dx = s.position.x - x;
        float dz = s.position.z - z;
        float d2 = dx * dx + dz * dz;
        if (d2 < best) best = d2;
    }
    return sqrtf(best);
}

int NearestSampleIndex(float x, float z)
{
    if (g_samples.empty()) return -1;

    float best = 1e9f;
    int bestI = 0;
    for (int i = 0; i < (int)g_samples.size(); i++) {
        float dx = g_samples[i].position.x - x;
        float dz = g_samples[i].position.z - z;
        float d2 = dx * dx + dz * dz;
        if (d2 < best) { best = d2; bestI = i; }
    }
    return bestI;
}

const Sample &NearestSample(float x, float z)
{
    static const Sample fallback = {
        Vector3Zero(), Vector3{ 0.0f, 0.0f, 1.0f }, Vector3{ 1.0f, 0.0f, 0.0f }
    };
    int i = NearestSampleIndex(x, z);
    return (i < 0) ? fallback : g_samples[i];
}

Sample StartPoint()
{
    if (g_samples.empty()) {
        Sample s;
        s.position = Vector3Zero();
        s.forward = Vector3{ 0.0f, 0.0f, 1.0f };
        s.right = Vector3{ 1.0f, 0.0f, 0.0f };
        return s;
    }
    return g_samples[0];
}

void RoadMesh::Load(Shader shader)
{
    const int count = (int)g_samples.size();
    if (count < 2) return;

    const float halfWidth = config::ROAD_WIDTH * 0.5f;
    const float lift = SURFACE_LIFT;

    meshbuild::MeshBuilder surface;
    meshbuild::MeshBuilder markings;

    const Color ASPHALT_A = Color{ 58, 58, 62, 255 };
    const Color ASPHALT_B = Color{ 52, 52, 56, 255 };
    const Color LINE = Color{ 226, 222, 198, 255 };

    for (int i = 0; i < count; i++) {
        const Sample &a = g_samples[i];
        const Sample &b = g_samples[(i + 1) % count];

        // Drape each edge onto the terrain so the road never clips through a hill.
        Vector3 a0 = Vector3Add(a.position, Vector3Scale(a.right, -halfWidth));
        Vector3 a1 = Vector3Add(a.position, Vector3Scale(a.right, halfWidth));
        Vector3 b0 = Vector3Add(b.position, Vector3Scale(b.right, -halfWidth));
        Vector3 b1 = Vector3Add(b.position, Vector3Scale(b.right, halfWidth));

        a0.y = terrain::HeightAt(a0.x, a0.z) + lift;
        a1.y = terrain::HeightAt(a1.x, a1.z) + lift;
        b0.y = terrain::HeightAt(b0.x, b0.z) + lift;
        b1.y = terrain::HeightAt(b1.x, b1.z) + lift;

        surface.AddQuad(a0, b0, b1, a1, (i % 2 == 0) ? ASPHALT_A : ASPHALT_B);

        // Dashed centre line.
        if (i % 4 < 2) {
            float w = 0.32f;
            Vector3 c0 = Vector3Add(a.position, Vector3Scale(a.right, -w));
            Vector3 c1 = Vector3Add(a.position, Vector3Scale(a.right, w));
            Vector3 d0 = Vector3Add(b.position, Vector3Scale(b.right, -w));
            Vector3 d1 = Vector3Add(b.position, Vector3Scale(b.right, w));

            c0.y = terrain::HeightAt(c0.x, c0.z) + lift + 0.04f;
            c1.y = terrain::HeightAt(c1.x, c1.z) + lift + 0.04f;
            d0.y = terrain::HeightAt(d0.x, d0.z) + lift + 0.04f;
            d1.y = terrain::HeightAt(d1.x, d1.z) + lift + 0.04f;

            markings.AddQuad(c0, d0, d1, c1, LINE);
        }
    }

    surface_ = LoadModelFromMesh(surface.Build());
    surface_.materials[0].shader = shader;

    markings_ = LoadModelFromMesh(markings.Build());
    markings_.materials[0].shader = shader;

    loaded_ = true;
}

void RoadMesh::Unload()
{
    if (loaded_) {
        UnloadModel(surface_);
        UnloadModel(markings_);
        loaded_ = false;
    }
}

void RoadMesh::Draw() const
{
    if (!loaded_) return;
    DrawModel(surface_, Vector3Zero(), 1.0f, WHITE);
    DrawModel(markings_, Vector3Zero(), 1.0f, WHITE);
}

} // namespace road
