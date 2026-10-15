#pragma once

#include "raylib.h"
#include <vector>

namespace meshbuild {

// Accumulates geometry from many small pieces into a single mesh, so things
// like a whole fence line cost one draw call instead of hundreds.
class MeshBuilder {
public:
    void AddQuad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color);
    void AddTri(Vector3 a, Vector3 b, Vector3 c, Color color);
    void AddBox(Vector3 center, Vector3 size, Color color);

    // Wheel-shaped primitive: a closed cylinder whose axis runs along world X.
    void AddCylinderX(Vector3 center, float radius, float halfLength, int segments,
                      Color side, Color cap);

    bool Empty() const { return indices_.empty(); }
    int VertexCount() const { return (int)(positions_.size() / 3); }

    // Averages normals across vertices that share a position, wherever the
    // angle between their face normals is below the threshold. Lets a loft
    // built from flat quad strips (the car body) read as a curved shell
    // while leaving genuinely sharp edges -- box corners, panel splits --
    // alone. Call once, after all the geometry that should share a smoothing
    // pass has been added, and before Build().
    void SmoothNormals(float angleThresholdDeg);

    // Transfers the accumulated geometry into a GPU mesh and clears the builder.
    Mesh Build();

private:
    void PushVertex(Vector3 p, Vector3 n, float u, float v, Color color);

    std::vector<float> positions_;
    std::vector<float> normals_;
    std::vector<float> texcoords_;
    std::vector<unsigned char> colors_;
    std::vector<unsigned short> indices_;
};

} // namespace meshbuild
