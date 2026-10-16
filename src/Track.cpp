#include "Track.hpp"

namespace track {

namespace {

// Small: a tight loop clear of the lake (centred at config::LAKE_X/Z =
// 140,-80) by placing it on the opposite side of the map entirely rather
// than just scaling the Long track down -- a naive scale-down brings the
// control points much closer to the lake's fixed position and cuts right
// through it.
const TrackDef SMALL = {
    "Small", "Tight loop, good for quick laps",
    {
        { -110.000f,  150.000f },
        { -158.075f,  227.135f },
        { -223.953f,  297.724f },
        { -315.000f,  262.583f },
        { -381.558f,  197.883f },
        { -362.763f,  108.958f },
        { -325.000f,   20.096f },
        { -227.421f,   21.981f },
        { -142.733f,   59.979f },
    }
};

// Long: the original circuit, unchanged -- same control points Road.cpp
// used to hardcode directly.
const TrackDef LONG = {
    "Long", "The original circuit",
    {
        { -336.0f, -282.0f },
        {  -48.0f, -360.0f },
        {  258.0f, -366.0f },
        {  402.0f, -180.0f },
        {  366.0f,  102.0f },
        {  180.0f,  312.0f },
        { -108.0f,  360.0f },
        { -342.0f,  228.0f },
        { -414.0f,  -36.0f },
    }
};

// Longest: a big sweeping loop using the open-world expansion (WORLD_SIZE
// 4000, mountain ring starting at 1400) -- farthest point ~1050, so it
// still closes well inside the mountains with margin.
const TrackDef LONGEST = {
    "Longest", "A big sweep through the open world",
    {
        {  950.000f,    0.000f },
        {  651.144f,  546.332f },
        {  182.368f, 1034.070f },
        { -475.000f,  822.724f },
        { -798.734f,  290.704f },
        { -986.676f, -359.117f },
        { -475.000f, -822.724f },
        {  147.620f, -837.003f },
        {  804.503f, -675.070f },
    }
};

} // namespace

const TrackDef &Get(TrackId id)
{
    switch (id) {
        case TRACK_SMALL:   return SMALL;
        case TRACK_LONGEST: return LONGEST;
        case TRACK_LONG:
        default:             return LONG;
    }
}

} // namespace track
