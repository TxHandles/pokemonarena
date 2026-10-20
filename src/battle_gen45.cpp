// Sinnoh and Unova arenas: Fantina's door hall, Wake's floodgates, Volkner's gears, Cynthia's room,
// the Distortion World, the Spear Pillar and Newmoon / Hall of Origin lairs; the Striaton
// restaurant, Elesa's coaster, Skyla's cannons, N's throne room, Alder's hall, and the Unova lairs.
// The lairs' falling strikes share UpdateLegendField (battle_field.cpp).
// Everything that changes the battle is battle state (Field::xT / xI / xMarks / side*, saved for online
// play); Draw* is visual only.
//
// What the Field slots mean, by arena:
//   Hearthome       xT0 next door swap, xT1 swap warning, xT2 chandelier light left; xI0 door pairing (0..2); xMarks doors
//   Pastoria        xT0 next level change, xT1 warning, xT2 switch jammed; xI0 0 low water / 1 high water
//   Sunyshore       xT0 next reverse, xT1 gear angle, xT2 sunbeam left; xI0 turn direction, xI1 sunbeam team; xMarks gears
//   SinnohChampion  xT0 guardian left; xI0 next guardian, xI1 guardian team, xI2 guardian (0 Uxie, 1 Mesprit, 2 Azelf)
//   Distortion      xT0 next shift, xT1 warning, xT2 pull left, xT3 rift left; xI0 gravity (0 E, 1 W, 2 S, 3 N), xI1 rift team
//   SpearTime       lights: the slowed-time patches
//   SpearSpace      xT0 portals move, xT1 next rift, xT2 rift warning; xMarks the rift line
//   HallOrigin      xT0 next Plate; xI0 the Plate's type
//   Striaton        xT0..2 table i put out; xI0 how far the tables have turned
//   Nimbasa         xT0 next train, xT1 warning, xT2 the train's run (0..1), xT3 switch jammed; xI0 track (-1 none),
//                   xI1 who it has hit, xI2 direction, xI3 the switch's team (-1 none); xMarks the two tracks
//   Mistralton      xT0 gust left, xT1 propeller stalled; xI0 gust team; xMarks pads; sideT/sideFrom/sideTo the flights
//   ThroneRoom      xT0 next strike, xT1 pillar power left, xT2 pillar cooldown; xI0 next strike type, xI1 power team, xI2 0 fire / 1 electric
//   UnovaChampion   xT0 next flare, xT1 warning, xT2 sunshine left, xT3 doused; xI0 sunshine team; xMarks braziers
//   AbundantShrine  xT0 next tremor
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
constexpr float BODY_R = 9.0f;
constexpr float GEAR_R = 54, PAD_R = 15, DOOR_R = 16, SLOW_R = 46, BRAZIER_R = 36;
float Frand(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }
bool HasT(const SpeciesDef* sp, Type t) { return sp->type1 == t || sp->type2 == t; }
Color Cc(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
bool OnFloor(const Fighter& f) { return f.state == FState::Active && !f.burrowed && f.jumpT <= 0 && f.heldZ <= 0 && !IsAirborne(*f.sp); }
Vector2 Clamp(Vector2 p, float m = 9) { return {std::max(m, std::min(ARENA_W - m, p.x)), std::max(m, std::min(ARENA_H - m, p.y))}; }

const Color kGhostLight = {255, 220, 150, 255};
const Color kDoor = {200, 150, 255, 255};
const Color kWater = {120, 180, 255, 255};
const Color kVolt = {255, 230, 90, 255};
const Color kVoid = {190, 110, 255, 255};
const Color kTime = {120, 200, 255, 255};
const Color kSpace = {255, 140, 200, 255};
const Color kWhite = {255, 240, 220, 255};
const Color kBlue = {110, 170, 255, 255};
const Color kFlame = {255, 150, 60, 255};
const Type kTable[3] = {Type::Fire, Type::Water, Type::Grass};
const char* kTableName[3] = {"grill", "fountain", "herb bed"};
const char* kGuardian[3] = {"Uxie", "Mesprit", "Azelf"};
const Color kGuardianCol[3] = {{255, 220, 90, 255}, {255, 140, 190, 255}, {110, 170, 255, 255}};

// Fantina's doors: where each door leads, for each way the shapes pair up.
int DoorTo(int mode, int k) {
    static const int to[3][4] = {{1, 0, 3, 2}, {2, 3, 0, 1}, {3, 2, 1, 0}};
    return to[((mode % 3) + 3) % 3][k & 3];
}
int DoorShape(int mode, int k) {   // the two doors that lead to each other share a shape (0..3)
    int a = std::min(k, DoorTo(mode, k));
    return (a + mode) % 4;
}
Vector2 GravDir(int d) { return d == 0 ? Vector2{1, 0} : d == 1 ? Vector2{-1, 0} : d == 2 ? Vector2{0, 1} : Vector2{0, -1}; }
const char* GravName(int d) { return d == 0 ? "east" : d == 1 ? "west" : d == 2 ? "south" : "north"; }
Type Beats(Type t) { return t == Type::Fire ? Type::Water : t == Type::Water ? Type::Grass : Type::Fire; }

} // namespace

int Battle::LairBoss() const {
    for (int s = 0; s < N(); s++) if (sides[s].teamId == 1 && sides[s].Cur().boss > 0) return s;
    return -1;
}

// Strikes falling from above on marked spots: on the challengers (bossSide >= 0) or on everyone.
void Battle::DropStrikes(Type t, int power, int count, int bossSide, Status st, float chance, float radius) {
    std::vector<int> at;
    for (int s = 0; s < N(); s++) if (Targetable(s) && (bossSide < 0 || sides[s].teamId == 0)) at.push_back(s);
    for (int k = 0; k < count && !at.empty(); k++) {
        int ts = at[SimRand(0, (int)at.size() - 1)];
        Projectile p;
        p.hit = EnvSpec(t, power, st, chance);
        p.hit.side = bossSide;
        p.hit.special = true;
        p.drop = true;
        Vector2 to = Clamp(sides[ts].Cur().pos + Vector2{Frand(-24, 24), Frand(-18, 18)}, 6);
        p.target = p.pos = to;
        p.dur = 1.3f;
        p.t = -k * 0.35f;
        p.z = 110;
        p.radius = radius;
        p.spin = Frand(0, 6);
        projs.push_back(p);
    }
}

// ---------------------------------------------------------------- Update

void Battle::UpdateGen45(float dt) {
    bool live = winner < 0 && introT <= 0;
    int boss = LairBoss();
    bool tick = (int)(time * 2) != (int)((time - dt) * 2);
    auto text = [&](const char* s, Color c) { AddText({ARENA_W / 2, ARENA_H * 0.1f}, s, c, 20); };
    switch (field.kind) {
        case ArenaKind::Hearthome:
            if (field.xT[2] > 0) {
                field.xT[2] -= dt;
                if (field.xT[2] <= 0) text("The chandelier's candles burn out - lights out!", UI_DIM);
            }
            if (!live) break;
            field.xT[0] -= dt;
            if (field.xT[1] <= 0 && field.xT[0] <= 1.5f && field.xT[0] > 0) { field.xT[1] = 1.5f; text("The doors are flickering...", kDoor); PlaySfx(Sfx::Status); }
            if (field.xT[1] > 0) field.xT[1] -= dt;
            if (field.xT[0] <= 0) {
                field.xI[0] = (field.xI[0] + 1 + SimRand(0, 1)) % 3;
                field.xT[0] = 10.0f;
                field.xT[1] = 0;
                text("The doors' shapes changed!", kDoor);
                for (const Vector2& d : field.xMarks) Burst(d, kDoor, 8, 40, 10);
                PlaySfx(Sfx::Shield);
            }
            break;

        case ArenaKind::Pastoria:
            if (field.xT[2] > 0) field.xT[2] -= dt;
            if (!live) break;
            field.xT[0] -= dt;
            if (field.xT[1] <= 0 && field.xT[0] <= 1.5f && field.xT[0] > 0) {
                field.xT[1] = 1.5f;
                text(field.xI[0] ? "The floodgates rumble - the water is draining!" : "The floodgates rumble - the water is rising!", kWater);
                PlaySfx(Sfx::Status);
            }
            if (field.xT[1] > 0) field.xT[1] -= dt;
            if (field.xT[0] <= 0) {
                field.xI[0] ^= 1;
                field.xT[0] = 12.0f;
                field.xT[1] = 0;
                text(field.xI[0] ? "High water! Only the platforms are dry!" : "Low water! The walkways are dry again!", kWater);
                Shake(1);
                PlaySfx(Sfx::Water);
            }
            break;

        case ArenaKind::Sunyshore: {
            field.xT[1] += dt * 0.9f * (float)field.xI[0];
            if (field.xT[2] > 0) {
                field.xT[2] -= dt;
                if (field.xT[2] <= 0) { field.xI[1] = -1; text("The sunbeam fades.", UI_DIM); }
            }
            field.center.charge = std::min(100.0f, field.center.charge + dt * 2.0f);   // the sun charges it slowly
            if (!live) break;
            field.xT[0] -= dt;
            bool flip = field.xT[0] <= 0;
            if (flip) {
                field.xI[0] = -field.xI[0];
                field.xT[0] = Frand(7.0f, 9.0f);
                text("The gears reverse!", kVolt);
                Shake(1);
                PlaySfx(Sfx::Thud);
            }
            for (int s = 0; s < N(); s++) {
                Fighter& f = sides[s].Cur();
                if (!OnFloor(f) || Held(s)) continue;
                for (const Vector2& g : field.xMarks) {
                    float d = Dist(f.pos, g);
                    if (d > GEAR_R) continue;
                    if (flip) {   // flung off
                        f.knock += SafeNorm(f.pos - g, Vector2{1, 0}) * 230.0f;
                        AddText(f.pos, "Flung off!", kVolt, 26);
                    } else {
                        float a = 0.9f * (float)field.xI[0] * dt;
                        Vector2 r = f.pos - g;
                        f.pos = Clamp(g + Vector2{r.x * std::cos(a) - r.y * std::sin(a), r.x * std::sin(a) + r.y * std::cos(a)});
                    }
                    if (d > GEAR_R - 9 && tick && !HasT(f.sp, Type::Electric) && TypeEffectiveness(Type::Electric, f.sp->type1, f.sp->type2) > 0) {
                        DirectDamage(s, f.maxHp * 0.01f, kVolt, AnyFoe(s));
                        if (SimRand(0, 1) == 0) Emit(PK::Spark, f.pos, Frand(2, 10), {Frand(-40, 40), Frand(-20, 20)}, 30, 0.3f, 1.4f, kVolt);
                    }
                    break;
                }
            }
        } break;

        case ArenaKind::SinnohChampion:
            if (field.xT[0] > 0) {
                field.xT[0] -= dt;
                if (field.xT[0] <= 0) { field.xI[1] = -1; AddText(field.center.pos, "The mural's glow fades.", UI_DIM, 30); }
            }
            break;

        case ArenaKind::Distortion: {
            bool lair = boss >= 0;
            if (field.xT[3] > 0) { field.xT[3] -= dt; if (field.xT[3] <= 0) { field.xI[1] = -1; AddText(field.center.pos, "The rift closes.", UI_DIM, 30); } }
            if (field.xT[2] > 0) field.xT[2] -= dt;
            if (!live) break;
            field.xT[0] -= dt;
            if (field.xT[1] <= 0 && field.xT[0] <= 1.5f && field.xT[0] > 0) {
                field.xI[0] = (field.xI[0] + 1 + SimRand(0, 2)) % 4;
                field.xT[1] = 1.5f;
                text(TextFormat("Gravity is turning to the %s...", GravName(field.xI[0])), kVoid);
                PlaySfx(Sfx::Status);
            }
            if (field.xT[1] > 0) field.xT[1] -= dt;
            if (field.xT[0] <= 0) {
                field.xT[0] = 12.0f;
                field.xT[1] = 0;
                field.xT[2] = 3.0f;
                text(TextFormat("Gravity pulls to the %s!", GravName(field.xI[0])), kVoid);
                Shake(2);
                PlaySfx(Sfx::Buff);
            }
            for (int s = 0; s < N(); s++) {
                Fighter& f = sides[s].Cur();
                if (!OnFloor(f) || Held(s) || (lair && sides[s].teamId != 0)) continue;
                if (!HasT(f.sp, Type::Ghost)) {
                    Vector2 push{0, 0};
                    if (field.xT[2] > 0) push += GravDir(field.xI[0]) * 70.0f;
                    if (field.xI[1] >= 0 && sides[s].teamId != field.xI[1]) push += Vector2{field.xI[1] == 0 ? 1.0f : -1.0f, 0} * 55.0f;
                    if (push.x != 0 || push.y != 0) f.pos = Clamp(f.pos + push * dt);
                }
                if (f.portalCd <= 0 && !DistortionGround(f.pos)) {   // into the void: climb back onto the nearest ground
                    Vector2 best{ARENA_W / 2, ARENA_H / 2};
                    float bd = 1e9f;
                    for (int r = 1; r <= 6; r++)
                        for (int k = 0; k < 16; k++) {
                            float a = k * 2 * PI_F / 16;
                            Vector2 p = f.pos + Vector2{std::cos(a), std::sin(a)} * (16.0f * r);
                            if (p.x < 12 || p.x > ARENA_W - 12 || p.y < 12 || p.y > ARENA_H - 12 || !DistortionGround(p)) continue;
                            float d = Dist(p, f.pos);
                            if (d < bd) { bd = d; best = p; }
                        }
                    Vector2 in = SafeNorm(Vector2{ARENA_W / 2, ARENA_H / 2} - best, Vector2{0, 0});
                    Burst(f.pos, kVoid, 12, 50, 4);
                    f.pos = best + in * 10.0f;
                    f.dashing = false;
                    f.knock = {0, 0};
                    f.portalCd = 1.0f;
                    DirectDamage(s, f.maxHp * 0.04f, kVoid, AnyFoe(s));
                    AddText(f.pos, "Fell into the void!", kVoid, 30);
                    PlaySfx(Sfx::Thud);
                }
            }
            if (lair) {   // Giratina's shadow claws
                field.strikeT -= dt;
                if (field.strikeT <= 0) {
                    field.strikeT = 5.0f * Frand(0.9f, 1.1f);
                    DropStrikes(Type::Ghost, 55, 2, boss);
                    text("Shadow claws reach out of the void!", kVoid);
                }
            }
        } break;

        case ArenaKind::SpearTime: {   // the slowed-time patches drift after the challengers
            std::vector<int> ch;
            for (int s = 0; s < N(); s++) if (sides[s].teamId == 0 && Targetable(s)) ch.push_back(s);
            for (size_t i = 0; i < field.lights.size(); i++) {
                Vector2 want = ch.empty() ? Vector2{ARENA_W / 2, ARENA_H / 2} : sides[ch[i % ch.size()]].Cur().pos;
                Vector2 d = want - field.lights[i];
                float l = Len(d);
                if (l > 1) field.lights[i] += d * (std::min(l, 26.0f * dt) / l);
            }
        } break;

        case ArenaKind::SpearSpace: {
            if (!live || boss < 0) break;
            field.xT[0] -= dt;
            if (field.xT[0] <= 0) {
                for (const Portal& p : field.portals) Burst(p.pos, kSpace, 8, 40, 6);
                field.PlacePortals();
                field.xT[0] = 8.0f;
                text("Space twists - the portals moved!", kSpace);
                PlaySfx(Sfx::Shield);
            }
            field.xT[1] -= dt;
            if (field.xT[2] <= 0 && field.xT[1] <= 1.5f && field.xMarks.empty()) {   // mark the rift's line through a challenger
                std::vector<int> ch;
                for (int s = 0; s < N(); s++) if (sides[s].teamId == 0 && Targetable(s)) ch.push_back(s);
                if (!ch.empty()) {
                    Vector2 c = sides[ch[SimRand(0, (int)ch.size() - 1)]].Cur().pos;
                    float a = Frand(0, PI_F);
                    Vector2 d{std::cos(a), std::sin(a)};
                    field.xMarks = {Clamp(c - d * 400.0f, 4), Clamp(c + d * 400.0f, 4)};
                    field.xT[2] = 1.5f;
                    text("Space is tearing open along the line!", kSpace);
                    PlaySfx(Sfx::Status);
                }
            }
            if (field.xT[2] > 0) {
                field.xT[2] -= dt;
                if (field.xT[2] <= 0 && field.xMarks.size() == 2) {
                    Vector2 a = field.xMarks[0], b = field.xMarks[1];
                    Vector2 n = SafeNorm(Vector2{-(b.y - a.y), b.x - a.x}, Vector2{0, 1});
                    HitSpec h = EnvSpec(Type::Dragon, 55);
                    h.side = boss;
                    h.special = true;
                    for (int s = 0; s < N(); s++)
                        if (sides[s].teamId == 0 && Hittable(s) && DistPointSeg(sides[s].Cur().pos, a, b) < HitR(sides[s].Cur()) + 10)
                            ApplyHit(h, s, Dot(sides[s].Cur().pos - a, n) >= 0 ? n : n * -1.0f);
                    for (int k = 0; k <= 10; k++) Burst(a + (b - a) * (k / 10.0f), kSpace, 4, 50, 6);
                    Shake(2);
                    PlaySfx(Sfx::HitSuper);
                    field.xMarks.clear();
                    field.xT[1] = 6.0f * Frand(0.9f, 1.1f);
                }
            }
        } break;

        case ArenaKind::Newmoon: {   // Dark Void clouds drift after the challengers
            if (!live || boss < 0) break;
            for (Wisp& w : field.wisps) {
                if (w.life <= 0) {
                    w.respawnT -= dt;
                    if (w.respawnT > 0) continue;
                    w.pos = Clamp(sides[boss].Cur().pos + Vector2{Frand(-40, 40), Frand(-30, 30)}, 20);
                    w.vel = {0, 0};
                    w.life = Frand(9, 12);
                    Burst(w.pos, Cc(90, 40, 140), 10, 30, 10);
                    continue;
                }
                w.life -= dt;
                int best = -1;
                float bd = 1e9f;
                for (int s = 0; s < N(); s++) {
                    if (sides[s].teamId != 0 || !Targetable(s)) continue;
                    float d = Dist(sides[s].Cur().pos, w.pos);
                    if (d < bd) { bd = d; best = s; }
                }
                Vector2 want = best >= 0 ? SafeNorm(sides[best].Cur().pos - w.pos) * 32.0f : Vector2{0, 0};
                w.vel = w.vel + (want - w.vel) * std::min(1.0f, dt * 1.2f);
                w.pos = Clamp(w.pos + w.vel * dt, 10);
                if (best >= 0 && bd < BODY_R + 8 && Hittable(best)) {
                    TryStatus(best, Status::Sleep, boss);
                    AddText(w.pos, "Dark Void!", Cc(170, 110, 230), 30);
                    Burst(w.pos, Cc(90, 40, 140), 16, 50, 10);
                    PlaySfx(Sfx::Status);
                    w.life = 0;
                    w.respawnT = Frand(5.0f, 7.0f);
                } else if (w.life <= 0) {
                    w.respawnT = 2.0f;
                }
            }
        } break;

        case ArenaKind::HallOrigin:
            if (!live) break;
            field.xT[0] -= dt;
            if (field.xT[0] <= 0) {
                field.xT[0] = 10.0f;
                int t = field.xI[0];
                while (t == field.xI[0]) t = SimRand(0, TYPE_COUNT - 1);
                field.xI[0] = t;
                AddText({ARENA_W / 2, ARENA_H * 0.1f}, TextFormat("A Plate glows: Judgment turns %s!", TypeName((Type)t)), TypeColor((Type)t), 20);
                if (boss >= 0) Burst(sides[boss].Cur().pos, TypeColor((Type)t), 18, 70, 20);
                PlaySfx(Sfx::Buff);
            }
            break;

        case ArenaKind::Striaton:
            for (int i = 0; i < 3; i++)
                if (field.xT[i] > 0) {
                    field.xT[i] -= dt;
                    if (field.xT[i] <= 0) {
                        int z = 0;
                        for (const Zone& zz : field.zones) if (zz.kind == Zone::Table && z++ == i) { AddText(zz.pos, "Back in service!", UI_GOLD, 30); break; }
                    }
                }
            break;

        case ArenaKind::Nimbasa: {
            if (field.xT[3] > 0) field.xT[3] -= dt;
            if (field.xI[0] < 0) {
                if (!live) break;
                field.xT[0] -= dt;
                if (field.xT[0] <= 0) {
                    if (field.xT[3] > 0) { field.xT[0] = 1.0f; break; }   // the switch is jammed: no trains
                    field.xI[0] = SimRand(0, 1);
                    field.xI[2] = SimRand(0, 1) ? 1 : -1;
                    field.xT[1] = 1.5f;
                    field.xT[2] = 0;
                    field.xI[1] = 0;
                    text(field.xI[3] >= 0 ? "The switched train is coming!" : "A coaster train is coming - off the tracks!", kVolt);
                    PlaySfx(Sfx::Status);
                }
                break;
            }
            if (field.xT[1] > 0) { field.xT[1] -= dt; break; }
            field.xT[2] += dt / 1.1f;
            Vector2 a = field.xMarks[field.xI[0] * 2], b = field.xMarks[field.xI[0] * 2 + 1];
            if (field.xI[2] < 0) std::swap(a, b);
            Vector2 dir = SafeNorm(b - a, Vector2{1, 0});
            Vector2 head = a + (b - a) * Clamp01(field.xT[2]);
            if (SimRand(0, 1) == 0) Emit(PK::Spark, head, Frand(4, 12), dir * -80.0f + Vector2{Frand(-30, 30), Frand(-20, 20)}, 30, 0.3f, 1.4f, kVolt);
            for (int s = 0; s < N(); s++) {
                const Fighter& f = sides[s].Cur();
                if ((field.xI[1] >> s) & 1) continue;
                if (!OnFloor(f) || !Hittable(s) || (field.xI[3] >= 0 && sides[s].teamId == field.xI[3])) continue;
                if (Dist(f.pos, head) > 22 + HitR(f)) continue;
                HitSpec h = EnvSpec(Type::Electric, 50, Status::Paralyze, 0.15f);
                h.special = true;
                h.knock = 2.0f;
                if (field.xI[3] >= 0) for (int q = 0; q < N(); q++) if (sides[q].teamId == field.xI[3]) { h.side = q; break; }
                ApplyHit(h, s, dir);
                field.xI[1] |= 1 << s;
                AddText(f.pos, "Hit by the coaster!", kVolt, 30);
            }
            if (field.xT[2] >= 1) {
                field.xI[0] = -1;
                field.xT[0] = 8.0f * Frand(0.9f, 1.1f);
                field.xI[3] = -1;   // the switch only sends one train
                Shake(1);
            }
        } break;

        case ArenaKind::Mistralton: {
            if (field.xT[1] > 0) field.xT[1] -= dt;
            if (field.xT[0] > 0) {   // the propeller's gust over the foes
                field.xT[0] -= dt;
                Vector2 d{field.xI[0] == 0 ? 1.0f : -1.0f, 0};
                for (int s = 0; s < N(); s++) {
                    Fighter& f = sides[s].Cur();
                    if (sides[s].teamId == field.xI[0] || !OnFloor(f) || Held(s) || f.dashing || HasT(f.sp, Type::Flying)) continue;
                    bool heavy = HasT(f.sp, Type::Steel) || HasT(f.sp, Type::Rock);
                    f.pos = Clamp(f.pos + d * ((heavy ? 40.0f : 80.0f) * dt), BODY_R);
                    if (SimRand(0, 5) == 0) Emit(PK::Smoke, f.pos, 2, d * 60.0f, 4, 0.4f, 2.0f, Cc(220, 225, 230, 200), false).grow = 3;
                }
                if (field.xT[0] <= 0) field.xI[0] = -1;
            }
            for (int s = 0; s < N() && s < 8; s++) {   // the cannon pads
                Fighter& f = sides[s].Cur();
                float& st = field.sideT[s];
                if (st > 0) {   // flying across the hangar
                    st -= dt;
                    float u = Clamp01(1 - st / 0.9f);
                    f.pos = field.sideFrom[s] + (field.sideTo[s] - field.sideFrom[s]) * u;
                    f.knock = {0, 0};
                    f.dashing = false;
                    if (st <= 0) {   // the landing shoves anyone close
                        st = -3.0f;
                        Boom(f.pos, 18, Impact::Wind, 2);
                        Shake(1);
                        PlaySfx(Sfx::Thud);
                        for (int q = 0; q < N(); q++) {
                            if (q == s || !Targetable(q)) continue;
                            Fighter& o = sides[q].Cur();
                            float d = Dist(o.pos, f.pos);
                            if (d > 48 || !OnFloor(o)) continue;
                            o.knock += SafeNorm(o.pos - f.pos, Vector2{1, 0}) * 240.0f;
                            DirectDamage(q, o.maxHp * 0.02f, Cc(220, 230, 240), s);
                            AddText(o.pos, "Shoved by the landing!", Cc(220, 230, 240), 26);
                        }
                        AddText(f.pos, HasT(f.sp, Type::Flying) ? "A perfect landing!" : "Dizzy...", HasT(f.sp, Type::Flying) ? UI_GOLD : UI_DIM, 30);
                    }
                    continue;
                }
                if (st < 0) { st = std::min(0.0f, st + dt); continue; }
                if (!live || f.state != FState::Active || f.burrowed || f.jumpT > 0 || IsAirborne(*f.sp) || Held(s)) continue;
                for (const Vector2& pad : field.xMarks) {
                    if (Dist(f.pos, pad) > PAD_R) continue;
                    field.sideFrom[s] = f.pos;
                    field.sideTo[s] = Clamp(Vector2{ARENA_W - pad.x, ARENA_H - pad.y} + Vector2{Frand(-30, 30), Frand(-24, 24)}, 30);
                    st = 0.9f;
                    f.jumpT = f.jumpDur = 0.9f;
                    AddText(f.pos, "Blast off!", Cc(255, 200, 120), 30);
                    Burst(pad, Cc(255, 200, 120), 14, 70, 4);
                    PlaySfx(Sfx::Shoot);
                    break;
                }
            }
        } break;

        case ArenaKind::ThroneRoom:
            if (field.xT[1] > 0) { field.xT[1] -= dt; if (field.xT[1] <= 0) { field.xI[1] = -1; AddText(field.center.pos, "The pillar dims.", UI_DIM, 30); } }
            if (field.xT[2] > 0) field.xT[2] -= dt;
            if (!live) break;
            field.xT[0] -= dt;
            if (field.xT[0] <= 0) {
                field.xT[0] = 6.0f * Frand(0.9f, 1.1f);
                bool volt = field.xI[0] == 1;
                field.xI[0] ^= 1;
                DropStrikes(volt ? Type::Electric : Type::Fire, 55, 2, boss, volt ? Status::Paralyze : Status::Burn, 0.2f, 16);
                text(volt ? "Blue lightning strikes the marks!" : "White flames fall on the marks!", volt ? kBlue : kWhite);
            }
            break;

        case ArenaKind::UnovaChampion: {
            if (field.xT[2] > 0) { field.xT[2] -= dt; if (field.xT[2] <= 0) { field.xI[0] = -1; AddText(field.center.pos, "The sunshine fades.", UI_DIM, 30); } }
            if (field.xT[3] > 0) field.xT[3] -= dt;
            if (!live) break;
            field.xT[0] -= dt;
            if (field.xT[1] <= 0 && field.xT[0] <= 1.2f && field.xT[0] > 0) { field.xT[1] = 1.2f; text("The braziers are flaring up!", kFlame); PlaySfx(Sfx::Status); }
            if (field.xT[1] > 0) field.xT[1] -= dt;
            if (field.xT[0] <= 0) {
                field.xT[0] = 10.0f;
                field.xT[1] = 0;
                HitSpec h = EnvSpec(Type::Fire, 45, Status::Burn, 0.2f);
                h.special = true;
                for (const Vector2& b : field.xMarks) {
                    Boom(b, 22, Impact::Fire, 4);
                    for (int s = 0; s < N(); s++) {
                        const Fighter& f = sides[s].Cur();
                        if (!Hittable(s) || !OnFloor(f) || HasT(f.sp, Type::Fire) || Dist(f.pos, b) > BRAZIER_R + HitR(f)) continue;
                        ApplyHit(h, s, SafeNorm(f.pos - b, Vector2{1, 0}));
                    }
                }
                Shake(1);
                PlaySfx(Sfx::Fire);
            }
        } break;

        case ArenaKind::AbundantShrine: {
            if (!live || boss < 0) break;
            field.xT[0] -= dt;
            if (field.xT[0] > 0) break;
            field.xT[0] = 6.0f * Frand(0.9f, 1.1f);
            std::vector<int> ch;
            for (int s = 0; s < N(); s++) if (sides[s].teamId == 0 && Targetable(s)) ch.push_back(s);
            if (ch.empty()) break;
            ShockWave w;
            w.hit = EnvSpec(Type::Ground, 50);
            w.hit.side = boss;
            w.origin = Clamp(sides[ch[SimRand(0, (int)ch.size() - 1)]].Cur().pos + Vector2{Frand(-70, 70), Frand(-50, 50)}, 30);
            w.maxR = 120;
            w.speed = 130;
            waves.push_back(w);
            Boom(w.origin, 16, Impact::Sand, 2);
            text("The ground trembles!", Cc(210, 170, 110));
            Shake(2);
            PlaySfx(Sfx::Thud);
        } break;
        default: break;
    }
}

// ---------------------------------------------------------------- Standing in it

void Battle::Gen45EffectsOn(int s, float dt) {
    (void)dt;
    Fighter& f = sides[s].Cur();
    if (f.state != FState::Active || f.burrowed || f.hp <= 0) return;
    const SpeciesDef* sp = f.sp;
    bool air = IsAirborne(*sp) || f.jumpT > 0;
    bool tick = (int)(time * 2) != (int)((time - dt) * 2);
    int team = sides[s].teamId;
    switch (field.kind) {
        case ArenaKind::Hearthome: {
            if (field.xT[2] > 0 && HasT(sp, Type::Ghost)) f.envSpeed = 1.0f;   // the hall is lit: no hiding in the dark
            if (air || f.portalCd > 0 || Held(s)) break;
            for (int k = 0; k < (int)field.xMarks.size() && k < 4; k++) {
                if (Dist(f.pos, field.xMarks[k]) > DOOR_R) continue;
                Vector2 to = field.xMarks[DoorTo(field.xI[0], k)];
                Vector2 out = SafeNorm(Vector2{ARENA_W / 2, ARENA_H / 2} - to, Vector2{0, 1});
                Burst(f.pos, kDoor, 12, 60, 6);
                f.pos = to + out * 26.0f;
                f.dashing = false;
                f.portalCd = 1.2f;
                Burst(f.pos, kDoor, 12, 60, 6);
                PlaySfx(Sfx::Shield);
                break;
            }
        } break;
        case ArenaKind::Pastoria:
            if (field.xI[0] == 1 && !air && HasT(sp, Type::Water) && field.IsWater(f.pos)) f.envAtk *= 1.15f;
            break;
        case ArenaKind::Sunyshore:
            if (!air && HasT(sp, Type::Electric))
                for (const Vector2& g : field.xMarks) if (Dist(f.pos, g) < GEAR_R) { f.envSpeed *= 1.25f; f.envAtk *= 1.15f; break; }
            if (field.xT[2] > 0 && field.xI[1] >= 0 && team != field.xI[1] && (field.xI[1] == 0 ? f.pos.x > ARENA_W / 2 : f.pos.x < ARENA_W / 2))
                f.envSpeed *= 0.75f;   // dazzled by the sunbeam
            break;
        case ArenaKind::SinnohChampion:
            if (field.InSpot(f.pos) && (HasT(sp, Type::Dragon) || HasT(sp, Type::Ground))) f.envDef *= 1.1f;
            if (field.xI[1] == team && field.xT[0] > 0) {
                int g = field.xI[2];
                if (g == 0) f.envDef *= 1.15f; else if (g == 1) f.envAtk *= 1.15f; else f.envSpeed *= 1.15f;
                if (SimRand(0, 12) == 0) Emit(PK::Star, f.pos + Vector2{Frand(-8, 8), Frand(-3, 3)}, Frand(6, 20), {0, 0}, 16, 0.5f, 1.3f, kGuardianCol[g % 3]);
            }
            break;
        case ArenaKind::Distortion:
        case ArenaKind::ThroneRoom:
            if (HasT(sp, Type::Dragon) || (field.kind == ArenaKind::Distortion && HasT(sp, Type::Ghost))) f.envAtk *= 1.15f;
            break;
        case ArenaKind::SpearTime:
            if (team == 0 && LairBoss() >= 0)
                for (const Vector2& l : field.lights) if (Dist(f.pos, l) < SLOW_R) { f.envSpeed *= 0.6f; break; }
            break;
        case ArenaKind::SpearSpace:
            if (f.portalCd <= 0 && field.portals.size() > 1 && !air && !Held(s)) {
                for (size_t k = 0; k < field.portals.size(); k++) {
                    if (Dist(f.pos, field.portals[k].pos) > 13) continue;
                    size_t j = (k + 1 + SimRand(0, (int)field.portals.size() - 2)) % field.portals.size();
                    Vector2 out = SafeNorm(f.moveVel, f.aim);
                    Burst(f.pos, kSpace, 12, 60, 6);
                    f.pos = Clamp(field.portals[j].pos + out * 20.0f);
                    f.dashing = false;
                    f.portalCd = 1.2f;
                    Burst(f.pos, kSpace, 12, 60, 6);
                    PlaySfx(Sfx::Shield);
                    break;
                }
            }
            break;
        case ArenaKind::UnovaChampion:
            if (field.InSpot(f.pos) && HasT(sp, Type::Bug)) f.envAtk *= 1.04f;   // 1.25 -> 1.3: Alder loves his Bug types
            break;
        case ArenaKind::Striaton: {
            if (air) break;
            int i = 0;
            for (const Zone& z : field.zones) {
                if (z.kind != Zone::Table) continue;
                int idx = i++;
                if (!z.Contains(f.pos) || field.xT[idx] > 0) continue;
                Type t = kTable[(idx + field.xI[0]) % 3];
                if (HasT(sp, t)) { f.envAtk *= 1.2f; break; }
                if (t == Type::Grass) f.envSpeed *= 0.85f;
                if (t == Type::Water) f.envSpeed *= 0.92f;
                if (tick) DirectDamage(s, f.maxHp * (HasT(sp, Type::Normal) ? 0.0025f : 0.005f), TypeColor(t), AnyFoe(s));
                break;
            }
        } break;
        case ArenaKind::Nimbasa:
            if (std::fabs(f.pos.x - ARENA_W / 2) < 30 && HasT(sp, Type::Electric)) { f.envSpeed *= 1.2f; f.envAtk *= 1.15f; }
            break;
        case ArenaKind::Mistralton:
            if (s < 8 && field.sideT[s] < 0) {
                if (HasT(sp, Type::Flying)) f.envSpeed *= 1.2f;
                else if (field.sideT[s] < -2.0f) f.envSpeed *= 0.8f;
            }
            break;
        default: break;
    }
}

float Battle::Gen45MoveMul(int s, Type t, bool special) const {
    (void)special;
    if (s < 0 || s >= N()) return 1.0f;
    const Fighter& f = sides[s].Cur();
    int team = sides[s].teamId;
    switch (field.kind) {
        case ArenaKind::Hearthome: return field.xT[2] <= 0 && HasT(f.sp, Type::Ghost) ? 1.15f : 1.0f;
        case ArenaKind::Sunyshore: return field.xT[2] > 0 && field.xI[1] == team && t == Type::Electric ? 1.3f : 1.0f;
        case ArenaKind::ThroneRoom:
            if (field.xT[1] > 0 && field.xI[1] == team && ((field.xI[2] == 0 && t == Type::Fire) || (field.xI[2] == 1 && t == Type::Electric))) return 1.25f;
            return 1.0f;
        case ArenaKind::UnovaChampion:
            if (field.xT[2] > 0 && field.xI[0] == team) return t == Type::Fire ? 1.25f : t == Type::Water ? 0.75f : 1.0f;
            return 1.0f;
        case ArenaKind::Dragonspiral:
            if (f.boss > 0 && (t == Type::Electric || t == Type::Fire))
                for (const Obstacle& o : field.obs) if (o.kind == Obstacle::Pillar && o.alive) return 1.15f;   // the tower's pillars charge it
            return 1.0f;
        default: return 1.0f;
    }
}

// ---------------------------------------------------------------- CPU awareness

Vector2 Battle::Gen45Pull(int s) const {
    const Fighter& f = sides[s].Cur();
    const SpeciesDef* sp = f.sp;
    bool air = IsAirborne(*sp);
    Vector2 pull{0, 0};
    switch (field.kind) {
        case ArenaKind::Distortion: {   // keep away from the edges of the islands
            if (air) break;
            for (int k = 0; k < 8; k++) {
                float a = k * PI_F / 4;
                if (!DistortionGround(f.pos + Vector2{std::cos(a), std::sin(a)} * 30.0f)) { pull += SafeNorm(Vector2{ARENA_W / 2, ARENA_H / 2} - f.pos) * 0.9f; break; }
            }
        } break;
        case ArenaKind::Pastoria:
            if (!air && !HasT(sp, Type::Water) && field.IsWater(f.pos)) {
                Vector2 best{0, 0};
                float bd = 1e9f;
                for (const Zone& z : field.zones) {
                    if (z.kind != Zone::Platform && !(z.kind == Zone::Walk && field.xI[0] == 0)) continue;
                    float d = Dist(z.pos, f.pos);
                    if (d < bd) { bd = d; best = z.pos; }
                }
                if (bd < 1e8f) pull += SafeNorm(best - f.pos) * 0.6f;
            }
            break;
        case ArenaKind::Striaton: {
            if (air) break;
            int i = 0;
            for (const Zone& z : field.zones) {
                if (z.kind != Zone::Table) continue;
                int idx = i++;
                if (field.xT[idx] > 0) continue;
                Type t = kTable[(idx + field.xI[0]) % 3];
                if (HasT(sp, t)) { if (!z.Contains(f.pos) && Dist(z.pos, f.pos) < 200) pull += SafeNorm(z.pos - f.pos) * 0.4f; }
                else if (z.Contains(f.pos, 10)) pull += SafeNorm(f.pos - z.pos, Vector2{1, 0}) * 1.0f;
            }
        } break;
        case ArenaKind::Nimbasa:
            if (field.xI[0] >= 0 && !air && !(field.xI[3] >= 0 && sides[s].teamId == field.xI[3])) {
                Vector2 a = field.xMarks[field.xI[0] * 2], b = field.xMarks[field.xI[0] * 2 + 1];
                if (DistPointSeg(f.pos, a, b) < 34) {
                    Vector2 n = SafeNorm(Vector2{-(b.y - a.y), b.x - a.x}, Vector2{0, 1});
                    pull += (Dot(f.pos - a, n) >= 0 ? n : n * -1.0f) * 1.2f;
                }
            } else if (HasT(sp, Type::Electric) && std::fabs(f.pos.x - ARENA_W / 2) > 30) {
                pull.x += (f.pos.x < ARENA_W / 2 ? 1.0f : -1.0f) * 0.3f;
            }
            break;
        case ArenaKind::Sunyshore:
            if (air) break;
            for (const Vector2& g : field.xMarks) {
                float d = Dist(f.pos, g);
                if (HasT(sp, Type::Electric)) { if (d > GEAR_R && d < 160) pull += SafeNorm(g - f.pos) * 0.3f; }
                else if (d > GEAR_R - 14 && d < GEAR_R + 4) pull += SafeNorm(f.pos - g, Vector2{1, 0}) * 0.6f;
            }
            break;
        case ArenaKind::UnovaChampion:
            if (field.xT[1] > 0 || field.xT[0] < 2.0f)
                for (const Vector2& b : field.xMarks) if (Dist(f.pos, b) < BRAZIER_R + 18) pull += SafeNorm(f.pos - b, Vector2{1, 0}) * 1.1f;
            break;
        case ArenaKind::SpearSpace:
            if (field.xT[2] > 0 && field.xMarks.size() == 2 && sides[s].teamId == 0) {
                Vector2 a = field.xMarks[0], b = field.xMarks[1];
                if (DistPointSeg(f.pos, a, b) < 36) {
                    Vector2 n = SafeNorm(Vector2{-(b.y - a.y), b.x - a.x}, Vector2{0, 1});
                    pull += (Dot(f.pos - a, n) >= 0 ? n : n * -1.0f) * 1.2f;
                }
            }
            break;
        case ArenaKind::SpearTime:
            if (sides[s].teamId == 0)
                for (const Vector2& l : field.lights) if (Dist(f.pos, l) < SLOW_R + 10) pull += SafeNorm(f.pos - l, Vector2{1, 0}) * 0.7f;
            break;
        default: break;
    }
    return pull;
}

// A move landed: the type that beats a Striaton table puts it out.
void Battle::Gen45FieldReact(Vector2 at, const HitSpec& h, float radius) {
    if (field.kind != ArenaKind::Striaton || h.power <= 0) return;
    int i = 0;
    for (const Zone& z : field.zones) {
        if (z.kind != Zone::Table) continue;
        int idx = i++;
        if (field.xT[idx] > 0 || !z.Contains(at, radius)) continue;
        Type t = kTable[(idx + field.xI[0]) % 3];
        if (h.type != Beats(t)) continue;
        field.xT[idx] = 10.0f;
        AddText(z.pos, TextFormat("The %s is out!", kTableName[(int)(std::find(kTable, kTable + 3, t) - kTable)]), UI_DIM, 34);
        Burst(z.pos, Cc(200, 200, 210), 14, 50, 6);
        PlaySfx(Sfx::Water);
    }
}

// ---------------------------------------------------------------- Centerpieces

void Battle::Gen45CenterUpdate(float dt) {
    Centerpiece& c = field.center;
    switch (c.kind) {
        case Centerpiece::Chandelier:
            if (c.phase == 1) {   // dropped: hoisted back up after a while
                c.t -= dt;
                if (c.t <= 0) {
                    c.phase = 0;
                    if (Obstacle* body = CenterBody()) body->alive = true;
                    AddText(c.pos, "The chandelier is hoisted back up.", UI_DIM, 30);
                }
            }
            break;
        case Centerpiece::Bell: c.spin *= std::max(0.0f, 1 - dt * 2); break;
        case Centerpiece::Propeller: c.spin += dt * (field.xT[1] > 0 ? 0.0f : field.xT[0] > 0 ? 24.0f : 3.0f); break;
        case Centerpiece::Rift: c.spin += dt * (field.xI[1] >= 0 ? 3.0f : 0.8f); break;
        default: break;
    }
}

void Battle::Gen45CenterHit(const HitSpec& h, Vector2 dir, bool physical) {
    (void)physical;
    Centerpiece& c = field.center;
    int team = h.side >= 0 && h.side < N() ? sides[h.side].teamId : -1;
    switch (c.kind) {
        case Centerpiece::Chandelier: {
            if (c.phase == 1) return;
            c.hitCd = 1.0f;
            if (h.type == Type::Ghost) {   // it drops: a crash under it, and the hall goes dark
                c.phase = 1;
                c.t = 8.0f;
                field.xT[2] = 0;
                if (Obstacle* body = CenterBody()) body->alive = false;
                HitSpec r = EnvSpec(Type::Ghost, 55);
                r.side = h.side;
                r.special = true;
                for (int s = 0; s < N(); s++)
                    if (Hittable(s) && (h.side < 0 || Foes(h.side, s)) && Dist(sides[s].Cur().pos, c.pos) < 64) ApplyHit(r, s, SafeNorm(sides[s].Cur().pos - c.pos, Vector2{1, 0}));
                Boom(c.pos, 30, Impact::Shadow, 4);
                AddText(c.pos, "The chandelier crashed down!", kDoor, 40);
                Shake(3);
                PlaySfx(Sfx::Thud);
                break;
            }
            field.xT[2] = 8.0f;
            AddText(c.pos, "The chandelier blazes - the hall is lit!", kGhostLight, 40);
            AddFx(FxKind::Flash, c.pos, 0.2f, kGhostLight, 80, 40);
            PlaySfx(Sfx::Buff);
        } break;
        case Centerpiece::Floodgate:
            c.hitCd = 1.0f;
            if (field.xT[2] > 0) { AddText(c.pos, "The switch is stuck...", UI_DIM, 28); return; }
            if (h.type == Type::Electric) {   // a short: everyone in the water gets a shock
                field.xT[2] = 10.0f;
                HitSpec z = EnvSpec(Type::Electric, 45, Status::Paralyze, 0.2f);
                z.side = h.side;
                z.special = true;
                for (int s = 0; s < N(); s++) {
                    const Fighter& f = sides[s].Cur();
                    if (Hittable(s) && OnFloor(f) && field.IsWater(f.pos) && (h.side < 0 || Foes(h.side, s))) ApplyHit(z, s, {0, -1});
                }
                AddText(c.pos, "Short circuit! The water is live!", kVolt, 40);
                AddFx(FxKind::Flash, c.pos, 0.15f, kVolt, 60, 30);
                PlaySfx(Sfx::Zap);
                return;
            }
            field.xI[0] ^= 1;
            field.xT[0] = 12.0f;
            field.xT[1] = 0;
            AddText(c.pos, field.xI[0] ? "The switch floods the gym!" : "The switch drains the gym!", kWater, 40);
            Shake(1);
            PlaySfx(Sfx::Water);
            break;
        case Centerpiece::SolarPanel:
            c.hitCd = 0.4f;
            if (h.type == Type::Ground) { c.charge = 0; AddText(c.pos, "The panel drained!", UI_DIM, 34); PlaySfx(Sfx::Thud); return; }
            if (c.charge >= 100 && team >= 0) {
                c.charge = 0;
                field.xI[1] = team;
                field.xT[2] = 10.0f;
                AddText(c.pos, "Sunbeam! Electric moves power up!", kVolt, 44);
                AddFx(FxKind::Flash, c.pos, 0.25f, kVolt, 120, 40);
                Shake(2);
                PlaySfx(Sfx::Buff);
                return;
            }
            c.charge = std::min(100.0f, c.charge + (h.type == Type::Electric ? 28.0f : 9.0f) + h.power * 0.05f);
            if (c.charge >= 100) AddText(c.pos, "The panel is fully charged!", kVolt, 30);
            break;
        case Centerpiece::Mural: {
            if (team < 0) return;
            c.hitCd = 1.5f;
            int g = field.xI[0] % 3;
            field.xI[2] = g;
            field.xI[0] = (g + 1) % 3;
            field.xI[1] = team;
            field.xT[0] = 10.0f;
            static const char* kGift[3] = {"%s lends its knowledge - defense up!", "%s lends its emotion - attack up!", "%s lends its willpower - speed up!"};
            AddText(c.pos, TextFormat(kGift[g], kGuardian[g]), kGuardianCol[g], 42);
            AddFx(FxKind::Flash, c.pos, 0.15f, kGuardianCol[g], 50, 40);
            PlaySfx(Sfx::Buff);
        } break;
        case Centerpiece::Rift:
            if (team < 0) return;
            c.hitCd = 2.0f;
            field.xI[1] = team;
            field.xT[3] = 10.0f;
            AddText(c.pos, "The rift turns gravity on your foes!", kVoid, 40);
            AddFx(FxKind::Flash, c.pos, 0.15f, kVoid, 60, 40);
            PlaySfx(Sfx::Shield);
            break;
        case Centerpiece::Bell:
            c.hitCd = 1.0f;
            c.spin = 1.0f;
            field.xI[0] = (field.xI[0] + 1) % 3;
            AddText(c.pos, "Ding! Next course - the tables turn!", UI_GOLD, 40);
            PlaySfx(Sfx::MenuOk);
            break;
        case Centerpiece::TrackSwitch:
            c.hitCd = 1.0f;
            if (h.type == Type::Ground) { field.xT[3] = 10.0f; field.xI[3] = -1; AddText(c.pos, "The switch jammed - no trains!", UI_DIM, 36); PlaySfx(Sfx::Thud); return; }
            if (team < 0) return;
            field.xI[3] = team;
            AddText(c.pos, "Switched! The next train runs at your foes!", kVolt, 40);
            PlaySfx(Sfx::Zap);
            break;
        case Centerpiece::Propeller:
            c.hitCd = 1.0f;
            if (field.xT[1] > 0) { AddText(c.pos, "The propeller is stalled...", UI_DIM, 28); return; }
            if (h.type == Type::Electric) { field.xT[1] = 10.0f; field.xT[0] = 0; field.xI[0] = -1; AddText(c.pos, "The propeller stalled!", UI_DIM, 36); PlaySfx(Sfx::Zap); return; }
            if (team < 0) return;
            field.xI[0] = team;
            field.xT[0] = 3.0f;
            AddText(c.pos, "Whoosh! A gust blasts your foes!", Cc(200, 230, 255), 40);
            PlaySfx(Sfx::Shoot);
            break;
        case Centerpiece::Pillars: {
            if (team < 0) return;
            c.hitCd = 1.0f;
            if (field.xT[2] > 0) { AddText(c.pos, "The pillars are still dim...", UI_DIM, 28); return; }
            bool white = dir.x > 0;   // a hit from the left strikes the white pillar
            field.xI[1] = team;
            field.xI[2] = white ? 0 : 1;
            field.xT[1] = 10.0f;
            field.xT[2] = 20.0f;
            AddText(c.pos, white ? "Turboblaze! Fire moves power up!" : "Teravolt! Electric moves power up!", white ? kWhite : kBlue, 42);
            AddFx(FxKind::Flash, c.pos, 0.2f, white ? kWhite : kBlue, 60, 40);
            PlaySfx(Sfx::Buff);
        } break;
        case Centerpiece::Brazier:
            c.hitCd = 1.0f;
            if (h.type == Type::Water) {
                field.xT[3] = 15.0f;
                field.xT[2] = 0;
                field.xI[0] = -1;
                AddText(c.pos, "The great brazier was doused!", kWater, 36);
                for (int k = 0; k < 10; k++) Emit(PK::Smoke, c.pos, Frand(10, 30), {Frand(-20, 20), 0}, Frand(20, 40), 0.8f, 3, Cc(200, 200, 210, 200));
                PlaySfx(Sfx::Water);
                return;
            }
            if (field.xT[3] > 0) { AddText(c.pos, "The brazier is still smoking...", UI_DIM, 28); return; }
            if (team < 0) return;
            field.xI[0] = team;
            field.xT[2] = 10.0f;
            AddText(c.pos, "The great brazier blazes - sunshine on your side!", kFlame, 42);
            AddFx(FxKind::Flash, c.pos, 0.2f, kFlame, 60, 40);
            PlaySfx(Sfx::Fire);
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- Drawing (visual only)

void Battle::DrawGen45Floor() const {
    const float t = time;
    auto ring = [](Vector2 w, float r, Color c, float th = 1.0f) { Vector2 p = ToScreen(w); VRingE(p, r, r * DEPTH, th, c, 32); };
    auto disc = [](Vector2 w, float r, Color c) { Vector2 p = ToScreen(w); DrawEllipse((int)p.x, (int)p.y, r, r * DEPTH, c); };
    auto line = [](Vector2 a, Vector2 b, float th, Color c) { DrawLineEx(ToScreen(a), ToScreen(b), th, c); };
    switch (field.kind) {
        case ArenaKind::Hearthome:
            for (int k = 0; k < (int)field.xMarks.size() && k < 4; k++) {   // shape-marked doors on the walls
                Vector2 d = field.xMarks[k];
                Vector2 p = ToScreen(d);
                bool flick = field.xT[1] > 0 && ((int)(t * 10) % 2);
                DrawRectangle((int)p.x - 9, (int)p.y - 22, 18, 22, Cc(60, 36, 70));
                DrawRectangleLines((int)p.x - 9, (int)p.y - 22, 18, 22, Cc(150, 110, 60));
                Color sc = flick ? WHITE : kDoor;
                Vector2 c{p.x, p.y - 12};
                switch (DoorShape(field.xI[0], k)) {
                    case 0: DrawCircleV(c, 4, sc); break;
                    case 1: DrawTriangle({c.x, c.y - 5}, {c.x - 5, c.y + 4}, {c.x + 5, c.y + 4}, sc); break;
                    case 2: DrawRectangle((int)c.x - 4, (int)c.y - 4, 8, 8, sc); break;
                    default: DrawPoly(c, 4, 5, 45, sc); break;
                }
                disc(d, DOOR_R, Fade(kDoor, 0.18f));
            }
            break;
        case ArenaKind::Pastoria: {
            bool low = field.xI[0] == 0;
            float warn = field.xT[1] > 0 && ((int)(t * 8) % 2) ? 0.5f : 1.0f;
            for (const Zone& z : field.zones) {
                if (z.kind != Zone::Walk) continue;
                Vector2 p = ToScreen(z.pos);
                if (low) {
                    DrawEllipse((int)p.x, (int)p.y, z.rx, z.ry * DEPTH, Fade(Cc(150, 120, 86), warn));
                    for (int k = -2; k <= 2; k++) DrawLine((int)(p.x - z.rx * 0.8f), (int)(p.y + k * z.ry * DEPTH * 0.3f), (int)(p.x + z.rx * 0.8f), (int)(p.y + k * z.ry * DEPTH * 0.3f), Fade(Cc(110, 84, 58), warn));
                    VRingE(p, z.rx, z.ry * DEPTH, 1.0f, Fade(Cc(200, 230, 255), 0.6f), 32);
                } else {
                    DrawEllipse((int)p.x, (int)p.y, z.rx, z.ry * DEPTH, Fade(Cc(80, 110, 120), 0.35f * warn));
                    float u = std::fmod(t * 0.7f + z.pos.x * 0.01f, 1.0f);
                    VRingE(p, z.rx * (0.6f + 0.4f * u), z.ry * DEPTH * (0.6f + 0.4f * u), 0.8f, Fade(WHITE, 0.4f * (1 - u)), 24);
                }
            }
            if (!low) {   // high water: the whole floor shimmers deeper
                Vector2 a = ToScreen({0, 0}), b = ToScreen({ARENA_W, ARENA_H});
                DrawRectangle((int)a.x, (int)a.y, (int)(b.x - a.x), (int)(b.y - a.y), Fade(Cc(30, 70, 150), 0.18f));
            }
        } break;
        case ArenaKind::Sunyshore: {
            if (field.xT[2] > 0 && field.xI[1] >= 0) {   // the sunbeam over the foes' half
                bool right = field.xI[1] == 0;
                Vector2 a = ToScreen({right ? ARENA_W / 2 : 0.0f, 0}), b = ToScreen({right ? ARENA_W : ARENA_W / 2, ARENA_H});
                VAdd();
                DrawRectangle((int)a.x, (int)a.y, (int)(b.x - a.x), (int)(b.y - a.y), Fade(kVolt, 0.14f + 0.04f * std::sin(t * 6)));
                VNorm();
            }
            for (size_t i = 0; i < field.xMarks.size(); i++) {   // gears: a hub, spokes and sparking teeth
                Vector2 g = field.xMarks[i];
                disc(g, GEAR_R, Cc(96, 100, 112));
                disc(g, GEAR_R - 6, Cc(120, 126, 140));
                float ang = field.xT[1] * (i % 2 ? -1.0f : 1.0f);
                for (int k = 0; k < 12; k++) {
                    float a = ang + k * PI_F / 6;
                    Vector2 e = g + Vector2{std::cos(a), std::sin(a)} * GEAR_R;
                    Vector2 m = g + Vector2{std::cos(a), std::sin(a)} * (GEAR_R - 8);
                    line(m, e, 3, Cc(70, 74, 86));
                    if (k % 3 == 0) line(g, m, 2, Cc(90, 94, 106));
                    if (((int)(t * 8) + k) % 6 == 0) { Vector2 sp = ToScreen(e); DrawCircle((int)sp.x, (int)sp.y, 2, kVolt); }
                }
                disc(g, 10, Cc(200, 170, 60));
            }
        } break;
        case ArenaKind::Distortion: {
            if (field.xT[1] > 0 || field.xT[2] > 0) {   // the gravity arrows
                Vector2 d = GravDir(field.xI[0]);
                float a = field.xT[2] > 0 ? 0.6f : (((int)(t * 8) % 2) ? 0.6f : 0.2f);
                for (int k = 0; k < 6; k++) {
                    Vector2 c{ARENA_W * (0.2f + 0.12f * k), ARENA_H * (0.25f + 0.1f * (k % 3) * 2)};
                    float u = std::fmod(t * 1.5f + k * 0.17f, 1.0f);
                    Vector2 p = c + d * (u * 30.0f);
                    Vector2 tip = p + d * 12.0f, l = p + Vector2{-d.y, d.x} * 6.0f, r = p - Vector2{-d.y, d.x} * 6.0f;
                    DrawTriangle(ToScreen(tip), ToScreen(r), ToScreen(l), Fade(kVoid, a * (1 - u)));
                    DrawTriangle(ToScreen(tip), ToScreen(l), ToScreen(r), Fade(kVoid, a * (1 - u)));
                }
            }
            if (field.xI[1] >= 0) {   // the rift's pull on the foes' side
                bool right = field.xI[1] == 0;
                Vector2 a = ToScreen({right ? ARENA_W / 2 : 0.0f, 0}), b = ToScreen({right ? ARENA_W : ARENA_W / 2, ARENA_H});
                DrawRectangle((int)a.x, (int)a.y, (int)(b.x - a.x), (int)(b.y - a.y), Fade(kVoid, 0.08f + 0.04f * std::sin(t * 5)));
            }
        } break;
        case ArenaKind::SpearTime:
            for (const Vector2& l : field.lights) {   // patches of slowed time: clock rings
                disc(l, SLOW_R, Fade(kTime, 0.12f));
                ring(l, SLOW_R, Fade(kTime, 0.6f));
                for (int k = 0; k < 12; k++) {
                    float a = k * PI_F / 6;
                    Vector2 p = ToScreen(l + Vector2{std::cos(a), std::sin(a)} * (SLOW_R - 6));
                    DrawRectangle((int)p.x, (int)p.y, 2, 1, Fade(kTime, 0.8f));
                }
                Vector2 c = ToScreen(l);
                float h = t * 0.2f, m = t * 1.5f;
                DrawLineEx(c, {c.x + std::cos(h) * 14, c.y + std::sin(h) * 14 * DEPTH}, 2, Fade(kTime, 0.7f));
                DrawLineEx(c, {c.x + std::cos(m) * 22, c.y + std::sin(m) * 22 * DEPTH}, 1, Fade(kTime, 0.7f));
            }
            break;
        case ArenaKind::SpearSpace:
            for (const Portal& p : field.portals) {
                float u = std::fmod(t * 0.9f, 1.0f);
                disc(p.pos, 14, Fade(kSpace, 0.25f));
                ring(p.pos, 14, Fade(kSpace, 0.8f), 1.5f);
                ring(p.pos, 6 + 8 * u, Fade(WHITE, 0.5f * (1 - u)));
            }
            if (field.xT[2] > 0 && field.xMarks.size() == 2) {
                float blink = ((int)(t * 10) % 2) ? 0.9f : 0.4f;
                line(field.xMarks[0], field.xMarks[1], 4, Fade(kSpace, 0.2f));
                line(field.xMarks[0], field.xMarks[1], 1, Fade(kSpace, blink));
            }
            break;
        case ArenaKind::HallOrigin: {   // the Plate's ring round the middle
            Color c = TypeColor((Type)field.xI[0]);
            ring({ARENA_W / 2, ARENA_H / 2}, 90, Fade(c, 0.5f + 0.2f * std::sin(t * 3)), 2);
            for (int k = 0; k < 18; k++) {
                float a = t * 0.2f + k * 2 * PI_F / 18;
                Vector2 p = ToScreen(Vector2{ARENA_W / 2, ARENA_H / 2} + Vector2{std::cos(a), std::sin(a)} * 90.0f);
                DrawCircle((int)p.x, (int)p.y, 2, Fade(TypeColor((Type)k), 0.8f));
            }
        } break;
        case ArenaKind::Striaton: {
            int i = 0;
            for (const Zone& z : field.zones) {
                if (z.kind != Zone::Table) continue;
                int idx = i++;
                Type ty = kTable[(idx + field.xI[0]) % 3];
                bool out = field.xT[idx] > 0;
                Vector2 p = ToScreen(z.pos);
                Color base = ty == Type::Fire ? Cc(150, 60, 40) : ty == Type::Water ? Cc(50, 100, 170) : Cc(70, 130, 60);
                if (out) base = Cc(90, 90, 96);
                DrawEllipse((int)p.x, (int)p.y, z.rx, z.ry * DEPTH, Fade(base, 0.75f));
                VRingE(p, z.rx, z.ry * DEPTH, 1.5f, Cc(220, 200, 150), 32);
                if (!out) {
                    for (int k = 0; k < 6; k++) {
                        float a = VRand(300 + idx, k) * 2 * PI_F, r = VRand(310 + idx, k) * 0.7f;
                        Vector2 q{p.x + std::cos(a) * z.rx * r, p.y + std::sin(a) * z.ry * DEPTH * r};
                        float bob = std::sin(t * 4 + k) * 1.5f;
                        if (ty == Type::Fire) DrawTriangle({q.x - 2, q.y}, {q.x + 2, q.y}, {q.x, q.y - 5 - bob}, Cc(255, 180, 60));
                        else if (ty == Type::Water) DrawCircle((int)q.x, (int)(q.y - bob), 1.5f, Cc(180, 220, 255));
                        else DrawEllipse((int)q.x, (int)q.y, 3, 1.5f, Cc(140, 210, 100));
                    }
                }
                DrawTextCentered(out ? "OUT" : (ty == Type::Fire ? "GRILL" : ty == Type::Water ? "FOUNTAIN" : "HERBS"), (int)p.x, (int)(p.y + z.ry * DEPTH + 2), 8, Fade(WHITE, 0.6f), false);
            }
        } break;
        case ArenaKind::Nimbasa: {
            Vector2 a = ToScreen({ARENA_W / 2 - 30, 0}), b = ToScreen({ARENA_W / 2 + 30, ARENA_H});
            DrawRectangle((int)a.x, (int)a.y, (int)(b.x - a.x), (int)(b.y - a.y), Fade(Cc(240, 220, 120), 0.12f));   // the runway
            for (int k = 0; k < 10; k++) {
                Vector2 p = ToScreen({ARENA_W / 2, ARENA_H * (k + 0.5f) / 10});
                float on = ((int)(t * 4) + k) % 4 == 0 ? 1.0f : 0.4f;
                DrawCircle((int)p.x - 26, (int)p.y, 1.5f, Fade(kVolt, on));
                DrawCircle((int)p.x + 26, (int)p.y, 1.5f, Fade(kVolt, on));
            }
            for (int k = 0; k < 2 && field.xMarks.size() >= 4; k++) {   // the two tracks
                Vector2 s0 = field.xMarks[k * 2], s1 = field.xMarks[k * 2 + 1];
                Vector2 n = SafeNorm(Vector2{-(s1.y - s0.y), s1.x - s0.x}, Vector2{0, 1}) * 6.0f;
                bool lit = field.xI[0] == k;
                float blink = lit && field.xT[1] > 0 ? (((int)(t * 10) % 2) ? 1.0f : 0.3f) : 0.0f;
                if (lit) line(s0, s1, 10, Fade(Cc(255, 80, 60), 0.25f * (blink > 0 ? blink : 1)));
                line(s0 + n, s1 + n, 1.5f, Cc(150, 150, 165));
                line(s0 - n, s1 - n, 1.5f, Cc(150, 150, 165));
                for (int q = 0; q <= 24; q++) { Vector2 m = s0 + (s1 - s0) * (q / 24.0f); line(m + n * 1.4f, m - n * 1.4f, 1, Cc(110, 90, 70)); }
            }
        } break;
        case ArenaKind::Mistralton:
            for (const Vector2& pad : field.xMarks) {
                disc(pad, PAD_R + 2, Cc(70, 74, 90));
                disc(pad, PAD_R, Cc(200, 120, 60));
                ring(pad, PAD_R - 5 + 3 * std::sin(t * 4), Cc(255, 220, 140));
                Vector2 to = SafeNorm(Vector2{ARENA_W / 2, ARENA_H / 2} - pad, {1, 0});
                line(pad, pad + to * 12.0f, 2, Cc(255, 240, 200));
            }
            if (field.xT[0] > 0 && field.xI[0] >= 0) {   // the gust's streaks
                bool right = field.xI[0] == 0;
                for (int k = 0; k < 24; k++) {
                    float x = (right ? ARENA_W / 2 : 0) + std::fmod(VRand(400, k) * ARENA_W / 2 + t * 180.0f * (right ? 1 : -1) + ARENA_W, ARENA_W / 2);
                    float y = VRand(401, k) * ARENA_H;
                    line({x, y}, {x + (right ? 14.0f : -14.0f), y}, 1, Fade(WHITE, 0.4f));
                }
            }
            break;
        case ArenaKind::UnovaChampion: {
            for (const Vector2& b : field.xMarks) {
                float warn = field.xT[1] > 0 ? (((int)(t * 8) % 2) ? 0.6f : 0.2f) : 0.0f;
                disc(b, BRAZIER_R, Fade(kFlame, 0.1f + warn * 0.4f));
                ring(b, BRAZIER_R, Fade(kFlame, 0.5f + warn));
                Vector2 p = ToScreen(b);
                DrawRectangle((int)p.x - 6, (int)p.y - 10, 12, 10, Cc(110, 80, 50));
                for (int k = 0; k < 3; k++) {
                    float fx = p.x - 4 + k * 4 + std::sin(t * 9 + k) * 1.2f;
                    DrawTriangle({fx - 2.5f, p.y - 10}, {fx + 2.5f, p.y - 10}, {fx, p.y - 18 - std::fabs(std::sin(t * 7 + k)) * 4}, Cc(255, 170, 60));
                }
            }
            if (field.xT[2] > 0 && field.xI[0] >= 0) {
                bool left = field.xI[0] == 0;
                Vector2 a = ToScreen({left ? 0.0f : ARENA_W / 2, 0}), c = ToScreen({left ? ARENA_W / 2 : ARENA_W, ARENA_H});
                VAdd();
                DrawRectangle((int)a.x, (int)a.y, (int)(c.x - a.x), (int)(c.y - a.y), Fade(kFlame, 0.12f));
                VNorm();
            }
        } break;
        default: break;
    }
}

void Battle::DrawGen45Top() const {
    const float t = time;
    switch (field.kind) {
        case ArenaKind::Hearthome:
            if (field.xT[2] > 0) {   // lit: the wisps show as plain flames
                for (const Wisp& w : field.wisps) {
                    if (w.life <= 0) continue;
                    Vector2 c = ToScreen(w.pos, 12 + std::sin(t * 4 + (w.seed % 11)) * 2);
                    DrawTriangle({c.x - 3, c.y + 2}, {c.x + 3, c.y + 2}, {c.x, c.y - 7}, Cc(150, 120, 255));
                }
            }
            break;
        case ArenaKind::Newmoon:
            for (const Wisp& w : field.wisps) {   // Dark Void clouds
                if (w.life <= 0) continue;
                Vector2 c = ToScreen(w.pos, 10);
                float fade = std::min(1.0f, w.life);
                for (int k = 0; k < 5; k++) {
                    float a = t * 1.5f + k * 1.26f;
                    DrawCircleV({c.x + std::cos(a) * 6, c.y + std::sin(a) * 3}, 5, Fade(Cc(40, 10, 60), 0.75f * fade));
                }
                DrawCircleV(c, 3, Fade(Cc(200, 60, 120), fade));
            }
            break;
        case ArenaKind::Nimbasa:
            if (field.xI[0] >= 0 && field.xT[1] <= 0 && field.xMarks.size() >= 4) {   // the train: a lit engine and three cars
                Vector2 a = field.xMarks[field.xI[0] * 2], b = field.xMarks[field.xI[0] * 2 + 1];
                if (field.xI[2] < 0) std::swap(a, b);
                Vector2 d = SafeNorm(b - a, Vector2{1, 0});
                Vector2 head = a + (b - a) * Clamp01(field.xT[2]);
                for (int k = 3; k >= 0; k--) {
                    Vector2 p = ToScreen(head - d * (k * 18.0f), 8);
                    Color col = k == 0 ? Cc(250, 220, 80) : (k % 2 ? Cc(230, 70, 90) : Cc(70, 150, 230));
                    DrawRectangle((int)p.x - 7, (int)p.y - 6, 14, 9, col);
                    DrawRectangle((int)p.x - 6, (int)p.y - 5, 12, 2, Fade(WHITE, 0.6f));
                }
                VAdd();
                VGlow(ToScreen(head, 8), 26, kVolt, 0.4f);
                VNorm();
            }
            break;
        default: break;
    }
}

void Battle::DrawGen45CenterBody(const Obstacle& o) const {
    const Centerpiece& c = field.center;
    Vector2 g = ToScreen(o.pos);
    float fl = c.flash > 0 ? 1.0f : 0.0f;
    const float t = time;
    switch (c.kind) {
        case Centerpiece::Chandelier: {
            DrawEllipse((int)g.x, (int)g.y, 14, 6, Fade(BLACK, 0.35f));
            if (c.phase == 1) {   // fallen: broken glass on the floor
                for (int k = 0; k < 6; k++) DrawRectangle((int)(g.x + std::cos(k * 1.1f) * 10), (int)(g.y + std::sin(k * 1.1f) * 4), 2, 2, Cc(220, 200, 150));
                break;
            }
            DrawLine((int)g.x, (int)g.y - 70, (int)g.x, (int)g.y - 46, Cc(140, 120, 80));
            DrawEllipse((int)g.x, (int)g.y - 44, 16, 5, Cc(200, 170, 80));
            bool lit = field.xT[2] > 0;
            for (int k = 0; k < 5; k++) {
                float x = g.x - 12 + k * 6;
                DrawRectangle((int)x - 1, (int)g.y - 52, 2, 6, Cc(240, 230, 210));
                if (lit || k % 2 == 0) DrawCircle((int)x, (int)g.y - 54, 1.5f, kGhostLight);
            }
            if (lit || fl > 0) { VAdd(); DrawCircleGradient((int)g.x, (int)g.y - 48, 40, Fade(kGhostLight, 0.35f), Fade(kGhostLight, 0)); VNorm(); }
        } break;
        case Centerpiece::Floodgate: {
            DrawEllipse((int)g.x, (int)g.y, 16, 7, Cc(110, 100, 90));
            DrawRectangle((int)g.x - 7, (int)g.y - 22, 14, 20, Cc(90, 96, 110));
            bool high = field.xI[0] == 1, jam = field.xT[2] > 0;
            DrawRectangle((int)g.x - 4, (int)g.y - 19, 8, 8, jam ? Cc(120, 120, 120) : high ? Cc(80, 160, 255) : Cc(240, 200, 80));
            DrawLineEx({g.x, g.y - 22}, {g.x + (high ? 8.0f : -8.0f), g.y - 32}, 2, Cc(200, 60, 50));
            if (jam && ((int)(t * 10) % 2)) DrawCircle((int)g.x + 3, (int)g.y - 26, 2, kVolt);
            if (fl > 0) DrawCircleGradient((int)g.x, (int)g.y - 16, 22, Fade(WHITE, 0.5f), Fade(WHITE, 0));
        } break;
        case Centerpiece::SolarPanel: {
            DrawEllipse((int)g.x, (int)g.y, 18, 7, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 2, (int)g.y - 18, 4, 18, Cc(120, 120, 130));
            float ch = Clamp01(c.charge / 100.0f);
            Vector2 a{g.x - 18, g.y - 30}, b{g.x + 18, g.y - 30}, d{g.x + 14, g.y - 18}, e{g.x - 14, g.y - 18};
            DrawTriangle(a, e, d, Cc(40, 60, 110));
            DrawTriangle(a, d, b, Cc(50, 74, 130));
            for (int k = 1; k < 4; k++) DrawLineEx({a.x + k * 9.0f, a.y}, {e.x + k * 7.0f, e.y}, 1, Cc(120, 150, 200));
            DrawRectangle((int)g.x - 14, (int)g.y - 36, (int)(28 * ch), 3, ch >= 1 ? kVolt : Cc(200, 180, 80));
            if (ch >= 1 || fl > 0) { VAdd(); DrawCircleGradient((int)g.x, (int)g.y - 26, 30, Fade(kVolt, 0.3f), Fade(kVolt, 0)); VNorm(); }
        } break;
        case Centerpiece::Mural: {   // a carved stone slab with the three lake guardians
            DrawEllipse((int)g.x, (int)g.y, 20, 7, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 18, (int)g.y - 40, 36, 40, Cc(150, 140, 120));
            DrawRectangleLines((int)g.x - 18, (int)g.y - 40, 36, 40, Cc(100, 90, 74));
            for (int k = 0; k < 3; k++) {
                bool on = field.xI[1] >= 0 && field.xT[0] > 0 && field.xI[2] == k;
                bool next = field.xI[0] % 3 == k;
                Color col = on ? kGuardianCol[k] : next ? ColorBrightness(kGuardianCol[k], -0.3f) : Cc(110, 100, 86);
                DrawCircle((int)g.x - 10 + k * 10, (int)g.y - 22, 4, col);
            }
            if (fl > 0) DrawCircleGradient((int)g.x, (int)g.y - 22, 26, Fade(WHITE, 0.5f), Fade(WHITE, 0));
        } break;
        case Centerpiece::Rift: {   // a tear in the air
            Vector2 cc{g.x, g.y - 30};
            VAdd();
            DrawCircleGradient((int)cc.x, (int)cc.y, 22, Fade(kVoid, 0.4f), Fade(kVoid, 0));
            VNorm();
            for (int k = 0; k < 8; k++) {
                float a = c.spin + k * PI_F / 4;
                DrawLineEx(cc, {cc.x + std::cos(a) * 12, cc.y + std::sin(a) * 16}, 1.5f, Fade(k % 2 ? kVoid : Cc(40, 10, 60), 0.9f));
            }
            DrawEllipse((int)cc.x, (int)cc.y, 4, 10, Cc(20, 0, 30));
        } break;
        case Centerpiece::Bell: {
            DrawEllipse((int)g.x, (int)g.y, 14, 6, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 10, (int)g.y - 10, 20, 10, Cc(120, 70, 50));   // a little serving table
            float sw = std::sin(t * 30) * c.spin * 4;
            DrawTriangle({g.x + sw, g.y - 30}, {g.x - 8 + sw, g.y - 12}, {g.x + 8 + sw, g.y - 12}, Cc(230, 190, 80));
            DrawCircle((int)(g.x + sw), (int)g.y - 31, 2, Cc(250, 220, 120));
            if (fl > 0) DrawCircleGradient((int)g.x, (int)g.y - 20, 20, Fade(WHITE, 0.5f), Fade(WHITE, 0));
        } break;
        case Centerpiece::TrackSwitch: {
            DrawEllipse((int)g.x, (int)g.y, 12, 5, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 6, (int)g.y - 10, 12, 10, Cc(80, 80, 90));
            bool jam = field.xT[3] > 0;
            float lean = field.xI[3] == 0 ? -1.0f : field.xI[3] == 1 ? 1.0f : 0.0f;
            DrawLineEx({g.x, g.y - 8}, {g.x + lean * 8, g.y - 28}, 2, Cc(180, 180, 190));
            DrawCircle((int)(g.x + lean * 8), (int)g.y - 29, 3, jam ? Cc(120, 120, 120) : Cc(230, 60, 60));
            if (fl > 0) DrawCircleGradient((int)g.x, (int)g.y - 18, 18, Fade(WHITE, 0.5f), Fade(WHITE, 0));
        } break;
        case Centerpiece::Propeller: {
            DrawEllipse((int)g.x, (int)g.y, 16, 6, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 3, (int)g.y - 30, 6, 30, Cc(130, 130, 140));
            Vector2 hub{g.x, g.y - 32};
            for (int k = 0; k < 3; k++) {
                float a = c.spin + k * 2 * PI_F / 3;
                DrawLineEx(hub, {hub.x + std::cos(a) * 16, hub.y + std::sin(a) * 16}, 4, field.xT[1] > 0 ? Cc(110, 110, 110) : Cc(200, 70, 60));
            }
            DrawCircle((int)hub.x, (int)hub.y, 3, Cc(220, 220, 230));
        } break;
        case Centerpiece::Pillars: {   // a white and a blue pillar flanking the throne
            DrawEllipse((int)g.x, (int)g.y, 22, 8, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 6, (int)g.y - 22, 12, 16, Cc(200, 170, 70));   // the throne
            DrawRectangle((int)g.x - 6, (int)g.y - 30, 12, 8, Cc(170, 140, 60));
            bool dim = field.xT[2] > 0 && field.xT[1] <= 0;
            for (int k = 0; k < 2; k++) {
                Color col = k == 0 ? kWhite : kBlue;
                if (dim) col = ColorBrightness(col, -0.4f);
                int x = (int)g.x + (k == 0 ? -16 : 12);
                DrawRectangle(x, (int)g.y - 44, 5, 42, col);
                bool on = field.xT[1] > 0 && field.xI[2] == k;
                if (on) { VAdd(); DrawCircleGradient(x + 2, (int)g.y - 44, 18, Fade(col, 0.5f), Fade(col, 0)); VNorm(); }
            }
        } break;
        case Centerpiece::Brazier: {
            DrawEllipse((int)g.x, (int)g.y, 18, 7, Fade(BLACK, 0.35f));
            DrawRectangle((int)g.x - 4, (int)g.y - 16, 8, 16, Cc(100, 76, 50));
            DrawEllipse((int)g.x, (int)g.y - 18, 16, 6, Cc(150, 110, 60));
            bool doused = field.xT[3] > 0, sun = field.xT[2] > 0;
            if (!doused)
                for (int k = 0; k < 5; k++) {
                    float fx = g.x - 10 + k * 5 + std::sin(t * 8 + k) * 1.5f;
                    float h = (sun ? 22 : 12) + std::fabs(std::sin(t * 6 + k * 1.3f)) * 6;
                    DrawTriangle({fx - 3, g.y - 18}, {fx + 3, g.y - 18}, {fx, g.y - 18 - h}, k % 2 ? Cc(255, 200, 80) : kFlame);
                }
            if (sun || fl > 0) { VAdd(); DrawCircleGradient((int)g.x, (int)g.y - 26, 36, Fade(kFlame, 0.35f), Fade(kFlame, 0)); VNorm(); }
        } break;
        default: break;
    }
}
