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
