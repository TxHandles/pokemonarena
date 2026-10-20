#pragma once
#include <string>
#include <vector>
#include "field.h"

// The legendary Pokemon: one-off boss encounters where they live in the games. Each can be
// fought again until someone catches it (3 balls alone, 6 in co-op taking turns), then it's gone.
enum class LegendUnlock { Badges8, Champion, AfterMewtwo, AfterStage, AfterCatch };   // AfterCatch: once the legendaries named in "after" (comma-separated) are all caught
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
    int region = 0;           // which region's map it lives on (0 Kanto, 1 Johto, 2 Hoenn, 3 Sinnoh, 4 Unova)
    const char* after = "";   // AfterStage: the story stage (its name) that has to be beaten first
    const char* hint = "";    // AfterStage: what the list says while it's locked
};
constexpr int LEGEND_COUNT = 48;
// 0 Articuno, 1 Zapdos, 2 Moltres, 3 Mewtwo, 4 Mew (Kanto); 5 Raikou, 6 Entei, 7 Suicune, 8 Ho-Oh,
// 9 Lugia, 10 Celebi (Johto); 11 Regirock, 12 Regice, 13 Registeel, 14 Latias, 15 Latios, 16 Groudon,
// 17 Kyogre, 18 Rayquaza, 19 Jirachi, 20 Deoxys (Hoenn); 21 Uxie, 22 Mesprit, 23 Azelf, 24 Giratina, 25 Dialga,
// 26 Palkia, 27 Heatran, 28 Regigigas, 29 Cresselia, 30 Darkrai, 31 Shaymin, 32 Arceus, 33 Manaphy, 34 Phione (Sinnoh);
// 35 Cobalion, 36 Terrakion, 37 Virizion, 38 Keldeo, 39 Tornadus, 40 Thundurus, 41 Landorus, 42 Zekrom, 43 Kyurem,
// 44 Victini, 45 Volcarona, 46 Meloetta, 47 Genesect (Unova)
const LegendDef& Legend(int id);
constexpr int LEGEND_THROWS_SOLO = 3, LEGEND_THROWS_COOP = 6;
