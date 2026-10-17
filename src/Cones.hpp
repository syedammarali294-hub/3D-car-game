#pragma once

#include "raylib.h"
#include <vector>

// Slalom mode: traffic cones placed alternating left/right along the
// current track's centreline (road::Centreline(), the same source
// Fence.cpp and the minimap already read from), so it works on any track
// -- built-in or a custom .cgmap -- with no track-specific data needed.
// Drawn with raylib's own DrawCylinder(radiusTop=0) -- a real cone shape
// with zero new mesh/shader code, which also means no per-instance
// batching problem: each cone's knocked-over tint is just a draw-call
// argument, not baked mesh data.
namespace cones {

class Cones {
public:
    void Load(); // builds cone placements from the already-built road::Centreline()
    void Unload();
    void Draw() const;

    // Same shape as Fence::ResolveCollisions/World::ResolveCollisions (both
    // const, same reasoning -- a const Cones reference can still register
    // that the car brushed a cone): pushes the car out of an overlapping
    // cone and returns true so the caller can react (a light speed knock,
    // a hit counter) -- never a hard wall, clipping a cone is a mistake to
    // notice, not a crash.
    bool ResolveCollisions(Vector3 &position, float radius) const;

    // Cones knocked since the last Load() -- a per-attempt mistake count
    // for the slalom HUD, reset whenever the mode (re)loads.
    int HitsThisRun() const { return hits_; }
    int TotalCones() const { return (int)cones_.size(); }

private:
    struct Cone {
        Vector3 position;
        mutable bool hit = false; // true once clipped this run -- stays visually knocked over, doesn't re-count
    };

    std::vector<Cone> cones_;
    mutable int hits_ = 0;
    bool loaded_ = false;
};

} // namespace cones
