// Hoenn arenas: Norman's Balance Gym, Tate & Liza's Twin Moons, Juan's cracked ice over Sootopolis's
// crater lake, Wallace's Waterfall Hall, Steven's Meteor Falls, and the crumbling Sky Pillar.
// (The legendary lairs' falling strikes live with the other lairs in battle_field.cpp.)
// Everything that changes the battle is battle state (saved for online play); Draw* is visual only.
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
bool HasT(const SpeciesDef* sp, Type t) { return sp->type1 == t || sp->type2 == t; }
Color Cc(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

const Color kSun = {255, 200, 90, 255};
const Color kMoon = {150, 170, 255, 255};
const Color kRain = {120, 180, 255, 255};
const Color kFoam = {225, 240, 255, 255};
const Color kStar = {255, 220, 120, 255};
const Color kRoom[4] = {{120, 200, 255, 255}, {190, 190, 200, 255}, {130, 220, 130, 255}, {240, 120, 90, 255}};
const char* kRoomName[4] = {"SPEED", "DEFENSE", "RECOVERY", "STRENGTH"};

bool OnFloor(const Fighter& f) { return f.state == FState::Active && !f.burrowed && f.jumpT <= 0 && f.heldZ <= 0 && !IsAirborne(*f.sp); }

} // namespace

// ---------------------------------------------------------------- Update

void Battle::UpdateHoenn(float dt) {
    bool live = winner < 0 && introT <= 0;
    switch (field.kind) {
        case ArenaKind::Balance:
            if (field.scaleT > 0) {
                field.scaleT -= dt;
                if (field.scaleT <= 0) { field.scaleOwner = -1; AddText(field.center.pos, "The scale levels out.", UI_DIM, 30); }
            }
            break;

        case ArenaKind::TwinMoons: {
            if (field.jamT > 0) { field.jamT -= dt; break; }   // a Dark move jammed the dial: no eclipses for now
            if (!live) break;
            field.phaseT -= dt;
            if (field.warnT <= 0 && field.phaseT <= 1.5f) {
                field.warnT = 1.5f;
                AddText({ARENA_W / 2, ARENA_H * 0.1f}, "An eclipse is coming!", kMoon, 20);
                PlaySfx(Sfx::Status);
            }
            if (field.warnT > 0) field.warnT -= dt;
            if (field.phaseT <= 0) {
                field.phase ^= 1;
                field.phaseT = 12.0f;
                field.warnT = 0;
                AddText({ARENA_W / 2, ARENA_H * 0.1f}, "Eclipse! The sun and the moon trade places!", kSun, 20);
                AddFx(FxKind::Flash, {ARENA_W / 2, ARENA_H / 2}, 0.25f, kMoon, 200, 30);
                Shake(1);
                PlaySfx(Sfx::Buff);
            }
        } break;

        case ArenaKind::Sootopolis:
        case ArenaKind::SkyPillar: {
            bool sky = field.kind == ArenaKind::SkyPillar;
            if (field.rainT > 0) { field.rainT -= dt; if (field.rainT <= 0) { field.rainOwner = -1; AddText(field.center.pos, "The rain from the font stopped.", UI_DIM, 30); } }
            for (size_t i = 0; i < field.tiles.size(); i++) {
                Tile& t = field.tiles[i];
                bool someone = false;
                for (int s = 0; s < N(); s++) {
                    const Fighter& f = sides[s].Cur();
                    if (OnFloor(f) && field.TileAt(f.pos) == (int)i) someone = true;
                }
                if (t.state == 2) {   // broken: it freezes over / settles back once it's clear
                    t.t -= dt;
                    if (t.t <= 0 && !someone) {
                        t.state = 0;
                        t.standT = 0;
                        Burst(t.pos, sky ? Cc(170, 180, 160) : Cc(210, 240, 255), 8, 30, 2);
                    }
                    continue;
                }
                if (!live) continue;
                if (someone) t.standT += dt;
                else t.standT = std::max(0.0f, t.standT - dt * 0.25f);
                if (t.standT < 2.0f) continue;
                t.standT = 0;
                t.state++;
                if (t.state == 1) {
                    Burst(t.pos, sky ? Cc(150, 140, 120) : Cc(220, 245, 255), 6, 30, 2);
                    PlaySfx(Sfx::Thud);
                    continue;
                }
                t.t = sky ? 12.0f : 15.0f;   // it gives way
                Boom(t.pos, 16, Impact::Debris, 2);
                for (int s = 0; s < N(); s++) {
                    Fighter& f = sides[s].Cur();
                    if (!OnFloor(f) || field.TileAt(f.pos) != (int)i) continue;
                    if (sky) {   // a fall: you climb back up beside the hole
                        DirectDamage(s, f.maxHp * 0.05f, Cc(200, 200, 220), AnyFoe(s));
                        Vector2 best = f.pos;
                        float bd = 1e9f;
                        for (int k = 0; k < 16; k++) {
                            float a = k * 2 * PI_F / 16;
                            Vector2 p = f.pos + Vector2{std::cos(a), std::sin(a)} * 44.0f;
                            if (p.x < 12 || p.x > ARENA_W - 12 || p.y < 12 || p.y > ARENA_H - 12 || field.Broken(p)) continue;
                            float d = Dist(p, {ARENA_W / 2, ARENA_H / 2});
                            if (d < bd) { bd = d; best = p; }
                        }
                        f.pos = best;
                        f.dashing = false;
                        f.knock = {0, 0};
                        AddText(f.pos, "The floor gave way!", Cc(220, 220, 240), 30);
                    } else if (!HasT(f.sp, Type::Water)) {
                        DirectDamage(s, f.maxHp * 0.03f, kRain, AnyFoe(s));
                        AddText(f.pos, "Splash! The ice broke!", kRain, 30);
                    } else {
                        AddText(f.pos, "Into the lake!", kRain, 26);
                    }
                }
                PlaySfx(sky ? Sfx::Thud : Sfx::Water);
            }
        } break;

        case ArenaKind::HoennChampion: {
            if (field.surgeT >= 0) {   // the waterfall's wave rolls across the floor
                field.surgeT += dt / 1.6f;
                Vector2 d = field.surgeDir;
                // where the front is: from one wall to the other along d
                float span = std::fabs(d.x) * ARENA_W + std::fabs(d.y) * ARENA_H;
                Vector2 start{d.x > 0 ? 0.0f : d.x < 0 ? ARENA_W : 0.0f, d.y > 0 ? 0.0f : d.y < 0 ? ARENA_H : 0.0f};
                float front = field.surgeT * span;
                for (int s = 0; s < N(); s++) {
                    Fighter& f = sides[s].Cur();
                    if (!OnFloor(f) || Held(s)) continue;
                    float along = Dot(f.pos - start, d);
                    if (along > front - 36 && along < front + 6) {
                        Vector2 np = f.pos + d * (170.0f * dt);
                        f.pos = {std::max(9.0f, std::min(ARENA_W - 9, np.x)), std::max(9.0f, std::min(ARENA_H - 9, np.y))};
                        if (SimRand(0, 3) == 0) Emit(PK::Drop, f.pos, Frand(4, 14), d * 60.0f, Frand(20, 40), 0.4f, 1.5f, kFoam, true);
                    }
                }
                if (field.surgeT >= 1) {
                    field.surgeT = -1;
                    if (weather == Weather::None) {
                        weather = Weather::Rain;
                        weatherT = 8.0f;
                        weatherTick = 1.0f;
                        AddText({ARENA_W / 2, ARENA_H * 0.2f}, "Spray from the falls - a rain shower!", kRain, 20);
                    }
                }
                break;
            }
            if (!live) break;
            field.phaseT -= dt;
            if (field.warnT <= 0 && field.phaseT <= 1.6f) {
                field.warnT = 1.6f;
                AddText({ARENA_W / 2, ARENA_H * 0.1f}, "The waterfall is surging!", kRain, 20);
                PlaySfx(Sfx::Status);
            }
            if (field.warnT > 0) field.warnT -= dt;
            if (field.phaseT <= 0) {
                field.phaseT = Frand(15.0f, 20.0f);
                field.warnT = 0;
                field.surgeT = 0;
                field.surgeDir = {0, 1};   // from the falls on the back wall, down across the floor
                Shake(2);
                PlaySfx(Sfx::Water);
            }
        } break;

        case ArenaKind::MeteorFalls: {
            if (!live) break;
            field.strikeT -= dt;
            if (field.strikeT > 0) break;
            field.strikeT = 6.0f * Frand(0.9f, 1.1f);
            int boss = -1;   // Jirachi's visit: lair rules, the meteors only fall on the challengers
            for (int s = 0; s < N(); s++) if (sides[s].teamId == 1 && sides[s].Cur().boss > 0) boss = s;
            std::vector<int> at;
            for (int s = 0; s < N(); s++) if (Targetable(s) && (boss < 0 || sides[s].teamId == 0)) at.push_back(s);
            for (int k = 0; k < 2 && !at.empty(); k++) {
                int ts = at[SimRand(0, (int)at.size() - 1)];
                Projectile p;
                p.hit = EnvSpec(Type::Rock, 55);
                p.hit.side = boss;
                p.hit.special = true;
                p.drop = true;
                Vector2 to = sides[ts].Cur().pos + Vector2{Frand(-26, 26), Frand(-18, 18)};
                to.x = std::max(6.0f, std::min(ARENA_W - 6, to.x));
                to.y = std::max(6.0f, std::min(ARENA_H - 4, to.y));
                p.target = p.pos = to;
                p.dur = 1.3f;
                p.t = -k * 0.35f;
                p.z = 110;
                p.radius = 16;
                p.spin = Frand(0, 6);
                projs.push_back(p);
            }
            AddText({ARENA_W / 2, ARENA_H * 0.1f}, "Meteor fragments are falling!", kStar, 20);
        } break;
        default: break;
    }
}

// ---------------------------------------------------------------- Standing in it

void Battle::HoennEffectsOn(int s, float dt) {
    Fighter& f = sides[s].Cur();
    if (f.state != FState::Active || f.burrowed || f.hp <= 0) return;
    const SpeciesDef* sp = f.sp;
    bool air = IsAirborne(*sp) || f.jumpT > 0;
    bool tick = (int)(time * 2) != (int)((time - dt) * 2);   // twice a second
    int team = sides[s].teamId;
    switch (field.kind) {
        case ArenaKind::Balance: {
            bool flip = field.scaleOwner >= 0 && field.scaleT > 0 && team != field.scaleOwner;
            int room = Field::RoomAt(f.pos);
            auto apply = [&](int r, float w) {
                switch (r) {
                    case 0: f.envSpeed *= flip ? 1.0f - 0.2f * w : 1.0f + 0.25f * w; break;
                    case 1: f.envDef *= flip ? 1.0f - 0.2f * w : 1.0f + 0.25f * w; break;
                    case 2:
                        if (flip) { if (tick) DirectDamage(s, f.maxHp * 0.004f * w, kRoom[2], AnyFoe(s)); }
                        else if (f.hp < f.maxHp) {
                            f.regenAcc += f.maxHp * 0.01f * w * dt;
                            if (f.regenAcc >= 1) { float a = std::floor(f.regenAcc); f.hp = std::min((float)f.maxHp, f.hp + a); f.regenAcc -= a; }
                        }
                        break;
                    default: f.envAtk *= flip ? 1.0f - 0.15f * w : 1.0f + 0.2f * w; break;
                }
            };
            apply(room, 1.0f);
            if (HasT(sp, Type::Normal)) { apply(room ^ 1, 0.5f); apply(room ^ 2, 0.5f); }   // Normal types share in the rooms next door
            if (SimRand(0, 9) == 0) Emit(PK::Glow, f.pos + Vector2{Frand(-7, 7), Frand(-3, 3)}, Frand(2, 12), {0, 0}, 14, 0.5f, 1.2f, flip ? Cc(200, 70, 70) : kRoom[room], false);
        } break;

        case ArenaKind::TwinMoons: {
            bool sun = field.SunAt(f.pos);
            bool psychic = HasT(sp, Type::Psychic);
            if (psychic) f.envAtk *= 1.15f;
            else if (!sun) f.envSpeed *= 0.9f;   // the moon's pull
            bool own = (sun && sp->name == "Solrock") || (!sun && sp->name == "Lunatone");
            if (own && f.hp < f.maxHp) {
                f.regenAcc += f.maxHp * 0.006f * dt;
                if (f.regenAcc >= 1) { float a = std::floor(f.regenAcc); f.hp = std::min((float)f.maxHp, f.hp + a); f.regenAcc -= a; }
                if (SimRand(0, 6) == 0) Emit(PK::Star, f.pos + Vector2{Frand(-8, 8), Frand(-3, 3)}, Frand(6, 20), {0, 0}, 16, 0.5f, 1.4f, sun ? kSun : kMoon);
            }
        } break;

        case ArenaKind::Sootopolis:
            if (!air && field.Broken(f.pos)) {
                if (HasT(sp, Type::Water)) { f.envSpeed = 1.35f; f.envAtk = 1.15f; }
                else f.envSpeed = 0.6f;
                if (f.moving && SimRand(0, 3) == 0) Emit(PK::Drop, f.pos + Vector2{Frand(-6, 6), Frand(-2, 2)}, 1, {0, 0}, Frand(10, 30), 0.35f, 1.0f, Cc(170, 210, 255), true);
            }
            break;

        case ArenaKind::HoennChampion:
            if (field.InSpot(f.pos) && HasT(sp, Type::Water)) f.envAtk = 1.3f;   // Wallace's showmanship
            if (!air && field.InZone(f.pos, Zone::Pool)) {
                f.envSpeed *= HasT(sp, Type::Water) ? 1.17f : 0.8f;
                if (f.moving && SimRand(0, 4) == 0) Emit(PK::Drop, f.pos + Vector2{Frand(-6, 6), Frand(-2, 2)}, 1, {0, 0}, Frand(10, 25), 0.3f, 1.0f, Cc(170, 210, 255), true);
            }
            break;

        case ArenaKind::MeteorFalls:
            if (HasT(sp, Type::Steel) || HasT(sp, Type::Rock))
                for (const Obstacle& o : field.obs)
                    if (o.kind == Obstacle::Pillar && o.alive && Dist(o.pos, f.pos) < o.radius + 40) { f.envDef *= 1.15f; break; }
            break;

        case ArenaKind::DesertRuins:   // the sandstorm wears down the challengers
            if (team == 0 && tick && !(HasT(sp, Type::Rock) || HasT(sp, Type::Ground) || HasT(sp, Type::Steel)) && !cloudNine)
                DirectDamage(s, f.maxHp * 0.0025f, Cc(210, 180, 120), AnyFoe(s));
            break;
        default: break;
    }
}

float Battle::FieldMoveMul(int s, Type t, bool special) const {
    if (s < 0 || s >= N()) return 1.0f;
    const Fighter& f = sides[s].Cur();
    int team = sides[s].teamId;
    float m = 1.0f;
    switch (field.kind) {
        case ArenaKind::TwinMoons: {
            bool sun = field.SunAt(f.pos);
            if (t == Type::Psychic || (sun && t == Type::Fire)) m *= 1.15f;
        } break;
        case ArenaKind::Sootopolis:
            if (field.rainT > 0 && field.rainOwner == team) m *= t == Type::Water ? 1.25f : t == Type::Fire ? 0.75f : 1.0f;
            break;
        case ArenaKind::MeteorFalls:
            if (field.glowT > 0 && field.glowOwner == team && special) m *= 1.2f;
            break;
        default: break;
    }
    return m * Gen45MoveMul(s, t, special);
}

// ---------------------------------------------------------------- CPU awareness

Vector2 Battle::HoennPull(int s) const {
    const Fighter& f = sides[s].Cur();
    const SpeciesDef* sp = f.sp;
    bool air = IsAirborne(*sp);
    Vector2 pull{0, 0};
    switch (field.kind) {
        case ArenaKind::Balance: {   // hurt: rest in the Recovery room; otherwise fight from Strength
            int want = f.hp < f.maxHp * 0.45f ? 2 : 3;
            bool flip = field.scaleOwner >= 0 && field.scaleT > 0 && sides[s].teamId != field.scaleOwner;
            if (flip) want = 0;   // the rooms are against you: Speed hurts least
            Vector2 c{(want & 1) ? ARENA_W * 0.75f : ARENA_W * 0.25f, (want & 2) ? ARENA_H * 0.75f : ARENA_H * 0.25f};
            if (Field::RoomAt(f.pos) != want) pull += SafeNorm(c - f.pos) * 0.5f;
        } break;
        case ArenaKind::TwinMoons:
            if (!HasT(sp, Type::Psychic) && !field.SunAt(f.pos)) pull.x += ((field.phase == 0) ? -1.0f : 1.0f) * 0.4f;   // out of the moon
            break;
        case ArenaKind::Sootopolis:
        case ArenaKind::SkyPillar: {
            if (air) break;
            bool water = field.kind == ArenaKind::Sootopolis && HasT(sp, Type::Water);
            int i = field.TileAt(f.pos);
            bool bad = i >= 0 && (field.tiles[i].state == 1 || (field.tiles[i].state == 2 && !water) || field.tiles[i].standT > 1.2f);
            if (water && i >= 0 && field.tiles[i].state == 2) break;   // happy swimming
            if (!bad) break;
            Vector2 best{0, 0};
            float bd = 1e9f;
            for (const Tile& t : field.tiles) {
                if (t.state != 0 || t.standT > 0.5f) continue;
                float d = Dist(t.pos, f.pos);
                if (d > 1 && d < bd) { bd = d; best = t.pos; }
            }
            if (bd < 1e8f) pull += SafeNorm(best - f.pos) * 1.1f;
        } break;
        case ArenaKind::HoennChampion:
            if (!air && !HasT(sp, Type::Water))
                for (const Zone& z : field.zones)
                    if (z.kind == Zone::Pool && z.Contains(f.pos, 12)) pull += SafeNorm(f.pos - z.pos, Vector2{0, 1}) * 1.0f;
            break;
        case ArenaKind::MeteorFalls:
            if (HasT(sp, Type::Steel) || HasT(sp, Type::Rock)) {
                const Obstacle* best = nullptr;
                float bd = 1e9f;
                for (const Obstacle& o : field.obs)
                    if (o.kind == Obstacle::Pillar && o.alive && Dist(o.pos, f.pos) < bd) { bd = Dist(o.pos, f.pos); best = &o; }
                if (best && bd > best->radius + 34 && bd < 220) pull += SafeNorm(best->pos - f.pos) * 0.5f;
            }
            break;
        default: break;
    }
    return pull;
}

// A move landed: Ice freezes Juan's lake over again.
void Battle::HoennFieldReact(Vector2 at, const HitSpec& h, float radius) {
    if (field.kind != ArenaKind::Sootopolis || h.type != Type::Ice) return;
    int n = 0;
    for (Tile& t : field.tiles)
        if (t.state == 2 && Dist(t.pos, at) < radius + 34) { t.state = 0; t.standT = 0; n++; Burst(t.pos, Cc(210, 240, 255), 8, 40, 2); }
    if (n) AddText(at, "The water froze over!", Cc(190, 235, 255), 30);
}

// ---------------------------------------------------------------- Centerpieces

void Battle::HoennCenterUpdate(float dt) {
    Centerpiece& c = field.center;
    switch (c.kind) {
        case Centerpiece::Scale: {   // the beam swings toward whoever it favours
            float want = field.scaleOwner == 0 ? -0.35f : field.scaleOwner == 1 ? 0.35f : 0.0f;
            c.spin += (want - c.spin) * std::min(1.0f, dt * 4);
        } break;
        case Centerpiece::Dial: {
            float want = field.phase ? PI_F : 0.0f;
            c.spin += std::remainder(want - c.spin, 2 * PI_F) * std::min(1.0f, dt * 3);
        } break;
        case Centerpiece::Font:
            if (field.rainT > 0 && SimRand(0, 2) == 0) Emit(PK::Drop, c.pos + Vector2{Frand(-6, 6), Frand(-3, 3)}, Frand(18, 30), {Frand(-20, 20), Frand(-10, 10)}, Frand(30, 60), 0.6f, 1.4f, kRain, true);
            break;
        case Centerpiece::Meteorite: {
            Obstacle* body = CenterBody();
            if (c.phase == 1) {   // cracked open and glowing
                field.glowT -= dt;
                if (SimRand(0, 2) == 0) Emit(PK::Star, c.pos + Vector2{Frand(-14, 14), Frand(-6, 6)}, Frand(8, 30), {0, 0}, Frand(10, 30), 0.6f, 1.6f, kStar);
                if (field.glowT <= 0) {
                    c.phase = 2;
                    c.t = 25.0f;
                    field.glowOwner = -1;
                    if (body) body->alive = false;
                    Boom(c.pos, 22, Impact::Debris, 6);
                    AddText(c.pos, "The meteorite crumbled.", UI_DIM, 30);
                }
            } else if (c.phase == 2) {   // rubble: it reforms
                c.t -= dt;
                if (c.t <= 0) {
                    bool clear = true;
                    for (int s = 0; s < N(); s++) if (Dist(sides[s].Cur().pos, c.pos) < c.radius + 12) clear = false;
                    if (clear) {
                        c.phase = 0;
                        if (body) body->alive = true;
                        Burst(c.pos, kStar, 16, 60, 10);
                        AddText(c.pos, "The meteorite glows whole again!", kStar, 30);
                    }
                }
            }
        } break;
        default: break;
    }
}

void Battle::HoennCenterHit(const HitSpec& h, Vector2 dir, bool physical) {
    Centerpiece& c = field.center;
    int team = h.side >= 0 && h.side < N() ? sides[h.side].teamId : -1;
    switch (c.kind) {
        case Centerpiece::Scale:
            if (team < 0) return;
            c.hitCd = 0.8f;
            if (field.scaleOwner != team) AddText(c.pos, "The scale tipped! The rooms turn on your foes!", UI_GOLD, 40);
            field.scaleOwner = team;
            field.scaleT = 10.0f;
            AddFx(FxKind::Flash, c.pos, 0.12f, UI_GOLD, 40, 40);
            PlaySfx(Sfx::Buff);
            break;
        case Centerpiece::Dial:
            c.hitCd = 1.0f;
            if (h.type == Type::Dark) {
                field.jamT = 10.0f;
                field.warnT = 0;
                field.phaseT = std::max(field.phaseT, 4.0f);
                AddText(c.pos, "The dial jammed!", Cc(170, 120, 200), 36);
                Boom(c.pos, 12, Impact::Debris, 20);
                PlaySfx(Sfx::Thud);
                break;
            }
            if (field.jamT > 0) { AddText(c.pos, "The dial won't turn...", UI_DIM, 28); break; }
            if (field.phaseT > 0.6f) {
                field.phaseT = 0.6f;
                field.warnT = 0.6f;
                AddText(c.pos, "The dial spins - an eclipse!", kMoon, 36);
                PlaySfx(Sfx::Shield);
            }
            break;
        case Centerpiece::Font:
            c.hitCd = 1.0f;
            if (h.type == Type::Grass || h.type == Type::Electric) {
                if (field.rainT > 0) AddText(c.pos, "The font dried up!", UI_DIM, 34);
                field.rainT = 0;
                field.rainOwner = -1;
                break;
            }
            if (team < 0) return;
            field.rainOwner = team;
            field.rainT = 12.0f;
            AddText(c.pos, "The font calls the rain!", kRain, 36);
            for (int i = 0; i < 14; i++) Emit(PK::Drop, c.pos, 20, Vector2{Frand(-80, 80), Frand(-40, 40)}, Frand(60, 110), 0.7f, 1.6f, kRain, true);
            PlaySfx(Sfx::Water);
            break;
        case Centerpiece::Crystal: {
            if (field.surgeT >= 0) return;
            c.hitCd = 2.0f;
            Vector2 d = SafeNorm(Vector2{dir.x, dir.y * 0.4f}, {1, 0});   // across the floor, away from the hitter
            d = std::fabs(d.x) >= std::fabs(d.y) ? Vector2{d.x > 0 ? 1.0f : -1.0f, 0} : Vector2{0, d.y > 0 ? 1.0f : -1.0f};
            field.surgeDir = d;
            field.surgeT = 0;
            field.phaseT = Frand(15.0f, 20.0f);
            field.warnT = 0;
            AddText(c.pos, "The crystal sets off the falls!", kRain, 40);
            AddFx(FxKind::Flash, c.pos, 0.15f, kFoam, 50, 40);
            Shake(2);
            PlaySfx(Sfx::Water);
        } break;
        case Centerpiece::Meteorite: {
            if (c.phase != 0 || team < 0) return;
            bool heavy = h.heavy || h.power >= 75 || (physical && h.power >= 55) ||
                         (h.power >= 50 && (h.type == Type::Rock || h.type == Type::Steel || h.type == Type::Fighting || h.type == Type::Ground));
            c.hitCd = 0.5f;
            if (!heavy) {
                if (SimRand(0, 2) == 0) AddText(c.pos, "The meteorite barely chipped...", UI_DIM, 30);
                Emit(PK::Debris, c.pos - dir * c.radius, 12, -dir * 40.0f, 30, 0.4f, 1.4f, Cc(110, 100, 120), true);
                return;
            }
            c.phase = 1;
            field.glowOwner = team;
            field.glowT = 10.0f;
            AddText(c.pos, "The meteorite cracked open! Its power surges!", kStar, 44);
            Boom(c.pos, 20, Impact::Debris, 10);
            AddFx(FxKind::Flash, c.pos, 0.2f, kStar, 60, 40);
            Shake(3);
            PlaySfx(Sfx::Buff);
        } break;
        default: break;
    }
}

// ---------------------------------------------------------------- Drawing (visual only)

void Battle::DrawHoennFloor() const {
    const float t = time;
    auto quad = [](float x0, float y0, float x1, float y1, Color c) {   // a floor rectangle (world coords)
        Vector2 a = ToScreen({x0, y0}), b = ToScreen({x1, y1});
        DrawRectangle((int)a.x, (int)a.y, (int)(b.x - a.x), (int)(b.y - a.y) + 1, c);
    };
    switch (field.kind) {
        case ArenaKind::Balance: {
            for (int r = 0; r < 4; r++) {   // the room names, painted by each room's back corner
                float x = (r & 1) ? ARENA_W * 0.75f : ARENA_W * 0.25f, y = (r & 2) ? ARENA_H * 0.55f : ARENA_H * 0.05f;
                Vector2 p = ToScreen({x, y});
                DrawTextCentered(kRoomName[r], (int)p.x, (int)p.y, 10, Fade(kRoom[r], 0.55f), false);
            }
            if (field.scaleOwner >= 0 && field.scaleT > 0) {   // the rooms flicker red for the side out of balance
                float a = 0.06f + 0.04f * std::sin(t * 6);
                quad(0, 0, ARENA_W, ARENA_H, Fade(Cc(200, 40, 40), a));
            }
        } break;

        case ArenaKind::TwinMoons: {
            bool left = field.phase == 0;   // sun on the left?
            float warn = field.warnT > 0 ? (((int)(t * 8) % 2) ? 1.0f : 0.4f) : 1.0f;
            VAdd();
            quad(left ? 0 : ARENA_W / 2, 0, left ? ARENA_W / 2 : ARENA_W, ARENA_H, Fade(kSun, 0.16f * warn));
            VNorm();
            quad(left ? ARENA_W / 2 : 0, 0, left ? ARENA_W : ARENA_W / 2, ARENA_H, Fade(Cc(20, 24, 70), 0.38f * warn));
            for (int i = 0; i < 40; i++) {   // stars twinkling on the moon half
                float wx = (left ? ARENA_W / 2 : 0) + VRand(71, i) * ARENA_W / 2, wy = VRand(72, i) * ARENA_H;
                float a = 0.4f + 0.6f * std::sin(t * 2 + i * 1.7f);
                Vector2 p = ToScreen({wx, wy});
                DrawRectangle((int)p.x, (int)p.y, 1, 1, Fade(WHITE, Clamp01(a)));
            }
            Vector2 m0 = ToScreen({ARENA_W / 2, 0}), m1 = ToScreen({ARENA_W / 2, ARENA_H});
            DrawLineEx(m0, m1, 2, Fade(field.jamT > 0 ? Cc(170, 120, 200) : WHITE, 0.5f));
        } break;

        case ArenaKind::Sootopolis:
        case ArenaKind::SkyPillar: {
            bool sky = field.kind == ArenaKind::SkyPillar;
            for (size_t i = 0; i < field.tiles.size(); i++) {
                const Tile& tl = field.tiles[i];
                float h = Field::TILE / 2 - 1;
                Vector2 a = ToScreen({tl.pos.x - h, tl.pos.y - h}), b = ToScreen({tl.pos.x + h, tl.pos.y + h});
                int x = (int)a.x, y = (int)a.y, w = (int)(b.x - a.x), hh = (int)(b.y - a.y);
                if (tl.state == 2) {
                    if (sky) {   // a hole: the sky far below
                        DrawRectangle(x, y, w, hh, Cc(120, 170, 220));
                        DrawRectangle(x, y, w, 2, Cc(40, 40, 50));
                        float cx = x + std::fmod(t * 6 + i * 13.0f, (float)w);
                        DrawEllipse((int)cx, y + hh / 2, 6, 2, Fade(WHITE, 0.6f));
                    } else {   // open water: a ripple
                        float u = std::fmod(t * 0.8f + i * 0.31f, 1.0f);
                        Vector2 c{(a.x + b.x) / 2, (a.y + b.y) / 2};
                        VRingE(c, 3 + u * 12, (3 + u * 12) * DEPTH, 0.8f, Fade(kFoam, 0.5f * (1 - u)), 16);
                    }
                    continue;
                }
                if (!sky) {   // ice over the lake
                    DrawRectangle(x, y, w, hh, Fade(Cc(214, 236, 250), 0.78f));
                    DrawRectangle(x, y, w, 1, Fade(WHITE, 0.9f));
                    DrawRectangle(x, y + hh - 1, w, 1, Fade(Cc(130, 170, 210), 0.9f));
                    DrawRectangle(x + 3, y + 2, w / 3, 1, Fade(WHITE, 0.7f));
                }
                float stress = Clamp01(tl.standT / 2.0f);
                if (tl.state == 1 || stress > 0.3f) {   // cracks spreading across it
                    Color cr = sky ? Cc(60, 50, 40) : Cc(90, 130, 170);
                    int n = tl.state == 1 ? 4 : 2;
                    Vector2 c{(a.x + b.x) / 2, (a.y + b.y) / 2};
                    for (int k = 0; k < n; k++) {
                        float an = VRand(80 + (uint32_t)i, k) * 2 * PI_F;
                        float len = (tl.state == 1 ? 0.45f : 0.25f) * w;
                        Vector2 e{c.x + std::cos(an) * len, c.y + std::sin(an) * len * DEPTH};
                        DrawLineEx(c, e, 1, cr);
                        DrawLineEx(e, {e.x + std::cos(an + 0.8f) * len * 0.4f, e.y + std::sin(an + 0.8f) * len * 0.4f * DEPTH}, 1, cr);
                    }
                    if (tl.state == 1 && stress > 0.5f && ((int)(t * 10) % 2)) DrawRectangleLines(x, y, w, hh, Fade(Cc(255, 90, 80), 0.7f));
                }
            }
        } break;

        case ArenaKind::HoennChampion: {
            // the waterfall along the back wall
            for (int i = 0; i < 60; i++) {
                float wx = 20 + i * (ARENA_W - 40) / 60.0f;
                float ph = std::fmod(t * 1.8f + VRand(90, i), 1.0f);
                Vector2 p = ToScreen({wx, 2}, 18 - ph * 18);
                DrawRectangle((int)p.x, (int)p.y, 2, 3, Fade(i % 3 ? Cc(150, 200, 255) : kFoam, 0.7f));
            }
            if (field.warnT > 0 && ((int)(t * 8) % 2)) {   // about to surge
                Vector2 a = ToScreen({0, 0}), b = ToScreen({ARENA_W, 24});
                DrawRectangle((int)a.x, (int)a.y, (int)(b.x - a.x), (int)(b.y - a.y), Fade(kRain, 0.3f));
            }
            if (field.surgeT >= 0) {   // the wave front
                Vector2 d = field.surgeDir;
                float span = std::fabs(d.x) * ARENA_W + std::fabs(d.y) * ARENA_H;
                float front = field.surgeT * span;
                for (int k = 0; k < 3; k++) {
                    float off = front - k * 10;
                    if (off < 0) continue;
                    Vector2 a, b;
                    if (std::fabs(d.y) > 0.5f) {
                        float y = d.y > 0 ? off : ARENA_H - off;
                        a = ToScreen({0, y}); b = ToScreen({ARENA_W, y});
                    } else {
                        float x = d.x > 0 ? off : ARENA_W - off;
                        a = ToScreen({x, 0}); b = ToScreen({x, ARENA_H});
                    }
                    DrawLineEx(a, b, k == 0 ? 3.0f : 2.0f, Fade(k == 0 ? kFoam : kRain, 0.8f - k * 0.22f));
                }
            }
        } break;
        default: break;
    }
}

void Battle::DrawHoennTop() const {
    const float t = time;
    if (field.kind == ArenaKind::Sootopolis && field.rainT > 0) {   // the font's rain, over the side it favours
        bool left = field.rainOwner == 0;   // team 0 starts on the left
        for (int i = 0; i < 70; i++) {
            float wx = (left ? 0.0f : ARENA_W / 2) + VRand(61, i) * ARENA_W / 2;
            float ph = std::fmod(t * 1.5f + VRand(62, i), 1.0f);
            float wy = VRand(63, i) * ARENA_H;
            Vector2 p = ToScreen({wx, wy}, 60 * (1 - ph));
            DrawLine((int)p.x, (int)p.y, (int)p.x - 1, (int)p.y + 4, Fade(kRain, 0.6f));
        }
    }
    if (field.kind == ArenaKind::TwinMoons) {   // a sun and a moon hanging over their halves
        bool left = field.phase == 0;
        Vector2 sp = ToScreen({left ? ARENA_W * 0.25f : ARENA_W * 0.75f, 0}, 22);
        Vector2 mp = ToScreen({left ? ARENA_W * 0.75f : ARENA_W * 0.25f, 0}, 22);
        VAdd();
        DrawCircleGradient((int)sp.x, (int)sp.y, 16, Fade(kSun, 0.45f), Fade(kSun, 0));
        VNorm();
        DrawCircle((int)sp.x, (int)sp.y, 6, kSun);
        DrawCircle((int)mp.x, (int)mp.y, 6, Cc(220, 225, 255));
        DrawCircle((int)mp.x + 3, (int)mp.y - 1, 5, Cc(40, 40, 90));
    }
}

void Battle::DrawHoennCenterBody(const Obstacle& o) const {
    const Centerpiece& c = field.center;
    Vector2 g = ToScreen(o.pos);
    float fl = c.flash > 0 ? 1.0f : 0.0f;
    switch (c.kind) {
        case Centerpiece::Scale: {   // a brass balance scale on a stone plinth
            DrawEllipse((int)g.x, (int)g.y, 16, 6, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 9, (int)g.y - 8, 18, 8, Cc(120, 110, 100));
            DrawRectangle((int)g.x - 9, (int)g.y - 8, 18, 1, Cc(170, 160, 150));
            DrawRectangle((int)g.x - 1, (int)g.y - 40, 3, 32, Cc(200, 160, 70));
            Vector2 piv{g.x + 0.5f, g.y - 40};
            float a = c.spin;
            Vector2 l{piv.x - std::cos(a) * 18, piv.y - std::sin(a) * 18}, r{piv.x + std::cos(a) * 18, piv.y + std::sin(a) * 18};
            DrawLineEx(l, r, 2, Cc(230, 190, 90));
            for (int k = 0; k < 2; k++) {
                Vector2 e = k ? r : l;
                DrawLine((int)e.x, (int)e.y, (int)e.x - 4, (int)e.y + 9, Cc(200, 160, 70));
                DrawLine((int)e.x, (int)e.y, (int)e.x + 4, (int)e.y + 9, Cc(200, 160, 70));
                bool own = field.scaleOwner == k && field.scaleT > 0;
                DrawEllipse((int)e.x, (int)e.y + 10, 6, 2, own ? UI_GOLD : Cc(210, 170, 80));
            }
            DrawCircle((int)piv.x, (int)piv.y, 2, Cc(255, 220, 120));
            if (fl > 0) DrawCircleGradient((int)piv.x, (int)piv.y + 10, 20, Fade(WHITE, 0.5f), Fade(WHITE, 0));
        } break;
        case Centerpiece::Dial: {   // a stone disc on a pedestal: half sun, half moon
            DrawEllipse((int)g.x, (int)g.y, 15, 6, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 5, (int)g.y - 16, 10, 16, Cc(110, 100, 130));
            Vector2 cc{g.x, g.y - 30};
            float r = 13;
            for (int k = 0; k < 24; k++) {   // two half discs, turning with the eclipses
                float a0 = c.spin + k * PI_F / 12, a1 = a0 + PI_F / 12;
                Color col = k < 12 ? kSun : kMoon;
                if (field.jamT > 0) col = ColorBrightness(col, -0.5f);
                DrawTriangle(cc, {cc.x + std::cos(a1) * r, cc.y + std::sin(a1) * r}, {cc.x + std::cos(a0) * r, cc.y + std::sin(a0) * r}, col);
            }
            DrawCircleLines((int)cc.x, (int)cc.y, r, Cc(60, 50, 80));
            DrawCircle((int)cc.x, (int)cc.y, 2, WHITE);
            if (fl > 0) DrawCircleGradient((int)cc.x, (int)cc.y, 20, Fade(WHITE, 0.5f), Fade(WHITE, 0));
        } break;
        case Centerpiece::Font: {   // a carved stone basin brimming with water
            DrawEllipse((int)g.x, (int)g.y, 16, 7, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 6, (int)g.y - 14, 12, 14, Cc(140, 150, 170));
            DrawEllipse((int)g.x, (int)g.y - 16, 15, 6, Cc(160, 170, 190));
            DrawEllipse((int)g.x, (int)g.y - 16, 12, 4, field.rainT > 0 ? Cc(120, 190, 255) : Cc(70, 130, 200));
            float sh = 0.5f + 0.5f * std::sin(time * 3);
            DrawRectangle((int)g.x - 5, (int)g.y - 17, 4, 1, Fade(WHITE, 0.5f + 0.4f * sh));
            if (field.rainT > 0) {   // a little cloud over it
                DrawEllipse((int)g.x, (int)g.y - 44, 14, 6, Cc(180, 190, 210));
                DrawEllipse((int)g.x - 8, (int)g.y - 42, 8, 5, Cc(170, 180, 200));
            }
            if (fl > 0) DrawCircleGradient((int)g.x, (int)g.y - 16, 22, Fade(WHITE, 0.5f), Fade(WHITE, 0));
        } break;
        case Centerpiece::Crystal: {   // a tall blue crystal on a shell-shaped base
            DrawEllipse((int)g.x, (int)g.y, 14, 6, Fade(BLACK, 0.35f));
            DrawEllipse((int)g.x, (int)g.y - 2, 13, 5, Cc(230, 220, 240));
            float pulse = 0.5f + 0.5f * std::sin(time * 2.5f);
            Vector2 top{g.x, g.y - 46}, l{g.x - 8, g.y - 10}, r{g.x + 8, g.y - 10}, m{g.x, g.y - 4};
            DrawTriangle(top, l, m, Cc(90, 160, 240));
            DrawTriangle(top, m, r, Cc(140, 200, 255));
            DrawLineEx(top, m, 1, Fade(WHITE, 0.6f));
            VAdd();
            DrawCircleGradient((int)g.x, (int)g.y - 26, 20 + 4 * pulse, Fade(Cc(120, 190, 255), 0.25f + 0.15f * fl), Fade(Cc(120, 190, 255), 0));
            VNorm();
        } break;
        case Centerpiece::Meteorite: {
            if (c.phase == 2) {   // rubble
                for (int k = 0; k < 5; k++) {
                    float a = k * 1.3f;
                    DrawEllipse((int)(g.x + std::cos(a) * 12), (int)(g.y + std::sin(a) * 5), 5, 3, Cc(70, 60, 80));
                }
                break;
            }
            DrawEllipse((int)g.x, (int)g.y, 24, 9, Fade(BLACK, 0.4f));
            DrawEllipse((int)g.x, (int)g.y - 16, 22, 17, Cc(60, 52, 72));
            DrawEllipse((int)g.x - 6, (int)g.y - 22, 12, 8, Cc(84, 74, 98));
            DrawEllipse((int)g.x + 8, (int)g.y - 10, 8, 6, Cc(48, 40, 58));
            Color vein = c.phase == 1 ? kStar : Cc(120, 110, 200);
            float glow = c.phase == 1 ? 1.0f : 0.4f + 0.2f * std::sin(time * 2);
            for (int k = 0; k < 4; k++) {   // glowing veins
                float a = k * 1.6f + 0.4f;
                DrawLineEx({g.x, g.y - 16}, {g.x + std::cos(a) * 16, g.y - 16 + std::sin(a) * 12}, c.phase == 1 ? 2.0f : 1.0f, Fade(vein, glow));
            }
            if (c.phase == 1 || fl > 0) {
                VAdd();
                DrawCircleGradient((int)g.x, (int)g.y - 16, 34, Fade(kStar, 0.35f), Fade(kStar, 0));
                VNorm();
            }
        } break;
        default: break;
    }
}
