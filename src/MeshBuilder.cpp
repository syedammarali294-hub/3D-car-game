#include "MeshBuilder.hpp"

#include "raymath.h"
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace meshbuild {

void MeshBuilder::PushVertex(Vector3 p, Vector3 n, float u, float v, Color color)
{
    positions_.push_back(p.x);
    positions_.push_back(p.y);
    positions_.push_back(p.z);

    normals_.push_back(n.x);
    normals_.push_back(n.y);
    normals_.push_back(n.z);

    texcoords_.push_back(u);
    texcoords_.push_back(v);

    colors_.push_back(color.r);
    colors_.push_back(color.g);
    colors_.push_back(color.b);
    colors_.push_back(color.a);
}

void MeshBuilder::AddQuad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
{
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    unsigned short base = (unsigned short)VertexCount();

    PushVertex(a, n, 0.0f, 0.0f, color);
    PushVertex(b, n, 1.0f, 0.0f, color);
    PushVertex(c, n, 1.0f, 1.0f, color);
    PushVertex(d, n, 0.0f, 1.0f, color);

    indices_.push_back(base);
    indices_.push_back((unsigned short)(base + 1));
    indices_.push_back((unsigned short)(base + 2));
    indices_.push_back(base);
    indices_.push_back((unsigned short)(base + 2));
    indices_.push_back((unsigned short)(base + 3));
}

void MeshBuilder::AddTri(Vector3 a, Vector3 b, Vector3 c, Color color)
{
    Vector3 n = Vector3Normalize(Vector3CrossProduct(Vector3Subtract(b, a), Vector3Subtract(c, a)));
    unsigned short base = (unsigned short)VertexCount();

    PushVertex(a, n, 0.0f, 0.0f, color);
    PushVertex(b, n, 1.0f, 0.0f, color);
    PushVertex(c, n, 0.5f, 1.0f, color);

    indices_.push_back(base);
    indices_.push_back((unsigned short)(base + 1));
    indices_.push_back((unsigned short)(base + 2));
}

void MeshBuilder::AddCylinderX(Vector3 center, float radius, float halfLength, int segments,
                               Color side, Color cap)
{
    if (segments < 3) segments = 3;

    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / (float)segments * 2.0f * PI;
        float a1 = (float)(i + 1) / (float)segments * 2.0f * PI;

        float y0 = cosf(a0) * radius, z0 = sinf(a0) * radius;
        float y1 = cosf(a1) * radius, z1 = sinf(a1) * radius;

        Vector3 lo0 = Vector3{ center.x - halfLength, center.y + y0, center.z + z0 };
        Vector3 lo1 = Vector3{ center.x - halfLength, center.y + y1, center.z + z1 };
        Vector3 hi0 = Vector3{ center.x + halfLength, center.y + y0, center.z + z0 };
        Vector3 hi1 = Vector3{ center.x + halfLength, center.y + y1, center.z + z1 };

        AddQuad(lo0, lo1, hi1, hi0, side);

        Vector3 capLo = Vector3{ center.x - halfLength, center.y, center.z };
        Vector3 capHi = Vector3{ center.x + halfLength, center.y, center.z };
        AddTri(capLo, lo1, lo0, cap);
        AddTri(capHi, hi0, hi1, cap);
    }
}

void MeshBuilder::AddBox(Vector3 center, Vector3 size, Color color)
{
    float hx = size.x * 0.5f, hy = size.y * 0.5f, hz = size.z * 0.5f;
    float x = center.x, y = center.y, z = center.z;

    Vector3 p000 = Vector3{ x - hx, y - hy, z - hz };
    Vector3 p100 = Vector3{ x + hx, y - hy, z - hz };
    Vector3 p110 = Vector3{ x + hx, y + hy, z - hz };
    Vector3 p010 = Vector3{ x - hx, y + hy, z - hz };
    Vector3 p001 = Vector3{ x - hx, y - hy, z + hz };
    Vector3 p101 = Vector3{ x + hx, y - hy, z + hz };
    Vector3 p111 = Vector3{ x + hx, y + hy, z + hz };
    Vector3 p011 = Vector3{ x - hx, y + hy, z + hz };

    AddQuad(p001, p101, p111, p011, color); // front
    AddQuad(p100, p000, p010, p110, color); // back
    AddQuad(p000, p001, p011, p010, color); // left
    AddQuad(p101, p100, p110, p111, color); // right
    AddQuad(p010, p011, p111, p110, color); // top
    AddQuad(p000, p100, p101, p001, color); // bottom
}

void MeshBuilder::SmoothNormals(float angleThresholdDeg)
{
    const int n = VertexCount();
    if (n == 0) return;

    const float cosThreshold = cosf(angleThresholdDeg * DEG2RAD);

    // Bucket vertices by position so only genuinely coincident verts (shared
    // edges/corners from separate AddQuad/AddTri calls) are ever compared.
    // Positions come out of quad/tri generation exactly equal (no accumulated
    // float drift), so a plain key works without a fuzzy-match pass.
    auto key = [&](int i) {
        // Quantize lightly anyway, in case a future caller feeds in
        // positions built from slightly different arithmetic paths.
        long long x = (long long)llroundf(positions_[i * 3 + 0] * 4096.0f);
        long long y = (long long)llroundf(positions_[i * 3 + 1] * 4096.0f);
        long long z = (long long)llroundf(positions_[i * 3 + 2] * 4096.0f);
        return (x * 73856093LL) ^ (y * 19349663LL) ^ (z * 83492791LL);
    };

    std::unordered_map<long long, std::vector<int>> buckets;
    buckets.reserve(n);
    for (int i = 0; i < n; i++) buckets[key(i)].push_back(i);

    std::vector<Vector3> result(n);
    for (int i = 0; i < n; i++) {
        result[i] = Vector3{ normals_[i * 3 + 0], normals_[i * 3 + 1], normals_[i * 3 + 2] };
    }

    for (auto &entry : buckets) {
        std::vector<int> &group = entry.second;
        if (group.size() < 2) continue;

        for (int gi : group) {
            Vector3 ni = Vector3{ normals_[gi * 3 + 0], normals_[gi * 3 + 1], normals_[gi * 3 + 2] };
            Vector3 sum = ni;
            for (int gj : group) {
                if (gj == gi) continue;
                Vector3 nj = Vector3{ normals_[gj * 3 + 0], normals_[gj * 3 + 1], normals_[gj * 3 + 2] };
                if (Vector3DotProduct(ni, nj) >= cosThreshold) {
                    sum = Vector3Add(sum, nj);
                }
            }
            result[gi] = Vector3Normalize(sum);
        }
    }

    for (int i = 0; i < n; i++) {
        normals_[i * 3 + 0] = result[i].x;
        normals_[i * 3 + 1] = result[i].y;
        normals_[i * 3 + 2] = result[i].z;
    }
}

Mesh MeshBuilder::Build()
{
    Mesh mesh = { 0 };
    if (indices_.empty()) return mesh;

    mesh.vertexCount = VertexCount();
    mesh.triangleCount = (int)(indices_.size() / 3);

    mesh.vertices = (float *)MemAlloc((unsigned int)(positions_.size() * sizeof(float)));
    mesh.normals = (float *)MemAlloc((unsigned int)(normals_.size() * sizeof(float)));
    mesh.texcoords = (float *)MemAlloc((unsigned int)(texcoords_.size() * sizeof(float)));
    mesh.colors = (unsigned char *)MemAlloc((unsigned int)(colors_.size() * sizeof(unsigned char)));
    mesh.indices = (unsigned short *)MemAlloc((unsigned int)(indices_.size() * sizeof(unsigned short)));

    memcpy(mesh.vertices, positions_.data(), positions_.size() * sizeof(float));
    memcpy(mesh.normals, normals_.data(), normals_.size() * sizeof(float));
    memcpy(mesh.texcoords, texcoords_.data(), texcoords_.size() * sizeof(float));
    memcpy(mesh.colors, colors_.data(), colors_.size() * sizeof(unsigned char));
    memcpy(mesh.indices, indices_.data(), indices_.size() * sizeof(unsigned short));

    positions_.clear();
    normals_.clear();
    texcoords_.clear();
    colors_.clear();
    indices_.clear();

    UploadMesh(&mesh, false);
    return mesh;
}

} // namespace meshbuild
