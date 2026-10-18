#include "rng.h"
#include "battle.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "arena.h"
#include "audio.h"
#include "sprites.h"
#include "ui.h"
#include "vec.h"

namespace {

constexpr float BODY_R = 9.0f;       // collision radius of a creature
// How big a creature's body is for hits and bumping: its Pokedex size (the same scale its picture
// is drawn at), so Onix is a big target and Pikachu a small one.
constexpr float HP_SCALE = 3.0f;     // stretches HP so fights last a while in real time
constexpr float PI_F = 3.14159265f;


float Frand(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }
bool Chance(float p) { return p > 0 && Frand(0, 1) < p; }
float EaseOutF(float t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); return 1 - (1 - t) * (1 - t) * (1 - t); }
Vector2 RandDirB() { float a = Frand(0, 6.2831853f); return {std::cos(a), std::sin(a)}; }

int DirFromVec(Vector2 v) {
    if (std::fabs(v.x) >= std::fabs(v.y) * 0.8f) return v.x < 0 ? VIEW_LEFT : VIEW_RIGHT;
    return v.y < 0 ? VIEW_BACK : VIEW_FRONT;
}

Vector2 ClampToArena(Vector2 p, float margin) {
    p.x = std::max(margin, std::min(ARENA_W - margin, p.x));
    p.y = std::max(margin, std::min(ARENA_H - margin * 0.6f, p.y));
    return p;
}

bool IsStatusMove(const MoveDef& m) {
    return m.power <= 0 && m.kind != MoveKind::Buff && m.kind != MoveKind::Heal && m.kind != MoveKind::Shield &&
           m.kind != MoveKind::Blink;
}

bool InRange(const MoveDef& m, float d) {
    switch (m.kind) {
        case MoveKind::Projectile:
        case MoveKind::Spread: return d < m.range * 0.85f;
        case MoveKind::Melee:  return d < m.range + m.radius * 0.5f + 4;
        case MoveKind::Dash:   return d < m.range + 6 && d > 16;
        case MoveKind::Burst:  return d < m.radius + 4;
        case MoveKind::Beam:   return d < m.range * 0.95f;
        case MoveKind::Lob:    return d < m.range + 20 && d > 30;
        case MoveKind::Zone:   return d < m.range + m.radius * 0.6f;
        case MoveKind::Chain:  return d < m.range;
        case MoveKind::Wave:   return d < m.radius * 0.9f;
        case MoveKind::Rain:   return d < m.range + 20;
        case MoveKind::Blink:  return m.power > 0 && d < m.range + 20;
        case MoveKind::Burrow: return d < m.range + 20 && d > 20;
        case MoveKind::Boomerang: return d < m.range * 0.95f;
        case MoveKind::Orbit:  return d < 75;
        case MoveKind::Trap:   return d > 40 && d < m.range + 40;
        case MoveKind::Vortex: return d < m.range + m.radius * 0.6f;
        case MoveKind::Cone:   return d < m.range + 4;
        case MoveKind::Leap:   return d < m.range + 20 && d > 25;
        case MoveKind::Wall:   return false;
        default: return true;
    }
}

float IdealRange(const Fighter& f) {
    float closeP = 0, farP = 0;
    for (int i = 0; i < f.nMoves; i++) {
        const MoveDef& m = f.moves[i];
        switch (m.kind) {
            case MoveKind::Melee: case MoveKind::Burst: case MoveKind::Dash: case MoveKind::Zone:
            case MoveKind::Wave: case MoveKind::Blink: case MoveKind::Burrow:
            case MoveKind::Orbit: case MoveKind::Cone: case MoveKind::Leap: closeP += m.power; break;
            case MoveKind::Projectile: case MoveKind::Spread: case MoveKind::Beam: case MoveKind::Lob:
            case MoveKind::Chain: case MoveKind::Rain: case MoveKind::Boomerang: case MoveKind::Trap:
            case MoveKind::Vortex: farP += m.power; break;
            default: break;
        }
    }
    return closeP > farP * 1.1f ? 28.0f : 110.0f;
}

float BestEff(const Fighter& att, const Fighter& def) {
    float best = 0;
    for (int i = 0; i < att.nMoves; i++)
        if (IsDamaging(att.moves[i])) best = std::max(best, TypeEffectiveness(att.moves[i].type, def.sp->type1, def.sp->type2));
    return best;
}

float Matchup(const Fighter& me, const Fighter& them) { return BestEff(me, them) - BestEff(them, me); }

const char* StatName(StatId s) { return s == StatId::Atk ? "ATK" : (s == StatId::Def ? "DEF" : "SPD"); }

bool HasType(const SpeciesDef* sp, Type t) { return sp->type1 == t || sp->type2 == t; }

bool ImmuneTo(const SpeciesDef* sp, Status s) {
    switch (s) {
        case Status::Burn: return HasType(sp, Type::Fire);
        case Status::Paralyze: return HasType(sp, Type::Electric);
        case Status::Freeze: return HasType(sp, Type::Ice);
        case Status::Poison: case Status::Toxic: return HasType(sp, Type::Poison) || HasType(sp, Type::Steel);
        case Status::Seed: return HasType(sp, Type::Grass);
        default: return false;
    }
}

Color StatusColor(Status s) {
    switch (s) {
        case Status::Burn: return {255, 120, 60, 255};
        case Status::Poison: case Status::Toxic: return {190, 90, 220, 255};
        case Status::Paralyze: return {250, 220, 60, 255};
        case Status::Freeze: return {150, 220, 255, 255};
        case Status::Sleep: return {170, 170, 200, 255};
        case Status::Confuse: return {255, 140, 200, 255};
        case Status::Seed: return {120, 220, 90, 255};
        default: return WHITE;
    }
}

//                       react        dodge  speed  aimErr lead  cdMul  rest   strafe
const AIParams kAI[4] = {{1.0f, 1.8f, 0.08f, 0.75f, 0.22f, 0.0f, 1.35f, 0.50f, 0.25f},   // Easy
                         {0.55f, 1.1f, 0.22f, 0.88f, 0.12f, 0.4f, 1.12f, 0.30f, 0.50f},  // Normal
                         {0.35f, 0.8f, 0.45f, 1.00f, 0.06f, 0.7f, 1.00f, 0.10f, 0.80f},  // Hard
                         {0.30f, 0.6f, 0.15f, 1.00f, 0.00f, 0.0f, 1.00f, 0.15f, 0.50f}}; // simulated player

const AIParams& Params(AILevel l) { return kAI[std::max(0, std::min(3, (int)l))]; }

Sfx SoundFor(const MoveDef& m) {
    switch (m.type) {
        case Type::Electric: return Sfx::Zap;
        case Type::Fire: return Sfx::Fire;
        case Type::Water: case Type::Ice: return Sfx::Water;
        case Type::Ground: case Type::Rock: case Type::Fighting: return Sfx::Thud;
        default: return Sfx::Shoot;
    }
}

void LearnNewMoves(Fighter& f, std::vector<FloatText>& texts, bool evolving = false) {
    if (f.transformFrom >= 0) return;  // a transformed creature learns nothing (it changes back after)
    for (const MoveDef& m : MovesLearnedAt(*f.sp, f.level, evolving)) {
        bool known = false;
        for (int i = 0; i < f.nMoves; i++) known |= f.moves[i].name == m.name;
        for (const auto& p : f.pendingLearn) known |= p == m.name;
        if (known) continue;
        if (f.nMoves < 4) {
            f.moves[f.nMoves++] = m;
            f.learnedNow.push_back(m.name);
            FloatText t;
            t.pos = f.pos;
            t.z = 52;
            t.text = "Learned " + m.name + "!";
            t.col = Color{140, 230, 255, 255};
            t.maxLife = 1.6f;
            texts.push_back(t);
        } else {
            f.pendingLearn.push_back(m.name);
        }
    }
}

} // namespace

const char* AILevelName(AILevel l) {
    switch (l) {
        case AILevel::Easy: return "EASY";
        case AILevel::Hard: return "HARD";
        default: return "NORMAL";
    }
}

// ---------------------------------------------------------------- Fighter / Side

float Fighter::Stat(StatId s, bool special) const {
    int b = s == StatId::Atk ? (special ? sp->SpAtk() : sp->atk) : (s == StatId::Def ? (special ? sp->SpDef() : sp->def) : sp->spd);
    float base = Scaled(b);
    float v = (base + 5) * StatMul(level) * buffMul[(int)s] * dropMul[(int)s];
    v *= NatureMul(nature, s == StatId::Atk ? (special ? NS_SPA : NS_ATK) : (s == StatId::Def ? (special ? NS_SPD : NS_DEF) : NS_SPE));
    if (s == StatId::Atk && !special && st.major == Status::Burn) v *= 0.75f;  // burns weaken physical attacks
    if (s == StatId::Atk) v *= envAtk;
    if (s == StatId::Atk && !special) v *= abAtk;   // Guts, Hustle
    if (s == StatId::Def) v *= abDef;               // Marvel Scale
    if (item) {
        if (s == StatId::Atk && (item->fx == ItemFx::AtkMul || item->fx == ItemFx::ChoiceBand)) v *= item->value;
        if (s == StatId::Def && item->fx == ItemFx::DefMul) v *= item->value;
        if (s == StatId::Def && item->fx == ItemFx::DefIfEvolves && CanEvolve(species)) v *= item->value;
    }
    return v;
}

float Fighter::MoveSpeed() const {
    float v = 45.0f + (Scaled(sp->spd) * NatureMul(nature, NS_SPE) + 5) * buffMul[2] * dropMul[2] * 0.75f;
    if (st.major == Status::Paralyze) v *= 0.55f;
    if (item && (item->fx == ItemFx::SpeedMul || item->fx == ItemFx::ChoiceScarf)) v *= item->value;
    return v * envSpeed * abSpd;
}

float Fighter::CooldownMul() const {
    float m = 1.1f - (Scaled(sp->spd) * NatureMul(nature, NS_SPE) - 45) / 650.0f;
    if (item) {
        if (item->fx == ItemFx::CooldownMul) m *= item->value;
        if (item->fx == ItemFx::ChoiceScarf || item->fx == ItemFx::ChoiceBand) m *= 1.15f;
    }
    return m;
}

float Fighter::JumpZ() const {
    if (jumpT <= 0 || jumpDur <= 0) return 0;
    float u = 1.0f - jumpT / jumpDur;
    return 4.0f * JUMP_HEIGHT * u * (1.0f - u);
}

bool Fighter::CanAct() const {
    return st.major != Status::Freeze && st.major != Status::Sleep && st.flinchT <= 0 && st.paraStunT <= 0 &&
           rechargeT <= 0 && hitstun <= 0 && !burrowed && dazeT <= 0;
}

void Fighter::RecalcHp(bool keepDamage) {
    int newMax = std::max(1, (int)((Scaled(sp->hp) + 60) * HP_SCALE * StatMul(level) * (boss > 0 ? boss : 1.0f)));
    if (keepDamage) hp += newMax - maxHp;
    else hp = (float)newMax;
    maxHp = newMax;
    hp = std::max(hp > 0 ? 1.0f : 0.0f, std::min((float)maxHp, hp));
}

int Side::AliveCount() const {
    int n = 0;
    for (const auto& f : team) if (f.hp > 0 && !f.retired && !f.lent) n++;
    return n;
}

int Side::NextAlive(int from) const {
    int n = (int)team.size();
    for (int k = 1; k < n; k++) {
        int i = (from + k) % n;
        if (team[i].hp > 0 && !team[i].retired && !team[i].lent && team[i].state == FState::Benched) return i;
    }
    return -1;
}

int Side::PrevAlive(int from) const {
    int n = (int)team.size();
    for (int k = 1; k < n; k++) {
        int i = ((from - k) % n + n) % n;
        if (team[i].hp > 0 && !team[i].retired && !team[i].lent && team[i].state == FState::Benched) return i;
    }
    return -1;
}

// ---------------------------------------------------------------- Slots, teams and targets

std::vector<int> Battle::FoeSlots(int s) const {
    std::vector<int> v;
    for (int k = 0; k < N(); k++)
        if (Foes(s, k)) v.push_back(k);
    return v;
}

int Battle::AnyFoe(int s) const {
    int any = -1;
    for (int k = 0; k < N(); k++) {
        if (!Foes(s, k)) continue;
        if (Targetable(k)) return k;
        if (any < 0) any = k;
    }
    return any >= 0 ? any : (s == 0 && N() > 1 ? 1 : 0);
}

int Battle::NearestFoe(int src, Vector2 p) const {
    int best = -1;
    float bd = 1e9f;
    for (int k = 0; k < N(); k++) {
        if (!Foes(src, k) || !Targetable(k)) continue;
        float d = Dist(p, sides[k].Cur().pos);
        if (d < bd) { bd = d; best = k; }
    }
    return best;
}

void Battle::PickTarget(int s, float dt) {
    Side& sd = sides[s];
    const Fighter& f = sd.Cur();
    std::vector<int> foes = FoeSlots(s);
    if (foes.empty()) { sd.target = -1; return; }
    for (int k : foes)   // Follow Me / Rage Powder: everyone aims at that one
        if (Targetable(k) && sides[k].Cur().followT > 0) { sd.target = k; return; }
    bool curOk = sd.target >= 0 && sd.target < N() && Foes(s, sd.target) && Targetable(sd.target);
    if (sd.cpu) {
        Fighter& me = sd.Cur();
        int lv = std::max(0, std::min(2, (int)sd.level));
        // Got hit by someone it isn't aiming at (especially from behind)? Turn on them.
        if (me.hitFresh) {
            me.hitFresh = false;
            int att = me.lastHitBy;
            if (att >= 0 && att < N() && att != sd.target && Foes(s, att) && Targetable(att)) {
                bool behind = Dot(f.aim, SafeNorm(sides[att].Cur().pos - f.pos, f.aim)) < 0.2f;
                const float base[3] = {0.3f, 0.6f, 0.85f};
                if (Chance(base[lv] + (behind ? 0.2f : 0.0f))) { sd.target = att; sd.ai.targetT = 2.0f; return; }
            }
        }
        // Every so often, pick the foe that's best to hit.
        sd.ai.targetT -= dt;
        if (curOk && sd.ai.targetT > 0) return;
        const float every[3] = {3.5f, 2.5f, 1.6f};
        sd.ai.targetT = every[lv];
        int best = -1;
        float bs = -1e9f;
        for (int k : foes) {
            if (!Targetable(k)) continue;
            const Fighter& e = sides[k].Cur();
            float hpf = e.hp / std::max(1.0f, (float)e.maxHp);
            float sc = BestEff(f, e) * 30 - hpf * 40 - Dist(f.pos, e.pos) * 0.1f + Frand(0, 15);
            if (hpf < 0.35f) sc += 25 + 20 * lv;                        // close to fainting: finish it off
            if (k == f.lastHitBy && f.hurtT < 3) sc += 20;              // whoever has been hitting us
            if (k == sd.target) sc += 6;                                 // a little stickiness
            for (int a = 0; a < N(); a++)                                 // spread out unless going for the kill
                if (a != s && !Foes(s, a) && sides[a].cpu && sides[a].target == k && hpf > 0.4f) sc -= 18;
            if (sc > bs) { bs = sc; best = k; }
        }
        sd.target = best >= 0 ? best : (curOk ? sd.target : foes[0]);
        return;
    }
    if (sd.manualT > 0) {   // the player chose this target: keep it for a while
        sd.manualT -= dt;
        if (curOk) return;
    }
    // A player re-targets by moving toward a foe.
    if (f.moving || !curOk) {
        int best = -1;
        float bd = -2;
        Vector2 dir = SafeNorm(f.moveVel, f.aim);
        for (int k : foes) {
            if (!Targetable(k)) continue;
            Vector2 to = SafeNorm(sides[k].Cur().pos - f.pos);
            float d = Dot(dir, to);
            if (!f.moving) d = -Dist(f.pos, sides[k].Cur().pos);  // nothing to go on: the nearest
            if (d > bd) { bd = d; best = k; }
        }
        if (best >= 0 && (!curOk || !f.moving || bd > 0.7f)) sd.target = best;
    }
    if (sd.target < 0 || sd.target >= N() || !Foes(s, sd.target)) sd.target = foes[0];
}

void Battle::CycleTarget(int s, int dir) {
    Side& sd = sides[s];
    std::vector<int> live;
    for (int k : FoeSlots(s)) if (Targetable(k)) live.push_back(k);
    if (live.size() < 2) return;
    int i = 0;
    for (int k = 0; k < (int)live.size(); k++) if (live[k] == sd.target) i = k;
    i = ((i + (dir < 0 ? -1 : 1)) % (int)live.size() + (int)live.size()) % (int)live.size();
    sd.target = live[i];
    sd.manualT = 5.0f;
    const Fighter& t = sides[sd.target].Cur();
    AddText(t.pos, "Target!", TeamColor(sd.teamId), 34);
    PlaySfx(Sfx::MenuMove);
}

int Battle::PartnerWithBench(int s) const {
    for (int k = 0; k < N(); k++) {
        if (k == s || sides[k].teamId != sides[s].teamId) continue;
        if (sides[k].NextAlive(sides[k].active) >= 0) return k;
    }
    return -1;
}

void Battle::Borrow(int s) {
    int p = PartnerWithBench(s);
    if (p < 0) return;
    Side& from = sides[p];
    int idx = from.NextAlive(from.active);
    if (idx < 0) return;
    Fighter copy = from.team[idx];
    from.team[idx].lent = true;  // stays in the partner's list (for results), but can't be sent out there
    Side& sd = sides[s];
    sd.lastPos = sd.Cur().pos;
    sd.team.push_back(copy);
    AddText(sd.lastPos, (sd.tag.empty() ? sd.label : sd.tag) + " takes over " + copy.sp->name + "!", TeamColor(sd.teamId), 40);
    SendOut(s, (int)sd.team.size() - 1);
}

std::vector<const Fighter*> Battle::TeamFighters(int team) const {
    std::vector<const Fighter*> v;
    for (const auto& sd : sides)
        if (sd.teamId == team)
            for (const auto& f : sd.team)
                if (!f.lent) v.push_back(&f);
    return v;
}

std::string Battle::TeamLabel(int team) const {
    std::string l;
    for (const auto& sd : sides)
        if (sd.teamId == team) {
            if (!l.empty() && l != sd.label) l += " & " + sd.label;
            else if (l.empty()) l = sd.label;
        }
    return l;
}

// ---------------------------------------------------------------- Setup

void Battle::Start(const BattleSetup& su) {
    *this = Battle();
    setup = su;
    rng.s = ((uint64_t)(uint32_t)GetRandomValue(0, 0x7FFFFFFF) << 32) ^ (uint64_t)(uint32_t)GetRandomValue(0, 0x7FFFFFFF);
    SimRngScope rngScope(rng);
    for (int si = 0; si < (int)su.side.size(); si++) {
        const SideSetup& ss = su.side[si];
        if (ss.team.empty()) continue;  // no creatures: no slot
        sides.push_back(Side());
        Side& sd = sides.back();
        sd.cpu = ss.cpu;
        sd.level = ss.level;
        sd.earnXp = ss.earnXp;
        sd.label = ss.label;
        sd.teamId = ss.team_ >= 0 ? ss.team_ : std::min(si, 1);
        sd.input = ss.cpu ? -1 : (ss.input >= 0 ? ss.input : std::min(si, 1));
        sd.tag = ss.tag;
        sd.dummy = ss.dummy;
        for (const CombatantSpec& c : ss.team) {
            Fighter f;
            f.owner = c.owner;
            f.species = c.species;
            f.sp = &AllSpecies()[c.species];
            f.level = std::max(1, std::min(MAX_LEVEL, c.level));
            f.xp = std::max(c.xp, XpForLevel(f.level));
            f.ownedId = c.ownedId;
            f.boss = c.boss;
            InitAbility(f, c.abilitySlot);
            f.nature = c.nature >= 0 && c.nature < NATURE_COUNT ? c.nature : SimRand(0, NATURE_COUNT - 1);
            f.item = FindItem(c.item);
            if (f.item && IsBall(*f.item)) f.item = nullptr;
            f.nMoves = 0;
            for (const auto& m : c.moves)
                if (f.nMoves < 4) f.moves[f.nMoves++] = m;
            if (f.nMoves == 0) {
                for (const auto& m : MovesAtLevel(*f.sp, f.level))
                    if (f.nMoves < 4) f.moves[f.nMoves++] = m;
            }
            f.RecalcHp(false);
            if (c.hpFrac < 1.0f) f.hp = std::max(1.0f, std::round(f.maxHp * std::max(0.0f, c.hpFrac)));
            f.displayHp = f.hp;
            f.startLevel = f.level;
            f.startSpecies = f.species;
            sd.team.push_back(f);
        }
    }
    // Start positions: each team on its own side; two slots on a team stand apart.
    for (int t = 0; t < 2; t++) {
        std::vector<int> mine;
        for (int s = 0; s < N(); s++)
            if (sides[s].teamId == t) mine.push_back(s);
        for (size_t k = 0; k < mine.size(); k++) {
            float y = ARENA_H / 2 + (mine.size() > 1 ? (k == 0 ? -70.0f : 70.0f) : 0.0f);
            sides[mine[k]].lastPos = {t == 0 ? 120.0f : ARENA_W - 120, y};
        }
    }
    int lvSum = 0, lvN = 0;
    for (int s = 0; s < N(); s++)
        for (const auto& f : sides[s].team) { lvSum += f.level; lvN++; }
    envLevel = lvN ? lvSum / lvN : 50;
    field.Build(su.arena, (Venue)su.venue, su.season);
    if (IsWindowReady()) SetBattleArena(su.arena, field);
    if (su.weather != Weather::None) {   // the day's weather (Story Mode seasons)
        weather = natural = su.weather;
        weatherT = 1e6f;
        weatherTick = 1.0f;
    }
    for (int s = 0; s < N(); s++) SendOut(s, 0);
    for (int s = 0; s < N(); s++) PickTarget(s);
    introT = 2.0f;
}

void Battle::SendOut(int s, int idx) {
    Side& sd = sides[s];
    sd.active = idx;
    Fighter& f = sd.Cur();
    f.state = FState::Entering;
    f.stateT = 0;
    f.pos = sd.lastPos;
    f.knock = {0, 0};
    f.slide = {0, 0};
    f.onIce = false;
    f.pendingMove = -1;
    f.dashing = false;
    f.burrowed = false;
    f.leaping = false;
    f.volleyLeft = 0;
    f.hitstun = 0;
    f.shieldT = f.rechargeT = 0;
    f.sentOut = true;
    for (int k = 0; k < 3; k++) { f.buffMul[k] = 1; f.buffT[k] = 0; f.dropMul[k] = 1; f.dropT[k] = 0; }
    f.st.confuseT = f.st.flinchT = f.st.seedT = 0;
    f.aim = {sides[s].teamId == 0 ? 1.0f : -1.0f, 0};
    f.dir = DirFromVec(f.aim);
    AddText(f.pos, "Go! " + f.sp->name + "!", TeamColor(sides[s].teamId), 30);
    Burst(f.pos, WHITE, 14, 60, 8);
    f.flashFire = false;
    f.sinceAct = 99;
    AbOnEnter(s);
}

bool Battle::Finished() const {
    if (winner < 0 || overT <= 2.5f) return false;
    for (int s = 0; s < N(); s++)
        if (sides[s].Cur().state == FState::Evolving) return false;
    return true;
}

// ---------------------------------------------------------------- Update

void Battle::Update(float dt, const PlayerInput in[2]) {
    SimRngScope rngScope(rng);
    AbRefresh();
    time += dt;
    hype = std::max(0.0f, hype - dt * 0.35f);
    if (introT > 0) introT -= dt;
    bool canAct = introT <= 0 && winner < 0;

    // Stalemate breaker: if nobody loses HP for 30 s (e.g. two creatures that can't hurt each
    // other), or the battle passes 200 s, sudden death starts: everyone on the field loses HP.
    if (canAct && !setup.practice) {
        float sum = 0;
        for (const auto& sd : sides)
            for (const auto& f : sd.team) sum += std::max(0.0f, f.hp);
        if (lastHpSum >= 0 && sum < lastHpSum - 0.5f && !suddenDeath) sinceDamage = 0;
        else sinceDamage += dt;
        lastHpSum = sum;
        if (!suddenDeath && (sinceDamage > 30 || time > 200)) {
            suddenDeath = true;
            AddText({ARENA_W / 2.0f, ARENA_H / 2.0f}, "Stalemate! Sudden death!", UI_GOLD, 60);
            PlaySfx(Sfx::Status);
        }
        if (suddenDeath && (suddenTick -= dt) <= 0) {
            suddenTick = 0.5f;
            for (int s = 0; s < N(); s++)
                if (sides[s].Cur().state == FState::Active) DirectDamage(s, sides[s].Cur().maxHp * 0.03f, UI_GOLD, AnyFoe(s), true);
        }
    }

    static const PlayerInput none{};
    for (int s = 0; s < N(); s++) {
        int pi = sides[s].input;
        if (!sides[s].cpu && pi >= 0 && pi < 2 && in[pi].target != 0) CycleTarget(s, in[pi].target);
        PickTarget(s, dt);
        int ii = sides[s].input;
        UpdateSide(s, dt, ii >= 0 && ii < 2 ? in[ii] : none, canAct);
    }

    auto solid = [](const Fighter& f) {
        return (f.state == FState::Active || f.state == FState::Entering) && !f.burrowed && !f.dashing;
    };
    // Bodies are solid - foes and teammates alike. The check runs in a squashed space so two
    // creatures can't stand on top of each other on screen (the arena is drawn foreshortened),
    // and bigger creatures take up more room.
    auto bodyR = [](const Fighter& f) { return HitR(f) + 2.0f; };
    const float YK = 0.7f;
    for (int pass = 0; pass < 2; pass++)
        for (int i = 0; i < N(); i++)
            for (int j = i + 1; j < N(); j++) {
                Fighter& a = sides[i].Cur();
                Fighter& b = sides[j].Cur();
                if (!solid(a) || !solid(b)) continue;
                Vector2 d = b.pos - a.pos;
                Vector2 q{d.x, d.y * YK};
                float l = Len(q);
                float minD = bodyR(a) + bodyR(b);
                if (l < minD) {
                    Vector2 n = l > 0.001f ? q * (1.0f / l) : Vector2{i < j ? 1.0f : -1.0f, 0};
                    float push = (minD - l) * 0.5f;
                    Vector2 w{n.x * push, n.y * push / YK};
                    a.pos -= w;
                    b.pos += w;
                }
            }
    for (int s = 0; s < N(); s++) {
        Fighter& f = sides[s].Cur();
        if ((solid(f) || f.dashing) && f.jumpT <= 0) {   // jumping clears obstacles
            for (const Obstacle& o : field.obs) {
                if (!o.Blocks()) continue;
                Vector2 d = f.pos - o.pos;
                float minD = o.radius + BODY_R * 0.7f;
                if (Len(d) < minD) f.pos = o.pos + SafeNorm(d, Vector2{0, 1}) * minD;
            }
        }
        f.pos = ClampToArena(f.pos, BODY_R + 2);
    }

    if (weather != Weather::None) {
        weatherT -= dt;
        weatherTick -= dt;
        if (weatherTick <= 0 && weather == Weather::Sand && winner < 0 && weather != natural) {   // snow does no damage (as in Scarlet/Violet)
            weatherTick = 1.0f;
            for (int s = 0; s < N(); s++) {
                Fighter& f = sides[s].Cur();
                if (f.state != FState::Active || f.burrowed) continue;
                bool immune = weather == Weather::Sand ? (HasType(f.sp, Type::Rock) || HasType(f.sp, Type::Ground) || HasType(f.sp, Type::Steel))
                                                       : HasType(f.sp, Type::Ice);
                Ab fa = AbOf(f);
                if (cloudNine || fa == Ab::Overcoat || fa == Ab::MagicGuard) immune = true;
                if (weather == Weather::Sand && (fa == Ab::SandVeil || fa == Ab::SandRush || fa == Ab::SandForce)) immune = true;
                if (weather == Weather::Snow && (fa == Ab::IceBody || fa == Ab::SnowCloak)) immune = true;
                if (!immune) DirectDamage(s, f.maxHp * 0.03f, weather == Weather::Sand ? Color{210, 180, 120, 255} : Color{200, 230, 255, 255}, AnyFoe(s));
            }
        }
        if (weatherT <= 0 && natural != Weather::None) {   // a weather move wore off: the day's weather returns
            weather = natural;
            weatherT = 1e6f;
            AddText({ARENA_W / 2, ARENA_H * 0.2f}, natural == Weather::Rain ? "The rain kept falling." : natural == Weather::Sun ? "The sun is still shining."
                                                   : natural == Weather::Snow ? "Snow is still falling." : "The sandstorm rages on.", UI_DIM, 20);
        }
        if (weatherT <= 0) {
            AddText({ARENA_W / 2, ARENA_H * 0.2f}, weather == Weather::Rain ? "The rain stopped." : weather == Weather::Sun ? "The sunlight faded."
                                                   : weather == Weather::Sand ? "The sandstorm subsided." : "The snow stopped.", UI_DIM, 20);
            weather = Weather::None;
        }
    }
    UpdateField(dt);
    UpdateWeatherField(dt);
    UpdateProjectiles(dt);
    UpdateHazards(dt);
    UpdateWaves(dt);
    UpdateEffects(dt);

    if (shakeT > 0 && setup.screenShake) {
        shakeT -= dt;
        gShake = {(float)GetRandomValue(-shakeAmt, shakeAmt), (float)GetRandomValue(-shakeAmt, shakeAmt)};
    } else {
        shakeT = 0;
        gShake = {0, 0};
    }

    if (winner < 0) {
        bool out[2] = {true, true};
        for (int s = 0; s < N(); s++)
            if (!(sides[s].AliveCount() == 0 && sides[s].Cur().state == FState::Fainted)) out[sides[s].teamId] = false;
        bool d0 = out[0], d1 = out[1];
        if (d0 && d1) winner = 2;
        else if (d0) winner = 1;
        else if (d1) winner = 0;
        if (winner >= 0) PlaySfx(Sfx::Win);
        if (winner >= 0 && setup.arena != ArenaKind::Stadium && !IsLegendArena(setup.arena)) { Confetti(140); hype = 1; }
    } else {
        overT += dt;
    }
}

void Battle::UpdateSide(int s, float dt, const PlayerInput& in, bool canAct) {
    Side& sd = sides[s];
    sd.swapCd = std::max(0.0f, sd.swapCd - dt);
    for (auto& tf : sd.team)
        for (float& c : tf.cd) c = std::max(0.0f, c - dt);

    Fighter& f = sd.Cur();
    f.stateT += dt;
    f.animT += dt;
    f.flash = std::max(0.0f, f.flash - dt);
    f.hitstun = std::max(0.0f, f.hitstun - dt);
    f.shieldT = std::max(0.0f, f.shieldT - dt);
    f.safeT = std::max(0.0f, f.safeT - dt);
    f.mistT = std::max(0.0f, f.mistT - dt);
    f.chantT = std::max(0.0f, f.chantT - dt);
    f.followT = std::max(0.0f, f.followT - dt);
    f.dodgeCd = std::max(0.0f, f.dodgeCd - dt);
    f.hurtT = std::min(99.0f, f.hurtT + dt);
    f.counterT = std::max(0.0f, f.counterT - dt);
    f.endureT = std::max(0.0f, f.endureT - dt);
    f.rechargeT = std::max(0.0f, f.rechargeT - dt);
    for (int k = 0; k < 3; k++) {
        if (f.buffT[k] > 0 && (f.buffT[k] -= dt) <= 0) { f.buffT[k] = 0; f.buffMul[k] = 1; }
        if (f.dropT[k] > 0 && (f.dropT[k] -= dt) <= 0) { f.dropT[k] = 0; f.dropMul[k] = 1; }
    }
    f.displayHp += (f.hp - f.displayHp) * std::min(1.0f, dt * 8);
    if (f.Holds(ItemFx::Leftovers) && f.state == FState::Active && f.hp > 0 && f.hp < f.maxHp && !AbOnField(Ab::Unnerve, -1, s)) {
        f.leftAcc += f.maxHp * f.item->value * dt;
        if (f.leftAcc >= 1) { float a = std::floor(f.leftAcc); f.hp = std::min((float)f.maxHp, f.hp + a); f.leftAcc -= a; }
    }
    f.moving = false;
    f.moveVel = {0, 0};

    switch (f.state) {
        case FState::Entering:
            if (f.stateT >= 0.45f) { f.state = FState::Active; f.stateT = 0; }
            break;
        case FState::Recalling:
            if (f.stateT >= 0.35f) {
                sd.lastPos = f.pos;
                AbOnLeave(s);
                f.state = FState::Benched;
                SendOut(s, sd.swapTo);
            }
            return;
        case FState::Evolving:
            f.knock = {0, 0};
            if (f.stateT >= 1.8f) FinishEvolution(s);
            return;
        case FState::Fainting:
            f.pos += f.knock * dt;
            f.knock *= std::exp(-9 * dt);
            if (f.stateT >= 1.0f) {
                f.state = FState::Fainted;
                sd.lastPos = f.pos;
                if (sd.NextAlive(sd.active) >= 0 || PartnerWithBench(s) >= 0) sd.sendT = sd.cpu ? 0.6f : (sd.NextAlive(sd.active) >= 0 ? 0.6f : 0.0f);
            }
            return;
        case FState::Fainted: {
            bool someoneEvolving = false;
            for (int k = 0; k < N(); k++) someoneEvolving |= sides[k].Cur().state == FState::Evolving;
            if (someoneEvolving || winner >= 0) return;
            int n = sd.NextAlive(sd.active);
            if (n >= 0) {
                if (sd.sendT >= 0) {
                    sd.sendT -= dt;
                    if (sd.sendT < 0) SendOut(s, n);
                }
            } else if (PartnerWithBench(s) >= 0) {
                // Out of creatures: take over a teammate's benched one (the CPU does it right away,
                // a player presses Swap).
                if (sd.cpu) {
                    sd.sendT -= dt;
                    if (sd.sendT < -0.6f) Borrow(s);
                } else if (in.swap || in.swapPrev) {
                    Borrow(s);
                }
            }
            return;
        }
        default: break;
    }

    f.pos += f.knock * dt;
    f.knock *= std::exp(-9 * dt);
    if (f.state != FState::Active) { f.dodgeT = f.jumpT = 0; return; }

    UpdateStatus(s, dt);
    if (f.state != FState::Active) return;
    AbTick(s, dt);
    if (f.state != FState::Active) return;
    if (f.state != FState::Active) return;
    if (f.weakened && f.dazeT > 0) {
        f.dazeT -= dt;
        if (f.dazeT <= 0) { f.dazeT = 0; Retire(s); }
        return;
    }

    Intent it;
    if (canAct && !gVfxDemo && !sd.dummy) it = sd.cpu ? ThinkAI(s, dt) : HumanIntent(in);
    int os = sd.target;

    // Underground / in the sky: travel to the target, then erupt.
    if (f.burrowed) {
        const MoveDef& bm = f.moves[f.burrowMove];
        Vector2 to = f.burrowTarget - f.pos;
        float step = bm.range / std::max(0.2f, bm.duration) * dt;
        if (Len(to) > step) f.pos += SafeNorm(to) * step;
        else f.pos = f.burrowTarget;
        if (bm.type != Type::Flying && !f.leaping && SimRand(0, 1)) {
            Particle& p = Emit(bm.type == Type::Water ? PK::Drop : PK::Debris, f.pos + Vector2{Frand(-5, 5), Frand(-3, 3)}, 0, {0, 0}, Frand(20, 50), 0.4f, 1.6f,
                               bm.type == Type::Water ? Color{120, 170, 240, 255} : Color{150, 120, 80, 255}, true);
            p.spin = Frand(-8, 8);
        }
        f.burrowT -= dt;
        if (f.burrowT <= 0) {
            f.burrowed = false;
            f.leaping = false;
            HitSpec h = Spec(s, bm);
            Fx fx;
            fx.kind = FxKind::Ring;
            fx.pos = f.pos;
            fx.maxLife = 0.35f;
            fx.radius = bm.radius;
            fx.col = TypeColor(bm.type);
            fxs.push_back(fx);
            {
                AddFx(FxKind::Shock, f.pos, 0.4f, TypeColor(bm.type), bm.radius * 1.2f);
                bool sky = bm.type == Type::Flying || f.leaping;
                if (!sky) AddFx(FxKind::Crack, f.pos, 1.0f, Color{60, 45, 30, 255}, bm.radius * 0.8f).len = 5;
                for (int i = 0; i < 14; i++) {
                    Vector2 d = RandDirB();
                    PK k = bm.type == Type::Water ? PK::Drop : (sky ? PK::Feather : PK::Debris);
                    Color c = bm.type == Type::Water ? Color{150, 200, 255, 255} : (sky ? WHITE : Color{140, 115, 85, 255});
                    if (bm.type == Type::Ghost) { k = PK::Wisp; c = Color{170, 100, 255, 255}; }
                    Particle& q = Emit(k, f.pos, 2, d * Frand(40, 110), Frand(60, 120), 0.7f, Frand(1.6f, 2.6f), c, k != PK::Wisp);
                    q.spin = Frand(-10, 10);
                }
                for (int i = 0; i < 5; i++) Emit(PK::Smoke, f.pos + RandDirB() * 6.0f, 2, RandDirB() * 25.0f, 10, 0.9f, 4, bm.type == Type::Ghost ? Color{60, 30, 80, 255} : Color{165, 145, 115, 255}, false).grow = 8;
            }
            Shake(2);
            PlaySfx(Sfx::Thud);
            for (int ts : FoeSlots(s)) {
                if (!Hittable(ts) || Dist(f.pos, sides[ts].Cur().pos) >= bm.radius + HitR(sides[ts].Cur())) continue;
                Vector2 ep = sides[ts].Cur().pos;
                ApplyHit(h, ts, SafeNorm(ep - f.pos));
                TryElectrify(ep, h);
            }
            if (bm.kind == MoveKind::Leap) ObstaclesInArea(f.pos, bm.radius * 0.6f, h, true, f.pos);
        }
        return;
    }

    if (f.dashing) {
        f.pos += f.dashVel * dt;
        f.dashT -= dt;
        {
            const MoveDef& dm = f.moves[f.dashMove];
            const Look& DL = GetLook(LookKey(dm.name, dm.type, dm.kind), dm.type);
            PK dk = DL.trail == PK::Ring || DL.trail == PK::Smoke ? PK::Glow : DL.trail;
            Vector2 back = SafeNorm(f.dashVel) * -30.0f;
            Particle& p = Emit(dk, f.pos + Vector2{Frand(-4, 4), Frand(-3, 3)}, Frand(2, 12), back + RandDirB() * 15.0f, Frand(0, 20), 0.3f,
                               dk == PK::Glow ? 1.4f : (dk == PK::Flame ? 2.4f : 1.8f), ColorBrightness(DL.glow, Frand(-0.1f, 0.3f)), false);
            p.spin = Frand(-8, 8);
            if (SimRand(0, 1)) Emit(PK::Smoke, f.pos + Vector2{Frand(-3, 3), 2}, 1, back * 0.5f, 5, 0.4f, 2.5f, Color{190, 180, 165, 255}, false).grow = 5;
        }
        for (int ts : FoeSlots(s)) {
            if (f.dashHit || !Hittable(ts)) continue;
            const Fighter& e = sides[ts].Cur();
            if (Dist(f.pos, e.pos) < BODY_R * 2 + 4) {
                HitSpec h = Spec(s, f.moves[f.dashMove]);
                Vector2 dir = SafeNorm(f.dashVel);
                f.dashHit = true;
                f.dashT = 0;
                f.knock = dir * -60.0f;
                ApplyHit(h, ts, dir);
            }
        }
        if (!f.dashObsHit) {
            for (Obstacle& o : field.obs) {
                if (!o.Blocks() || Dist(f.pos, o.pos) > o.radius + BODY_R * 0.7f + 2) continue;
                Vector2 dir = SafeNorm(f.dashVel);
                f.dashObsHit = true;
                ObstacleImpact(o, Spec(s, f.moves[f.dashMove]), dir, true);
                f.dashT = 0;
                f.knock = dir * -60.0f;
                break;
            }
        }
        if (f.dashT <= 0) f.dashing = false;
        return;
    }
    if (f.dodgeT > 0) {   // dodging: a quick dash that attacks pass straight through
        f.dodgeT -= dt;
        f.pos += f.dodgeVel * dt;
        f.moving = true;
        Particle& p = Emit(PK::Glow, f.pos + Vector2{Frand(-4, 4), Frand(-3, 3)}, Frand(4, 14), SafeNorm(f.dodgeVel) * -40.0f, 0, 0.25f, 1.6f,
                           Color{200, 230, 255, 255}, false);
        p.spin = 0;
        if (f.dodgeT > 0) return;
        f.dodgeT = 0;
    }
    if (f.jumpT > 0) {   // in the air: steer a little, can't use moves until you land
        float l = Len(it.move);
        if (l > 0.15f) {
            Vector2 d = it.move * (1.0f / std::max(1.0f, l));
            if (f.st.confuseT > 0) d = Rot(d, f.st.confuseAngle);
            f.jumpVel += (d * (f.MoveSpeed() * 1.25f) - f.jumpVel) * std::min(1.0f, dt * 2.5f);
        }
        f.pos += f.jumpVel * dt;
        f.moving = true;
        f.jumpT -= dt;
        if (f.jumpT > 0) {
            if (it.swap || it.swapBack) TrySwap(s, it.swapBack && !it.swap);   // you can still call it back mid-air
            return;
        }
        f.jumpT = 0;
        bool wet = field.IsWater(f.pos);
        for (const Puddle& pd : field.puddles) wet |= pd.ice <= 0 && pd.Contains(f.pos);
        for (int i = 0; i < 8; i++) {   // landing: a splash or a puff of dust
            Vector2 d = RandDirB();
            if (wet) Emit(PK::Drop, f.pos + d * 4.0f, 1, d * Frand(25, 60), Frand(40, 80), 0.45f, 1.4f, Color{160, 205, 255, 255}, true);
            else Emit(PK::Smoke, f.pos + d * 5.0f, 1, d * Frand(15, 35), 4, 0.4f, 2.2f, Color{190, 180, 165, 255}, false).grow = 4;
        }
        PlaySfx(wet ? Sfx::Water : Sfx::Thud);
    }
    if (!f.CanAct()) return;
    if (it.dodge && f.dodgeCd <= 0) {
        Vector2 d = Len(it.move) > 0.15f ? it.move : f.aim * -1.0f;   // standing still: hop back from where you face
        StartDodge(s, d);
        return;
    }
    if (it.jump && f.pendingMove < 0 && f.volleyLeft <= 0) {
        StartJump(s, Len(it.move) > 0.15f ? it.move : Vector2{0, 0});
        return;
    }

    if (f.pendingMove >= 0) {
        f.windupT -= dt;
        if (f.windupT <= 0) {
            int mi = f.pendingMove;
            f.pendingMove = -1;
            Execute(s, mi, true);
        }
        return;
    }

    if (f.volleyLeft > 0) {
        f.volleyT -= dt;
        if (f.volleyT <= 0) {
            f.volleyLeft--;
            f.volleyT = 0.11f;
            if (Targetable(os) && (sd.cpu || setup.autoAim)) f.pendingAim = SafeNorm(sides[os].Cur().pos - f.pos, f.pendingAim);
            Execute(s, f.volleyMove, false);
        }
    }

    float l = Len(it.move);
    if (l > 0.15f) {
        Vector2 d = it.move * (1.0f / std::max(1.0f, l));
        if (f.st.confuseT > 0) d = Rot(d, f.st.confuseAngle);
        Vector2 want = d * f.MoveSpeed();
        if (f.onIce) {   // slippery: you can only change direction slowly
            f.slide += (want - f.slide) * std::min(1.0f, dt * 2.0f);
            f.moveVel = f.slide;
        } else {
            f.moveVel = want;
            f.slide = want;
        }
        f.slid = true;
        f.pos += f.moveVel * dt;
        f.moving = true;
        if (!sd.cpu) {
            f.aim = SafeNorm(it.move, f.aim);
            f.dir = DirFromVec(f.aim);
        }
    } else if (!sd.cpu && setup.autoAim && Targetable(os)) {
        f.aim = SafeNorm(sides[os].Cur().pos - f.pos, f.aim);
        f.dir = DirFromVec(f.aim);
    }

    if (f.volleyLeft > 0) return;
    if (it.swap || it.swapBack) { TrySwap(s, it.swapBack && !it.swap); return; }
    if (it.attack >= 0 && it.attack < f.nMoves) TryBeginMove(s, it.attack, it.hasAim ? it.aim : f.aim, it.hasAim);
}

void Battle::UpdateStatus(int s, float dt) {
    Fighter& f = sides[s].Cur();
    StatusState& st = f.st;
    int os = f.lastHitBy >= 0 && f.lastHitBy < N() && Foes(s, f.lastHitBy) ? f.lastHitBy : AnyFoe(s);
    if (st.major != Status::None) {
        st.majorT -= dt;
        st.tickT -= dt;
        if (st.tickT <= 0) {
            st.tickT = 0.5f;
            float pct = 0;
            if (st.major == Status::Burn) pct = 0.015f;
            else if (st.major == Status::Poison) pct = 0.012f;
            else if (st.major == Status::Toxic) pct = 0.007f * std::min(5, ++st.toxicN);
            if (pct > 0) {
                Particle p;
                p.pos = f.pos + Vector2{Frand(-6, 6), Frand(-3, 3)};
                p.z = Frand(4, 14);
                p.vz = 25;
                p.life = p.maxLife = 0.5f;
                p.col = StatusColor(st.major);
                p.size = 2;
                parts.push_back(p);
                DirectDamage(s, f.maxHp * pct, StatusColor(st.major), os);
                if (f.state != FState::Active) return;
            }
        }
        if (st.major == Status::Paralyze) {
            st.paraT -= dt;
            if (st.paraT <= 0) {
                st.paraT = 1.2f;
                if (Chance(0.3f)) {
                    st.paraStunT = 0.5f;
                    AddText(f.pos, "Paralyzed!", StatusColor(Status::Paralyze), 30);
                }
            }
        }
        if (st.majorT <= 0) {
            if (st.major == Status::Freeze) AddText(f.pos, "Thawed out!", StatusColor(Status::Freeze), 30);
            else if (st.major == Status::Sleep) AddText(f.pos, "Woke up!", UI_TEXT, 30);
            st.major = Status::None;
        }
    }
    st.paraStunT = std::max(0.0f, st.paraStunT - dt);
    st.flinchT = std::max(0.0f, st.flinchT - dt);
    if (st.confuseT > 0) {
        st.confuseT -= dt;
        st.confuseSwitchT -= dt;
        if (st.confuseSwitchT <= 0) {
            st.confuseSwitchT = 0.5f;
            st.confuseAngle = Frand(-1.6f, 1.6f);
        }
    }
    if (st.seedT > 0) {
        st.seedT -= dt;
        st.seedTickT -= dt;
        if (st.seedTickT <= 0) {
            st.seedTickT = 0.5f;
            float amt = f.maxHp * 0.015f;
            Fighter& sower = sides[st.seedSide < 0 || st.seedSide >= N() ? os : st.seedSide].Cur();
            if (sower.state == FState::Active && sower.hp > 0) sower.hp = std::min((float)sower.maxHp, sower.hp + amt);
            for (int i = 0; i < 2; i++) {
                Particle p;
                p.pos = f.pos;
                p.vel = SafeNorm(sower.pos - f.pos) * 60.0f;
                p.z = 8;
                p.life = p.maxLife = 0.5f;
                p.col = StatusColor(Status::Seed);
                p.size = 2;
                parts.push_back(p);
            }
            DirectDamage(s, amt, StatusColor(Status::Seed), os);
        }
    }
}

void Battle::StartDodge(int s, Vector2 dir) {
    Fighter& f = sides[s].Cur();
    Vector2 d = SafeNorm(dir, Vector2{f.dir == 1 ? -1.0f : 1.0f, 0});
    if (f.st.confuseT > 0) d = Rot(d, f.st.confuseAngle);
    f.pendingMove = -1;   // a dodge cancels a move that's still winding up
    f.volleyLeft = 0;
    f.dodgeVel = d * DODGE_SPEED;
    f.dodgeT = DODGE_TIME;
    f.dodgeCd = DODGE_COOLDOWN;
    f.slide = {0, 0};
    for (int i = 0; i < 6; i++) Emit(PK::Smoke, f.pos + RandDirB() * 4.0f, 1, d * -30.0f + RandDirB() * 15.0f, 4, 0.35f, 2.0f, Color{200, 195, 185, 255}, false).grow = 4;
    PlaySfx(Sfx::Shoot);
}

void Battle::StartJump(int s, Vector2 dir) {
    Fighter& f = sides[s].Cur();
    float l = Len(dir);
    Vector2 d = l > 0.15f ? dir * (1.0f / std::max(1.0f, l)) : Vector2{0, 0};
    if (f.st.confuseT > 0) d = Rot(d, f.st.confuseAngle);
    f.jumpDur = f.jumpT = JUMP_TIME;
    f.jumpVel = d * (f.MoveSpeed() * 1.25f);
    if (f.onIce) f.jumpVel = f.jumpVel * 0.5f + f.slide * 0.5f;   // you take your slide with you
    for (int i = 0; i < 4; i++) Emit(PK::Smoke, f.pos + RandDirB() * 4.0f, 1, RandDirB() * 15.0f, 4, 0.3f, 1.8f, Color{190, 180, 165, 255}, false).grow = 3;
    PlaySfx(Sfx::MenuMove);
}

Intent Battle::HumanIntent(const PlayerInput& in) const {
    Intent it;
    it.move = in.move;
    it.swap = in.swap;
    it.swapBack = in.swapPrev;
    it.dodge = in.dodge;
    it.jump = in.jump;
    for (int k = 0; k < 4; k++)
        if (in.attack[k]) { it.attack = k; break; }
    return it;
}

void Battle::TrySwap(int s, bool back) {
    Side& sd = sides[s];
    Fighter& f = sd.Cur();
    if (sd.swapCd > 0 || f.state != FState::Active) return;
    int n = back ? sd.PrevAlive(sd.active) : sd.NextAlive(sd.active);
    if (n < 0) return;
    Ab fa = AbOf(f);
    if (fa != Ab::RunAway && !HasType(f.sp, Type::Ghost)) {
        bool grounded = !HasType(f.sp, Type::Flying) && fa != Ab::Levitate && !f.sp->floats;
        if ((grounded && AbOnField(Ab::ArenaTrap, -1, s)) || (HasType(f.sp, Type::Steel) && AbOnField(Ab::MagnetPull, -1, s))) {
            AddText(f.pos, f.sp->name + " can't escape!", UI_DIM, 30);
            sd.swapCd = 0.8f;
            return;
        }
    }
    f.state = FState::Recalling;
    f.stateT = 0;
    sd.swapTo = n;
    sd.swapCd = fa == Ab::RunAway ? 1.25f : 2.5f;
    AddText(f.pos, f.sp->name + ", come back!", TeamColor(sides[s].teamId), 30);
}

void Battle::TryBeginMove(int s, int mi, Vector2 aim, bool exactAim) {
    Side& sd = sides[s];
    Fighter& f = sd.Cur();
    if (mi >= f.nMoves || f.cd[mi] > 0 || !f.HasUses(mi)) return;
    const MoveDef& m = f.moves[mi];
    aim = SafeNorm(aim, f.aim);
    f.uses[mi]++;

    if (!exactAim) {
        // Aim at the foe you're facing; with auto-aim on, fall back to your current target.
        int best = -1;
        float bestDot = std::cos(0.5f);
        for (int ts : FoeSlots(s)) {
            if (!Targetable(ts)) continue;
            Vector2 to = sides[ts].Cur().pos - f.pos;
            float d = Len(to);
            if (d < 1 || d > 300) continue;
            float dt2 = Dot(aim, to * (1.0f / d));
            if (dt2 > bestDot) { bestDot = dt2; best = ts; }
        }
        if (best < 0 && setup.autoAim && sd.target >= 0 && Targetable(sd.target)) best = sd.target;
        if (best >= 0) {
            sd.target = best;
            aim = SafeNorm(sides[best].Cur().pos - f.pos, aim);
        }
    }
    if (f.st.confuseT > 0) aim = Rot(aim, Frand(-0.9f, 0.9f));

    f.aim = aim;
    f.dir = DirFromVec(aim);
    f.pendingAim = aim;
    f.cd[mi] = m.cooldown * f.CooldownMul() * (sd.cpu ? Params(sd.level).cooldownMul : 1.0f) * (AbOnField(Ab::Pressure, -1, s) ? 1.15f : 1.0f);
    f.pendingMove = mi;
    f.windupT = m.windup;
    // Sun: Solar Beam / Solar Blade fire at once. Rain: so does Electro Shot.
    if (((m.name == "Solar Beam" || m.name == "Solar Blade") && Wx() == Weather::Sun) || (m.name == "Electro Shot" && Wx() == Weather::Rain)) f.windupT = 0;

    if (m.kind == MoveKind::Beam || m.kind == MoveKind::Burst || m.kind == MoveKind::Wave || m.windup >= 0.5f) {
        Fx fx;
        fx.kind = FxKind::Charge;
        fx.pos = f.pos;
        fx.maxLife = std::max(0.1f, m.windup);
        fx.radius = 20;
        fx.col = GetLook(LookKey(m.name, m.type, m.kind), m.type).glow;
        fx.seed = (uint32_t)SimRand(1, 0x3fffffff);
        fx.z = 9;
        fxs.push_back(fx);
    }
    if (f.windupT <= 0) {
        f.pendingMove = -1;
        Execute(s, mi, true);
    }
}

HitSpec Battle::Spec(int s, const MoveDef& m) const {
    const Fighter& f = sides[s].Cur();
    HitSpec h;
    h.side = s;
    h.fighter = sides[s].active;
    h.name = m.name;
    h.type = m.type;
    h.power = m.power;
    h.special = IsSpecial(m);
    h.atk = f.Stat(StatId::Atk, h.special);
    h.lvlMul = StatMul(f.level);
    h.t1 = f.sp->type1;
    h.t2 = f.sp->type2;
    h.status = m.status;
    h.statusChance = m.statusChance;
    h.lowers = m.lowers;
    h.lowerStat = m.lowerStat;
    h.lowerChance = m.lowerChance;
    h.drain = m.drain;
    h.recoil = m.recoil;
    h.knock = m.knock;
    h.highCrit = m.highCrit;
    h.hex = m.hex;
    h.patchPower = m.patchPower;
    h.patchTime = m.patchTime;
    h.patchRadius = m.patchRadius;
    h.heavy = IsHeavy(*f.sp);
    h.falseSwipe = m.falseSwipe;
    h.forceSwap = m.forceSwap;
    h.ohko = m.ohko;
    h.fixedDmg = m.fixedDmg;
    h.attLevel = f.level;
    h.brine = m.powerMod == PowerMod::Brine;
    h.look = LookKey(m.name, m.type, m.kind);
    {
        float frac = f.hp / std::max(1.0f, (float)f.maxHp);
        switch (m.powerMod) {
            case PowerMod::Facade: if (f.st.major != Status::None) h.power *= 2; break;
            case PowerMod::Reversal: h.power = (int)(h.power * (1.0f + 2.0f * (1.0f - frac))); break;
            case PowerMod::Eruption: h.power = std::max(1, (int)(h.power * std::max(0.25f, frac))); break;
            case PowerMod::Acrobatics: if (!f.item) h.power *= 2; break;
            case PowerMod::WeatherBall:
                if (weather != Weather::None) {
                    h.power *= 2;
                    h.type = weather == Weather::Rain ? Type::Water : weather == Weather::Sun ? Type::Fire
                           : weather == Weather::Sand ? Type::Rock : Type::Ice;
                }
                break;
            default: break;
        }
    }
    if ((m.name == "Solar Beam" || m.name == "Solar Blade") && (Wx() == Weather::Rain || Wx() == Weather::Sand || Wx() == Weather::Snow))
        h.power /= 2;   // weak in bad weather
    h.contact = m.kind == MoveKind::Melee || m.kind == MoveKind::Dash || m.kind == MoveKind::Blink || m.kind == MoveKind::Burrow;
    if (f.item && m.power > 0) {
        const ItemDef& it = *f.item;
        if (it.fx == ItemFx::TypeBoost && it.type == m.type) h.dmgMul *= it.value;
        if (it.fx == ItemFx::LifeOrb) h.dmgMul *= it.value;
        if (it.fx == ItemFx::ExpertBelt) h.superMul = it.value;
        if (it.fx == ItemFx::ScopeLens) h.scope = true;
        if (it.fx == ItemFx::KingsRock) h.flinchChance = it.value;
        if (it.fx == ItemFx::ShellBell) h.drain += it.value;
    }
    for (int i = 0; i < f.nMoves; i++)
        if (f.moves[i].name == m.name) { h.moveIdx = i; break; }
    AbSpec(h, f, m);
    return h;
}

HitSpec Battle::EnvSpec(Type t, int power, Status st, float chance) const {
    HitSpec h;
    h.side = -1;
    h.fighter = -1;
    h.name = "arena";
    h.type = t;
    h.power = power;
    h.atk = 85.0f * StatMul(envLevel);
    h.lvlMul = StatMul(envLevel);
    h.t1 = Type::None;
    h.t2 = Type::None;
    h.status = st;
    h.statusChance = chance;
    return h;
}

namespace {
bool InstantKind(const MoveDef& m) {
    switch (m.kind) {
        case MoveKind::Projectile: case MoveKind::Spread: case MoveKind::Melee: case MoveKind::Beam:
        case MoveKind::Burst: case MoveKind::Lob: case MoveKind::Chain: case MoveKind::Rain: case MoveKind::Wave:
        case MoveKind::Cone: case MoveKind::Zone: case MoveKind::Vortex:
            return m.power > 0 && m.hits <= 1 && m.recharge <= 0 && !m.metronome && !m.copycat && !m.selfKO;
        default: return false;
    }
}
} // namespace

void Battle::Execute(int s, int mi, bool first, const MoveDef* override) {
    Side& sd = sides[s];
    Fighter& f = sd.Cur();
    const MoveDef& m = override ? *override : f.moves[mi];
    if (m.transform) {
        int ts = sd.target >= 0 ? sd.target : AnyFoe(s);
        if (!Targetable(ts)) { AddText(f.pos, "But it failed!", UI_DIM, 30); return; }
        DoTransform(s, ts, false);
        return;
    }
    if (!override && (m.metronome || m.copycat)) {
        const MoveDef* pick = nullptr;
        if (m.metronome) {
            std::vector<const MoveDef*> pool;
            for (const auto& c : MoveLibrary())
                if (InstantKind(c)) pool.push_back(&c);
            if (!pool.empty()) pick = pool[SimRand(0, (int)pool.size() - 1)];
        } else if (sd.target >= 0 && hasLastMove[sd.target] && InstantKind(lastMove[sd.target])) {
            pick = &lastMove[sd.target];
        }
        if (!pick) { AddText(f.pos, "But it failed!", UI_DIM, 30); return; }
        MoveDef use = *pick;
        AddText(f.pos, m.name + ": " + use.name + "!", TypeColor(use.type), 40);
        Execute(s, mi, first, &use);
        return;
    }
    if (m.selfKO && AbOnField(Ab::Damp)) {
        AddText(f.pos, "[Damp] " + m.name + " can't be used!", Color{255, 225, 130, 255}, 44);
        return;
    }
    if (m.name == "Aurora Veil" && Wx() != Weather::Snow) {   // only works in snow
        AddText(f.pos, "But it failed! (needs snow)", UI_DIM, 30);
        return;
    }
    if (first) { lastMove[s] = m; hasLastMove[s] = true; }
    f.sinceAct = 0;
    HitSpec h = Spec(s, m);
    Vector2 a = f.pendingAim;
    Color tc = TypeColor(m.type);
    int os = sd.target >= 0 ? sd.target : AnyFoe(s);
    bool enemy = Targetable(os);
    std::vector<int> foes = FoeSlots(s);
    Vector2 epos = enemy ? sides[os].Cur().pos : Vector2{0, 0};
    float edist = enemy ? Dist(f.pos, epos) : 9999.0f;
    // Weather accuracy: Thunder and Hurricane can't miss in rain (but often miss in harsh sun);
    // Blizzard can't miss in snow.
    bool sureHit = false;
    {
        bool stormMove = m.name == "Thunder" || m.name == "Hurricane";
        Weather wx = Wx();
        if (enemy && edist < m.range + 60 && ((stormMove && wx == Weather::Rain) || (m.name == "Blizzard" && wx == Weather::Snow))) {
            a = SafeNorm(epos - f.pos, a);
            sureHit = true;
        } else if (stormMove && wx == Weather::Sun && Chance(0.5f)) {
            a = Rot(a, Chance(0.5f) ? 0.32f : -0.32f);
        }
    }

    if (first) {
        if (m.hits > 1 && (m.kind == MoveKind::Projectile || m.kind == MoveKind::Spread || m.kind == MoveKind::Melee)) {
            f.volleyMove = mi;
            f.volleyLeft = m.hits - 1 + (AbOf(f) == Ab::SkillLink ? 1 : 0);
            f.volleyT = 0.11f;
        }
        if (m.recharge > 0) f.rechargeT = m.recharge + 0.1f;
        if (m.selfDrop) {
            f.dropMul[(int)m.selfStat] = 0.67f;
            f.dropT[(int)m.selfStat] = 6;
        }
        PlaySfx(m.kind == MoveKind::Buff ? Sfx::Buff : (m.kind == MoveKind::Heal ? Sfx::Heal : (m.kind == MoveKind::Shield ? Sfx::Shield : SoundFor(m))));
    }
    CastVfx(s, m, h, a);

    // Lobs, zones and rain land on the enemy if they are roughly in the aim direction.
    auto placeDist = [&](float maxD) {
        if (!enemy) return maxD;
        if (edist < maxD + 30 && Dot(SafeNorm(epos - f.pos), a) > 0.85f) return std::min(edist, maxD);
        return maxD;
    };

    if (m.scope && (m.kind == MoveKind::Buff || m.kind == MoveKind::Heal || m.kind == MoveKind::Shield)) {
        TeamStatusMove(s, m);   // moves that help the whole team (or only the partner) in a double battle
        return;
    }
    switch (m.kind) {
        case MoveKind::Projectile:
            SpawnProjectile(h, m, f.pos + a * 11.0f, a);
            break;
        case MoveKind::Spread:
            for (int k = -1; k <= 1; k++) SpawnProjectile(h, m, f.pos + a * 11.0f, Rot(a, k * 0.3f));
            break;
        case MoveKind::Melee: {
            Fx fx;
            fx.kind = FxKind::Slash;
            fx.pos = f.pos;
            fx.dir = a;
            fx.maxLife = 0.3f;
            fx.radius = m.range;
            fx.col = tc;
            fx.look = h.look;
            fx.type = m.type;
            fx.seed = (uint32_t)SimRand(1, 0x3fffffff);
            fxs.push_back(fx);
            f.knock += a * 50.0f;
            Vector2 c = f.pos + a * (m.range * 0.7f);
            ObstaclesInArea(c, m.radius, h, true, f.pos);
            for (int ts : foes) {
                if (!Hittable(ts) || Dist(c, sides[ts].Cur().pos) >= m.radius + HitR(sides[ts].Cur())) continue;
                Vector2 tp = sides[ts].Cur().pos;
                ApplyHit(h, ts, a);
                TryElectrify(tp, h);
            }
        } break;
        case MoveKind::Dash: {
            float t = m.quick ? 0.12f : 0.22f;
            f.dashing = true;
            f.dashT = t;
            f.dashVel = a * (m.range / t);
            f.dashHit = false;
            f.dashObsHit = false;
            f.dashMove = mi;
        } break;
        case MoveKind::Burst: {
            Fx fx;
            fx.kind = FxKind::Ring;
            fx.pos = f.pos;
            fx.maxLife = 0.35f;
            fx.radius = m.radius;
            fx.col = tc;
            fx.look = h.look;
            if (GetLook(h.look, m.type).cast != Cast::Explosion) fxs.push_back(fx);
            if (m.power > 0) Shake(1);
            if (m.power > 0) ObstaclesInArea(f.pos, m.radius, h, true, f.pos);
            for (int ts : foes)
                if (Hittable(ts) && Dist(f.pos, sides[ts].Cur().pos) < m.radius + HitR(sides[ts].Cur()))
                    ApplyHit(h, ts, SafeNorm(sides[ts].Cur().pos - f.pos));
            if (m.power > 0) FieldReact(f.pos, h, m.radius);
        } break;
        case MoveKind::Beam: {
            float blen = BeamLength(f.pos, a, m.range, h);
            Vector2 end = f.pos + a * blen;
            BeamVfx(h, f.pos, end, m.radius);
            Shake(2);
            bool bhit = false;
            for (int ts : foes) {
                if (!Hittable(ts) || DistPointSeg(sides[ts].Cur().pos, f.pos, end) >= m.radius + HitR(sides[ts].Cur())) continue;
                Vector2 tp = sides[ts].Cur().pos;
                ApplyHit(h, ts, a);
                if (!bhit) TryElectrify(tp, h);
                bhit = true;
            }
            if (!bhit) TryElectrify(end, h);
            FieldReactLine(f.pos, end, h);
        } break;
        case MoveKind::Lob: {
            float d = placeDist(m.range);
            Projectile p;
            p.hit = h;
            p.lob = true;
            p.start = f.pos;
            p.target = ClampToArena(f.pos + a * d, 4);
            p.dur = 0.55f + d / 600.0f;
            p.pos = f.pos;
            p.radius = m.radius;
            projs.push_back(p);
        } break;
        case MoveKind::Zone: {
            float d = placeDist(m.range);
            Hazard hz;
            hz.hit = h;
            hz.hit.light = true;
            hz.pos = ClampToArena(f.pos + a * d, 4);
            hz.radius = m.radius;
            hz.life = hz.maxLife = m.duration;
            hz.tickT = 0.15f;
            hazards.push_back(hz);
            Burst(hz.pos, tc, 10, 40, 2);
            FieldReact(hz.pos, h, m.radius);
        } break;
        case MoveKind::Buff: {
            if (m.amount <= 1.0f) {  // Splash
                if (!m.haze) AddText(f.pos, "But nothing happened!", UI_DIM, 30);
                break;
            }
            f.buffMul[(int)m.stat] = m.name == "Growth" && Wx() == Weather::Sun ? 1 + (m.amount - 1) * 2 : m.amount;   // doubled in sun
            f.buffT[(int)m.stat] = m.duration;
            AddText(f.pos, std::string(StatName(m.stat)) + " up!", tc);
            if (m.stat2 >= 0 && m.stat2 < 3) {
                f.buffMul[m.stat2] = m.name == "Growth" && Wx() == Weather::Sun ? 1 + (m.amount - 1) * 2 : m.amount;
                f.buffT[m.stat2] = m.duration;
                AddText(f.pos, std::string(StatName((StatId)m.stat2)) + " up!", tc, 36);
            }
            for (int i = 0; i < 12; i++) {
                Particle p;
                p.pos = f.pos + Vector2{Frand(-8, 8), Frand(-4, 4)};
                p.z = Frand(0, 8);
                p.vz = Frand(30, 60);
                p.life = p.maxLife = 0.6f;
                p.col = tc;
                p.size = 1;
                parts.push_back(p);
            }
        } break;
        case MoveKind::Heal: {
            float amount = m.amount;
            Weather wx = Wx();
            if (m.name == "Synthesis" || m.name == "Moonlight" || m.name == "Morning Sun") {   // more in sun, less in other weather
                if (wx == Weather::Sun) amount *= 1.33f;
                else if (wx != Weather::None) amount *= 0.5f;
            }
            if (m.name == "Shore Up" && wx == Weather::Sand) amount *= 1.33f;
            int amt = (int)(f.maxHp * amount);
            amt = std::max(0, std::min(amt, f.maxHp - (int)f.hp));
            f.hp = std::min((float)f.maxHp, f.hp + amt);
            AddText(f.pos, "+" + std::to_string(amt), Color{120, 240, 130, 255});
            if (m.cure && f.st.major != Status::None) {
                f.st.major = Status::None;
                AddText(f.pos, "Status cured!", UI_TEXT, 44);
            }
            if (m.status == Status::Sleep) {  // Rest
                f.st.major = Status::Sleep;
                f.st.majorT = 2.5f;
                AddText(f.pos, "Fell asleep!", StatusColor(Status::Sleep), 36);
            }
            for (int i = 0; i < 14; i++) {
                Particle p;
                p.pos = f.pos + Vector2{Frand(-9, 9), Frand(-4, 4)};
                p.z = Frand(0, 10);
                p.vz = Frand(25, 50);
                p.life = p.maxLife = 0.7f;
                p.col = Color{140, 250, 150, 255};
                p.size = 2;
                parts.push_back(p);
            }
        } break;
        case MoveKind::Chain: {
            bool hit = enemy && edist <= m.range;
            Vector2 end = hit ? epos : f.pos + a * m.range;
            if (hit) {   // obstacles (pillars, snow mounds) block the bolt
                Vector2 dir = SafeNorm(epos - f.pos, a);
                float bl = BeamLength(f.pos, dir, edist, h);
                if (bl < edist - 2) { hit = false; end = f.pos + dir * bl; }
            }
            Fx fx;
            fx.kind = m.type == Type::Electric ? FxKind::Bolt : FxKind::Mind;
            fx.pos = f.pos;
            fx.dir = end;
            fx.maxLife = fx.kind == FxKind::Bolt ? 0.32f : 0.4f;
            fx.col = GetLook(h.look, m.type).glow;
            fx.radius = 14;
            fx.look = h.look;
            fx.type = m.type;
            fx.seed = (uint32_t)SimRand(1, 0x3fffffff);
            fxs.push_back(fx);
            if (fx.kind == FxKind::Bolt && h.power >= 80) AddFx(FxKind::Flash, end, 0.1f, Color{255, 250, 200, 255}, 30);
            if (hit) ApplyHit(h, os, SafeNorm(epos - f.pos));
            if (hit && m.abFx && Targetable(os)) AbilityMove(s, os, m.abFx);
            TryElectrify(end, h);
        } break;
        case MoveKind::Wave: {
            ShockWave w;
            w.hit = h;
            w.origin = f.pos;
            w.maxR = m.radius;
            w.speed = m.speed;
            waves.push_back(w);
            Shake(1, 0.3f);
            FieldReact(f.pos, h, m.radius * 0.6f);
        } break;
        case MoveKind::Rain: {
            float d = placeDist(m.range);
            Vector2 center = ClampToArena(sureHit ? epos : f.pos + a * d, 4);
            for (int i = 0; i < m.count; i++) {
                Projectile p;
                p.hit = h;
                p.drop = true;
                if (sureHit) p.homing = 1;   // follows the target down
                float ang = Frand(0, 2 * PI_F), r = (m.count == 1) ? 0.0f : Frand(0, m.radius * (sureHit ? 0.6f : 1.0f));
                p.target = p.pos = ClampToArena(center + Vector2{std::cos(ang) * r, std::sin(ang) * r}, 4);
                p.dur = m.duration;
                p.t = -i * 0.12f;
                p.z = 110;
                p.radius = m.blast;
                p.spin = Frand(0, 6);
                projs.push_back(p);
            }
        } break;
        case MoveKind::Blink: {
            Fx puff;
            puff.kind = FxKind::Puff;
            puff.pos = f.pos;
            puff.maxLife = 0.35f;
            puff.col = tc;
            fxs.push_back(puff);
            if (m.power > 0 && enemy && edist <= m.range + 20) {
                Vector2 dirTo = SafeNorm(epos - f.pos);
                f.pos = ClampToArena(epos + dirTo * 18.0f, BODY_R + 2);
                f.aim = -dirTo;
                f.dir = DirFromVec(f.aim);
                ApplyHit(h, os, dirTo);
            } else if (m.power > 0) {
                f.pos = ClampToArena(f.pos + a * (m.range * 0.6f), BODY_R + 2);
            } else {
                Vector2 away = enemy ? SafeNorm(f.pos - epos, -a) : -a;
                f.pos = ClampToArena(f.pos + away * m.range, BODY_R + 2);
            }
            puff.pos = f.pos;
            fxs.push_back(puff);
        } break;
        case MoveKind::Shield:
            if (m.endure) f.endureT = m.duration;
            else f.shieldT = m.duration;
            if (m.counter) f.counterT = m.duration;
            AddText(f.pos, m.name + "!", tc, 30);
            break;
        case MoveKind::Weather: {
            weather = m.weather;
            weatherT = m.duration;
            weatherTick = 1.0f;
            const char* msg = weather == Weather::Rain ? "It started to rain!" : weather == Weather::Sun ? "The sunlight turned harsh!"
                            : weather == Weather::Sand ? "A sandstorm kicked up!" : "It started to snow!";
            AddText({ARENA_W / 2, ARENA_H * 0.2f}, msg, tc, 20);
        } break;
        case MoveKind::Burrow: {
            f.burrowed = true;
            f.burrowT = m.duration;
            f.burrowMove = mi;
            Vector2 target = enemy ? epos : f.pos + a * m.range;
            Vector2 to = target - f.pos;
            if (Len(to) > m.range) target = f.pos + SafeNorm(to) * m.range;
            f.burrowTarget = ClampToArena(target, BODY_R + 2);
            Burst(f.pos, m.type == Type::Flying ? WHITE : Color{150, 120, 80, 255}, 12, 60, 2);
        } break;
        case MoveKind::Leap: {
            f.burrowed = true;
            f.leaping = true;
            f.burrowT = m.duration;
            f.burrowMove = mi;
            Vector2 target = enemy ? epos : f.pos + a * m.range;
            Vector2 to = target - f.pos;
            if (Len(to) > m.range) target = f.pos + SafeNorm(to) * m.range;
            f.burrowTarget = ClampToArena(target, BODY_R + 2);
            Burst(f.pos, WHITE, 8, 40, 2);
        } break;
        case MoveKind::Boomerang: {
            Projectile p;
            p.hit = h;
            p.pos = f.pos + a * 11.0f;
            p.vel = a * m.speed;
            p.maxDist = m.range;
            p.radius = m.radius;
            p.boomer = true;
            projs.push_back(p);
        } break;
        case MoveKind::Orbit: {
            for (auto& hz : hazards)
                if (hz.orbit && hz.hit.side == s) hz.life = 0;  // replaces your old orbs
            int n = std::max(1, m.count);
            for (int i = 0; i < n; i++) {
                Hazard hz;
                hz.hit = h;
                hz.orbit = true;
                hz.orbitAngle = i * 2 * PI_F / n;
                hz.orbitDist = m.range;
                hz.orbitSpeed = m.speed;
                hz.radius = m.radius;
                hz.life = hz.maxLife = m.duration;
                hz.pos = f.pos;
                hazards.push_back(hz);
            }
            Burst(f.pos, tc, 10, 40, 6);
        } break;
        case MoveKind::Wall: {
            Vector2 nrm{-a.y, a.x};
            int segs = std::max(1, m.count);
            for (int i = 0; i < segs; i++) {
                float off = (i - (segs - 1) / 2.0f) * m.radius * 1.9f;
                Obstacle o;
                o.kind = Obstacle::TempWall;
                o.pos = ClampToArena(f.pos + a * m.range + nrm * off, 6);
                o.radius = m.radius;
                o.height = 18;
                o.life = o.maxLife = m.duration;
                o.wallType = m.type;
                field.obs.push_back(o);
            }
            Burst(f.pos + a * m.range, tc, 14, 50, 6);
        } break;
        case MoveKind::Trap: {
            int mine = 0;
            for (const auto& hz : hazards) mine += hz.trap && hz.hit.side == s && hz.life > 0;
            if (mine >= 3)
                for (auto& hz : hazards)
                    if (hz.trap && hz.hit.side == s && hz.life > 0) { hz.life = 0; break; }  // oldest goes
            float d = placeDist(m.range);
            Hazard hz;
            hz.hit = h;
            hz.trap = true;
            hz.pos = ClampToArena(f.pos + a * d, 6);
            hz.radius = m.radius;
            hz.life = hz.maxLife = m.duration;
            hz.armT = 0.4f;
            hazards.push_back(hz);
            Burst(hz.pos, tc, 6, 30, 2);
        } break;
        case MoveKind::Vortex: {
            float d = placeDist(m.range);
            Hazard hz;
            hz.hit = h;
            hz.hit.light = true;
            hz.pos = ClampToArena(f.pos + a * d, 4);
            hz.radius = m.radius;
            hz.life = hz.maxLife = m.duration;
            hz.tickT = 0.15f;
            hz.pull = m.amount;
            hazards.push_back(hz);
            Burst(hz.pos, tc, 12, 40, 2);
        } break;
        case MoveKind::Cone: {
            Fx fx;
            fx.kind = FxKind::Cone;
            fx.pos = f.pos;
            fx.dir = a;
            fx.len = m.range;
            fx.radius = m.radius;
            fx.maxLife = 0.35f;
            fx.col = tc;
            fxs.push_back(fx);
            float half = m.radius * DEG2RAD;
            {
                const Look& CL = GetLook(h.look, m.type);
                PK ck = CL.trail == PK::Smoke ? PK::Smoke : (CL.trail == PK::Ring ? PK::Ring : CL.trail);
                for (int i = 0; i < 22; i++) {
                    Particle& p = Emit(ck, f.pos + a * 8.0f, 8, Rot(a, Frand(-half, half)) * Frand(m.range * 2.0f, m.range * 3.2f), Frand(-5, 15),
                                       Frand(0.25f, 0.4f), ck == PK::Glow ? 1.2f : 2.0f, ColorBrightness(CL.glow, Frand(-0.2f, 0.4f)), false);
                    p.spin = Frand(-8, 8);
                    if (ck == PK::Ring) { p.vel *= 0.6f; p.grow = 14; p.size = 2; }
                    if (ck == PK::Smoke) { p.grow = 10; }
                }
            }
            for (int ts : foes) {
                if (!Hittable(ts)) continue;
                Vector2 tp = sides[ts].Cur().pos;
                float d2 = Len(Vector2{tp.x - f.pos.x, (tp.y - f.pos.y) * 0.7f});  // same squashed space as bodies
                if (d2 < m.range + HitR(sides[ts].Cur()) && (d2 < 14 || Dot(SafeNorm(tp - f.pos), a) > std::cos(half + 0.15f))) {
                    ApplyHit(h, ts, SafeNorm(tp - f.pos));
                    TryElectrify(tp, h);
                }
            }
        } break;
        default: break;
    }
    if (m.haze) {
        for (int k = 0; k < N(); k++) {
            Fighter& x = sides[k].Cur();
            for (int st = 0; st < 3; st++) { x.buffMul[st] = x.dropMul[st] = 1; x.buffT[st] = x.dropT[st] = 0; }
        }
        AddText(f.pos, "All stat changes were wiped away!", tc, 34);
    }
    if (first && m.payDay && m.power > 0) {
        payDayCoins += std::max(1, f.level * 2);
        for (int i = 0; i < 6; i++) {
            Particle p;
            p.pos = (enemy ? epos : f.pos) + Vector2{Frand(-8, 8), Frand(-4, 4)};
            p.vz = Frand(40, 70);
            p.grav = true;
            p.life = p.maxLife = 0.7f;
            p.col = Color{255, 210, 60, 255};
            p.size = 2;
            parts.push_back(p);
        }
    }
    if (first && m.selfKO && f.state == FState::Active) {
        f.hp = 0;
        Faint(s, -1);
        return;
    }
    if (first && m.power > 0 && f.Holds(ItemFx::LifeOrb) && f.state == FState::Active)
        DirectDamage(s, f.maxHp * 0.015f, Color{200, 120, 220, 255}, os);
}

bool Battle::Targetable(int s) const {
    if (s < 0 || s >= N()) return false;
    const Fighter& f = sides[s].Cur();
    return f.state == FState::Active && f.hp > 0 && !f.burrowed;
}

bool Battle::Hittable(int s) const { return Targetable(s) && sides[s].Cur().dodgeT <= 0; }

float Battle::ApplyHit(const HitSpec& h, int ts, Vector2 dir) {
    if (!Targetable(ts)) return 0;
    if (sides[ts].Cur().dodgeT > 0 && (h.side < 0 || Foes(h.side, ts))) return 0;   // dodged
    Fighter& t = sides[ts].Cur();
    const float hpBefore = t.hp;
    bool critHit = false;

    if (t.shieldT > 0) {
        if (t.counterT > 0 && !h.light && h.power > 0 && h.side >= 0 && sides[h.side].active == h.fighter &&
            sides[h.side].Cur().state == FState::Active) {
            const Fighter& at = sides[h.side].Cur();
            float back = ((22.0f * h.power * t.Stat(StatId::Atk, h.special) / at.Stat(StatId::Def, h.special)) / 50.0f + 2.0f) * 1.5f * StatMul(t.level);
            AddText(t.pos, "Counter!", UI_GOLD, 34);
            DirectDamage(h.side, back, UI_GOLD, ts, true);
            return 0;
        }
        if (!h.light) {
            AddText(t.pos, "Protected!", UI_TEXT, 30);
            Burst(t.pos, WHITE, 6, 40);
            PlaySfx(Sfx::Shield);
        }
        return 0;
    }
    if (AbBlocksHit(h, ts)) return 0;
    float eff = TypeEffectiveness(h.type, t.sp->type1, t.sp->type2);
    if (eff <= 0 && h.ab == Ab::Scrappy && (h.type == Type::Normal || h.type == Type::Fighting) && HasType(t.sp, Type::Ghost))
        eff = TypeEffectiveness(h.type, t.sp->type1 == Type::Ghost ? t.sp->type2 : t.sp->type1, Type::None);
    if (eff <= 0) {
        if (!h.light || t.flash <= 0) AddText(t.pos, "No effect", UI_DIM);
        t.flash = 0.05f;
        Burst(t.pos, Color{170, 170, 180, 255}, 4, 25);
        return 0;
    }

    float dmg = 0, landed = 0;   // landed: HP actually lost (no overkill)
    if (h.ohko) {  // one-hit KO moves
        if (Chance(0.3f) && h.attLevel >= t.level) {
            dmg = t.hp;
            t.hp = 0;
            t.flash = 0.2f;
            AddText(t.pos, "It's a one-hit KO!", UI_GOLD, 46);
            Shake(4, 0.3f);
        } else {
            AddText(t.pos, "Missed!", UI_DIM, 30);
        }
    } else if (h.fixedDmg != 0) {  // Dragon Rage, Seismic Toss, Super Fang...
        dmg = h.fixedDmg > 0 ? h.fixedDmg * 2.5f : (h.fixedDmg == -1 ? h.attLevel * 2.5f : std::max(1.0f, std::floor(t.hp / 2)));
        if (h.falseSwipe && t.hp > 1) dmg = std::min(dmg, t.hp - 1);
        dmg = std::round(dmg);
        landed = std::min(dmg, std::max(0.0f, t.hp));
        t.dmgTaken += landed;
        t.hp -= dmg;
        t.flash = 0.14f;
        if (!h.light) {
            t.knock = dir * 90.0f * h.knock;
            t.hitstun = 0.15f;
            t.pendingMove = -1;
            Shake(2);
        }
        HitVfx(h, t.pos, 9, dir);
        if (setup.damageNumbers) AddText(t.pos, std::to_string((int)dmg), WHITE, 22);
        PlaySfx(Sfx::Hit);
    } else if (h.power > 0) {
        float power = h.power * ((h.hex && t.st.major != Status::None) ? 2.0f : 1.0f);
        float stab = (h.type == h.t1 || h.type == h.t2) ? 1.5f : 1.0f;
        Ab tAb = h.ab == Ab::MoldBreaker ? Ab::None : AbOf(t);
        bool crit = !h.light && tAb != Ab::ShellArmor && tAb != Ab::BattleArmor && t.chantT <= 0 && Chance((h.highCrit ? 0.25f : 0.0625f) + (h.scope ? 0.15f : 0.0f));
        // Snow: Ice types' Defense x1.5. Sandstorm: Rock types' Sp. Def x1.5.
        float wDef = (Wx() == Weather::Snow && !h.special && HasType(t.sp, Type::Ice)) || (Wx() == Weather::Sand && h.special && HasType(t.sp, Type::Rock)) ? 1.5f : 1.0f;
        float raw = (22.0f * power * h.atk / (t.Stat(StatId::Def, h.special) * wDef)) / 50.0f + 2.0f;
        float itemMul = h.dmgMul * (eff > 1 ? h.superMul : 1.0f);
        if (Wx() == Weather::Rain) itemMul *= h.type == Type::Water ? 1.5f : (h.type == Type::Fire ? 0.5f : 1.0f);
        if (Wx() == Weather::Sun) itemMul *= h.type == Type::Fire ? 1.5f : (h.type == Type::Water ? (h.name == "Hydro Steam" ? 1.5f : 0.5f) : 1.0f);
        itemMul *= AbDamageMul(h, ts, eff, crit);
        critHit = crit;
        if (crit || eff > 1) hype = std::max(hype, 0.55f);   // the crowd loves a big hit
        if (h.brine && t.hp < t.maxHp * 0.5f) itemMul *= 2;
        dmg = std::max(1.0f, std::round(raw * stab * eff * itemMul * (crit ? 1.5f : 1.0f) * Frand(0.9f, 1.0f) * h.lvlMul));
        if (h.falseSwipe && t.hp > 1) dmg = std::min(dmg, t.hp - 1);
        landed = std::min(dmg, std::max(0.0f, t.hp));
        t.dmgTaken += landed;
        t.hp -= dmg;
        t.flash = 0.14f;

        if (!h.light) {
            t.knock = dir * (70.0f + power * 1.3f) * h.knock;
            t.hitstun = 0.12f + power / 800.0f;
            t.pendingMove = -1;
            t.dashing = false;
            t.volleyLeft = 0;
            Shake(eff > 1 || crit ? 3 : 2);
            if (t.st.major == Status::Sleep) { t.st.major = Status::None; AddText(t.pos, "Woke up!", UI_TEXT, 44); }
        }
        if (t.st.major == Status::Freeze && h.type == Type::Fire) { t.st.major = Status::None; AddText(t.pos, "Thawed out!", UI_TEXT, 44); }

        HitVfx(h, t.pos, 9, dir);
        if (setup.damageNumbers) {
            Color dc = eff > 1 ? UI_GOLD : (eff < 1 ? UI_DIM : WHITE);
            AddText(t.pos, std::to_string((int)dmg), dc, 22);
        }
        if (!h.light) {
            if (crit) AddText(t.pos, "Critical hit!", Color{255, 150, 90, 255}, 46);
            if (eff >= 2) AddText(t.pos, eff >= 4 ? "Super effective!!" : "Super effective!", UI_GOLD, 34);
            else if (eff < 1) AddText(t.pos, "Not very effective...", UI_DIM, 34);
            PlaySfx(crit ? Sfx::Crit : (eff > 1 ? Sfx::HitSuper : (eff < 1 ? Sfx::HitWeak : Sfx::Hit)));
        }

        if (h.drain > 0 && h.side >= 0 && sides[h.side].active == h.fighter) {
            Side& as = sides[h.side];
            Fighter& a = as.Cur();
            if (a.state == FState::Active && a.hp > 0 && AbOf(t) == Ab::LiquidOoze) {
                AddText(t.pos, "[Liquid Ooze] It sucked up the ooze!", Color{255, 225, 130, 255}, 40);
                DirectDamage(h.side, dmg * h.drain, Color{170, 110, 210, 255}, ts);
            } else if (a.state == FState::Active && a.hp > 0) {
                float heal = std::min((float)a.maxHp - a.hp, dmg * h.drain);
                if (heal >= 1) {
                    a.hp += heal;
                    AddText(a.pos, "+" + std::to_string((int)heal), Color{120, 240, 130, 255}, 24);
                }
            }
        }
    }

    if (t.hp <= 0 && dmg > 0 && t.endureT > 0) {
        t.hp = 1;
        AddText(t.pos, t.sp->name + " endured the hit!", UI_GOLD, 52);
    }
    if (t.hp <= 0 && dmg > 0 && t.Holds(ItemFx::FocusBand) && !t.focusUsed) {
        t.hp = 1;
        t.focusUsed = true;
        AddText(t.pos, t.sp->name + " hung on with its Focus Band!", UI_GOLD, 52);
    }
    AbAfterHit(h, ts, dmg, critHit, hpBefore);
    bool shieldDust = AbOf(t) == Ab::ShieldDust && h.ab != Ab::MoldBreaker && h.power > 0;
    if (t.hp > 0 && !shieldDust && t.state == FState::Active) {
        if (h.flinchChance > 0 && dmg > 0 && !h.light && Chance(h.flinchChance)) TryStatus(ts, Status::Flinch, h.side);
        if (h.status != Status::None && Chance(h.statusChance)) TryStatus(ts, h.status, h.side);
        if (h.lowers && t.mistT > 0 && Chance(h.lowerChance)) AddText(t.pos, "The mist protected it!", UI_DIM, 36);
        else if (h.lowers && Chance(h.lowerChance) && !AbBlocksLower(ts, h.lowerStat, h.side, h.name)) {
            if (t.dropT[(int)h.lowerStat] <= 0) AddText(t.pos, std::string(StatName(h.lowerStat)) + " fell!", UI_DIM, 40);
            t.dropMul[(int)h.lowerStat] = 0.67f;
            t.dropT[(int)h.lowerStat] = 6;
            AbLowered(ts, h.side);
        }
    }
    if (h.side >= 0 && h.side < N() && dmg > 0) {
        t.lastHitBy = h.side;
        sides[h.side].damageDealt += dmg;
        if (Foes(h.side, ts)) { sides[h.side].Cur().dmgDealt += landed; t.hurtT = 0; t.hitFresh = true; }
    }
    if (t.hp <= 0) Faint(ts, h.side >= 0 ? h.side : AnyFoe(ts));
    else CheckWeakened(ts);
    if (h.forceSwap && t.hp > 0 && t.state == FState::Active && !t.weakened) {  // Roar, Whirlwind, Dragon Tail
        Side& tsd = sides[ts];
        int n = tsd.NextAlive(tsd.active);
        if (n >= 0) {
            t.state = FState::Recalling;
            t.stateT = 0;
            t.pendingMove = -1;
            t.dashing = false;
            tsd.swapTo = n;
            AddText(t.pos, t.sp->name + " was blown away!", TeamColor(sides[ts].teamId), 40);
        }
    }

    if (h.contact && dmg > 0 && h.side >= 0 && t.Holds(ItemFx::RockyHelmet)) {
        Side& as = sides[h.side];
        if (as.active == h.fighter && as.Cur().state == FState::Active)
            DirectDamage(h.side, as.Cur().maxHp * t.item->value, Color{200, 170, 140, 255}, ts);
    }
    if (h.recoil > 0 && dmg > 0 && h.side >= 0) {
        Side& as = sides[h.side];
        if (as.active == h.fighter && as.Cur().state == FState::Active) DirectDamage(h.side, dmg * h.recoil, UI_DIM, ts);
    }
    return dmg;
}

void Battle::TryStatus(int ts, Status s, int src) {
    Fighter& t = sides[ts].Cur();
    if (ImmuneTo(t.sp, s)) return;
    if (s == Status::Freeze && Wx() == Weather::Sun) return;   // nothing freezes in harsh sunlight
    if (t.safeT > 0 && src >= 0 && src < N() && Foes(src, ts)) {   // Safeguard
        if (IsMajorStatus(s) || s == Status::Confuse) { AddText(t.pos, "Safeguard protected it!", UI_DIM, 36); return; }
    }
    if (AbBlocksStatus(ts, s, src)) return;
    StatusState& st = t.st;
    if (IsMajorStatus(s)) {
        if (st.major != Status::None) return;
        if (AbOf(t) == Ab::Synchronize && src >= 0 && src < N() && Foes(src, ts) &&
            (s == Status::Burn || s == Status::Poison || s == Status::Toxic || s == Status::Paralyze)) {
            st.major = s;   // set first so the copy can't bounce back
            AddText(t.pos, "[Synchronize] It passed the condition on!", Color{255, 225, 130, 255}, 52);
            TryStatus(src, s, ts);
            st.major = Status::None;
        }
        st.major = s;
        st.tickT = 0.5f;
        st.toxicN = 0;
        st.paraT = 0.8f;
        switch (s) {
            case Status::Burn: st.majorT = 4; break;
            case Status::Poison: st.majorT = 5; break;
            case Status::Toxic: st.majorT = 6; break;
            case Status::Paralyze: st.majorT = 4; break;
            case Status::Freeze: st.majorT = 1.2f; break;
            case Status::Sleep: st.majorT = 2.0f; break;
            default: break;
        }
        const char* msg = s == Status::Burn ? "Burned!" : s == Status::Paralyze ? "Paralyzed!" : s == Status::Freeze ? "Frozen solid!" :
                          s == Status::Sleep ? "Fell asleep!" : s == Status::Toxic ? "Badly poisoned!" : "Poisoned!";
        AddText(t.pos, msg, StatusColor(s), 40);
        StatusVfx(t.pos, s);
        PlaySfx(Sfx::Status);
    } else if (s == Status::Confuse) {
        if (st.confuseT <= 0) { AddText(t.pos, "Confused!", StatusColor(s), 40); StatusVfx(t.pos, s); }
        st.confuseT = t.Holds(ItemFx::ConfuseGuard) ? 3 * t.item->value : 3;
        PlaySfx(Sfx::Status);
    } else if (s == Status::Flinch) {
        st.flinchT = 0.45f;
        if (AbOf(t) == Ab::Steadfast) {
            t.buffMul[2] = std::max(t.buffMul[2], 1.5f);
            t.buffT[2] = std::max(t.buffT[2], 6.0f);
        }
        AddText(t.pos, "Flinched!", UI_TEXT, 40);
    } else if (s == Status::Seed) {
        if (st.seedT <= 0) { AddText(t.pos, "Seeded!", StatusColor(s), 40); StatusVfx(t.pos, s); }
        st.seedT = 5;
        st.seedSide = src;
        PlaySfx(Sfx::Status);
    }
}

void Battle::DirectDamage(int s, float amount, Color c, int creditSide, bool direct) {
    Fighter& f = sides[s].Cur();
    if (f.state != FState::Active || f.hp <= 0) return;
    if (!direct && AbOf(f) == Ab::MagicGuard) return;
    float a = std::max(1.0f, std::round(amount));
    a = std::min(a, std::max(0.0f, f.hp));
    f.hp -= a;
    f.dmgTaken += a;
    if (creditSide >= 0 && creditSide < N() && Foes(creditSide, s)) sides[creditSide].Cur().dmgDealt += a;
    if (f.hp < 1 && f.endureT > 0) f.hp = 1;
    if (setup.damageNumbers) AddText(f.pos, std::to_string((int)a), c, 20);
    if (f.hp <= 0) Faint(s, creditSide);
    else CheckWeakened(s);
}

// Status moves with a scope (MoveDef::scope): who they help, as in the games' double battles.
void Battle::TeamStatusMove(int s, const MoveDef& m) {
    Fighter& f = sides[s].Cur();
    Color tc = TypeColor(m.type);
    std::vector<int> who;
    for (int k = 0; k < N(); k++) {
        const Fighter& x = sides[k].Cur();
        if (x.state != FState::Active || x.hp <= 0) continue;
        bool ally = k != s && !Foes(s, k);
        switch (m.scope) {
            case 1: if (k == s || ally) who.push_back(k); break;
            case 2: if (ally) who.push_back(k); break;
            case 3: if (HasType(x.sp, Type::Grass) && (m.name != "Rototiller" || !IsAirborne(*x.sp))) who.push_back(k); break;
            case 4: if (k == s) who.push_back(k); break;
            default: break;
        }
    }
    if (who.empty()) {
        AddText(f.pos, m.scope == 2 ? "But it failed! (no partner to help)" : "But it failed!", UI_DIM, 30);
        return;
    }
    for (int k : who) {
        Fighter& x = sides[k].Cur();
        if (k != s) {   // a little stream of light to each Pokemon it helps
            for (int i = 0; i < 8; i++) {
                Vector2 p = LerpV(f.pos, x.pos, i / 7.0f);
                Emit(PK::Spark, p, 10, {Frand(-10, 10), Frand(-10, 10)}, 20, 0.5f, 1.5f, tc);
            }
        }
        switch (m.kind) {
            case MoveKind::Buff: {
                if (m.amount <= 1.0f) { AddText(x.pos, m.name == "Hold Hands" ? "They held hands!" : "But nothing happened!", tc, 30); break; }
                float amt = m.name == "Growth" && Wx() == Weather::Sun ? 1 + (m.amount - 1) * 2 : m.amount;
                x.buffMul[(int)m.stat] = std::max(x.buffMul[(int)m.stat], amt);
                x.buffT[(int)m.stat] = m.duration;
                AddText(x.pos, std::string(StatName(m.stat)) + " up!", tc);
                if (m.stat2 >= 0 && m.stat2 < 3) {
                    x.buffMul[m.stat2] = std::max(x.buffMul[m.stat2], amt);
                    x.buffT[m.stat2] = m.duration;
                    AddText(x.pos, std::string(StatName((StatId)m.stat2)) + " up!", tc, 36);
                }
            } break;
            case MoveKind::Heal: {
                int amt = (int)(x.maxHp * m.amount);
                amt = std::max(0, std::min(amt, x.maxHp - (int)x.hp));
                x.hp = std::min((float)x.maxHp, x.hp + amt);
                if (amt > 0) AddText(x.pos, "+" + std::to_string(amt), Color{120, 240, 130, 255});
                if (m.cure && x.st.major != Status::None) { x.st.major = Status::None; AddText(x.pos, "Status cured!", UI_TEXT, 44); }
                for (int i = 0; i < 10; i++)
                    Emit(PK::Pixel, x.pos + Vector2{Frand(-9, 9), Frand(-4, 4)}, Frand(0, 10), {0, 0}, Frand(25, 50), 0.7f, 2, Color{140, 250, 150, 255});
            } break;
            case MoveKind::Shield:
                if (m.name == "Safeguard") { x.safeT = 6; AddText(x.pos, "Safeguard!", tc, 30); }
                else if (m.name == "Mist") { x.mistT = 6; AddText(x.pos, "Shrouded in mist!", tc, 30); }
                else if (m.name == "Lucky Chant") { x.chantT = 6; AddText(x.pos, "Lucky Chant!", tc, 30); }
                else if (m.scope == 4) { x.followT = 5; AddText(x.pos, m.name + "! Foes aim here!", tc, 30); }
                else { x.shieldT = std::max(x.shieldT, m.duration); AddText(x.pos, m.name + "!", tc, 30); }
                break;
            default: break;
        }
    }
}

void Battle::Confetti(int n) {
    const Color cols[] = {{240, 80, 80, 255}, {80, 160, 240, 255}, {250, 220, 80, 255}, {120, 210, 110, 255}, {240, 240, 240, 255}, GymColor(setup.arena)};
    for (int i = 0; i < n; i++) {
        Particle& p = Emit(PK::Pixel, {Frand(0, ARENA_W), Frand(0, ARENA_H)}, Frand(90, 160), {Frand(-20, 20), Frand(-10, 10)}, Frand(-30, 0),
                           Frand(1.8f, 3.0f), Frand(1.5f, 2.5f), cols[SimRand(0, 5)], true);
        p.drag = 1.5f;
        p.spin = Frand(-8, 8);
    }
}

// Practice dummies: a different type each time one is "knocked out", so every matchup can be tried.
static const char* kDummyCycle[] = {"Snorlax", "Geodude", "Squirtle", "Bulbasaur", "Charmander", "Pikachu", "Gastly",
                                    "Pidgey", "Machop", "Abra", "Dratini", "Jynx", "Onix", "Nidoking"};

void Battle::PracticeNextDummy(int s, bool announce) {
    Fighter& t = sides[s].Cur();
    int n = (int)(sizeof(kDummyCycle) / sizeof(kDummyCycle[0]));
    int cur = -1;
    for (int i = 0; i < n; i++) if (SpeciesIndex(kDummyCycle[i]) == t.species) cur = i;
    int ni = SpeciesIndex(kDummyCycle[(cur + 1 + n) % n]);
    if (ni >= 0) { t.species = ni; t.sp = &AllSpecies()[ni]; t.startSpecies = ni; }
    t.st = StatusState();
    t.RecalcHp(false);
    t.displayHp = t.hp;
    if (announce) AddText(t.pos, "Next dummy: " + t.sp->name, UI_GOLD, 44);
}

void Battle::PracticeCycleDummies() {
    for (int s = 0; s < N(); s++) if (sides[s].dummy) PracticeNextDummy(s, true);
}

void Battle::Faint(int s, int credit) {
    if (setup.practice) {   // nobody faints in practice: patch up (a dummy turns into the next type)
        Fighter& t = sides[s].Cur();
        if (credit >= 0 && credit < N() && Foes(s, credit)) sides[credit].Cur().kos++;
        if (sides[s].dummy) {
            FaintVfx(t.pos, t.sp->type1);
            PlaySfx(Sfx::Faint);
            AddText(t.pos, t.sp->name + " is down!", UI_GOLD, 44);
            PracticeNextDummy(s, true);
        } else {
            t.st = StatusState();
            t.RecalcHp(false);
            t.displayHp = t.hp;
            AddText(t.pos, "Knocked out - patched up!", UI_GOLD, 44);
        }
        return;
    }
    Fighter& t = sides[s].Cur();
    if (t.boss > 0 && setup.catchable && sides[s].teamId == 1 && !t.retired && t.state == FState::Active) {
        t.hp = 1;   // a legendary is never knocked out: it's weakened, then it's time to catch it
        CheckWeakened(s);
        return;
    }
    if (setup.arena != ArenaKind::Stadium && !IsLegendArena(setup.arena)) { Confetti(45); hype = 1; }   // the gym crowd goes wild
    if (t.state == FState::Fainting || t.state == FState::Fainted) return;
    t.hp = 0;
    t.state = FState::Fainting;
    t.stateT = 0;
    t.pendingMove = -1;
    t.dashing = false;
    t.burrowed = false;
    t.leaping = false;
    t.volleyLeft = 0;
    t.st = StatusState();
    AddText(t.pos, t.sp->name + " fainted!", TeamColor(sides[s].teamId), 44);
    FaintVfx(t.pos, t.sp->type1);
    Shake(3, 0.25f);
    PlaySfx(Sfx::Faint);

    CreditKo(s, credit);
}

void Battle::CreditKo(int s, int credit) {
    if (credit < 0 || credit >= N() || !Foes(s, credit)) return;
    const Fighter& t = sides[s].Cur();
    Fighter& k = sides[credit].Cur();
    if (k.hp > 0 && (k.state == FState::Active || k.state == FState::Entering)) { k.kos++; AbKo(credit); }
    // The active Pokemon of everyone earning XP on that team gets it (co-op: 60% each, since two
    // players face about twice as many foes).
    int earners = 0;
    for (int c = 0; c < N(); c++) earners += sides[c].teamId == sides[credit].teamId && sides[c].earnXp;
    int xp = (int)std::lround(KoXp(*t.sp, t.level, setup.trainerBattle) * (earners > 1 ? 0.6f : 1.0f));
    for (int c = 0; c < N(); c++) {
        if (sides[c].teamId != sides[credit].teamId || !sides[c].earnXp) continue;
        Fighter& r = sides[c].Cur();
        if (r.hp <= 0 || (r.state != FState::Active && r.state != FState::Entering)) continue;
        int got = r.Holds(ItemFx::LuckyEgg) ? (int)(xp * r.item->value) : xp;
        GiveXp(c, got);
    }
}

void Battle::CheckWeakened(int s) {
    if (!setup.catchable || sides[s].teamId != 1) return;
    Fighter& f = sides[s].Cur();
    if (f.weakened || f.hp <= 0 || f.state != FState::Active || f.hp > f.maxHp * 0.25f) return;
    f.weakened = true;
    f.dazeT = 1.2f;
    f.pendingMove = -1;
    f.dashing = false;
    f.volleyLeft = 0;
    f.burrowed = false;
    f.leaping = false;
    AddText(f.pos, f.sp->name + " is weakened!", UI_GOLD, 44);
    PlaySfx(Sfx::Status);
}

void Battle::Retire(int s) {
    Side& sd = sides[s];
    Fighter& f = sd.Cur();
    f.retired = true;
    f.state = FState::Fainted;
    f.stateT = 0;
    f.st = StatusState();
    sd.lastPos = f.pos;
    AddText(f.pos, f.sp->name + " retreated!", TeamColor(sides[s].teamId), 44);
    Fx puff;
    puff.kind = FxKind::Puff;
    puff.pos = f.pos;
    puff.maxLife = 0.45f;
    puff.col = WHITE;
    fxs.push_back(puff);
    Burst(f.pos, WHITE, 14, 60, 8);
    PlaySfx(Sfx::Shield);
    CreditKo(s, sides[s].Cur().lastHitBy >= 0 ? sides[s].Cur().lastHitBy : AnyFoe(s));
    if (sd.NextAlive(sd.active) >= 0) sd.sendT = 0.6f;
}

void Battle::GiveXp(int s, int amount) {
    Fighter& f = sides[s].Cur();
    if (f.level >= MAX_LEVEL) return;
    f.xp += amount;
    f.xpGained += amount;
    AddText(f.pos, "+" + std::to_string(amount) + " XP", Color{120, 190, 255, 255}, 36);
    while (f.level < MAX_LEVEL && f.xp >= XpForLevel(f.level + 1)) LevelUp(s);
}

void Battle::LevelUp(int s) {
    Fighter& f = sides[s].Cur();
    f.level++;
    f.RecalcHp(true);
    AddText(f.pos, "Level " + std::to_string(f.level) + "!", UI_GOLD, 44);
    PlaySfx(Sfx::LevelUp);
    for (int i = 0; i < 10; i++) {
        Particle p;
        p.pos = f.pos + Vector2{Frand(-8, 8), Frand(-4, 4)};
        p.vz = Frand(40, 70);
        p.life = p.maxLife = 0.8f;
        p.col = UI_GOLD;
        p.size = 2;
        parts.push_back(p);
    }
    LearnNewMoves(f, texts);

    int evo = f.transformFrom >= 0 ? -1 : EvolutionIndex(f.species);
    if (evo >= 0 && f.evolveTo < 0 && f.level >= f.sp->evolveLevel &&
        (f.state == FState::Active || f.state == FState::Entering)) {
        f.evolveTo = evo;
        f.state = FState::Evolving;
        f.stateT = 0;
        f.pendingMove = -1;
        f.dashing = false;
        f.burrowed = false;
        f.leaping = false;
        f.volleyLeft = 0;
        f.knock = {0, 0};
        AddText(f.pos, "What? " + f.sp->name + " is evolving!", WHITE, 58);
        PlaySfx(Sfx::Evolve);
    }
}

void Battle::FinishEvolution(int s) {
    Fighter& f = sides[s].Cur();
    std::string from = f.sp->name;
    f.species = f.evolveTo;
    f.sp = &AllSpecies()[f.species];
    f.evolveTo = -1;
    InitAbility(f, f.abSlot);
    f.RecalcHp(true);
    f.evolvedNames.push_back(from + " -> " + f.sp->name);
    f.state = FState::Active;
    f.stateT = 0;
    f.flash = 0.3f;
    AddText(f.pos, from + " evolved into " + f.sp->name + "!", UI_GOLD, 50);
    Burst(f.pos, WHITE, 30, 110, 10);
    Shake(2, 0.3f);
    LearnNewMoves(f, texts, true);
    int evo = EvolutionIndex(f.species);  // another evolution already due?
    if (evo >= 0 && f.level >= f.sp->evolveLevel) {
        f.evolveTo = evo;
        f.state = FState::Evolving;
        f.stateT = 0;
    }
}

void Battle::SpawnProjectile(const HitSpec& h, const MoveDef& m, Vector2 pos, Vector2 dir) {
    Projectile p;
    p.hit = h;
    p.bounces = m.bounces;
    p.pos = pos;
    p.vel = dir * m.speed;
    p.maxDist = m.range;
    p.radius = m.radius;
    p.homing = m.homing;
    if (h.ab == Ab::CompoundEyes) { p.homing += 1.2f; p.maxDist *= 1.2f; }
    if (h.ab == Ab::NoGuard) p.homing += 2.0f;
    p.spin = Frand(0, 6);
    projs.push_back(p);
}

void Battle::SpawnPatch(const HitSpec& src, Vector2 pos) {
    if (src.patchPower <= 0) return;
    Hazard hz;
    hz.hit = src;
    hz.hit.power = src.patchPower;
    hz.hit.light = true;
    hz.hit.lowers = false;
    hz.hit.statusChance = src.statusChance * 0.5f;
    hz.hit.drain = hz.hit.recoil = 0;
    hz.pos = ClampToArena(pos, 4);
    hz.radius = src.patchRadius;
    hz.life = hz.maxLife = src.patchTime;
    hz.tickT = 0.25f;
    hz.patch = true;
    hazards.push_back(hz);
}

void Battle::UpdateProjectiles(float dt) {
    for (size_t i = 0; i < projs.size(); i++) {
        if (!projs[i].alive) continue;
        Projectile p = projs[i];  // copy: spawning patches/boulders can't invalidate it
        Color tc = TypeColor(p.hit.type);
        p.spin += dt * 12;
        p.portalCd -= dt;

        auto landAt = [&](Vector2 at, float radius) {
            for (int ts = 0; ts < N(); ts++) {
                if (!Hits(p.hit.side, ts) || !Hittable(ts)) continue;
                Vector2 epos = sides[ts].Cur().pos;
                if (Dist(at, epos) < radius + HitR(sides[ts].Cur())) ApplyHit(p.hit, ts, SafeNorm(epos - at));
            }
            ObstaclesInArea(at, radius * 0.6f, p.hit, false, at);
            FieldReact(at, p.hit, radius);
        };

        if (p.drop) {
            p.t += dt;
            if (p.homing > 0 && p.hit.side >= 0 && p.t < p.dur * 0.8f) {
                int ts = NearestFoe(p.hit.side, p.target);
                if (ts >= 0 && Targetable(ts)) p.target = p.pos = sides[ts].Cur().pos;
            }
            if (p.t >= 0) {
                float u = std::min(1.0f, p.t / p.dur);
                p.z = 110.0f * (1.0f - u * u);
                if (u < 1 && !GetLook(p.hit.look, p.hit.type).skyStrike) TrailVfx(p, p.pos);
                if (u >= 1) {
                    p.alive = false;
                    LandVfx(p.hit, p.target, p.radius, true);
                    Shake(1);
                    PlaySfx(p.hit.type == Type::Electric ? Sfx::Zap : Sfx::Thud);
                    landAt(p.target, p.radius);
                }
            }
        } else if (p.lob) {
            p.t += dt;
            float u = std::min(1.0f, p.t / p.dur);
            p.pos = LerpV(p.start, p.target, u);
            p.z = std::sin(u * PI_F) * (28.0f + Dist(p.start, p.target) * 0.15f);
            if (p.hit.side >= 0 && u < 1) TrailVfx(p, p.pos);
            if (u >= 1) {
                p.alive = false;
                LandVfx(p.hit, p.target, p.radius, false);
                Shake(1);
                PlaySfx(Sfx::Thud);
                landAt(p.target, p.radius);
                SpawnPatch(p.hit, p.target);
            }
        } else {
            int os = p.hit.side >= 0 ? NearestFoe(p.hit.side, p.pos) : -1;
            if (p.boomer) {
                p.t += dt;
                const Side& ow = p.hit.side >= 0 ? sides[p.hit.side] : sides[0];
                bool ownerHere = p.hit.side >= 0 && ow.active == p.hit.fighter && ow.Cur().state == FState::Active;
                if (!p.returning && p.traveled >= p.maxDist) p.returning = true;
                if (p.returning) {
                    if (!ownerHere || p.t > 5) { p.alive = false; projs[i] = p; continue; }
                    Vector2 to = ow.Cur().pos - p.pos;
                    if (Len(to) < 10) { p.alive = false; projs[i] = p; continue; }  // caught
                    p.vel = SafeNorm(to) * Len(p.vel);
                }
            }
            if (p.homing > 0 && os >= 0 && Targetable(os)) {
                Vector2 want = SafeNorm(sides[os].Cur().pos - p.pos);
                Vector2 cur = SafeNorm(p.vel);
                float ang = std::atan2(cur.x * want.y - cur.y * want.x, Dot(cur, want));
                float turn = std::max(-p.homing * dt, std::min(p.homing * dt, ang));
                p.vel = Rot(p.vel, turn);
            }
            Vector2 step = p.vel * dt;
            p.pos += step;
            p.traveled += Len(step);
            if (p.thrown) {
                Particle& q = Emit(PK::Debris, p.pos, p.z, RandDirB() * 20.0f, 10, 0.4f, 1.6f, Color{150, 130, 110, 255}, true);
                q.spin = 6;
            } else {
                TrailVfx(p, p.pos);
            }
            Vector2 vdir = SafeNorm(p.vel);
            bool hitSomething = false;
            if ((p.hit.type == Type::Electric || p.hit.type == Type::Ice) && p.hit.power > 0 && p.z < 24)   // skimming over a puddle
                for (size_t k = 0; k < field.puddles.size(); k++)
                    if (field.puddles[k].Contains(p.pos) && (p.hit.type == Type::Ice ? field.puddles[k].ice <= 0 : field.puddles[k].ice <= 0 && field.puddles[k].shockCd <= 0))
                        FieldReact(field.puddles[k].pos, p.hit, 0);
            if (p.boomer) {  // hits once going out and once coming back, never stops
                for (int ts = 0; ts < N(); ts++) {
                    if (!Hits(p.hit.side, ts) || !Hittable(ts)) continue;
                    int bit = p.returning ? 2 : 1;
                    if ((p.hitMask & bit) || Dist(p.pos, sides[ts].Cur().pos) >= p.radius + HitR(sides[ts].Cur())) continue;
                    p.hitMask |= bit;
                    ApplyHit(p.hit, ts, vdir);
                    TryElectrify(p.pos, p.hit);
                    if (!p.returning) p.returning = true;
                }
                for (Obstacle& o : field.obs) {
                    if (!o.Blocks() || Dist(p.pos, o.pos) > o.radius + p.radius * 0.5f) continue;
                    if (p.returning) { p.alive = false; Burst(p.pos, tc, 6, 40, p.z); }
                    else { p.returning = true; ObstacleImpact(o, p.hit, vdir, false); }
                    break;
                }
                if (p.pos.x < 0 || p.pos.x > ARENA_W || p.pos.y < 0 || p.pos.y > ARENA_H) {
                    p.pos.x = std::max(0.0f, std::min(ARENA_W, p.pos.x));
                    p.pos.y = std::max(0.0f, std::min(ARENA_H, p.pos.y));
                    p.returning = true;
                }
                projs[i] = p;
                continue;
            }
            for (int ts = 0; ts < N() && !hitSomething; ts++) {
                if (!Hits(p.hit.side, ts) || !Hittable(ts)) continue;
                Vector2 epos = sides[ts].Cur().pos;
                if (Dist(p.pos, epos) < p.radius + HitR(sides[ts].Cur())) {
                    p.alive = false;
                    hitSomething = true;
                    ApplyHit(p.hit, ts, vdir);
                    if (p.thrown) { Boom(p.pos, 22, Impact::Debris, 6); }
                    else if (GetLook(p.hit.look, p.hit.type).boom) Boom(p.pos, GetLook(p.hit.look, p.hit.type).boomR, GetLook(p.hit.look, p.hit.type).impact, p.z);
                    SpawnPatch(p.hit, p.pos);
                    TryElectrify(p.pos, p.hit);
                }
            }
            if (!hitSomething) {
                for (Obstacle& o : field.obs) {
                    if (!o.Blocks() || Dist(p.pos, o.pos) > o.radius + p.radius * 0.5f) continue;
                    if (o.kind == Obstacle::TempWall && p.hit.ab == Ab::Infiltrator) continue;   // passes through barriers
                    if (p.bounces > 0) {  // ricochet off it
                        Vector2 nrm = SafeNorm(p.pos - o.pos);
                        if (Dot(p.vel, nrm) < 0) p.vel = p.vel - nrm * (2 * Dot(p.vel, nrm));
                        p.pos = o.pos + nrm * (o.radius + p.radius * 0.5f + 1);
                        p.bounces--;
                        p.traveled = std::max(0.0f, p.traveled - 40);
                        Burst(p.pos, tc, 5, 40, p.z);
                        break;
                    }
                    p.alive = false;
                    hitSomething = true;
                    ObstacleImpact(o, p.hit, vdir, false);
                    if (p.thrown) Boom(p.pos - vdir * 3.0f, 20, Impact::Debris, p.z);
                    else if (GetLook(p.hit.look, p.hit.type).boom) Boom(p.pos - vdir * 3.0f, GetLook(p.hit.look, p.hit.type).boomR * 0.8f, GetLook(p.hit.look, p.hit.type).impact, p.z);
                    else { HitSpec hh = p.hit; hh.light = true; HitVfx(hh, p.pos - vdir * 3.0f, p.z, -vdir); }
                    if (p.thrown) Shake(2);
                    SpawnPatch(p.hit, p.pos - vdir * (o.radius * 0.6f));
                    break;
                }
            }
            if (!hitSomething && field.kind == ArenaKind::Psychic && p.portalCd <= 0 && field.portals.size() > 1) {
                for (size_t k = 0; k < field.portals.size(); k++) {
                    if (Dist(p.pos, field.portals[k].pos) > 10) continue;
                    size_t j = (k + 1 + SimRand(0, (int)field.portals.size() - 2)) % field.portals.size();
                    p.pos = field.portals[j].pos + vdir * 12.0f;
                    p.portalCd = 0.5f;
                    Burst(field.portals[k].pos, Color{230, 120, 240, 255}, 6, 40, p.z);
                    Burst(p.pos, Color{230, 120, 240, 255}, 6, 40, p.z);
                    break;
                }
            }
            bool outX = p.pos.x < 0 || p.pos.x > ARENA_W, outY = p.pos.y < 0 || p.pos.y > ARENA_H;
            if (!hitSomething && p.bounces > 0 && (outX || outY) && p.traveled <= p.maxDist) {  // ricochet off the walls
                if (outX) { p.vel.x = -p.vel.x; p.pos.x = std::max(0.0f, std::min(ARENA_W, p.pos.x)); }
                if (outY) { p.vel.y = -p.vel.y; p.pos.y = std::max(0.0f, std::min(ARENA_H, p.pos.y)); }
                p.bounces--;
                p.traveled = std::max(0.0f, p.traveled - 40);
                Burst(p.pos, tc, 5, 40, p.z);
                outX = outY = false;
            }
            if (!hitSomething && (p.traveled > p.maxDist || outX || outY)) {
                p.alive = false;
                if (GetLook(p.hit.look, p.hit.type).boom) Boom(p.pos, GetLook(p.hit.look, p.hit.type).boomR * 0.7f, GetLook(p.hit.look, p.hit.type).impact, p.z);
                else { HitSpec hh = p.hit; hh.light = true; HitVfx(hh, p.pos, p.z, {0, 0}); }
                SpawnPatch(p.hit, p.pos);
                TryElectrify(p.pos, p.hit);
            }
        }
        projs[i] = p;
    }
    projs.erase(std::remove_if(projs.begin(), projs.end(), [](const Projectile& p) { return !p.alive; }), projs.end());
}

void Battle::UpdateHazards(float dt) {
    for (size_t i = 0; i < hazards.size(); i++) {
        hazards[i].life -= dt;
        hazards[i].tickT -= dt;
        Hazard& hz = hazards[i];
        int os = -1;  // a foe touching it
        if (hz.hit.side >= 0)
            for (int ts : FoeSlots(hz.hit.side))
                if (Hittable(ts) && Dist(hz.pos, sides[ts].Cur().pos) < hz.radius + HitR(sides[ts].Cur()) - (hz.trap ? 2 : 0)) { os = ts; break; }

        if (hz.orbit) {  // circles its owner; hits on touch
            const Side& ow = sides[std::max(0, hz.hit.side)];
            if (hz.hit.side < 0 || ow.active != hz.hit.fighter || ow.Cur().state != FState::Active) { hz.life = 0; continue; }
            hz.orbitAngle += hz.orbitSpeed * dt;
            Vector2 c = ow.Cur().pos;
            hz.pos = c + Vector2{std::cos(hz.orbitAngle) * hz.orbitDist, std::sin(hz.orbitAngle) * hz.orbitDist * 0.85f};
            hz.hitCd -= dt;
            if (hz.hitCd <= 0 && os >= 0) {
                hz.hitCd = 0.6f;
                HitSpec hit = hz.hit;
                Vector2 dir = SafeNorm(sides[os].Cur().pos - c);
                Vector2 at = hz.pos;
                ApplyHit(hit, os, dir);
                Burst(at, TypeColor(hit.type), 6, 50, 8);
            }
            if (SimRand(0, 2) == 0) {
                Particle p;
                p.pos = hazards[i].pos;
                p.z = 8;
                p.life = p.maxLife = 0.25f;
                p.col = TypeColor(hazards[i].hit.type);
                p.size = 1;
                parts.push_back(p);
            }
            continue;
        }
        if (hz.trap) {  // springs once when the foe steps on it
            hz.armT -= dt;
            if (hz.armT <= 0 && os >= 0) {
                HitSpec hit = hz.hit;
                Vector2 at = hz.pos;
                hz.life = 0;
                Fx fx;
                fx.kind = FxKind::Ring;
                fx.pos = at;
                fx.maxLife = 0.3f;
                fx.radius = 22;
                fx.col = TypeColor(hit.type);
                fxs.push_back(fx);
                Burst(at, TypeColor(hit.type), 14, 70, 3);
                AddText(at, "Trap!", TypeColor(hit.type), 30);
                PlaySfx(Sfx::Thud);
                ApplyHit(hit, os, {0, 0});
            }
            continue;
        }
        if (hz.pull > 0 && hz.hit.side >= 0) {  // vortex: drag foes in
            for (int ts : FoeSlots(hz.hit.side)) {
                if (!Hittable(ts)) continue;
                Fighter& e = sides[ts].Cur();
                Vector2 to = hz.pos - e.pos;
                float d = Len(to);
                float reach = hz.radius * 1.8f;
                if (d < reach && d > 2) e.pos += SafeNorm(to) * (hz.pull * (0.4f + 0.6f * (1 - d / reach)) * dt);
            }
        }
        if (hz.tickT <= 0) {
            hz.tickT = 0.5f;
            for (int ts = 0; ts < N(); ts++) {
                if (!Hits(hazards[i].hit.side, ts) || !Hittable(ts)) continue;
                if (Dist(hazards[i].pos, sides[ts].Cur().pos) < hazards[i].radius + HitR(sides[ts].Cur()) - 2) {
                    HitSpec hit = hazards[i].hit;
                    ApplyHit(hit, ts, {0, 0});
                }
            }
        }
        {
            Hazard hc = hazards[i];
            int n = (!hc.patch && hc.hit.type == Type::Fire) ? 2 : (SimRand(0, hc.patch ? 1 : 2) == 0 ? 1 : 0);
            for (int k = 0; k < n; k++) HazardVfx(hc);
        }
    }
    hazards.erase(std::remove_if(hazards.begin(), hazards.end(), [](const Hazard& h) { return h.life <= 0; }), hazards.end());
}

void Battle::UpdateWaves(float dt) {
    for (size_t i = 0; i < waves.size(); i++) {
        waves[i].r += waves[i].speed * dt;
        if (waves[i].hit.side >= 0) {
            for (int os : FoeSlots(waves[i].hit.side)) {
                if ((waves[i].hitMask >> os) & 1 || !Hittable(os)) continue;
                float d = Dist(waves[i].origin, sides[os].Cur().pos);
                if (d <= waves[i].r + HitR(sides[os].Cur()) && d <= waves[i].maxR + HitR(sides[os].Cur())) {
                    waves[i].hitMask |= 1 << os;
                    HitSpec hit = waves[i].hit;
                    Vector2 o = waves[i].origin;
                    ApplyHit(hit, os, SafeNorm(sides[os].Cur().pos - o));
                }
            }
        }
        for (int k = 0; k < 2; k++) {
            ShockWave wc = waves[i];
            WaveVfx(wc);
        }
    }
    waves.erase(std::remove_if(waves.begin(), waves.end(), [](const ShockWave& w) { return w.r >= w.maxR; }), waves.end());
}

void Battle::UpdateEffects(float dt) {
    for (auto& fx : fxs) fx.life += dt;
    fxs.erase(std::remove_if(fxs.begin(), fxs.end(), [](const Fx& f) { return f.life >= f.maxLife; }), fxs.end());

    for (auto& p : parts) {
        p.pos += p.vel * dt;
        p.vel *= std::exp(-p.drag * dt);
        p.rot += p.spin * dt;
        p.size += p.grow * dt;
        if (p.grav) {
            p.vz -= 220 * dt;
            p.z += p.vz * dt;
            if (p.z < 0) { p.z = 0; p.vz = 0; }
        } else {
            p.z += p.vz * dt;
        }
        p.life -= dt;
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.life <= 0; }), parts.end());

    for (auto& t : texts) {
        t.life += dt;
        t.z += 16 * dt;
    }
    texts.erase(std::remove_if(texts.begin(), texts.end(), [](const FloatText& t) { return t.life >= t.maxLife; }), texts.end());
}

void Battle::Burst(Vector2 pos, Color c, int n, float speed, float z) {
    for (int i = 0; i < n; i++) {
        float ang = Frand(0, PI_F * 2);
        Particle p;
        p.pos = pos;
        p.vel = Vector2{std::cos(ang), std::sin(ang)} * Frand(speed * 0.3f, speed);
        p.z = z;
        p.vz = Frand(20, 70);
        p.grav = true;
        p.life = p.maxLife = Frand(0.3f, 0.6f);
        p.size = (float)SimRand(1, 2);
        p.col = ColorBrightness(c, Frand(-0.2f, 0.3f));
        if (i % 2) { p.kind = PK::Ember; p.size = 0.9f; }
        parts.push_back(p);
    }
}

void Battle::AddText(Vector2 pos, const std::string& text, Color c, float z) {
    FloatText t;
    t.pos = pos;
    t.z = z;
    t.text = text;
    t.col = c;
    t.maxLife = 1.1f;
    texts.push_back(t);
}

void Battle::Shake(int amt, float t) {
    shakeAmt = std::max(shakeAmt * (shakeT > 0 ? 1 : 0), amt);
    shakeT = std::max(shakeT, t);
}

// ---------------------------------------------------------------- CPU opponent

Intent Battle::ThinkAI(int s, float dt) {
    Intent it;
    Side& sd = sides[s];
    Fighter& f = sd.Cur();
    AIBrain& b = sd.ai;
    const AIParams& P = Params(sd.level);
    int os = sd.target >= 0 ? sd.target : AnyFoe(s);
    const Fighter& e = sides[os].Cur();
    bool eHere = (e.state == FState::Active || e.state == FState::Entering) && !e.burrowed;

    if (!eHere) {
        Vector2 home{sd.teamId == 0 ? 170.0f : ARENA_W - 170.0f, sd.lastPos.y};
        if (e.burrowed) home = f.pos + SafeNorm(f.pos - e.pos) * 40.0f;  // keep away from a digger
        Vector2 to = home - f.pos;
        if (Len(to) > 10) it.move = SafeNorm(to) * 0.6f;
        return it;
    }

    Vector2 to = e.pos - f.pos;
    float dist = Len(to);
    Vector2 dirTo = SafeNorm(to);
    f.aim = dirTo;
    f.dir = DirFromVec(dirTo);

    b.restT -= dt;
    b.strafeT -= dt;
    if (b.strafeT <= 0) {
        b.strafeSign = SimRand(0, 1) ? 1.0f : -1.0f;
        b.strafing = Frand(0, 1) < P.strafe;
        b.strafeT = Frand(0.9f, 2.0f);
    }
    Vector2 perp{-dirTo.y * b.strafeSign, dirTo.x * b.strafeSign};
    float sideW = b.strafing ? 0.7f : 0.0f;
    float ideal = IdealRange(f);
    if (f.boss > 0) ideal = std::min(ideal, 55.0f);   // a legendary stands its ground instead of kiting

    Vector2 mv{0, 0};
    if (dist > ideal + 30) mv = dirTo + perp * (sideW * 0.4f);
    else if (dist < ideal - 25) mv = -dirTo * 0.8f + perp * sideW;
    else mv = perp * sideW;

    b.dodgeT -= dt;
    if (b.dodgeT <= 0) {
        b.dodgeT = 0.6f;
        b.dodging = Frand(0, 1) < P.dodgeChance;
    }
    bool danger = false;
    for (const auto& p : projs) {
        if (p.hit.side >= 0 && !Foes(s, p.hit.side)) continue;
        if (p.lob || p.drop) {
            if (Dist(p.target, f.pos) < p.radius + 10) { danger = true; if (b.dodging) mv += SafeNorm(f.pos - p.target, perp); }
            continue;
        }
        Vector2 rel = f.pos - p.pos;
        Vector2 vd = SafeNorm(p.vel);
        float along = Dot(rel, vd);
        if (along < 0 || along > 90) continue;
        Vector2 lat = rel - vd * along;
        if (Len(lat) < 18) { danger = true; if (b.dodging) mv += SafeNorm(lat, Vector2{-vd.y, vd.x}); }
    }
    if (e.dashing || e.pendingMove >= 0) danger |= dist < 120;
    for (const auto& h : hazards)
        if ((h.hit.side < 0 || Foes(s, h.hit.side)) && Dist(h.pos, f.pos) < h.radius + 10) mv += SafeNorm(f.pos - h.pos, perp) * 1.2f;
    for (int k = 0; k < N(); k++) {  // don't crowd a teammate
        if (k == s || Foes(s, k)) continue;
        const Fighter& al = sides[k].Cur();
        if ((al.state == FState::Active || al.state == FState::Entering) && Dist(al.pos, f.pos) < 34)
            mv += SafeNorm(f.pos - al.pos, perp) * 0.8f;
    }

    mv += FieldPull(s);
    const float M = 30;
    if (f.pos.x < M) mv.x += 1;
    if (f.pos.x > ARENA_W - M) mv.x -= 1;
    if (f.pos.y < M) mv.y += 1;
    if (f.pos.y > ARENA_H - M) mv.y -= 1;
    if (b.restT > 0 && !b.dodging) mv = {0, 0};
    if (Len(mv) > 0.05f) it.move = SafeNorm(mv) * P.speedMul;
    // Hop across water that would slow it down (Water Gym).
    if (f.jumpT <= 0 && Len(it.move) > 0.3f && !IsAirborne(*f.sp) && !HasType(f.sp, Type::Water) && field.IsWater(f.pos) && Chance(std::min(1.0f, dt * 2.0f)))
        it.jump = true;

    // Dodge (or Protect / Detect) when something is about to hit.
    b.guardT -= dt;
    if (danger && b.guardT <= 0) {
        b.guardT = 0.6f;
        if (f.dodgeCd <= 0 && b.dodging && Chance(P.dodgeChance * 0.8f)) {
            it.dodge = true;
            if (Len(it.move) < 0.1f) it.move = perp;
            return it;
        }
        for (int k = 0; k < f.nMoves; k++)
            if (f.moves[k].kind == MoveKind::Shield && f.cd[k] <= 0 && Chance(P.dodgeChance + 0.1f)) {
                it.attack = k;
                it.hasAim = true;
                it.aim = dirTo;
                return it;
            }
    }

    b.actT -= dt;
    if (b.actT <= 0 && e.state == FState::Active) {
        int best = -1;
        float bestScore = 0;
        for (int k = 0; k < f.nMoves; k++) {
            if (f.cd[k] > 0 || !f.HasUses(k)) continue;
            const MoveDef& m = f.moves[k];
            float sc = 0;
            if (m.scope == 2 || m.scope == 4) {   // partner-only moves need a partner; Follow Me helps one
                bool partner = false;
                for (int q = 0; q < N(); q++) if (q != s && !Foes(s, q) && Targetable(q)) partner = true;
                if (!partner) continue;
            }
            if (m.kind == MoveKind::Shield && !m.counter && !m.endure && m.scope == 0) continue;
            if (m.kind == MoveKind::Weather) {
                bool likes = (m.weather == Weather::Rain && HasType(f.sp, Type::Water)) || (m.weather == Weather::Sun && HasType(f.sp, Type::Fire)) ||
                             (m.weather == Weather::Sand && (HasType(f.sp, Type::Rock) || HasType(f.sp, Type::Ground) || HasType(f.sp, Type::Steel))) ||
                             (m.weather == Weather::Snow && HasType(f.sp, Type::Ice));
                sc = weather == m.weather ? 0.0f : (likes ? 45.0f : 12.0f);
            } else if (m.ohko || m.fixedDmg != 0) {
                if (!InRange(m, dist) || (TypeEffectiveness(m.type, e.sp->type1, e.sp->type2) * AbilityTypeMul(AbOf(e), m.type)) <= 0) continue;
                sc = m.ohko ? 50.0f : 75.0f;
            } else if (m.transform) {
                sc = f.transformFrom >= 0 ? 0.0f : 90.0f;  // copy the foe right away
            } else if (m.metronome || m.copycat) {
                sc = 40.0f;
            } else if (m.kind == MoveKind::Shield && (m.counter || m.endure)) {
                sc = danger ? 50.0f : 0.0f;
            } else if (m.kind == MoveKind::Wall) {
                sc = danger ? 55.0f : (dist > 110 && Chance(0.15f) ? 20.0f : 0.0f);
            } else if (m.kind == MoveKind::Orbit || m.kind == MoveKind::Trap) {
                int mine = 0;
                for (const auto& hz : hazards) mine += hz.hit.side == s && ((m.kind == MoveKind::Orbit && hz.orbit) || (m.kind == MoveKind::Trap && hz.trap));
                if ((m.kind == MoveKind::Orbit && mine > 0) || (m.kind == MoveKind::Trap && mine >= 3) || !InRange(m, dist)) continue;
                float eff = (TypeEffectiveness(m.type, e.sp->type1, e.sp->type2) * AbilityTypeMul(AbOf(e), m.type));
                if (eff <= 0) continue;
                sc = m.power > 0 ? m.power * eff * (m.kind == MoveKind::Orbit ? m.count * 0.6f : 0.8f) : 30.0f;
            } else if (m.kind == MoveKind::Buff) {
                sc = (m.amount > 1.0f && f.buffT[(int)m.stat] <= 0 && dist > 60) ? 35.0f : 0.0f;
            } else if (m.kind == MoveKind::Heal) {
                float th = m.status == Status::Sleep ? 0.35f : 0.45f;
                sc = (f.hp < f.maxHp * th) ? 90.0f : 0.0f;
            } else if (m.kind == MoveKind::Blink && m.power <= 0) {
                sc = (dist < 40 && f.hp < f.maxHp * 0.5f) ? 40.0f : 0.0f;
            } else if (IsStatusMove(m)) {
                if (!InRange(m, dist)) continue;
                if ((TypeEffectiveness(m.type, e.sp->type1, e.sp->type2) * AbilityTypeMul(AbOf(e), m.type)) <= 0) continue;
                if (m.status != Status::None) {
                    bool has = IsMajorStatus(m.status) ? e.st.major != Status::None
                             : (m.status == Status::Confuse ? e.st.confuseT > 0 : (m.status == Status::Seed ? e.st.seedT > 0 : false));
                    if (!has && !ImmuneTo(e.sp, m.status)) sc = 45;
                } else if (m.lowers) {
                    sc = e.dropT[(int)m.lowerStat] <= 0 ? 30.0f : 0.0f;
                }
            } else {
                if (!InRange(m, dist)) continue;
                float eff = (TypeEffectiveness(m.type, e.sp->type1, e.sp->type2) * AbilityTypeMul(AbOf(e), m.type));
                if (eff <= 0) continue;
                sc = m.power * eff * (m.hits > 1 ? m.hits * 0.8f : 1.0f);
                if (m.hex && e.st.major != Status::None) sc *= 2;
                if (m.kind == MoveKind::Spread) sc *= 1.6f;
                if (m.kind == MoveKind::Zone) sc *= 1.5f;
                if (m.recoil > 0 && f.hp < f.maxHp * 0.3f) sc *= 0.4f;
                if (m.selfKO) { if (f.hp > f.maxHp * 0.3f || sd.AliveCount() <= 1) continue; sc *= 0.8f; }
            }
            sc *= Frand(0.75f, 1.25f);
            if (sc > bestScore) { bestScore = sc; best = k; }
        }
        if (best >= 0) {
            const MoveDef& m = f.moves[best];
            Vector2 aim = dirTo;
            if (m.kind == MoveKind::Beam) {
                aim = SafeNorm(e.pos + e.moveVel * (m.windup * P.lead) - f.pos, dirTo);
            } else if ((m.kind == MoveKind::Projectile || m.kind == MoveKind::Spread) && m.speed > 0) {
                Vector2 lead = e.pos + e.moveVel * (dist / m.speed * P.lead);
                aim = SafeNorm(lead - f.pos, dirTo);
            }
            it.attack = best;
            it.aim = Rot(aim, Frand(-P.aimError, P.aimError));
            it.hasAim = true;
            b.actT = Frand(P.reactMin, P.reactMax);
            if (Frand(0, 1) < P.restChance) b.restT = Frand(0.5f, 1.1f);
        } else {
            b.actT = 0.2f;
        }
    }

    b.swapThinkT -= dt;
    if (b.swapThinkT <= 0) {
        b.swapThinkT = 2.0f;
        if (sd.swapCd <= 0 && sd.AliveCount() > 1 && it.attack < 0) {
            float cur = Matchup(f, e);
            int n = sd.NextAlive(sd.active);
            if (n >= 0) {
                const Fighter& alt = sd.team[n];
                bool better = Matchup(alt, e) > cur + 1.0f;
                bool rescue = f.hp < f.maxHp * 0.25f && alt.hp > alt.maxHp * 0.5f;
                if ((better && SimRand(0, 1)) || (rescue && SimRand(0, 2) == 0)) it.swap = true;
            }
        }
    }
    return it;
}

// ---------------------------------------------------------------- Drawing

void Battle::DrawProjectile(const Projectile& p) const {
    if (p.drop && p.t < 0) return;
    if (p.thrown) {  // a hurled stone pillar
        Vector2 sp = ToScreen(p.pos, p.z);
        int x = (int)sp.x, y = (int)sp.y;
        bool f = ((int)(p.spin) % 2) != 0;
        int w = f ? 14 : 9, h = f ? 9 : 14;
        DrawRectangle(x - w / 2 - 1, y - h / 2 - 1, w + 2, h + 2, Color{80, 70, 60, 255});
        DrawRectangle(x - w / 2, y - h / 2, w, h, Color{150, 136, 118, 255});
        DrawRectangle(x - w / 2, y - h / 2, w, 2, Color{184, 170, 150, 255});
        return;
    }
    const Look& L = GetLook(p.hit.look, p.hit.type);
    Vector2 sp = ToScreen(p.pos, p.z);
    uint32_t seed = (uint32_t)(p.start.x * 13 + p.start.y * 7 + p.dur * 1000) ^ (uint32_t)(p.hit.side + 5);
    if (p.drop && L.skyStrike) {  // charge gathering in the sky before the strike
        float u = std::min(1.0f, p.t / std::max(0.05f, p.dur));
        Vector2 g = ToScreen(p.pos, 0);
        VAdd();
        VGlow(Vector2{g.x, (float)(FLOOR_Y - STANDS_H)}, 8 + u * 10, L.glow, 0.3f + 0.5f * u);
        if (((int)(time * 30)) % 3 == 0) VBolt(Vector2{g.x + std::sin(time * 50) * 20, (float)(FLOOR_Y - STANDS_H)}, Vector2{g.x, (float)(FLOOR_Y - STANDS_H) + 18 * u}, VHash(seed, (uint32_t)(time * 30)), 0.6f, L.glow, 0.7f, 0);
        VNorm();
        return;
    }
    Vector2 vel = p.vel;
    float radius = p.radius;
    if (p.lob) {   // arc direction on screen
        float u = std::min(1.0f, p.t / std::max(0.05f, p.dur));
        Vector2 dir = p.target - p.start;
        vel = Vector2{dir.x, dir.y * DEPTH - std::cos(u * PI_F) * 80};
        radius = p.radius * 0.28f + 2.0f;
    } else if (p.drop) {
        vel = Vector2{0, 1};
        radius = std::max(3.0f, std::min(p.radius * 0.4f, 7.0f));
    } else {
        vel = Vector2{p.vel.x, p.vel.y * DEPTH};
    }
    if (p.boomer) radius = p.radius + 1;
    VProjectile(sp, vel, radius, p.spin, time, L, seed);
}

void Battle::DrawFighter(int s) const {
    const Fighter& f = sides[s].Cur();
    if (f.state == FState::Benched || f.state == FState::Fainted) return;
    if (f.burrowed && !f.leaping) {
        Vector2 g = ToScreen(f.pos);
        if (f.moves[f.burrowMove].type == Type::Flying) DrawEllipse((int)g.x, (int)g.y + 1, 7, 2.5f, Fade(BLACK, 0.3f));
        else DrawEllipse((int)g.x, (int)g.y + 1, 8, 3, Fade(Color{90, 70, 50, 255}, 0.7f));
        return;
    }
    const CreatureSprites& cs = SpritesFor(f.species);
    bool animated = cs.frames[f.dir] > 1;
    bool still = !f.CanAct() && f.state == FState::Active;

    float z = 0, bob = 0;
    if (f.sp->floats) z = 5.0f + (still ? 0 : std::sin(f.animT * 3.0f + s) * 2.0f);
    else if (!still && !animated && f.moving) bob = ((int)(f.animT * 8) % 2) ? 1.0f : 0.0f;
    else if (!still && !animated) bob = ((int)(f.animT * 1.6f) % 2) ? 1.0f : 0.0f;
    int frame = (animated && !still && (f.moving || f.dashing)) ? ((int)(f.animT * 7) % 2) : 0;

    float sx = 1, sy = 1, alpha = 1, white = f.flash > 0 ? 0.85f : 0.0f;
    int species = f.species;
    if (f.state == FState::Entering) {
        float u = std::min(1.0f, f.stateT / 0.3f);
        sx = sy = u;
        white = std::max(white, 1.0f - std::min(1.0f, f.stateT / 0.45f));
    } else if (f.state == FState::Recalling) {
        float u = 1.0f - std::min(1.0f, f.stateT / 0.35f);
        sx = sy = u;
        white = 1;
    } else if (f.state == FState::Fainting) {
        float u = std::min(1.0f, f.stateT / 0.8f);
        sy = 1.0f - u * 0.6f;
        alpha = 1.0f - u;
        z = 0;
    } else if (f.state == FState::Evolving) {
        // Glow white and flicker between the old and new forms, faster and faster.
        float u = f.stateT / 1.8f;
        float rate = 3.0f + u * 14.0f;
        if (u > 0.25f && ((int)(f.stateT * rate) % 2) && f.evolveTo >= 0) species = f.evolveTo;
        white = 0.55f + 0.45f * std::sin(f.stateT * 12.0f);
        sx = sy = 1.0f + 0.08f * std::sin(f.stateT * 9.0f);
        z = 3.0f * std::sin(u * PI_F);
    }

    if (f.leaping && f.burrowMove >= 0) {  // up in the air
        float dur = std::max(0.05f, f.moves[f.burrowMove].duration);
        float u = 1.0f - std::max(0.0f, f.burrowT) / dur;
        z = std::sin(u * PI_F) * 46.0f;
        bob = 0;
    }
    if (f.jumpT > 0) {   // a jump: up in the air, with a shadow left on the ground
        float jz = f.JumpZ();
        Vector2 g = ToScreen(f.pos);
        float k = 1.0f - jz / (JUMP_HEIGHT * 1.8f);
        DrawEllipse((int)g.x, (int)g.y + 1, 8.0f * k, 2.8f * k, Fade(BLACK, 0.35f));
        z += jz;
        bob = 0;
    }
    Vector2 base = ToScreen(f.pos, z + bob);
    if (f.hitstun > 0) base.x += ((int)(time * 60) % 2) ? 1.0f : -1.0f;
    Color tint = Fade(WHITE, alpha);
    if (f.st.major == Status::Freeze) tint = Fade(Color{170, 220, 255, 255}, alpha);
    else if (f.st.major == Status::Burn && ((int)(time * 4) % 2)) tint = Fade(Color{255, 210, 190, 255}, alpha);
    if (f.dashing && f.dashMove >= 0) {
        const MoveDef& dm = f.moves[f.dashMove];
        VDashAura(base, CreatureHeight(species, f.dir) * sy, Vector2{f.dashVel.x, f.dashVel.y * DEPTH}, GetLook(LookKey(dm.name, dm.type, dm.kind), dm.type), time);
    }
    if (f.dodgeT > 0) {   // a dodge: see-through, with after-images trailing behind
        for (int k = 2; k >= 1; k--) {
            Vector2 gp = ToScreen(f.pos - f.dodgeVel * (0.035f * k), z + bob);
            DrawCreature(species, f.dir, frame, gp.x, gp.y + 2, sx, sy, Fade(Color{170, 210, 255, 255}, 0.28f / k), 0.6f);
        }
        tint = Fade(tint, 0.6f);
    }
    DrawCreature(species, f.dir, frame, base.x, base.y + 2, sx, sy, tint, white);
    float h = CreatureHeight(species, f.dir) * sy;
    if (f.state == FState::Active) VStatus(base, h, (int)f.st.major, time, (uint32_t)(s * 7919 + 17));

    if (f.shieldT > 0) {
        float r = h * 0.62f + 3;
        Color sc = Fade(Color{150, 220, 255, 255}, 0.35f + 0.25f * std::sin(time * 20));
        DrawEllipse((int)base.x, (int)(base.y - h * 0.45f), r, r * 0.95f, Fade(sc, 0.25f));
        EllipsePix(base.x, base.y - h * 0.45f, r, r * 0.95f, sc);
    }
    if (f.weakened && f.dazeT > 0) {
        for (int i = 0; i < 4; i++) {
            float a = time * 7 + i * 1.57f;
            DrawRectangle((int)(base.x + std::cos(a) * 10), (int)(base.y - h - 4 + std::sin(a) * 3), 2, 2, WHITE);
        }
    }
    if (f.st.confuseT > 0) {
        for (int i = 0; i < 3; i++) {
            float a = time * 5 + i * 2.1f;
            DrawRectangle((int)(base.x + std::cos(a) * 8), (int)(base.y - h - 3 + std::sin(a) * 2), 2, 2, UI_GOLD);
        }
    }

    for (int k = 0; k < 3; k++) {
        if (f.buffT[k] > 0 && ((int)(time * 6) % 2)) {
            Color bc = k == 0 ? Color{255, 120, 90, 255} : (k == 1 ? Color{120, 170, 255, 255} : Color{255, 230, 90, 255});
            DrawRectangle((int)base.x - 11 + k * 11, (int)(base.y - h - 1), 2, 2, bc);
        }
    }

    if (f.state == FState::Active || f.state == FState::Entering) {
        int bx = (int)base.x - 10, by = (int)(base.y - h - 5);
        DrawRectangle(bx - 1, by - 1, 22, 4, UI_INK);
        float frac = f.displayHp / f.maxHp;
        Color hc = frac > 0.5f ? Color{96, 216, 96, 255} : (frac > 0.2f ? Color{240, 200, 60, 255} : Color{232, 72, 64, 255});
        DrawRectangle(bx, by, (int)(20 * frac + 0.5f), 2, hc);
        Color tc = TeamColor(sides[s].teamId);
        int mx = (int)base.x, my = by - 6;
        const char* tag = StatusShort(f.st.major);
        if (*tag) {
            int w = TextW(tag, 10);
            DrawRectangle(mx - w / 2 - 2, my - 10, w + 4, 10, StatusColor(f.st.major));
            DrawText(tag, mx - w / 2, my - 10, 10, UI_INK);
        }
        if (IsDoubles() && !sides[s].tag.empty()) {  // who controls it
            const char* tg = sides[s].tag.c_str();
            Color tgc = sides[s].input == 0 ? UI_GOLD : (sides[s].input == 1 ? Color{120, 230, 255, 255} : tc);
            DrawTextOutlined(tg, mx - TextW(tg, 10) / 2, *tag ? my - 22 : my - 11, 10, tgc);
        } else {
            DrawRectangle(mx - 2, my, 5, 1, tc);
            DrawRectangle(mx - 1, my + 1, 3, 1, tc);
            DrawRectangle(mx, my + 2, 1, 1, tc);
        }
    }
}

void Battle::Draw(bool banners) const {
    DrawBattleArena(time, hype);
    DrawFieldFloor();
    DrawWeatherFloor();
    DrawPuddles();

    for (const auto& h : hazards) {
        Color c = TypeColor(h.hit.type);
        float a = std::min(1.0f, h.life * 2.0f) * std::min(1.0f, (h.maxLife - h.life) * 6.0f + 0.3f);
        Vector2 sp = ToScreen(h.pos);
        float pulse = 1.0f + std::sin(time * (h.patch ? 14 : 6)) * 0.06f;
        if (h.orbit) {  // shadow only; the orb itself is drawn above
            DrawEllipse((int)sp.x, (int)sp.y, 3, 1.2f, Fade(BLACK, 0.3f));
            continue;
        }
        if (h.trap) {  // a small, half-hidden trap
            bool armed = h.armT <= 0;
            float ta = armed ? 0.45f + 0.15f * std::sin(time * 4 + h.pos.x) : 0.8f;
            if (armed && AbOnField(Ab::Illuminate)) ta = 0.95f;
            for (int k = 0; k < 4; k++) {
                float ang = k * PI_F / 2 + PI_F / 4;
                Vector2 tip = ToScreen(h.pos + Vector2{std::cos(ang), std::sin(ang)} * 5.0f, 3);
                DrawTriangle(Vector2{sp.x - 1.5f, sp.y}, Vector2{sp.x + 1.5f, sp.y}, tip, Fade(c, ta));
                DrawLineEx(sp, tip, 1, Fade(ColorBrightness(c, -0.4f), ta));
            }
            DrawRectangle((int)sp.x - 1, (int)sp.y - 1, 2, 2, Fade(ColorBrightness(c, 0.4f), ta));
            continue;
        }
        if (h.pull > 0) {  // vortex: spinning spiral
            DrawEllipse((int)sp.x, (int)sp.y, h.radius, h.radius * DEPTH, Fade(c, 0.25f * a));
            for (int arm = 0; arm < 3; arm++) {
                for (int k = 0; k < 18; k++) {
                    float u = k / 18.0f;
                    float ang = time * 7 + arm * 2.094f + u * 4.0f;
                    float r = h.radius * (1.0f - u * 0.85f);
                    DrawRectangle((int)(sp.x + std::cos(ang) * r), (int)(sp.y + std::sin(ang) * r * DEPTH), 2, 2,
                                  Fade(ColorBrightness(c, 0.5f - u * 0.5f), a));
                }
            }
            EllipsePix(sp.x, sp.y, h.radius, h.radius * DEPTH, Fade(c, 0.6f * a));
            continue;
        }
        if (h.patch) {  // burning spot / puddle
            DrawEllipse((int)sp.x, (int)sp.y, h.radius * pulse, h.radius * DEPTH * pulse, Fade(ColorBrightness(c, -0.3f), 0.45f * a));
            DrawEllipse((int)sp.x, (int)sp.y, h.radius * 0.55f, h.radius * 0.55f * DEPTH, Fade(ColorBrightness(c, 0.3f), 0.5f * a));
        } else {
            DrawEllipse((int)sp.x, (int)sp.y, h.radius * pulse, h.radius * DEPTH * pulse, Fade(c, 0.28f * a));
            EllipsePix(sp.x, sp.y, h.radius * pulse, h.radius * DEPTH * pulse, Fade(c, 0.9f * a));
            EllipsePix(sp.x, sp.y, h.radius * 0.55f, h.radius * 0.55f * DEPTH, Fade(c, 0.5f * a));
        }
    }
    for (const auto& w : waves) {
        Vector2 sp = ToScreen(w.origin);
        const Look& L = GetLook(w.hit.look, w.hit.type);
        Color c = L.glow;
        float a = 1.0f - w.r / w.maxR * 0.6f;
        if (L.cast == Cast::Quake) {
            DrawEllipse((int)sp.x, (int)sp.y, w.r, w.r * DEPTH, Fade(Color{90, 70, 45, 255}, 0.12f * a));
            for (int k = 0; k < 3; k++) {
                float rr = w.r - k * 4;
                if (rr > 1) VRingE(sp, rr, rr * DEPTH, 2.2f - k * 0.6f, Fade(k == 0 ? Color{230, 200, 150, 255} : Color{120, 95, 65, 255}, a * (1 - k * 0.25f)), 48);
            }
        } else if (L.cast == Cast::Sound) {
            for (int k = 0; k < 3; k++) {
                float rr = w.r - k * 7;
                if (rr > 1) VRingE(ToScreen(w.origin, 8), rr, rr * 0.75f, 1.2f, Fade(k ? c : WHITE, a * (1 - k * 0.3f)), 40);
            }
        } else {
            DrawEllipse((int)sp.x, (int)sp.y, w.r, w.r * DEPTH, Fade(c, 0.14f * a));
            VRingE(sp, w.r, w.r * DEPTH, 3.5f, Fade(ColorBrightness(c, -0.35f), 0.75f * a), 48);
            if (w.hit.type == Type::Water)
                for (int k = 1; k < 4; k++) VRingE(ToScreen(w.origin, k * 2.5f), w.r - k * 1.5f, (w.r - k * 1.5f) * DEPTH, 2.5f, Fade(ColorBrightness(c, -0.2f + k * 0.12f), 0.8f * a), 48);
            VAdd();
            VRingE(sp, w.r, w.r * DEPTH, 4.0f, Fade(c, 0.35f * a), 48);
            VRingE(sp, w.r, w.r * DEPTH, 1.5f, Fade(L.core, a), 48);
            if (w.hit.type == Type::Water) VRingE(ToScreen(w.origin, 3), w.r - 2, (w.r - 2) * DEPTH, 1.2f, Fade(WHITE, 0.8f * a), 48);
            if (w.hit.type == Type::Fire)
                for (int k = 0; k < 16; k++) {
                    float ang = k * 0.3927f + w.r * 0.01f;
                    Vector2 q = ToScreen(w.origin + Vector2{std::cos(ang), std::sin(ang)} * w.r, 3 + std::sin(time * 20 + k) * 2);
                    VGlow(q, 5, Color{255, 140, 40, 255}, 0.7f * a);
                }
            VNorm();
        }
    }
    for (const auto& p : projs) {
        if (!p.drop || p.t < 0) continue;  // target markers for falling objects
        Vector2 sp = ToScreen(p.target);
        float u = std::min(1.0f, p.t / p.dur);
        float k = 1.2f - u * 0.4f;
        EllipsePix(sp.x, sp.y, p.radius * k, p.radius * DEPTH * k, Fade(Color{255, 80, 60, 255}, 0.4f + u * 0.5f));
    }
    for (const auto& p : projs) {  // landing spots of arena hazards (lava globs)
        if (!p.lob || p.hit.side >= 0) continue;
        Vector2 sp = ToScreen(p.target);
        float u = std::min(1.0f, p.t / std::max(0.01f, p.dur));
        EllipsePix(sp.x, sp.y, p.radius * (1.2f - u * 0.3f), p.radius * DEPTH * (1.2f - u * 0.3f), Fade(Color{255, 120, 40, 255}, 0.4f + u * 0.5f));
    }
    for (const auto& fx : fxs) {
        float t = fx.life / fx.maxLife;
        Vector2 sp = ToScreen(fx.pos);
        if (fx.kind == FxKind::Ring) {
            float r = fx.radius * (0.3f + 0.7f * EaseOutF(t));
            Vector2 g = ToScreen(fx.pos);
            VAdd();
            VRingE(g, r, r * DEPTH, 2.5f * (1 - t) + 0.5f, Fade(fx.col, 0.9f * (1 - t)));
            VRingE(g, r * 0.92f, r * 0.92f * DEPTH, 0.8f, Fade(WHITE, 0.8f * (1 - t)));
            VNorm();
            DrawEllipse((int)g.x, (int)g.y, r, r * DEPTH, Fade(fx.col, 0.15f * (1 - t)));
        } else if (fx.kind == FxKind::Charge) {
            VCharge(ToScreen(fx.pos, 9), fx.radius, t, fx.col, fx.seed, time);
        } else if (fx.kind == FxKind::Scorch) {
            float a = t < 0.75f ? 0.55f : 0.55f * (1 - (t - 0.75f) / 0.25f);
            Vector2 g = ToScreen(fx.pos);
            DrawEllipse((int)g.x, (int)g.y, fx.radius, fx.radius * DEPTH, Fade(fx.col, a));
            DrawEllipse((int)g.x, (int)g.y, fx.radius * 0.6f, fx.radius * 0.6f * DEPTH, Fade(fx.col, a * 0.6f));
            if (t < 0.4f) { VAdd(); VGlow(g, fx.radius * 0.7f, Color{255, 120, 40, 255}, 0.5f * (1 - t / 0.4f)); VNorm(); }
        } else if (fx.kind == FxKind::Shock) {
            float r = fx.radius * (0.2f + 0.8f * EaseOutF(t));
            Vector2 g = ToScreen(fx.pos);
            VAdd();
            VRingE(g, r, r * DEPTH, 3.0f * (1 - t) + 0.4f, Fade(fx.col, 0.7f * (1 - t)));
            VNorm();
        } else if (fx.kind == FxKind::Crack) {
            DrawCrackFx(fx.pos, fx.radius, t, fx.seed, (int)fx.len, fx.col);
        }
    }

    for (int s = 0; s < N(); s++) {
        const Fighter& f = sides[s].Cur();
        if (f.state == FState::Benched || f.state == FState::Fainted || (f.burrowed && !f.leaping)) continue;
        Vector2 g = ToScreen(f.pos);
        float a = f.state == FState::Fainting ? 1.0f - std::min(1.0f, f.stateT) : 1.0f;
        DrawEllipse((int)g.x, (int)g.y + 1, 9, 3.0f, Fade(BLACK, 0.35f * a));
        EllipsePix(g.x, g.y + 1, 11, 4.0f, Fade(TeamColor(sides[s].teamId), 0.8f * a));
    }
    for (const auto& p : projs) {
        if (p.drop && p.t < 0) continue;
        Vector2 g = ToScreen(p.pos);
        DrawEllipse((int)g.x, (int)g.y, 3, 1.2f, Fade(BLACK, 0.3f));
    }

    struct Item { float y; int kind; int idx; };
    std::vector<Item> items;
    for (int s = 0; s < N(); s++) items.push_back({sides[s].Cur().pos.y, 0, s});
    if (IsDoubles()) {  // each player's current target
        for (int s = 0; s < N(); s++) {
            if (sides[s].input < 0 || sides[s].target < 0 || !Targetable(sides[s].target)) continue;
            Vector2 g = ToScreen(sides[sides[s].target].Cur().pos);
            Color c = sides[s].input == 0 ? UI_GOLD : Color{120, 230, 255, 255};
            int x = (int)g.x, y = (int)g.y + 1, w = 13 + sides[s].input * 3, hh = 5 + sides[s].input * 2;
            DrawRectangle(x - w, y - hh, 4, 1, c); DrawRectangle(x - w, y - hh, 1, 3, c);
            DrawRectangle(x + w - 3, y - hh, 4, 1, c); DrawRectangle(x + w, y - hh, 1, 3, c);
            DrawRectangle(x - w, y + hh, 4, 1, c); DrawRectangle(x - w, y + hh - 2, 1, 3, c);
            DrawRectangle(x + w - 3, y + hh, 4, 1, c); DrawRectangle(x + w, y + hh - 2, 1, 3, c);
        }
    }
    for (int i = 0; i < (int)projs.size(); i++) items.push_back({projs[i].pos.y, 1, i});
    for (int i = 0; i < (int)field.obs.size(); i++)
        if (field.obs[i].alive) items.push_back({field.obs[i].pos.y, 2, i});
    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.y < b.y; });
    for (const auto& it : items) {
        if (it.kind == 0) DrawFighter(it.idx);
        else if (it.kind == 1) DrawProjectile(projs[it.idx]);
        else DrawObstacle(field.obs[it.idx]);
    }
    DrawFieldTop();
    DrawTimeOfDay();

    for (const auto& fx : fxs) {
        float t = fx.life / fx.maxLife;
        if (fx.kind == FxKind::Beam) {
            Vector2 a = ToScreen(fx.pos + fx.dir * 8.0f, fx.z);
            Vector2 b = ToScreen(fx.pos + fx.dir * fx.len, fx.z);
            VBeam(a, b, fx.radius, t, time, GetLook(fx.look, fx.type), fx.seed);
        } else if (fx.kind == FxKind::Slash) {
            const Look& L = GetLook(fx.look, fx.type);
            Vector2 from = ToScreen(fx.pos, 9), to = ToScreen(fx.pos + fx.dir * (fx.radius * 0.95f), 9);
            VMelee(from, to, t, L.melee, L, fx.seed);
        } else if (fx.kind == FxKind::Bolt) {
            const Look& L = GetLook(fx.look, fx.type);
            uint32_t fr = (uint32_t)(fx.life * 30);
            float a = t < 0.7f ? 1.0f : 1.0f - (t - 0.7f) / 0.3f;
            if (fr % 4 != 3) {
                Vector2 from = ToScreen(fx.pos, 10), to = ToScreen(fx.dir, 10);
                VBolt(from, to, VHash(fx.seed, fr / 2), L.boltWidth * (0.7f + 0.3f * a), fx.col, a, L.boltBranches);
                if (L.boltWidth >= 3) VBolt(from, to, VHash(fx.seed + 3, fr), L.boltWidth * 0.4f, Color{255, 255, 255, 255}, a * 0.6f, 1);
                VNorm();
            }
        } else if (fx.kind == FxKind::Mind) {
            Vector2 c = ToScreen(fx.dir, 10);
            Vector2 from = ToScreen(fx.pos, 10);
            VAdd();
            for (int k = 0; k < 4; k++) {   // waves travelling from the user to the target
                float u = std::fmod(t * 2.0f + k * 0.25f, 1.0f);
                Vector2 q = LerpV(from, c, u);
                float r = 3 + u * 5;
                VRingE(q, r, r * 0.8f, 1.0f, Fade(fx.col, 0.7f * (1 - t)), 16);
            }
            for (int k = 0; k < 3; k++) {
                float r = 4 + (t * 18 + k * 5);
                VRingE(c, r, r * 0.7f, 1.4f, Fade(fx.col, (1 - t) * (1 - k * 0.25f)), 24);
            }
            VGlow(c, 10 * (1 - t) + 4, fx.col, 0.6f * (1 - t));
            VNorm();
        } else if (fx.kind == FxKind::Puff) {
            Vector2 c = ToScreen(fx.pos, 6);
            for (int k = 0; k < 8; k++) {
                float a = k * PI_F / 4;
                float r = 4 + t * 12;
                DrawRectangle((int)(c.x + std::cos(a) * r), (int)(c.y + std::sin(a) * r * 0.7f), 2, 2, Fade(fx.col, 1 - t));
            }
        } else {
            DrawFxTop(fx);
        }
    }

    for (const auto& h : hazards) {  // orbiting orbs
        if (!h.orbit) continue;
        Color c = TypeColor(h.hit.type);
        Vector2 o = ToScreen(h.pos, 8 + std::sin(time * 6 + h.orbitAngle) * 1.5f);
        (void)c;
        Vector2 tang{-std::sin(h.orbitAngle), std::cos(h.orbitAngle) * DEPTH};
        VProjectile(o, tang, h.radius * 0.8f, time * 8 + h.orbitAngle, time, GetLook(h.hit.look, h.hit.type), (uint32_t)(h.orbitAngle * 100));
    }
    for (const auto& fx : fxs) {  // breath / spray cones
        if (fx.kind != FxKind::Cone) continue;
        float t = fx.life / fx.maxLife;
        float half = fx.radius * DEG2RAD;
        float reach = fx.len * std::min(1.0f, 0.4f + t * 1.5f);
        Vector2 o = ToScreen(fx.pos + fx.dir * 6.0f, 8);
        int seg = 8;
        for (int k = 0; k < seg; k++) {
            Vector2 d1 = Rot(fx.dir, -half + 2 * half * k / seg), d2 = Rot(fx.dir, -half + 2 * half * (k + 1) / seg);
            Vector2 p1 = ToScreen(fx.pos + d1 * reach, 8), p2 = ToScreen(fx.pos + d2 * reach, 8);
            DrawTriangle(o, p2, p1, Fade(fx.col, 0.28f * (1 - t)));
            DrawTriangle(o, p1, p2, Fade(fx.col, 0.28f * (1 - t)));
        }
        Vector2 e1 = ToScreen(fx.pos + Rot(fx.dir, -half) * reach, 8), e2 = ToScreen(fx.pos + Rot(fx.dir, half) * reach, 8);
        DrawLineEx(e1, e2, 1, Fade(ColorBrightness(fx.col, 0.4f), 0.7f * (1 - t)));
    }

    DrawParticles();

    if (weather != Weather::None) {  // weather overlay over the arena floor area
        float fade = std::min(1.0f, weatherT);
        int top = FLOOR_Y - 10, bottom = FLOOR_BOTTOM;
        if (weather == Weather::Sun) {
            DrawRectangle(0, 0, VW, bottom, Fade(Color{255, 210, 120, 255}, 0.10f * fade));
            for (int i = 0; i < 6; i++) {
                float x = 60.0f + i * 110 + std::sin(time + i) * 10;
                DrawTriangle({x, (float)top}, {x - 30, (float)bottom}, {x + 10, (float)bottom}, Fade(Color{255, 240, 180, 255}, 0.05f * fade));
            }
        } else {
            for (int i = 0; i < 140; i++) {
                float seed = i * 37.17f;
                float speed = weather == Weather::Rain ? 260.0f : (weather == Weather::Snow ? 40.0f : 120.0f);
                float x = std::fmod(seed * 13.3f + (weather == Weather::Sand ? time * 220 : time * 30), (float)VW);
                float y = top + std::fmod(seed * 7.1f + time * speed, (float)(bottom - top));
                if (weather == Weather::Rain) DrawLineEx({x, y}, {x - 3, y + 8}, 1, Fade(Color{150, 190, 255, 255}, 0.55f * fade));
                else if (weather == Weather::Snow) DrawRectangle((int)(x + std::sin(time * 2 + i) * 3), (int)y, 2, 2, Fade(WHITE, 0.8f * fade));
                else DrawRectangle((int)x, (int)y, 3, 1, Fade(Color{210, 180, 120, 255}, 0.6f * fade));
            }
            if (weather == Weather::Rain) DrawRectangle(0, 0, VW, bottom, Fade(Color{40, 60, 120, 255}, 0.10f * fade));
            if (weather == Weather::Sand) DrawRectangle(0, 0, VW, bottom, Fade(Color{180, 140, 80, 255}, 0.12f * fade));
        }
    }

    for (const auto& t : texts) {
        Vector2 sp = ToScreen(t.pos, t.z);
        float a = t.life < t.maxLife * 0.7f ? 1.0f : (t.maxLife - t.life) / (t.maxLife * 0.3f);
        int w = TextW(t.text.c_str(), 10);
        int x = std::max(2, std::min(VW - w - 2, (int)sp.x - w / 2));
        DrawTextOutlined(t.text.c_str(), x, std::max(34, (int)sp.y), 10, Fade(t.col, a), Fade(UI_INK, a));
    }

    DrawHud();

    if (!banners) return;
    if (introT > 0) {
        const char* msg = introT > 0.6f ? "READY..." : "FIGHT!";
        DrawTextCentered(msg, VW / 2, 145, 30, introT > 0.6f ? UI_TEXT : UI_GOLD);
    }
    if (winner >= 0) {
        std::string msg = winner == 2 ? "DRAW!" : TeamLabel(winner) + " WIN" + (TeamLabel(winner).find(" & ") != std::string::npos ? "!" : "S!");
        Color c = winner == 2 ? UI_TEXT : TeamColor(winner);
        DrawTextCentered(msg.c_str(), VW / 2, 145, 30, c);
    }
}

void Battle::DrawHud() const {
    if (weather != Weather::None) {
        const char* wn = weather == Weather::Rain ? "Rain" : weather == Weather::Sun ? "Harsh sun" : weather == Weather::Sand ? "Sandstorm" : "Snow";
        const char* lbl = weatherT > 1000 ? wn : TextFormat("%s %ds", wn, (int)std::ceil(weatherT));
        int w = TextW(lbl, 10);
        int y = field.kind != ArenaKind::Stadium ? 17 : 5;
        DrawRectangle(VW / 2 - w / 2 - 4, y - 1, w + 8, 12, Fade(UI_INK, 0.7f));
        DrawText(lbl, VW / 2 - w / 2, y, 10, Color{170, 210, 255, 255});
    }
    if (field.kind != ArenaKind::Stadium) {
        const char* an = ArenaName(field.kind);
        int w = TextW(an, 10);
        DrawRectangle(VW / 2 - w / 2 - 4, 4, w + 8, 12, Fade(UI_INK, 0.7f));
        DrawText(an, VW / 2 - w / 2, 5, 10, UI_GOLD);
        if (field.kind == ArenaKind::Psychic && field.portalT < 3.0f) {
            const char* m = "portals shifting...";
            DrawText(m, VW / 2 - TextW(m, 10) / 2, 17, 10, Color{230, 120, 240, 255});
        }
    }
    // Which slots get the big move panels at the bottom: players first, then their teammates.
    std::vector<int> bottom;
    if (!IsDoubles()) {
        for (int s = 0; s < N(); s++) if (sides[s].teamId == 0) bottom.push_back(s);
        for (int s = 0; s < N(); s++) if (sides[s].teamId == 1) bottom.push_back(s);
    } else {
        for (int in = 0; in < 2; in++)
            for (int s = 0; s < N(); s++) if (sides[s].input == in) bottom.push_back(s);
        for (int s = 0; s < N(); s++)
            if (sides[s].teamId == 0 && std::find(bottom.begin(), bottom.end(), s) == bottom.end()) bottom.push_back(s);
        for (int s = 0; s < N(); s++)
            if (std::find(bottom.begin(), bottom.end(), s) == bottom.end()) bottom.push_back(s);
    }
    if (bottom.size() > 2) bottom.resize(2);

    int perTeam[2] = {0, 0};
    for (int s = 0; s < N(); s++) {
        const Side& sd = sides[s];
        const Fighter& f = sd.Cur();
        Color tc = TeamColor(sd.teamId);
        bool compact = IsDoubles();
        int pw = 250, ph = compact ? 21 : 29;
        int x = sd.teamId == 0 ? 4 : VW - 4 - pw;
        int y = 3 + perTeam[sd.teamId]++ * (ph + 2);
        const char* who = compact && !sd.tag.empty() ? sd.tag.c_str() : sd.label.c_str();
        DrawPanel(x, y, pw, ph);
        DrawRectangle(x, y, 3, ph, tc);
        DrawTextShadow(who, x + 7, y + (compact ? 1 : 3), 10, tc);
        int nx = x + 9 + TextW(who, 10) + 4;
        DrawTextShadow(f.sp->name.c_str(), nx, y + (compact ? 1 : 3), 10, UI_TEXT);
        int bx = nx + TextW(f.sp->name.c_str(), 10) + 4;
        std::string lv = "Lv" + std::to_string(f.level);
        DrawText(lv.c_str(), bx, y + (compact ? 1 : 3), 10, UI_GOLD);
        bx += TextW(lv.c_str(), 10) + 4;
        if (!compact) {
            bx += DrawTypeBadge(f.sp->type1, bx, y + 2) + 2;
            if (f.sp->type2 != Type::None) DrawTypeBadge(f.sp->type2, bx, y + 2);
        }
        int hy = compact ? y + 13 : y + 17;
        DrawHpBar(x + 7, hy, 110, compact ? 4 : 5, f.displayHp / f.maxHp);
        if (sd.earnXp) {  // XP toward the next level
            int lo = XpForLevel(f.level), hi = XpForLevel(f.level + 1);
            float xf = f.level >= MAX_LEVEL ? 1.0f : (float)(f.xp - lo) / std::max(1, hi - lo);
            int xy = compact ? y + 18 : y + 24;
            DrawRectangle(x + 7, xy, 110, 2, Color{40, 40, 60, 255});
            DrawRectangle(x + 7, xy, (int)(110 * std::max(0.0f, std::min(1.0f, xf))), 2, Color{100, 180, 255, 255});
        }
        char hpbuf[32];
        std::snprintf(hpbuf, sizeof hpbuf, "%d/%d", (int)std::ceil(f.hp), f.maxHp);
        DrawText(hpbuf, x + 122, compact ? y + 10 : y + 16, 10, UI_TEXT);

        char sw[64];
        int nextAlive = sd.NextAlive(sd.active);
        bool out = sd.AliveCount() == 0 && f.state == FState::Fainted;
        int lend = out ? PartnerWithBench(s) : -1;
        if (lend >= 0) {
            if (sd.cpu) std::snprintf(sw, sizeof sw, "taking over...");
            else if (gLocalInput >= 0 && sd.input != gLocalInput) std::snprintf(sw, sizeof sw, "can take over");
            else std::snprintf(sw, sizeof sw, "[%s] take over", gLocalInput >= 0 ? SwapKeyLabel(0, false) : SwapKeyLabel(std::max(0, sd.input), setup.twoPlayer));
        } else if (nextAlive < 0) sw[0] = 0;
        else if (sd.swapCd > 0) std::snprintf(sw, sizeof sw, "swap %.1fs", sd.swapCd);
        else if (sd.cpu || (gLocalInput >= 0 && sd.input != gLocalInput)) std::snprintf(sw, sizeof sw, "swap ready");
        else {
            int pl = gLocalInput >= 0 ? 0 : std::max(0, sd.input);
            bool tp = gLocalInput >= 0 ? false : setup.twoPlayer;
            const char* back = SwapPrevKeyLabel(pl, tp);
            bool hasBack = back[0] && std::string(back) != "-" && sd.PrevAlive(sd.active) != nextAlive;   // only worth showing with 2+ on the bench
            if (hasBack) std::snprintf(sw, sizeof sw, "[%s/%s] swap", back, SwapKeyLabel(pl, tp));
            else std::snprintf(sw, sizeof sw, "[%s] swap", SwapKeyLabel(pl, tp));
        }
        DrawText(sw, x + pw - 6 - TextW(sw, 10), compact ? y + 10 : y + 16, 10, lend >= 0 ? UI_GOLD : (sd.swapCd > 0 ? UI_DIM : UI_GOLD));

        int n = 0;
        for (const auto& m : sd.team) n += !m.lent;
        if (compact) {  // little dots on the first line
            int dxs = x + pw - 6 - n * 7;
            int di = 0;
            for (int i = 0; i < (int)sd.team.size(); i++) {
                const Fighter& m = sd.team[i];
                if (m.lent) continue;
                int dx = dxs + di++ * 7 + 3, dy = y + 5;
                Color dc = m.retired ? UI_GOLD : (m.hp > 0 ? tc : Color{80, 76, 90, 255});
                if (i == sd.active) DrawCircle(dx, dy, 3.2f, WHITE);
                DrawCircle(dx, dy, 2.5f, UI_INK);
                DrawCircle(dx, dy, 1.8f, dc);
            }
        } else {
            int stripW = n * 9 + 3;
            int stripX = sd.teamId == 0 ? x : x + pw - stripW;
            DrawRectangle(stripX, y + ph, stripW, 8, Fade(UI_INK, 0.75f));
            int di = 0;
            for (int i = 0; i < (int)sd.team.size(); i++) {
                const Fighter& m = sd.team[i];
                if (m.lent) continue;
                int dx = stripX + 6 + di++ * 9, dy = y + ph + 4;
                Color dc = m.retired ? UI_GOLD : (m.hp > 0 ? tc : Color{80, 76, 90, 255});
                if (i == sd.active) DrawCircle(dx, dy, 3.8f, WHITE);
                DrawCircle(dx, dy, 3.0f, UI_INK);
                DrawCircle(dx, dy, 2.2f, dc);
            }
        }
    }

    for (size_t bi = 0; bi < bottom.size(); bi++) {
        int s = bottom[bi];
        const Side& sd = sides[s];
        const Fighter& f = sd.Cur();
        Color tc = TeamColor(sd.teamId);
        int mx = bi == 0 ? 4 : 324;
        int my = 310, mw = 312, mh = 47;
        DrawPanel(mx, my, mw, mh);
        DrawRectangle(mx, my, mw, 2, tc);
        if (!sd.cpu && f.state == FState::Active) {   // Dodge cooldown and the Jump key, on a tab above the moves
            bool mine = gLocalInput < 0 || sd.input == gLocalInput;
            int pl = gLocalInput >= 0 ? 0 : std::max(0, sd.input);
            bool tp = gLocalInput >= 0 ? false : setup.twoPlayer;
            char db[40], jb[64];
            if (f.dodgeCd > 0) std::snprintf(db, sizeof db, "dodge %.1fs", f.dodgeCd);
            else if (mine) std::snprintf(db, sizeof db, "[%s] dodge", DodgeKeyLabel(pl, tp));
            else std::snprintf(db, sizeof db, "dodge ready");
            if (mine && IsDoubles()) std::snprintf(jb, sizeof jb, "[%s] jump  [%s] target", JumpKeyLabel(pl, tp), TargetKeyLabel(pl, tp));
            else if (mine) std::snprintf(jb, sizeof jb, "[%s] jump", JumpKeyLabel(pl, tp));
            else jb[0] = 0;
            int dw = TextW(db, 10), jw = *jb ? TextW(jb, 10) + 10 : 0;
            int tw = dw + jw + 10, tx0 = mx + mw - tw;
            if (IsDoubles()) tx0 -= TextW(sd.tag.empty() ? sd.label.c_str() : sd.tag.c_str(), 10) + 10;   // room for the P1 / P2 tag
            DrawRectangle(tx0, my - 12, tw, 12, Fade(UI_INK, 0.85f));
            DrawRectangle(tx0, my - 12, tw, 1, tc);
            if (f.dodgeCd > 0) {   // fills back up
                float fr = 1.0f - f.dodgeCd / DODGE_COOLDOWN;
                DrawRectangle(tx0 + 4, my - 2, (int)((dw + 2) * fr), 1, Color{140, 200, 255, 255});
            }
            DrawText(db, tx0 + 5, my - 11, 10, f.dodgeCd > 0 ? UI_DIM : Color{140, 220, 255, 255});
            if (*jb) DrawText(jb, tx0 + 5 + dw + 10, my - 11, 10, UI_GOLD);
        }

        const Fighter& opp = sides[sd.target >= 0 ? sd.target : AnyFoe(s)].Cur();
        for (int k = 0; k < 4; k++) {
            int sx = mx + 3 + (k % 2) * 154, sy = my + 4 + (k / 2) * 21;
            int sw2 = 152, sh = 20;
            DrawRectangle(sx, sy, sw2, sh, UI_PANEL_LIGHT);
            if (k >= f.nMoves) {
                DrawText("-", sx + 8, sy + 5, 10, UI_DIM);
                continue;
            }
            const MoveDef& m = f.moves[k];
            DrawRectangle(sx, sy, 3, sh, TypeColor(m.type));
            int tx = sx + 6;
            if (!sd.cpu && (gLocalInput < 0 || sd.input == gLocalInput)) {
                const char* key = gLocalInput >= 0 ? AttackKeyLabel(0, k, false) : AttackKeyLabel(std::max(0, sd.input), k, setup.twoPlayer);
                int kw = std::max(10, TextW(key, 10) + 4);
                DrawRectangle(tx, sy + 2, kw, 10, UI_INK);
                DrawText(key, tx + kw / 2 - TextW(key, 10) / 2, sy + 2, 10, UI_GOLD);
                tx += kw + 4;
            }
            DrawText(m.name.c_str(), tx, sy + 1, 10, UI_TEXT);

            char info[48];
            if (IsDamaging(m)) std::snprintf(info, sizeof info, "%s %d", MoveKindName(m.kind), m.power);
            else if (m.maxUses > 0) std::snprintf(info, sizeof info, "%s  %d/%d left", MoveKindName(m.kind), m.maxUses - f.uses[k], m.maxUses);
            else std::snprintf(info, sizeof info, "%s", MoveKindName(m.kind));
            DrawText(info, tx, sy + 10, 10, UI_DIM);

            if (IsDamaging(m) || IsStatusMove(m)) {
                float eff = TypeEffectiveness(m.type, opp.sp->type1, opp.sp->type2);
                if (eff != 1.0f && (IsDamaging(m) || eff == 0)) {
                    char eb[16];
                    if (eff == 0) std::snprintf(eb, sizeof eb, "x0");
                    else if (eff < 1) std::snprintf(eb, sizeof eb, "x%.2g", eff);
                    else std::snprintf(eb, sizeof eb, "x%d", (int)eff);
                    Color ec = eff > 1 ? UI_GOLD : (eff == 0 ? Color{240, 90, 90, 255} : UI_DIM);
                    DrawText(eb, sx + sw2 - 4 - TextW(eb, 10), sy + 10, 10, ec);
                }
            }

            if (!f.HasUses(k)) {
                DrawRectangle(sx, sy, sw2, sh, Fade(UI_INK, 0.6f));
            } else if (f.cd[k] > 0.05f) {
                float full = m.cooldown * f.CooldownMul() * (sd.cpu ? Params(sd.level).cooldownMul : 1.0f);
                float frac = std::min(1.0f, f.cd[k] / full);
                DrawRectangle(sx, sy, (int)(sw2 * frac), sh, Fade(UI_INK, 0.6f));
                char cb[8];
                std::snprintf(cb, sizeof cb, "%.1f", f.cd[k]);
                DrawText(cb, sx + sw2 - 4 - TextW(cb, 10), sy + 1, 10, UI_TEXT);
            }
        }
        if (IsDoubles()) {  // whose moves these are
            const char* t = sd.tag.empty() ? sd.label.c_str() : sd.tag.c_str();
            int tw = TextW(t, 10);
            DrawRectangle(mx + mw - tw - 8, my - 10, tw + 6, 10, Fade(UI_INK, 0.8f));
            DrawText(t, mx + mw - tw - 5, my - 10, 10, sd.input == 1 ? Color{120, 230, 255, 255} : (sd.input == 0 ? UI_GOLD : tc));
        }
    }
}

// How big a creature's body is for hits and bumping: its Pokedex size (the same scale its picture
// is drawn at), so Onix is a big target and Pikachu a small one.
float HitR(const Fighter& f) {
    float r = 9.0f * std::max(0.75f, std::min(1.6f, f.sp->drawScale));
    return f.boss > 0 ? r * 1.25f : r;   // a legendary boss is a bigger target
}
