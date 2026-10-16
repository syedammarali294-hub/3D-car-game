#include "Fence.hpp"

#include "Config.hpp"
#include "MeshBuilder.hpp"
#include "Road.hpp"
#include "Terrain.hpp"
#include "raymath.h"
#include <cmath>

namespace fence {

namespace {
constexpr float OFFSET = config::ROAD_SHOULDER + 1.0f; // beyond the shoulder, outside the drivable verge
constexpr int POST_SPACING = 4; // samples between posts
constexpr float POST_HEIGHT = 1.1f;
constexpr float POST_SIZE = 0.09f;
constexpr float RAIL_HEIGHT_A = 0.45f;
constexpr float RAIL_HEIGHT_B = 0.85f;
constexpr float RAIL_THICK = 0.05f;
}

bool Fence::InGap(float fraction) const
{
    for (const Vector2 &g : gaps_) {
        if (fraction >= g.x && fraction < g.y) return true;
    }
    return false;
}

void Fence::Load(Shader shader, const std::vector<Vector2> &gaps)
{
    gaps_ = gaps;

    const auto &samples = road::Centreline();
    const int count = (int)samples.size();
    if (count < 2) return;

    meshbuild::MeshBuilder mb;
    const Color WOOD = Color{ 120, 86, 54, 255 };
    const Color WOOD_DARK = Color{ 96, 68, 42, 255 };

    for (int side = -1; side <= 1; side += 2) {
        for (int i = 0; i < count; i += POST_SPACING) {
            if (InGap((float)i / (float)count)) continue; // deliberate opening -- no post/rail here

            const road::Sample &s = samples[i];
            Vector3 post = Vector3Add(s.position, Vector3Scale(s.right, (float)side * OFFSET));
            post.y = terrain::HeightAt(post.x, post.z);

            mb.AddBox(Vector3{ post.x, post.y + POST_HEIGHT * 0.5f, post.z },
                      Vector3{ POST_SIZE, POST_HEIGHT, POST_SIZE }, WOOD_DARK);

            int nextI = i + POST_SPACING;
            if (nextI >= count) continue;

            const road::Sample &sNext = samples[nextI];
            Vector3 postNext = Vector3Add(sNext.position, Vector3Scale(sNext.right, (float)side * OFFSET));
            postNext.y = terrain::HeightAt(postNext.x, postNext.z);

            // Rails are two straight boxes between consecutive posts, with
            // no sampling in between -- on a steep local slope (or any
            // terrain style producing a sharp height change over one post's
            // spacing) that stretches into a long, near-vertical sliver
            // that reads as a stray wire shooting into the sky rather than
            // a fence. Posts always still get placed at their own correct
            // ground height; only the rail connecting an implausibly
            // mismatched pair is skipped; the next post is still fenced by
            // the segment after it.
            constexpr float MAX_RAIL_RISE = 2.5f;
            if (fabsf(postNext.y - post.y) > MAX_RAIL_RISE) continue;

            Vector3 mid = Vector3Scale(Vector3Add(post, postNext), 0.5f);
            float railLen = Vector3Distance(post, postNext);
            Vector3 dir = Vector3Normalize(Vector3Subtract(postNext, post));
            float yaw = atan2f(dir.x, dir.z);

            const float railHeights[2] = { RAIL_HEIGHT_A, RAIL_HEIGHT_B };
            for (int ri = 0; ri < 2; ri++) {
                float railY = railHeights[ri];
                Vector3 center = Vector3{ mid.x, mid.y + railY, mid.z };
                Vector3 half = Vector3{ RAIL_THICK * 0.5f, RAIL_THICK * 0.5f, railLen * 0.5f };

                Vector3 a = Vector3{ -half.x, -half.y, -half.z };
                Vector3 b = Vector3{  half.x, -half.y, -half.z };
                Vector3 c = Vector3{  half.x,  half.y, -half.z };
                Vector3 d = Vector3{ -half.x,  half.y, -half.z };
                Vector3 e = Vector3{ -half.x, -half.y,  half.z };
                Vector3 f = Vector3{  half.x, -half.y,  half.z };
                Vector3 g = Vector3{  half.x,  half.y,  half.z };
                Vector3 h = Vector3{ -half.x,  half.y,  half.z };

                auto rot = [&](Vector3 v) {
                    float cs = cosf(yaw), sn = sinf(yaw);
                    return Vector3{ v.x * cs + v.z * sn, v.y, -v.x * sn + v.z * cs };
                };
                auto place = [&](Vector3 v) { return Vector3Add(center, rot(v)); };

                mb.AddQuad(place(e), place(f), place(g), place(h), WOOD); // top-facing long rail
                mb.AddQuad(place(a), place(b), place(f), place(e), WOOD);
                mb.AddQuad(place(b), place(c), place(g), place(f), WOOD);
                mb.AddQuad(place(d), place(a), place(e), place(h), WOOD);
                mb.AddQuad(place(c), place(d), place(h), place(g), WOOD);
            }
        }
    }

    model_ = LoadModelFromMesh(mb.Build());
    model_.materials[0].shader = shader;
    loaded_ = true;
}

void Fence::Unload()
{
    if (loaded_) {
        UnloadModel(model_);
        loaded_ = false;
    }
}

void Fence::Draw() const
{
    if (loaded_) DrawModel(model_, Vector3Zero(), 1.0f, WHITE);
}

bool Fence::ResolveCollisions(Vector3 &position, float radius) const
{
    if (!loaded_) return false;

    int idx = road::NearestSampleIndex(position.x, position.z);
    int count = (int)road::Centreline().size();
    if (idx >= 0 && count > 0 && InGap((float)idx / (float)count)) return false; // open exit, not a wall

    const road::Sample &s = road::NearestSample(position.x, position.z);

    Vector3 toPos = Vector3Subtract(position, s.position);
    float lateral = Vector3DotProduct(toPos, s.right);

    float boundary = OFFSET - radius * 0.5f;
    if (lateral > boundary) {
        position.x -= s.right.x * (lateral - boundary);
        position.z -= s.right.z * (lateral - boundary);
        return true;
    }
    if (lateral < -boundary) {
        position.x -= s.right.x * (lateral + boundary);
        position.z -= s.right.z * (lateral + boundary);
        return true;
    }
    return false;
}

} // namespace fence
