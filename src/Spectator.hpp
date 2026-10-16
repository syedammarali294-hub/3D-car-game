#pragma once

#include "Input.hpp"
#include "raylib.h"

// Free-fly admin camera: no car, no collision, no engine sound (main.cpp
// simply stops calling EngineSound::Update while this is active -- see its
// own comment on why that's enough to go silent). Reuses whatever the
// player has their drive keys bound to for horizontal movement, so
// rebinding WASD to something else carries over here too instead of this
// having its own hardcoded set.
namespace spectator {

class FlyCam {
public:
    void Init(Vector3 position, float yaw);
    // Mouse-look only while the right mouse button is held (left-click
    // stays free for the minimap/UI, same reasoning as the map editor
    // reserving its own mouse buttons per-purpose).
    void Update(float dt, const input::Input &input);

    const Camera3D &Get() const { return camera_; }

private:
    Vector3 position_{ 0.0f, 0.0f, 0.0f };
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    Camera3D camera_{ 0 };
};

} // namespace spectator
