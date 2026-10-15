#pragma once

#include "raylib.h"

namespace carbody {

// A car silhouette described by numbers rather than an imported model file.
// Everything is in metres, with +Z forward, +Y up and the origin on the ground
// directly under the centre of the car.
struct Design {
    float length;        // nose to tail
    float width;         // widest point of the lower body
    float rideHeight;    // floor height above the ground
    float beltHeight;    // top of the lower body (where the windows start)
    float noseTaper;     // width multiplier at the nose (1 = no taper)
    float rearTaper;     // width multiplier at the tail
    float flare;         // extra half-width bulged over each axle
    float noseDrop;      // how far the bonnet falls away toward the nose
    float tailDrop;      // how far the boot falls away toward the tail

    float cabinRear;     // z where the greenhouse starts
    float cabinFront;    // z where the greenhouse ends
    float roofHeight;    // roof height above the belt line
    float rakeRear;      // backlight rake, as a z inset
    float rakeFront;     // windscreen rake, as a z inset
    float cabinWidth;    // greenhouse width as a fraction of the body

    float wheelbase;     // axle to axle
    float trackWidth;    // centre to centre across an axle
    float wheelRadius;
    float wheelWidth;

    float wingHeight;    // 0 for no rear wing
    float wingChord;

    Color paint;
    Color accent;        // wheels, wing, trim
    Color glass;
};

// Builds the body shell (no wheels). Caller owns the mesh.
Mesh BuildBody(const Design &d);

// Builds one wheel, centred on the origin with its axle along X.
Mesh BuildWheel(const Design &d);

} // namespace carbody
