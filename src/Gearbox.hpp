#pragma once

// Pure gear maths, shared by the car physics and the settings screen.
// Gears are numbered 1..gearCount going forward; reverse is a single separate
// gear handled by Car (brake from a standstill), so it isn't counted here.
namespace gearbox {

// Everyday cars get 5 forward gears (+ reverse); sports cars 7; hypercars 8.
const char *ClassLabel(int gearCount);

// Highest speed (world units/s, same scale as CarSpec::maxSpeed) the car can
// pull in `gear` before it hits the rev limiter. The last gear tops out at
// exactly maxSpeed, so a full-length run still ends at the car's rated top
// speed. Gaps shrink toward the top, like real ratios.
float TopSpeed(float maxSpeed, int gearCount, int gear);

// 0..1+ engine speed in `gear` at the given forward speed.
float Rpm(float speed, float maxSpeed, int gearCount, int gear);

// Multiplier on CarSpec::acceleration: short gears pull hard, tall gears
// less; lugging at very low revs is weak and the limiter cuts power at the
// redline.
float AccelMultiplier(int gearCount, int gear, float rpm);

} // namespace gearbox
