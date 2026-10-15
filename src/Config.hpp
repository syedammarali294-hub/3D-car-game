#pragma once

// Central tuning values. Keep gameplay/visual constants here rather than
// scattered through the systems so they can be balanced in one place.
namespace config {

// Mirrors the project root's VERSION file -- kept here too since that file
// isn't bundled into the build, so the running game can still show its own
// version without reading a file that won't exist next to the .exe.
constexpr const char *GAME_VERSION = "1.5.0";

constexpr int SCREEN_WIDTH = 1280;
constexpr int SCREEN_HEIGHT = 720;

constexpr float WORLD_SIZE = 1600.0f;
constexpr float WATER_LEVEL = 0.0f;

// The lake sits inside the circuit, clear of both the spawn pad and the road.
constexpr float LAKE_X = 140.0f;
constexpr float LAKE_Z = -80.0f;
constexpr float LAKE_RADIUS = 85.0f;

// Ring of mountains that closes the world off well outside the circuit
// (the circuit's farthest control point sits at radius ~450).
constexpr float MOUNTAIN_START = 560.0f;
constexpr float MOUNTAIN_FULL = 740.0f;
constexpr float MOUNTAIN_HEIGHT = 110.0f;

constexpr float ROAD_WIDTH = 13.0f;
constexpr float ROAD_SHOULDER = 11.0f;

constexpr float DRAW_DISTANCE = 520.0f;
constexpr float FOG_DENSITY = 0.0022f;

constexpr const char *SETTINGS_FILE = "settings.cfg";
constexpr const char *OPTIONS_FILE = "options.cfg";
constexpr const char *UPGRADES_FILE = "upgrades.cfg";

} // namespace config
