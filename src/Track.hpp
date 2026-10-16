#pragma once

#include "raylib.h"
#include <vector>

// Track definitions: the circuit is now data (a named set of control
// points), not the single hardcoded array Road.cpp used to own directly.
// Road::BuildCentreline() takes a track's control points and builds the
// same Catmull-Rom centreline it always did -- nothing about how the road
// is built changed, only where the points it builds from come from.
namespace track {

enum TrackId { TRACK_SMALL = 0, TRACK_LONG, TRACK_LONGEST, TRACK_COUNT };

struct TrackDef {
    const char *name;
    const char *description;
    std::vector<Vector2> control; // Catmull-Rom control points, closed loop
};

const TrackDef &Get(TrackId id);

} // namespace track
