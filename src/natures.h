#pragma once
#include <string>

// The 25 natures, exactly as in the games: one stat x1.1, another x0.9 (5 are neutral).
// Stats: 0 Attack, 1 Defense, 2 Speed, 3 Sp. Atk, 4 Sp. Def (nature number = 5 * raised + lowered).
constexpr int NATURE_COUNT = 25;
enum NatureStat { NS_ATK = 0, NS_DEF = 1, NS_SPE = 2, NS_SPA = 3, NS_SPD = 4 };
const char* NatureName(int nature);
int NatureUp(int nature);      // raised stat, -1 for a neutral nature
int NatureDown(int nature);    // lowered stat, -1 for a neutral nature
float NatureMul(int nature, int stat);
int NatureByName(const std::string& name);   // -1 if unknown
const char* NatureStatName(int stat);         // "Attack", "Sp. Atk"...
