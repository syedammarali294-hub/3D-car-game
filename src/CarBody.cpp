#include "CarBody.hpp"

#include "MeshBuilder.hpp"
#include "raymath.h"
#include <cmath>
#include <vector>

namespace carbody {

namespace {

// One cross-section of the lofted shell. topHalfWidth is normally a little
// narrower than halfWidth: real body sides lean inward toward the roofline
// (tumblehome) instead of standing dead vertical, which is what kept the
// early version of this shape reading as a slab-sided box.
struct Station {
    float z;
    float halfWidth;
    float topHalfWidth;
    float yBottom;
    float yTop;
};

Color Shade(Color c, float f)
{
    return Color{
        (unsigned char)Clamp(c.r * f, 0.0f, 255.0f),
        (unsigned char)Clamp(c.g * f, 0.0f, 255.0f),
        (unsigned char)Clamp(c.b * f, 0.0f, 255.0f),
        c.a
    };
}

float Smooth01(float t)
{
    t = Clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Soft bump used to bulge the bodywork out over an axle.
float FenderBump(float z, float axleZ, float reach)
{
    float d = fabsf(z - axleZ) / reach;
    if (d >= 1.0f) return 0.0f;
    return (1.0f - d * d) * (1.0f - d * d);
}

// Half of a cross-section outline, from the sill (v=0) up to the centre of the
// roof (v=1). u is a fraction of the station's width at that height. The
// shoulder rounding here is what stops a lofted shell reading as a slab: a
// two-row loft can only ever produce vertical sides meeting a flat lid.
struct ProfilePoint { float u, v; };

const ProfilePoint PROFILE[] = {
    { 1.000f, 0.000f },
    { 1.000f, 0.300f },
    { 0.985f, 0.550f },
    { 0.940f, 0.750f },
    { 0.840f, 0.880f },
    { 0.620f, 0.965f },
    { 0.330f, 1.000f },
    { 0.000f, 1.000f },
};
constexpr int PROFILE_N = (int)(sizeof(PROFILE) / sizeof(PROFILE[0]));
// Right half up, mirrored half back down: one closed loop minus the floor.
constexpr int RING_N = PROFILE_N * 2 - 1;

// Expands a station into its full ring of outline points.
void BuildRing(const Station &s, Vector3 *out)
{
    for (int i = 0; i < PROFILE_N; i++) {
        const ProfilePoint &p = PROFILE[i];
        float w = Lerp(s.halfWidth, s.topHalfWidth, p.v) * p.u;
        float y = Lerp(s.yBottom, s.yTop, p.v);
        out[i] = Vector3{ w, y, s.z };
        if (i < PROFILE_N - 1) out[RING_N - 1 - i] = Vector3{ -w, y, s.z };
    }
}

// Connects a chain of cross-sections into a closed shell.
void Loft(meshbuild::MeshBuilder &mb, const std::vector<Station> &s,
          Color side, Color top, Color bottom, bool capEnds)
{
    const int n = (int)s.size();
    if (n < 2) return;

    std::vector<Vector3> ringA(RING_N), ringB(RING_N);
    BuildRing(s[0], ringA.data());

    for (int i = 0; i < n - 1; i++) {
        BuildRing(s[i + 1], ringB.data());

        for (int k = 0; k < RING_N - 1; k++) {
            // Blend flank colour into roof colour over the shoulder so the
            // rounded corner catches a highlight instead of going flat.
            float v = 0.5f * (PROFILE[k < PROFILE_N ? k : RING_N - 1 - k].v +
                              PROFILE[(k + 1) < PROFILE_N ? (k + 1) : RING_N - 2 - k].v);
            Color c = ColorLerp(side, top, Smooth01(v));
            mb.AddQuad(ringA[k], ringB[k], ringB[k + 1], ringA[k + 1], c);
        }

        // Floor: closes the loop from the left sill back round to the right.
        mb.AddQuad(ringA[0], ringA[RING_N - 1], ringB[RING_N - 1], ringB[0], bottom);

        ringA.swap(ringB);
    }

    if (capEnds) {
        std::vector<Vector3> ring(RING_N);
        Color capC = Shade(side, 0.92f);

        BuildRing(s.front(), ring.data());
        Vector3 cf = Vector3{ 0.0f, (s.front().yBottom + s.front().yTop) * 0.5f, s.front().z };
        for (int k = 0; k < RING_N - 1; k++) mb.AddTri(cf, ring[k], ring[k + 1], capC);
        mb.AddTri(cf, ring[RING_N - 1], ring[0], capC);

        BuildRing(s.back(), ring.data());
        Vector3 cb = Vector3{ 0.0f, (s.back().yBottom + s.back().yTop) * 0.5f, s.back().z };
        for (int k = 0; k < RING_N - 1; k++) mb.AddTri(cb, ring[k + 1], ring[k], capC);
        mb.AddTri(cb, ring[0], ring[RING_N - 1], capC);
    }
}

} // namespace

Mesh BuildBody(const Design &d)
{
    meshbuild::MeshBuilder mb;

    const float halfL = d.length * 0.5f;
    const float frontAxle =  d.wheelbase * 0.5f;
    const float rearAxle  = -d.wheelbase * 0.5f;
    const float baseHalfW = d.width * 0.5f;

    // ---- lower body -------------------------------------------------------
    const int STATIONS = 25;
    std::vector<Station> body;
    body.reserve(STATIONS);

    for (int i = 0; i < STATIONS; i++) {
        float t = (float)i / (float)(STATIONS - 1);   // 0 at the tail, 1 at the nose
        float z = -halfL + t * d.length;

        float w = 1.0f;
        if (t < 0.14f)      w = Lerp(d.rearTaper, 1.0f, Smooth01(t / 0.14f));
        else if (t > 0.84f) w = Lerp(1.0f, d.noseTaper, Smooth01((t - 0.84f) / 0.16f));

        float hw = baseHalfW * w;
        hw += d.flare * (FenderBump(z, frontAxle, d.wheelRadius * 2.0f) +
                         FenderBump(z, rearAxle,  d.wheelRadius * 2.0f));

        // Round the extreme ends off in plan view, so the nose and tail close
        // with a small curved cap instead of a full-width vertical wall.
        if (t > 0.94f) {
            float e = (t - 0.94f) / 0.06f;
            hw *= sqrtf(1.0f - e * e * 0.55f);
        } else if (t < 0.06f) {
            float e = (0.06f - t) / 0.06f;
            hw *= sqrtf(1.0f - e * e * 0.45f);
        }

        float top = d.beltHeight;
        if (t > 0.74f) top -= d.noseDrop * Smooth01((t - 0.74f) / 0.26f);
        if (t < 0.16f) top -= d.tailDrop * Smooth01((0.16f - t) / 0.16f);

        float bot = d.rideHeight;
        if (t > 0.93f) bot += 0.06f * Smooth01((t - 0.93f) / 0.07f);
        if (t < 0.07f) bot += 0.08f * Smooth01((0.07f - t) / 0.07f);

        Station s;
        s.z = z;
        s.halfWidth = hw;
        s.topHalfWidth = hw * 0.93f; // tumblehome: sides lean in toward the beltline
        s.yBottom = bot;
        s.yTop = top;
        body.push_back(s);
    }

    Loft(mb, body, Shade(d.paint, 0.86f), Shade(d.paint, 1.22f), Shade(d.paint, 0.40f), true);

    // Height of the belt line at a given z, so the cabin sits flush on the body.
    auto beltAt = [&](float z) {
        float t = Clamp((z + halfL) / d.length, 0.0f, 1.0f);
        float top = d.beltHeight;
        if (t > 0.74f) top -= d.noseDrop * Smooth01((t - 0.74f) / 0.26f);
        if (t < 0.16f) top -= d.tailDrop * Smooth01((0.16f - t) / 0.16f);
        return top;
    };
    auto widthAt = [&](float z) {
        float t = Clamp((z + halfL) / d.length, 0.0f, 1.0f);
        float w = 1.0f;
        if (t < 0.14f)      w = Lerp(d.rearTaper, 1.0f, Smooth01(t / 0.14f));
        else if (t > 0.84f) w = Lerp(1.0f, d.noseTaper, Smooth01((t - 0.84f) / 0.16f));
        return baseHalfW * w;
    };

    // ---- greenhouse -------------------------------------------------------
    const float roofRear = d.cabinRear + d.rakeRear;
    const float roofFront = d.cabinFront - d.rakeFront;

    std::vector<Station> cabin;
    const float zs[4] = { d.cabinRear, roofRear, roofFront, d.cabinFront };
    for (int i = 0; i < 4; i++) {
        float z = zs[i];
        bool roofLevel = (i == 1 || i == 2);

        Station s;
        s.z = z;
        s.halfWidth = widthAt(z) * d.cabinWidth * (roofLevel ? 0.96f : 1.0f);
        s.topHalfWidth = s.halfWidth * (roofLevel ? 0.94f : 0.97f); // the ring profile does the doming
        s.yBottom = beltAt(z) - 0.02f;
        // The end stations are barely taller than the body, which is what gives
        // the windscreen and backlight their rake.
        s.yTop = s.yBottom + (roofLevel ? d.roofHeight : 0.04f);
        cabin.push_back(s);
    }

    Loft(mb, cabin, d.glass, Shade(d.paint, 1.18f), Shade(d.paint, 0.5f), true);

    // ---- lights, grille, aero --------------------------------------------
    const Color DARK = Color{ 26, 27, 30, 255 };
    const Color LAMP = Color{ 236, 240, 248, 255 };
    const Color TAIL = Color{ 196, 36, 34, 255 };

    float noseZ = halfL - 0.06f;
    float tailZ = -halfL + 0.06f;
    float noseHW = widthAt(noseZ);
    float tailHW = widthAt(tailZ);
    float noseY = beltAt(noseZ) - 0.12f;
    float tailY = beltAt(tailZ) - 0.10f;

    for (int sign = -1; sign <= 1; sign += 2) {
        mb.AddBox(Vector3{ sign * noseHW * 0.60f, noseY, noseZ },
                  Vector3{ noseHW * 0.52f, 0.14f, 0.10f }, LAMP);
        mb.AddBox(Vector3{ sign * tailHW * 0.62f, tailY, tailZ },
                  Vector3{ tailHW * 0.48f, 0.13f, 0.08f }, TAIL);
        // Mirrors on short stalks.
        mb.AddBox(Vector3{ sign * (baseHalfW + 0.10f), d.beltHeight + 0.06f, d.cabinFront - 0.15f },
                  Vector3{ 0.22f, 0.09f, 0.13f }, Shade(d.paint, 0.9f));
    }

    mb.AddBox(Vector3{ 0.0f, noseY - 0.16f, noseZ }, Vector3{ noseHW * 1.0f, 0.18f, 0.10f }, DARK);
    mb.AddBox(Vector3{ 0.0f, d.rideHeight + 0.02f, noseZ - 0.02f },
              Vector3{ baseHalfW * 1.9f, 0.07f, 0.26f }, DARK);          // front splitter
    mb.AddBox(Vector3{ 0.0f, d.rideHeight + 0.03f, tailZ + 0.04f },
              Vector3{ baseHalfW * 1.7f, 0.10f, 0.22f }, DARK);          // rear diffuser

    for (int sign = -1; sign <= 1; sign += 2) {
        mb.AddBox(Vector3{ sign * baseHalfW * 0.45f, d.rideHeight + 0.06f, tailZ - 0.02f },
                  Vector3{ 0.13f, 0.11f, 0.12f }, Color{ 150, 152, 158, 255 });
    }

    if (d.wingHeight > 0.0f) {
        float wz = tailZ + 0.22f;
        float wy = beltAt(wz) + d.wingHeight;
        float whw = baseHalfW * 0.92f;
        mb.AddBox(Vector3{ 0.0f, wy, wz }, Vector3{ whw * 2.0f, 0.06f, d.wingChord }, d.accent);
        for (int sign = -1; sign <= 1; sign += 2) {
            mb.AddBox(Vector3{ sign * whw * 0.8f, (wy + beltAt(wz)) * 0.5f, wz },
                      Vector3{ 0.06f, d.wingHeight, 0.14f }, Shade(d.accent, 0.8f));
        }
    }

    // Wheel arches: dark liners so the gap above each tyre does not look hollow.
    for (int fz = 0; fz < 2; fz++) {
        float az = (fz == 0) ? frontAxle : rearAxle;
        for (int sign = -1; sign <= 1; sign += 2) {
            mb.AddBox(Vector3{ sign * (baseHalfW + d.flare * 0.5f), d.wheelRadius * 1.15f, az },
                      Vector3{ 0.10f, d.wheelRadius * 0.9f, d.wheelRadius * 2.1f }, DARK);
        }
    }

    // Smooth only the genuinely curved joins (the loft's rounded ring
    // profile); anything sharper than this -- box corners, panel splits,
    // the body/cabin seam -- stays faceted on purpose.
    mb.SmoothNormals(40.0f);

    return mb.Build();
}

Mesh BuildWheel(const Design &d)
{
    meshbuild::MeshBuilder mb;

    const Color TYRE = Color{ 28, 28, 31, 255 };
    const Color TREAD = Color{ 20, 20, 23, 255 };
    const Color HUB = Shade(d.accent, 0.7f);

    float hw = d.wheelWidth * 0.5f;

    mb.AddCylinderX(Vector3Zero(), d.wheelRadius, hw, 20, TREAD, TYRE);
    // Rim faces, pushed just proud of the tyre on both sides.
    mb.AddCylinderX(Vector3Zero(), d.wheelRadius * 0.66f, hw + 0.012f, 18, HUB, d.accent);
    mb.AddCylinderX(Vector3Zero(), d.wheelRadius * 0.22f, hw + 0.02f, 10, HUB, Shade(d.accent, 1.15f));

    // Rounds the tyre's curved side out; the rim/hub discs are flat faces
    // meeting at a shallow angle too, so a modest threshold is enough.
    mb.SmoothNormals(35.0f);

    return mb.Build();
}

} // namespace carbody
