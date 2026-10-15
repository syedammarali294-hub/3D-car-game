#include "Gearbox.hpp"

#include <cmath>

namespace gearbox {

const char *ClassLabel(int gearCount)
{
    if (gearCount >= 8) return "Hypercar";
    if (gearCount >= 7) return "Sports car";
    return "Everyday car";
}

float TopSpeed(float maxSpeed, int gearCount, int gear)
{
    if (gearCount < 1) gearCount = 1;
    if (gear < 1) gear = 1;
    if (gear > gearCount) gear = gearCount;
    return maxSpeed * powf((float)gear / (float)gearCount, 0.8f);
}

float Rpm(float speed, float maxSpeed, int gearCount, int gear)
{
    float top = TopSpeed(maxSpeed, gearCount, gear);
    if (top <= 0.01f) return 0.0f;
    float r = (speed > 0.0f ? speed : 0.0f) / top;
    return r > 1.1f ? 1.1f : r;
}

float AccelMultiplier(int gearCount, int gear, float rpm)
{
    float t = (gearCount > 1) ? (float)(gear - 1) / (float)(gearCount - 1) : 0.0f;
    // Short gear -> tall gear. Scaled well below 1: the raw acceleration
    // values were tuned for a single-gear car that hit top speed in ~2s,
    // which would leave no time to feel (or manually make) a shift.
    float ratio = 0.85f + (0.38f - 0.85f) * t;

    float lug = 0.35f + rpm * 1.3f;                      // weak when barely turning
    if (lug > 1.0f) lug = 1.0f;

    float limiter = 1.0f;                                // redline cut
    if (rpm >= 1.0f)       limiter = 0.0f;
    else if (rpm > 0.92f)  limiter = (1.0f - rpm) / 0.08f;

    return ratio * lug * limiter;
}

} // namespace gearbox
