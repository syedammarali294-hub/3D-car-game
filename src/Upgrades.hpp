#pragma once

// Per-car performance upgrades, bought with nothing (free to toggle) but
// persisted per car so the choice sticks between sessions -- the "make the
// engine more upgradable" system. Three independent tracks, 0..3 each:
// Engine (accel + top speed), Handling (turn rate), Nitro (boost regen and
// capacity). Kept as flat multipliers applied at the point of use in
// Car::Update rather than mutating CarDatabase's own numbers, so the base
// roster stays the single source of truth for a car's stock stats.
namespace upgrades {

constexpr int MAX_LEVEL = 3;

struct CarUpgrade {
    int engine = 0;
    int handling = 0;
    int nitro = 0;
    float totalMeters = 0.0f; // persistent odometer -- lifetime distance driven in this car
};

// Indexed by the same car index as cardb::Roster(). Grows automatically if
// the roster is bigger than what was last saved.
CarUpgrade &Get(int carIndex);

// Adds to the car's lifetime odometer. Called every frame from Car::Update
// -- cheap (just a float add), does not itself touch disk. Save() still
// has to be called periodically/at shutdown for it to actually persist.
void AddDistance(int carIndex, float meters);

void Load();
void Save();

float EngineMultiplier(int level);   // accel + top speed
float HandlingMultiplier(int level); // turn rate
float NitroRegenMultiplier(int level); // boost regen speed
float NitroCapMultiplier(int level);   // boost top-speed cap while active

} // namespace upgrades
