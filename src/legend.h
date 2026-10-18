#pragma once
#include <string>
#include <vector>
#include "field.h"

// The legendary Pokemon: one-off boss encounters where they live in the games. Each can be
// fought again until someone catches it (3 balls alone, 6 in co-op taking turns), then it's gone.
enum class LegendUnlock { Badges8, Champion, AfterMewtwo };
struct LegendDef {
    const char* species;
    const char* place;        // where it lives
    ArenaKind arena;
    int minLevel;             // never below its level in the games
    int bonus;                // levels above the challengers' average
    float hpPer;              // boss HP: x(1 + hpPer per creature the challengers bring)
    float catchMul;           // harder to catch than the birds: < 1
    float mapX, mapY;         // on the story map
    LegendUnlock unlock;
    const char* lore;         // why it's here
    const char* cry;          // the line on its intro
};
constexpr int LEGEND_COUNT = 5;
const LegendDef& Legend(int id);   // 0 Articuno, 1 Zapdos, 2 Moltres, 3 Mewtwo, 4 Mew
constexpr int LEGEND_THROWS_SOLO = 3, LEGEND_THROWS_COOP = 6;
