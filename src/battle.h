#pragma once
#include <array>
#include <string>
#include <vector>
#include "data.h"
#include "field.h"
#include "items.h"
#include "input.h"
#include "raylib.h"
#include "rng.h"
#include "vfx.h"
#include "abilities.h"
#include "natures.h"

enum class FState { Benched, Entering, Active, Recalling, Fainting, Fainted, Evolving };

// CPU skill levels.
enum class AILevel { Easy = 0, Normal = 1, Hard = 2, Count = 3, TestPlayer = 3 };  // TestPlayer: simulated human for balance tests
const char* AILevelName(AILevel l);

// Online: which input is this computer's player (the HUD shows only their keys). -1 = all local.
extern int gLocalInput;
extern bool gVfxDemo;
extern float gVfxDemoGap;   // effect showcase: nobody moves on their own

struct AIParams {
    float reactMin, reactMax;  // seconds between attacks
    float dodgeChance;         // chance per 0.6 s window that it reacts to incoming attacks
    float speedMul;            // movement speed relative to a player
    float aimError;            // radians of random aim error
    float lead;                // how much it leads moving targets (0..1)
    float cooldownMul;         // multiplies its move cooldowns
    float restChance;          // chance to stand still for a moment after attacking
    float strafe;              // how much it circles you (0..1)
};

struct AIBrain {
    float actT = 1.2f, strafeT = 0, swapThinkT = 2.0f, dodgeT = 0, restT = 0, guardT = 0, targetT = 0;
    bool dodging = false;
    bool strafing = true;
    float strafeSign = 1;
};

// One creature going into a battle.
struct CombatantSpec {
    int species = 0;
    int level = FREE_PLAY_LEVEL;
    int xp = 0;
    std::vector<MoveDef> moves;  // 1-4
    int ownedId = -1;            // index in the profile's collection (-1 = not owned)
    float hpFrac = 1.0f;         // HP carried over from the last battle (0..1)
    std::string item;            // held item name (empty = none)
    int owner = 0;               // which player's collection it belongs to (co-op)
    int abilitySlot = -1;        // 0/1 regular, 2 hidden; -1 = pick one at random (CPU creatures)
    int nature = -1;             // natures.h; -1 = random (CPU creatures)
    float boss = 0;              // > 0: a legendary boss (HP x this, can't be knocked out: it's weakened instead)
    bool shiny = false;          // shiny colours (and a sparkle when it's sent out)
    int vit[4] = {0, 0, 0, 0};   // vitamins taken: +1% each to HP, attacks, defenses, speed
};

// One controller slot in a battle: a player or CPU with its own creatures, one on the field at a time.
// A 1v1 battle has two slots; 2v2 has four (two per team).
struct SideSetup {
    std::vector<CombatantSpec> team;
    bool cpu = false;
    AILevel level = AILevel::Normal;
    bool earnXp = false;
    std::string label;           // shown in the HUD: "P1", "CPU", a trainer name
    int team_ = -1;              // which team (0 or 1); -1 = its index (slots 0 and 1)
    int input = -1;              // human input (0 = player 1, 1 = player 2); -1 = by index
    std::string tag;             // short tag over the creature in 2v2 ("P1", "P2", "CPU")
    bool dummy = false;          // practice dummy: stands still, never faints
    std::string hot[4];          // battle items in the four hotkey slots (Medicine), and how many of each it carries
    int hotN[4] = {0, 0, 0, 0};
    int squad = -1;              // the raid: this player's squad (0 / 1); its slots share one controller and come in in waves
};

struct BattleSetup {
    std::vector<SideSetup> side = std::vector<SideSetup>(2);  // slots; 2 = 1v1, 4 = 2v2
    bool twoPlayer = false;
    bool autoAim = true;
    bool trainerBattle = false;  // more XP
    bool screenShake = true;
    bool damageNumbers = true;
    ArenaKind arena = ArenaKind::Stadium;
    bool catchable = false;      // side 1's creatures retreat when weak instead of fainting (catch chance)
    bool night = false;          // Story Mode time of day (darker arena)
    bool daylight = false;       // Story Mode daytime (a little brighter)
    Weather weather = Weather::None;   // the day's weather (season), lasts the whole battle
    uint8_t venue = 0;           // Venue (field.h): the look of a non-gym battle
    int season = -1;             // Story season (outdoor venues dress for it), -1 none
    bool practice = false;       // practice arena: nobody faints, no XP
    bool raid = false;           // the Daily Raid: your whole party fights at once (small name plates for the allies)
    std::string leader;          // a gym battle: the leader ("Leader Brock"), for their picture and cut-ins (Leaders/)
};

struct StatusState {
    Status major = Status::None;
    float majorT = 0, tickT = 0, paraT = 0, paraStunT = 0;
    int toxicN = 0;
    float confuseT = 0, confuseAngle = 0, confuseSwitchT = 0;
    float flinchT = 0;
    float seedT = 0, seedTickT = 0;
    int seedSide = -1;
};

struct Fighter {
    int species = 0;
    const SpeciesDef* sp = nullptr;
    int level = 50;
    int xp = 0;
    int ownedId = -1;
    std::array<MoveDef, 4> moves;
    int transformFrom = -1;              // Transform: the real species (-1 = not transformed)
    std::array<MoveDef, 4> origMoves;   // Transform: the real moves
    int origN = 0;
    int megaFrom = -1;                   // Mega Evolved: the base species (it stays Mega for the rest of the battle)
    int megaTo = -1;                     // Mega Evolving right now: the Mega form
    int RealSpecies() const { return megaFrom >= 0 ? megaFrom : transformFrom >= 0 ? transformFrom : species; }
    int nMoves = 0;

    int maxHp = 1;
    float boss = 0;                      // legendary boss: HP multiplier (0 = a normal creature)
    bool shiny = false;                  // shiny colours
    float hp = 1;
    float displayHp = 1;

    Vector2 pos{0, 0}, knock{0, 0}, moveVel{0, 0}, aim{1, 0};
    int dir = 3;

    FState state = FState::Benched;
    float stateT = 0;
    float cd[4] = {0, 0, 0, 0};
    int uses[4] = {0, 0, 0, 0};
    float hitstun = 0, flash = 0, animT = 0;
    bool moving = false;

    int pendingMove = -1;
    float windupT = 0;
    Vector2 pendingAim{1, 0};

    bool dashing = false;
    float dashT = 0;
    Vector2 dashVel{0, 0};
    int dashMove = -1;
    bool dashHit = false;

    float buffMul[3] = {1, 1, 1}, buffT[3] = {0, 0, 0};
    float dropMul[3] = {1, 1, 1}, dropT[3] = {0, 0, 0};
    StatusState st;
    float shieldT = 0, rechargeT = 0;

    // Dodge (invulnerable dash, cooldown) and Jump (a hop over water, puddles and obstacles)
    float dodgeT = 0, dodgeCd = 0;
    Vector2 dodgeVel{0, 0};
    float jumpT = 0, jumpDur = 0;
    Vector2 jumpVel{0, 0};
    bool Dodging() const { return dodgeT > 0; }
    bool Jumping() const { return jumpT > 0; }
    float JumpZ() const;    // height above the ground while jumping

    bool burrowed = false;
    bool leaping = false;   // a Leap move: in the air rather than underground
    float burrowT = 0;
    int burrowMove = -1;
    Vector2 burrowTarget{0, 0};

    int volleyMove = -1, volleyLeft = 0;
    float volleyT = 0;

    int evolveTo = -1;

    // Arena effects (recomputed every frame) and catching
    float envSpeed = 1, envAtk = 1, envTickT = 0, portalCd = 0, regenAcc = 0;
    float envDef = 1;   // the arena's defense boost (Norman's Defense room, Steven's meteorites)
    bool dashObsHit = false;
    bool weakened = false;   // dropped low in a catchable battle: dazed, then retreats
    float dazeT = 0;
    bool retired = false;    // retreated (still has HP): can be caught afterwards
    bool inBall = false;     // a Poke Ball thrown at it is shaking (it can't act or be hit meanwhile)
    bool caught = false;     // caught in battle (by the side whose owner is caughtBy)
    int caughtBy = -1;       // the catcher's owner (0 / 1: co-op player)
    std::string caughtBall;

    float counterT = 0, endureT = 0;  // Counter-style shield / Endure active
    int vit[4] = {0, 0, 0, 0};        // vitamins taken (CombatantSpec)
    float critT = 0;                  // Dire Hit / X Accuracy: critical hits more likely while > 0
    int lastHitBy = -1;             // slot that last hurt it (credit for status damage)
    bool lent = false;              // handed to a teammate whose own creatures are all out
    int owner = 0;                  // player whose collection it's from (co-op)
    const ItemDef* item = nullptr;   // held item
    bool focusUsed = false;
    float leftAcc = 0;
    // Ability (abilities.h) and what it's doing this battle
    Ab ab = Ab::None;
    int abSlot = 0;
    int nature = 0;
    float abSpd = 1, abAtk = 1, abDef = 1;  // recomputed every frame
    float abT = 0, sinceAct = 99;           // ability timer; seconds since it last used a move
    bool flashFire = false, snackUsed = false, pinchShown = false;
    Type shiftT = Type::None;   // Color Change / Forecast: the type it has become (None = its own)
    bool illusion = false;      // Illusion: still disguised (its first hit taken is halved)
    bool zen = false;           // Zen Mode: entered (below half HP)
    Type T1() const { return shiftT != Type::None ? shiftT : sp->type1; }
    Type T2() const { return shiftT != Type::None ? Type::None : sp->type2; }
    // Ice underfoot: momentum carries you (slide is the current sliding velocity)
    float safeT = 0, mistT = 0, chantT = 0;   // Safeguard (no status), Mist (no stat drops), Lucky Chant (no crits)
    float followT = 0;                        // Follow Me / Rage Powder: foes aim at it
    bool onIce = false, slid = false;
    Vector2 slide{0, 0};
    // Sweeping beams (the user is rooted while the beam swings), being held by a grapple or throw, pins
    float sweepT = 0, sweepDur = 0, sweepA0 = 0, sweepA1 = 0;
    int sweepMove = -1, sweepMask = 0;
    float heldT = 0, heldZ = 0;   // grabbed: can't act; heldZ = carried height (throws)
    float pinT = 0;               // can't switch out (Mean Look, Anchor Shot, Spirit Shackle)
    // Roll (Rollout, Gyro Ball...): rolling as a ball, ricocheting
    float rollT = 0, rollBoost = 1, rollDist = 0, rollTrailT = 0;
    int rollMove = -1, rollMask = 0, rollBounces = 0;
    Vector2 rollVel{0, 0};
    bool Holds(ItemFx fx) const { return item && item->fx == fx && ab != Ab::Klutz; }

    // Progress during this battle (read by the game afterwards)
    bool sentOut = false;
    float dmgDealt = 0, dmgTaken = 0;        // for the after-battle stats screen
    float hurtT = 99;                        // seconds since a foe last hurt it (CPU targeting)
    bool hitFresh = false;                   // just got hit (the CPU decides whether to turn on the attacker)
    int startLevel = 0, startSpecies = 0, xpGained = 0, kos = 0;
    std::vector<std::string> learnedNow;     // learned into an empty slot during the battle
    std::vector<std::string> pendingLearn;   // wants to learn, needs a slot (decided after battle)
    std::vector<std::string> evolvedNames;   // e.g. "Thistlet -> Thornbloom"

    float Stat(StatId s, bool special = false) const;  // special: Sp. Atk / Sp. Def
    float MoveSpeed() const;
    float CooldownMul() const;
    bool HasUses(int mi) const { return moves[mi].maxUses == 0 || uses[mi] < moves[mi].maxUses; }
    bool CanAct() const;
    void RecalcHp(bool keepDamage);
};

// Everything needed to resolve one hit, copied when the move is used.
struct HitSpec {
    int side = 0, fighter = 0;
    std::string name;
    Type type = Type::Normal;
    int power = 0;
    bool light = false;            // damage-over-time tick: no knockback or interruption
    float atk = 50, lvlMul = 1;
    Type t1 = Type::Normal, t2 = Type::None;
    Status status = Status::None;
    float statusChance = 0;
    bool lowers = false;
    StatId lowerStat = StatId::Atk;
    float lowerChance = 0;
    float drain = 0, recoil = 0, knock = 1;
    bool highCrit = false, hex = false;
    bool heavy = false;            // from a heavy creature (smashes obstacles)
    float dmgMul = 1, superMul = 1, flinchChance = 0;  // held-item bonuses
    bool scope = false, contact = false;
    bool special = false;          // Sp. Atk vs Sp. Def
    bool falseSwipe = false, forceSwap = false, ohko = false, brine = false;
    int fixedDmg = 0, attLevel = 50;
    int patchPower = 0;
    float patchTime = 0, patchRadius = 0;
    uint32_t look = 0;             // how the move looks (vfx.h)
    Ab ab = Ab::None;              // the attacker's ability when it used the move
    int moveIdx = -1;              // the attacker's move slot (Cursed Body)
    float atkStage = 1;            // the attacker's ATK stat changes (Unaware ignores them)
    bool effects = true;           // has added effects (Sheer Force removes them)
};

struct Projectile {
    HitSpec hit;
    Vector2 pos{0, 0}, vel{0, 0};
    float traveled = 0, maxDist = 0, radius = 4, homing = 0;
    bool lob = false, drop = false;
    Vector2 start{0, 0}, target{0, 0};
    float t = 0, dur = 0;
    float z = 7, spin = 0;
    bool alive = true;
    int bounces = 0;               // ricochets left
    bool boomer = false, returning = false;
    int hitMask = 0;               // boomerang: 1 = hit going out, 2 = hit coming back
    bool thrown = false;           // a stone pillar hurled by a heavy creature
    float portalCd = 0;
    // Grapple: a hook on a line that yanks the foe back to its owner
    uint8_t hook = 0;              // 1 pulls, 2 also knocks a jumping foe down (Smack Down)
    float hookPull = 1, hookPin = 0;
    Vector2 hookFrom{0, 0};        // where the line starts (the user's position when thrown)
};

// A line latched between two battlers (Tether moves): drains, binds or pins until it breaks.
struct Tether {
    HitSpec hit;
    int side = 0, target = 0, ufi = 0, tfi = 0;   // slots, and which team member is on each end
    uint8_t kind = 1;                              // 1 drain, 2 bind, 3 pin
    float life = 3, maxLife = 3, range = 140, leash = 100, tickT = 0.3f, tickPower = 0, age = 0;
    bool sleepOnly = false, alive = true;
};

// A battler being moved by someone else's move: yanked in by a grapple, or thrown.
struct Grab {
    HitSpec hit;
    int by = 0, target = 0, tfi = 0;
    uint8_t kind = 0;               // 0 yank (grapple), 1 throw, 2 carried up and dropped (Sky Drop)
    Vector2 from{0, 0}, to{0, 0};
    float t = 0, dur = 0.3f, height = 0;
    float bonus = 1;                // throws: x damage when it slams into something
    int slam = 0;                   // 0 nothing, 1 an obstacle, 2 the arena wall, 3 a hazard
    bool alive = true;
};

struct Hazard {
    HitSpec hit;
    Vector2 pos{0, 0};
    float radius = 20, life = 3, maxLife = 3, tickT = 0;
    bool patch = false;  // small burning spot / puddle left by a projectile
    // Orbit: circles its owner
    bool orbit = false;
    float orbitAngle = 0, orbitDist = 26, orbitSpeed = 5, hitCd = 0;
    // Trap: hidden until the foe steps on it, then springs once
    bool trap = false;
    float armT = 0.4f;
    // Vortex: drags the foe toward its center
    float pull = 0;
};

struct ShockWave {
    HitSpec hit;
    Vector2 origin{0, 0};
    float r = 0, maxR = 90, speed = 240;
    bool done = false;
    int hitMask = 0;   // slots already hit
};

enum class FxKind { Ring, Slash, Beam, Charge, Bolt, Mind, Puff, Cone,
                    Explosion, Strike, Flash, Scorch, Radial, Hit, Shock, Pillar, Crack,
                    Lash,      // a vine / chain / tongue whipping out from pos along dir (len) and snapping back
                    Snap };    // a tether breaking: the line's pieces flying apart at pos
struct Fx {
    FxKind kind = FxKind::Ring;
    Vector2 pos{0, 0}, dir{1, 0};
    float life = 0, maxLife = 0.3f, radius = 10, len = 0;
    Color col = WHITE;
    std::vector<Vector2> pts;
    uint32_t look = 0, seed = 0;   // move look and a random seed for its shape
    float z = 0;
    Type type = Type::Normal;
    uint8_t sub = 0;               // Explosion: Impact palette
};

struct Particle {
    Vector2 pos{0, 0}, vel{0, 0};
    float z = 0, vz = 0, life = 0, maxLife = 0.5f, size = 1;
    Color col = WHITE;
    bool grav = false;
    PK kind = PK::Pixel;
    float rot = 0, spin = 0, grow = 0, drag = 3;
};

struct FloatText {
    Vector2 pos{0, 0};
    float z = 0, life = 0, maxLife = 1.0f;
    std::string text;
    Color col = WHITE;
};

// Dodge and Jump
constexpr float DODGE_COOLDOWN = 6.0f;   // seconds between dodges
constexpr float DODGE_TIME = 0.28f;      // invulnerable dash time
constexpr float DODGE_SPEED = 230.0f;    // world units per second during the dash
constexpr float JUMP_TIME = 0.55f;       // time in the air
constexpr float JUMP_HEIGHT = 22.0f;     // peak height (screen pixels)

struct Intent {
    Vector2 move{0, 0};
    int attack = -1;
    bool swap = false;
    bool swapBack = false;   // swap to the previous creature instead of the next
    int swapTo = -1;         // swap straight to this team slot
    bool dodge = false, jump = false;
    bool mega = false;
    int item = -1;           // use the item in this hotkey slot
    bool hasAim = false;
    Vector2 aim{1, 0};
};

float HitR(const Fighter& f);   // body radius for hits: from the Pokedex size (battle.cpp)

struct Side {
    std::vector<Fighter> team;
    int active = 0;
    int teamId = 0;          // 0 or 1
    int input = -1;          // human input index, -1 = CPU
    int target = -1;         // foe slot this one is aiming at
    std::string tag;
    float damageDealt = 0;   // for co-op catch order
    bool cpu = false;
    bool earnXp = false;
    AILevel level = AILevel::Normal;
    std::string label;
    float swapCd = 0;
    bool megaUsed = false;   // one Mega Evolution a battle for each player
    std::string hot[4];      // item hotkeys: the item, how many are left, the slot's cooldown, how many it started with
    int hotN[4] = {0, 0, 0, 0}, hotStart[4] = {0, 0, 0, 0};
    float hotCd[4] = {0, 0, 0, 0};
    int HotUsed(int k) const { return hotStart[k] - hotN[k]; }
    int squad = -1;          // the raid: whose squad (0 / 1) - the player steers one of its slots, the CPU the rest
    int swapTo = -1;
    float manualT = 0;       // a player picked this target themselves: keep it for a while
    bool dummy = false;      // practice dummy
    float sendT = -1;
    Vector2 lastPos{0, 0};
    AIBrain ai;

    Fighter& Cur() { return team[active]; }
    const Fighter& Cur() const { return team[active]; }
    int AliveCount() const;
    int NextAlive(int from) const;
    int PrevAlive(int from) const;
};

// Wild battles: which species you already own (a Poke Ball mark on the foe's name plate, like the games).
void SetOwnedSpecies(std::vector<char> owned);
bool OwnedSpecies(int species);

struct BallThrow {   // a Poke Ball on its way: flies, swallows the target, shakes, then holds or breaks
    int from = -1, to = -1;
    std::string ball;
    float t = 0;
    int shakes = 0;          // how many times it wobbles (0-3)
    bool ok = false, done = false;
    Vector2 start{0, 0};
};

class Battle {
public:
    bool PressHotItem(int s, int slot) { return UseHotItem(s, slot); }   // (autotests: a hotkey pressed)
    void Start(const BattleSetup& setup);
    void Update(float dt, const PlayerInput in[2]);
    void Draw(bool banners = true) const;

    bool Finished() const;  // result decided, banner shown, nobody mid-evolution
    float LeaderLastStandAt() const { return leaderLastT < 0 ? -1 : time - leaderLastT; }   // seconds since a gym leader got down to one Pokemon (-1 = not yet)
    int Winner() const { return winner; }   // 0 or 1, 2 = draw, -1 = still going
    float AnimTimeScale() const;   // < 1 just after the final knockout (offline slow motion; battle_anim.cpp)
    const Side& GetSide(int s) const { return sides[s]; }
    int NumSlots() const { return (int)sides.size(); }
    std::vector<const Fighter*> TeamFighters(int team) const;   // every creature on a team, all slots
    std::string TeamLabel(int team) const;
    bool IsDoubles() const { return sides.size() > 2; }
    int N() const { return (int)sides.size(); }
    const BattleSetup& Setup() const { return setup; }
    void PracticeCycleDummies();   // practice arena: every dummy turns into the next type
    const Field& GetField() const { return field; }
    bool CanMega(int s) const;     // that slot's Pokemon on the field holds its Mega Stone and the player hasn't Mega Evolved yet
    float MegaFreeze() const { return megaFreezeT; }

private:
    void PracticeNextDummy(int s, bool announce);
    BattleSetup setup;
    std::vector<Side> sides;
    Field field;
    std::vector<Projectile> projs;
    std::vector<Hazard> hazards;
    std::vector<ShockWave> waves;
    std::vector<Fx> fxs;
    std::vector<Particle> parts;
    std::vector<FloatText> texts;
    std::vector<Tether> tethers;
    std::vector<Grab> grabs;
    std::vector<BallThrow> throws;
    float time = 0, introT = 0, shakeT = 0;
    float sinceDamage = 0, lastHpSum = -1, suddenTick = 0;  // stalemate breaker
    bool suddenDeath = false;
    int shakeAmt = 0;
    int winner = -1;
    float overT = 0;
    int envLevel = 50;          // arena hazards hit like a creature of this level
    Weather weather = Weather::None;
    float weatherT = 0, weatherTick = 0;
    MoveDef lastMove[16];       // for Mirror Move / Copycat (by slot; up to 16 slots - a co-op raid has 13)
    bool hasLastMove[16] = {};
    SimRng rng;                 // all of the battle's randomness (see rng.h)
    bool gasOn = false, cloudNine = false;   // Neutralizing Gas / Cloud Nine on the field
    Weather natural = Weather::None;         // the day's weather: comes back when a weather move wears off
    float megaFreezeT = 0;                   // a Mega Evolution: everything else pauses for a moment
    float hype = 0;                          // how loud the gym crowd is (visual only, not saved)
    friend struct BattleIO;     // online: save / load the whole battle (net_state.cpp)
public:
    int payDayCoins = 0;        // coins scattered by Pay Day (Story Mode adds them after a win)
    Weather CurrentWeather() const { return weather; }
    Weather Wx() const { return cloudNine ? Weather::None : weather; }   // weather as it affects battlers (Cloud Nine)
    void DebugCenter();   // gallery: set the gym centerpiece off
    void DebugCast(int s, const char* move);   // effect showcase (autotest builds)
    void DebugHold();
    void DebugWeather(Weather w) { weather = natural = w; weatherT = w == Weather::None ? 0 : 1e6f; }  // tests only
    void DebugPuddle(int s, float r, bool ice, uint32_t seed = 12345u, Vector2 off = {0, 0}) {   // tests only: a puddle (or ice) under slot s
        Puddle pd; pd.seed = seed; pd.pos = {sides[s].Cur().pos.x + off.x, sides[s].Cur().pos.y + off.y}; pd.r = pd.maxR = r; pd.ice = ice ? 30.0f : 0.0f; field.puddles.push_back(pd);
    }
    void DebugMound(Vector2 at, uint32_t seed = 1) {   // tests only
        Obstacle o; o.kind = Obstacle::SnowMound; o.seed = seed; o.pos = at; o.radius = 14; o.height = 20; o.hp = o.maxHp = 150; o.grow = 1; field.obs.push_back(o);
    }
    Vector2 DebugPos(int s) const { return sides[s].Cur().pos; }
private:
    HitSpec EnvSpec(Type t, int power, Status st = Status::None, float chance = 0) const;

    void SendOut(int s, int idx);
    void UpdateSide(int s, float dt, const PlayerInput& in, bool canAct);
    void UpdateStatus(int s, float dt);
    Intent HumanIntent(const PlayerInput& in) const;
    Intent ThinkAI(int s, float dt);
    void TrySwap(int s, bool back = false, int to = -1);
    void TryBeginMove(int s, int mi, Vector2 aim, bool exactAim);
    void Execute(int s, int mi, bool first, const MoveDef* override = nullptr);
    HitSpec Spec(int s, const MoveDef& m) const;
    float ApplyHit(const HitSpec& h, int targetSide, Vector2 dir);
    void TryStatus(int targetSide, Status st, int sourceSide);
    void DirectDamage(int s, float amount, Color c, int creditSide, bool direct = false);  // direct: not blocked by Magic Guard
    // Abilities (battle_ab.cpp)
    Ab AbOf(const Fighter& f) const;          // Ab::None while Neutralizing Gas is out
    bool AbOnField(Ab a, int exceptSide = -1, int onlyFoesOf = -1) const;
    void AbRefresh();
    void AbPopup(int s, const std::string& text);
    void AbOnEnter(int s);
    void AbOnLeave(int s);
    void AbTick(int s, float dt);
    bool AbBlocksHit(const HitSpec& h, int ts);
    float AbDamageMul(const HitSpec& h, int ts, float eff, bool crit) const;
    void AbAfterHit(const HitSpec& h, int ts, float dmg, bool crit, float hpBefore);
    bool AbBlocksStatus(int ts, Status st, int src);
    bool AbBlocksLower(int ts, StatId stat, int src, const std::string& moveName);
    void AbLowered(int ts, int src);
    void AbSpec(HitSpec& h, const Fighter& f, const MoveDef& m) const;
    void AbKo(int killerSide);
    void DoTransform(int s, int ts, bool announce);
    void AbilityMove(int s, int ts, int kind);   // Skill Swap, Entrainment, Simple Beam, Doodle, Psycho Shift
    void InitAbility(Fighter& f, int slot);
    void Faint(int s, int creditSide);
    void Confetti(int n);   // gym crowds celebrate: paper falls over the arena
    void TeamStatusMove(int s, const MoveDef& m);
    void CreditKo(int s, int creditSide);
    void CheckWeakened(int s);
    void Retire(int s);
    void UpdateField(float dt);
    void UpdateWisps(float dt);       // Agatha's hall
    void UpdateStatues(float dt);     // Lance's den
    void UpdateSpotlight(float dt);   // the Champion's stadium
    void UpdateWind(float dt);        // Falkner's gusts
    void UpdateLights(float dt);      // Karen's drifting lights
    void UpdateLegendField(float dt); // legendary lairs: strikes from above, Mewtwo's pulse, Mew's teleports
    void FieldEffectsOn(int s, float dt);
    // Hoenn arenas (battle_hoenn.cpp)
    void UpdateHoenn(float dt);               // eclipses, cracking tiles, rain, surges, meteors
    void HoennEffectsOn(int s, float dt);     // what the Hoenn arenas do to whoever stands where
    float FieldMoveMul(int s, Type t, bool special) const;   // the arena's boost to a move (Twin Moons, the rain, the meteorite)
    Vector2 HoennPull(int s) const;           // CPU: Hoenn arena awareness
    void HoennFieldReact(Vector2 at, const HitSpec& h, float radius);   // Ice refreezes Juan's lake
    void HoennCenterUpdate(float dt);
    void HoennCenterHit(const HitSpec& h, Vector2 dir, bool physical);
    void DrawHoennFloor() const;
    void DrawHoennTop() const;
    void DrawHoennCenterBody(const Obstacle& o) const;
    // Sinnoh and Unova arenas (battle_gen45.cpp)
    void UpdateGen45(float dt);
    void Gen45EffectsOn(int s, float dt);
    float Gen45MoveMul(int s, Type t, bool special) const;
    Vector2 Gen45Pull(int s) const;
    void Gen45FieldReact(Vector2 at, const HitSpec& h, float radius);
    void Gen45CenterUpdate(float dt);
    void Gen45CenterHit(const HitSpec& h, Vector2 dir, bool physical);
    void DrawGen45Floor() const;
    void DrawGen45Top() const;
    void DrawGen45CenterBody(const Obstacle& o) const;
    int LairBoss() const;                     // the legendary boss's slot (-1: an ordinary battle)
    void DropStrikes(Type t, int power, int count, int bossSide, Status st = Status::None, float chance = 0, float radius = 15);
    void ObstacleImpact(Obstacle& o, const HitSpec& h, Vector2 dir, bool physical);
    void ObstaclesInArea(Vector2 c, float r, const HitSpec& h, bool physical, Vector2 fromPos);
    void TryElectrify(Vector2 at, const HitSpec& h);   // a move landed here: the field reacts (FieldReact)
    void FieldReact(Vector2 at, const HitSpec& h, float radius);   // puddles shock / freeze / dry, gym water freezes
    void FieldReactLine(Vector2 a, Vector2 b, const HitSpec& h);   // a beam passing over puddles
    void ShockPuddle(Puddle& pd, const HitSpec& h);
    void UpdateWeatherField(float dt);                              // puddles form in rain, snow mounds in snow
    void MoundGone(Obstacle& o, bool melted);
    bool OnIce(Vector2 p) const;
    void DrawPuddles() const;
    float BeamLength(Vector2 from, Vector2 dir, float range, const HitSpec& h);
    Vector2 FieldPull(int s) const;
    bool Hits(int srcSide, int ts) const { return srcSide < 0 || sides[srcSide].teamId != sides[ts].teamId; }
    bool Foes(int a, int b) const { return a >= 0 && b >= 0 && sides[a].teamId != sides[b].teamId; }
    std::vector<int> FoeSlots(int s) const;
    int AnyFoe(int s) const;              // some foe slot (for credit / positions)
    void PickTarget(int s, float dt = 0);
    int PartnerWithBench(int s) const;    // a teammate slot with a creature waiting on the bench, or -1
    void Borrow(int s);                   // take over that creature when your own are all out
    int NearestFoe(int src, Vector2 p) const;
    void GiveXp(int s, int amount);
    void LevelUp(int s);
    void FinishEvolution(int s);
    void StartMega(int s);
    bool UseHotItem(int s, int slot);
    bool ThrowBall(int s, int slot);   // a wild battle: throw the ball in this hotkey at your target
    void UpdateThrows(float dt);
    void DrawThrows() const;
    int HotSide(int s) const;          // the slot whose item hotkeys this slot uses (a raid squad shares its leader's)
    bool SquadFighting(int squad) const;   // a raid squad still has a Pokemon up on the field
    void RaidControl(PlayerInput in[2]);   // the raid: Swap moves your control between your squad's Pokemon on the field   // false: nothing to use it on (or empty / cooling down)
    bool ItemWouldHelp(const Fighter& f, const ItemDef& d) const;
    void FinishMega(int s);
    bool Targetable(int s) const;
    bool Hittable(int s) const;      // targetable and not mid-dodge (attacks pass through a dodge)
    void StartDodge(int s, Vector2 dir);
    void StartJump(int s, Vector2 dir);
    void CycleTarget(int s, int dir);   // a player switches to the next / previous foe
    void SpawnProjectile(const HitSpec& h, const MoveDef& m, Vector2 pos, Vector2 dir);
    void SpawnPatch(const HitSpec& h, Vector2 pos);
    void UpdateProjectiles(float dt);
    void UpdateHazards(float dt);
    void UpdateWaves(float dt);
    void UpdateEffects(float dt);
    void Burst(Vector2 pos, Color c, int n, float speed, float z = 6);
    // Sweeping beams, tethers, grapples and throws (battle_combat.cpp)
    void StartSweep(int s, int mi, const MoveDef& m, Vector2 aim);
    void UpdateSweep(int s, float dt);
    void StartTether(int s, int ts, const MoveDef& m, const HitSpec& h, Vector2 aim);
    void UpdateTethers(float dt);
    void BreakTether(Tether& t, const char* why);
    void SpawnHook(const HitSpec& h, const MoveDef& m, int s, Vector2 aim);
    void HookHit(const Projectile& p, int ts);
    void StartThrow(int s, int ts, const MoveDef& m, const HitSpec& h, Vector2 aim);
    void UpdateGrabs(float dt);
    bool Held(int s) const;
    void StartRoll(int s, int mi, const MoveDef& m, Vector2 aim);
    void UpdateRoll(int s, float dt, Vector2 steer);
    void EndRoll(int s, bool bonk);
    void DrawRollBall(int s) const;   // visual only: a battler rolling as a ball
    // Gym centerpieces (battle_center.cpp)
    Obstacle* CenterBody();
    void UpdateCenterpiece(float dt);
    void CenterHit(const HitSpec& h, Vector2 dir, bool physical);
    void CenterReact(Vector2 at, const HitSpec& h, float radius);
    void CenterJam();
    Vector2 CenterAvoid(int s) const;
    Vector2 ClampToArenaC(Vector2 p) const;
    void DrawCenterFloor() const;
    void DrawCenterBody(const Obstacle& o) const;
    void DrawCenterTop() const;
    void DrawCombatLinks() const;   // visual only: sweeping beams, tether lines, hook lines, held battlers
    // Visual effects (battle_vfx.cpp). All spawned in the simulation so they stay in step online.
    Particle& Emit(PK k, Vector2 pos, float z, Vector2 vel, float vz, float life, float size, Color c, bool grav = false);
    Fx& AddFx(FxKind k, Vector2 pos, float life, Color c, float radius = 10, float z = 0);
    void HitVfx(const HitSpec& h, Vector2 pos, float z, Vector2 dir);
    void Boom(Vector2 pos, float r, Impact im, float z = 4);
    void CastVfx(int s, const MoveDef& m, const HitSpec& h, Vector2 aim);
    void TrailVfx(const Projectile& p, Vector2 at);
    void LandVfx(const HitSpec& h, Vector2 pos, float radius, bool drop);
    void BeamVfx(const HitSpec& h, Vector2 from, Vector2 to, float width);
    void StatusVfx(Vector2 pos, Status st);
    void HazardVfx(const Hazard& hz);
    void WaveVfx(const ShockWave& w);
    void FaintVfx(Vector2 pos, Type t);
    void DrawFxTop(const Fx& fx) const;
    void DrawParticles() const;
    void DrawWeatherFloor() const;   // wet floor and puddles, snow cover, sun glare
    void DrawTimeOfDay() const;      // night darkens the arena, day brightens it
    void DrawWeatherSky() const;     // rain, snow, sand and sunlight in the air, over everything on the field
    // Battle animation (battle_anim.cpp): visual only, never part of the battle state
    void AnimTick() const;
    void AnimPose(int s, Vector2& off, float& sx, float& sy, float& lift) const;
    float AnimCamera(Vector2& focus) const;
    float AnimGhost(int s, Vector2& back) const;   // lunge after-images: strength, offset
    float AnimHpTrail(int s) const;      // the HP bar's damage trail (fraction)
    float AnimSinceWin() const;          // seconds since the battle was decided
    void DrawAnimUnder() const;
    void DrawAnimOver() const;
    void DrawAnimCinematic() const;
    void DrawAnimBanners() const;   // shiny / legendary appearance banners
    void DrawLeaderCutIn() const;   // a gym leader down to their last Pokemon (visual only)
    mutable float leaderLastT = -1;   // when the leader got down to one Pokemon (drawing only)
    mutable int leaderAlive = -1;
    void AddText(Vector2 pos, const std::string& text, Color c, float z = 24);
    void Shake(int amt, float t = 0.15f);

    void DrawFighter(int s) const;
    void DrawProjectile(const Projectile& p) const;
    void DrawHud() const;
    void DrawObstacle(const Obstacle& o) const;
    void DrawFieldFloor() const;
    void DrawFieldTop() const;
    void DrawRingRopes(bool front) const;
    void DrawDragonStatue(const Turret& tu) const;
};
