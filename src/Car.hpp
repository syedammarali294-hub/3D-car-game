#pragma once

#include "CarDatabase.hpp"
#include "Input.hpp"
#include "raylib.h"

namespace world { class World; }
namespace fence { class Fence; }
namespace traffic { class AiTraffic; }

namespace car {

class Car {
public:
    void Load(Shader shader, int carIndex, Color paintOverride);

    // Repaints without touching the mesh -- paintTint_ is only ever a
    // draw-time tint, never baked into geometry, so a colour-only change
    // never needed a full Load()/Unload() cycle in the first place.
    void SetPaint(Color paint) { paintTint_ = paint; }
    int CarIndex() const { return carIndex_; }
    void Unload();
    void Update(float dt, const input::Input &input, const world::World &world, const fence::Fence &fences,
               const traffic::AiTraffic *aiTraffic = nullptr);
    void Draw() const;

    Vector3 GetPosition() const { return position_; }
    float GetHeading() const { return heading_; }
    float GetSpeed() const { return speed_; }
    float GetTripKm() const { return tripMeters_ / 1000.0f; }
    bool IsInWater() const { return inWater_; }
    bool JustHitObstacle() const { return justHit_; } // true only the one frame contact begins
    float GetBoost() const { return boost_; }   // 0..1 remaining NOS fuel
    bool IsBoosting() const { return boosting_; }
    bool IsDrifting() const { return drifting_; }
    float GetDriftScore() const { return driftScore_; }       // live score of the current drift, 0 when not drifting
    float GetLastDriftScore() const { return lastDriftScore_; } // final score of the drift that just ended
    bool JustFinishedDrift() const { return driftJustEnded_; }  // true only the one frame it ends
    bool IsAutoDrive() const { return autoDrive_; }
    int GetCarIndex() const { return carIndex_; }

    // Gearbox state (see Gearbox.hpp). Gear is 1..GetGearCount() going forward;
    // IsReversing() means the HUD should show R instead.
    int GetGear() const { return gear_; }
    int GetGearCount() const;
    bool IsReversing() const { return speed_ < -0.3f; }
    float GetRpm() const { return rpm_; }    // 0..~1.1, drives the engine note
    bool JustShifted() const { return justShifted_; } // true only the frame a shift happens

    void Respawn(Vector3 position, float heading);
    void RespawnNearestRoad();

    // Draws this car's already-loaded body at an arbitrary position/heading
    // instead of its own current state -- used for the best-lap ghost
    // replay, which reuses the player's own model rather than loading a
    // second one.
    void DrawGhost(Vector3 position, float heading, Color tint) const;

private:
    Matrix BuildOrientation() const;

    Model body_ = { 0 };
    Model wheel_ = { 0 }; // stays zeroed for kit cars -- their mesh already includes wheels
    bool loaded_ = false;
    int carIndex_ = 0;
    float bodyScale_ = 1.0f;
    Color paintTint_ = WHITE;

    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 groundNormal_ = { 0.0f, 1.0f, 0.0f };
    float heading_ = 0.0f;
    float speed_ = 0.0f;
    float steerAngle_ = 0.0f;
    float wheelSpin_ = 0.0f;
    float bodyRoll_ = 0.0f;
    bool inWater_ = false;
    float tripMeters_ = 0.0f; // odometer: accumulates |speed|*dt, never reset by Respawn
    float stuckTimer_ = 0.0f; // seconds spent holding throttle with near-zero speed
    float boost_ = 1.0f;      // 0..1 NOS fuel; drains while boosting, regenerates when not
    bool boosting_ = false;

    bool drifting_ = false;
    float driftScore_ = 0.0f;
    float lastDriftScore_ = 0.0f;
    float slipAngle_ = 0.0f; // visual-only body yaw offset during a drift
    bool autoDrive_ = false;
    float autoProgress_ = -1.0f; // fractional index into road::Centreline(); -1 = needs resync
    void UpdateAutoDrive(float dt);
    bool driftJustEnded_ = false;
    bool wasBlocked_ = false;
    bool justHit_ = false;

    int gear_ = 1;
    float shiftTimer_ = 0.0f; // torque is cut while > 0, so a shift is felt
    float rpm_ = 0.0f;
    bool justShifted_ = false;
};

} // namespace car
