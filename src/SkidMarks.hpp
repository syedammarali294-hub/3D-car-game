#pragma once

#include "raylib.h"

// Tyre skid marks left on the road/ground while drifting. A flat trail of
// fading quads built from consecutive left/right wheel-ground positions,
// drawn with immediate-mode 3D triangles rather than a rebuilt GPU mesh --
// the list changes every frame while drifting, so there is no stable mesh
// to upload once and reuse the way the rest of this project's geometry does.
namespace skidmarks {

// Appends a new cross-section (the two rear wheels' ground contact points
// this frame). Connects to the previous Add() into a quad, unless Break()
// was called since, which starts a new, unconnected strip.
void Add(Vector3 left, Vector3 right);

// Ends the current strip without drawing a connecting quad to whatever
// comes next -- call when the drift stops or the car respawns.
void Break();

void Update(float dt);
void Draw();

} // namespace skidmarks
