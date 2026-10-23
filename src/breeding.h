#pragma once
#include <string>
#include <vector>

// IVs, EVs, gender, nicknames, the Day Care and Eggs - as the main games have them.
struct OwnedMon;
struct Profile;

// The six stats, in the games' order (IVs, EVs and EV yields use it).
enum { ST_HP, ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE };
const char* StatShort(int k);          // "HP", "Atk", "Def", "SpA", "SpD", "Spe"
constexpr int IV_MAX = 31;
constexpr int EV_STAT_MAX = 252;       // per stat
constexpr int EV_TOTAL_MAX = 510;      // in all
constexpr int VITAMIN_EVS = 10;        // a vitamin: +10 EVs (to 252, as in Sword/Shield on)
constexpr int POWER_ITEM_EVS = 8;      // a Power item: +8 EVs in its stat for every Pokemon knocked out

void RollIVs(int iv[6]);
void FixMonStats(OwnedMon& m);          // roll what an old save is missing (IVs, gender); EVs within their caps
int EvTotal(const OwnedMon& m);
int AddEvs(OwnedMon& m, const int add[6]);   // adds what fits under the caps; returns how many it gained
void EvYield(const std::string& species, int out[6]);   // EVs a Pokemon gives when it's knocked out
std::string IvJudge(int iv);            // the Judge's words: "Best", "Fantastic", "Very good", ...
std::string IvVerdict(const OwnedMon& m);   // the Judge's overall line

// Gender: 0 male, 1 female, 2 genderless
int RollGender(const std::string& species);
int GenderRate(const std::string& species);   // -1 genderless, else eighths female (0 always male, 8 always female)
void DrawGender(int gender, float x, float y, float size = 7);   // the blue / pink symbol

std::string MonName(const OwnedMon& m);   // its nickname, or its species
constexpr int NICK_MAX = 12;

// ---- the Day Care
std::vector<std::string> EggGroups(const std::string& species);   // "Undiscovered" can't breed, "Ditto" breeds with anyone
int DaycareCompat(const OwnedMon& a, const OwnedMon& b);   // chance (percent) of an Egg every 256 steps: 0, 20, 50 or 70
std::string DaycareLine(int compat);    // what the old man says about how they get along
bool MakeEgg(const OwnedMon& a, const OwnedMon& b, OwnedMon& egg, const std::string& trainer);
int EggCycles(const std::string& species);
constexpr int EGG_CYCLE_STEPS = 256;    // steps in an egg cycle
constexpr int BATTLE_STEPS = 256;       // a battle away from the walked world counts as this many steps
constexpr int DAYCARE_FEE = 100;        // coins to take a Pokemon back, plus 100 per level it grew
constexpr int MAX_EGGS = 6;             // Eggs you can carry

// Steps walked (or battles fought): the Day Care Pokemon grow, an Egg may turn up, Eggs you carry
// get closer to hatching. Returns the indexes in p.eggs of Eggs that are ready to hatch now.
std::vector<int> DaycareSteps(Profile& p, int steps);
std::string EggStatusLine(const OwnedMon& egg);   // "It will take some time to hatch." ...
bool HatchEgg(Profile& p, int eggIndex, int* monIndex = nullptr);   // into the party (or the Box); its index in p.mons
void DrawEgg(float x, float y, float size, float wobble = 0, int species = -1);
