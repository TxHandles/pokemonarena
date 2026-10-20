// Sweeping beams, tethers, grapples and throws.
//   Sweep   - a beam that swings across an arc while the user stands rooted; each foe is hit once.
//             Jumping over it, dodging through it or ducking behind an obstacle beats it.
//   Tether  - a line latched onto one foe: drains it, binds it on a leash, or pins it (no switching).
//             Running out of range, putting an obstacle in the way or dodging breaks it.
//   Grapple - a vine / tongue / chain shot; when it connects the foe is yanked toward the user.
//   Throw   - grab a foe in reach and hurl it; slamming into a rock, the wall or a hazard hurts more.
// All of it is battle state (saved for online play); the drawing at the bottom is visual only.
#include <algorithm>
#include <cmath>
#include "rng.h"
#include "arena.h"
#include "audio.h"
#include "battle.h"
#include "sprites.h"
#include "ui.h"
#include "vec.h"
#include "vfx.h"

namespace {

constexpr float PI_F = 3.14159265f;
constexpr float BODY_R = 9.0f;
constexpr float TETHER_TICK = 0.35f;

float Frand(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }
Vector2 RandDirC() { float a = Frand(0, 2 * PI_F); return {std::cos(a), std::sin(a)}; }
Vector2 ClampIn(Vector2 p, float margin) {
    return {std::max(margin, std::min(ARENA_W - margin, p.x)), std::max(margin, std::min(ARENA_H - margin, p.y))};
}
float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
float Smooth(float u) { u = Clamp01(u); return u * u * (3 - 2 * u); }
bool HasType(const SpeciesDef* sp, Type t) { return sp->type1 == t || sp->type2 == t; }
int DirFromVec(Vector2 v) {
    if (std::fabs(v.x) >= std::fabs(v.y) * 0.8f) return v.x < 0 ? VIEW_LEFT : VIEW_RIGHT;
    return v.y < 0 ? VIEW_BACK : VIEW_FRONT;
}
Color Cc(int r, int g, int b) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, 255}; }

// How far a line goes before an obstacle stops it (no damage done: a pure check).
float RayLen(const Field& field, Vector2 from, Vector2 dir, float range) {
    float best = range;
    for (const Obstacle& o : field.obs) {
        if (!o.Blocks()) continue;
        float t = Dot(o.pos - from, dir);
        if (t <= 0 || t > best + o.radius) continue;
        float lat = Len(o.pos - (from + dir * t));
        if (lat >= o.radius) continue;
        float th = t - std::sqrt(o.radius * o.radius - lat * lat);
        if (th < best) best = std::max(4.0f, th);
    }
    return best;
}

float SweepAngle(const Fighter& f) {
    float u = f.sweepDur > 0 ? 1.0f - f.sweepT / f.sweepDur : 1.0f;
    return f.sweepA0 + (f.sweepA1 - f.sweepA0) * Smooth(u);
}

// Which picture a line uses, from the move.
enum class LineStyle { Vine, Tongue, Chain, Spirit, Rock, Drain, Dream, Volt, Wing, Rope, Swarm, Shell, Gaze };
LineStyle StyleFor(const std::string& n, Type t) {
    if (n == "Lick") return LineStyle::Tongue;
    if (n == "Anchor Shot") return LineStyle::Chain;
    if (n == "Spirit Shackle") return LineStyle::Spirit;
    if (n == "Smack Down") return LineStyle::Rock;
    if (n == "Dream Eater") return LineStyle::Dream;
    if (n == "Parabolic Charge" || n == "Thunder Cage") return LineStyle::Volt;
    if (n == "Oblivion Wing") return LineStyle::Wing;
    if (n == "Infestation") return LineStyle::Swarm;
    if (n == "Clamp") return LineStyle::Shell;
    if (n == "Mean Look") return LineStyle::Gaze;
    if (n == "Wrap" || n == "Bind") return LineStyle::Rope;
    if (t == Type::Grass) return n == "Vine Whip" || n == "Power Whip" ? LineStyle::Vine : LineStyle::Drain;
    return LineStyle::Rope;
}

} // namespace

// ---------------------------------------------------------------- Sweeping beams

void Battle::StartSweep(int s, int mi, const MoveDef& m, Vector2 aim) {
    Fighter& f = sides[s].Cur();
    float base = std::atan2(aim.y, aim.x);
    float half = std::max(0.2f, m.arc) * 0.5f;
    bool cw = SimRand(0, 1) == 1;
    f.sweepA0 = base + (cw ? -half : half);
    f.sweepA1 = base + (cw ? half : -half);
    f.sweepDur = f.sweepT = std::max(0.2f, m.duration);
    f.sweepMove = mi;
    f.sweepMask = 0;
    if (m.recharge > 0) f.rechargeT += f.sweepDur;   // the recharge starts once the beam is done
    f.knock = aim * -40.0f;
    Shake(2, f.sweepDur);
    const Look& L = GetLook(LookKey(m.name, m.type, m.kind), m.type);
    AddFx(FxKind::Flash, f.pos + aim * 10.0f, 0.12f, L.core, 26, 10);
    AddFx(FxKind::Shock, f.pos, 0.35f, L.glow, 30, 2);
}

void Battle::UpdateSweep(int s, float dt) {
    Fighter& f = sides[s].Cur();
    if (f.sweepMove < 0 || f.sweepMove >= f.nMoves) { f.sweepT = 0; return; }
    const MoveDef& m = f.moves[f.sweepMove];
    f.sweepT = std::max(0.0f, f.sweepT - dt);
    float ang = SweepAngle(f);
    Vector2 dir{std::cos(ang), std::sin(ang)};
    f.aim = dir;
    f.dir = DirFromVec(dir);
    float len = RayLen(field, f.pos, dir, m.range);
    Vector2 end = f.pos + dir * len;
    HitSpec h = Spec(s, m);
    for (int ts : FoeSlots(s)) {
        if (f.sweepMask & (1 << ts)) continue;
        if (!Hittable(ts)) continue;
        const Fighter& e = sides[ts].Cur();
        if (e.JumpZ() > 8) continue;   // jumped over the beam
        if (DistPointSeg(e.pos, f.pos, end) >= m.radius + HitR(e)) continue;
        f.sweepMask |= 1 << ts;
        Vector2 tp = e.pos;
        ApplyHit(h, ts, dir);
        TryElectrify(tp, h);
        Shake(3);
    }
    // the beam scorching across the floor: sparks where it ends, the element along it
    const Look& L = GetLook(h.look, m.type);
    PK k = L.trail == PK::Smoke ? PK::Glow : L.trail;
    Emit(k, f.pos + dir * Frand(14, len), 10, Vector2{-dir.y, dir.x} * Frand(-50, 50) + dir * Frand(10, 60), Frand(0, 30), Frand(0.2f, 0.4f),
         k == PK::Glow ? 1.2f : 1.8f, ColorBrightness(L.glow, Frand(-0.1f, 0.3f)), false).drag = 4;
    if (len < m.range - 2 || SimRand(0, 2) == 0)
        Emit(PK::Spark, end, 8, RandDirC() * Frand(40, 120) - dir * 30.0f, Frand(10, 50), 0.25f, 1.2f, L.core, false).drag = 3;
    if (SimRand(0, 3) == 0) Emit(PK::Smoke, end, 2, RandDirC() * 15.0f, 8, 0.5f, 3, Cc(110, 100, 95), false).grow = 6;
    if (f.sweepT <= 0) {
        FieldReactLine(f.pos, end, h);
        if (len < m.range - 2) BeamLength(f.pos, dir, m.range, h);   // the last stretch strikes the obstacle in the way
    }
}

// ---------------------------------------------------------------- Tethers

void Battle::StartTether(int s, int os, const MoveDef& m, const HitSpec& h, Vector2 aim) {
    Fighter& f = sides[s].Cur();
    // the target: the one you're aiming at if it's in front of you and in reach, else the nearest such foe
    int best = -1;
    float bestScore = 1e9f;
    for (int ts : FoeSlots(s)) {
        if (!Hittable(ts)) continue;
        Vector2 to = sides[ts].Cur().pos - f.pos;
        float d = Len(to);
        if (d > m.range || (d > 4 && Dot(to * (1.0f / d), aim) < 0.35f)) continue;
        float score = d - (ts == os ? 60.0f : 0.0f);
        if (score < bestScore) { bestScore = score; best = ts; }
    }
    Color tc = TypeColor(m.type);
    if (best >= 0) {
        Vector2 to = sides[best].Cur().pos - f.pos;
        float d = Len(to);
        if (RayLen(field, f.pos, to * (1.0f / std::max(1.0f, d)), d) < d - 6) best = -1;   // something in the way
    }
    if (best >= 0 && m.sleepOnly && sides[best].Cur().st.major != Status::Sleep) {
        AddText(f.pos, "But it failed! (it's awake)", UI_DIM, 30);
        Fx& fx = AddFx(FxKind::Lash, f.pos, 0.35f, tc, 4, 10);
        fx.dir = aim;
        fx.len = std::min(m.range, Dist(f.pos, sides[best].Cur().pos));
        fx.look = h.look;
        fx.type = m.type;
        return;
    }
    if (best < 0) {   // nothing to latch onto: the line whips out and snaps back
        Fx& fx = AddFx(FxKind::Lash, f.pos, 0.4f, tc, 4, 10);
        fx.dir = aim;
        fx.len = m.range * 0.8f;
        fx.look = h.look;
        fx.type = m.type;
        fx.seed = (uint32_t)SimRand(1, 0x3fffffff);
        return;
    }
    for (auto& t : tethers)   // one line at a time per battler
        if (t.alive && t.side == s) BreakTether(t, nullptr);
    Fighter& e = sides[best].Cur();
    Tether t;
    t.hit = h;
    t.hit.light = true;
    t.hit.lowers = false;
    t.side = s;
    t.target = best;
    t.ufi = sides[s].active;
    t.tfi = sides[best].active;
    t.kind = m.tether ? m.tether : 1;
    t.life = t.maxLife = std::max(0.5f, m.duration);
    t.range = m.range;
    float d = Dist(f.pos, e.pos);
    t.leash = std::max(34.0f, std::min(d, m.range * 0.7f));
    float ticks = t.maxLife / TETHER_TICK;
    t.tickPower = m.power > 0 ? std::max(1.0f, m.power * (t.kind == 1 ? 1.25f : 1.8f) / ticks) : 0;
    t.hit.statusChance = m.statusChance > 0 ? std::min(1.0f, m.statusChance * 2.5f / ticks) : 0;
    t.tickT = 0.15f;
    t.sleepOnly = m.sleepOnly;
    tethers.push_back(t);
    if (m.lowers || m.power <= 0) ApplyHit(h, best, SafeNorm(e.pos - f.pos));   // Mean Look: lowers speed as it latches
    AddFx(FxKind::Hit, e.pos, 0.2f, tc, 10, 10).type = m.type;
    AddText(e.pos, t.kind == 3 ? "Can't escape!" : (t.kind == 2 ? "Bound!" : "Latched on!"), tc, 34);
    PlaySfx(Sfx::Status);
}

void Battle::BreakTether(Tether& t, const char* why) {
    if (!t.alive) return;
    t.alive = false;
    Vector2 a = sides[t.side].Cur().pos, b = sides[t.target].Cur().pos;
    Vector2 mid = (a + b) * 0.5f;
    Fx& fx = AddFx(FxKind::Snap, mid, 0.35f, TypeColor(t.hit.type), 10, 10);
    fx.dir = SafeNorm(b - a);
    fx.len = Dist(a, b);
    fx.type = t.hit.type;
    if (why) AddText(b, why, UI_TEXT, 36);
}

void Battle::UpdateTethers(float dt) {
    for (auto& t : tethers) {
        if (!t.alive) continue;
        if (t.side >= N() || t.target >= N()) { t.alive = false; continue; }
        Fighter& u = sides[t.side].Cur();
        Fighter& e = sides[t.target].Cur();
        if (sides[t.side].active != t.ufi || sides[t.target].active != t.tfi || u.state != FState::Active || e.state != FState::Active ||
            u.hp <= 0 || e.hp <= 0 || u.burrowed || e.burrowed || winner >= 0) { BreakTether(t, nullptr); continue; }
        t.age += dt;
        t.life -= dt;
        if (t.life <= 0) { BreakTether(t, nullptr); continue; }
        if (e.dodgeT > 0) { BreakTether(t, "Broke free!"); continue; }
        if (t.sleepOnly && e.st.major != Status::Sleep) { BreakTether(t, "Woke up!"); continue; }
        Vector2 to = e.pos - u.pos;
        float d = Len(to);
        Vector2 dir = to * (1.0f / std::max(1.0f, d));
        if (d > 6 && RayLen(field, u.pos, dir, d) < d - 6) { BreakTether(t, "The line snapped!"); continue; }
        if (t.kind == 1) {
            if (d > t.range * 1.15f) { BreakTether(t, "Out of reach!"); continue; }
        } else if (d > t.leash) {   // bound or pinned: held on a leash
            e.pos = u.pos + dir * t.leash;
            e.knock *= 0.5f;
        }
        if (t.kind == 3) e.pinT = std::max(e.pinT, 0.2f);
        t.tickT -= dt;
        if (t.tickT <= 0) {
            t.tickT += TETHER_TICK;
            if (t.tickPower > 0) {
                HitSpec hh = t.hit;
                hh.power = (int)std::lround(t.tickPower);
                ApplyHit(hh, t.target, dir);
            }
            if (t.kind == 2) e.knock += dir * -25.0f;   // a squeeze
        }
    }
    tethers.erase(std::remove_if(tethers.begin(), tethers.end(), [](const Tether& t) { return !t.alive; }), tethers.end());
}

// ---------------------------------------------------------------- Grapples

void Battle::SpawnHook(const HitSpec& h, const MoveDef& m, int s, Vector2 aim) {
    const Fighter& f = sides[s].Cur();
    Projectile p;
    p.hit = h;
    p.pos = f.pos + aim * 11.0f;
    p.vel = aim * (m.speed > 0 ? m.speed : 320.0f);
    p.maxDist = m.range;
    p.radius = std::max(5.0f, m.radius);
    p.hook = m.antiAir ? 2 : 1;
    p.hookPull = m.amount > 0 ? m.amount : 1.0f;
    p.hookPin = m.pin;
    p.hookFrom = f.pos;
    p.z = 9;
    projs.push_back(p);
}

void Battle::HookHit(const Projectile& p, int ts) {
    int s = p.hit.side;
    if (s < 0 || s >= N() || !Targetable(ts)) return;
    Fighter& u = sides[s].Cur();
    Fighter& e = sides[ts].Cur();
    if (u.state != FState::Active) return;
    if (p.hook == 2 && e.jumpT > 0) {   // Smack Down: out of the air
        e.jumpT = 0;
        e.hitstun = std::max(e.hitstun, 0.5f);
        AddText(e.pos, "Knocked down!", Cc(220, 190, 140), 34);
        AddFx(FxKind::Shock, e.pos, 0.35f, Cc(200, 170, 120), 24, 0);
        Shake(2);
    }
    if (p.hookPin > 0 && !HasType(e.sp, Type::Ghost)) {
        if (e.pinT <= 0) AddText(e.pos, "Can't escape!", TypeColor(p.hit.type), 40);
        e.pinT = std::max(e.pinT, p.hookPin);
    }
    for (const auto& g : grabs)
        if (g.alive && g.target == ts) return;   // already being moved
    Vector2 to = u.pos - e.pos;
    float d = Len(to);
    float gap = HitR(u) + HitR(e) + 6;
    if (d <= gap + 2) return;
    Vector2 dir = to * (1.0f / d);
    float pull = (d - gap) * std::max(0.1f, std::min(1.0f, p.hookPull));
    if (e.boss > 0) pull *= 0.35f;   // a legendary barely budges
    Grab g;
    g.hit = p.hit;
    g.by = s;
    g.target = ts;
    g.tfi = sides[ts].active;
    g.kind = 0;
    g.from = e.pos;
    g.to = ClampIn(e.pos + dir * pull, BODY_R + 2);
    g.dur = std::max(0.12f, std::min(0.4f, pull / 360.0f));
    g.height = 5;
    grabs.push_back(g);
    e.heldT = g.dur;
    PlaySfx(Sfx::Thud);
}

// ---------------------------------------------------------------- Throws

void Battle::StartThrow(int s, int ts, const MoveDef& m, const HitSpec& h, Vector2 aim) {
    Fighter& u = sides[s].Cur();
    Fighter& e = sides[ts].Cur();
    for (const auto& g : grabs)
        if (g.alive && g.target == ts) return;
    if (TypeEffectiveness(h.type, e.sp->type1, e.sp->type2) <= 0 && h.ab != Ab::Scrappy) {   // a Ghost slips right through a Fighting grab
        AddText(e.pos, "No effect", UI_DIM, 30);
        AddFx(FxKind::Slash, e.pos, 0.2f, TypeColor(m.type), 12, 8);
        return;
    }
    Grab g;
    g.hit = h;
    g.by = s;
    g.target = ts;
    g.tfi = sides[ts].active;
    g.from = e.pos;
    Vector2 dir = SafeNorm(aim, SafeNorm(e.pos - u.pos, {1, 0}));
    if (m.throwKind == 1) {   // Sky Drop: up, up, then down where it was
        g.kind = 2;
        g.to = ClampIn(e.pos + dir * m.radius, BODY_R + 2);
        g.height = 80;
        g.dur = std::max(0.6f, m.duration);
    } else {
        g.kind = 1;
        float dist = m.radius * (e.boss > 0 ? 0.3f : 1.0f);
        // the flight path: an obstacle in the way stops it short (a slam), the wall does too
        float free = RayLen(field, e.pos, dir, dist);
        Vector2 land = e.pos + dir * free;
        if (free < dist - 2) { g.slam = 1; g.bonus = 1.35f; land = e.pos + dir * std::max(0.0f, free - BODY_R); }
        Vector2 inside = ClampIn(land, BODY_R + 2);
        if (g.slam == 0 && Dist(inside, land) > 3) { g.slam = 2; g.bonus = 1.3f; }
        land = inside;
        if (g.slam == 0 && (field.InZone(land, Zone::LavaPool) || field.InZone(land, Zone::PoisonPool) || field.InSpikes(land))) { g.slam = 3; g.bonus = 1.2f; }
        g.to = land;
        g.height = 22 + Dist(e.pos, land) * 0.12f;
        g.dur = std::max(0.25f, m.duration * (0.6f + 0.4f * Dist(e.pos, land) / std::max(1.0f, dist)));
    }
    grabs.push_back(g);
    e.heldT = g.dur;
    e.pendingMove = -1;
    e.dashing = false;
    u.rechargeT = std::max(u.rechargeT, 0.25f);
    u.knock = dir * 40.0f;
    AddText(e.pos, g.kind == 2 ? "Carried off!" : "Grabbed!", TypeColor(m.type), 34);
    AddFx(FxKind::Hit, e.pos, 0.18f, TypeColor(m.type), 9, 10).type = m.type;
    PlaySfx(Sfx::Hit);
}

bool Battle::Held(int s) const { return sides[s].Cur().heldT > 0; }

void Battle::UpdateGrabs(float dt) {
    for (auto& g : grabs) {
        if (!g.alive) continue;
        if (g.target >= N()) { g.alive = false; continue; }
        Fighter& e = sides[g.target].Cur();
        if (sides[g.target].active != g.tfi || e.state != FState::Active || e.hp <= 0) { e.heldT = e.heldZ = 0; g.alive = false; continue; }
        g.t += dt;
        float u = Clamp01(g.t / g.dur);
        e.pos = g.from + (g.to - g.from) * (g.kind == 2 ? Smooth(u) : (g.kind == 0 ? 1 - (1 - u) * (1 - u) : u));
        if (g.kind == 2) e.heldZ = g.height * (u < 0.65f ? Smooth(u / 0.65f) : 1.0f - ((u - 0.65f) / 0.35f) * ((u - 0.65f) / 0.35f));
        else e.heldZ = g.height * 4 * u * (1 - u);
        e.heldT = std::max(0.0f, g.dur - g.t);
        e.knock = {0, 0};
        e.jumpT = 0;
        if (g.kind != 0 && SimRand(0, 1)) {   // speed lines / dust behind a flying body
            Emit(PK::Smoke, e.pos, e.heldZ * 0.5f + 4, (g.from - g.to) * 0.6f, 0, 0.3f, 2.4f, Cc(220, 215, 205), false).grow = 4;
        }
        if (u < 1) continue;
        g.alive = false;
        e.heldT = e.heldZ = 0;
        if (g.kind == 0) continue;   // a yank: the hit already landed
        // landing: the throw's damage, more when it slammed into something
        HitSpec h = g.hit;
        h.dmgMul *= g.bonus;
        Vector2 dir = SafeNorm(g.to - g.from, {1, 0});
        int ts = g.target;
        ApplyHit(h, ts, dir);
        Shake(g.slam ? 4 : 3);
        PlaySfx(Sfx::Thud);
        AddFx(FxKind::Shock, g.to, 0.4f, Cc(230, 220, 200), g.slam ? 34.0f : 26.0f, 0);
        AddFx(FxKind::Crack, g.to, 1.0f, Cc(60, 45, 30), g.slam ? 22.0f : 16.0f, 0).len = (float)(4 + SimRand(0, 3));
        for (int i = 0; i < 12; i++) {
            Vector2 d = RandDirC();
            Emit(PK::Debris, g.to + d * 4.0f, 2, d * Frand(40, 110), Frand(40, 100), 0.6f, Frand(1.4f, 2.4f), Cc(150, 125, 95), true).spin = Frand(-10, 10);
            Emit(PK::Smoke, g.to + d * 6.0f, 2, d * Frand(15, 40), 6, 0.6f, 3.2f, Cc(200, 190, 175), false).grow = 6;
        }
        if (g.slam == 1) {   // into a rock / pillar / tree: it takes the hit too
            for (Obstacle& o : field.obs)
                if (o.Blocks() && Dist(o.pos, g.to) < o.radius + BODY_R * 2.5f) { ObstacleImpact(o, g.hit, dir, true); break; }
            AddText(g.to, "Slammed!", UI_GOLD, 40);
        } else if (g.slam == 2) {
            AddText(g.to, "Into the wall!", UI_GOLD, 40);
        } else if (g.slam == 3) {
            AddText(g.to, "Into the hazard!", UI_GOLD, 40);
        }
        if (g.kind == 2) AddFx(FxKind::Flash, g.to, 0.1f, WHITE, 30, 4);
    }
    grabs.erase(std::remove_if(grabs.begin(), grabs.end(), [](const Grab& g) { return !g.alive; }), grabs.end());
}

// ---------------------------------------------------------------- Drawing (visual only)

namespace {

// A line between two screen points that waves a little: n points along it.
void WavyPoints(Vector2 a, Vector2 b, float amp, float phase, int n, std::vector<Vector2>& out) {
    out.clear();
    Vector2 d = SafeNorm(b - a, {1, 0});
    Vector2 nrm{-d.y, d.x};
    for (int i = 0; i <= n; i++) {
        float u = i / (float)n;
        float w = std::sin(u * PI_F) * std::sin(u * 9.0f + phase) * amp;
        out.push_back(a + (b - a) * u + nrm * w);
    }
}

void Polyline(const std::vector<Vector2>& pts, float w, Color c) {
    for (size_t i = 1; i < pts.size(); i++) DrawLineEx(pts[i - 1], pts[i], w, c);
}

// One link line in a move's style (screen space). t = time, k = strength 0..1, flow: orbs moving b -> a (drains)
void DrawLink(LineStyle st, Vector2 a, Vector2 b, Color tc, float time, float k, bool drain, uint32_t seed) {
    std::vector<Vector2> pts;
    float len = Dist(a, b);
    int n = std::max(6, (int)(len / 6));
    Vector2 d = SafeNorm(b - a, {1, 0});
    Vector2 nrm{-d.y, d.x};
    switch (st) {
        case LineStyle::Vine: {   // a green vine with leaves along it
            WavyPoints(a, b, 2.2f, time * 10, n, pts);
            Polyline(pts, 3.4f, Fade(Cc(30, 70, 30), k));
            Polyline(pts, 2.0f, Fade(Cc(80, 170, 60), k));
            for (size_t i = 2; i + 1 < pts.size(); i += 3) {
                Vector2 p = pts[i];
                float side = (i / 3) % 2 ? 1.0f : -1.0f;
                Vector2 q = p + nrm * (3.5f * side) + d * 2.0f;
                DrawTriangle(p, q, p + d * 3.5f, Fade(Cc(120, 210, 80), k));
                DrawTriangle(p, p + d * 3.5f, q, Fade(Cc(120, 210, 80), k));
            }
        } break;
        case LineStyle::Tongue: {   // a long pink tongue
            WavyPoints(a, b, 1.5f, time * 14, n, pts);
            Polyline(pts, 4.6f, Fade(Cc(150, 50, 90), k));
            Polyline(pts, 3.0f, Fade(Cc(240, 120, 160), k));
            Polyline(pts, 1.0f, Fade(Cc(255, 190, 210), 0.7f * k));
        } break;
        case LineStyle::Chain: case LineStyle::Spirit: {   // chain links (steel, or ghostly purple)
            bool ghost = st == LineStyle::Spirit;
            Color dark = ghost ? Cc(60, 30, 90) : Cc(60, 64, 72), lite = ghost ? Cc(190, 140, 255) : Cc(200, 205, 215);
            int links = std::max(3, (int)(len / 6));
            for (int i = 0; i < links; i++) {
                float u = (i + 0.5f) / links;
                Vector2 c = a + (b - a) * u + nrm * (std::sin(u * PI_F) * 3.0f);
                bool flat = i % 2 == 0;
                float rx = flat ? 3.2f : 1.4f, ry = flat ? 1.8f : 2.6f;
                VRingE(c, rx + 0.8f, ry + 0.8f, 1.6f, Fade(dark, k), 12);
                VRingE(c, rx, ry, 1.0f, Fade(lite, k), 12);
            }
            if (ghost) { VAdd(); for (int i = 0; i <= 6; i++) VGlow(a + (b - a) * (i / 6.0f), 6, Cc(150, 90, 255), 0.18f * k); VNorm(); }
        } break;
        case LineStyle::Rock: {   // a dusty streak with pebbles
            Polyline({a, b}, 2.0f, Fade(Cc(120, 95, 70), 0.8f * k));
            for (int i = 1; i < 6; i++) DrawCircleV(a + (b - a) * (i / 6.0f) + nrm * std::sin(i * 2.3f + time * 8) * 2.0f, 1.3f, Fade(Cc(170, 145, 110), k));
        } break;
        case LineStyle::Drain: case LineStyle::Dream: case LineStyle::Wing: {
            Color line = st == LineStyle::Dream ? Cc(220, 120, 220) : (st == LineStyle::Wing ? Cc(200, 40, 60) : Cc(110, 230, 120));
            Color orb = st == LineStyle::Dream ? Cc(255, 200, 255) : (st == LineStyle::Wing ? Cc(255, 110, 120) : Cc(200, 255, 170));
            WavyPoints(a, b, st == LineStyle::Dream ? 4.0f : 2.0f, time * 6, n, pts);
            VAdd();
            Polyline(pts, 5.0f, Fade(line, 0.25f * k));
            Polyline(pts, 2.0f, Fade(line, 0.8f * k));
            Polyline(pts, 0.8f, Fade(WHITE, 0.6f * k));
            for (int i = 0; i < 6; i++) {   // life flowing from the foe to the user
                float u = std::fmod(time * 0.9f + i / 6.0f + (seed % 7) * 0.1f, 1.0f);
                float uu = drain ? 1.0f - u : u;
                Vector2 p = a + (b - a) * uu + nrm * std::sin(uu * 12 + time * 5) * 3.0f;
                VGlow(p, 5.5f, orb, 0.6f * k);
                DrawCircleV(p, 1.4f, Fade(WHITE, 0.9f * k));
            }
            if (st == LineStyle::Dream)   // little dream bubbles
                for (int i = 0; i < 3; i++) {
                    float u = std::fmod(time * 0.5f + i / 3.0f, 1.0f);
                    VRingE(a + (b - a) * (1 - u) + Vector2{0, -6.0f * std::sin(u * PI_F)}, 3, 3, 0.8f, Fade(orb, 0.7f * k), 10);
                }
            VNorm();
        } break;
        case LineStyle::Volt: {   // crackling electricity
            uint32_t fr = (uint32_t)(time * 24);
            VBolt(a, b, VHash(seed, fr), 1.6f, Cc(255, 230, 90), 0.9f * k, 1);
            VBolt(a, b, VHash(seed + 7, fr + 3), 0.8f, WHITE, 0.6f * k, 0);
            VNorm();
        } break;
        case LineStyle::Gaze: {   // a thin dark thread of malice
            WavyPoints(a, b, 1.0f, time * 3, n, pts);
            Polyline(pts, 2.0f, Fade(Cc(40, 10, 50), 0.8f * k));
            Polyline(pts, 0.8f, Fade(Cc(180, 60, 200), k));
        } break;
        default: {   // rope / shell grip / swarm: a coiled line in the type's colour
            WavyPoints(a, b, 1.6f, time * 8, n, pts);
            Color dk = ColorBrightness(tc, -0.5f);
            Polyline(pts, 3.2f, Fade(dk, k));
            Polyline(pts, 1.8f, Fade(tc, k));
            for (size_t i = 1; i < pts.size(); i += 2) DrawLineEx(pts[i] - nrm * 1.6f + d * 0.8f, pts[i] + nrm * 1.6f - d * 0.8f, 0.8f, Fade(dk, 0.8f * k));
        } break;
    }
}

// What a held-on foe looks like: coils, a cage, a swarm, a shell grip, an evil eye above it.
void DrawBindOn(LineStyle st, Vector2 c, Color tc, float time, float k, float squeeze) {
    switch (st) {
        case LineStyle::Volt: {   // a cage of lightning bars
            VAdd();
            for (int i = 0; i < 6; i++) {
                float ang = i * PI_F / 3 + time * 1.5f;
                Vector2 base{c.x + std::cos(ang) * 12, c.y + 3 + std::sin(ang) * 4};
                VBolt(base, base + Vector2{0, -22}, VHash((uint32_t)i, (uint32_t)(time * 20)), 1.0f, Cc(255, 230, 90), 0.8f * k, 0);
            }
            VRingE({c.x, c.y + 3}, 13, 5, 1.2f, Fade(Cc(255, 240, 150), 0.8f * k), 20);
            VRingE({c.x, c.y - 19}, 13, 5, 1.2f, Fade(Cc(255, 240, 150), 0.8f * k), 20);
            VNorm();
        } break;
        case LineStyle::Swarm: {   // little bugs buzzing around it
            for (int i = 0; i < 12; i++) {
                float ang = time * (3 + (i % 3)) + i * 0.52f;
                Vector2 p{c.x + std::cos(ang) * (10 + (i % 4) * 2), c.y - 8 + std::sin(ang * 1.3f) * 9};
                DrawRectangleV(p, {1.6f, 1.6f}, Fade(i % 2 ? Cc(120, 160, 50) : Cc(60, 80, 30), k));
            }
        } break;
        case LineStyle::Shell: {   // two shell halves clamped onto it
            float o = 12 - squeeze * 3;
            DrawCircleSector({c.x - o, c.y - 6}, 7, -60, 60, 8, Fade(Cc(150, 160, 210), k));
            DrawCircleSector({c.x + o, c.y - 6}, 7, 120, 240, 8, Fade(Cc(150, 160, 210), k));
        } break;
        case LineStyle::Gaze: {   // an eye watching from above
            Vector2 e{c.x, c.y - 30 + std::sin(time * 2) * 1.5f};
            VAdd();
            VGlow(e, 10, Cc(150, 40, 180), 0.5f * k);
            VNorm();
            DrawEllipse((int)e.x, (int)e.y, 6, 3, Fade(Cc(30, 0, 40), 0.9f * k));
            DrawEllipse((int)e.x, (int)e.y, 4.5f, 2.2f, Fade(Cc(230, 50, 70), k));
            DrawCircleV(e, 1.2f, Fade(BLACK, k));
        } break;
        default: {   // coils of rope / body wrapped around it, squeezing
            Color dk = ColorBrightness(tc, -0.45f);
            for (int i = 0; i < 3; i++) {
                float y = c.y - 4 - i * 6 + std::sin(time * 6 + i) * 0.6f;
                float rx = 11 - squeeze * 2 - i;
                VRingE({c.x, y}, rx + 0.8f, 3.4f, 2.2f, Fade(dk, k), 18);
                VRingE({c.x, y}, rx, 2.8f, 1.2f, Fade(tc, k), 18);
            }
        } break;
    }
}

} // namespace

void Battle::DrawCombatLinks() const {
    // sweeping beams: the scorched wedge already swept, then the beam itself
    for (int s = 0; s < N(); s++) {
        const Fighter& f = sides[s].Cur();
        if (f.sweepT <= 0 || f.sweepMove < 0 || f.sweepMove >= f.nMoves || f.state != FState::Active) continue;
        const MoveDef& m = f.moves[f.sweepMove];
        const Look& L = GetLook(LookKey(m.name, m.type, m.kind), m.type);
        float ang = SweepAngle(f);
        Vector2 dir{std::cos(ang), std::sin(ang)};
        float len = RayLen(field, f.pos, dir, m.range);
        float u = 1.0f - f.sweepT / std::max(0.01f, f.sweepDur);
        Vector2 o = ToScreen(f.pos, 10);
        // the swept area: a fading fan of glow behind the beam
        VAdd();
        int steps = 14;
        for (int i = 0; i < steps; i++) {
            float a0 = f.sweepA0 + (ang - f.sweepA0) * (i / (float)steps);
            float a1 = f.sweepA0 + (ang - f.sweepA0) * ((i + 1) / (float)steps);
            float fade = 0.10f * ((i + 1) / (float)steps);
            float r0 = RayLen(field, f.pos, {std::cos(a0), std::sin(a0)}, m.range);
            float r1 = RayLen(field, f.pos, {std::cos(a1), std::sin(a1)}, m.range);
            Vector2 p0 = ToScreen(f.pos + Vector2{std::cos(a0), std::sin(a0)} * r0, 2);
            Vector2 p1 = ToScreen(f.pos + Vector2{std::cos(a1), std::sin(a1)} * r1, 2);
            Vector2 g = ToScreen(f.pos, 2);
            DrawTriangle(g, p0, p1, Fade(L.glow, fade));
            DrawTriangle(g, p1, p0, Fade(L.glow, fade));
        }
        VNorm();
        // the beam, at full strength while it swings, flaring at the start and fading at the end
        float tt = u < 0.1f ? 0.12f * u / 0.1f : 0.12f + 0.4f * std::max(0.0f, (u - 0.75f) / 0.25f);
        Vector2 a = ToScreen(f.pos + dir * 8.0f, 10), b = ToScreen(f.pos + dir * len, 10);
        VBeam(a, b, m.radius * 1.15f, tt, time, L, (uint32_t)(s * 977 + f.sweepMove));
        VAdd();
        VGlow(b, 16 + std::sin(time * 40) * 2, L.glow, 0.7f);
        VGlow(b, 7, WHITE, 0.8f);
        VGlow(o + Vector2{dir.x * 8, dir.y * 8 * DEPTH}, 14, L.glow, 0.6f);
        VGlow(ToScreen(f.pos + dir * len, 0), 12, L.glow, 0.35f);   // where it scorches the floor
        VNorm();
    }
    // tethers
    for (const auto& t : tethers) {
        if (!t.alive || t.side >= N() || t.target >= N()) continue;
        const Fighter& u = sides[t.side].Cur();
        const Fighter& e = sides[t.target].Cur();
        LineStyle st = StyleFor(t.hit.name, t.hit.type);
        float k = Clamp01(t.age / 0.12f) * Clamp01(t.life / 0.25f);
        Vector2 a = ToScreen(u.pos, 10), b = ToScreen(e.pos, 10 + e.heldZ);
        DrawLink(st, a, b, TypeColor(t.hit.type), time, k, t.kind == 1, (uint32_t)(t.side * 31 + t.target));
        if (t.kind != 1) DrawBindOn(st, ToScreen(e.pos, 0), TypeColor(t.hit.type), time, k, 0.5f + 0.5f * std::sin(t.age * 8));
        else {   // the drained foe glows faintly; the user glows with what it takes
            VAdd();
            VGlow(b, 12 + std::sin(time * 9) * 2, TypeColor(t.hit.type), 0.25f * k);
            VGlow(a, 9, Cc(200, 255, 190), 0.25f * k);
            VNorm();
        }
    }
    // grapple hooks in flight: the line back to the user, then the head
    for (const auto& p : projs) {
        if (!p.hook || !p.alive || p.hit.side < 0 || p.hit.side >= N()) continue;
        const Fighter& u = sides[p.hit.side].Cur();
        LineStyle st = StyleFor(p.hit.name, p.hit.type);
        Vector2 a = ToScreen(u.pos, 9), b = ToScreen(p.pos, p.z);
        DrawLink(st, a, b, TypeColor(p.hit.type), time, 1.0f, false, 7);
        Vector2 d = SafeNorm(b - a, {1, 0});
        Vector2 n{-d.y, d.x};
        switch (st) {
            case LineStyle::Chain: case LineStyle::Spirit: {   // an anchor / shackle on the end
                Color c = st == LineStyle::Spirit ? Cc(190, 140, 255) : Cc(170, 175, 190);
                DrawLineEx(b - d * 4.0f, b + d * 4.0f, 2.2f, c);
                DrawLineEx(b + d * 4.0f - n * 4.0f, b + d * 4.0f + n * 4.0f, 2.0f, c);
                DrawLineEx(b + d * 4.0f - n * 4.0f, b + d * 1.0f - n * 5.0f, 1.6f, c);
                DrawLineEx(b + d * 4.0f + n * 4.0f, b + d * 1.0f + n * 5.0f, 1.6f, c);
                if (st == LineStyle::Spirit) { VAdd(); VGlow(b, 9, Cc(150, 90, 255), 0.6f); VNorm(); }
            } break;
            case LineStyle::Tongue: DrawCircleV(b, 3.2f, Cc(240, 120, 160)); DrawCircleV(b - n * 1.0f, 1.4f, Cc(255, 200, 215)); break;
            case LineStyle::Rock: DrawPoly(b, 5, 3.4f, time * 400, Cc(140, 115, 85)); DrawPolyLines(b, 5, 3.6f, time * 400, Cc(70, 55, 40)); break;
            default: {   // a leafy vine tip
                DrawTriangle(b + d * 5.0f, b - n * 3.0f, b + n * 3.0f, Cc(120, 210, 80));
                DrawTriangle(b + d * 5.0f, b + n * 3.0f, b - n * 3.0f, Cc(120, 210, 80));
            } break;
        }
    }
    // battlers being yanked in or thrown: the line that pulled them, speed lines behind a throw
    for (const auto& g : grabs) {
        if (!g.alive || g.target >= N() || g.by >= N()) continue;
        const Fighter& e = sides[g.target].Cur();
        const Fighter& u = sides[g.by].Cur();
        float uu = Clamp01(g.t / std::max(0.01f, g.dur));
        if (g.kind == 0) {
            LineStyle st = StyleFor(g.hit.name, g.hit.type);
            DrawLink(st, ToScreen(u.pos, 9), ToScreen(e.pos, 10 + e.heldZ), TypeColor(g.hit.type), time, 1.0f - uu * 0.5f, false, 11);
        } else {
            Vector2 c = ToScreen(e.pos, e.heldZ + 10);
            Vector2 back = SafeNorm(g.from - g.to, {-1, 0});
            Vector2 sb{back.x, back.y * DEPTH};
            for (int i = 0; i < 5; i++) {
                Vector2 off{(i - 2) * 3.0f * -sb.y, (i - 2) * 3.0f * sb.x};
                DrawLineEx(c + off + sb * 8.0f, c + off + sb * (16.0f + (i % 2) * 6), 1.0f, Fade(WHITE, 0.6f * (1 - uu)));
            }
            if (g.kind == 2) {   // Sky Drop: carried in the user's grip, wind spiralling around
                VAdd();
                for (int i = 0; i < 4; i++) {
                    float ang = time * 9 + i * PI_F / 2;
                    VGlow(c + Vector2{std::cos(ang) * 12, std::sin(ang) * 5}, 3, Cc(220, 240, 255), 0.6f);
                }
                VNorm();
            }
            // spinning stars around a battler in a throw
            for (int i = 0; i < 3; i++) {
                float ang = time * 10 + i * 2.1f;
                DrawPoly(c + Vector2{std::cos(ang) * 10, -10 + std::sin(ang) * 3}, 4, 1.6f, ang * 60, Fade(UI_GOLD, 0.9f));
            }
        }
    }
}

// ---------------------------------------------------------------- Rolls (pinball)
// The user curls into a ball and rolls along its aim, steering a little. It ricochets off the arena wall,
// rocks, pillars and summoned walls, and bounces off any foe it hits (each foe once per pass: a fresh
// wall bounce lets it hit them again). Every bounce makes it faster and harder-hitting. A Protect stops it
// dead; a dodge or a jump lets it roll past.

namespace {
enum class RollStyle { Rock, Ice, Bug, Steel, Gyro, Spin, Fire, Plain };
RollStyle RollStyleFor(const std::string& n, Type t) {
    if (n == "Rollout") return RollStyle::Rock;
    if (n == "Ice Ball") return RollStyle::Ice;
    if (n == "Steamroller") return RollStyle::Bug;
    if (n == "Steel Roller") return RollStyle::Steel;
    if (n == "Gyro Ball") return RollStyle::Gyro;
    if (n == "Rapid Spin") return RollStyle::Spin;
    if (n == "Flame Wheel") return RollStyle::Fire;
    switch (t) {
        case Type::Rock: case Type::Ground: return RollStyle::Rock;
        case Type::Ice: return RollStyle::Ice;
        case Type::Bug: return RollStyle::Bug;
        case Type::Steel: return RollStyle::Steel;
        case Type::Fire: return RollStyle::Fire;
        default: return RollStyle::Plain;
    }
}
float RollRadius(RollStyle st) { return st == RollStyle::Steel ? 11.0f : (st == RollStyle::Spin ? 9.0f : 10.0f); }
Vector2 Reflect(Vector2 v, Vector2 n) { return v - n * (2 * Dot(v, n)); }
}

void Battle::StartRoll(int s, int mi, const MoveDef& m, Vector2 aim) {
    Fighter& f = sides[s].Cur();
    f.rollMove = mi;
    f.rollT = std::max(0.3f, m.duration);
    f.rollVel = SafeNorm(aim, {1, 0}) * (m.speed > 0 ? m.speed : 250.0f);
    f.rollBoost = 1;
    f.rollMask = 0;
    f.rollBounces = 0;
    f.rollDist = 0;
    f.rollTrailT = 0;
    f.dashing = false;
    Color tc = TypeColor(m.type);
    AddFx(FxKind::Shock, f.pos, 0.3f, tc, 20, 0);
    for (int i = 0; i < 8; i++) {   // a kick of dust as it takes off
        Vector2 d = RandDirC();
        Emit(PK::Smoke, f.pos - aim * 4.0f + d * 3.0f, 1, d * Frand(15, 35) - aim * 40.0f, Frand(4, 12), 0.5f, 2.8f, Cc(195, 185, 170), false).grow = 5;
    }
    PlaySfx(Sfx::Shoot);
}

void Battle::EndRoll(int s, bool bonk) {
    Fighter& f = sides[s].Cur();
    if (f.rollT <= 0) return;
    f.rollT = 0;
    f.knock = f.rollVel * (bonk ? -0.5f : 0.25f);
    f.rechargeT = std::max(f.rechargeT, bonk ? 0.6f : 0.25f);
    if (bonk) {
        f.hitstun = std::max(f.hitstun, 0.5f);
        AddText(f.pos, "Bonk!", UI_DIM, 34);
    }
    AddFx(FxKind::Ring, f.pos, 0.3f, Cc(240, 235, 225), 14, 2);
    for (int i = 0; i < 6; i++) {
        Vector2 d = RandDirC();
        Emit(PK::Smoke, f.pos + d * 4.0f, 2, d * Frand(15, 40), Frand(5, 15), 0.5f, 3.0f, Cc(200, 192, 180), false).grow = 6;
    }
}

void Battle::UpdateRoll(int s, float dt, Vector2 steer) {
    Fighter& f = sides[s].Cur();
    if (f.rollMove < 0 || f.rollMove >= f.nMoves) { f.rollT = 0; return; }
    const MoveDef& m = f.moves[f.rollMove];
    if (f.hitstun > 0.2f) { EndRoll(s, false); return; }   // knocked out of it
    RollStyle st = RollStyleFor(m.name, m.type);
    float R = RollRadius(st);
    float base = m.speed > 0 ? m.speed : 250.0f;
    f.rollT -= dt;
    // steering: turn a little toward the stick / the CPU's chosen direction
    float sl = Len(steer);
    float speed = Len(f.rollVel);
    if (sl > 0.15f && speed > 1) {
        Vector2 want = steer * (1.0f / sl);
        float cur = std::atan2(f.rollVel.y, f.rollVel.x), tgt = std::atan2(want.y, want.x);
        float da = std::remainder(tgt - cur, 2 * PI_F);
        float turn = 2.2f * dt;
        cur += std::max(-turn, std::min(turn, da));
        f.rollVel = Vector2{std::cos(cur), std::sin(cur)} * speed;
    }
    Vector2 dir = SafeNorm(f.rollVel, {1, 0});
    f.pos += f.rollVel * dt;
    f.rollDist += speed * dt;
    f.aim = dir;
    f.dir = DirFromVec(dir);
    f.animT += dt;
    HitSpec h = Spec(s, m);
    h.dmgMul *= f.rollBoost;
    Color tc = TypeColor(m.type);

    auto bounce = [&](Vector2 n, Vector2 at, bool wall) {
        f.rollVel = Reflect(f.rollVel, n);
        float sp = std::min(base * 1.45f, Len(f.rollVel) * 1.07f);
        f.rollVel = SafeNorm(f.rollVel, n) * sp;
        f.rollBounces++;
        f.rollBoost += m.amount;
        if (wall) f.rollMask = 0;   // a fresh pass: the foes can be hit again
        f.rollT = std::max(f.rollT, 0.35f);   // a ricochet always gets to carry on a little
        AddFx(FxKind::Shock, at, 0.3f, tc, 16 + f.rollBounces * 2.0f, 4);
        Color spark = st == RollStyle::Ice ? Cc(220, 245, 255) : (st == RollStyle::Fire ? Cc(255, 190, 80) : Cc(255, 240, 190));
        int n2 = st == RollStyle::Gyro || st == RollStyle::Steel ? 12 : 7;
        for (int i = 0; i < n2; i++) {
            Vector2 d = SafeNorm(n + RandDirC() * 0.8f, n);
            Emit(PK::Spark, at, 6, d * Frand(60, 160), Frand(10, 50), 0.25f, 1.2f, spark, false).drag = 3;
        }
        PK chip = st == RollStyle::Ice ? PK::Shard : (st == RollStyle::Fire ? PK::Ember : PK::Debris);
        for (int i = 0; i < 4; i++)
            Emit(chip, at, 4, SafeNorm(n + RandDirC(), n) * Frand(30, 80), Frand(30, 70), 0.5f, 1.5f,
                 st == RollStyle::Ice ? Cc(200, 235, 255) : (st == RollStyle::Fire ? Cc(255, 140, 40) : Cc(150, 125, 95)), chip != PK::Ember).spin = Frand(-10, 10);
        Shake(std::min(4, 1 + f.rollBounces / 2), 0.15f);
        PlaySfx(Sfx::Thud);
        if (f.rollBounces >= 2) AddText(at, TextFormat("x%.1f", f.rollBoost), UI_GOLD, 24);
    };

    // the arena wall
    if (f.pos.x < R) { f.pos.x = R; bounce({1, 0}, {0, f.pos.y}, true); }
    else if (f.pos.x > ARENA_W - R) { f.pos.x = ARENA_W - R; bounce({-1, 0}, {ARENA_W, f.pos.y}, true); }
    if (f.pos.y < R) { f.pos.y = R; bounce({0, 1}, {f.pos.x, 0}, true); }
    else if (f.pos.y > ARENA_H - R) { f.pos.y = ARENA_H - R; bounce({0, -1}, {f.pos.x, ARENA_H}, true); }
    // rocks, pillars, trees, summoned walls (heavy rollers can crack them)
    for (Obstacle& o : field.obs) {
        if (!o.Blocks()) continue;
        Vector2 to = f.pos - o.pos;
        float d = Len(to);
        if (d >= o.radius + R * 0.8f) continue;
        Vector2 n = d > 0.01f ? to * (1.0f / d) : -dir;
        f.pos = o.pos + n * (o.radius + R * 0.8f + 0.5f);
        if (Dot(f.rollVel, n) < 0) {
            Vector2 at = o.pos + n * o.radius;
            bounce(n, at, true);
            ObstacleImpact(o, h, -n, true);
        }
    }
    // foes: hit and bounce off them
    for (int ts : FoeSlots(s)) {
        if (f.rollMask & (1 << ts)) continue;
        if (!Hittable(ts)) continue;
        Fighter& e = sides[ts].Cur();
        if (e.JumpZ() > 8) continue;   // hopped over it
        Vector2 to = f.pos - e.pos;
        float d = Len(to);
        if (d >= R + HitR(e)) continue;
        f.rollMask |= 1 << ts;
        if (e.shieldT > 0) {   // Protect: stopped dead
            ApplyHit(h, ts, dir);
            EndRoll(s, true);
            return;
        }
        Vector2 ep = e.pos;
        float dealt = ApplyHit(h, ts, dir);
        TryElectrify(ep, h);
        if (dealt > 0) {
            Vector2 n = d > 0.01f ? to * (1.0f / d) : -dir;
            f.pos = e.pos + n * (R + HitR(e) + 0.5f);
            bounce(n, e.pos + n * HitR(e), false);
            AddFx(FxKind::Hit, ep, 0.2f, tc, 14, 8).type = m.type;
        }
    }
    // rolling over the field: sweep away hazards, leave a trail of fire
    if (m.clears) {
        for (auto& hz : hazards) {
            if (hz.orbit || hz.life <= 0 || hz.hit.side == s || !Foes(s, hz.hit.side)) continue;
            if (Dist(hz.pos, f.pos) > hz.radius + R) continue;
            hz.life = 0;
            AddFx(FxKind::Ring, hz.pos, 0.3f, Cc(240, 240, 240), hz.radius + 6, 1);
            for (int i = 0; i < 6; i++) Emit(PK::Smoke, hz.pos + RandDirC() * 4.0f, 2, RandDirC() * 40.0f + dir * 40.0f, 10, 0.5f, 3.0f, Cc(210, 205, 195), false).grow = 6;
            AddText(hz.pos, "Swept away!", UI_TEXT, 30);
        }
    }
    f.rollTrailT -= dt;
    if (m.trail && f.rollTrailT <= 0) {
        f.rollTrailT = 0.12f;
        HitSpec ph = h;
        ph.patchPower = std::max(4, m.power / 8);
        ph.patchRadius = 8;
        ph.patchTime = 1.8f;
        ph.dmgMul = 1;
        SpawnPatch(ph, f.pos - dir * R);
    }
    // what it throws up as it rolls
    {
        Vector2 back = f.pos - dir * (R * 0.8f);
        switch (st) {
            case RollStyle::Ice:
                Emit(PK::Snow, back + RandDirC() * 3.0f, 2, -dir * 20.0f + RandDirC() * 15.0f, Frand(5, 20), 0.5f, 1.4f, Cc(225, 245, 255), false);
                if (SimRand(0, 2) == 0) Emit(PK::Glow, back, 3, RandDirC() * 10.0f, 5, 0.4f, 1.0f, Cc(170, 225, 255), false);
                break;
            case RollStyle::Fire:
                Emit(PK::Flame, back + RandDirC() * 3.0f, Frand(3, 9), -dir * 30.0f + RandDirC() * 10.0f, Frand(15, 35), Frand(0.25f, 0.4f), Frand(1.6f, 2.4f), Cc(255, 150, 50), false).drag = 4;
                if (SimRand(0, 1)) Emit(PK::Ember, back, 6, RandDirC() * 30.0f, Frand(20, 50), 0.6f, 1.0f, Cc(255, 200, 90), false);
                break;
            case RollStyle::Gyro: case RollStyle::Steel:
                if (SimRand(0, 1)) Emit(PK::Spark, back, 1, RandDirC() * Frand(40, 100) - dir * 40.0f, Frand(20, 50), 0.2f, 1.0f, Cc(255, 230, 160), false).drag = 3;
                if (SimRand(0, 2) == 0) Emit(PK::Smoke, back, 1, -dir * 15.0f, 4, 0.5f, 2.6f, Cc(190, 185, 180), false).grow = 4;
                break;
            case RollStyle::Spin:
                if (SimRand(0, 1)) Emit(PK::Ring, f.pos, R, {0, 0}, 0, 0.25f, 1.4f, Cc(235, 240, 255), false).grow = 14;
                break;
            default:   // dust and pebbles
                if (SimRand(0, 1)) Emit(PK::Smoke, back, 1, -dir * 20.0f + RandDirC() * 10.0f, 4, 0.5f, 2.6f, Cc(185, 170, 150), false).grow = 4;
                if (SimRand(0, 3) == 0) Emit(PK::Debris, back, 2, RandDirC() * 30.0f - dir * 20.0f, Frand(20, 40), 0.4f, 1.2f, Cc(140, 115, 85), true).spin = Frand(-10, 10);
                break;
        }
    }
    if (f.rollT <= 0 || f.rollBounces > m.bounces) EndRoll(s, false);
}

void Battle::DrawRollBall(int s) const {
    const Fighter& f = sides[s].Cur();
    if (f.rollMove < 0 || f.rollMove >= f.nMoves) return;
    const MoveDef& m = f.moves[f.rollMove];
    RollStyle st = RollStyleFor(m.name, m.type);
    float R = RollRadius(st);
    float ang = f.rollDist / R;   // how far it has turned
    Vector2 dir = SafeNorm(f.rollVel, {1, 0});
    Vector2 sd = SafeNorm(Vector2{dir.x, dir.y * DEPTH}, {1, 0});
    Color tc = TypeColor(m.type);
    float hop = std::fabs(std::sin(f.rollDist * 0.09f)) * 1.5f;   // a little bounce as it rolls over bumps
    Vector2 g = ToScreen(f.pos);
    Vector2 c = ToScreen(f.pos, R * 0.9f + hop);
    VShadow({g.x, g.y + 1}, R * 1.05f, R * 0.4f, 0.55f);

    // after-images streaming behind it
    for (int k = 3; k >= 1; k--) {
        Vector2 p = c - sd * (R * 0.9f * k);
        DrawCircleV(p, R * (1 - k * 0.12f), Fade(ColorBrightness(tc, 0.2f), 0.12f * (4 - k)));
    }
    float speedK = Clamp01((Len(f.rollVel) - 150) / 200);
    for (int i = 0; i < 4; i++) {   // speed lines
        float off = (i - 1.5f) * R * 0.5f;
        Vector2 o{-sd.y * off, sd.x * off};
        DrawLineEx(c + o - sd * (R + 2.0f), c + o - sd * (R + 8.0f + 8 * speedK + (i % 2) * 4), 1.0f, Fade(WHITE, 0.35f + 0.3f * speedK));
    }
    // the body: a solid ball, then its pattern turning with the roll, then light on top
    auto stripes = [&](Color col, int n, float w) {   // bands that sweep across the face as it turns
        for (int i = 0; i < n; i++) {
            float ph = std::fmod(ang + i * 2 * PI_F / n, 2 * PI_F);
            if (ph > PI_F) continue;   // on the far side
            float x = std::cos(ph) * R * 0.92f;
            float half = std::sqrt(std::max(0.0f, R * R - x * x)) * 0.92f;
            Vector2 a = c + sd * x + Vector2{-sd.y, sd.x} * half, b = c + sd * x - Vector2{-sd.y, sd.x} * half;
            DrawLineEx(a, b, w * (0.4f + 0.6f * std::sin(ph)), col);
        }
    };
    switch (st) {
        case RollStyle::Rock: {
            DrawCircleV(c, R + 1, Cc(70, 55, 40));
            DrawCircleV(c, R, Cc(150, 120, 85));
            stripes(Cc(110, 85, 60), 4, 2.4f);
            for (int i = 0; i < 5; i++) {   // pebbly flecks
                float a2 = ang + i * 1.3f;
                if (std::sin(a2) < 0) continue;
                DrawCircleV(c + sd * (std::cos(a2) * R * 0.7f) + Vector2{-sd.y, sd.x} * ((i - 2) * R * 0.3f), 1.2f, Cc(185, 160, 120));
            }
            DrawCircleV(c + Vector2{-R * 0.35f, -R * 0.4f}, R * 0.3f, Fade(Cc(210, 190, 150), 0.6f));
        } break;
        case RollStyle::Ice: {
            DrawCircleV(c, R + 1, Cc(70, 120, 170));
            DrawCircleV(c, R, Cc(170, 220, 250));
            stripes(Cc(235, 250, 255), 5, 1.4f);
            for (int i = 0; i < 6; i++) {   // facets
                float a2 = i * PI_F / 3 + ang * 0.5f;
                DrawLineEx(c, c + Vector2{std::cos(a2), std::sin(a2)} * R * 0.85f, 0.7f, Fade(WHITE, 0.45f));
            }
            VAdd();
            VGlow(c, R * 1.8f, Cc(150, 210, 255), 0.22f);
            float tw = 0.5f + 0.5f * std::sin(time * 14);
            VGlow(c + Vector2{-R * 0.4f, -R * 0.45f}, 3 + tw * 2, WHITE, 0.9f);
            VNorm();
        } break;
        case RollStyle::Bug: {   // a segmented shell wheel
            DrawCircleV(c, R + 1, Cc(50, 25, 50));
            DrawCircleV(c, R, Cc(140, 60, 120));
            for (int i = 0; i < 8; i++) {
                float a2 = ang + i * PI_F / 4;
                Vector2 e{std::cos(a2), std::sin(a2)};
                DrawLineEx(c + e * (R * 0.35f), c + e * R, 1.4f, Cc(70, 30, 70));
                DrawCircleV(c + e * (R * 0.72f), 1.3f, Cc(240, 200, 60));
            }
            DrawCircleV(c, R * 0.35f, Cc(90, 40, 90));
            DrawCircleV(c, R * 0.18f, Cc(240, 200, 60));
        } break;
        case RollStyle::Steel: {   // a heavy metal drum with rivets and a glint sweeping across
            DrawCircleV(c, R + 1.2f, Cc(50, 55, 65));
            DrawCircleV(c, R, Cc(150, 158, 172));
            DrawCircleV(c, R * 0.72f, Cc(120, 128, 142));
            for (int i = 0; i < 6; i++) {
                float a2 = ang + i * PI_F / 3;
                DrawCircleV(c + Vector2{std::cos(a2), std::sin(a2)} * (R * 0.86f), 1.2f, Cc(220, 225, 235));
            }
            DrawLineEx(c + Vector2{std::cos(ang), std::sin(ang)} * (R * 0.7f), c - Vector2{std::cos(ang), std::sin(ang)} * (R * 0.7f), 1.6f, Cc(90, 96, 110));
            VAdd();
            float gl = std::fmod(time * 1.5f, 1.0f);
            VGlow(c + Vector2{(gl - 0.5f) * R * 1.6f, -R * 0.4f}, 4, WHITE, 0.8f);
            VGlow(c, R * 1.8f, Cc(200, 210, 230), 0.25f);
            VNorm();
        } break;
        case RollStyle::Gyro: {   // a spinning top, tilted, with rings whirling round its rim
            Vector2 top = c + Vector2{0, -R * 0.2f};
            DrawEllipse((int)top.x, (int)top.y, R * 1.25f, R * 0.7f, Cc(60, 66, 80));
            DrawEllipse((int)top.x, (int)top.y - 1, R * 1.15f, R * 0.6f, Cc(160, 170, 190));
            for (int i = 0; i < 6; i++) {
                float a2 = ang * 2 + i * PI_F / 3;
                Vector2 p{top.x + std::cos(a2) * R * 0.95f, top.y - 1 + std::sin(a2) * R * 0.5f};
                DrawCircleV(p, 1.6f, std::sin(a2) > 0 ? Cc(255, 220, 120) : Cc(110, 118, 135));
            }
            DrawTriangle(Vector2{top.x - R * 0.6f, top.y + 1}, Vector2{top.x, top.y + R * 1.0f}, Vector2{top.x + R * 0.6f, top.y + 1}, Cc(100, 108, 125));
            DrawCircleV(Vector2{top.x, top.y - 1}, 2.2f, Cc(255, 230, 140));
            VAdd();
            VRingE(top, R * 1.5f, R * 0.75f, 1.0f, Fade(Cc(255, 230, 160), 0.6f), 24);
            VGlow({top.x, top.y + R}, 7, Cc(255, 220, 140), 0.6f);   // sparks where it grinds the floor
            VNorm();
        } break;
        case RollStyle::Spin: {   // a shell spinning so fast it's a blur of whirl lines
            DrawCircleV(c, R + 1, Cc(70, 75, 95));
            DrawCircleV(c, R, ColorBrightness(tc, 0.15f));
            for (int i = 0; i < 3; i++) {
                float a0 = ang * 2 + i * 2 * PI_F / 3;
                Vector2 prev = c;
                for (int k = 1; k <= 6; k++) {
                    float r = R * k / 6.0f, a2 = a0 + k * 0.45f;
                    Vector2 p = c + Vector2{std::cos(a2), std::sin(a2)} * r;
                    DrawLineEx(prev, p, 1.2f, Fade(WHITE, 0.75f));
                    prev = p;
                }
            }
            VAdd();
            VRingE(c, R * 1.4f, R * 1.4f * 0.6f, 1.2f, Fade(WHITE, 0.5f), 24);
            VNorm();
        } break;
        case RollStyle::Fire: {   // a wheel of fire: a dark core inside a ring of tongues of flame
            DrawCircleV(c, R * 0.8f, Cc(90, 30, 20));
            DrawCircleV(c, R * 0.55f, Cc(150, 50, 25));
            VAdd();
            for (int i = 0; i < 10; i++) {
                float a2 = ang + i * PI_F / 5;
                float fl = 0.6f + 0.4f * std::sin(time * 25 + i * 1.7f);
                Vector2 e{std::cos(a2), std::sin(a2)};
                Vector2 tip = c + e * (R * (1.0f + 0.45f * fl)) - sd * (R * 0.35f * fl);   // flames lean back
                DrawTriangle(c + Vector2{-e.y, e.x} * 3.0f + e * (R * 0.6f), tip, c - Vector2{-e.y, e.x} * 3.0f + e * (R * 0.6f), Fade(Cc(255, 140, 40), 0.8f));
                DrawTriangle(c - Vector2{-e.y, e.x} * 3.0f + e * (R * 0.6f), tip, c + Vector2{-e.y, e.x} * 3.0f + e * (R * 0.6f), Fade(Cc(255, 140, 40), 0.8f));
            }
            VRingE(c, R * 0.85f, R * 0.85f, 2.0f, Fade(Cc(255, 220, 120), 0.9f), 24);
            VGlow(c, R * 2.6f, Cc(255, 120, 30), 0.55f);
            VNorm();
        } break;
        default: {
            DrawCircleV(c, R + 1, ColorBrightness(tc, -0.5f));
            DrawCircleV(c, R, tc);
            stripes(ColorBrightness(tc, -0.3f), 4, 2.0f);
        } break;
    }
    if (st != RollStyle::Gyro) {   // a highlight on top, the type's light around it
        DrawCircleV(c + Vector2{-R * 0.35f, -R * 0.42f}, R * 0.22f, Fade(WHITE, 0.45f));
        VAdd();
        VGlow(c, R * 1.7f, tc, 0.25f + 0.1f * (f.rollBoost - 1));
        VNorm();
    }
    if (f.flash > 0) DrawCircleV(c, R + 1, Fade(WHITE, 0.7f));
    if (f.rollBoost > 1.29f) {   // charged up from bounces: a hot rim
        VAdd();
        VRingE(c, R + 2, R + 2, 1.2f, Fade(UI_GOLD, std::min(0.9f, (f.rollBoost - 1) * 0.8f)), 24);
        VNorm();
    }
}
