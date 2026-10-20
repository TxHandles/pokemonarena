#pragma once
#include <string>
#include <vector>
#include "types.h"

// Shop items: Poke Balls and held items (each creature holds one).
enum class ItemFx {
    None,
    Ball,           // value = catch-rate multiplier (0 = always catches)
    Stone,          // evolution stone: used on a creature from Party & Box
    TypeBoost,      // moves of `type` deal value x damage
    AtkMul,         // ATK x value
    DefMul,         // DEF x value
    DefIfEvolves,   // DEF x value while it can still evolve (Eviolite)
    SpeedMul,       // movement x value
    CooldownMul,    // cooldowns x value
    ChoiceScarf,    // movement x value, cooldowns x1.15
    ChoiceBand,     // ATK x value, cooldowns x1.15
    LifeOrb,        // damage x value, loses 3% HP per attack
    ExpertBelt,     // super-effective hits x value
    ScopeLens,      // more critical hits
    KingsRock,      // value chance to make the target flinch
    ShellBell,      // heals value of the damage it deals
    Leftovers,      // heals value of max HP per second in battle
    RockyHelmet,    // contact attackers take value of their max HP
    FocusBand,      // survives one knockout hit at 1 HP per battle
    LuckyEgg,       // XP x value
    AmuletCoin,     // battle coins x value
    TM,             // Technical Machine: teaches `move` (used up), from Party & Box
    KeyItem,        // story items (fossils): not sold, not held
    ConfuseGuard,   // confusion wears off much sooner (Yellow Flute)
    ExpAll,         // a Bag item: co-op, your partner's party gets value x the XP of your solo wild battles
};

struct ItemDef {
    std::string name;
    std::string desc;
    ItemFx fx = ItemFx::None;
    Type type = Type::None;
    float value = 1;
    int price = 100;
    int badges = 0;   // gym badges needed before the shop sells it
    std::string move; // TM: the move it teaches
    int tmNo = 0;     // TM: its number (Scarlet/Violet numbering)
};

const std::vector<ItemDef>& ItemList();
const ItemDef* FindItem(const std::string& name);   // nullptr if unknown/empty
bool IsBall(const ItemDef& d);
bool IsStone(const ItemDef& d);
bool IsHoldable(const ItemDef& d);   // not a ball, stone or TM
bool IsTM(const ItemDef& d);
