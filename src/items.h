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
    Medicine,       // a Bag item used in battle from an item hotkey: heals `value` of max HP and cures `cure` (Potions, Full Heal, berries)
    MegaStone,      // held: lets its Pokemon Mega Evolve in battle (`move` = the Pokemon it works for); sold after a Champion
    PartyUse,       // used on a Pokemon from Party & Box: Rare Candy, Revive (`value` = HP back), vitamins (`boost` = the stat)
    Field,          // used from the Bag out in the world: Repels (`value` = steps), the Escape Rope
    Valuable,       // only worth selling (from the Bag): half its price
    Everstone,      // held: it doesn't evolve
    PowerItem,      // held: +8 EVs in its stat (`boost`) for every Pokemon it helps knock out; it moves 10% slower
    Breeding,       // held at the Day Care: the Destiny Knot (5 IVs passed down)
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
    int hmNo = 0;     // an HM (1 Cut .. 7 Waterfall): a TM that is never used up, given in the story, needed on the map
    int cure = 0;     // Medicine: the conditions it cures (CURE_ bits)
    int boost = -1;   // Medicine (battle items): BOOST_ below. PartyUse vitamins: the stat (0 HP .. 5 Spe, +10 EVs); 10..15: an EV-lowering berry. Power items: the stat
    static constexpr int BOOST_ATK = 0, BOOST_DEF = 1, BOOST_SPD = 2, BOOST_CRIT = 3, BOOST_GUARD = 4, BOOST_COOLDOWN = 5;
    static constexpr int CURE_POISON = 1, CURE_PARALYZE = 2, CURE_SLEEP = 4, CURE_BURN = 8, CURE_FREEZE = 16, CURE_CONFUSE = 32, CURE_ALL = 63;
    static constexpr int AFTER_CHAMPION = 50;   // badges value: sold once you've beaten a Champion (Mega Stones)
};

const std::vector<ItemDef>& ItemList();
const ItemDef* FindItem(const std::string& name);   // nullptr if unknown/empty
bool IsBall(const ItemDef& d);
bool IsStone(const ItemDef& d);
bool IsHoldable(const ItemDef& d);   // not a ball, stone or TM
bool IsTM(const ItemDef& d);
bool IsMegaStone(const ItemDef& d);
bool IsHM(const ItemDef& d);
const char* HmMove(int hmNo);                // "Cut" .. "Waterfall"
int HmBadges(int hmNo);                       // FireRed: the badge count that lets you use it on the map
bool CanLearnHM(const std::string& species, const std::string& move);   // hm_compat.inc
bool IsMedicine(const ItemDef& d);   // used in battle from an item hotkey
bool IsPartyUse(const ItemDef& d);   // used on a Pokemon from Party & Box (Rare Candy, Revive, vitamins)
int SellPrice(const ItemDef& d);     // what a shop pays (0 = it won't buy it)
constexpr int VITAMIN_MAX = 10;      // vitamins per stat (each +1%)
constexpr float ITEM_COOLDOWN = 30.0f;   // seconds before an item slot can be used again
