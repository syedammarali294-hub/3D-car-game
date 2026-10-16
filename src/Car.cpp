#include "Car.hpp"

#include "AiTraffic.hpp"
#include "CarBody.hpp"
#include "Gearbox.hpp"
#include "Settings.hpp"
#include "SkidMarks.hpp"
#include "Upgrades.hpp"
#include "Fence.hpp"
#include "Road.hpp"
#include "Terrain.hpp"
#include "World.hpp"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>

namespace car {

namespace {

// Same fix as Camera.cpp's SmoothFactor and for the same reason: a bare
// `rate * dt` lerp factor (no clamp) can exceed 1.0 at low frame rate, and
// raylib's Lerp(a, b, t) with t > 1 extrapolates PAST the target instead of
// easing toward it -- steering/body-roll could overshoot and oscillate
// during any real frame drop, not just look slightly off. This is the
// correct frame-rate-independent exponential-decay smoothing factor.
float SmoothFactor(float rate, float dt)
{
    return 1.0f - expf(-rate * dt);
}

float ApproachZero(float value, float amount)
{
    if (value > 0.0f) return (value > amount) ? value - amount : 0.0f;
    if (value < 0.0f) return (value < -amount) ? value + amount : 0.0f;
    return 0.0f;
}

} // namespace

void Car::Load(Shader shader, int carIndex, Color paintOverride)
{
    carIndex_ = carIndex;
    const cardb::CarSpec &spec = cardb::Roster()[carIndex];
    paintTint_ = paintOverride;

    // Kenney Car Kit (CC0, assets/models/car/) -- a licensed, real asset
    // pack, not the procedural loft. Its body mesh already includes its own
    // wheels, so there is no separate wheel_ model to load; wheel_ stays
    // zeroed and Draw() skips the spin/steer animation for these cars (see
    // TODO.md -- that's a known, accepted trade for real geometry today).
    body_ = LoadModel(spec.kitModel);
    for (int i = 0; i < body_.materialCount; i++) body_.materials[i].shader = shader;
    wheel_ = Model{ 0 };

    // The kit's own units don't match this project's Design::length; scale
    // the whole body so its actual bounding-box length lands on
    // Design::length, which keeps every other Design-derived offset
    // (wheelbase, cabin position, camera framing) consistent with before.
    BoundingBox bb = GetModelBoundingBox(body_);
    float kitLength = bb.max.z - bb.min.z;
    bodyScale_ = (kitLength > 0.01f) ? spec.design.length / kitLength : 1.0f;

    position_ = Vector3{ 0.0f, terrain::HeightAt(0.0f, 0.0f), 0.0f };
    tripMeters_ = 0.0f;
    gear_ = 1;
    shiftTimer_ = 0.0f;
    rpm_ = 0.0f;
    loaded_ = true;
}

void Car::Unload()
{
    if (loaded_) {
        UnloadModel(body_);
        if (wheel_.meshCount > 0) UnloadModel(wheel_);
        loaded_ = false;
    }
}

void Car::Respawn(Vector3 position, float heading)
{
    position_ = position;
    position_.y = terrain::HeightAt(position_.x, position_.z) + 0.25f; // settle from just above the surface
    heading_ = heading;
    speed_ = 0.0f;
    steerAngle_ = 0.0f;
    wheelSpin_ = 0.0f;
    bodyRoll_ = 0.0f;
    stuckTimer_ = 0.0f;
    slipAngle_ = 0.0f;
    gear_ = 1;
    shiftTimer_ = 0.0f;
    rpm_ = 0.0f;
    groundNormal_ = terrain::NormalAt(position_.x, position_.z);
    skidmarks::Break(); // a teleport shouldn't connect to whatever trail was left behind
}

int Car::GetGearCount() const
{
    return cardb::Roster()[carIndex_].gears;
}

// Snaps to the nearest point on the road circuit to where the car actually
// is, rather than always sending it back to the fixed start line -- so
// recovering from a ditch on the far side of the map doesn't throw away the
// whole drive.
void Car::RespawnNearestRoad()
{
    const road::Sample &s = road::NearestSample(position_.x, position_.z);
    float h = atan2f(s.forward.x, s.forward.z);
    Respawn(s.position, h);
}

void Car::Update(float dt, const input::Input &input, const world::World &world, const fence::Fence &fences,
                 const traffic::AiTraffic *aiTraffic)
{
    const cardb::CarSpec &spec = cardb::Roster()[carIndex_];

    // Garage upgrades: flat multipliers on top of the roster's stock
    // numbers, applied here rather than mutating CarDatabase so the roster
    // stays the single source of truth for a car's unmodified stats.
    const upgrades::CarUpgrade &upg = upgrades::Get(carIndex_);
    const float engineMul = upgrades::EngineMultiplier(upg.engine);
    const float handlingMul = upgrades::HandlingMultiplier(upg.handling);
    const float nitroRegenMul = upgrades::NitroRegenMultiplier(upg.nitro);
    const float nitroCapMul = upgrades::NitroCapMultiplier(upg.nitro);
    const float effAccel = spec.acceleration * engineMul;
    const float effMaxSpeed = spec.maxSpeed * engineMul;
    const float effTurnRate = spec.turnRate * handlingMul;

    bool throttle = input.Down(input::ACTION_ACCELERATE);
    bool brake = input.Down(input::ACTION_BRAKE);
    bool steerLeft = input.Down(input::ACTION_STEER_LEFT);
    bool steerRight = input.Down(input::ACTION_STEER_RIGHT);
    bool handbrake = input.Down(input::ACTION_HANDBRAKE);
    bool boostHeld = input.Down(input::ACTION_BOOST);

    if (input.Pressed(input::ACTION_RESET)) {
        RespawnNearestRoad();
    }

    // Auto-drive: F toggles it; touching steering or the pedals by hand
    // hands control straight back, so there's never a fight over the wheel.
    //
    // This used to steer *toward* the road through the normal physics --
    // acceleration, turn-rate-limited heading, the lot -- the same way a
    // player would. At real cruising speed the turn rate couldn't keep up
    // with the circuit's tighter bends, so it ran wide and clipped the
    // fence exactly the way a player understeering would. AiTraffic never
    // has this problem because it isn't steering at all: it reads its
    // position and heading directly off the centreline every frame, so it
    // is kinematically incapable of leaving the road. UpdateAutoDrive()
    // below does the same thing for the player car while this is active.
    if (input.Pressed(input::ACTION_AUTODRIVE)) {
        autoDrive_ = !autoDrive_;
        if (autoDrive_) autoProgress_ = -1.0f; // resync to wherever the car actually is
    }
    if (autoDrive_ && (throttle || brake || steerLeft || steerRight)) autoDrive_ = false;

    if (autoDrive_) {
        UpdateAutoDrive(dt);
        return;
    }

    // Water drags the car down to a crawl instead of hard-blocking it.
    float powerScale = inWater_ ? 0.25f : 1.0f;

    // ---- Gearbox ----------------------------------------------------------
    // Automatic: climbs gears near the redline, drops them as speed falls.
    // Manual: the player shifts with the gear keys; the rev limiter and a
    // little engine braking make a wrong gear actually matter.
    const int gearCount = spec.gears;
    justShifted_ = false;
    if (shiftTimer_ > 0.0f) shiftTimer_ -= dt;

    if (gear_ > gearCount) gear_ = gearCount;
    if (gear_ < 1) gear_ = 1;

    const bool manual = settings::Get().manual;
    const float fwd = speed_ > 0.0f ? speed_ : 0.0f;

    if (manual) {
        // Direct gear select -- fixed number-key bindings (not routed
        // through the rebindable Input system; rebinding nine separate
        // per-gear keys isn't worth the Settings UI it would need). 1-9
        // covers every gear count this project uses (max 8, see
        // Gearbox.hpp). Existing over-rev protection elsewhere in Update()
        // already penalises selecting too low a gear at speed, so this
        // doesn't need its own safety check.
        for (int g = 1; g <= gearCount && g <= 9; g++) {
            if (IsKeyPressed(KEY_ONE + (g - 1)) && gear_ != g) {
                gear_ = g;
                shiftTimer_ = 0.15f;
                justShifted_ = true;
            }
        }

        if (input.Pressed(input::ACTION_GEAR_UP) && gear_ < gearCount) {
            gear_++;
            shiftTimer_ = 0.20f;
            justShifted_ = true;
        } else if (input.Pressed(input::ACTION_GEAR_DOWN) && gear_ > 1) {
            // Refuse a downshift that would over-rev the engine.
            if (fwd <= gearbox::TopSpeed(effMaxSpeed, gearCount, gear_ - 1) * 1.12f) {
                gear_--;
                shiftTimer_ = 0.12f;
                justShifted_ = true;
            }
        }
    } else if (shiftTimer_ <= 0.0f && speed_ > -0.3f) {
        float rpmNow = gearbox::Rpm(fwd, effMaxSpeed, gearCount, gear_);
        if (throttle && rpmNow > 0.93f && gear_ < gearCount) {
            gear_++;
            shiftTimer_ = 0.28f;
            justShifted_ = true;
        } else if (gear_ > 1 &&
                   fwd < gearbox::TopSpeed(effMaxSpeed, gearCount, gear_ - 1) * 0.55f) {
            gear_--;
            shiftTimer_ = 0.15f;
            justShifted_ = true;
        }
    }

    const float topNow = gearbox::TopSpeed(effMaxSpeed, gearCount, gear_);
    rpm_ = (speed_ < -0.3f)
               ? Clamp(-speed_ / (effMaxSpeed * 0.25f), 0.0f, 1.0f)
               : gearbox::Rpm(fwd, effMaxSpeed, gearCount, gear_);
    float gearAccel = (shiftTimer_ > 0.0f)
                          ? 0.0f
                          : gearbox::AccelMultiplier(gearCount, gear_, rpm_);
    // Held in too low a gear: the engine fights you down toward its limit.
    if (fwd > topNow * 1.02f) speed_ -= 12.0f * dt;

    // Braking while still moving forward uses full brake power; braking from
    // a stop (or holding brake already at rest) eases into reverse instead,
    // at a fraction of forward accel -- otherwise reverse launched as fast
    // as forward drive, which read as broken.
    // Manual mode gets a dedicated reverse key (brakes down from forward
    // speed exactly like the brake pedal, then continues into reverse once
    // stopped -- same safety behaviour as brake-to-reverse, just its own
    // control instead of overloading the brake key).
    bool reverseKey = manual && input.Down(input::ACTION_REVERSE);
    if (throttle) {
        speed_ += effAccel * gearAccel * powerScale * dt;
    } else if (brake || reverseKey) {
        if (speed_ > 0.05f) speed_ -= spec.brakePower * powerScale * dt;
        else                speed_ -= effAccel * 0.45f * powerScale * dt;
    } else {
        speed_ = ApproachZero(speed_, 5.0f * dt);
    }

    if (handbrake) speed_ = ApproachZero(speed_, spec.brakePower * 1.6f * dt);
    if (inWater_) speed_ = ApproachZero(speed_, 14.0f * dt);

    // Climbing a slope costs speed, descending gives some back.
    Vector3 forwardFlat = Vector3{ sinf(heading_), 0.0f, cosf(heading_) };
    float slope = Vector3DotProduct(groundNormal_, forwardFlat);
    speed_ -= slope * 14.0f * dt;

    // NOS boost: a consumable speed/accel burst rather than a free extra
    // gear. Only usable moving forward, costs fuel steadily while held,
    // regenerates slowly otherwise so it's a thing to manage, not spam.
    // Nitro upgrade level speeds up regen and raises the cap boosting can
    // reach, rather than changing the burn rate -- makes a maxed-out nitro
    // tank feel like "boost more often, boost a little harder", not just a
    // bigger number.
    boosting_ = boostHeld && boost_ > 0.02f && fwd > 0.5f && !inWater_;
    if (boosting_) {
        boost_ = Clamp(boost_ - 0.35f * dt, 0.0f, 1.0f);
        speed_ += effAccel * 0.9f * dt; // on top of normal accel above
    } else {
        boost_ = Clamp(boost_ + 0.12f * nitroRegenMul * dt, 0.0f, 1.0f);
    }
    float boostSpeedCap = boosting_ ? effMaxSpeed * 1.22f * nitroCapMul : effMaxSpeed;

    const float maxReverse = -effAccel * 0.75f;
    speed_ = Clamp(speed_, maxReverse, boostSpeedCap * powerScale);

    float steerInput = 0.0f;
    if (steerLeft) steerInput += 1.0f;
    if (steerRight) steerInput -= 1.0f;

    steerAngle_ = Lerp(steerAngle_, steerInput * 0.55f, SmoothFactor(9.0f, dt));

    float speedFactor = speed_ / effMaxSpeed;
    heading_ += steerInput * effTurnRate * speedFactor * dt;

    // Drift scoring: an arcade-style score for holding the handbrake into a
    // turn at speed, the same conditions that already trigger the tyre
    // squeal in the engine audio. Points accrue with speed and how hard
    // you're turning, finalising into lastDriftScore_ the frame it ends.
    bool nowDrifting = handbrake && fabsf(steerInput) > 0.3f && fwd > 8.0f;
    driftJustEnded_ = drifting_ && !nowDrifting;
    if (driftJustEnded_) {
        lastDriftScore_ = driftScore_;
        driftScore_ = 0.0f;
    } else if (nowDrifting) {
        driftScore_ += fwd * fabsf(steerInput) * 9.0f * dt;
    }
    drifting_ = nowDrifting;

    // Visual slide: the rendered body yaws further into the turn than the
    // path it's actually following, which is what reads as "drifting" --
    // the movement itself stays the simple heading-based model above, only
    // BuildOrientation() ever looks at slipAngle_.
    float targetSlip = drifting_ ? Clamp(steerInput, -1.0f, 1.0f) * 0.40f : 0.0f;
    slipAngle_ = Lerp(slipAngle_, targetSlip, Clamp((drifting_ ? 6.0f : 3.5f) * dt, 0.0f, 1.0f));

    // Tyre marks: laid down behind the rear axle while actually drifting,
    // broken (so the next drift doesn't visually connect to this one) the
    // moment it stops.
    if (drifting_) {
        Vector3 right = Vector3Normalize(Vector3CrossProduct(Vector3{ 0.0f, 1.0f, 0.0f }, forwardFlat));
        Vector3 rearCenter = Vector3Subtract(position_, Vector3Scale(forwardFlat, spec.design.wheelbase * 0.5f));
        Vector3 markL = Vector3Add(rearCenter, Vector3Scale(right, spec.design.trackWidth * 0.5f));
        Vector3 markR = Vector3Subtract(rearCenter, Vector3Scale(right, spec.design.trackWidth * 0.5f));
        // Road surface sits at terrain + SURFACE_LIFT (0.14); marks were
        // previously drawn at +0.03, which put them entirely UNDER the
        // opaque road mesh -- invisible every time, which is why no drift
        // ever showed one. Now sits just above the real surface instead of
        // a separately-guessed number.
        markL.y = terrain::HeightAt(markL.x, markL.z) + road::SURFACE_LIFT + 0.01f;
        markR.y = terrain::HeightAt(markR.x, markR.z) + road::SURFACE_LIFT + 0.01f;
        skidmarks::Add(markL, markR);
    } else if (driftJustEnded_) {
        skidmarks::Break();
    }

    Vector3 nextPos = Vector3Add(position_, Vector3Scale(forwardFlat, speed_ * dt));

    const float limit = terrain::WORLD_SIZE * 0.5f - 10.0f;
    nextPos.x = Clamp(nextPos.x, -limit, limit);
    nextPos.z = Clamp(nextPos.z, -limit, limit);

    // Mountain face wall: the existing slope-vs-speed term above already
    // bleeds speed on any incline, but did nothing to stop the car actually
    // climbing the mountain ring given enough run-up. A genuinely steep
    // face (normal.y below this) is now a hard wall instead -- the move is
    // rejected outright rather than merely slowed.
    Vector3 nextNormal = terrain::NormalAt(nextPos.x, nextPos.z);
    bool tooSteep = (nextNormal.y < 0.55f);
    if (tooSteep) nextPos = position_;

    position_ = nextPos;

    // Fence collision is back (Phase 3) -- but each track now defines
    // deliberate gaps (Track.hpp) where Fence::ResolveCollisions no-ops,
    // so "open world" survives as "open through the gaps" instead of
    // "the fence never did anything."
    const float carRadius = spec.design.width * 0.55f;
    bool blocked = world.ResolveCollisions(position_, carRadius);
    blocked = fences.ResolveCollisions(position_, carRadius) || blocked;
    blocked = tooSteep || blocked;
    if (aiTraffic) blocked = aiTraffic->ResolveCollisions(position_, carRadius) || blocked;

    // Lake wall: a soft push back toward shore once well below the surface,
    // rather than letting the car drive clean across the lake bed. Shallow
    // water (the existing IN WATER drag/slow) still floods right up to the
    // shoreline unimpeded -- this only stops full crossings.
    float waterDepth = terrain::WATER_LEVEL - terrain::HeightAt(position_.x, position_.z);
    if (waterDepth > 1.0f) {
        Vector3 awayFromLake = Vector3Normalize(Vector3{
            position_.x - terrain::LAKE_X, 0.0f, position_.z - terrain::LAKE_Z
        });
        float push = Clamp((waterDepth - 1.0f) * 6.0f * dt, 0.0f, 2.0f);
        position_.x += awayFromLake.x * push;
        position_.z += awayFromLake.z * push;
        blocked = true;
    }

    // Edge-detected, not level-triggered: true only on the frame contact
    // begins, so leaning on a tree plays one thump, not one every frame.
    justHit_ = blocked && !wasBlocked_;
    wasBlocked_ = blocked;

    if (blocked) {
        speed_ = ApproachZero(speed_, 40.0f * dt);
        if (speed_ > 6.0f) speed_ = 6.0f;
        if (speed_ < -6.0f) speed_ = -6.0f;
    }

    position_.y = terrain::HeightAt(position_.x, position_.z);
    groundNormal_ = terrain::NormalAt(position_.x, position_.z);
    bool wasInWater = inWater_;
    inWater_ = (position_.y < terrain::WATER_LEVEL);
    justEnteredWater_ = inWater_ && !wasInWater;

    tripMeters_ += fabsf(speed_) * dt;
    upgrades::AddDistance(carIndex_, fabsf(speed_) * dt);

    // Auto-recovery: holding the throttle against something (a tree, a
    // fence, a cliff face) with almost no speed for a few seconds snaps
    // back to the road, the same way Backspace does manually. Releasing the
    // throttle or actually moving resets the timer, so a deliberately
    // parked car is never touched.
    if (throttle && fabsf(speed_) < 0.4f) {
        stuckTimer_ += dt;
        if (stuckTimer_ > 3.5f) {
            RespawnNearestRoad();
            stuckTimer_ = 0.0f;
        }
    } else {
        stuckTimer_ = 0.0f;
    }

    wheelSpin_ += (speed_ / spec.design.wheelRadius) * dt;
    bodyRoll_ = Lerp(bodyRoll_, -steerInput * speedFactor * 0.08f, SmoothFactor(6.0f, dt));

    skidmarks::Update(dt);
}

// Drives the car exactly the way AiTraffic drives its own cars: position and
// heading are read straight off the centreline every frame, interpolated
// between the two nearest samples, rather than reached by steering a
// physics body toward it. No collision, no fence check, no "running wide"
// -- there is nothing to collide with, because the car is never anywhere
// except on the road.
void Car::UpdateAutoDrive(float dt)
{
    const cardb::CarSpec &spec = cardb::Roster()[carIndex_];
    const auto &samples = road::Centreline();
    int n = (int)samples.size();
    if (n < 2) { autoDrive_ = false; return; }

    if (autoProgress_ < 0.0f) {
        int idx = road::NearestSampleIndex(position_.x, position_.z);
        autoProgress_ = (idx >= 0) ? (float)idx : 0.0f;
    }

    const upgrades::CarUpgrade &upg = upgrades::Get(carIndex_);
    float effMaxSpeed = spec.maxSpeed * upgrades::EngineMultiplier(upg.engine);
    float cruiseSpeed = effMaxSpeed * 0.55f;
    speed_ = Lerp(speed_, cruiseSpeed, SmoothFactor(1.5f, dt));

    // Real distance to the next sample, not a guessed constant -- see
    // AiTraffic::Update for the actual measurement (average ~11.8 units,
    // not the ~2.2 this and AiTraffic both wrongly assumed before, which is
    // why both moved several times faster along the track than their speed
    // value implied).
    {
        int i0now = (int)autoProgress_;
        int i1now = (i0now + 1) % n;
        float segLen = Vector3Distance(samples[i0now].position, samples[i1now].position);
        if (segLen < 0.01f) segLen = 0.01f;
        autoProgress_ += (speed_ * dt) / segLen;
    }
    while (autoProgress_ >= (float)n) autoProgress_ -= (float)n;
    while (autoProgress_ < 0.0f) autoProgress_ += (float)n;

    int i0 = (int)autoProgress_;
    int i1 = (i0 + 1) % n;
    float t = autoProgress_ - (float)i0;

    const road::Sample &a = samples[i0];
    const road::Sample &b = samples[i1];
    Vector3 base = Vector3Lerp(a.position, b.position, t);
    Vector3 fwd = Vector3Normalize(Vector3Lerp(a.forward, b.forward, t));

    position_ = base;
    position_.y = terrain::HeightAt(position_.x, position_.z);
    groundNormal_ = terrain::NormalAt(position_.x, position_.z);
    heading_ = atan2f(fwd.x, fwd.z);
    inWater_ = false;
    justEnteredWater_ = false;

    // Cosmetic follow-through so the HUD/engine note don't just freeze.
    int gearCount = spec.gears;
    rpm_ = gearbox::Rpm(speed_, effMaxSpeed, gearCount, gear_);
    if (rpm_ > 0.85f && gear_ < gearCount) gear_++;
    else if (rpm_ < 0.30f && gear_ > 1) gear_--;

    tripMeters_ += fabsf(speed_) * dt;
    upgrades::AddDistance(carIndex_, fabsf(speed_) * dt);
    wheelSpin_ += (speed_ / spec.design.wheelRadius) * dt;
    bodyRoll_ = Lerp(bodyRoll_, 0.0f, SmoothFactor(6.0f, dt));
    slipAngle_ = Lerp(slipAngle_, 0.0f, SmoothFactor(6.0f, dt));

    justHit_ = false;
    wasBlocked_ = false;
    stuckTimer_ = 0.0f;
    drifting_ = false;
    driftJustEnded_ = false;

    skidmarks::Break(); // never drifting under auto-drive, so no trail to continue
    skidmarks::Update(dt);
}

Matrix Car::BuildOrientation() const
{
    Vector3 up = groundNormal_;
    // slipAngle_ only ever affects how the body is drawn -- see the drift
    // comment in Update() for why movement itself stays on heading_ alone.
    Vector3 forward = Vector3{ sinf(heading_ + slipAngle_), 0.0f, cosf(heading_ + slipAngle_) };
    Vector3 right = Vector3Normalize(Vector3CrossProduct(up, forward));
    forward = Vector3Normalize(Vector3CrossProduct(right, up));

    Matrix m = MatrixIdentity();
    m.m0 = right.x;   m.m1 = right.y;   m.m2 = right.z;
    m.m4 = up.x;      m.m5 = up.y;      m.m6 = up.z;
    m.m8 = forward.x; m.m9 = forward.y; m.m10 = forward.z;
    return m;
}

void Car::DrawGhost(Vector3 position, float heading, Color tint) const
{
    if (!loaded_) return;

    Vector3 up = terrain::NormalAt(position.x, position.z);
    Vector3 forward = Vector3{ sinf(heading), 0.0f, cosf(heading) };
    Vector3 right = Vector3Normalize(Vector3CrossProduct(up, forward));
    forward = Vector3Normalize(Vector3CrossProduct(right, up));

    Matrix orientation = MatrixIdentity();
    orientation.m0 = right.x;   orientation.m1 = right.y;   orientation.m2 = right.z;
    orientation.m4 = up.x;      orientation.m5 = up.y;      orientation.m6 = up.z;
    orientation.m8 = forward.x; orientation.m9 = forward.y; orientation.m10 = forward.z;

    Matrix place = MatrixMultiply(orientation, MatrixTranslate(position.x, position.y, position.z));
    Model body = body_;
    body.transform = MatrixMultiply(MatrixScale(bodyScale_, bodyScale_, bodyScale_), place);
    DrawModel(body, Vector3Zero(), 1.0f, tint);
}

void Car::Draw() const
{
    skidmarks::Draw(); // drawn regardless of car-loaded state so a trail never just vanishes mid-swap

    if (!loaded_) return;

    const cardb::CarSpec &spec = cardb::Roster()[carIndex_];

    // Contact shadow: the car otherwise reads as floating since nothing
    // darkens the ground beneath it. A soft alpha-gradient fan (immediate
    // mode, same technique SkidMarks.cpp already uses) draped onto the
    // ground normal, drawn before the body so it sits underneath.
    {
        const int SEGMENTS = 14;
        const float radius = spec.design.length * 0.42f;
        Vector3 up = groundNormal_;
        Vector3 fwd = Vector3{ sinf(heading_), 0.0f, cosf(heading_) };
        Vector3 right = Vector3Normalize(Vector3CrossProduct(up, fwd));
        fwd = Vector3Normalize(Vector3CrossProduct(right, up));
        Vector3 center = Vector3Add(position_, Vector3Scale(up, 0.03f)); // just above the ground, avoids z-fighting

        rlDisableBackfaceCulling();
        rlBegin(RL_TRIANGLES);
        for (int i = 0; i < SEGMENTS; i++) {
            float a0 = (float)i / SEGMENTS * 2.0f * PI;
            float a1 = (float)(i + 1) / SEGMENTS * 2.0f * PI;
            // Squashed slightly along the car's width so the blob reads as
            // an oval under the body rather than a circle floating free of it.
            Vector3 p0 = Vector3Add(center, Vector3Add(Vector3Scale(fwd, cosf(a0) * radius),
                                                        Vector3Scale(right, sinf(a0) * radius * 0.7f)));
            Vector3 p1 = Vector3Add(center, Vector3Add(Vector3Scale(fwd, cosf(a1) * radius),
                                                        Vector3Scale(right, sinf(a1) * radius * 0.7f)));
            rlColor4ub(10, 10, 12, 90);
            rlVertex3f(center.x, center.y, center.z);
            rlColor4ub(10, 10, 12, 0);
            rlVertex3f(p0.x, p0.y, p0.z);
            rlVertex3f(p1.x, p1.y, p1.z);
        }
        rlEnd();
        rlEnableBackfaceCulling();
    }

    Matrix orientation = BuildOrientation();
    Matrix place = MatrixMultiply(orientation, MatrixTranslate(position_.x, position_.y, position_.z));
    Matrix lean = MatrixRotateZ(bodyRoll_);

    Model body = body_;
    body.transform = MatrixMultiply(MatrixScale(bodyScale_, bodyScale_, bodyScale_), MatrixMultiply(lean, place));
    DrawModel(body, Vector3Zero(), 1.0f, paintTint_);

    // Kit cars have their wheels baked into the body mesh; procedural cars
    // (if ever loaded again) would animate separate wheel models here.
    if (wheel_.meshCount == 0) return;

    const float hx = spec.design.trackWidth * 0.5f;
    const float hz = spec.design.wheelbase * 0.5f;
    const float wy = spec.design.wheelRadius;

    const Vector3 offsets[4] = {
        Vector3{  hx, wy,  hz },
        Vector3{ -hx, wy,  hz },
        Vector3{  hx, wy, -hz },
        Vector3{ -hx, wy, -hz }
    };

    for (int i = 0; i < 4; i++) {
        bool front = (i < 2);
        Matrix spin = MatrixRotateX(wheelSpin_);
        Matrix steer = front ? MatrixRotateY(steerAngle_) : MatrixIdentity();
        Matrix local = MatrixTranslate(offsets[i].x, offsets[i].y, offsets[i].z);

        Model wheel = wheel_;
        wheel.transform = MatrixMultiply(MatrixMultiply(spin, steer), MatrixMultiply(local, place));
        DrawModel(wheel, Vector3Zero(), 1.0f, WHITE);
    }
}

} // namespace car
