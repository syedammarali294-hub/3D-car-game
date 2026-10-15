#pragma once

#include "CarBody.hpp"
#include <vector>

namespace cardb {

struct CarSpec {
    const char *name;
    const char *subtitle;   // short flavour line shown in the garage
    carbody::Design design; // still used for stats, wheelbase/track/cabin
                             // placement (camera framing) and tint colour

    // Kenney Car Kit model (CC0, assets/models/car/, already includes its
    // own wheels baked into the mesh -- see TODO.md for what that costs).
    // Scaled at load time to match design.length, so everything else in
    // Design that assumes "the car's own coordinate scale" still lines up.
    const char *kitModel;

    float maxSpeed;
    float acceleration;
    float brakePower;
    float turnRate;

    // Forward gears (reverse is separate): 5 for everyday cars, 7 for sports
    // cars, 8 for hypercars. See Gearbox.hpp.
    int gears;

    // True for the handful of downloaded, licensed, high-poly models
    // (tens of thousands of triangles vs ~2-3k for everything else) --
    // gated behind Settings > Car > Ultra Graphics rather than always
    // loaded, since this project's stated target is low-end hardware.
    // False (the default) for every procedural and Kenney-kit car.
    bool ultraOnly = false;

    // Attribution shown in Credits for an ultraOnly car. Empty for
    // everything else -- only a downloaded asset needs one.
    const char *credit = "";
};

// Three original silhouettes inspired by the shapes of well-known sports
// cars (wide German sedan, long-nose JDM coupe, square-shouldered JDM GT) --
// no imported meshes, no badges, no brand names in the data itself.
const std::vector<CarSpec> &Roster();

// Repaint options offered in the Garage, independent of each car's default
// paint. Index 0 is always "factory" -- that car's own Design::paint, read
// fresh from Roster() rather than duplicated here.
const std::vector<Color> &PaintPalette();
const char *PaintLabel(int index);

} // namespace cardb
