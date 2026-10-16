#include "Camera.hpp"

#include "CarDatabase.hpp"
#include "Terrain.hpp"
#include "raymath.h"
#include <cmath>

namespace cam {

namespace {

// Keeps a camera position from sinking into a hill or under the water.
Vector3 ClampAboveGround(Vector3 p, float minClearance)
{
    float groundAtCam = terrain::HeightAt(p.x, p.z) + minClearance;
    if (p.y < groundAtCam) p.y = groundAtCam;
    if (p.y < terrain::WATER_LEVEL + minClearance) p.y = terrain::WATER_LEVEL + minClearance;
    return p;
}

// Proper exponential-decay smoothing factor, used in place of the more
// common Clamp(rate * dt, 0, 1) approximation every camera mode below used
// to lerp with. That approximation only holds for small dt (high frame
// rate) -- under any real hitch or frame drop, rate*dt can exceed 1 and
// gets clamped there, which doesn't ease the camera toward its target, it
// snaps it there instantly. 1 - exp(-rate*dt) asymptotically approaches 1
// without ever doing that, so camera motion stays smooth and consistent
// regardless of frame-time variance, not just at a steady 60fps.
float SmoothFactor(float rate, float dt)
{
    return 1.0f - expf(-rate * dt);
}

} // namespace

void GameCamera::Init(const car::Car &target)
{
    camera_.up = Vector3{ 0.0f, 1.0f, 0.0f };
    camera_.fovy = 60.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
    yaw_ = target.GetHeading();
    mode_ = MODE_CHASE;
    Update(target, input::Input{}, 0.016f);
}

void GameCamera::Update(const car::Car &target, const input::Input &input, float dt)
{
    if (input.Pressed(input::ACTION_CAMERA)) {
        mode_ = (Mode)((mode_ + 1) % MODE_COUNT);
    }

    switch (mode_) {
        case MODE_CHASE:     UpdateChase(target, dt); break;
        case MODE_HOOD:      UpdateHood(target, dt); break;
        case MODE_BUMPER:    UpdateBumper(target, dt); break;
        case MODE_ORBIT:     UpdateOrbit(target, dt); break;
        case MODE_CINEMATIC: UpdateCinematic(target, dt); break;
        default: break;
    }
}

void GameCamera::UpdateChase(const car::Car &target, float dt)
{
    Vector3 carPos = target.GetPosition();

    float diff = Wrap(target.GetHeading() - yaw_, -PI, PI);
    yaw_ += diff * SmoothFactor(5.0f, dt);

    float pullBack = distance_ + fabsf(target.GetSpeed()) * 0.06f;

    Vector3 desired = Vector3{
        carPos.x - sinf(yaw_) * pullBack,
        carPos.y + height_,
        carPos.z - cosf(yaw_) * pullBack
    };
    desired = ClampAboveGround(desired, 2.0f);

    camera_.position = Vector3Lerp(camera_.position, desired, SmoothFactor(8.0f, dt));
    camera_.target = Vector3Add(carPos, Vector3{ 0.0f, 1.6f, 0.0f });
    camera_.fovy = 60.0f;
}

void GameCamera::UpdateHood(const car::Car &target, float dt)
{
    const cardb::CarSpec &spec = cardb::Roster()[target.GetCarIndex()];
    Vector3 carPos = target.GetPosition();
    float h = target.GetHeading();
    Vector3 forward = Vector3{ sinf(h), 0.0f, cosf(h) };

    // The cabin shell is a sealed box (no open glass), so the eye must sit
    // clearly above the roofline -- anywhere inside the greenhouse height
    // range puts the camera inside solid geometry and the screen goes dark.
    float eyeZ = spec.design.cabinFront - 0.15f;
    float eyeY = spec.design.beltHeight + spec.design.roofHeight + 0.18f;
    Vector3 eye = Vector3Add(carPos, Vector3{ forward.x * eyeZ, eyeY, forward.z * eyeZ });
    eye = ClampAboveGround(eye, 0.3f);

    camera_.position = Vector3Lerp(camera_.position, eye, SmoothFactor(20.0f, dt));
    camera_.target = Vector3Add(eye, Vector3Scale(forward, 8.0f));
    camera_.fovy = 68.0f;
}

void GameCamera::UpdateBumper(const car::Car &target, float dt)
{
    const cardb::CarSpec &spec = cardb::Roster()[target.GetCarIndex()];
    Vector3 carPos = target.GetPosition();
    float h = target.GetHeading();
    Vector3 forward = Vector3{ sinf(h), 0.0f, cosf(h) };

    float eyeZ = spec.design.length * 0.5f + 0.4f;
    Vector3 eye = Vector3Add(carPos, Vector3{
        forward.x * eyeZ, spec.design.rideHeight + 0.45f, forward.z * eyeZ
    });
    eye = ClampAboveGround(eye, 0.2f);

    camera_.position = Vector3Lerp(camera_.position, eye, SmoothFactor(20.0f, dt));
    camera_.target = Vector3Add(eye, Vector3Scale(forward, 8.0f));
    camera_.fovy = 72.0f;
}

void GameCamera::UpdateOrbit(const car::Car &target, float dt)
{
    Vector3 carPos = target.GetPosition();
    orbitAngle_ += dt * 0.35f;

    float r = 9.5f;
    Vector3 desired = Vector3{
        carPos.x - sinf(orbitAngle_) * r,
        carPos.y + 3.6f,
        carPos.z - cosf(orbitAngle_) * r
    };
    desired = ClampAboveGround(desired, 1.6f);

    camera_.position = Vector3Lerp(camera_.position, desired, SmoothFactor(6.0f, dt));
    camera_.target = Vector3Add(carPos, Vector3{ 0.0f, 1.2f, 0.0f });
    camera_.fovy = 55.0f;
}

void GameCamera::UpdateCinematic(const car::Car &target, float dt)
{
    Vector3 carPos = target.GetPosition();
    cineTimer_ += dt * 0.5f;

    // A low, wide sweep that favours drama over usability -- fine for a
    // cosmetic replay-style mode.
    float r = 7.0f + sinf(cineTimer_ * 0.6f) * 2.0f;
    float ang = cineTimer_;
    Vector3 desired = Vector3{
        carPos.x + sinf(ang) * r,
        carPos.y + 1.1f + fabsf(sinf(cineTimer_ * 1.3f)) * 0.8f,
        carPos.z + cosf(ang) * r
    };
    desired = ClampAboveGround(desired, 0.8f);

    camera_.position = Vector3Lerp(camera_.position, desired, SmoothFactor(3.0f, dt));
    camera_.target = Vector3Add(carPos, Vector3{ 0.0f, 1.0f, 0.0f });
    camera_.fovy = 46.0f;
}

const char *GameCamera::ModeLabel() const
{
    switch (mode_) {
        case MODE_CHASE:     return "Chase";
        case MODE_HOOD:      return "Hood";
        case MODE_BUMPER:    return "Bumper";
        case MODE_ORBIT:     return "Orbit";
        case MODE_CINEMATIC: return "Cinematic";
        default: return "?";
    }
}

} // namespace cam
