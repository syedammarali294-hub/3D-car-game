#pragma once

#include "raylib.h"

namespace fence {

// A single merged mesh of fence posts and rails run along both shoulders of
// the road, built once from the road centreline. One draw call for the
// whole circuit keeps this cheap on weak GPUs.
class Fence {
public:
    void Load(Shader shader);
    void Unload();
    void Draw() const;

    // Pushes a circle (e.g. the car) back behind the rail line if it has
    // crossed it. Returns true if a push happened.
    bool ResolveCollisions(Vector3 &position, float radius) const;

private:
    Model model_ = { 0 };
    bool loaded_ = false;
};

} // namespace fence
