#pragma once

#include "raylib.h"
#include <vector>

// AI traffic: a handful of cars that drive themselves around the same
// circuit the player uses, each at a fixed speed and a fixed lateral offset
// from the centreline (a "lane"), looping forever. Only active in Traffic
// mode (picked from the main menu) -- Free Drive never loads this at all.
namespace traffic {

class AiTraffic {
public:
    // count: how many AI cars to spawn. Uses the same Kenney Car Kit bodies
    // and CC0 licence as the player's own cars (see CarDatabase.cpp) --
    // nothing new to credit.
    void Load(Shader shader, int count);
    void Unload();

    void Update(float dt);
    void Draw() const;

    // Pushes the player out of any AI car it's overlapping, the same shape
    // as World::ResolveCollisions / Fence::ResolveCollisions. Returns true
    // if a push happened.
    bool ResolveCollisions(Vector3 &playerPosition, float playerRadius) const;

    int CarCount() const { return (int)cars_.size(); }
    Vector3 CarPosition(int i) const { return cars_[i].position; }
    bool CarBoosting(int i) const { return cars_[i].boostTimeLeft > 0.0f; }

private:
    struct Car {
        Model body{};
        float bodyScale = 1.0f;
        Color paint = WHITE;
        float progress = 0.0f; // index into road::Centreline(), fractional
        float speed = 10.0f;   // world units/s along the centreline, cruising (non-boost) pace
        float laneOffset = 0.0f; // sideways from the centre, in Sample::right units
        float radius = 1.0f;   // collision radius, derived from the body length
        Vector3 position{};
        float heading = 0.0f;

        // Nitro: fires periodically rather than never, same idea as the
        // player's own NOS -- a speed burst, not a permanently higher pace.
        float boostCooldown = 0.0f; // seconds until this car is allowed to fire again
        float boostTimeLeft = 0.0f; // seconds remaining in the current burst, 0 = not boosting
        unsigned int rngState = 1u; // own RNG stream so boost timing isn't synchronised across cars
    };

    std::vector<Car> cars_;
    bool loaded_ = false;
};

} // namespace traffic
