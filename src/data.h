#pragma once
#include <array>
#include <string>
#include <vector>
#include "moves.h"
#include "types.h"

// Max creatures per team.
constexpr int MAX_TEAM_SIZE = 6;
constexpr int MAX_LEVEL = 100;
constexpr int FREE_PLAY_LEVEL = 50;
// Species use real Pokemon base stats (as in the Pokedex). The battle math scales them up by
// this much, so a real base stat of 50 plays like 77 did before the switch.
constexpr float STAT_SCALE = 1.55f;
inline float Scaled(int baseStat) { return baseStat * STAT_SCALE; }  // level used in Quick Battle with the free-play roster

struct LearnEntry {
    int level = 1;
    MoveDef move;
};

// Evolutions that don't happen by level: an item (evolution stone) or high friendship.
struct EvoAlt {
    std::string to;          // species name
    std::string item;        // set = evolves when this item is used on it
    int friendship = 0;      // > 0 = evolves after a win once friendship reaches this
    bool trade = false;      // evolves when traded to the other player (co-op)
};

struct SpeciesDef {
    std::string name;
    Type type1 = Type::Normal;
    Type type2 = Type::None;
    int hp = 50, atk = 50, def = 50, spd = 50;  // real base stats (Pokedex numbers)
    int spAtk = 0, spDef = 0;                   // Sp. Atk / Sp. Def (0 = same as atk / def)
    int SpAtk() const { return spAtk > 0 ? spAtk : atk; }
    int SpDef() const { return spDef > 0 ? spDef : def; }
    bool floats = false;                        // hovers instead of walking
    bool heavy = false;                         // big body: smashes trees, throws stone pillars
    bool winged = false;                        // flies over water/pools without being a Flying type
    int dex = 0;                                // Pokedex number (sort order)
    bool rare = false;
    bool legendary = false;                     // rare encounter only once your team averages level 40
    bool starter = false;                       // offered as a Story Mode starter                          // shows up in rare training encounters
    std::vector<MoveDef> moves;                 // moveset for free play (up to 4)
    std::vector<LearnEntry> learnset;           // moves learned by level
    std::vector<std::string> tmMoves;           // moves it can learn from TMs (pokemon.txt "tm =")
    std::vector<std::pair<std::string, int>> wildAreas;   // "wild = Route 1, 20": extra places it lives (area, rate)
    std::string evolvesTo;                      // species name, empty = doesn't evolve
    int evolveLevel = 0;
    std::vector<EvoAlt> evoAlts;                // stone / friendship evolutions
    int xpYield = 0;                            // 0 = worked out from base stats
    // Pokedex
    std::string ability1, ability2, hiddenAbility;  // ability names (slot 0, 1, hidden)
    std::string category, dexEntry;
    int heightDm = 0, weightHg = 0;             // height in decimetres, weight in hectograms

    // Art: built-in pixel art is looked up by artName; spriteDir (if set) holds image files
    // that override it. size multiplies the on-screen sprite size.
    std::string artName;
    std::string spriteDir;
    float size = 1.0f;
    bool custom = false;   // defined or changed by a creatures/ folder
};

// All playable creatures: the built-ins, then anything added from the creatures/ folder.
const std::vector<SpeciesDef>& AllSpecies();
std::vector<SpeciesDef>& SpeciesRegistry();   // for the loader; don't add after a battle starts
const std::vector<SpeciesDef>& BuiltinSpeciesList();  // the originals, never modified
int SpeciesIndex(const std::string& name);    // -1 if unknown; also matches a renamed slot's original name

// ---- Levels and growth
int XpForLevel(int level);                    // total XP needed to reach a level
float StatMul(int level);                     // stat multiplier at a level (1.0 at level 50)
int XpYield(const SpeciesDef& sp);
int KoXp(const SpeciesDef& defeated, int defeatedLevel, bool trainerBattle);
// The (up to) 4 most recent moves a species knows at a level.
std::vector<MoveDef> MovesAtLevel(const SpeciesDef& sp, int level);
// Moves a species learns exactly at this level.
std::vector<MoveDef> MovesLearnedAt(const SpeciesDef& sp, int level, bool evolving = false);  // evolving: plus its "evo" moves
// Moveset for free play: the species' moves list, or what it knows at FREE_PLAY_LEVEL.
std::vector<MoveDef> FreePlayMoves(const SpeciesDef& sp);
int EvolutionIndex(int speciesIndex);         // -1 if it doesn't evolve (or target missing)
bool IsBaseForm(int speciesIndex);            // nothing evolves into it
bool CanEvolve(int speciesIndex);             // by level, stone or friendship
int BaseFormOf(int speciesIndex);             // first stage of its line
bool InSpeciesPool(int speciesIndex);         // can show up as an opponent / reward / free-play pick
int StoneEvolution(int speciesIndex, const std::string& item);   // target, or -1
int FriendshipEvolution(int speciesIndex, int friendship);       // target, or -1
std::vector<std::string> EvolutionStonesFor(int speciesIndex);   // stones that would evolve it
std::string EvolutionHint(int speciesIndex);
int TradeEvolution(int speciesIndex);                              // target when traded, or -1
// Abilities: slot 0/1 regular, 2 hidden (falls back to slot 0 if the species has none there).
std::string SpeciesAbilityName(int speciesIndex, int slot);
int AbilitySlotCount(int speciesIndex);                            // 1 or 2 regular abilities
void ApplyDexData();   // fills in Pokedex info and abilities for the original 151 (after loading)  // "Evolves into X at level 16", "... with a Fire Stone", ...
constexpr int START_FRIENDSHIP = 70;
constexpr int MAX_FRIENDSHIP = 255;
// Final form reachable at a level, following evolutions (for CPU opponents).
int EvolvedFormAt(int speciesIndex, int level);
// Heavy (explicit flag or size >= 1.15) and airborne (hovers, Flying type or winged).
bool IsHeavy(const SpeciesDef& sp);
bool IsAirborne(const SpeciesDef& sp);
