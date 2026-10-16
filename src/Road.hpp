#pragma once

#include "raylib.h"
#include <vector>

namespace road {

// How far above raw terrain::HeightAt() the actual road surface mesh sits
// (see Road.cpp's RoadMesh::Load) -- shared so anything else that needs to
// sit visibly ON the road, like skid marks, uses the real number instead of
// guessing its own and ending up drawn underneath the road surface.
constexpr float SURFACE_LIFT = 0.14f;

struct Sample {
    Vector3 position;  // centreline point, draped onto the terrain
    Vector3 forward;   // unit direction along the road
    Vector3 right;     // unit direction across the road
};

// Builds the circuit centreline from a track's control points (see
// Track.hpp -- a closed Catmull-Rom loop, same as always, just no longer
// hardcoded to one fixed shape). Must run before anything queries the road.
void BuildCentreline(const std::vector<Vector2> &control);

const std::vector<Sample> &Centreline();

// Distance from (x,z) to the centreline, in world units.
float DistanceToRoad(float x, float z);

// Closest centreline sample to (x,z). Used to find which side of the road
// a point is on (via its right vector) for things like fence collision.
const Sample &NearestSample(float x, float z);

// Index into Centreline() of the closest sample, or -1 if the circuit
// hasn't been built yet. Used for lap-progress tracking.
int NearestSampleIndex(float x, float z);

// Where the car should start: on the road, facing along it.
Sample StartPoint();

class RoadMesh {
public:
    void Load(Shader shader);
    void Unload();
    void Draw() const;

private:
    Model surface_ = { 0 };
    Model markings_ = { 0 };
    bool loaded_ = false;
};

} // namespace road
