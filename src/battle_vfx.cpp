// Battle visual effects: spawning (inside the simulation, so online games stay in step) and the
// drawing of the newer effect kinds. How each move looks comes from vfx.cpp.
#include <algorithm>
#include <cmath>
#include "rng.h"
#include "arena.h"
#include "battle.h"
#include "ui.h"
#include "vec.h"
#include "vfx.h"
#include "rlgl.h"
#include "settings.h"

namespace {

constexpr float PI_F = 3.14159265f;
constexpr size_t MAX_PARTS = 1400;

float Frand(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }
Vector2 RandDir() { float a = Frand(0, 2 * PI_F); return {std::cos(a), std::sin(a)}; }
Color Cc(int r, int g, int b) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, 255}; }
Color Vary(Color c, float lo, float hi) { return ColorBrightness(c, Frand(lo, hi)); }
float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
float EaseOutQ(float u) { u = Clamp01(u); return 1 - (1 - u) * (1 - u); }
float EaseInQ(float u) { u = Clamp01(u); return u * u; }

Particle gDummy;

} // namespace

// ---------------------------------------------------------------- Spawning helpers

Particle& Battle::Emit(PK k, Vector2 pos, float z, Vector2 vel, float vz, float life, float size, Color c, bool grav) {
    if (parts.size() >= MAX_PARTS) { gDummy = Particle(); return gDummy; }
    Particle p;
    p.kind = k;
    p.pos = pos;
    p.z = z;
    p.vel = vel;
    p.vz = vz;
    p.life = p.maxLife = life;
    p.size = size;
    p.col = c;
    p.grav = grav;
    p.rot = Frand(0, 2 * PI_F);
    parts.push_back(p);
    return parts.back();
}

Fx& Battle::AddFx(FxKind k, Vector2 pos, float life, Color c, float radius, float z) {
    Fx fx;
    fx.kind = k;
    fx.pos = pos;
    fx.maxLife = life;
    fx.col = c;
    fx.radius = radius;
    fx.z = z;
    fx.seed = (uint32_t)SimRand(1, 0x3fffffff);
    fxs.push_back(fx);
    return fxs.back();
}


// ---------------------------------------------------------------- Hits

void Battle::HitVfx(const HitSpec& h, Vector2 pos, float z, Vector2 dir) {
    const Look& L = GetLook(h.look, h.type);
    bool light = h.light;
    float m = light ? 0.35f : (h.power >= 90 ? 1.5f : 1.0f);
    Vector2 bias = dir * 40.0f;
    if (!light) {
        Fx& f = AddFx(FxKind::Hit, pos, 0.3f, L.glow, h.power >= 90 ? 17.0f : 12.0f, z);
        f.type = h.type;
    }
    Color c0 = ImpactColor(L.impact, 0), c1 = ImpactColor(L.impact, 1), c2 = ImpactColor(L.impact, 2);
    auto N = [&](int base) { return std::max(1, (int)(base * m)); };
    auto spray = [&](PK k, int n, float spd, Color c, float size, float life) {
        for (int i = 0; i < n; i++) {
            Vector2 v = RandDir() * Frand(spd * 0.3f, spd) + bias;
            Particle& p = Emit(k, pos + RandDir() * Frand(0, 3), z + Frand(-3, 3), v, Frand(10, 60), Frand(life * 0.6f, life), size * Frand(0.7f, 1.2f), Vary(c, -0.15f, 0.25f), false);
            p.spin = Frand(-10, 10);
            p.drag = 4;
            if (k == PK::Debris || k == PK::Drop || k == PK::Leaf || k == PK::Shard || k == PK::Petal || k == PK::Coin) { p.grav = true; p.vz = Frand(40, 100); }
            if (k == PK::Smoke) { p.vz = Frand(5, 20); p.grow = size * 1.5f; p.drag = 2; }
            if (k == PK::Flame || k == PK::Bubble || k == PK::Wisp) { p.vz = Frand(25, 60); }
            if (k == PK::Ring) { p.vel = {0, 0}; p.vz = 0; p.size = 2; p.grow = size * 6; }
        }
    };
    switch (L.impact) {
        case Impact::Pow:    spray(PK::Spark, N(8), 150, c0, 1.3f, 0.25f); spray(PK::Ember, N(4), 70, c1, 1.0f, 0.35f); break;
        case Impact::Fire:   spray(PK::Flame, N(9), 70, c1, 2.0f, 0.45f); spray(PK::Spark, N(6), 140, c1, 1.1f, 0.3f); spray(PK::Smoke, N(2), 20, Cc(80, 70, 65), 4, 0.8f); break;
        case Impact::Spark:  spray(PK::Spark, N(12), 200, c1, 1.3f, 0.25f); spray(PK::Glow, N(3), 40, c1, 1.2f, 0.25f);
                             if (!light) { Fx& r = AddFx(FxKind::Radial, pos, 0.2f, L.glow, 14, z); r.len = 4; } break;
        case Impact::Splash: spray(PK::Drop, N(12), 90, c1, 2.0f, 0.6f); spray(PK::Bubble, N(3), 40, c0, 1.6f, 0.5f); break;
        case Impact::Leaves: spray(PK::Leaf, N(7), 90, L.core, 2.2f, 0.7f); spray(PK::Glow, N(3), 40, c1, 1.0f, 0.3f); break;
        case Impact::Frost:  spray(PK::Shard, N(7), 110, c0, 2.0f, 0.6f); spray(PK::Snow, N(5), 50, WHITE, 1.5f, 0.6f); spray(PK::Glow, N(3), 30, c1, 1.4f, 0.3f); break;
        case Impact::Debris: spray(PK::Debris, N(8), 110, L.core, 2.2f, 0.7f); spray(PK::Smoke, N(3), 25, Cc(150, 135, 115), 4, 0.8f); break;
        case Impact::Poison: spray(PK::Bubble, N(7), 50, c1, 2.0f, 0.6f); spray(PK::Drop, N(5), 90, c1, 1.6f, 0.5f); break;
        case Impact::Psy:    spray(PK::Ring, N(2), 0, c1, 2.5f, 0.35f); spray(PK::Glow, N(6), 70, c1, 1.3f, 0.35f); break;
        case Impact::Shadow: spray(PK::Wisp, N(6), 60, c1, 1.8f, 0.5f); spray(PK::Smoke, N(3), 20, Cc(40, 25, 55), 4, 0.7f); break;
        case Impact::Dragon: spray(PK::Glow, N(7), 90, c1, 1.6f, 0.35f); spray(PK::Spark, N(6), 160, c0, 1.2f, 0.25f); break;
        case Impact::Fairy:  spray(PK::Star, N(6), 80, c0, 2.0f, 0.55f); spray(PK::Glow, N(4), 50, c1, 1.2f, 0.35f); break;
        case Impact::Metal:  spray(PK::Spark, N(12), 220, c0, 1.2f, 0.22f); spray(PK::Ember, N(3), 60, c1, 1.0f, 0.3f); break;
        case Impact::Wind:   spray(PK::Spark, N(7), 160, c0, 1.0f, 0.25f); spray(PK::Feather, N(2), 40, WHITE, 2.0f, 0.7f); break;
        case Impact::Bug:    spray(PK::Glow, N(6), 70, c1, 1.1f, 0.3f); spray(PK::Pixel, N(6), 90, c2, 1.5f, 0.4f); break;
        case Impact::Sand:   spray(PK::Smoke, N(4), 40, c1, 4, 0.7f); spray(PK::Debris, N(5), 90, c2, 1.6f, 0.5f); break;
        case Impact::Sound:  spray(PK::Ring, N(3), 0, c0, 2.5f, 0.4f); break;
    }
}

// ---------------------------------------------------------------- Explosions

void Battle::Boom(Vector2 pos, float r, Impact im, float z) {
    Color c0 = ImpactColor(im, 0), c1 = ImpactColor(im, 1), c2 = ImpactColor(im, 2);
    Fx& e = AddFx(FxKind::Explosion, pos, 0.5f + r / 220.0f, c1, r, z + r * 0.25f);
    e.sub = (uint8_t)im;
    if (im == Impact::Fire || im == Impact::Pow || im == Impact::Debris || im == Impact::Poison || im == Impact::Shadow || im == Impact::Dragon || im == Impact::Sand)
        AddFx(FxKind::Scorch, pos, 4.0f, im == Impact::Poison ? Cc(70, 30, 90) : Cc(35, 28, 24), r * 0.55f);
    AddFx(FxKind::Shock, pos, 0.45f, c0, r * 1.4f);
    if (r >= 45) AddFx(FxKind::Flash, pos, 0.28f, c0, r);
    bool fiery = im == Impact::Fire || im == Impact::Pow;
    int nd = 6 + (int)(r / 5);
    for (int i = 0; i < nd; i++) {   // debris
        Vector2 v = RandDir() * Frand(r * 1.5f, r * 4.0f);
        PK k = im == Impact::Poison ? PK::Drop : (im == Impact::Splash ? PK::Drop : (im == Impact::Frost ? PK::Shard : PK::Debris));
        Color dc = im == Impact::Poison ? c1 : (k == PK::Debris ? Cc(105, 92, 82) : c0);
        Particle& p = Emit(k, pos, z + 2, v, Frand(70, 160), Frand(0.7f, 1.1f), Frand(1.6f, 3.0f), Vary(dc, -0.2f, 0.2f), true);
        p.spin = Frand(-12, 12);
        p.drag = 1.2f;
    }
    for (int i = 0; i < 16; i++) {   // sparks
        Particle& p = Emit(PK::Spark, pos, z + 4, RandDir() * Frand(r * 3.5f, r * 7.0f), Frand(10, 90), Frand(0.25f, 0.45f), 1.4f, c0, false);
        p.drag = 3.5f;
    }
    for (int i = 0; i < 12; i++) {   // embers drifting down
        Particle& p = Emit(fiery ? PK::Ember : PK::Glow, pos + RandDir() * Frand(0, r * 0.3f), z + Frand(4, r * 0.4f),
                           RandDir() * Frand(r * 0.8f, r * 2.2f), Frand(50, 140), Frand(0.7f, 1.3f), Frand(0.8f, 1.4f), Vary(c1, -0.1f, 0.3f), true);
        p.drag = 2.0f;
    }
    Color sc = fiery ? Cc(75, 68, 62) : Vary(c2, -0.3f, -0.1f);
    for (int i = 0; i < 9; i++) {    // smoke
        Particle& p = Emit(PK::Smoke, pos + RandDir() * Frand(0, r * 0.45f), z + Frand(0, r * 0.4f),
                           RandDir() * Frand(r * 0.2f, r * 0.8f), Frand(12, 30), Frand(1.0f, 1.7f), r * Frand(0.22f, 0.35f), sc, false);
        p.grow = r * 0.35f;
        p.drag = 1.8f;
        p.spin = Frand(-1, 1);
    }
    Shake(r >= 60 ? 6 : (r >= 34 ? 4 : 3), 0.25f + r / 300.0f);
}

// ---------------------------------------------------------------- Casting

void Battle::CastVfx(int s, const MoveDef& m, const HitSpec& h, Vector2 a) {
    const Fighter& f = sides[s].Cur();
    const Look& L = GetLook(h.look, m.type);
    Vector2 mouth = f.pos + a * 10.0f;
    switch (m.kind) {
        case MoveKind::Projectile: case MoveKind::Spread: case MoveKind::Boomerang: {
            Fx& fx = AddFx(FxKind::Hit, mouth, 0.12f, L.glow, 6, 9);
            fx.type = m.type;
            for (int i = 0; i < 3; i++) {
                Particle& p = Emit(L.trail, mouth, 9, Rot(a, Frand(-0.6f, 0.6f)) * Frand(30, 80), Frand(0, 20), 0.3f, 1.4f, L.glow, false);
                p.spin = Frand(-8, 8);
            }
        } break;
        case MoveKind::Burst: {
            if (m.power <= 0 && L.cast == Cast::None) {
                for (int i = 0; i < 20; i++) {
                    Vector2 d = RandDir();
                    Particle& p = Emit(PK::Glow, f.pos + d * 6.0f, 4, d * (m.radius * Frand(1.5f, 2.6f)), Frand(0, 15), 0.35f, 1.0f, L.glow, false);
                    p.drag = 4;
                }
                break;
            }
            switch (L.cast) {
                case Cast::Explosion: Boom(f.pos, L.boomR, L.impact, 4); break;
                case Cast::Discharge: {
                    AddFx(FxKind::Radial, f.pos, 0.42f, L.glow, m.radius, 8).len = 9;
                    AddFx(FxKind::Flash, f.pos, 0.15f, Cc(255, 250, 200), m.radius);
                    for (int i = 0; i < 22; i++) Emit(PK::Spark, f.pos, 8, RandDir() * Frand(m.radius * 3, m.radius * 6), Frand(0, 40), 0.3f, 1.3f, L.glow, false).drag = 3;
                } break;
                case Cast::Sparkle: {
                    for (int i = 0; i < 16; i++) {
                        Vector2 d = RandDir();
                        Particle& p = Emit(PK::Star, f.pos + d * 4.0f, 8, d * Frand(m.radius * 1.5f, m.radius * 3), Frand(10, 40), Frand(0.5f, 0.8f), Frand(1.6f, 2.4f), Vary(L.core, -0.1f, 0.2f), false);
                        p.spin = Frand(-8, 8);
                        p.drag = 3;
                    }
                    for (int i = 0; i < 10; i++) Emit(PK::Glow, f.pos, 8, RandDir() * Frand(m.radius, m.radius * 3), Frand(10, 30), 0.4f, 1.5f, L.glow, false).drag = 3;
                    AddFx(FxKind::Flash, f.pos, 0.18f, L.glow, m.radius);
                } break;
                case Cast::Flames: {
                    for (int i = 0; i < 28; i++) {
                        Vector2 d = RandDir();
                        Particle& p = Emit(PK::Flame, f.pos + d * 5.0f, 3, d * Frand(m.radius * 2.2f, m.radius * 3.4f), Frand(15, 45), Frand(0.35f, 0.55f), Frand(2.0f, 3.0f), L.glow, false);
                        p.drag = 3.5f;
                    }
                } break;
                case Cast::Powder: {
                    for (int i = 0; i < 34; i++) {
                        Vector2 d = RandDir();
                        Particle& p = Emit(PK::Glow, f.pos + d * Frand(2, 8), Frand(6, 16), d * Frand(m.radius * 0.8f, m.radius * 2.0f), Frand(-6, 8), Frand(0.8f, 1.3f), Frand(1.4f, 2.2f), Vary(L.glow, -0.1f, 0.3f), false);
                        p.drag = 2.5f;
                    }
                } break;
                case Cast::Sound: {
                    for (int i = 0; i < 3; i++) {
                        Particle& p = Emit(PK::Ring, f.pos, 8, {0, 0}, 0, 0.35f + i * 0.08f, 3, i % 2 ? L.glow : WHITE, false);
                        p.grow = m.radius * (2.6f - i * 0.4f);
                    }
                } break;
                default: {
                    AddFx(FxKind::Shock, f.pos, 0.45f, L.glow, m.radius * 1.15f);
                    AddFx(FxKind::Ring, f.pos, 0.5f, L.glow, m.radius * 1.05f);
                    if (m.power >= 80) AddFx(FxKind::Flash, f.pos, 0.15f, L.glow, m.radius);
                    if (m.type == Type::Dark || m.type == Type::Ghost) {   // dark waves read as smoke, not light
                        for (int i = 0; i < 18; i++) {
                            float ang = i * PI_F * 2 / 18;
                            Vector2 d{std::cos(ang), std::sin(ang)};
                            Particle& p = Emit(PK::Smoke, f.pos + d * 6.0f, 4, d * (m.radius * Frand(2.2f, 2.8f)), Frand(4, 12), Frand(0.4f, 0.55f), 3.2f,
                                               m.type == Type::Dark ? Cc(50, 30, 70) : Cc(80, 50, 120), false);
                            p.grow = 7;
                            p.drag = 3.2f;
                        }
                    }
                    for (int i = 0; i < 24; i++) {
                        float ang = i * PI_F * 2 / 24;
                        Vector2 d{std::cos(ang), std::sin(ang)};
                        Particle& p = Emit(L.trail == PK::Smoke ? PK::Glow : L.trail, f.pos, 5, d * (m.radius * 3.0f), 0, 0.32f, 1.8f, L.glow, false);
                        p.spin = Frand(-8, 8);
                        p.drag = 3;
                    }
                } break;
            }
        } break;
        case MoveKind::Wave: {
            if (L.cast == Cast::Quake) {
                AddFx(FxKind::Crack, f.pos, 1.4f, Cc(60, 45, 30), m.radius).len = (float)(5 + SimRand(0, 3));
                for (int i = 0; i < 10; i++) {
                    Particle& p = Emit(PK::Smoke, f.pos + RandDir() * Frand(4, 14), 2, RandDir() * Frand(20, 60), Frand(5, 15), Frand(0.8f, 1.2f), Frand(4, 6), Cc(160, 135, 100), false);
                    p.grow = 8;
                    p.drag = 2;
                }
            } else if (L.cast == Cast::Sound) {
                for (int i = 0; i < 2; i++) Emit(PK::Ring, f.pos, 8, {0, 0}, 0, 0.3f, 3, WHITE, false).grow = 60;
            }
        } break;
        case MoveKind::Heal: {
            for (int i = 0; i < 10; i++) {
                Particle& p = Emit(PK::Glow, f.pos + Vector2{Frand(-9, 9), Frand(-4, 4)}, Frand(0, 8), {0, 0}, Frand(30, 55), 0.8f, 1.4f, Cc(150, 255, 160), false);
                p.drag = 1;
            }
            for (int i = 0; i < 4; i++) {
                Particle& p = Emit(PK::Star, f.pos + Vector2{Frand(-9, 9), Frand(-4, 4)}, Frand(4, 14), {0, 0}, Frand(20, 40), 0.7f, 1.6f, Cc(200, 255, 200), false);
                p.spin = Frand(-5, 5);
            }
            for (int i = 0; i < 5; i++) Emit(PK::Plus, f.pos + Vector2{Frand(-10, 10), Frand(-4, 4)}, Frand(2, 14), {0, 0}, Frand(22, 40), Frand(0.6f, 0.9f), Frand(1.4f, 2.0f), Cc(140, 255, 150), false).drag = 1;
            Emit(PK::Ring, f.pos, 1, {0, 0}, 0, 0.45f, 3, Cc(150, 255, 160), false).grow = 40;
            AddFx(FxKind::Pillar, f.pos, 0.7f, Cc(140, 255, 160), 10);
        } break;
        case MoveKind::Buff: {   // the stat's colour: chevrons rising through a column of light
            bool down = m.amount > 0 && m.amount < 1;
            Color bc = m.stat == StatId::Atk ? Cc(255, 110, 70) : m.stat == StatId::Def ? Cc(90, 160, 255) : Cc(120, 240, 140);
            if (m.power > 0) bc = L.glow;
            for (int i = 0; i < 14; i++) {
                float ang = i * PI_F / 7;
                Particle& p = Emit(PK::Glow, f.pos + Vector2{std::cos(ang) * 11, std::sin(ang) * 6}, 0, {0, 0}, Frand(45, 80), Frand(0.5f, 0.75f), 1.3f, bc, false);
                p.drag = 1;
            }
            for (int i = 0; i < 5; i++) {
                Particle& p = Emit(PK::Arrow, f.pos + Vector2{Frand(-9, 9), Frand(-3, 3)}, Frand(-2, 10), {0, 0}, down ? -Frand(25, 40) : Frand(30, 50), Frand(0.55f, 0.8f), Frand(1.6f, 2.2f), bc, false);
                p.rot = down ? 4.0f : 0.0f;
                p.drag = 1;
            }
            if (m.stat == StatId::Def && m.power <= 0) Emit(PK::Shield, f.pos, 10, {0, 0}, 0, 0.45f, 3.4f, bc, false).spin = 2;
            Emit(PK::Ring, f.pos, 0, {0, 0}, 0, 0.5f, 4, bc, false).grow = 34;
            Emit(PK::Ring, f.pos, 10, {0, 0}, 0, 0.4f, 2, WHITE, false).grow = 24;
            AddFx(FxKind::Pillar, f.pos, 0.6f, bc, 9);
        } break;
        case MoveKind::Dash: {
            for (int i = 0; i < 6; i++) {
                Particle& p = Emit(PK::Smoke, f.pos - a * 6.0f + RandDir() * 3.0f, 1, -a * Frand(30, 60) + RandDir() * 15.0f, Frand(5, 15), 0.6f, 3.0f, Cc(190, 180, 165), false);
                p.grow = 6;
            }
        } break;
        case MoveKind::Blink: {
            for (int i = 0; i < 8; i++) {
                Particle& p = Emit(PK::Smoke, f.pos + RandDir() * 4.0f, 6, RandDir() * Frand(15, 40), Frand(5, 20), 0.5f, 3.5f, Vary(L.glow, -0.4f, -0.1f), false);
                p.grow = 7;
            }
        } break;
        case MoveKind::Sweep: {   // the beam's muzzle flares and the ground scorches under the user's feet
            AddFx(FxKind::Hit, mouth, 0.2f, L.glow, 12, 9).type = m.type;
            AddFx(FxKind::Ring, f.pos, 0.35f, L.glow, 22, 1);
            for (int i = 0; i < 10; i++) {
                Particle& p = Emit(PK::Spark, mouth, 9, Rot(a, Frand(-1.2f, 1.2f)) * Frand(60, 140), Frand(0, 40), 0.3f, 1.2f, L.core, false);
                p.drag = 4;
            }
            for (int i = 0; i < 6; i++) {
                Particle& p = Emit(PK::Smoke, f.pos + RandDir() * 6.0f, 1, RandDir() * Frand(20, 50) - a * 30.0f, Frand(4, 12), 0.6f, 3.0f, Cc(170, 160, 150), false);
                p.grow = 6;
            }
        } break;
        case MoveKind::Tether: {   // a burst of the line's colour at the user as it lashes out
            Color c = m.tether == 1 ? Cc(140, 240, 140) : (m.tether == 3 ? Cc(150, 70, 190) : L.glow);
            if (m.name == "Dream Eater") c = Cc(230, 140, 230);
            if (m.name == "Oblivion Wing") c = Cc(230, 60, 80);
            AddFx(FxKind::Hit, mouth, 0.15f, c, 7, 10).type = m.type;
            for (int i = 0; i < 6; i++) Emit(PK::Glow, mouth, 10, Rot(a, Frand(-0.8f, 0.8f)) * Frand(30, 70), Frand(0, 20), 0.35f, 1.3f, c, false).drag = 4;
        } break;
        case MoveKind::Grapple: {   // a snap as the hook / vine / tongue fires
            AddFx(FxKind::Hit, mouth, 0.1f, L.glow, 6, 9).type = m.type;
            for (int i = 0; i < 4; i++) Emit(PK::Pixel, mouth, 9, Rot(a, Frand(-0.5f, 0.5f)) * Frand(40, 90), Frand(0, 15), 0.25f, 1.0f, Cc(240, 235, 220), false);
        } break;
        case MoveKind::Roll: {   // curling up: a ring tightening around the user as it tucks into a ball
            Fx& fx = AddFx(FxKind::Mind, f.pos, 0.3f, L.glow, 14, 6);
            fx.type = m.type;
            for (int i = 0; i < 6; i++) {
                Vector2 d = RandDir();
                Emit(PK::Glow, f.pos + d * 14.0f, 6, d * -40.0f, Frand(0, 10), 0.3f, 1.2f, L.glow, false).drag = 3;
            }
        } break;
        case MoveKind::Throw: {   // a grab: dust kicked up as the user plants and reaches
            for (int i = 0; i < 6; i++) {
                Particle& p = Emit(PK::Smoke, f.pos - a * 4.0f + RandDir() * 4.0f, 1, RandDir() * Frand(20, 45), Frand(4, 10), 0.5f, 2.8f, Cc(190, 180, 165), false);
                p.grow = 5;
            }
        } break;
        default: break;
    }
}

// ---------------------------------------------------------------- Projectile trails and landings

void Battle::TrailVfx(const Projectile& p, Vector2 at) {
    const Look& L = GetLook(p.hit.look, p.hit.type);
    if (Frand(0, 1) > L.trailRate) return;
    Vector2 back = SafeNorm(p.vel, {0, 0}) * -25.0f;
    Vector2 jit = RandDir() * Frand(5, 20);
    PK k = L.trail;
    Color c = Vary(L.glow, -0.1f, 0.3f);
    float z = p.z + Frand(-1.5f, 1.5f);
    switch (k) {
        case PK::Flame: { Particle& q = Emit(k, at, z, back + jit, Frand(10, 25), Frand(0.25f, 0.4f), Frand(1.4f, 2.0f) * std::max(0.7f, p.radius / 4.5f), L.glow, false); q.drag = 4; } break;
        case PK::Smoke: { Particle& q = Emit(k, at, z, back * 0.3f + jit * 0.3f, Frand(4, 12), 0.6f, 2.5f, Cc(120, 115, 120), false); q.grow = 6; } break;
        case PK::Spark: Emit(k, at, z, RandDir() * Frand(40, 90), Frand(-20, 20), 0.18f, 1.0f, c, false); break;
        case PK::Drop: case PK::Debris: Emit(k, at, z, back * 0.4f + jit, Frand(10, 30), 0.4f, 1.2f, k == PK::Debris ? L.core : c, true).spin = Frand(-8, 8); break;
        case PK::Ring: { Particle& q = Emit(k, at, z, {0, 0}, 0, 0.3f, 1.5f, c, false); q.grow = 16; } break;
        case PK::Bubble: Emit(k, at, z, jit * 0.5f, Frand(8, 20), 0.5f, Frand(1.0f, 1.6f), c, false); break;
        default: {
            Particle& q = Emit(k, at, z, back * 0.4f + jit * 0.6f, Frand(-5, 12), Frand(0.3f, 0.5f), k == PK::Glow ? 1.1f : 1.8f, k == PK::Leaf || k == PK::Petal ? L.core : c, false);
            q.spin = Frand(-9, 9);
        } break;
    }
}

void Battle::LandVfx(const HitSpec& h, Vector2 pos, float radius, bool drop) {
    const Look& L = GetLook(h.look, h.type);
    if (L.skyStrike) {
        Fx& f = AddFx(FxKind::Strike, pos, 0.35f, L.glow, radius);
        f.len = L.boltWidth;
        AddFx(FxKind::Flash, pos, 0.12f, Cc(255, 255, 220), radius);
        for (int i = 0; i < 16; i++) Emit(PK::Spark, pos, 3, RandDir() * Frand(80, 200), Frand(20, 80), 0.3f, 1.3f, L.glow, false).drag = 3;
        AddFx(FxKind::Scorch, pos, 2.0f, Cc(40, 35, 30), radius * 0.5f);
        Shake(3, 0.2f);
        return;
    }
    if (L.boom) {
        Boom(pos, std::max(radius, L.boomR * (drop ? 0.6f : 1.0f)), L.impact, 2);
        return;
    }
    Fx& ring = AddFx(FxKind::Shock, pos, 0.35f, L.glow, radius * 1.2f);
    (void)ring;
    HitSpec hh = h;
    hh.light = false;
    hh.power = std::max(hh.power, 90);
    HitVfx(hh, pos, 3, {0, 0});
    if (L.impact == Impact::Debris || L.impact == Impact::Sand) {
        for (int i = 0; i < 4; i++) {
            Particle& p = Emit(PK::Smoke, pos + RandDir() * Frand(0, radius * 0.4f), 2, RandDir() * Frand(10, 30), Frand(4, 12), 0.9f, radius * 0.25f, Cc(150, 135, 115), false);
            p.grow = radius * 0.3f;
        }
    }
}

void Battle::BeamVfx(const HitSpec& h, Vector2 from, Vector2 to, float width) {
    const Look& L = GetLook(h.look, h.type);
    Fx& fx = AddFx(FxKind::Beam, from, 0.42f, L.glow, width, 10);
    fx.dir = SafeNorm(to - from);
    fx.len = Dist(from, to);
    fx.look = h.look;
    fx.type = h.type;
    float len = fx.len;
    Vector2 d = fx.dir, n{-d.y, d.x};
    PK k = L.trail == PK::Smoke ? PK::Glow : L.trail;
    for (int i = 0; i < 16; i++) {
        Vector2 p = from + d * Frand(10, len);
        Particle& q = Emit(k, p, 10, n * Frand(-60, 60) + d * Frand(0, 40), Frand(-10, 25), Frand(0.25f, 0.45f), k == PK::Glow ? 1.2f : 1.8f, Vary(L.glow, -0.1f, 0.3f), false);
        q.spin = Frand(-8, 8);
        q.drag = 4;
    }
    for (int i = 0; i < 8; i++) Emit(PK::Spark, to, 10, RandDir() * Frand(60, 160) - d * 40.0f, Frand(0, 50), 0.25f, 1.2f, L.core, false).drag = 3;
}

// ---------------------------------------------------------------- Status, hazards, waves, faint

void Battle::StatusVfx(Vector2 pos, Status st) {
    switch (st) {
        case Status::Burn:
            for (int i = 0; i < 12; i++) Emit(PK::Flame, pos + RandDir() * Frand(2, 9), Frand(0, 10), RandDir() * Frand(5, 25), Frand(30, 60), Frand(0.4f, 0.6f), 2.2f, Cc(255, 120, 40), false);
            break;
        case Status::Paralyze:
            AddFx(FxKind::Radial, pos, 0.3f, Cc(255, 230, 60), 18, 9).len = 5;
            for (int i = 0; i < 10; i++) Emit(PK::Spark, pos, 9, RandDir() * Frand(60, 140), Frand(-20, 30), 0.25f, 1.1f, Cc(255, 240, 120), false);
            break;
        case Status::Poison: case Status::Toxic:
            for (int i = 0; i < 10; i++) Emit(PK::Bubble, pos + RandDir() * Frand(2, 9), Frand(0, 8), RandDir() * Frand(5, 20), Frand(20, 45), Frand(0.6f, 0.9f), Frand(1.2f, 2.0f), Cc(190, 90, 230), false);
            break;
        case Status::Freeze:
            for (int i = 0; i < 10; i++) {
                Particle& p = Emit(PK::Shard, pos, 9, RandDir() * Frand(40, 100), Frand(30, 80), 0.6f, 2.0f, Cc(210, 245, 255), true);
                p.spin = Frand(-10, 10);
            }
            for (int i = 0; i < 6; i++) Emit(PK::Glow, pos + RandDir() * 6.0f, Frand(2, 14), {0, 0}, 5, 0.5f, 1.5f, Cc(180, 230, 255), false);
            break;
        case Status::Sleep:
            for (int i = 0; i < 3; i++) Emit(PK::Zzz, pos + Vector2{4.0f + i * 3, 0}, 18 + i * 5, {Frand(5, 15), 0}, 15, 0.9f, 1, WHITE, false);
            break;
        case Status::Confuse:
            for (int i = 0; i < 6; i++) {
                Particle& p = Emit(PK::Star, pos, 20, RandDir() * Frand(20, 40), Frand(-5, 10), 0.7f, 1.8f, Cc(255, 220, 90), false);
                p.spin = 6;
            }
            break;
        case Status::Seed:
            for (int i = 0; i < 6; i++) Emit(PK::Leaf, pos + RandDir() * 5.0f, Frand(2, 12), RandDir() * Frand(10, 40), Frand(10, 30), 0.7f, 2.0f, Cc(130, 220, 90), true).spin = Frand(-8, 8);
            break;
        default: break;
    }
}

void Battle::HazardVfx(const Hazard& h) {
    const Look& L = GetLook(h.hit.look, h.hit.type);
    float ang = Frand(0, PI_F * 2), r = Frand(0, h.radius);
    Vector2 pos = h.pos + Vector2{std::cos(ang) * r, std::sin(ang) * r * 0.8f};
    Vector2 vel = {0, 0};
    if (h.pull > 0) vel = Rot(Vector2{-std::sin(ang), std::cos(ang)}, 0.3f) * (40.0f + r);
    PK k = L.trail;
    Color c = Vary(L.glow, -0.1f, 0.35f);
    switch (h.hit.type) {
        case Type::Fire: k = PK::Flame; break;
        case Type::Poison: k = SimRand(0, 2) ? PK::Bubble : PK::Smoke; c = Vary(L.glow, -0.3f, 0.2f); break;
        case Type::Water: k = SimRand(0, 1) ? PK::Bubble : PK::Drop; break;
        case Type::Electric: k = PK::Spark; vel = RandDir() * 50.0f; break;
        case Type::Ground: k = PK::Smoke; c = Cc(170, 145, 105); break;
        case Type::Normal: if (L.trail == PK::Glow) { k = PK::Smoke; c = Cc(150, 150, 155); } break;
        default: break;
    }
    if (h.patch && k == PK::Smoke) k = PK::Glow;
    Particle& p = Emit(k, pos, Frand(0, 3), vel, h.pull > 0 ? Frand(5, 15) : Frand(15, 35), Frand(0.4f, 0.7f),
                       k == PK::Smoke ? 3.5f : (k == PK::Flame ? (h.patch ? 1.6f : 2.8f) : 1.6f), c, k == PK::Drop);
    p.spin = Frand(-6, 6);
    if (k == PK::Smoke) { p.grow = 5; p.drag = 2; }
    if (k == PK::Drop) p.vz = Frand(30, 60);
}

void Battle::WaveVfx(const ShockWave& w) {
    const Look& L = GetLook(w.hit.look, w.hit.type);
    float ang = Frand(0, 2 * PI_F);
    Vector2 d{std::cos(ang), std::sin(ang)};
    Vector2 pos = w.origin + d * std::min(w.r, w.maxR);
    switch (L.cast) {
        case Cast::Quake: {
            Particle& p = Emit(PK::Debris, pos, 0, d * Frand(10, 30), Frand(50, 110), 0.6f, Frand(1.5f, 2.6f), Cc(140, 115, 85), true);
            p.spin = Frand(-10, 10);
            Particle& q = Emit(PK::Smoke, pos, 1, d * 15.0f, Frand(5, 15), 0.8f, 3.5f, Cc(165, 140, 105), false);
            q.grow = 6;
        } break;
        case Cast::Sound: Emit(PK::Spark, pos, 8, d * 60.0f, 0, 0.2f, 1.0f, WHITE, false); break;
        default: {
            PK k = L.trail;
            if (w.hit.type == Type::Water) k = SimRand(0, 1) ? PK::Drop : PK::Bubble;
            if (k == PK::Ring) k = PK::Glow;
            Particle& p = Emit(k, pos, Frand(0, 6), d * Frand(10, 40), Frand(20, 50), Frand(0.35f, 0.55f), k == PK::Glow ? 1.3f : 2.0f, Vary(L.glow, -0.1f, 0.3f), k == PK::Drop || k == PK::Debris || k == PK::Leaf);
            p.spin = Frand(-6, 6);
        } break;
    }
}

void Battle::FaintVfx(Vector2 pos, Type t) {
    for (int i = 0; i < 8; i++) {
        Particle& p = Emit(PK::Smoke, pos + RandDir() * Frand(0, 6), Frand(2, 10), RandDir() * Frand(10, 30), Frand(8, 20), Frand(0.8f, 1.2f), 4, Cc(200, 200, 205), false);
        p.grow = 8;
    }
    for (int i = 0; i < 10; i++) Emit(PK::Glow, pos + RandDir() * 5.0f, Frand(2, 14), RandDir() * Frand(10, 40), Frand(20, 50), 0.7f, 1.2f, ColorBrightness(TypeColor(t), 0.3f), false);
    Emit(PK::Ring, pos, 1, {0, 0}, 0, 0.5f, 4, WHITE, false).grow = 40;
}

// ---------------------------------------------------------------- Drawing

void DrawCrackFx(Vector2 origin, float radius, float t, uint32_t seed, int arms, Color c) {
    float grow = Clamp01(t * 4);
    float a = t < 0.7f ? 1.0f : 1.0f - (t - 0.7f) / 0.3f;
    for (int k = 0; k < arms; k++) {
        float ang = (k + VRand(seed, k) * 0.7f) * 2 * PI_F / arms;
        Vector2 p = origin;
        int segs = 7;
        float step = radius * 0.85f / segs;
        for (int i = 0; i < segs * grow; i++) {
            ang += (VRand(seed, 100 + k * 16 + i) - 0.5f) * 0.9f;
            Vector2 q = p + Vector2{std::cos(ang) * step, std::sin(ang) * step};
            float th = 2.2f * (1 - (float)i / segs) + 0.5f;
            DrawLineEx(ToScreen(p), ToScreen(q), th, Fade(c, a));
            DrawLineEx(ToScreen(p) + Vector2{0, -0.6f}, ToScreen(q) + Vector2{0, -0.6f}, std::max(0.4f, th * 0.35f), Fade(Color{255, 200, 120, 255}, 0.35f * a * (1 - t)));
            p = q;
        }
    }
}

void Battle::DrawFxTop(const Fx& fx) const {
    float t = fx.life / fx.maxLife;
    Vector2 sp = ToScreen(fx.pos, fx.z);
    switch (fx.kind) {
        case FxKind::Explosion: VExplosion(sp, fx.radius, t, (Impact)fx.sub, fx.seed); break;
        case FxKind::Hit: VImpactStar(sp, fx.radius, t, fx.col, fx.seed); break;
        case FxKind::Lash: {   // a whiffed tether: the line whips out, curls at the tip, and reels back in
            float reach = t < 0.45f ? EaseOutQ(t / 0.45f) : 1.0f - EaseInQ((t - 0.45f) / 0.55f);
            Vector2 a = ToScreen(fx.pos, 10);
            Vector2 dir = SafeNorm(fx.dir, {1, 0});
            Vector2 nrm{-dir.y, dir.x};
            int n = 14;
            Vector2 prev = a;
            Color dk = ColorBrightness(fx.col, -0.5f);
            for (int i = 1; i <= n; i++) {
                float u = i / (float)n;
                float curl = std::sin(u * PI_F * 2 + t * 12) * 6 * u * (1 - reach * 0.6f);
                Vector2 w = fx.pos + dir * (fx.len * reach * u) + nrm * curl;
                Vector2 p = ToScreen(w, 10 - u * 3 * (1 - reach));
                DrawLineEx(prev, p, 3.4f - u * 1.6f, dk);
                DrawLineEx(prev, p, 2.0f - u * 1.0f, fx.col);
                prev = p;
            }
            if (t > 0.35f && t < 0.6f) {   // the crack of the whip at full stretch
                VAdd();
                VGlow(prev, 9, fx.col, 0.7f * (1 - std::fabs(t - 0.45f) / 0.15f));
                VNorm();
                for (int k = 0; k < 4; k++) {
                    float ang = k * PI_F / 2 + fx.seed % 7;
                    DrawLineEx(prev, prev + Vector2{std::cos(ang), std::sin(ang)} * 5.0f, 1.0f, Fade(WHITE, 0.8f));
                }
            }
        } break;
        case FxKind::Snap: {   // a tether breaking: two frayed ends recoil apart with a spark at the break
            Vector2 dir = SafeNorm(fx.dir, {1, 0});
            Vector2 nrm{-dir.y, dir.x};
            float half = fx.len * 0.5f;
            float rec = EaseOutQ(t) * std::min(half, 40.0f);
            float a = 1 - t;
            for (int side = -1; side <= 1; side += 2) {
                Vector2 tip = fx.pos + dir * (side * (4 + rec * 0.5f));
                Vector2 far = fx.pos + dir * (side * std::max(6.0f, half - rec * 0.3f));
                Vector2 prev = ToScreen(far, 10);
                for (int i = 1; i <= 8; i++) {
                    float u = i / 8.0f;
                    Vector2 w = far + (tip - far) * u + nrm * (std::sin(u * 10 + t * 20) * 4 * u * side);
                    Vector2 p = ToScreen(w, 10 - u * 6 * t);
                    DrawLineEx(prev, p, 2.2f, Fade(fx.col, a));
                    prev = p;
                }
                for (int k = 0; k < 3; k++)   // frayed strands
                    DrawLineEx(prev, prev + Vector2{dir.x * side * 3 + (k - 1) * 2.0f, (k - 1) * 2.0f}, 0.8f, Fade(fx.col, a));
            }
            VAdd();
            VGlow(sp, 12 * (1 - t) + 4, fx.col, 0.8f * a);
            if (t < 0.3f) VGlow(sp, 6, WHITE, 1 - t / 0.3f);
            VNorm();
        } break;
        case FxKind::Strike: {
            Vector2 ground = ToScreen(fx.pos, 0);
            uint32_t fr = (uint32_t)(fx.life * 40);
            Vector2 top{ground.x + (VRand(fx.seed, 1) - 0.5f) * 60, (float)(FLOOR_Y - STANDS_H - 20)};
            float a = t < 0.6f ? 1.0f : 1.0f - (t - 0.6f) / 0.4f;
            VAdd();
            DrawRectangleGradientV((int)ground.x - 6, (int)top.y, 12, (int)(ground.y - top.y), Fade(fx.col, 0.0f), Fade(fx.col, 0.35f * a));
            if (fr % 3 != 2 || t < 0.3f) {
                VBolt(top, ground, VHash(fx.seed, fr / 2), std::max(2.0f, fx.len), fx.col, a, 5);
                if (t < 0.5f) VBolt(top + Vector2{8, 0}, ground, VHash(fx.seed + 5, fr), 1.2f, WHITE, a * 0.7f, 2);
            }
            VGlow(ground, fx.radius * 1.4f, fx.col, 0.8f * a);
            VGlow(ground, fx.radius * 0.6f, WHITE, a);
            VNorm();
        } break;
        case FxKind::Radial: {
            uint32_t fr = (uint32_t)(fx.life * 30);
            int n = std::max(3, (int)fx.len);
            float a = 1 - t;
            for (int k = 0; k < n; k++) {
                float ang = (k + VRand(fx.seed + fr, k) * 0.5f) * 2 * PI_F / n;
                float R = fx.radius * (0.4f + 0.6f * std::min(1.0f, t * 3)) * (0.75f + VRand(fx.seed + fr, 20 + k) * 0.4f);
                Vector2 e = ToScreen(fx.pos + Vector2{std::cos(ang) * R, std::sin(ang) * R}, fx.z);
                VBolt(sp, e, VHash(fx.seed, fr * 13 + k), fx.radius > 25 ? 1.4f : 0.7f, fx.col, a, fx.radius > 25 ? 1 : 0);
            }
            VGlow(sp, fx.radius * 0.6f, fx.col, 0.6f * a);
            VNorm();
        } break;
        case FxKind::Flash: {
            float a = (1 - t) * (fx.radius >= 60 ? 0.28f : 0.18f) * (Cfg().fxLevel == 0 ? 1.0f : Cfg().fxLevel == 1 ? 0.35f : 0.0f);
            VAdd();
            DrawRectangle(0, 0, VW, FLOOR_BOTTOM + 4, Fade(fx.col, a));
            VNorm();
        } break;
        case FxKind::Pillar: {
            float a = t < 0.2f ? t / 0.2f : 1 - (t - 0.2f) / 0.8f;
            Vector2 g = ToScreen(fx.pos, 0);
            VAdd();
            float w = fx.radius * (0.6f + 0.4f * a);
            float top = fx.radius < 14 ? g.y - 46 : 0;   // small ones: an aura column around a Pokemon
            DrawRectangleGradientV((int)(g.x - w), (int)top, (int)(w * 2), (int)(g.y - top), Fade(fx.col, 0), Fade(fx.col, 0.55f * a));
            DrawRectangleGradientV((int)(g.x - w * 0.35f), (int)top, (int)(w * 0.7f), (int)(g.y - top), Fade(WHITE, 0), Fade(WHITE, 0.7f * a));
            VGlow(g, w * 2, fx.col, a);
            if (fx.radius < 14) VRingE(g, w * 1.3f, w * 0.55f, 1.0f, Fade(fx.col, a), 24);
            VNorm();
        } break;
        default: break;
    }
}

void Battle::DrawParticles() const {
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 1) VAdd();
        for (const auto& p : parts) {
            bool add = PKAdditive(p.kind);
            Vector2 sp = ToScreen(p.pos, p.z);
            Vector2 sv{p.vel.x, p.vel.y * DEPTH - p.vz};
            float lf = p.life / std::max(0.01f, p.maxLife);
            if (pass == 0 && add) VParticleBody(p.kind, sp, sv, p.size, p.rot, lf, p.col, time);
            if (add != (pass == 1)) continue;
            VParticle(p.kind, sp, sv, p.size, p.rot, lf, p.col, time);
        }
        if (pass == 1) VNorm();
    }
}

// ---------------------------------------------------------------- Showcase (screenshots)

bool gVfxDemo = false;
float gVfxDemoGap = 210;

void Battle::DebugCast(int s, const char* move) {
    const MoveDef* m = FindMove(move);
    if (!m || s >= N()) return;
    Fighter& f = sides[s].Cur();
    f.moves[0] = *m;
    if (f.nMoves < 1) f.nMoves = 1;
    f.cd[0] = 0;
    f.uses[0] = 0;
    f.pendingMove = -1;
    f.hitstun = 0;
    f.rechargeT = 0;
    int os = AnyFoe(s);
    if (os >= 0 && m->sleepOnly) { sides[os].Cur().st.major = Status::Sleep; sides[os].Cur().st.majorT = 20; }   // Dream Eater's demo
    Vector2 aim = os >= 0 ? SafeNorm(sides[os].Cur().pos - f.pos) : Vector2{1, 0};
    TryBeginMove(s, 0, aim, true);
}

void Battle::DebugHold() {
    for (int s = 0; s < N(); s++) {
        Fighter& f = sides[s].Cur();
        if (f.state != FState::Active) continue;
        f.hp = (float)f.maxHp;
        f.knock = {0, 0};
        if (s == 0) { f.st = StatusState(); f.hitstun = 0; f.rechargeT = 0; }
        if (!f.dashing && !f.burrowed && f.heldT <= 0 && f.rollT <= 0) f.pos = Vector2{s == 0 ? 295.0f - gVfxDemoGap / 2 : 295.0f + gVfxDemoGap / 2, 250.0f};
        f.aim = SafeNorm(Vector2{s == 0 ? 1.0f : -1.0f, 0});
    }
    introT = 0;
    sinceDamage = 0;
    suddenDeath = false;
    lastHpSum = -1;
}

// ---------------------------------------------------------------- Weather on the floor, day and night

void Battle::DrawWeatherFloor() const {
    if (weather == Weather::None) return;
    const float x0 = ARENA_SX + gShake.x, y0 = FLOOR_Y + gShake.y, w = ARENA_W, h = ARENA_H * DEPTH;
    float in = std::min(1.0f, (weatherT > 1000 ? time : (time - 0)) * 0.5f + 0.3f);
    if (weatherT < 1.0f && weatherT < 1000) in *= std::max(0.0f, weatherT);
    switch (weather) {
        case Weather::Rain: {
            DrawRectangle((int)x0, (int)y0, (int)w, (int)h, Fade(Color{20, 40, 80, 255}, 0.20f * in));
            for (int k = 0; k < 26; k++) {   // ripples from raindrops
                float ph = time * 1.7f + VRand(81, k);
                uint32_t cyc = (uint32_t)ph;
                float u = ph - cyc;
                float px = x0 + VRand(82 + cyc, k) * w, py = y0 + VRand(97 + cyc, k) * h;
                float r = 1 + u * 7;
                VRingE(Vector2{px, py}, r, r * 0.4f, 0.7f, Fade(Color{210, 230, 255, 255}, 0.55f * (1 - u) * in), 14);
            }
        } break;
        case Weather::Snow: {
            // A snowy day starts with the field already white; a Snowscape builds up over time.
            // Keep the canvas opaque: plain alpha blending would lower its alpha and the white
            // would come out grey once the canvas is put on screen.
            rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            float cover = (weatherT > 1000 ? std::min(0.46f, 0.36f + time * 0.002f) : std::min(0.34f, 0.10f + time * 0.004f)) * in;
            DrawRectangle((int)x0, (int)y0, (int)w, (int)h, Fade(Color{232, 240, 255, 255}, cover));
            for (int k = 0; k < 16; k++) {   // drifts
                float px = x0 + 20 + VRand(61, k) * (w - 40), py = y0 + 10 + VRand(62, k) * (h - 20);
                float r = 16 + VRand(63, k) * 26;
                DrawEllipse((int)px, (int)py, r, r * 0.42f, Fade(WHITE, cover * 0.9f));
                DrawEllipse((int)px, (int)(py - r * 0.06f), r * 0.65f, r * 0.24f, Fade(WHITE, cover * 0.9f));
                DrawEllipse((int)px, (int)(py + r * 0.3f), r * 0.8f, r * 0.12f, Fade(Color{150, 170, 210, 255}, cover * 0.35f));
            }
            DrawRectangleGradientV((int)x0, (int)y0, (int)w, 14, Fade(WHITE, cover * 1.4f), Fade(WHITE, 0));   // banked against the wall
            EndBlendMode();
        } break;
        case Weather::Sun: {
            VAdd();
            DrawRectangle((int)x0, (int)y0, (int)w, (int)h, Fade(Color{255, 190, 110, 255}, 0.07f * in));
            for (int k = 0; k < 5; k++) {   // glare
                float px = x0 + (k + 0.5f) * w / 5 + std::sin(time * 0.6f + k) * 12, py = y0 + h * (0.3f + 0.4f * VRand(71, k));
                VGlow(Vector2{px, py}, 60, Color{255, 220, 150, 255}, 0.10f * in);
            }
            VNorm();
        } break;
        case Weather::Sand:
            DrawRectangle((int)x0, (int)y0, (int)w, (int)h, Fade(Color{200, 160, 100, 255}, 0.15f * in));
            break;
        default: break;
    }
}

// Real puddles and ice on the field (Battle::field.puddles): they slow, shock and slide.
namespace {
// A filled wobbly blob on the floor (world radius r around screen point c).
void FillBlob(Vector2 c, float r, uint32_t seed, Color col, float dx = 0, float dy = 0) {
    const int N = 22;
    Vector2 pts[N];
    for (int k = 0; k < N; k++) {
        float a = k * 6.2831853f / N, w = r * Wobble(seed, a);
        pts[k] = {c.x + dx + std::cos(a) * w, c.y + dy + std::sin(a) * w * DEPTH};
    }
    for (int k = 0; k < N; k++) DrawTriangle({c.x + dx, c.y + dy}, pts[(k + 1) % N], pts[k], col);
}
void BlobRing(Vector2 c, float r, uint32_t seed, Color col) {
    const int N = 26;
    for (int k = 0; k < N; k++) {
        float a0 = k * 6.2831853f / N, a1 = (k + 1) * 6.2831853f / N;
        float w0 = r * Wobble(seed, a0), w1 = r * Wobble(seed, a1);
        DrawLineEx({c.x + std::cos(a0) * w0, c.y + std::sin(a0) * w0 * DEPTH}, {c.x + std::cos(a1) * w1, c.y + std::sin(a1) * w1 * DEPTH}, 1, col);
    }
}
} // namespace

void Battle::DrawPuddles() const {
    for (size_t i = 0; i < field.puddles.size(); i++) {
        const Puddle& pd = field.puddles[i];
        Vector2 c = ToScreen(pd.pos);
        float rx = pd.r, ry = pd.r * DEPTH;
        uint32_t sd = pd.seed ? pd.seed : 77u + (uint32_t)i;
        if (pd.ice > 0) {
            float a = std::min(1.0f, pd.ice * 2.0f);
            FillBlob(c, rx + 1, sd, Fade(Color{120, 170, 220, 255}, 0.55f * a));
            FillBlob(c, rx, sd, Fade(Color{205, 235, 255, 255}, 0.85f * a));
            DrawEllipse((int)(c.x - rx * 0.2f), (int)(c.y - ry * 0.25f), rx * 0.55f, ry * 0.35f, Fade(WHITE, 0.6f * a));
            for (int k = 0; k < 3 + (int)(sd % 3); k++) {   // scratches in the ice
                float u = VRand(sd, k) * 2 - 1, v = VRand(sd + 1, k) * 2 - 1;
                Vector2 p{c.x + u * rx * 0.6f, c.y + v * ry * 0.6f};
                DrawLineEx(p, {p.x + rx * (0.15f + VRand(sd + 2, k) * 0.25f), p.y - ry * 0.15f}, 1, Fade(Color{150, 200, 240, 255}, 0.8f * a));
            }
            float tw = std::fmod(time * 0.7f + i * 0.37f, 1.0f);   // a glint sweeping across
            DrawRectangle((int)(c.x - rx + tw * rx * 2), (int)(c.y - 1), 2, 2, Fade(WHITE, a * std::sin(tw * 3.14159f)));
            if (pd.ice < 2 && ((int)(time * 8) % 2)) BlobRing(c, rx, sd, Fade(WHITE, 0.6f));
            continue;
        }
        // Water: a dark edge, the body, the sky's reflection drifting a little, and ripples.
        FillBlob(c, rx + 1.5f, sd, Fade(Color{30, 50, 90, 255}, 0.35f));
        FillBlob(c, rx, sd, Fade(Color{70, 110, 170, 255}, 0.55f));
        float sway = std::sin(time * 0.9f + i) * rx * 0.08f;
        DrawEllipse((int)(c.x - rx * 0.25f + sway), (int)(c.y - ry * 0.2f), rx * 0.45f, ry * 0.26f, Fade(Color{190, 215, 255, 255}, 0.3f));
        DrawEllipse((int)(c.x + rx * 0.3f - sway), (int)(c.y + ry * 0.25f), rx * 0.2f, ry * 0.12f, Fade(Color{190, 215, 255, 255}, 0.18f));
        int drops = weather == Weather::Rain ? 3 : 1;
        float speed = weather == Weather::Rain ? 1.9f : 0.45f;   // a still puddle still ripples now and then (wind, drips)
        for (int k = 0; k < drops; k++) {
            float ph = time * speed + VRand(sd + 600, k);
            uint32_t cyc = (uint32_t)ph;
            float u = ph - cyc;
            float px = c.x + (VRand(700 + cyc, k + (uint32_t)i * 3) * 2 - 1) * rx * 0.6f, py = c.y + (VRand(800 + cyc, k + (uint32_t)i * 3) * 2 - 1) * ry * 0.6f;
            float rr = 1 + u * (weather == Weather::Rain ? 5 : rx * 0.5f);
            VRingE({px, py}, rr, rr * 0.45f, 0.7f, Fade(Color{220, 235, 255, 255}, 0.7f * (1 - u)), 14);
            if (weather != Weather::Rain && u > 0.2f) VRingE({px, py}, rr * 0.6f, rr * 0.27f, 0.6f, Fade(Color{220, 235, 255, 255}, 0.4f * (1 - u)), 12);
        }
        if (pd.shockT > 0) {   // electrified: crackling light over the water
            float a = pd.shockT / 0.45f;
            BeginBlendMode(BLEND_ADDITIVE);
            FillBlob(c, rx + 3, sd, Fade(Color{250, 230, 90, 255}, 0.45f * a));
            EndBlendMode();
            for (int k = 0; k < 6; k++) {
                float a0 = VRand(900 + (uint32_t)(time * 30), k + i * 7) * 6.2832f, a1 = a0 + 1.0f + VRand(950 + (uint32_t)(time * 30), k) * 2.0f;
                Vector2 p0{c.x + std::cos(a0) * rx * 0.85f, c.y + std::sin(a0) * ry * 0.85f};
                Vector2 p1{c.x + std::cos(a1) * rx * 0.5f, c.y + std::sin(a1) * ry * 0.5f};
                Vector2 mid{(p0.x + p1.x) / 2 + (VRand(990, k + i) - 0.5f) * 6, (p0.y + p1.y) / 2 + (VRand(991, k + i) - 0.5f) * 4};
                DrawLineEx(p0, mid, 1, Fade(Color{255, 250, 180, 255}, a));
                DrawLineEx(mid, p1, 1, Fade(Color{255, 250, 180, 255}, a));
            }
        }
    }
}

void Battle::DrawTimeOfDay() const {
    if (setup.night) {
        DrawRectangleGradientV(0, 0, VW, FLOOR_BOTTOM + 8, Fade(Color{6, 10, 40, 255}, 0.52f), Fade(Color{10, 16, 50, 255}, 0.40f));
        DrawRectangle(0, FLOOR_BOTTOM + 8, VW, VH - FLOOR_BOTTOM - 8, Fade(Color{8, 14, 46, 255}, 0.42f));
        // darker toward the edges, where the floodlights don't reach
        DrawRectangleGradientH(0, 0, 90, FLOOR_BOTTOM + 8, Fade(Color{4, 6, 24, 255}, 0.35f), Fade(Color{4, 6, 24, 255}, 0.0f));
        DrawRectangleGradientH(VW - 90, 0, 90, FLOOR_BOTTOM + 8, Fade(Color{4, 6, 24, 255}, 0.0f), Fade(Color{4, 6, 24, 255}, 0.35f));
        DrawRectangleGradientV(0, FLOOR_BOTTOM - 50, VW, 58, Fade(Color{4, 6, 24, 255}, 0.0f), Fade(Color{4, 6, 24, 255}, 0.3f));
        VAdd();
        for (int i = 0; i < 26; i++) {   // stars twinkling over the scenery
            float x = VRand(700, i) * VW, y = 2 + VRand(701, i) * (FLOOR_Y - 8);
            float tw = 0.5f + 0.5f * std::sin(time * (1.5f + VRand(702, i) * 2) + i);
            DrawRectangleV({x, y}, {0.7f, 0.7f}, Fade(Color{220, 230, 255, 255}, 0.7f * tw));
        }
        // Floodlights on the field, and a soft light under each battler so they stay readable.
        VGlow(ToScreen({ARENA_W * 0.3f, ARENA_H * 0.5f}), 170, Color{255, 230, 180, 255}, 0.10f);
        VGlow(ToScreen({ARENA_W * 0.7f, ARENA_H * 0.5f}), 170, Color{255, 230, 180, 255}, 0.10f);
        for (int s = 0; s < N(); s++) {
            const Fighter& f = sides[s].Cur();
            if (f.state == FState::Benched || f.state == FState::Fainted) continue;
            VGlow(ToScreen(f.pos, 8), 26, Color{200, 210, 255, 255}, 0.16f);
        }
        VNorm();
    } else if (setup.daylight) {
        VAdd();
        DrawRectangle(0, 0, VW, VH, Fade(Color{255, 244, 215, 255}, 0.05f));
        VNorm();
    }
}

// ---------------------------------------------------------------- Weather in the air

void Battle::DrawWeatherSky() const {
    if (weather == Weather::None) return;
    float fade = std::min(1.0f, weatherT);
    const float top = 0, bottom = (float)FLOOR_BOTTOM + 6;
    const float H = bottom - top;
    switch (weather) {
        case Weather::Rain: {
            DrawRectangleGradientV(0, 0, VW, (int)bottom, Fade(Color{30, 40, 70, 255}, (setup.night ? 0.14f : 0.24f) * fade), Fade(Color{40, 60, 100, 255}, (setup.night ? 0.06f : 0.12f) * fade));
            const float wind = -0.22f;
            for (int layer = 0; layer < 3; layer++) {   // far, middle and near rain
                int n = layer == 0 ? 120 : layer == 1 ? 90 : 45;
                float speed = layer == 0 ? 300.0f : layer == 1 ? 420.0f : 560.0f;
                float len = layer == 0 ? 5.0f : layer == 1 ? 8.0f : 13.0f;
                float w = layer == 0 ? 0.6f : layer == 1 ? 0.9f : 1.3f;
                float al = layer == 0 ? 0.32f : layer == 1 ? 0.5f : 0.68f;
                for (int i = 0; i < n; i++) {
                    float h1 = VRand(300 + layer, i), h2 = VRand(310 + layer, i);
                    float y = top + std::fmod(h2 * H + time * speed, H);
                    float x = std::fmod(h1 * (VW + 80) + (y - top) * wind * -1.0f + 4000.0f, (float)(VW + 80)) - 40;
                    Vector2 a{x, y}, b{x + wind * len, y + len};
                    DrawLineEx(a, b, w, Fade(Color{190, 215, 255, 255}, al * fade));
                }
            }
            for (int k = 0; k < 40; k++) {   // splashes on the floor
                float ph = time * 2.6f + VRand(91, k);
                uint32_t cyc = (uint32_t)ph;
                float u = ph - cyc;
                if (u > 0.35f) continue;
                Vector2 p = ToScreen({VRand(92 + cyc, k) * ARENA_W, VRand(93 + cyc, k) * ARENA_H});
                float v = u / 0.35f;
                for (int j = -1; j <= 1; j++) {
                    Vector2 q{p.x + j * (1.5f + v * 3), p.y - std::sin(v * 3.14159f) * (2.5f - std::abs(j) * 0.8f)};
                    DrawRectangleV(q, Vector2{0.8f, 0.8f}, Fade(Color{220, 235, 255, 255}, 0.8f * (1 - v) * fade));
                }
            }
            float storm = std::fmod(time, 11.0f);   // now and then, lightning far away
            if (storm > 10.5f) {
                float f = storm < 10.6f ? 1.0f : storm < 10.68f ? 0.2f : storm < 10.75f ? 0.8f : 1.0f - (storm - 10.75f) / 0.25f;
                VAdd();
                DrawRectangle(0, 0, VW, (int)bottom, Fade(Color{200, 210, 255, 255}, 0.16f * f * fade * (Cfg().fxLevel == 0 ? 1.0f : 0.3f)));
                VNorm();
            }
        } break;
        case Weather::Snow: {
            DrawRectangleGradientV(0, 0, VW, (int)bottom, Fade(Color{200, 215, 240, 255}, 0.16f * fade), Fade(Color{220, 230, 250, 255}, 0.05f * fade));
            for (int layer = 0; layer < 3; layer++) {
                int n = layer == 0 ? 110 : layer == 1 ? 70 : 26;
                float speed = layer == 0 ? 18.0f : layer == 1 ? 30.0f : 46.0f;
                float sz = layer == 0 ? 0.8f : layer == 1 ? 1.3f : 2.2f;
                float al = layer == 0 ? 0.55f : layer == 1 ? 0.8f : 0.7f;
                for (int i = 0; i < n; i++) {
                    float h1 = VRand(400 + layer, i), h2 = VRand(410 + layer, i), h3 = VRand(420 + layer, i);
                    float y = top + std::fmod(h2 * H + time * speed * (0.8f + h3 * 0.4f), H);
                    float x = std::fmod(h1 * VW + std::sin(time * (0.8f + h3) + i) * (6 + layer * 4) + time * 8 + VW * 4, (float)VW);
                    if (layer == 2) { VSoft({x, y}, sz * 1.6f, WHITE, al * fade); VNorm(); }
                    else DrawCircleV({x, y}, sz, Fade(WHITE, al * fade));
                }
            }
        } break;
        case Weather::Sand: {
            DrawRectangleGradientV(0, 0, VW, (int)bottom, Fade(Color{190, 140, 80, 255}, 0.36f * fade), Fade(Color{200, 160, 100, 255}, 0.22f * fade));
            for (int k = 0; k < 14; k++) {   // dust banks rolling across
                float h1 = VRand(500, k), h2 = VRand(501, k);
                float x = std::fmod(h1 * (VW + 300) + time * (60 + h2 * 50), (float)(VW + 300)) - 150;
                float y = top + 20 + h2 * (H - 30);
                VSoft({x, y}, 70 + h1 * 60, Color{210, 170, 110, 255}, 0.26f * fade);
            }
            VNorm();
            for (int i = 0; i < 220; i++) {   // grains whipping past
                float h1 = VRand(510, i), h2 = VRand(511, i), h3 = VRand(512, i);
                float spd = 240 + h3 * 260;
                float x = std::fmod(h1 * (VW + 60) + time * spd, (float)(VW + 60)) - 30;
                float y = top + std::fmod(h2 * H + std::sin(time * 3 + i) * 4 + time * 12, H);
                float l = 2 + h3 * 6;
                DrawLineEx({x, y}, {x - l * 1.4f, y - l * 0.1f}, 0.8f + h3 * 0.8f, Fade(Color{240, 210, 160, 255}, (0.45f + h3 * 0.45f) * fade));
            }
        } break;
        case Weather::Sun: {
            VAdd();
            DrawRectangleGradientV(0, 0, VW, (int)bottom, Fade(Color{255, 210, 140, 255}, 0.12f * fade), Fade(Color{255, 180, 90, 255}, 0.04f * fade));
            Vector2 sun{VW * 0.12f, -40};
            for (int k = 0; k < 7; k++) {   // shafts of light
                float ang = 0.45f + k * 0.13f + std::sin(time * 0.3f + k) * 0.02f;
                float w = 0.025f + VRand(601, k) * 0.03f;
                float len = 700;
                Vector2 a{sun.x + std::cos(ang - w) * len, sun.y + std::sin(ang - w) * len}, b{sun.x + std::cos(ang + w) * len, sun.y + std::sin(ang + w) * len};
                float al = (0.05f + 0.04f * std::sin(time * 0.8f + k * 1.7f)) * fade;
                DrawTriangle(sun, b, a, Fade(Color{255, 235, 180, 255}, al));
                DrawTriangle(sun, a, b, Fade(Color{255, 235, 180, 255}, al));
            }
            VGlow({sun.x + 30, 10}, 90, Color{255, 220, 150, 255}, 0.35f * fade);
            for (int k = 0; k < 4; k++) {   // a faint lens flare
                float u = 0.25f + k * 0.2f;
                Vector2 p{sun.x + (VW * 0.7f - sun.x) * u, 10 + (FLOOR_Y + 120 - 10) * u};
                VGlow(p, 8 + k * 5, k % 2 ? Color{255, 200, 120, 255} : Color{180, 220, 255, 255}, 0.12f * fade);
            }
            for (int i = 0; i < 40; i++) {   // motes in the light
                float h1 = VRand(610, i), h2 = VRand(611, i);
                float x = std::fmod(h1 * VW + std::sin(time * 0.5f + i) * 10 + time * 4, (float)VW);
                float y = FLOOR_Y + std::fmod(h2 * (H - FLOOR_Y) - time * 6 + 400, H - FLOOR_Y);
                float tw = 0.5f + 0.5f * std::sin(time * 3 + i * 1.3f);
                VGlow({x, y}, 1.6f, Color{255, 240, 200, 255}, 0.5f * tw * fade);
            }
            VNorm();
            for (int k = 0; k < 5; k++) {   // heat shimmer over the floor
                float y = FLOOR_Y + 30 + k * 50 + std::fmod(time * 10, 50.0f);
                if (y > FLOOR_BOTTOM - 4) continue;
                Vector2 prev{0, y};
                for (int x = 8; x <= VW; x += 8) {
                    Vector2 q{(float)x, y + std::sin(x * 0.08f + time * 4 + k) * 1.2f};
                    DrawLineEx(prev, q, 0.6f, Fade(WHITE, 0.06f * fade));
                    prev = q;
                }
            }
        } break;
        default: break;
    }
}
