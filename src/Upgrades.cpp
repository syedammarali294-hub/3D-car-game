#include "Upgrades.hpp"

#include "CarDatabase.hpp"
#include "Config.hpp"
#include "raylib.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace upgrades {

namespace {
std::vector<CarUpgrade> g_upgrades;

void EnsureSize(int carIndex)
{
    if (g_upgrades.empty()) {
        g_upgrades.resize(cardb::Roster().size());
    }
    if (carIndex >= (int)g_upgrades.size()) {
        g_upgrades.resize(carIndex + 1);
    }
}
} // namespace

CarUpgrade &Get(int carIndex)
{
    EnsureSize(carIndex);
    return g_upgrades[carIndex];
}

void AddDistance(int carIndex, float meters)
{
    EnsureSize(carIndex);
    g_upgrades[carIndex].totalMeters += meters;
}

bool ReportSpeed(int carIndex, float speedMs)
{
    EnsureSize(carIndex);
    if (speedMs > g_upgrades[carIndex].bestTopSpeed + 0.01f) {
        g_upgrades[carIndex].bestTopSpeed = speedMs;
        return true;
    }
    return false;
}

void Load()
{
    g_upgrades.assign(cardb::Roster().size(), CarUpgrade{});
    if (!FileExists(config::UPGRADES_FILE)) return;

    char *text = LoadFileText(config::UPGRADES_FILE);
    if (text == nullptr) return;

    // One line per car: "carIndex engine handling nitro totalMeters
    // bestTopSpeed". Both trailing fields were added after earlier ones
    // already shipped, so an older file won't have them -- fall back
    // field-by-field rather than losing an earlier save's data.
    const char *p = text;
    while (*p) {
        int idx, e, h, n;
        float meters = 0.0f, topSpeed = 0.0f;
        int fields = sscanf(p, "%d %d %d %d %f %f", &idx, &e, &h, &n, &meters, &topSpeed);
        if (fields >= 4) {
            EnsureSize(idx);
            if (idx >= 0 && idx < (int)g_upgrades.size()) {
                g_upgrades[idx].engine = e < 0 ? 0 : (e > MAX_LEVEL ? MAX_LEVEL : e);
                g_upgrades[idx].handling = h < 0 ? 0 : (h > MAX_LEVEL ? MAX_LEVEL : h);
                g_upgrades[idx].nitro = n < 0 ? 0 : (n > MAX_LEVEL ? MAX_LEVEL : n);
                g_upgrades[idx].totalMeters = (fields >= 5) ? meters : 0.0f;
                g_upgrades[idx].bestTopSpeed = (fields >= 6) ? topSpeed : 0.0f;
            }
        }
        const char *nl = strchr(p, '\n');
        if (!nl) break;
        p = nl + 1;
    }
    UnloadFileText(text);
}

void Save()
{
    std::string buffer;
    char line[112];
    for (int i = 0; i < (int)g_upgrades.size(); i++) {
        const CarUpgrade &u = g_upgrades[i];
        // Only persist a car that's actually been upgraded or driven --
        // an untouched entry doesn't need a line of its own.
        if (u.engine == 0 && u.handling == 0 && u.nitro == 0 && u.totalMeters < 1.0f && u.bestTopSpeed < 0.1f) continue;
        snprintf(line, sizeof(line), "%d %d %d %d %.1f %.2f\n", i, u.engine, u.handling, u.nitro, u.totalMeters, u.bestTopSpeed);
        buffer += line;
    }
    SaveFileText(config::UPGRADES_FILE, (char *)buffer.c_str());
}

float EngineMultiplier(int level)   { return 1.0f + 0.10f * (float)level; }
float HandlingMultiplier(int level) { return 1.0f + 0.12f * (float)level; }
float NitroRegenMultiplier(int level) { return 1.0f + 0.50f * (float)level; }
float NitroCapMultiplier(int level)   { return 1.0f + 0.05f * (float)level; }

} // namespace upgrades
