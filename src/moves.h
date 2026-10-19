#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "types.h"

// How a move behaves in the arena.
enum class MoveKind {
    Projectile, // single shot in the aim direction (shown as "Shot")
    Spread,     // three shots in a fan
    Melee,      // short-range swipe in front of the user
    Dash,       // user lunges forward, hitting the first enemy touched
    Burst,      // ring around the user after a short wind-up
    Beam,       // charged instant line
    Lob,        // arcing shot that explodes where it lands
    Zone,       // lingering hazard placed near the target
    Buff,       // raises one of the user's stats for a while
    Heal,       // restores a fraction of max HP
    Chain,      // instant strike that leaps to the target if it's in range (lightning, psychic)
    Wave,       // shockwave ring that spreads out along the ground from the user
    Rain,       // several objects fall around the target (rocks, hail, meteors)
    Blink,      // teleport next to the target and strike (or just teleport if power is 0)
    Shield,     // blocks all damage for a moment
    Burrow,     // vanish underground / into the sky, travel to the target, then erupt
    Boomerang,  // flies out and comes back, hitting on the way out and on the way back
    Orbit,      // orbs circle the user for a while, hitting anything they touch
    Wall,       // raises a short barrier in front of the user that blocks shots
    Trap,       // hidden trap placed on the ground; goes off when the foe steps on it
    Vortex,     // swirling area that drags the foe toward its center while hurting it
    Cone,       // breath / spray filling a cone in front of the user
    Leap,       // jumps high (can't be hit), lands on the target with a shockwave
    Weather,    // changes the weather for a while (Rain Dance, Sunny Day, Sandstorm, Hail)
    Sweep,      // a beam that swings across an arc in front of the user (jump, dodge or take cover)
    Tether,     // latches a line onto the foe: drains, binds or pins it until it breaks free
    Grapple,    // a vine / tongue / chain shot that yanks the foe in when it connects
    Throw,      // grabs a foe in reach and hurls it (extra damage into walls, rocks and hazards)
    Count
};

enum class StatId { Atk = 0, Def = 1, Spd = 2 };

// Status conditions. Burn..Sleep are "major" (only one at a time); the rest stack on top.
enum class Status { None, Burn, Poison, Toxic, Paralyze, Freeze, Sleep, Confuse, Flinch, Seed, Count };
const char* StatusName(Status s);
const char* StatusShort(Status s);  // 3-letter tag for the HUD
bool IsMajorStatus(Status s);

enum class MoveCat { Auto, Physical, Special };
enum class Weather { None = 0, Rain, Sun, Sand, Snow };
// Power changes worked out when the move is used.
enum class PowerMod { None, Facade, Reversal, Eruption, Brine, Acrobatics, WeatherBall };

struct MoveDef {
    std::string name;
    Type type = Type::Normal;
    MoveKind kind = MoveKind::Projectile;
    int power = 0;          // 0 for status moves
    float cooldown = 1.0f;  // seconds
    float range = 0;        // travel distance / reach / beam length / dash distance / target distance
    float radius = 0;       // hit radius / blast radius / beam half-width / wave size / scatter radius
    float speed = 0;        // projectile speed or wave spread speed
    float windup = 0;       // delay before the move fires
    float duration = 0;     // zone lifetime / buff duration / shield or burrow time
    StatId stat = StatId::Atk;
    float amount = 0;       // buff multiplier or heal fraction
    int maxUses = 0;        // uses per battle (0 = unlimited), like PP

    // Extra effects
    Status status = Status::None;  // condition given to the target
    float statusChance = 0;        // 0..1
    bool lowers = false;           // lowers a target stat...
    StatId lowerStat = StatId::Atk;
    float lowerChance = 0;         // ...with this chance
    bool selfDrop = false;         // lowers the user's own stat afterwards
    StatId selfStat = StatId::Def;
    float drain = 0;               // heals the user by this share of damage dealt
    float recoil = 0;              // user takes this share of damage dealt
    float knock = 1;               // knockback multiplier
    float homing = 0;              // projectile turn rate (radians/second)
    int hits = 1;                  // shots / strikes in a quick volley
    bool highCrit = false;
    bool hex = false;              // double power against a target with a status condition
    bool quick = false;            // extra-fast dash
    float recharge = 0;            // user can't act for this long afterwards
    int count = 1;                 // Rain: number of falling objects
    float blast = 14;              // Rain: blast radius of each object
    int patchPower = 0;            // leaves a damaging patch where it hits (e.g. Ember)
    float patchTime = 0, patchRadius = 0;
    int bounces = 0;               // Shot: ricochets off walls and obstacles this many times
    Weather weather = Weather::None;  // Weather kind: the weather it brings
    int stat2 = -1;                // Buff: a second stat raised too (StatId)
    int fixedDmg = 0;              // >0 fixed damage (Dragon Rage), -1 = by level (Seismic Toss), -2 = halves HP (Super Fang)
    PowerMod powerMod = PowerMod::None;
    bool forceSwap = false;        // Roar / Whirlwind / Dragon Tail: the foe is sent back for another
    bool ohko = false;             // Fissure / Guillotine / Horn Drill / Sheer Cold: 30% one-hit KO
    bool selfKO = false;           // Explosion / Self-Destruct: the user faints
    bool counter = false;          // Shield: blocked hits are paid back x1.5 (Counter, Mirror Coat)
    bool endure = false;           // Shield: doesn't block, but hangs on at 1 HP (Endure)
    bool falseSwipe = false;       // never knocks out: leaves at least 1 HP (great for catching)
    bool haze = false;             // clears every stat change on the field
    bool cure = false;             // Heal: also cures the user's status (Heal Bell, Refresh)
    bool metronome = false;        // uses a random move
    bool copycat = false;          // uses the foe's last move (Mirror Move, Copycat)
    bool transform = false;        // becomes a copy of the target (Transform)
    bool payDay = false;           // scatters coins (Story Mode)
    bool original = false;         // made up for this game rather than from the Pokemon games
    uint8_t abFx = 0;              // ability moves: 1 Skill Swap, 2 Entrainment, 3 Simple Beam, 4 Doodle, 5 Psycho Shift
    uint8_t scope = 0;             // who a status move affects (as in the games): 0 the user, 1 the user and its allies,
                                   // 2 its allies only (fails with no ally), 3 every Grass type on the field, 4 draws attacks (Follow Me)
    MoveCat cat = MoveCat::Auto;   // Auto: close-range kinds are physical, ranged kinds special
    // Sweep / Tether / Grapple / Throw
    float arc = 0;                 // Sweep: how far the beam swings (radians)
    uint8_t tether = 0;            // Tether: 1 drains, 2 binds (a leash, small damage), 3 pins (no damage, no switching)
    bool sleepOnly = false;        // Tether: only latches onto a sleeping foe (Dream Eater)
    bool antiAir = false;          // Grapple: knocks a jumping foe out of the air (Smack Down)
    float pin = 0;                 // Grapple: the foe can't switch out for this long (Anchor Shot, Spirit Shackle)
    uint8_t throwKind = 0;         // Throw: 0 hurl, 1 carry up and drop (Sky Drop)
};

const std::vector<MoveDef>& MoveLibrary();
// Look up a move by name (case, spaces and punctuation ignored). nullptr if unknown.
const MoveDef* FindMove(const std::string& name);
// Build a move from scratch (creature files). Kind decides range/speed/etc. defaults.
MoveDef MakeMove(const std::string& name, Type t, MoveKind k, int power, float cooldown);

const char* MoveKindName(MoveKind k);
bool ParseMoveKind(const std::string& s, MoveKind& out);
bool ParseStatus(const std::string& s, Status& out);
bool ParseType(const std::string& s, Type& out);
bool IsDamaging(const MoveDef& m);
// Special moves use Sp. Atk vs the target's Sp. Def; physical ones use Attack vs Defense.
bool IsSpecial(const MoveDef& m);
std::string DescribeMove(const MoveDef& m);  // one-line plain-English summary
std::string Squash(const std::string& s);    // lower-case, no spaces/punctuation (for name matching)
