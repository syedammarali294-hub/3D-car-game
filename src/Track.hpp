#pragma once

#include "raylib.h"
#include <vector>

// Track definitions: the circuit is now data (a named set of control
// points), not the single hardcoded array Road.cpp used to own directly.
// Road::BuildCentreline() takes a track's control points and builds the
// same Catmull-Rom centreline it always did -- nothing about how the road
// is built changed, only where the points it builds from come from.
namespace track {

// TRACK_CUSTOM holds whatever .cgmap was most recently loaded via
// mapfile::Load() + SetCustom() below -- not one of the 3 fixed built-in
// tracks. TRACK_COUNT intentionally does NOT include it: it isn't
// something to randomly land on while cycling the built-in picker, only
// something explicitly chosen via the Custom Map list.
enum TrackId { TRACK_SMALL = 0, TRACK_LONG, TRACK_LONGEST, TRACK_COUNT, TRACK_CUSTOM = TRACK_COUNT };

struct TrackDef {
    const char *name;
    const char *description;
    std::vector<Vector2> control; // Catmull-Rom control points, closed loop

    // Deliberate fence openings: each pair is (startFraction, endFraction)
    // along the lap, 0..1. Fence::Load skips building posts/rails inside a
    // gap, and Fence::ResolveCollisions skips blocking there too -- an
    // intentional exit onto the open world, not a hole in otherwise-solid
    // geometry.
    std::vector<Vector2> fenceGaps;
};

const TrackDef &Get(TrackId id);

// Replaces the TRACK_CUSTOM slot's contents -- called by mapfile::Load()
// after reading a .cgmap file. Get(TRACK_CUSTOM) returns this until the
// next SetCustom() call; starts out as an empty (no control points) track,
// which Road::BuildCentreline() already handles safely (does nothing).
void SetCustom(const TrackDef &def);

} // namespace track
