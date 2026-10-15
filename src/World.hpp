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
    void Load(Shader shader);
    void Unload();
    void Draw(Vector3 viewPos, float drawDistance) const;

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
