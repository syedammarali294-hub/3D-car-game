#include "Spectator.hpp"

#include "raymath.h"

namespace spectator {

void FlyCam::Init(Vector3 position, float yaw)
{
    position_ = position;
    yaw_ = yaw;
    pitch_ = 0.0f;
    camera_.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera_.fovy = 65.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void FlyCam::Update(float dt, const input::Input &input)
{
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 d = GetMouseDelta();
        yaw_ -= d.x * 0.0035f;
        pitch_ = Clamp(pitch_ - d.y * 0.0035f, -1.5f, 1.5f);
    }

    // Horizontal movement stays on the flat plane regardless of where
    // you're looking -- only Shift/Ctrl move you vertically, so looking up
    // to admire the mountains doesn't also start climbing into them.
    Vector3 flatForward = Vector3{ sinf(yaw_), 0.0f, cosf(yaw_) };
    Vector3 flatRight = Vector3{ cosf(yaw_), 0.0f, -sinf(yaw_) };
    Vector3 lookForward = Vector3{ sinf(yaw_) * cosf(pitch_), sinf(pitch_), cosf(yaw_) * cosf(pitch_) };

    Vector3 move{ 0.0f, 0.0f, 0.0f };
    if (input.Down(input::ACTION_ACCELERATE))   move = Vector3Add(move, flatForward);
    if (input.Down(input::ACTION_BRAKE))        move = Vector3Subtract(move, flatForward);
    if (input.Down(input::ACTION_STEER_RIGHT))  move = Vector3Add(move, flatRight);
    if (input.Down(input::ACTION_STEER_LEFT))   move = Vector3Subtract(move, flatRight);
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))     move.y += 1.0f;
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) move.y -= 1.0f;

    const float baseSpeed = 40.0f;
    const float boostSpeed = 110.0f; // held handbrake key doubles as a speed boost, same as NOS would
    float speed = input.Down(input::ACTION_HANDBRAKE) ? boostSpeed : baseSpeed;

    if (Vector3Length(move) > 0.0001f) {
        position_ = Vector3Add(position_, Vector3Scale(Vector3Normalize(move), speed * dt));
    }

    camera_.position = position_;
    camera_.target = Vector3Add(position_, lookForward);
}

} // namespace spectator
