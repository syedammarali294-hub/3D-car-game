#include "CarDatabase.hpp"

namespace cardb {

namespace {

using carbody::Design;

// "Stormfront GT" -- wide-body German sports sedan silhouette.
Design MakeSedanSport()
{
    Design d{};
    d.length = 4.65f; d.width = 1.88f;
    d.rideHeight = 0.26f; d.beltHeight = 0.86f;
    d.noseTaper = 0.74f; d.rearTaper = 0.80f; d.flare = 0.07f;
    d.noseDrop = 0.22f; d.tailDrop = 0.10f;
    d.cabinRear = -0.45f; d.cabinFront = 0.95f;
    d.roofHeight = 0.42f; d.rakeRear = 0.55f; d.rakeFront = 0.42f; d.cabinWidth = 0.80f;
    d.wheelbase = 2.75f; d.trackWidth = 1.58f;
    d.wheelRadius = 0.345f; d.wheelWidth = 0.26f;
    d.wingHeight = 0.0f; d.wingChord = 0.0f;
    d.paint = Color{ 24, 58, 112, 255 };
    d.accent = Color{ 40, 40, 44, 255 };
    d.glass = Color{ 18, 22, 28, 235 };
    return d;
}

// "Nightrunner RZ" -- long-nose, short-tail JDM coupe silhouette.
Design MakeCoupeCruiser()
{
    Design d{};
    d.length = 4.52f; d.width = 1.81f;
    d.rideHeight = 0.24f; d.beltHeight = 0.82f;
    d.noseTaper = 0.60f; d.rearTaper = 0.86f; d.flare = 0.09f;
    d.noseDrop = 0.30f; d.tailDrop = 0.14f;
    d.cabinRear = -0.70f; d.cabinFront = 0.55f;
    d.roofHeight = 0.38f; d.rakeRear = 0.50f; d.rakeFront = 0.48f; d.cabinWidth = 0.78f;
    d.wheelbase = 2.60f; d.trackWidth = 1.55f;
    d.wheelRadius = 0.335f; d.wheelWidth = 0.27f;
    d.wingHeight = 0.30f; d.wingChord = 0.34f;
    d.paint = Color{ 184, 22, 30, 255 };
    d.accent = Color{ 28, 28, 30, 255 };
    d.glass = Color{ 16, 20, 26, 235 };
    return d;
}

// "Duneblade X" -- square-shouldered JDM GT silhouette.
Design MakeGtTurbo()
{
    Design d{};
    d.length = 4.58f; d.width = 1.84f;
    d.rideHeight = 0.25f; d.beltHeight = 0.84f;
    d.noseTaper = 0.68f; d.rearTaper = 0.90f; d.flare = 0.11f;
    d.noseDrop = 0.20f; d.tailDrop = 0.06f;
    d.cabinRear = -0.60f; d.cabinFront = 0.80f;
    d.roofHeight = 0.40f; d.rakeRear = 0.46f; d.rakeFront = 0.40f; d.cabinWidth = 0.82f;
    d.wheelbase = 2.65f; d.trackWidth = 1.60f;
    d.wheelRadius = 0.350f; d.wheelWidth = 0.28f;
    d.wingHeight = 0.34f; d.wingChord = 0.40f;
    d.paint = Color{ 232, 232, 236, 255 };
    d.accent = Color{ 180, 20, 40, 255 };
    d.glass = Color{ 20, 24, 30, 235 };
    return d;
}


// "Lonewolf SV" -- compact hot-hatch silhouette, short overhangs.
Design MakeHotHatch()
{
    Design d{};
    d.length = 4.10f; d.width = 1.79f;
    d.rideHeight = 0.22f; d.beltHeight = 0.80f;
    d.noseTaper = 0.70f; d.rearTaper = 0.94f; d.flare = 0.10f;
    d.noseDrop = 0.16f; d.tailDrop = 0.04f;
    d.cabinRear = -0.55f; d.cabinFront = 0.75f;
    d.roofHeight = 0.46f; d.rakeRear = 0.40f; d.rakeFront = 0.36f; d.cabinWidth = 0.88f;
    d.wheelbase = 2.48f; d.trackWidth = 1.56f;
    d.wheelRadius = 0.330f; d.wheelWidth = 0.25f;
    d.wingHeight = 0.22f; d.wingChord = 0.26f;
    d.paint = Color{ 240, 150, 20, 255 };
    d.accent = Color{ 32, 32, 34, 255 };
    d.glass = Color{ 18, 22, 28, 235 };
    return d;
}

// "Embercoupe GTX" -- long-hood American muscle coupe silhouette.
Design MakeMuscleCoupe()
{
    Design d{};
    d.length = 4.86f; d.width = 1.92f;
    d.rideHeight = 0.24f; d.beltHeight = 0.86f;
    d.noseTaper = 0.86f; d.rearTaper = 0.78f; d.flare = 0.13f;
    d.noseDrop = 0.12f; d.tailDrop = 0.08f;
    d.cabinRear = -0.85f; d.cabinFront = 0.35f;
    d.roofHeight = 0.36f; d.rakeRear = 0.58f; d.rakeFront = 0.44f; d.cabinWidth = 0.76f;
    d.wheelbase = 2.85f; d.trackWidth = 1.64f;
    d.wheelRadius = 0.360f; d.wheelWidth = 0.30f;
    d.wingHeight = 0.0f; d.wingChord = 0.0f;
    d.paint = Color{ 18, 90, 48, 255 };
    d.accent = Color{ 24, 24, 26, 255 };
    d.glass = Color{ 16, 20, 26, 235 };
    return d;
}

// "Vortexa Hyper" -- low, wide mid-engine hypercar silhouette.
Design MakeHypercar()
{
    Design d{};
    d.length = 4.48f; d.width = 1.98f;
    d.rideHeight = 0.16f; d.beltHeight = 0.70f;
    d.noseTaper = 0.46f; d.rearTaper = 0.70f; d.flare = 0.16f;
    d.noseDrop = 0.26f; d.tailDrop = 0.02f;
    d.cabinRear = -0.30f; d.cabinFront = 0.60f;
    d.roofHeight = 0.30f; d.rakeRear = 0.60f; d.rakeFront = 0.52f; d.cabinWidth = 0.74f;
    d.wheelbase = 2.70f; d.trackWidth = 1.68f;
    d.wheelRadius = 0.355f; d.wheelWidth = 0.32f;
    d.wingHeight = 0.42f; d.wingChord = 0.46f;
    d.paint = Color{ 220, 30, 60, 255 };
    d.accent = Color{ 16, 16, 18, 255 };
    d.glass = Color{ 14, 18, 24, 235 };
    return d;
}

// "Glacier Line" -- restrained, minimalist executive sedan silhouette.
Design MakeExecutiveSedan()
{
    Design d{};
    d.length = 4.78f; d.width = 1.86f;
    d.rideHeight = 0.27f; d.beltHeight = 0.90f;
    d.noseTaper = 0.78f; d.rearTaper = 0.84f; d.flare = 0.04f;
    d.noseDrop = 0.18f; d.tailDrop = 0.12f;
    d.cabinRear = -0.40f; d.cabinFront = 1.05f;
    d.roofHeight = 0.44f; d.rakeRear = 0.60f; d.rakeFront = 0.44f; d.cabinWidth = 0.82f;
    d.wheelbase = 2.82f; d.trackWidth = 1.56f;
    d.wheelRadius = 0.340f; d.wheelWidth = 0.24f;
    d.wingHeight = 0.0f; d.wingChord = 0.0f;
    d.paint = Color{ 214, 218, 222, 255 };
    d.accent = Color{ 50, 54, 60, 255 };
    d.glass = Color{ 20, 24, 30, 235 };
    return d;
}

// "Ridgeback RS" -- rally-flared all-weather coupe silhouette.
Design MakeRallyCoupe()
{
    Design d{};
    d.length = 4.34f; d.width = 1.83f;
    d.rideHeight = 0.30f; d.beltHeight = 0.84f;
    d.noseTaper = 0.66f; d.rearTaper = 0.88f; d.flare = 0.17f;
    d.noseDrop = 0.18f; d.tailDrop = 0.10f;
    d.cabinRear = -0.50f; d.cabinFront = 0.65f;
    d.roofHeight = 0.42f; d.rakeRear = 0.44f; d.rakeFront = 0.40f; d.cabinWidth = 0.86f;
    d.wheelbase = 2.56f; d.trackWidth = 1.58f;
    d.wheelRadius = 0.345f; d.wheelWidth = 0.27f;
    d.wingHeight = 0.26f; d.wingChord = 0.30f;
    d.paint = Color{ 32, 70, 190, 255 };
    d.accent = Color{ 236, 236, 240, 255 };
    d.glass = Color{ 18, 22, 28, 235 };
    return d;
}

// "Solstice Roadster" -- small, light two-seat roadster silhouette.
Design MakeRoadster()
{
    Design d{};
    d.length = 3.96f; d.width = 1.74f;
    d.rideHeight = 0.20f; d.beltHeight = 0.76f;
    d.noseTaper = 0.52f; d.rearTaper = 0.82f; d.flare = 0.08f;
    d.noseDrop = 0.22f; d.tailDrop = 0.06f;
    d.cabinRear = -0.40f; d.cabinFront = 0.35f;
    d.roofHeight = 0.30f; d.rakeRear = 0.34f; d.rakeFront = 0.38f; d.cabinWidth = 0.80f;
    d.wheelbase = 2.35f; d.trackWidth = 1.50f;
    d.wheelRadius = 0.320f; d.wheelWidth = 0.24f;
    d.wingHeight = 0.0f; d.wingChord = 0.0f;
    d.paint = Color{ 250, 210, 30, 255 };
    d.accent = Color{ 30, 30, 32, 255 };
    d.glass = Color{ 16, 20, 26, 235 };
    return d;
}

// "Obsidian Phantom" -- long, sleek grand tourer silhouette.
Design MakeGrandTourer()
{
    Design d{};
    d.length = 4.94f; d.width = 1.90f;
    d.rideHeight = 0.23f; d.beltHeight = 0.84f;
    d.noseTaper = 0.64f; d.rearTaper = 0.76f; d.flare = 0.09f;
    d.noseDrop = 0.26f; d.tailDrop = 0.14f;
    d.cabinRear = -0.75f; d.cabinFront = 0.50f;
    d.roofHeight = 0.38f; d.rakeRear = 0.62f; d.rakeFront = 0.50f; d.cabinWidth = 0.78f;
    d.wheelbase = 2.90f; d.trackWidth = 1.60f;
    d.wheelRadius = 0.350f; d.wheelWidth = 0.28f;
    d.wingHeight = 0.0f; d.wingChord = 0.0f;
    d.paint = Color{ 14, 14, 16, 255 };
    d.accent = Color{ 150, 120, 60, 255 };
    d.glass = Color{ 12, 16, 22, 235 };
    return d;
}

// "Meridian Apex" -- top-speed-focused hypercar silhouette, long and low
// with a big fixed wing. Built to actually reach ~200 km/h (55.6 m/s).
Design MakeTopSpeedHyper()
{
    Design d{};
    d.length = 4.62f; d.width = 1.96f;
    d.rideHeight = 0.15f; d.beltHeight = 0.68f;
    d.noseTaper = 0.42f; d.rearTaper = 0.68f; d.flare = 0.15f;
    d.noseDrop = 0.28f; d.tailDrop = 0.02f;
    d.cabinRear = -0.28f; d.cabinFront = 0.58f;
    d.roofHeight = 0.28f; d.rakeRear = 0.62f; d.rakeFront = 0.54f; d.cabinWidth = 0.72f;
    d.wheelbase = 2.72f; d.trackWidth = 1.70f;
    d.wheelRadius = 0.360f; d.wheelWidth = 0.33f;
    d.wingHeight = 0.46f; d.wingChord = 0.50f;
    d.paint = Color{ 250, 230, 20, 255 };
    d.accent = Color{ 18, 18, 20, 255 };
    d.glass = Color{ 12, 16, 22, 235 };
    return d;
}

// "Northgate Pioneer" -- tall, tough all-terrain utility silhouette.
// Slower and less agile than the rest of the roster on purpose -- a car
// meant to shrug off rough ground rather than win a lap.
Design MakeUtility()
{
    Design d{};
    d.length = 4.70f; d.width = 1.92f;
    d.rideHeight = 0.34f; d.beltHeight = 0.96f;
    d.noseTaper = 0.86f; d.rearTaper = 0.92f; d.flare = 0.06f;
    d.noseDrop = 0.08f; d.tailDrop = 0.02f;
    d.cabinRear = -0.55f; d.cabinFront = 0.90f;
    d.roofHeight = 0.48f; d.rakeRear = 0.30f; d.rakeFront = 0.30f; d.cabinWidth = 0.86f;
    d.wheelbase = 2.78f; d.trackWidth = 1.62f;
    d.wheelRadius = 0.380f; d.wheelWidth = 0.30f;
    d.wingHeight = 0.0f; d.wingChord = 0.0f;
    d.paint = Color{ 70, 86, 60, 255 };
    d.accent = Color{ 40, 40, 44, 255 };
    d.glass = Color{ 20, 24, 30, 235 };
    return d;
}

// "Comet Streak" -- tiny, nimble sprint racer silhouette. The lightest and
// most agile car in the roster, trading top speed for how quickly it turns.
Design MakeSprintRacer()
{
    Design d{};
    d.length = 3.78f; d.width = 1.70f;
    d.rideHeight = 0.17f; d.beltHeight = 0.74f;
    d.noseTaper = 0.44f; d.rearTaper = 0.74f; d.flare = 0.12f;
    d.noseDrop = 0.20f; d.tailDrop = 0.04f;
    d.cabinRear = -0.35f; d.cabinFront = 0.40f;
    d.roofHeight = 0.34f; d.rakeRear = 0.36f; d.rakeFront = 0.36f; d.cabinWidth = 0.76f;
    d.wheelbase = 2.30f; d.trackWidth = 1.52f;
    d.wheelRadius = 0.315f; d.wheelWidth = 0.24f;
    d.wingHeight = 0.24f; d.wingChord = 0.26f;
    d.paint = Color{ 30, 190, 210, 255 };
    d.accent = Color{ 20, 20, 22, 255 };
    d.glass = Color{ 16, 20, 26, 235 };
    return d;
}

} // namespace

const std::vector<CarSpec> &Roster()
{
    static const std::vector<CarSpec> roster = {
        { "Stormfront GT", "Wide-body sports sedan", MakeSedanSport(), "assets/models/car/sedan-sports.obj",     48.0f, 15.0f, 26.0f, 2.0f, 5 },
        { "Nightrunner RZ", "Long-nose turbo coupe", MakeCoupeCruiser(), "assets/models/car/hatchback-sports.obj",   36.0f, 18.0f, 30.0f, 2.4f, 7 },
        { "Duneblade X", "Square-shoulder JDM GT", MakeGtTurbo(), "assets/models/car/race.obj",        40.0f, 17.0f, 28.0f, 2.3f, 7 },
        { "Lonewolf SV", "Compact hot hatch", MakeHotHatch(), "assets/models/car/hatchback-sports.obj",       33.0f, 19.0f, 29.0f, 2.6f, 5 },
        { "Embercoupe GTX", "Long-hood muscle coupe", MakeMuscleCoupe(), "assets/models/car/sedan-sports.obj",    39.0f, 16.0f, 24.0f, 1.8f, 7 },
        { "Vortexa Hyper", "Low, wide mid-engine hyper", MakeHypercar(), "assets/models/car/race-future.obj",       52.0f, 21.0f, 32.0f, 2.2f, 8 },
        { "Glacier Line", "Restrained executive sedan", MakeExecutiveSedan(), "assets/models/car/sedan-sports.obj", 41.0f, 14.0f, 25.0f, 1.9f, 5 },
        { "Ridgeback RS", "Rally-flared all-weather", MakeRallyCoupe(), "assets/models/car/suv.obj",     37.0f, 18.0f, 29.0f, 2.5f, 5 },
        { "Solstice Roadster", "Light two-seat roadster", MakeRoadster(), "assets/models/car/race.obj",       38.0f, 18.5f, 27.0f, 2.3f, 5 },
        { "Obsidian Phantom", "Long, sleek grand tourer", MakeGrandTourer(), "assets/models/car/race-future.obj",    44.0f, 15.5f, 25.0f, 1.9f, 7 },
        { "Meridian Apex", "Top-speed hypercar, ~200 km/h", MakeTopSpeedHyper(), "assets/models/car/race-future.obj", 55.6f, 19.0f, 30.0f, 2.1f, 8 },
        { "Northgate Pioneer", "Tough all-terrain utility", MakeUtility(), "assets/models/car/suv.obj", 30.0f, 11.0f, 22.0f, 1.6f, 5 },
        { "Comet Streak", "Tiny, nimble sprint racer", MakeSprintRacer(), "assets/models/car/hatchback-sports.obj", 35.0f, 20.0f, 28.0f, 2.9f, 6 },

        // Downloaded, licensed models -- real meshes, not procedural. Each
        // decimated from a 300k-650k triangle Sketchfab "hero asset" down to
        // something game-sized (see TODO.md); gated behind Ultra Graphics
        // in Settings > Car since even decimated, they're still 5-25x the
        // triangle cost of every other car here. Design fields below are
        // stats only (wheelbase/track/wheel radius for drift marks and
        // camera, paint as the DrawModel tint) -- the mesh itself is real,
        // not generated from them.
        { "BMW M3 E30 (Modified)", "Licensed model - CC BY 4.0",
          carbody::Design{ 4.36f, 1.78f, 0,0,0,0,0,0,0, 0,0,0,0,0,0, 2.56f, 1.48f, 0.335f, 0.28f, 0,0,
                  WHITE, Color{30,30,34,255}, Color{20,24,30,200} },
          "assets/models/car/bmw_m3_e30.obj", 52.0f, 19.0f, 30.0f, 2.2f, 7,
          true, "\"BMW Modified Racing Car | BMW M3 E30\" (https://skfb.ly/pFIqo) by DevPoly3D, CC BY 4.0." },
        { "BMW M3 Competition", "Licensed model - CC BY-NC-SA 4.0",
          carbody::Design{ 4.99f, 2.15f, 0,0,0,0,0,0,0, 0,0,0,0,0,0, 2.85f, 1.65f, 0.37f, 0.30f, 0,0,
                  WHITE, Color{30,30,34,255}, Color{20,24,30,200} },
          "assets/models/car/bmw_m3_competition.obj", 58.0f, 20.0f, 32.0f, 2.0f, 8,
          true, "\"BMW M3 Competition\" (https://skfb.ly/pMrWu) by VTX, CC BY-NC-SA 4.0 -- NonCommercial." },
        { "BMW S1000RR", "Licensed model (bike) - CC BY-NC-SA 4.0",
          carbody::Design{ 2.03f, 0.78f, 0,0,0,0,0,0,0, 0,0,0,0,0,0, 1.44f, 0.70f, 0.30f, 0.17f, 0,0,
                  Color{40,120,210,255}, Color{20,20,22,255}, Color{16,20,26,200} },
          "assets/models/car/bmw_s1000rr.obj", 62.0f, 24.0f, 34.0f, 3.2f, 6,
          true, "\"BMW S1000 RR\" (https://skfb.ly/pMyBS) by VTX, CC BY-NC-SA 4.0 -- NonCommercial. "
                "Drives on the same 4-wheel-car physics as everything else here -- no real motorcycle "
                "lean/balance model, it just looks like a bike." },
    };
    return roster;
}

const std::vector<Color> &PaintPalette()
{
    // Index 0 ("Factory") is a placeholder -- callers substitute that car's
    // own Design::paint for it rather than using this entry's value.
    static const std::vector<Color> palette = {
        Color{ 0, 0, 0, 0 },         // Factory (placeholder)
        Color{ 196, 28, 36, 255 },   // Racing Red
        Color{ 24, 58, 112, 255 },   // Deep Blue
        Color{ 232, 232, 236, 255 }, // Pearl White
        Color{ 18, 18, 20, 255 },    // Jet Black
        Color{ 236, 200, 20, 255 },  // Solar Yellow
        Color{ 18, 90, 48, 255 },    // British Racing Green
        Color{ 150, 150, 158, 255 }, // Gunmetal
        Color{ 240, 120, 20, 255 },  // Blaze Orange
    };
    return palette;
}

const char *PaintLabel(int index)
{
    static const char *labels[] = {
        "Factory", "Racing Red", "Deep Blue", "Pearl White", "Jet Black",
        "Solar Yellow", "BRG", "Gunmetal", "Blaze Orange",
    };
    if (index < 0 || index >= (int)(sizeof(labels) / sizeof(labels[0]))) return "?";
    return labels[index];
}

} // namespace cardb
