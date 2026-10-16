#pragma once

#include "raylib.h"
#include <vector>

namespace fence {

// A single merged mesh of fence posts and rails run along both shoulders of
// the road, built once from the road centreline. One draw call for the
// whole circuit keeps this cheap on weak GPUs.
class Fence {
public:
    // gaps: (startFraction, endFraction) pairs along the lap (0..1) where
    // the fence has a deliberate opening -- see Track.hpp. No posts/rails
    // are built there, and ResolveCollisions doesn't block there either.
    void Load(Shader shader, const std::vector<Vector2> &gaps);
    void Unload();
    void Draw() const;

    // Pushes a circle (e.g. the car) back behind the rail line if it has
    // crossed it. Returns true if a push happened. No-ops inside a gap.
    bool ResolveCollisions(Vector3 &position, float radius) const;

private:
    bool InGap(float fraction) const;

    Model model_ = { 0 };
    bool loaded_ = false;
    std::vector<Vector2> gaps_;
};

} // namespace fence
