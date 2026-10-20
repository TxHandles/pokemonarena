// Gym centerpieces: one big interactive set piece in the middle of each of the eight gyms.
// See struct Centerpiece (field.h) for what each one does. All of it is battle state (saved for
// online play); the Draw functions at the bottom are visual only.
#include <algorithm>
#include <cmath>
#include "rng.h"
#include "arena.h"
#include "audio.h"
#include "battle.h"
#include "ui.h"
#include "vec.h"
#include "vfx.h"

namespace {

constexpr float PI_F = 3.14159265f;
float Frand(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }
Vector2 RandDirE() { float a = Frand(0, 2 * PI_F); return {std::cos(a), std::sin(a)}; }
Color Cc(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
bool HasT(const SpeciesDef* sp, Type t) { return sp->type1 == t || sp->type2 == t; }
float Cross(Vector2 a, Vector2 b) { return a.x * b.y - a.y * b.x; }
Vector2 RotV(Vector2 v, float a) { float c = std::cos(a), s = std::sin(a); return {v.x * c - v.y * s, v.x * s + v.y * c}; }

const Color kOrbIdle = {200, 160, 255, 255};
const Color kSludge = {150, 70, 190, 255};
const Color kPollen = {255, 190, 220, 255};
const Color kVolt = {255, 230, 90, 255};
const Color kMilk = {250, 248, 236, 255};
const Color kBeam = {255, 244, 190, 255};
const Color kSwarm = {200, 210, 70, 255};
float AngleTo(float from, float to) { return std::remainder(to - from, 2 * PI_F); }

} // namespace

Obstacle* Battle::CenterBody() {
    for (Obstacle& o : field.obs) if (o.kind == Obstacle::Center) return &o;
    return nullptr;
}

// Who is standing on / caught by a floor centerpiece (grounded, active, not mid-jump).
static bool Grounded(const Fighter& f) { return f.state == FState::Active && !f.burrowed && f.jumpT <= 0 && f.heldZ <= 0 && !IsAirborne(*f.sp); }

void Battle::UpdateCenterpiece(float dt) {
    Centerpiece& c = field.center;
    if (c.kind == Centerpiece::None || winner >= 0) return;
    c.hitCd = std::max(0.0f, c.hitCd - dt);
    c.flash = std::max(0.0f, c.flash - dt);
    if (introT > 0) return;
    Obstacle* body = CenterBody();
    switch (c.kind) {
        // ---------------------------------------------------------------- Brock: the great boulder
        case Centerpiece::Boulder: {
            if (c.phase == 1) {   // rolling
                float sp = Len(c.vel);
                c.pos += c.vel * dt;
                c.spin += sp * dt / c.radius;
                c.vel *= std::exp(-0.25f * dt);
                bool wall = false;
                if (c.pos.x < c.radius) { c.pos.x = c.radius; c.vel.x = std::fabs(c.vel.x); wall = true; }
                if (c.pos.x > ARENA_W - c.radius) { c.pos.x = ARENA_W - c.radius; c.vel.x = -std::fabs(c.vel.x); wall = true; }
                if (c.pos.y < c.radius) { c.pos.y = c.radius; c.vel.y = std::fabs(c.vel.y); wall = true; }
                if (c.pos.y > ARENA_H - c.radius) { c.pos.y = ARENA_H - c.radius; c.vel.y = -std::fabs(c.vel.y); wall = true; }
                HitSpec h = EnvSpec(Type::Rock, 80);
                h.knock = 1.8f;
                h.heavy = true;
                h.name = "Great Boulder";
                for (Obstacle& o : field.obs) {   // it smashes through pillars in its way
                    if (&o == body || !o.Blocks()) continue;
                    Vector2 to = c.pos - o.pos;
                    float d = Len(to);
                    if (d >= o.radius + c.radius) continue;
                    ObstacleImpact(o, h, SafeNorm(c.vel, {1, 0}), true);
                    if (o.Blocks()) {   // still standing: bounce off it
                        Vector2 n = SafeNorm(to, {1, 0});
                        c.pos = o.pos + n * (o.radius + c.radius + 0.5f);
                        c.vel = c.vel - n * (2 * Dot(c.vel, n));
                        wall = true;
                    }
                }
                if (wall) {
                    c.bounces++;
                    c.mask = 0;
                    c.vel *= 0.75f;
                    Shake(3);
                    PlaySfx(Sfx::Thud);
                    Boom(c.pos, 16, Impact::Debris, 4);
                }
                for (int ts = 0; ts < N(); ts++) {   // crushes anyone in its path (you included!)
                    if (c.mask & (1 << ts) || !Hittable(ts)) continue;
                    Fighter& e = sides[ts].Cur();
                    if (e.JumpZ() > 10) continue;
                    if (Dist(e.pos, c.pos) >= c.radius + HitR(e)) continue;
                    c.mask |= 1 << ts;
                    ApplyHit(h, ts, SafeNorm(c.vel, {1, 0}));
                    AddText(e.pos, "Crushed!", Cc(220, 190, 150), 40);
                    Shake(4);
                }
                if (SimRand(0, 1)) {
                    Vector2 back = c.pos - SafeNorm(c.vel, {1, 0}) * c.radius;
                    Emit(PK::Smoke, back + RandDirE() * 6.0f, 2, RandDirE() * 20.0f, 6, 0.7f, 4.0f, Cc(180, 165, 140), false).grow = 8;
                    if (SimRand(0, 2) == 0) Emit(PK::Debris, back, 3, RandDirE() * 40.0f, Frand(30, 60), 0.5f, 1.8f, Cc(140, 115, 85), true).spin = Frand(-9, 9);
                }
                if (body) { body->pos = c.pos; }
                if (c.bounces >= 3 || Len(c.vel) < 60) {   // shatters
                    c.phase = 2;
                    c.t = 16;
                    if (body) body->alive = false;
                    Boom(c.pos, 34, Impact::Debris, 6);
                    for (int i = 0; i < 18; i++) {
                        Vector2 d = RandDirE();
                        Emit(PK::Debris, c.pos + d * 8.0f, Frand(4, 20), d * Frand(40, 140), Frand(40, 120), 0.9f, Frand(2, 3.5f), Cc(150, 125, 95), true).spin = Frand(-12, 12);
                    }
                    AddText(c.pos, "The boulder shattered!", UI_GOLD, 44);
                    Shake(4);
                    PlaySfx(Sfx::Thud);
                }
            } else if (c.phase == 2) {   // broken: a new one rises at the centre
                c.t -= dt;
                if (c.t <= 0) {
                    c.phase = 3;
                    c.t = 1.2f;
                    c.pos = c.home;
                    c.vel = {0, 0};
                    c.spin = 0;
                    AddText(c.home, "A new boulder is rising!", UI_TEXT, 30);
                }
            } else if (c.phase == 3) {
                c.t -= dt;
                for (int s = 0; s < N(); s++) {   // shoves anyone standing on the spot out of the way
                    Fighter& f = sides[s].Cur();
                    if (f.state != FState::Active) continue;
                    Vector2 to = f.pos - c.home;
                    float d = Len(to);
                    if (d < c.radius + 10) f.pos = c.home + SafeNorm(to, {1, 0}) * (c.radius + 10);
                }
                if (SimRand(0, 1)) Emit(PK::Debris, c.home + RandDirE() * c.radius, 1, RandDirE() * 25.0f, Frand(30, 60), 0.5f, 1.6f, Cc(130, 105, 80), true);
                if (c.t <= 0) {
                    c.phase = 0;
                    if (body) { body->alive = true; body->pos = c.home; }
                    Shake(2);
                    PlaySfx(Sfx::Thud);
                }
            }
        } break;

        // ---------------------------------------------------------------- Misty: the whirlpool drain
        case Centerpiece::Drain: {
            c.t -= dt;
            c.spin += c.spinRate * dt;
            if (c.phase == 0) {
                c.spinRate += (0.4f - c.spinRate) * std::min(1.0f, dt * 2);
                if (c.t <= 0) {
                    c.phase = 1;
                    c.t = 2.0f;
                    AddText({ARENA_W / 2, ARENA_H * 0.1f}, "The drain is opening - a whirlpool is coming!", Cc(140, 200, 255), 20);
                    PlaySfx(Sfx::Status);
                }
            } else if (c.phase == 1) {
                c.spinRate += (3.0f - c.spinRate) * std::min(1.0f, dt * 1.5f);
                if (c.t <= 0) { c.phase = 2; c.t = 5.0f; Shake(2, 0.3f); }
            } else if (c.phase == 2) {   // the whirlpool: drags everyone in, the eye of it hurts
                c.spinRate = 4.0f;
                c.fireT -= dt;
                bool tick = c.fireT <= 0;
                if (tick) c.fireT = 0.5f;
                for (int s = 0; s < N(); s++) {
                    Fighter& f = sides[s].Cur();
                    if (!Grounded(f) || f.dodgeT > 0 || f.rollT > 0) continue;
                    Vector2 to = c.pos - f.pos;
                    float d = Len(to);
                    if (d > c.radius || d < 1) continue;
                    bool water = HasT(f.sp, Type::Water);
                    float k = 1.0f - d / c.radius * 0.5f;
                    Vector2 in = to * (1.0f / d), tang{-in.y, in.x};
                    f.pos += (in * (water ? 22.0f : 62.0f) + tang * (water ? 20.0f : 55.0f)) * k * dt;
                    if (d < 24 && tick && Hittable(s)) {
                        HitSpec h = EnvSpec(Type::Water, 22);
                        h.light = true;
                        h.knock = 0.2f;
                        h.name = "Whirlpool";
                        if (!water) ApplyHit(h, s, tang);
                    }
                }
                if (SimRand(0, 1)) {   // spray off the rim
                    Vector2 p = c.pos + RandDirE() * Frand(20, c.radius);
                    Emit(PK::Drop, p, 2, RotV(SafeNorm(c.pos - p, {1, 0}), 1.2f) * 60.0f, Frand(20, 40), 0.5f, 1.4f, Cc(200, 230, 255), true);
                }
                if (c.t <= 0) { c.phase = 0; c.t = Frand(11, 14); }
            } else if (c.phase == 3) {   // frozen shut
                c.spinRate = 0;
                if (c.t <= 0) { c.phase = 0; c.t = 6; AddText(c.pos, "The drain thawed.", Cc(170, 220, 255), 26); }
            }
        } break;

        // ---------------------------------------------------------------- Surge: the generator
        case Centerpiece::Generator: {
            if (c.phase == 0) {
                c.charge = std::min(100.0f, c.charge + 3.0f * dt);
                c.fireT -= dt;
                if (c.charge >= 60 && c.fireT <= 0) {   // crackling: zaps whoever leans on it
                    c.fireT = 1.1f;
                    int best = -1;
                    float bd = 58;
                    for (int s = 0; s < N(); s++) {
                        if (!Hittable(s) || HasT(sides[s].Cur().sp, Type::Electric)) continue;
                        float d = Dist(sides[s].Cur().pos, c.pos);
                        if (d < bd) { bd = d; best = s; }
                    }
                    if (best >= 0) {
                        Fighter& e = sides[best].Cur();
                        Fx fx;
                        fx.kind = FxKind::Bolt;
                        fx.maxLife = 0.25f;
                        fx.col = kVolt;
                        for (int i = 0; i <= 5; i++) fx.pts.push_back(LerpV(c.pos, e.pos, i / 5.0f) + Vector2{Frand(-5, 5), Frand(-5, 5)});
                        fxs.push_back(fx);
                        HitSpec h = EnvSpec(Type::Electric, 25, Status::Paralyze, 0.1f);
                        h.light = true;
                        h.name = "Generator";
                        ApplyHit(h, best, SafeNorm(e.pos - c.pos, {1, 0}));
                        PlaySfx(Sfx::Zap);
                    }
                }
                if (c.charge >= 100) {
                    c.phase = 1;
                    c.t = 1.5f;
                    AddText({ARENA_W / 2, ARENA_H * 0.1f}, "The generator is overloading!", kVolt, 20);
                    PlaySfx(Sfx::Status);
                }
            } else if (c.phase == 1) {   // overload warning: sparks everywhere, then BOOM
                c.t -= dt;
                Emit(PK::Spark, c.pos + RandDirE() * 8.0f, Frand(10, 30), RandDirE() * Frand(60, 160), Frand(20, 80), 0.3f, 1.3f, kVolt, false).drag = 3;
                Shake(1, 0.1f);
                if (c.t <= 0) {
                    const float R = 170;
                    AddFx(FxKind::Shock, c.pos, 0.6f, kVolt, R, 4);
                    AddFx(FxKind::Radial, c.pos, 0.5f, kVolt, 70, 20).len = 12;
                    AddFx(FxKind::Flash, c.pos, 0.15f, Cc(255, 250, 200), 70, 4);
                    for (const Obstacle& o : field.obs) {   // arcs to every coil
                        if (o.kind != Obstacle::Coil || !o.alive) continue;
                        Fx fx;
                        fx.kind = FxKind::Bolt;
                        fx.maxLife = 0.45f;
                        fx.col = kVolt;
                        for (int i = 0; i <= 7; i++) fx.pts.push_back(LerpV(c.pos, o.pos, i / 7.0f) + Vector2{Frand(-7, 7), Frand(-7, 7)});
                        fxs.push_back(fx);
                    }
                    HitSpec h = EnvSpec(Type::Electric, 55, Status::Paralyze, 0.3f);
                    h.name = "Overload";
                    h.knock = 1.2f;
                    for (int s = 0; s < N(); s++) {
                        if (!Hittable(s)) continue;
                        Fighter& f = sides[s].Cur();
                        if (Dist(f.pos, c.pos) > R + HitR(f) || f.jumpT > 0) continue;
                        if (HasT(f.sp, Type::Electric)) {   // Electric types soak it up
                            f.buffMul[2] = std::max(f.buffMul[2], 1.3f);
                            f.buffT[2] = std::max(f.buffT[2], 6.0f);
                            f.buffMul[0] = std::max(f.buffMul[0], 1.2f);
                            f.buffT[0] = std::max(f.buffT[0], 6.0f);
                            AddText(f.pos, "Supercharged!", kVolt, 40);
                            continue;
                        }
                        ApplyHit(h, s, SafeNorm(f.pos - c.pos, {1, 0}));
                    }
                    AddText(c.pos, "OVERLOAD!", kVolt, 50);
                    Shake(5, 0.4f);
                    PlaySfx(Sfx::Zap);
                    c.charge = 0;
                    c.phase = 0;
                    c.fireT = 2;
                }
            }
        } break;

        // ---------------------------------------------------------------- Erika: the giant bloom
        case Centerpiece::Bloom: {
            c.t -= dt;
            if (c.phase == 0) {   // a bud swelling
                c.charge = std::min(1.0f, c.charge + dt / 8);
                if (c.t <= 0) {
                    c.phase = 1;
                    c.t = 1.5f;
                    AddText({ARENA_W / 2, ARENA_H * 0.1f}, "The giant flower is opening...", kPollen, 20);
                }
            } else if (c.phase == 1) {
                if (c.t <= 0) {
                    c.phase = 2;
                    c.t = 4.5f;
                    c.fireT = 0;
                    AddFx(FxKind::Ring, c.pos, 0.6f, kPollen, 100, 2);
                    PlaySfx(Sfx::Status);
                }
            } else if (c.phase == 2) {   // sleep pollen: drifting cloud
                const float R = 100;
                c.fireT -= dt;
                bool tick = c.fireT <= 0;
                if (tick) c.fireT = 0.5f;
                for (int i = 0; i < 2; i++) {
                    Vector2 p = c.pos + RandDirE() * Frand(10, R);
                    Emit(PK::Glow, p, Frand(4, 26), RandDirE() * 8.0f + Vector2{0, -4}, Frand(-4, 6), Frand(0.8f, 1.4f), Frand(1.2f, 2.0f), SimRand(0, 1) ? kPollen : Cc(255, 240, 160), false).drag = 1;
                }
                if (tick) {
                    for (int s = 0; s < N(); s++) {
                        if (!Hittable(s)) continue;
                        Fighter& f = sides[s].Cur();
                        if (Dist(f.pos, c.pos) > R || f.jumpT > 0) continue;
                        if (HasT(f.sp, Type::Grass)) {   // Grass types bask in it
                            float add = std::min((float)f.maxHp - f.hp, f.maxHp * 0.025f);
                            if (add >= 1) { f.hp += add; AddText(f.pos, TextFormat("+%d", (int)add), Cc(140, 230, 120), 18); }
                        } else if (f.st.major == Status::None && SimRand(0, 99) < 22) {
                            TryStatus(s, Status::Sleep, -1);
                            if (f.st.major == Status::Sleep) AddText(f.pos, "Fell asleep in the pollen!", kPollen, 36);
                        }
                    }
                }
                if (c.t <= 0) { c.phase = 0; c.charge = 0; c.t = Frand(11, 14); }
            } else if (c.phase == 3) {   // burned: charred stalk, regrowing
                if (SimRand(0, 2) == 0) Emit(PK::Smoke, c.pos + RandDirE() * 6.0f, Frand(6, 20), {Frand(-5, 5), 0}, Frand(15, 25), 1.0f, 3.0f, Cc(90, 85, 80), false).grow = 5;
                if (c.t <= 0) { c.phase = 0; c.charge = 0; c.t = 8; AddText(c.pos, "The flower grew back!", Cc(140, 230, 120), 26); }
            }
        } break;

        // ---------------------------------------------------------------- Koga: the sludge vat
        case Centerpiece::Vat: {
            if (c.phase == 1) { c.t -= dt; if (c.t <= 0) c.phase = 0; }
            if (c.phase == 0) c.charge = std::min(100.0f, c.charge + 9.0f * dt);
            if (c.charge >= 100 && SimRand(0, 5) == 0)
                Emit(PK::Bubble, c.pos + RandDirE() * 6.0f, 28, {0, 0}, Frand(10, 20), 0.6f, Frand(1.2f, 2.0f), kSludge, false);
        } break;

        // ---------------------------------------------------------------- Sabrina: the psychic orb
        case Centerpiece::Orb: {
            c.spin += dt * (c.owner >= 0 ? 3.0f : 1.0f);
            if (c.phase == 2) {   // knocked out by a Dark move
                c.t -= dt;
                if (c.t <= 0) { c.phase = 0; AddText(c.pos, "The orb glows again.", kOrbIdle, 26); }
                break;
            }
            if (c.owner < 0) break;
            c.t -= dt;
            if (c.t <= 0) { c.owner = -1; break; }
            c.fireT -= dt;
            if (c.fireT > 0) break;
            c.fireT = 1.6f;
            int from = -1, best = -1;
            float bd = 1e9f;
            for (int s = 0; s < N(); s++) if (sides[s].teamId == c.owner) { from = s; break; }
            for (int s = 0; s < N(); s++) {
                if (sides[s].teamId == c.owner || !Targetable(s)) continue;
                float d = Dist(sides[s].Cur().pos, c.pos);
                if (d < bd) { bd = d; best = s; }
            }
            if (from < 0 || best < 0) break;
            Projectile p;
            p.hit = EnvSpec(Type::Psychic, 40, Status::Confuse, 0.1f);
            p.hit.special = true;
            p.hit.side = from;   // fights for this team: only its foes get hit
            p.hit.name = "Psychic Orb";
            p.hit.look = LookKey("Psybeam", Type::Psychic, MoveKind::Projectile);
            Vector2 d = SafeNorm(sides[best].Cur().pos - c.pos, {1, 0});
            p.pos = c.pos + d * 14.0f;
            p.vel = d * 230.0f;
            p.maxDist = 700;
            p.radius = 6;
            p.homing = 1.5f;
            p.z = 24;
            projs.push_back(p);
            AddFx(FxKind::Hit, c.pos + d * 10.0f, 0.15f, TeamColor(c.owner), 10, 24);
            c.flash = 0.15f;
            PlaySfx(Sfx::Shoot);
        } break;

        // ---------------------------------------------------------------- Blaine: the volcano vent
        case Centerpiece::Vent: {
            c.t -= dt;
            if (c.phase == 0) {
                if (SimRand(0, 3) == 0) Emit(PK::Smoke, c.pos + RandDirE() * 4.0f, 40, {Frand(-6, 6), 0}, Frand(15, 30), 1.4f, 3.5f, Cc(110, 100, 95), false).grow = 7;
                if (c.t <= 0) {   // rumbling: the bombs' landing spots light up (one under each battler, a few strays)
                    c.phase = 1;
                    c.t = 2.2f;
                    c.marks.clear();
                    for (int s = 0; s < N(); s++)
                        if (Targetable(s)) c.marks.push_back(ClampToArenaC(sides[s].Cur().pos + RandDirE() * Frand(0, 18)));
                    for (int i = 0; i < 3; i++) c.marks.push_back(ClampToArenaC(c.pos + RandDirE() * Frand(70, 200)));
                    AddText({ARENA_W / 2, ARENA_H * 0.1f}, "The volcano is rumbling!", Cc(255, 140, 70), 20);
                    PlaySfx(Sfx::Status);
                }
            } else if (c.phase == 1) {
                Shake(1, 0.1f);
                Emit(PK::Ember, c.pos + RandDirE() * 6.0f, 40, RandDirE() * 25.0f, Frand(40, 80), 0.6f, 1.2f, Cc(255, 170, 60), false);
                if (c.t <= 0) {   // ERUPTION
                    c.phase = 2;
                    c.t = 1.4f;
                    for (size_t i = 0; i < c.marks.size(); i++) {
                        Projectile p;
                        p.hit = EnvSpec(Type::Fire, 55, Status::Burn, 0.2f);
                        p.hit.name = "Lava Bomb";
                        p.hit.look = LookKey("Fire Blast", Type::Fire, MoveKind::Lob);
                        p.lob = true;
                        p.start = c.pos;
                        p.target = c.marks[i];
                        p.dur = 0.8f + 0.12f * i;
                        p.pos = c.pos;
                        p.radius = 22;
                        projs.push_back(p);
                    }
                    HitSpec h = EnvSpec(Type::Fire, 60, Status::Burn, 0.3f);
                    h.name = "Eruption";
                    for (int s = 0; s < N(); s++)
                        if (Hittable(s) && Dist(sides[s].Cur().pos, c.pos) < c.radius + 22) ApplyHit(h, s, SafeNorm(sides[s].Cur().pos - c.pos, {1, 0}));
                    AddFx(FxKind::Pillar, c.pos, 0.9f, Cc(255, 120, 40), 22, 0);
                    AddFx(FxKind::Flash, c.pos, 0.12f, Cc(255, 180, 90), 70, 4);
                    for (int i = 0; i < 20; i++)
                        Emit(PK::Flame, c.pos + RandDirE() * 6.0f, Frand(36, 50), RandDirE() * Frand(20, 70), Frand(80, 160), Frand(0.5f, 0.9f), Frand(2, 3.5f), Cc(255, 140, 40), false).drag = 1;
                    AddText(c.pos, "ERUPTION!", Cc(255, 150, 60), 50);
                    Shake(5, 0.5f);
                    PlaySfx(Sfx::Fire);
                }
            } else if (c.phase == 2) {
                Emit(PK::Smoke, c.pos + RandDirE() * 6.0f, 44, RandDirE() * 15.0f + Vector2{0, -5}, Frand(30, 50), 1.6f, 5.0f, Cc(80, 70, 70), false).grow = 10;
                if (c.t <= 0) { c.phase = 0; c.t = Frand(12, 16); c.marks.clear(); }
            }
        } break;

        // ---------------------------------------------------------------- Giovanni: the spinner turntable
        case Centerpiece::Spinner: {
            c.t -= dt;
            c.spin += c.spinRate * dt;
            bool surge = c.phase == 1;
            if (c.phase == 2) {   // jammed
                c.spinRate *= std::exp(-6 * dt);
                if (c.t <= 0) { c.phase = 0; c.t = 8; c.spinRate = (c.owner >= 0 ? 1 : -1) * 0.9f; AddText(c.pos, "The turntable whirs back to life.", UI_TEXT, 26); }
            } else if (c.phase == 0) {
                if (c.t <= 0) {
                    c.phase = 1;
                    c.t = 1.4f;
                    c.owner = c.owner >= 0 ? -1 : 1;   // the new direction
                    AddText({ARENA_W / 2, ARENA_H * 0.1f}, "The turntable reverses!", UI_GOLD, 20);
                    PlaySfx(Sfx::Status);
                }
            } else if (c.phase == 1) {
                c.spinRate += ((float)c.owner * 3.4f - c.spinRate) * std::min(1.0f, dt * 5);
                if (c.t <= 0) { c.phase = 0; c.t = Frand(8, 11); }
            }
            if (c.phase == 0) c.spinRate += ((float)(c.owner >= 0 ? 1 : -1) * 0.9f - c.spinRate) * std::min(1.0f, dt * 3);
            for (int s = 0; s < N(); s++) {   // whoever stands on it goes round with it
                Fighter& f = sides[s].Cur();
                if (!Grounded(f) || f.dashing) continue;
                Vector2 rel = f.pos - c.pos;
                float d = Len(rel);
                if (d > c.radius) continue;
                Vector2 np = c.pos + RotV(rel, c.spinRate * dt);
                if (surge && d > 8) np += rel * (1.0f / d) * (140.0f * dt * (d / c.radius));   // flung outward
                f.pos = np;
                if (surge && d > c.radius * 0.5f && SimRand(0, 2) == 0)
                    Emit(PK::Smoke, f.pos, 1, rel * (-0.3f), 4, 0.4f, 2.2f, Cc(200, 195, 185), false).grow = 4;
            }
            // Earthquake and other ground shockwaves rolling across it jam it
            for (const ShockWave& w : waves) {
                if (w.done || w.hit.type != Type::Ground || c.phase == 2) continue;
                if (std::fabs(Dist(w.origin, c.pos) - w.r) < c.radius * 0.5f) CenterJam();
            }
        } break;

        // ---------------------------------------------------------------- Falkner: the weather vane
        case Centerpiece::Vane: {
            if (c.t > 0) { c.t -= dt; if (c.t <= 0) { c.owner = -1; AddText(c.pos, "The vane swings free again.", UI_DIM, 26); } }
            float want = std::atan2(field.windDir.y, field.windDir.x);
            float d = AngleTo(c.spin, want);
            float step = (field.windOn ? 4.0f : 1.2f) * dt;
            c.spin += std::fabs(d) < step ? d : (d > 0 ? step : -step);
            if (field.windOn == 0) c.spin += std::sin(time * 1.7f) * 0.3f * dt;   // idly turning in the breeze
        } break;

        // ---------------------------------------------------------------- Bugsy: the hive
        case Centerpiece::Hive: {
            if (c.phase == 2) {   // smoked out
                c.t -= dt;
                if (SimRand(0, 2) == 0) Emit(PK::Smoke, c.pos + RandDirE() * 5.0f, Frand(20, 36), {Frand(-5, 5), 0}, Frand(14, 24), 1.0f, 3.0f, Cc(150, 150, 140), false).grow = 6;
                if (c.t <= 0) { c.phase = 0; c.charge = 30; AddText(c.pos, "The bugs are back in the hive!", kSwarm, 26); }
                break;
            }
            c.charge = std::min(100.0f, c.charge + 7.0f * dt);
            if (SimRand(0, c.charge >= 100 ? 2 : 6) == 0)   // bees buzzing round it
                Emit(PK::Spark, c.pos + RandDirE() * Frand(12, 22), Frand(20, 40), RandDirE() * 30.0f, Frand(-8, 8), 0.4f, 1.0f, SimRand(0, 1) ? kSwarm : Cc(60, 50, 30), false);
        } break;

        // ---------------------------------------------------------------- Whitney: the milk tank
        case Centerpiece::MilkTank: {
            if (c.phase == 0) { c.charge = std::min(100.0f, c.charge + 7.0f * dt); break; }
            c.t -= dt;
            c.fireT -= dt;
            if (c.fireT <= 0) {
                c.fireT = 0.5f;
                for (int s = 0; s < N(); s++) {
                    Fighter& f = sides[s].Cur();
                    if (!Grounded(f) || f.hp <= 0 || f.hp >= f.maxHp) continue;
                    bool in = false;
                    for (const Vector2& m : c.marks) if (Dist(f.pos, m) < 22) in = true;
                    if (!in) continue;
                    float add = std::min((float)f.maxHp - f.hp, f.maxHp * 0.012f * (HasT(f.sp, Type::Normal) ? 2.0f : 1.0f));
                    if (add >= 1) { f.hp += add; AddText(f.pos, TextFormat("+%d", (int)add), Cc(255, 200, 220), 18); }
                }
            }
            if (c.t <= 0) { c.phase = 0; c.marks.clear(); }
        } break;

        // ---------------------------------------------------------------- Jasmine: the lighthouse lamp
        case Centerpiece::Lamp: {
            float base = c.spinRate >= 0 ? 0.55f : -0.55f;
            c.spinRate += (base - c.spinRate) * std::min(1.0f, dt * 0.25f);
            c.spin = std::remainder(c.spin + c.spinRate * dt, 2 * PI_F);
        } break;
        default: HoennCenterUpdate(dt); break;
    }
}

Vector2 Battle::ClampToArenaC(Vector2 p) const {
    return {std::max(16.0f, std::min(ARENA_W - 16, p.x)), std::max(16.0f, std::min(ARENA_H - 16, p.y))};
}

void Battle::CenterJam() {
    Centerpiece& c = field.center;
    if (c.kind != Centerpiece::Spinner || c.phase == 2) return;
    c.phase = 2;
    c.t = 4.5f;
    c.flash = 0.3f;
    AddText(c.pos, "The turntable jammed!", Cc(220, 180, 120), 36);
    Boom(c.pos + RandDirE() * 30.0f, 18, Impact::Debris, 2);
    Shake(2);
    PlaySfx(Sfx::Thud);
}

// A move hit the centerpiece (its body, or the floor it sits in).
void Battle::CenterHit(const HitSpec& h, Vector2 dir, bool physical) {
    Centerpiece& c = field.center;
    if (c.kind == Centerpiece::None || h.power <= 0 || winner >= 0) return;
    if (c.hitCd > 0) return;
    c.flash = 0.12f;
    dir = SafeNorm(dir, {1, 0});
    switch (c.kind) {
        case Centerpiece::Boulder: {
            if (c.phase != 0) return;
            bool shove = h.heavy || (physical && h.power >= 50) ||
                         (h.power >= 60 && (h.type == Type::Rock || h.type == Type::Ground || h.type == Type::Fighting || h.type == Type::Steel));
            c.hitCd = 0.4f;
            if (!shove) {
                if (SimRand(0, 2) == 0) AddText(c.pos, "The boulder barely budged...", UI_DIM, 30);
                Emit(PK::Debris, c.pos - dir * c.radius, 10, -dir * 40.0f + RandDirE() * 20.0f, 30, 0.4f, 1.4f, Cc(140, 115, 85), true);
                return;
            }
            c.phase = 1;
            c.vel = dir * (200.0f + std::min(80.0f, h.power * 0.5f));
            c.mask = 0;
            c.bounces = 0;
            AddText(c.pos, "The boulder broke loose!", UI_GOLD, 44);
            Boom(c.pos - dir * c.radius, 18, Impact::Debris, 6);
            Shake(3);
            PlaySfx(Sfx::Thud);
        } break;
        case Centerpiece::Drain: {
            if (h.type == Type::Ice && c.phase != 3) {
                c.phase = 3;
                c.t = 9;
                c.hitCd = 1;
                Puddle pd;   // a sheet of ice over the drain
                pd.seed = (uint32_t)SimRand(1, 0x3fffffff);
                pd.pos = c.pos;
                pd.r = pd.maxR = 36;
                pd.ice = 9;
                pd.sheet = true;
                field.puddles.push_back(pd);
                AddText(c.pos, "The drain froze over!", Cc(170, 230, 255), 40);
                PlaySfx(Sfx::Shield);
            } else if (h.type == Type::Electric && c.phase == 2) {   // the whole whirlpool lights up
                c.hitCd = 1.2f;
                AddFx(FxKind::Radial, c.pos, 0.45f, kVolt, c.radius * 0.8f, 0).len = 10;
                AddText(c.pos, "The whirlpool is electrified!", kVolt, 36);
                PlaySfx(Sfx::Zap);
                Shake(3);
                for (int s = 0; s < N(); s++) {
                    if (!Hittable(s)) continue;
                    Fighter& f = sides[s].Cur();
                    if (!Grounded(f) || Dist(f.pos, c.pos) > c.radius) continue;
                    float eff = TypeEffectiveness(Type::Electric, f.sp->type1, f.sp->type2);
                    if (eff <= 0) continue;
                    DirectDamage(s, f.maxHp * 0.08f * eff, kVolt, h.side >= 0 && Foes(h.side, s) ? h.side : AnyFoe(s));
                    if (!HasT(f.sp, Type::Electric) && f.state == FState::Active) { f.st.paraStunT = std::max(f.st.paraStunT, 0.8f); f.pendingMove = -1; }
                }
            }
        } break;
        case Centerpiece::Generator: {
            if (c.phase != 0) return;
            if (h.type == Type::Ground) {
                c.hitCd = 1;
                if (c.charge > 5) {
                    c.charge = 0;
                    AddText(c.pos, "Grounded! The generator drained.", Cc(200, 170, 120), 34);
                    for (int i = 0; i < 10; i++) Emit(PK::Spark, c.pos, Frand(4, 24), RandDirE() * Frand(30, 90), -20, 0.3f, 1.0f, kVolt, false);
                }
                return;
            }
            float add = h.power * (h.type == Type::Electric ? 0.9f : 0.3f);
            c.charge = std::min(100.0f, c.charge + add);
            c.hitCd = 0.25f;
            for (int i = 0; i < 4; i++) Emit(PK::Spark, c.pos - dir * c.radius, 20, RandDirE() * 70.0f, 30, 0.25f, 1.1f, kVolt, false).drag = 3;
            if (h.type == Type::Electric) AddText(c.pos, TextFormat("Charging... %d%%", (int)c.charge), kVolt, 22);
        } break;
        case Centerpiece::Bloom: {
            if (h.type == Type::Fire && c.phase != 3) {
                c.phase = 3;
                c.t = 10;
                c.hitCd = 1;
                c.charge = 0;
                AddText(c.pos, "The giant flower burned!", Cc(255, 150, 60), 40);
                for (int i = 0; i < 16; i++)
                    Emit(PK::Flame, c.pos + RandDirE() * 10.0f, Frand(6, 30), RandDirE() * 30.0f, Frand(30, 70), 0.7f, Frand(2, 3), Cc(255, 140, 40), false).drag = 2;
                HitSpec fh = EnvSpec(Type::Fire, 35, Status::Burn, 0.2f);   // the flash fire singes whoever is right next to it
                fh.name = "Burning Flower";
                for (int s = 0; s < N(); s++)
                    if (Hittable(s) && Dist(sides[s].Cur().pos, c.pos) < 40) ApplyHit(fh, s, SafeNorm(sides[s].Cur().pos - c.pos, {1, 0}));
                PlaySfx(Sfx::Fire);
            } else if (c.phase == 0 && c.charge > 0.35f) {   // knocked: it puffs open early
                c.phase = 1;
                c.t = 0.8f;
                c.hitCd = 1;
                AddText(c.pos, "The flower shuddered open!", kPollen, 34);
            }
        } break;
        case Centerpiece::Vat: {
            if (c.charge < 100) {
                if (c.hitCd <= 0 && SimRand(0, 1)) AddText(c.pos, "The vat is still filling...", UI_DIM, 26);
                c.hitCd = 0.6f;
                return;
            }
            // BURST: sludge sprays out the far side, away from whoever hit it
            bool poison = h.type == Type::Poison;
            c.charge = 0;
            c.phase = 1;
            c.t = 1.0f;
            c.hitCd = 1;
            int side = h.side;
            HitSpec sh = EnvSpec(Type::Poison, poison ? 60 : 45, Status::Poison, 0.5f);
            sh.side = side;   // it's your sludge: it only hurts your foes
            sh.name = "Sludge Wave";
            const float RANGE = 170, HALF = 0.55f;
            for (int ts = 0; ts < N(); ts++) {
                if (!Hittable(ts) || (side >= 0 && !Foes(side, ts))) continue;
                Vector2 to = sides[ts].Cur().pos - c.pos;
                float d = Len(to);
                if (d > RANGE || d < 1 || Dot(to * (1.0f / d), dir) < std::cos(HALF)) continue;
                ApplyHit(sh, ts, to * (1.0f / d));
            }
            HitSpec ph = sh;   // and leaves toxic puddles along the way
            ph.patchPower = 10;
            ph.patchRadius = 15;
            ph.patchTime = 6;
            for (int i = 0; i < 4; i++) SpawnPatch(ph, c.pos + RotV(dir, Frand(-HALF, HALF) * 0.8f) * (40.0f + i * 32));
            for (int i = 0; i < 26; i++) {
                Vector2 d = RotV(dir, Frand(-HALF, HALF));
                Emit(PK::Drop, c.pos + d * c.radius, Frand(10, 26), d * Frand(80, 220), Frand(30, 90), Frand(0.5f, 0.9f), Frand(1.6f, 2.8f), i % 3 ? kSludge : Cc(190, 120, 230), true);
            }
            AddFx(FxKind::Cone, c.pos, 0.4f, kSludge, RANGE, 4).dir = dir;
            AddText(c.pos, "The vat burst!", Cc(200, 130, 240), 44);
            Shake(3);
            PlaySfx(Sfx::Thud);
        } break;
        case Centerpiece::Orb: {
            if (c.phase == 2) return;
            c.hitCd = 0.3f;
            if (h.type == Type::Dark) {
                c.phase = 2;
                c.t = 6;
                c.owner = -1;
                AddText(c.pos, "The orb went dark!", Cc(160, 120, 200), 36);
                Burst(c.pos, Cc(60, 30, 80), 14, 60, 24);
                return;
            }
            if (h.side < 0) return;
            int team = sides[h.side].teamId;
            if (c.owner != team) {
                c.owner = team;
                c.fireT = 0.7f;
                AddText(c.pos, team == sides[0].teamId ? "The orb sides with you!" : "The orb turned against you!", TeamColor(team), 36);
                AddFx(FxKind::Shock, c.pos, 0.4f, TeamColor(team), 30, 24);
                PlaySfx(Sfx::Status);
            }
            c.t = 9;
        } break;
        case Centerpiece::Vent: {
            if (h.type == Type::Water) {
                c.hitCd = 1.5f;
                for (int i = 0; i < 12; i++)
                    Emit(PK::Smoke, c.pos + RandDirE() * 10.0f, Frand(10, 26), RandDirE() * 20.0f, Frand(25, 45), Frand(0.8f, 1.3f), Frand(3, 5), Cc(235, 240, 245, 220), false).grow = 9;
                if (c.phase == 1) {
                    c.phase = 0;
                    c.t = 8;
                    c.marks.clear();
                    AddText(c.pos, "Water calmed the volcano!", Cc(150, 200, 255), 40);
                } else if (c.phase == 0) {
                    c.t += 5;
                    AddText(c.pos, "The vent cooled down.", Cc(150, 200, 255), 30);
                }
                PlaySfx(Sfx::Shield);
            } else if (h.type == Type::Fire && c.phase == 0 && c.t > 1.0f) {
                c.hitCd = 2;
                c.t = 1.0f;
                AddText(c.pos, "The volcano is stirring!", Cc(255, 150, 60), 34);
            }
        } break;
        case Centerpiece::Spinner: {
            if (h.type == Type::Ground) CenterJam();
        } break;
        case Centerpiece::Vane: {
            if (h.side < 0) return;
            int team = sides[h.side].teamId;
            c.hitCd = 0.5f;
            if (c.owner != team || c.t < 8) {
                AddText(c.pos, team == sides[0].teamId ? "The vane turned - the wind is on your side!" : "The vane turned against you!", TeamColor(team), 36);
                AddFx(FxKind::Ring, c.pos, 0.4f, TeamColor(team), 40, 30);
                PlaySfx(Sfx::Status);
            }
            c.owner = team;
            c.t = 15;
            if (field.windOn == 0 && field.windT > 2.5f) field.windT = 2.5f;   // and it calls the next gust sooner
        } break;
        case Centerpiece::Hive: {
            if (c.phase == 2) return;
            if (h.type == Type::Fire) {
                c.phase = 2;
                c.t = 12;
                c.charge = 0;
                c.hitCd = 1;
                AddText(c.pos, "The hive was smoked out!", Cc(255, 160, 80), 40);
                for (int i = 0; i < 12; i++)
                    Emit(PK::Smoke, c.pos + RandDirE() * 8.0f, Frand(16, 36), RandDirE() * 20.0f, Frand(20, 40), Frand(0.8f, 1.3f), Frand(3, 5), Cc(140, 140, 130, 220), false).grow = 8;
                PlaySfx(Sfx::Fire);
                return;
            }
            c.hitCd = 0.6f;
            if (c.charge < 100 || h.side < 0) {
                if (SimRand(0, 1)) AddText(c.pos, "The hive is still buzzing...", UI_DIM, 26);
                return;
            }
            std::vector<int> foes;
            for (int ts = 0; ts < N(); ts++) if (Foes(h.side, ts) && Targetable(ts)) foes.push_back(ts);
            if (foes.empty()) return;
            c.charge = 0;
            for (int i = 0; i < 6; i++) {
                int ts = foes[i % foes.size()];
                Vector2 d = SafeNorm(sides[ts].Cur().pos - c.pos, {1, 0});
                Projectile p;
                p.hit = EnvSpec(Type::Bug, 22);
                p.hit.side = h.side;   // the swarm only goes for the hitter's foes
                p.hit.name = "Bug Swarm";
                p.hit.look = LookKey("Pin Missile", Type::Bug, MoveKind::Projectile);
                Vector2 v = RotV(d, (i - 2.5f) * 0.35f);
                p.pos = c.pos + v * 16.0f;
                p.vel = v * 150.0f;
                p.maxDist = 650;
                p.radius = 4;
                p.homing = 3.0f;
                p.z = 22 + (i % 3) * 4;
                projs.push_back(p);
            }
            AddText(c.pos, "The swarm is out!", kSwarm, 40);
            Burst(c.pos, kSwarm, 16, 70, 26);
            Shake(1);
            PlaySfx(Sfx::Shoot);
        } break;
        case Centerpiece::MilkTank: {
            c.hitCd = 0.8f;
            if (c.phase == 1) return;
            if (c.charge < 100) {
                if (SimRand(0, 1)) AddText(c.pos, "The milk tank is still refilling...", UI_DIM, 26);
                return;
            }
            Vector2 at = h.side >= 0 ? sides[h.side].Cur().pos : c.pos - dir * 60.0f;
            Vector2 toward = SafeNorm(at - c.pos, -dir);
            if (Dist(at, c.pos) < 50) at = c.pos + toward * 50.0f;
            Vector2 lat{-toward.y, toward.x};
            c.marks.clear();
            c.marks.push_back(ClampToArenaC(at));
            c.marks.push_back(ClampToArenaC(at + lat * 30.0f - toward * 8.0f));
            c.marks.push_back(ClampToArenaC(at - lat * 30.0f - toward * 8.0f));
            c.charge = 0;
            c.phase = 1;
            c.t = 8;
            c.fireT = 0;
            for (const Vector2& m : c.marks)
                for (int i = 0; i < 6; i++) Emit(PK::Drop, c.pos + RandDirE() * 6.0f, 30, SafeNorm(m - c.pos, {1, 0}) * Frand(60, 120) + RandDirE() * 20.0f, Frand(30, 60), 0.6f, 2.0f, kMilk, true);
            AddText(c.pos, "Moomoo Milk spilled out!", Cc(255, 200, 220), 40);
            PlaySfx(Sfx::Heal);
        } break;
        case Centerpiece::Lamp: {
            c.hitCd = 1.0f;
            float sign = c.spinRate >= 0 ? -1.0f : 1.0f;
            c.spinRate = sign * std::min(2.0f, std::fabs(c.spinRate) * 1.6f + 0.3f);
            AddText(c.pos, "The beam swung the other way!", kBeam, 34);
            AddFx(FxKind::Flash, c.pos, 0.12f, kBeam, 40, 50);
            PlaySfx(Sfx::Shield);
        } break;
        default: HoennCenterHit(h, dir, physical); break;
    }
}

// A move landed somewhere on the field: floor centerpieces (and ones whose area it covers) react.
void Battle::CenterReact(Vector2 at, const HitSpec& h, float radius) {
    const Centerpiece& c = field.center;
    if (c.kind == Centerpiece::None || h.power <= 0) return;
    float reach;
    switch (c.kind) {
        case Centerpiece::Drain: reach = c.phase == 2 ? c.radius : 34; break;
        case Centerpiece::Spinner: reach = c.radius; break;
        case Centerpiece::Vent: reach = c.radius + 16; break;   // water splashing on it
        case Centerpiece::Bloom: reach = c.radius + 12; break;  // fire landing on it
        case Centerpiece::Hive: reach = c.radius + 12; break;   // fire landing on it
        default: return;
    }
    if (Dist(at, c.pos) > reach + radius) return;
    if (c.kind == Centerpiece::Vent && h.type != Type::Water) return;
    if ((c.kind == Centerpiece::Bloom || c.kind == Centerpiece::Hive) && h.type != Type::Fire) return;
    CenterHit(h, SafeNorm(c.pos - at, {1, 0}), false);
}

// CPU: what to keep away from.
Vector2 Battle::CenterAvoid(int s) const {
    const Centerpiece& c = field.center;
    const Fighter& f = sides[s].Cur();
    Vector2 away = SafeNorm(f.pos - c.pos, {1, 0});
    float d = Dist(f.pos, c.pos);
    switch (c.kind) {
        case Centerpiece::Drain: if (c.phase >= 1 && c.phase <= 2 && d < c.radius + 10 && !HasT(f.sp, Type::Water)) return away * 1.2f; break;
        case Centerpiece::Bloom: if (c.phase >= 1 && c.phase <= 2 && d < 110 && !HasT(f.sp, Type::Grass)) return away * 1.1f; break;
        case Centerpiece::Generator: if (c.phase == 1 && d < 180 && !HasT(f.sp, Type::Electric)) return away * 1.3f; break;
        case Centerpiece::Vent:
            if (c.phase == 1) {
                for (const Vector2& m : c.marks)
                    if (Dist(f.pos, m) < 34) return SafeNorm(f.pos - m, {0, 1}) * 1.3f;
                if (d < 50) return away;
            }
            break;
        case Centerpiece::Boulder:
            if (c.phase == 1) {   // step out of its path
                Vector2 v = SafeNorm(c.vel, {1, 0});
                Vector2 rel = f.pos - c.pos;
                if (Dot(rel, v) > 0 && std::fabs(Cross(v, rel)) < c.radius + 20) return Vector2{-v.y, v.x} * (Cross(v, rel) >= 0 ? 1.2f : -1.2f);
            }
            break;
        default: break;
    }
    return {0, 0};
}

// ---------------------------------------------------------------- Drawing (visual only)

// On the floor: the drain, the turntable, the vent's lava ring, the danger markers.
void Battle::DrawCenterFloor() const {
    const Centerpiece& c = field.center;
    Vector2 g = ToScreen(c.pos);
    switch (c.kind) {
        case Centerpiece::Drain: {
            float act = c.phase == 2 ? 1.0f : (c.phase == 1 ? 1.0f - c.t / 2.0f : 0.0f);
            float R = 30 + act * (c.radius - 30);
            // dark water deepening toward the eye
            for (int k = 6; k >= 1; k--) {
                float r = R * k / 6.0f;
                DrawEllipse((int)g.x, (int)g.y, r, r * DEPTH, Fade(Cc(20, 50 + k * 8, 110 + k * 10), 0.25f + 0.1f * act));
            }
            // spiral arms of foam
            int arms = 5;
            for (int a = 0; a < arms; a++) {
                Vector2 prev = g;
                for (int i = 1; i <= 18; i++) {
                    float u = i / 18.0f;
                    float ang = c.spin + a * 2 * PI_F / arms + u * 3.2f;
                    float r = u * R;
                    Vector2 p{g.x + std::cos(ang) * r, g.y + std::sin(ang) * r * DEPTH};
                    DrawLineEx(prev, p, 1.0f + u * 1.4f, Fade(Cc(220, 240, 255), (0.25f + 0.5f * act) * (1 - u * 0.5f)));
                    prev = p;
                }
            }
            // the grate
            DrawEllipse((int)g.x, (int)g.y, 16, 16 * DEPTH, Cc(40, 50, 60));
            for (int i = -2; i <= 2; i++) DrawLineEx({g.x + i * 5.0f, g.y - 6}, {g.x + i * 5.0f, g.y + 6}, 1.2f, Cc(120, 135, 150));
            VRingE(g, 16, 16 * DEPTH, 2.0f, Cc(150, 165, 180), 24);
            if (act > 0) DrawEllipse((int)g.x, (int)g.y, 11 * act, 11 * act * DEPTH, Fade(Cc(5, 15, 40), 0.8f * act));   // the eye
            if (c.phase == 1) {   // warning ring
                float pulse = 0.5f + 0.5f * std::sin(time * 12);
                VRingE(g, c.radius, c.radius * DEPTH, 1.5f, Fade(Cc(140, 200, 255), 0.3f + 0.4f * pulse), 48);
            }
        } break;
        case Centerpiece::Spinner: {
            float R = c.radius;
            DrawEllipse((int)g.x, (int)g.y + 2, R + 3, (R + 3) * DEPTH, Cc(40, 38, 36));
            DrawEllipse((int)g.x, (int)g.y, R, R * DEPTH, Cc(120, 112, 100));
            DrawEllipse((int)g.x, (int)g.y, R * 0.92f, R * 0.92f * DEPTH, Cc(150, 140, 125));
            // segments and arrows turning with it
            for (int i = 0; i < 8; i++) {
                float a = c.spin + i * PI_F / 4;
                Vector2 e{std::cos(a), std::sin(a) * DEPTH};
                DrawLineEx(g + e * (R * 0.25f), g + e * (R * 0.92f), 1.2f, Cc(100, 92, 82));
                // a chevron pointing the way it spins
                float a2 = a + PI_F / 8;
                float dirs = c.spinRate >= 0 ? 1.0f : -1.0f;
                Vector2 m{g.x + std::cos(a2) * R * 0.62f, g.y + std::sin(a2) * R * 0.62f * DEPTH};
                Vector2 t{-std::sin(a2) * dirs, std::cos(a2) * DEPTH * dirs};
                Vector2 n{std::cos(a2) * 4.0f, std::sin(a2) * 4.0f * DEPTH};
                Color ac = c.phase == 1 ? Cc(255, 200, 70) : (c.phase == 2 ? Cc(110, 100, 90) : Cc(230, 70, 60));
                DrawLineEx(m - t * 4.0f + n, m + t * 3.0f, 1.8f, ac);
                DrawLineEx(m - t * 4.0f - n, m + t * 3.0f, 1.8f, ac);
            }
            // a hub with the Rocket-red light, and rim lights chasing round
            DrawEllipse((int)g.x, (int)g.y, R * 0.22f, R * 0.22f * DEPTH, Cc(70, 66, 60));
            for (int i = 0; i < 16; i++) {
                float a = c.spin * 1.0f + i * PI_F / 8;
                bool on = ((int)(time * 8) + i) % 4 == 0 || c.phase == 1;
                Vector2 p{g.x + std::cos(a) * R, g.y + std::sin(a) * R * DEPTH};
                DrawCircleV(p, 1.6f, on ? Cc(255, 220, 120) : Cc(90, 80, 60));
            }
            if (c.phase == 1) { VAdd(); VRingE(g, R + 4, (R + 4) * DEPTH, 2, Fade(Cc(255, 200, 80), 0.5f), 48); VNorm(); }
            if (c.phase == 2) for (int i = 0; i < 3; i++) DrawCircleV({g.x + (i - 1) * 9.0f, g.y - 2}, 1.4f, Cc(255, 120, 60));   // warning lamps
        } break;
        case Centerpiece::Vent: {   // a ring of cracked, glowing rock around the crater
            float heat = c.phase == 1 ? 1.0f : (c.phase == 2 ? 0.8f : 0.35f);
            DrawEllipse((int)g.x, (int)g.y, c.radius + 18, (c.radius + 18) * DEPTH, Fade(Cc(60, 40, 35), 0.8f));
            for (int i = 0; i < 9; i++) {
                float a = i * 0.7f + 0.3f;
                Vector2 e{std::cos(a), std::sin(a) * DEPTH};
                DrawLineEx(g + e * (c.radius - 2), g + e * (c.radius + 16 + (i % 3) * 4), 1.2f, Fade(Cc(255, 120, 40), heat * (0.6f + 0.4f * std::sin(time * 5 + i))));
            }
            if (c.phase == 1)   // where the lava bombs will land
                for (const Vector2& m : c.marks) {
                    Vector2 mp = ToScreen(m);
                    float pulse = 0.5f + 0.5f * std::sin(time * 14);
                    float r = 20 * (1 - c.t / 2.2f) + 6;
                    DrawEllipse((int)mp.x, (int)mp.y, r, r * DEPTH, Fade(Cc(255, 80, 30), 0.18f + 0.15f * pulse));
                    VRingE(mp, 22, 22 * DEPTH, 1.5f, Fade(Cc(255, 140, 60), 0.6f + 0.4f * pulse), 28);
                }
        } break;
        case Centerpiece::Bloom:
            if (c.phase == 1 || c.phase == 2) {   // the pollen cloud's reach on the floor
                float a = c.phase == 2 ? 0.18f : 0.08f;
                DrawEllipse((int)g.x, (int)g.y, 100, 100 * DEPTH, Fade(kPollen, a));
                VRingE(g, 100, 100 * DEPTH, 1.2f, Fade(kPollen, 0.35f), 48);
            }
            break;
        case Centerpiece::Generator:
            if (c.phase == 1) {   // the blast radius
                float pulse = 0.5f + 0.5f * std::sin(time * 18);
                VRingE(g, 170, 170 * DEPTH, 1.5f, Fade(kVolt, 0.3f + 0.4f * pulse), 64);
            }
            break;
        case Centerpiece::Boulder:
            if (c.phase == 2 || c.phase == 3) {   // a crater where it stood
                Vector2 hg = ToScreen(c.home);
                DrawEllipse((int)hg.x, (int)hg.y, 18, 18 * DEPTH, Fade(Cc(70, 55, 40), 0.7f));
            }
            break;
        case Centerpiece::MilkTank:
            if (c.phase == 1)   // puddles of Moomoo Milk
                for (size_t i = 0; i < c.marks.size(); i++) {
                    Vector2 mp = ToScreen(c.marks[i]);
                    float fade = std::min(1.0f, c.t / 1.5f);
                    float grow = std::min(1.0f, (8 - c.t) / 0.5f);
                    float r = 20 * (0.4f + 0.6f * grow);
                    DrawEllipse((int)mp.x, (int)mp.y, r + 1, (r + 1) * DEPTH, Fade(Cc(220, 200, 170), 0.7f * fade));
                    DrawEllipse((int)mp.x, (int)mp.y, r, r * DEPTH, Fade(kMilk, 0.9f * fade));
                    DrawEllipse((int)mp.x - (int)(r * 0.3f), (int)mp.y - 1, r * 0.35f, r * 0.12f, Fade(WHITE, 0.8f * fade));
                    float ph = std::fmod(time * 1.5f + i * 0.4f, 1.0f);
                    VRingE(mp, r * (0.3f + 0.7f * ph), r * (0.3f + 0.7f * ph) * DEPTH, 1, Fade(Cc(255, 190, 210), (1 - ph) * 0.6f * fade), 24);
                }
            break;
        case Centerpiece::Lamp: {   // the sweeping beam, clipped to the arena walls
            auto edge = [&](float a) {
                Vector2 d{std::cos(a), std::sin(a)};
                float t = 2000;
                if (d.x > 1e-4f) t = std::min(t, (ARENA_W - c.pos.x) / d.x);
                if (d.x < -1e-4f) t = std::min(t, -c.pos.x / d.x);
                if (d.y > 1e-4f) t = std::min(t, (ARENA_H - c.pos.y) / d.y);
                if (d.y < -1e-4f) t = std::min(t, -c.pos.y / d.y);
                return ToScreen(c.pos + d * t);
            };
            VAdd();
            const int K = 10;
            for (int k = 0; k < K; k++) {
                float a0 = c.spin - Field::BEAM_HALF + 2 * Field::BEAM_HALF * k / K, a1 = c.spin - Field::BEAM_HALF + 2 * Field::BEAM_HALF * (k + 1) / K;
                float mid = 1.0f - std::fabs((k + 0.5f) / K - 0.5f) * 1.4f;
                Vector2 p0 = edge(a0), p1 = edge(a1);
                Color bc = Fade(kBeam, 0.10f + 0.10f * mid);
                DrawTriangle(g, p1, p0, bc);
                DrawTriangle(g, p0, p1, bc);
            }
            VNorm();
        } break;
        default: break;
    }
}

// The centerpiece's body (drawn in depth order with the battlers).
void Battle::DrawCenterBody(const Obstacle& o) const {
    const Centerpiece& c = field.center;
    Vector2 g = ToScreen(o.pos);
    float R = o.radius;
    float fl = c.flash > 0 ? 1.0f : 0.0f;
    switch (c.kind) {
        case Centerpiece::Boulder: {
            if (!o.alive && c.phase != 3) return;
            float grow = c.phase == 3 ? 1.0f - c.t / 1.2f : 1.0f;
            Vector2 hg = c.phase == 3 ? ToScreen(c.home) : g;
            float r = R * grow;
            VShadow({hg.x, hg.y + 1}, r * 1.1f, r * 0.45f, 0.6f);
            Vector2 cc{hg.x, hg.y - r * 0.85f};
            auto rock = [&](float rr, Color col) {   // a lumpy outline that turns as it rolls
                const int n = 18;
                for (int k = 0; k < n; k++) {
                    float a0 = k * 2 * PI_F / n, a1 = (k + 1) * 2 * PI_F / n;
                    auto rad = [&](float a) { float q = a - c.spin; return rr * (0.92f + 0.07f * std::sin(3 * q + 1) + 0.04f * std::sin(5 * q + 2.5f)); };
                    Vector2 p0{cc.x + std::cos(a0) * rad(a0), cc.y + std::sin(a0) * rad(a0)};
                    Vector2 p1{cc.x + std::cos(a1) * rad(a1), cc.y + std::sin(a1) * rad(a1)};
                    DrawTriangle(cc, p1, p0, col);
                }
            };
            rock(r + 1.5f, Cc(60, 48, 36));
            rock(r, Cc(150, 128, 98));
            for (int i = 0; i < 4; i++) {   // strata turning as it rolls
                float a = c.spin + i * PI_F / 2;
                if (std::sin(a) < -0.2f) continue;
                float x = std::cos(a) * r * 0.85f;
                float half = std::sqrt(std::max(0.0f, r * r - x * x)) * 0.85f;
                DrawLineEx({cc.x + x, cc.y - half}, {cc.x + x * 0.9f, cc.y + half}, 2.0f, Cc(118, 98, 74));
            }
            DrawCircleV({cc.x - r * 0.3f, cc.y - r * 0.35f}, r * 0.35f, Fade(Cc(190, 170, 135), 0.6f));
            DrawCircleV({cc.x + r * 0.25f, cc.y + r * 0.1f}, r * 0.22f, Fade(Cc(95, 130, 70), 0.8f));   // moss
            // a carved Onix-like glyph on its face (it's Brock's after all)
            if (c.phase != 1) for (int i = 0; i < 3; i++) DrawCircleLinesV({cc.x - 4 + i * 4.0f, cc.y + 2 - std::fabs(i - 1.0f) * 2}, 2.2f, Fade(Cc(80, 64, 48), 0.9f));
            if (fl > 0) DrawCircleV(cc, r, Fade(WHITE, 0.5f));
        } break;
        case Centerpiece::Generator: {
            float ch = c.charge / 100.0f;
            bool over = c.phase == 1;
            VShadow({g.x, g.y + 1}, R * 1.1f, R * 0.45f, 0.6f);
            // a squat metal drum with copper coils
            DrawRectangle((int)(g.x - R), (int)(g.y - 40), (int)(R * 2), 40, Cc(70, 76, 88));
            DrawEllipse((int)g.x, (int)g.y, R, R * DEPTH, Cc(70, 76, 88));
            DrawRectangle((int)(g.x - R + 2), (int)(g.y - 40), 4, 40, Cc(110, 118, 132));
            for (int i = 0; i < 5; i++) {
                float y = g.y - 7 - i * 7.0f;
                DrawEllipse((int)g.x, (int)y, R + 1, (R + 1) * DEPTH * 0.6f, Cc(170, 100, 50));
                DrawEllipse((int)g.x, (int)y - 1, R - 1, (R - 1) * DEPTH * 0.5f, Cc(210, 140, 70));
            }
            DrawEllipse((int)g.x, (int)(g.y - 40), R, R * DEPTH, Cc(95, 102, 116));
            // the charge meter: a column of lamps
            for (int i = 0; i < 5; i++) {
                bool on = ch > i / 5.0f;
                Color lc = over ? (((int)(time * 16) % 2) ? Cc(255, 80, 60) : WHITE) : (on ? (i >= 4 ? Cc(255, 120, 80) : kVolt) : Cc(60, 60, 50));
                DrawRectangle((int)g.x + 8, (int)(g.y - 8 - i * 6), 5, 4, lc);
            }
            // the top terminal and its crackle
            DrawLineEx({g.x, g.y - 40}, {g.x, g.y - 48}, 3, Cc(150, 155, 165));
            Vector2 top{g.x, g.y - 50};
            DrawCircleV(top, 5, Cc(200, 205, 215));
            VAdd();
            VGlow(top, 8 + ch * 10 + (over ? 8 : 0), kVolt, 0.3f + ch * 0.5f);
            if (ch > 0.5f || over) {
                uint32_t fr = (uint32_t)(time * 20);
                for (int k = 0; k < (over ? 4 : 2); k++) {
                    float a = VRand(fr, k) * 2 * PI_F;
                    VBolt(top, top + Vector2{std::cos(a) * (14 + ch * 10), std::sin(a) * 10 - 4}, VHash(fr, k), 1.0f, kVolt, 0.8f, 0);
                }
            }
            VNorm();
            if (fl > 0) DrawRectangle((int)(g.x - R), (int)(g.y - 40), (int)(R * 2), 40, Fade(WHITE, 0.5f));
        } break;
        case Centerpiece::Bloom: {
            VShadow({g.x, g.y + 1}, R * 1.3f, R * 0.5f, 0.55f);
            if (c.phase == 3) {   // a charred stump, a green shoot coming back
                DrawRectangle((int)g.x - 4, (int)g.y - 12, 8, 12, Cc(50, 40, 35));
                float rg = 1 - c.t / 10;
                DrawLineEx({g.x, g.y - 12}, {g.x + 2, g.y - 12 - rg * 10}, 2, Cc(90, 170, 70));
                break;
            }
            // leaves at the base
            for (int i = 0; i < 5; i++) {
                float a = i * 2 * PI_F / 5 + 0.3f;
                Vector2 tip{g.x + std::cos(a) * (R + 8), g.y + std::sin(a) * (R + 8) * DEPTH};
                DrawTriangle(g, tip, {g.x + std::cos(a + 0.4f) * R, g.y + std::sin(a + 0.4f) * R * DEPTH}, Cc(60, 130, 60));
                DrawTriangle(g, {g.x + std::cos(a + 0.4f) * R, g.y + std::sin(a + 0.4f) * R * DEPTH}, tip, Cc(60, 130, 60));
            }
            float open = c.phase == 2 ? 1.0f : (c.phase == 1 ? 1.0f - c.t / 1.5f : 0.0f);
            float bud = (0.5f + 0.5f * c.charge) * 1.5f;
            Vector2 head{g.x, g.y - 20 - 6 * bud};
            DrawLineEx({g.x, g.y}, head, 6, Cc(60, 125, 55));
            DrawLineEx({g.x - 1, g.y}, {head.x - 1, head.y}, 2, Cc(100, 170, 80));
            // five big petals (a Vileplume-ish rafflesia), closed into a bud until it opens
            for (int i = 0; i < 5; i++) {
                float a = i * 2 * PI_F / 5 - PI_F / 2;
                float pr = (6 + 12 * open) * bud;
                Vector2 pc{head.x + std::cos(a) * pr, head.y + std::sin(a) * pr * (0.5f + 0.2f * open)};
                DrawCircleV(pc, (5 + 6 * open) * bud, Cc(200, 60, 80));
                DrawCircleV(pc, (3 + 4 * open) * bud, Cc(235, 110, 120));
                DrawCircleV(pc + Vector2{1, -1}, 1.2f * bud, Cc(255, 220, 220));
            }
            DrawCircleV(head, (3 + 4 * open) * bud, Cc(250, 210, 90));
            if (c.phase == 1) { VAdd(); VGlow(head, 22, kPollen, 0.5f * (0.5f + 0.5f * std::sin(time * 12))); VNorm(); }
            if (fl > 0) DrawCircleV(head, 14, Fade(WHITE, 0.4f));
        } break;
        case Centerpiece::Vat: {
            float fill = c.charge / 100.0f;
            VShadow({g.x, g.y + 1}, R * 1.15f, R * 0.5f, 0.6f);
            DrawRectangle((int)(g.x - R), (int)(g.y - 32), (int)(R * 2), 32, Cc(70, 60, 80));
            DrawEllipse((int)g.x, (int)g.y, R, R * DEPTH, Cc(70, 60, 80));
            for (int i = 0; i < 2; i++) DrawRectangle((int)(g.x - R), (int)(g.y - 6 - i * 12), (int)(R * 2), 2, Cc(110, 100, 120));   // hoops
            DrawRectangle((int)(g.x - R + 3), (int)(g.y - 32), 3, 32, Cc(100, 90, 110));
            // skull-and-crossbones stencil
            DrawCircleV({g.x, g.y - 18}, 4, Fade(Cc(220, 210, 230), 0.8f));
            DrawLineEx({g.x - 6, g.y - 10}, {g.x + 6, g.y - 14}, 1.2f, Fade(Cc(220, 210, 230), 0.8f));
            DrawLineEx({g.x - 6, g.y - 14}, {g.x + 6, g.y - 10}, 1.2f, Fade(Cc(220, 210, 230), 0.8f));
            // the open top, sludge level and bubbles
            DrawEllipse((int)g.x, (int)(g.y - 32), R, R * DEPTH, Cc(50, 40, 60));
            if (fill > 0.05f) {
                DrawEllipse((int)g.x, (int)(g.y - 32 + (1 - fill) * 4), (R - 2) * fill, (R - 2) * DEPTH * fill, kSludge);
                VAdd();
                VGlow({g.x, g.y - 34}, 10 + fill * 8, Cc(170, 90, 220), 0.25f + 0.35f * fill);
                VNorm();
                for (int i = 0; i < 3; i++) {
                    float ph = std::fmod(time * 1.3f + i * 0.33f, 1.0f);
                    DrawCircleLinesV({g.x - 5 + i * 5.0f, g.y - 33 - ph * 3}, 1 + ph * 2, Fade(Cc(220, 170, 255), (1 - ph) * fill));
                }
                if (fill >= 1) for (int i = 0; i < 2; i++) DrawLineEx({g.x - R + 3 + i * 18.0f, g.y - 32}, {g.x - R + 3 + i * 18.0f, g.y - 24 + std::sin(time * 3 + i) * 2}, 2, kSludge);   // drips over the rim
            }
            if (fl > 0) DrawRectangle((int)(g.x - R), (int)(g.y - 32), (int)(R * 2), 32, Fade(WHITE, 0.4f));
        } break;
        case Centerpiece::Orb: {
            Color col = c.phase == 2 ? Cc(70, 50, 90) : (c.owner >= 0 ? TeamColor(c.owner) : kOrbIdle);
            float bob = std::sin(time * 2) * 2;
            VShadow({g.x, g.y + 1}, 9, 3.5f, 0.6f);
            // a stone pedestal
            DrawRectangle((int)g.x - 8, (int)g.y - 4, 16, 4, Cc(90, 80, 110));
            DrawRectangle((int)g.x - 5, (int)g.y - 20, 10, 18, Cc(110, 100, 130));
            DrawRectangle((int)g.x - 5, (int)g.y - 20, 3, 18, Cc(140, 130, 160));
            DrawEllipse((int)g.x, (int)g.y - 20, 9, 3.5f, Cc(150, 140, 170));
            Vector2 oc{g.x, g.y - 36 + bob};
            // runes orbiting
            for (int i = 0; i < 6; i++) {
                float a = c.spin + i * PI_F / 3;
                Vector2 p{oc.x + std::cos(a) * 19, oc.y + std::sin(a) * 6};
                DrawRectangleV(p - Vector2{1, 1}, {2.5f, 2.5f}, Fade(col, std::sin(a) > 0 ? 0.9f : 0.4f));
            }
            DrawCircleV(oc, 11, ColorBrightness(col, -0.5f));
            DrawCircleV(oc, 10, col);
            DrawCircleV(oc + Vector2{-3, -3.5f}, 3.2f, Fade(WHITE, 0.7f));
            if (c.phase != 2) {   // an eye that opens when it's fighting for someone
                float open = c.owner >= 0 ? 1.0f : 0.3f;
                DrawEllipse((int)oc.x, (int)oc.y + 1, 5.5f, 3.0f * open, Cc(30, 10, 40));
                DrawCircleV({oc.x, oc.y + 1}, 1.2f * open, WHITE);
                VAdd();
                VGlow(oc, 26 + (c.owner >= 0 ? 6 : 0), col, 0.35f + (c.owner >= 0 ? 0.25f : 0));
                if (c.flash > 0) VGlow(oc, 12, WHITE, 0.8f);
                VNorm();
            }
        } break;
        case Centerpiece::Vent: {
            float heat = c.phase == 1 ? 1.0f : (c.phase == 2 ? 0.8f : 0.3f);
            float rumble = c.phase == 1 ? std::sin(time * 60) * 1.0f : 0;
            Vector2 b{g.x + rumble, g.y};
            VShadow({b.x, b.y + 1}, R * 1.2f, R * 0.5f, 0.6f);
            // a squat volcanic cone
            const float H = 38;
            DrawTriangle({b.x - R * 1.25f, b.y}, {b.x + R * 1.25f, b.y}, {b.x + R * 0.35f, b.y - H}, Cc(96, 70, 60));
            DrawTriangle({b.x - R * 1.25f, b.y}, {b.x + R * 0.35f, b.y - H}, {b.x - R * 0.35f, b.y - H}, Cc(96, 70, 60));
            DrawTriangle({b.x - R * 1.25f, b.y}, {b.x - R * 0.35f, b.y - H}, {b.x - R * 0.3f, b.y}, Cc(124, 92, 78));
            DrawEllipse((int)b.x, (int)(b.y - H), R * 0.38f, R * 0.38f * DEPTH, Cc(40, 25, 22));
            DrawEllipse((int)b.x, (int)(b.y - H), R * 0.3f, R * 0.3f * DEPTH, Fade(Cc(255, 110, 30), 0.5f + 0.5f * heat));
            for (int i = 0; i < 3; i++) {   // lava trickling down the side, wavy and fading
                Vector2 prev{b.x - 4 + i * 4.0f, b.y - H + 2};
                float len = H * (0.35f + 0.15f * i);
                for (int k = 1; k <= 5; k++) {
                    float u = k / 5.0f;
                    Vector2 p{b.x - 4 + i * 4.0f + (i - 1) * u * 8 + std::sin(u * 7 + i * 2) * 1.5f, b.y - H + 2 + u * len};
                    DrawLineEx(prev, p, 2.2f - u, Fade(Cc(255, 130, 40), (0.4f + 0.5f * heat) * (1 - u * 0.6f)));
                    prev = p;
                }
            }
            VAdd();
            VGlow({b.x, b.y - H - 2}, 16 + 12 * heat, Cc(255, 120, 40), 0.3f + 0.5f * heat);
            VNorm();
            if (fl > 0) DrawTriangle({b.x - R, b.y}, {b.x + R, b.y}, {b.x, b.y - H}, Fade(WHITE, 0.4f));
        } break;
        case Centerpiece::Vane: {   // a tall pole with a bird-shaped weather vane turning on top
            VShadow({g.x, g.y + 1}, 9, 3.5f, 0.6f);
            DrawRectangle((int)g.x - 7, (int)g.y - 5, 14, 5, Cc(120, 90, 60));
            DrawRectangle((int)g.x - 7, (int)g.y - 5, 14, 1, Cc(160, 125, 85));
            DrawRectangle((int)g.x - 1, (int)g.y - 52, 3, 48, Cc(90, 96, 110));
            DrawRectangle((int)g.x - 1, (int)g.y - 52, 1, 48, Cc(150, 156, 170));
            Vector2 top{g.x + 0.5f, g.y - 54};
            // N/E/S/W arms
            for (int k = 0; k < 4; k++) {
                float a = k * PI_F / 2;
                Vector2 e{std::cos(a) * 8, std::sin(a) * 8 * DEPTH};
                DrawLineEx(top + Vector2{0, 6}, top + Vector2{0, 6} + e, 1, Cc(170, 150, 90));
            }
            Vector2 d{std::cos(c.spin), std::sin(c.spin) * DEPTH};
            Vector2 n{-d.y, d.x};
            Color vc = c.owner >= 0 && c.t > 0 ? TeamColor(c.owner) : Cc(214, 176, 80);
            // the arrow: a pointed head, a bird body and a feathered tail
            Vector2 head = top + d * 13.0f, tail = top - d * 13.0f;
            DrawLineEx(tail, head, 2, ColorBrightness(vc, -0.3f));
            DrawTriangle(head + d * 4.0f, head + n * 3.0f, head - n * 3.0f, vc);
            DrawTriangle(head + d * 4.0f, head - n * 3.0f, head + n * 3.0f, vc);
            DrawTriangle(tail, tail - d * 5.0f + n * 6.0f, tail - d * 5.0f - n * 6.0f, vc);
            DrawTriangle(tail, tail - d * 5.0f - n * 6.0f, tail - d * 5.0f + n * 6.0f, vc);
            DrawCircleV(top + d * 2.0f + Vector2{0, -3}, 3, vc);   // the bird
            DrawCircleV(top + d * 4.0f + Vector2{0, -5}, 2, vc);
            DrawCircleV(top, 1.5f, Cc(240, 230, 200));
            if (fl > 0) { VAdd(); VGlow(top, 14, WHITE, 0.6f); VNorm(); }
        } break;
        case Centerpiece::Hive: {   // a gnarled stump with a big striped hive hanging from it
            bool smoked = c.phase == 2;
            VShadow({g.x, g.y + 1}, R * 1.2f, R * 0.5f, 0.6f);
            DrawRectangle((int)(g.x - R * 0.7f), (int)g.y - 20, (int)(R * 1.4f), 20, Cc(96, 70, 46));
            DrawEllipse((int)g.x, (int)g.y, R * 0.7f, R * 0.7f * DEPTH, Cc(96, 70, 46));
            DrawRectangle((int)(g.x - R * 0.7f), (int)g.y - 20, 3, 20, Cc(126, 96, 64));
            for (int k = 0; k < 3; k++) DrawLineEx({g.x - R * 0.7f - 2 - k * 2, g.y - 2.0f}, {g.x - R * 0.7f + 2, g.y - 8.0f - k * 3}, 2, Cc(86, 62, 40));   // roots
            Vector2 hc{g.x, g.y - 34 + std::sin(time * 3) * 0.6f};
            Color band1 = smoked ? Cc(120, 115, 105) : Cc(214, 170, 60), band2 = smoked ? Cc(80, 76, 70) : Cc(150, 104, 40);
            for (int k = 0; k < 5; k++) {
                float y = hc.y - 12 + k * 5.0f;
                float w = R * (0.55f + 0.45f * std::sin((k + 0.6f) / 5.4f * PI_F));
                DrawEllipse((int)hc.x, (int)y, w, 3.4f, k % 2 ? band2 : band1);
            }
            DrawEllipse((int)hc.x, (int)hc.y + 6, 3.5f, 2.5f, Cc(40, 26, 14));   // the entrance
            float full = c.charge / 100.0f;
            if (!smoked && full >= 1) { VAdd(); VGlow(hc, 18, kSwarm, 0.25f + 0.1f * std::sin(time * 8)); VNorm(); }
            if (fl > 0) DrawCircleV(hc, R * 0.9f, Fade(WHITE, 0.4f));
        } break;
        case Centerpiece::MilkTank: {   // a big dairy tank with Miltank-pink spots and a tap
            float fill = c.charge / 100.0f;
            VShadow({g.x, g.y + 1}, R * 1.15f, R * 0.5f, 0.6f);
            const int H = 40;
            DrawRectangle((int)(g.x - R), (int)(g.y - H), (int)(R * 2), H, Cc(228, 228, 232));
            DrawEllipse((int)g.x, (int)g.y, R, R * DEPTH, Cc(228, 228, 232));
            DrawRectangle((int)(g.x - R), (int)(g.y - H), 4, H, Cc(250, 250, 252));
            DrawRectangle((int)(g.x + R - 4), (int)(g.y - H), 4, H, Cc(190, 190, 198));
            DrawCircleV({g.x - 6, g.y - 28}, 4, Cc(240, 150, 180));   // spots
            DrawCircleV({g.x + 7, g.y - 16}, 5, Cc(240, 150, 180));
            DrawCircleV({g.x - 2, g.y - 9}, 3, Cc(240, 150, 180));
            DrawEllipse((int)g.x, (int)(g.y - H), R, R * DEPTH, Cc(200, 200, 208));
            DrawEllipse((int)g.x, (int)(g.y - H), R - 3, (R - 3) * DEPTH, Cc(170, 170, 180));
            DrawRectangle((int)g.x - 2, (int)(g.y - H - 6), 4, 6, Cc(170, 170, 180));
            // the level gauge
            DrawRectangle((int)(g.x + R - 9), (int)(g.y - H + 6), 4, 26, Cc(60, 60, 70));
            DrawRectangle((int)(g.x + R - 8), (int)(g.y - H + 7 + 24 * (1 - fill)), 2, (int)(24 * fill), fill >= 1 ? Cc(255, 200, 220) : kMilk);
            // the tap, dripping when it's full
            DrawRectangle((int)(g.x - R - 4), (int)g.y - 10, 5, 3, Cc(160, 160, 170));
            if (fill >= 1) {
                float ph = std::fmod(time * 1.4f, 1.0f);
                DrawCircleV({g.x - R - 3, g.y - 6 + ph * 6}, 1.2f, Fade(kMilk, 1 - ph));
            }
            if (fl > 0) DrawRectangle((int)(g.x - R), (int)(g.y - H), (int)(R * 2), H, Fade(WHITE, 0.5f));
        } break;
        case Centerpiece::Lamp: {   // a small white-and-red lighthouse with the lamp room on top
            VShadow({g.x, g.y + 1}, R * 1.2f, R * 0.5f, 0.6f);
            const float H = 48;
            for (int k = 0; k < 4; k++) {   // a tapering tower in bands
                float y0 = g.y - k * H / 4, y1 = g.y - (k + 1) * H / 4;
                float w0 = R * (1.0f - 0.08f * k), w1 = R * (1.0f - 0.08f * (k + 1));
                Color bc = k % 2 ? Cc(200, 60, 60) : Cc(236, 234, 228);
                DrawTriangle({g.x - w0, y0}, {g.x + w0, y0}, {g.x + w1, y1}, bc);
                DrawTriangle({g.x - w0, y0}, {g.x + w1, y1}, {g.x - w1, y1}, bc);
                DrawLineEx({g.x - w0 + 2, y0}, {g.x - w1 + 2, y1}, 2, Fade(WHITE, 0.4f));
            }
            DrawEllipse((int)g.x, (int)g.y, R, R * DEPTH, Cc(236, 234, 228));
            Vector2 lamp{g.x, g.y - H - 5};
            DrawRectangle((int)lamp.x - 8, (int)(g.y - H - 1), 16, 2, Cc(60, 60, 70));
            DrawRectangle((int)lamp.x - 6, (int)lamp.y - 4, 12, 8, Cc(255, 240, 180));
            for (int k = -1; k <= 1; k++) DrawRectangle((int)lamp.x + k * 4, (int)lamp.y - 4, 1, 8, Cc(80, 80, 90));
            DrawTriangle({lamp.x - 8, lamp.y - 4}, {lamp.x + 8, lamp.y - 4}, {lamp.x, lamp.y - 11}, Cc(70, 74, 86));
            Vector2 d{std::cos(c.spin), std::sin(c.spin) * DEPTH};
            VAdd();
            VGlow(lamp, 14, kBeam, 0.6f);
            VGlow(lamp + d * 6.0f, 9, WHITE, 0.6f);
            if (fl > 0) VGlow(lamp, 24, WHITE, 0.7f);
            VNorm();
        } break;
        default: DrawHoennCenterBody(o); break;
    }
}

// Above everything: the generator's charge label, the orb's owner tag, the bloom's pollen haze.
void Battle::DrawCenterTop() const {
    const Centerpiece& c = field.center;
    switch (c.kind) {
        case Centerpiece::Bloom:
            if (c.phase == 2) {
                Vector2 g = ToScreen(c.pos, 10);
                VAdd();
                for (int i = 0; i < 7; i++) {
                    float a = time * 0.4f + i * 0.9f;
                    Vector2 p{g.x + std::cos(a) * 60 * (0.4f + 0.6f * VRand(7, i)), g.y + std::sin(a * 1.3f) * 22};
                    VGlow(p, 26, kPollen, 0.12f);
                }
                VNorm();
            }
            break;
        case Centerpiece::Generator: {
            Vector2 g = ToScreen(c.pos, 46);
            const char* t = c.phase == 1 ? "OVERLOAD" : TextFormat("%d%%", (int)c.charge);
            int w = GMeasureText(t, 10);
            GDrawText(t, (int)g.x - w / 2, (int)g.y, 10, c.phase == 1 ? Cc(255, 120, 80) : kVolt);
        } break;
        case Centerpiece::Orb:
            if (c.owner >= 0 && c.phase != 2) {   // a thin bar for how long it stays on your side
                Vector2 g = ToScreen(c.pos, 44);
                DrawRectangle((int)g.x - 10, (int)g.y, 20, 2, Fade(BLACK, 0.5f));
                DrawRectangle((int)g.x - 10, (int)g.y, (int)(20 * Clamp01(c.t / 9)), 2, TeamColor(c.owner));
            }
            break;
        case Centerpiece::Vane:
            if (c.owner >= 0 && c.t > 0) {
                Vector2 g = ToScreen(c.pos, 64);
                DrawRectangle((int)g.x - 10, (int)g.y, 20, 2, Fade(BLACK, 0.5f));
                DrawRectangle((int)g.x - 10, (int)g.y, (int)(20 * Clamp01(c.t / 15)), 2, TeamColor(c.owner));
            }
            break;
        case Centerpiece::Hive:
        case Centerpiece::MilkTank:
            if (c.phase == 0 && c.charge < 100) {   // refilling
                Vector2 g = ToScreen(c.pos, c.kind == Centerpiece::Hive ? 52 : 50);
                DrawRectangle((int)g.x - 10, (int)g.y, 20, 2, Fade(BLACK, 0.5f));
                DrawRectangle((int)g.x - 10, (int)g.y, (int)(20 * Clamp01(c.charge / 100)), 2, c.kind == Centerpiece::Hive ? kSwarm : Cc(255, 200, 220));
            }
            break;
        default: break;
    }
}

// Gallery/testing: set the centerpiece off as if side 0 had just hit it toward the right.
void Battle::DebugCenter() {
    Centerpiece& c = field.center;
    HitSpec h = EnvSpec(Type::Fighting, 90);
    h.side = 0;
    h.heavy = true;
    c.hitCd = 0;
    switch (c.kind) {
        case Centerpiece::Boulder: CenterHit(h, {1, 0.15f}, true); break;
        case Centerpiece::Drain: c.phase = 1; c.t = 0.3f; break;
        case Centerpiece::Generator: c.charge = 100; break;
        case Centerpiece::Bloom: c.phase = 1; c.t = 0.3f; c.charge = 1; break;
        case Centerpiece::Vat: c.charge = 100; CenterHit(h, {1, 0}, true); break;
        case Centerpiece::Orb: h.type = Type::Psychic; CenterHit(h, {1, 0}, false); break;
        case Centerpiece::Vent: c.phase = 0; c.t = 0.01f; break;
        case Centerpiece::Spinner: c.phase = 0; c.t = 0.01f; break;
        case Centerpiece::Vane: CenterHit(h, {1, 0}, true); field.windOn = 0; field.windT = 0.01f; break;
        case Centerpiece::Hive: c.charge = 100; CenterHit(h, {1, 0}, true); break;
        case Centerpiece::MilkTank: c.charge = 100; CenterHit(h, {1, 0}, true); break;
        case Centerpiece::Lamp: CenterHit(h, {1, 0}, true); break;
        default: break;
    }
}
