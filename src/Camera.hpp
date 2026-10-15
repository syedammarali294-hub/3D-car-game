#pragma once

#include "Car.hpp"
#include "Input.hpp"
#include "raylib.h"

namespace cam {

enum Mode {
    MODE_CHASE = 0,
    MODE_HOOD,
    MODE_BUMPER,
    MODE_ORBIT,
    MODE_CINEMATIC,
    MODE_COUNT
};

// Multi-mode follow camera. Cycles with the configured ACTION_CAMERA key.
class GameCamera {
public:
    void Init(const car::Car &target);
    void Update(const car::Car &target, const input::Input &input, float dt);

    const Camera3D &Get() const { return camera_; }
    Mode GetMode() const { return mode_; }
    const char *ModeLabel() const;

private:
    void UpdateChase(const car::Car &target, float dt);
    void UpdateHood(const car::Car &target, float dt);
    void UpdateBumper(const car::Car &target, float dt);
    void UpdateOrbit(const car::Car &target, float dt);
    void UpdateCinematic(const car::Car &target, float dt);

    Camera3D camera_ = { 0 };
    Mode mode_ = MODE_CHASE;
    float yaw_ = 0.0f;
    float orbitAngle_ = 0.0f;
    float cineTimer_ = 0.0f;
    float distance_ = 11.0f;
    float height_ = 4.2f;
};

} // namespace cam
