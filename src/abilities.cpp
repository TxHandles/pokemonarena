#include "abilities.h"
#include "moves.h"

namespace {
const AbilityDef kAbilities[] = {
    {"None", ""},
    {"Adaptability", "Same-type moves get a 2x boost instead of 1.5x."},
    {"Aftermath", "If knocked out by a contact move, the attacker loses 1/4 of its max HP."},
    {"Analytic", "Moves deal 30% more to a foe that attacked within the last second."},
    {"Anger Point", "Taking a critical hit sharply raises its Attack."},
    {"Anticipation", "Shudders on entry if a foe has a super-effective move."},
    {"Arena Trap", "Grounded foes can't switch out while it's on the field."},
    {"Battle Armor", "Hard armor: it never takes critical hits."},
    {"Big Pecks", "Its Defense can't be lowered by foes."},
    {"Blaze", "Fire moves deal 50% more when its HP is 1/3 or less."},
    {"Chlorophyll", "Moves 50% faster in harsh sunlight."},
    {"Clear Body", "Foes can't lower its stats."},
    {"Cloud Nine", "Weather has no effect while it's on the field."},
    {"Competitive", "When a foe lowers its stats, its Attack rises sharply."},
    {"Compound Eyes", "Its shots home in on targets and fly 20% farther."},
    {"Cursed Body", "Contact attackers may have the move they used disabled for a while."},
    {"Cute Charm", "Contact attackers may become confused (charmed)."},
    {"Damp", "Nobody can use Explosion or Self-Destruct while it's on the field."},
    {"Defiant", "When a foe lowers its stats, its Attack rises sharply."},
    {"Download", "On entry, raises its Attack."},
    {"Drought", "Brings harsh sunlight when it enters the battle."},
    {"Dry Skin", "Water heals it and rain restores HP; sun and Fire moves hurt it more."},
    {"Early Bird", "Wakes up from sleep twice as fast."},
    {"Effect Spore", "Contact attackers may be poisoned, paralyzed or put to sleep."},
    {"Filter", "Super-effective hits deal 25% less damage."},
    {"Flame Body", "Contact attackers may be burned."},
    {"Flash Fire", "Absorbs Fire moves, then its own Fire moves deal 50% more."},
    {"Forewarn", "On entry, reveals the foe's strongest move."},
    {"Friend Guard", "Its teammate takes 25% less damage (2v2)."},
    {"Frisk", "On entry, reveals the foe's held item."},
    {"Gluttony", "Snacks once when HP falls below 1/2, restoring 20% HP."},
    {"Guts", "Attack +50% while it has a status condition (burns don't weaken it)."},
    {"Harvest", "When HP falls below 1/2 it eats a berry (+15% HP); may grow another, more often in sun."},
    {"Healer", "May cure its teammate's status condition (2v2)."},
    {"Hustle", "Physical attacks deal 50% more, but sometimes miss."},
    {"Hydration", "Status conditions are washed away in rain."},
    {"Hyper Cutter", "Its Attack can't be lowered by foes."},
    {"Ice Body", "Restores HP in hail and isn't hurt by it."},
    {"Illuminate", "Lights up the arena, revealing hidden traps."},
    {"Immunity", "Can't be poisoned."},
    {"Imposter", "Transforms into the foe as soon as it enters."},
    {"Infiltrator", "Its attacks pass through barriers like Reflect and Light Screen."},
    {"Inner Focus", "Never flinches and isn't affected by Intimidate."},
    {"Insomnia", "Can't fall asleep."},
    {"Intimidate", "Lowers the foes' Attack when it enters the battle."},
    {"Iron Fist", "Punching moves deal 20% more."},
    {"Justified", "Being hit by a Dark move raises its Attack."},
    {"Keen Eye", "Moves like Sand Attack and Smokescreen don't affect it, and it sees through evasion."},
    {"Leaf Guard", "Can't get status conditions in harsh sunlight."},
    {"Levitate", "Floats: Ground moves and ground hazards can't touch it."},
    {"Lightning Rod", "Absorbs Electric moves and its Attack rises."},
    {"Limber", "Can't be paralyzed."},
    {"Liquid Ooze", "Foes that drain HP from it lose HP instead."},
    {"Magic Guard", "Only takes damage from attacks - no status, weather or recoil damage."},
    {"Magnet Pull", "Steel-type foes can't switch out while it's on the field."},
    {"Marvel Scale", "Defense +50% while it has a status condition."},
    {"Mold Breaker", "Its attacks ignore the target's abilities."},
    {"Moxie", "Knocking out a foe raises its Attack."},
    {"Multiscale", "Takes half damage while its HP is full."},
    {"Natural Cure", "Status conditions are cured when it switches out."},
    {"Neutralizing Gas", "Every other ability is switched off while it's on the field."},
    {"No Guard", "Its attacks home in on targets, and attacks on it can't be dodged."},
    {"Oblivious", "Isn't affected by Intimidate or charm."},
    {"Overcoat", "Isn't hurt by weather and isn't affected by powder moves."},
    {"Overgrow", "Grass moves deal 50% more when its HP is 1/3 or less."},
    {"Own Tempo", "Can't be confused, and Intimidate doesn't affect it."},
    {"Pickup", "Picks up coins whenever it knocks out a foe."},
    {"Poison Point", "Contact attackers may be poisoned."},
    {"Poison Touch", "Its contact moves may poison the target."},
    {"Pressure", "Foes' moves take 15% longer to recharge."},
    {"Quick Feet", "Moves 50% faster with a status condition (paralysis doesn't slow it)."},
    {"Rain Dish", "Gradually restores HP in rain."},
    {"Rattled", "Bug, Dark and Ghost hits make it faster."},
    {"Reckless", "Recoil moves deal 20% more."},
    {"Regenerator", "Restores 1/3 of its max HP when it switches out."},
    {"Rivalry", "Deals 25% more to foes that share a type with it."},
    {"Rock Head", "Takes no recoil damage."},
    {"Run Away", "Switches out twice as fast and can always escape traps."},
    {"Sand Force", "Rock, Ground and Steel moves deal 30% more in a sandstorm; immune to sand."},
    {"Sand Rush", "Moves 50% faster in a sandstorm; immune to sand."},
    {"Sand Veil", "May dodge attacks in a sandstorm; immune to sand."},
    {"Scrappy", "Normal and Fighting moves hit Ghost types; ignores Intimidate."},
    {"Serene Grace", "Added effects of its moves happen twice as often."},
    {"Shed Skin", "May shed off status conditions."},
    {"Sheer Force", "Moves with added effects deal 30% more, but lose those effects."},
    {"Shell Armor", "Hard shell: it never takes critical hits."},
    {"Shield Dust", "Blocks the added effects of foes' attacks."},
    {"Skill Link", "Multi-hit moves always strike one extra time."},
    {"Sniper", "Critical hits deal even more damage."},
    {"Snow Cloak", "May dodge attacks in hail; immune to hail."},
    {"Solar Power", "Special moves deal 50% more in sun, but it slowly loses HP."},
    {"Soundproof", "Immune to sound-based moves."},
    {"Static", "Contact attackers may be paralyzed."},
    {"Steadfast", "Flinching makes it faster."},
    {"Stench", "Its damaging hits may make the target flinch."},
    {"Sticky Hold", "Its sticky body slows down contact attackers."},
    {"Sturdy", "Hangs on at 1 HP from a hit taken at full HP; immune to one-hit KO moves."},
    {"Swarm", "Bug moves deal 50% more when its HP is 1/3 or less."},
    {"Swift Swim", "Moves 50% faster in rain."},
    {"Synchronize", "When a foe burns, poisons or paralyzes it, the foe gets the same condition."},
    {"Tangled Feet", "Hard to hit while confused."},
    {"Technician", "Weak moves (power 60 or less) deal 50% more."},
    {"Thick Fat", "Takes half damage from Fire and Ice moves."},
    {"Tinted Lens", "Not-very-effective moves deal double damage."},
    {"Torrent", "Water moves deal 50% more when its HP is 1/3 or less."},
    {"Trace", "Copies the foe's ability when it enters."},
    {"Unaware", "Ignores the foe's stat changes when attacking or being attacked."},
    {"Unburden", "Moves much faster once its held item is used up."},
    {"Unnerve", "Foes are too nervous to snack on berries or heal from Leftovers."},
    {"Vital Spirit", "Can't fall asleep."},
    {"Volt Absorb", "Electric moves restore its HP instead of hurting it."},
    {"Water Absorb", "Water moves restore its HP instead of hurting it."},
    {"Water Veil", "Can't be burned."},
    {"Weak Armor", "Physical hits lower its Defense but make it much faster."},
    {"Wonder Skin", "Status moves often fail against it."},
};
bool HasWord(const std::string& s, const char* w) { return s.find(w) != std::string::npos; }
} // namespace

const AbilityDef& AbilityInfo(Ab a) {
    int i = (int)a;
    if (i < 0 || i >= (int)Ab::Count) i = 0;
    return kAbilities[i];
}
const char* AbilityName(Ab a) { return AbilityInfo(a).name; }

Ab AbilityByName(const std::string& name) {
    std::string k = Squash(name);
    if (k.empty()) return Ab::None;
    for (int i = 1; i < (int)Ab::Count; i++)
        if (Squash(kAbilities[i].name) == k) return (Ab)i;
    return Ab::None;
}

bool IsSoundMoveName(const std::string& n) {
    static const char* w[] = {"Voice", "Growl", "Roar", "Screech", "Sing", "Snore", "Uproar", "Round", "Chatter", "Boomburst", "Bug Buzz",
                              "Metal Sound", "Clanging", "Synchronoise", "Perish Song", "Sparkling Aria", "Overdrive", "Supersonic", "Grass Whistle", "Snarl", "Echoed"};
    for (auto x : w) if (HasWord(n, x)) return true;
    return false;
}
bool IsPunchMoveName(const std::string& n) {
    return HasWord(n, "Punch") || n == "Hammer Arm" || n == "Meteor Mash" || n == "Sky Uppercut" || n == "Rage Fist" || n == "Double Iron Bash" || n == "Plasma Fists";
}
bool IsPowderMoveName(const std::string& n) {
    return HasWord(n, "Powder") || n == "Spore" || n == "Cotton Spore" || n == "Stun Spore" || n == "Magic Powder";
}
bool IsAccuracyMoveName(const std::string& n) {
    return n == "Sand Attack" || n == "Smokescreen" || n == "Flash" || n == "Kinesis" || n == "Mud-Slap" || n == "Octazooka" || n == "Mud Bomb" || n == "Muddy Water" || n == "Night Daze" || n == "Leaf Tornado" || n == "Mirror Shot";
}
float AbilityTypeMul(Ab a, Type t) {
    switch (a) {
        case Ab::Levitate: return t == Type::Ground ? 0.0f : 1.0f;
        case Ab::VoltAbsorb: case Ab::LightningRod: return t == Type::Electric ? 0.0f : 1.0f;
        case Ab::WaterAbsorb: case Ab::DrySkin: return t == Type::Water ? 0.0f : 1.0f;
        case Ab::FlashFire: return t == Type::Fire ? 0.0f : 1.0f;
        default: return 1.0f;
    }
}
