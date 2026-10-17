#pragma once

#include "raylib.h"
#include <vector>

namespace world {

struct Prop {
    Vector3 position;
    float rotation;
    float scale;
    int modelIndex;
    float colliderRadius; // 0 for props the car can drive through (rocks are tiny, decorative)
};

class World {
public:
    // densityScale multiplies PROP_TARGET -- the Quality setting's one
    // real lever on this system (Settings > Car > QUALITY). 1.0 = the
    // normal density; call sites pass a smaller value for Low.
    void Load(Shader shader, float densityScale = 1.0f);
    void Unload();
    // viewForward/fovyDeg/aspect add a cheap cone-based frustum cull on top
    // of the existing distance cull -- props well outside the camera's view
    // (behind it, or far off to the side) are skipped entirely rather than
    // instanced-drawn only to be clipped by the GPU. viewForward need not
    // be normalized.
    void Draw(Vector3 viewPos, float drawDistance, Vector3 viewForward, float fovyDeg, float aspect) const;

    int GetVisibleCount() const { return visibleCount_; }

    // Pushes a circle (e.g. the car) out of any overlapping tree trunk.
    // Returns true if a push happened so the caller can react (kill speed, etc).
    bool ResolveCollisions(Vector3 &position, float radius) const;

private:
    std::vector<Model> models_;
    std::vector<Prop> props_;
    mutable int visibleCount_ = 0;
    bool loaded_ = false;
};

} // namespace world
