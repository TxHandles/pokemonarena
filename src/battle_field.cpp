// Gym arena mechanics: obstacles, zones, portals, turrets, lava and electrified water.
#include <algorithm>
#include <cmath>
#include "rng.h"
#include "arena.h"
#include "audio.h"
#include "battle.h"
#include "ui.h"
#include "vec.h"

namespace {

constexpr float BODY_R = 9.0f;
constexpr float PI_F = 3.14159265f;

float Frand(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }
bool Chance(float p) { return p > 0 && Frand(0, 1) < p; }
bool HasType(const SpeciesDef* sp, Type t) { return sp->type1 == t || sp->type2 == t; }

const Color kYellow{250, 225, 70, 255};
const Color kPink{230, 120, 240, 255};

} // namespace

// ---------------------------------------------------------------- Per-frame field update

void Battle::UpdateField(float dt) {
    field.electrifyCd = std::max(0.0f, field.electrifyCd - dt);
    field.waterFlash = std::max(0.0f, field.waterFlash - dt);
    // Barriers made by Wall moves fade away.
    for (Obstacle& o : field.obs) {
        if (o.kind != Obstacle::TempWall) continue;
        o.life -= dt;
        if (o.life <= 0) Burst(o.pos, TypeColor(o.wallType), 6, 30, 8);
    }
    field.obs.erase(std::remove_if(field.obs.begin(), field.obs.end(),
                                   [](const Obstacle& o) { return o.kind == Obstacle::TempWall && o.life <= 0; }),
                    field.obs.end());
    for (Obstacle& o : field.obs) {
        if (o.alive && o.burnT > 0) {
            o.burnT -= dt;
            if (SimRand(0, 1)) {
                Particle p;
                p.pos = o.pos + Vector2{Frand(-7, 7), Frand(-3, 3)};
                p.z = Frand(14, o.height + 4);
                p.vz = Frand(20, 45);
                p.life = p.maxLife = 0.5f;
                p.col = SimRand(0, 2) ? Color{255, 140, 40, 255} : Color{255, 220, 90, 255};
                p.size = 2;
                parts.push_back(p);
            }
            if (o.burnT <= 0) {
                o.burnT = 0;
                o.fallT = 0;
                o.fallDir = Vector2{SimRand(0, 1) ? 1.0f : -1.0f, Frand(-0.4f, 0.4f)};
                o.fallDir = SafeNorm(o.fallDir);
                AddText(o.pos, "Timber!", Color{255, 170, 80, 255}, 44);
            }
        }
        if (o.alive && o.fallT >= 0) {
            float before = o.fallT;
            o.fallT += dt;
            if (before < 0.45f && o.fallT >= 0.45f) {  // the trunk hits the ground
                Vector2 a = o.pos, b = o.pos + o.fallDir * o.height;
                HitSpec h = EnvSpec(Type::Grass, 50);
                for (int ts = 0; ts < N(); ts++)
                    if (Hittable(ts) && DistPointSeg(sides[ts].Cur().pos, a, b) < BODY_R + 6) ApplyHit(h, ts, o.fallDir);
                Burst(b, Color{90, 160, 60, 255}, 16, 70, 4);
                Shake(2);
                PlaySfx(Sfx::Thud);
            }
            if (o.fallT >= 0.7f) { o.alive = false; o.fallT = -1; }
        }
        if (!o.alive && o.kind == Obstacle::Pillar && field.kind == ArenaKind::Rock) {   // the Stone Gym's pillars grow back
            o.respawnT -= dt;
            bool clear = true;
            for (int s = 0; s < N(); s++)
                if (Dist(sides[s].Cur().pos, o.pos) < o.radius + BODY_R + 4) clear = false;
            if (o.respawnT <= 0 && clear) {
                o.alive = true;
                Burst(o.pos, Color{150, 130, 110, 255}, 14, 50, 4);
                AddText(o.pos, "The pillar rises again!", UI_DIM, 36);
            }
        }
    }

    field.obs.erase(std::remove_if(field.obs.begin(), field.obs.end(),
                                   [](const Obstacle& o) { return o.kind == Obstacle::Crate && !o.alive; }),
                    field.obs.end());
    if (field.kind == ArenaKind::Stadium) {
        for (int s = 0; s < N(); s++) { sides[s].Cur().envSpeed = 1; sides[s].Cur().envAtk = 1; }
        return;
    }

    if (field.kind == ArenaKind::Psychic) {
        for (Portal& po : field.portals) po.age += dt;
        field.portalT -= dt;
        if (field.portalT <= 0) {
            for (const Portal& po : field.portals) Burst(po.pos, kPink, 10, 50, 6);
            field.PlacePortals();
            for (const Portal& po : field.portals) Burst(po.pos, kPink, 10, 50, 6);
            PlaySfx(Sfx::Buff);
        }
    }

    if (field.kind == ArenaKind::Fire && winner < 0 && introT <= 0) {
        field.lavaT -= dt;
        std::vector<const Zone*> pools;
        for (const Zone& z : field.zones)
            if (z.kind == Zone::LavaPool) pools.push_back(&z);
        if (field.lavaT <= 0 && !pools.empty()) {
            field.lavaT = Frand(3.0f, 5.0f);
            const Zone& z = *pools[SimRand(0, (int)pools.size() - 1)];
            Vector2 target;
            int ts = SimRand(0, N() - 1);
            if (Chance(0.6f) && Targetable(ts)) target = sides[ts].Cur().pos + Vector2{Frand(-18, 18), Frand(-18, 18)};
            else target = z.pos + Rot(Vector2{Frand(50, 140), 0}, Frand(0, 2 * PI_F));
            target.x = std::max(10.0f, std::min(ARENA_W - 10, target.x));
            target.y = std::max(10.0f, std::min(ARENA_H - 10, target.y));
            Projectile p;
            p.hit = EnvSpec(Type::Fire, 45, Status::Burn, 0.25f);
            p.hit.special = true;
            p.lob = true;
            p.start = p.pos = z.pos;
            p.target = target;
            p.dur = 1.1f + Dist(z.pos, target) / 500.0f;
            p.radius = 14;
            projs.push_back(p);
            Burst(z.pos, Color{255, 150, 40, 255}, 12, 60, 2);
            PlaySfx(Sfx::Fire);
        }
        for (const Zone* z : pools) {  // bubbling
            if (SimRand(0, 5) == 0) {
                Particle p;
                p.pos = z->pos + Vector2{Frand(-z->rx, z->rx) * 0.7f, Frand(-z->ry, z->ry) * 0.7f};
                p.vz = Frand(15, 40);
                p.grav = true;
                p.life = p.maxLife = 0.5f;
                p.col = SimRand(0, 1) ? Color{255, 200, 80, 255} : Color{255, 120, 40, 255};
                p.size = 2;
                parts.push_back(p);
            }
        }
    }

    if (field.kind == ArenaKind::Ground && !field.turrets.empty() && winner < 0 && introT <= 0) {
        field.turretT -= dt;
        if (field.turretT <= 0) {
            field.turretT = 10.0f;
            int n = (int)field.turrets.size();
            int a = SimRand(0, n - 1), b = (a + 1 + SimRand(0, n - 2)) % n;
            int pick[2] = {a, b};
            for (int k = 0; k < 2; k++) {
                Turret& t = field.turrets[pick[k]];
                t.warnT = 1.5f;
                int ts = -1;  // one cannon at each team
                for (int q = 0; q < N(); q++)
                    if (sides[q].teamId == k && Targetable(q) && (ts < 0 || SimRand(0, 1))) ts = q;
                if (ts < 0) ts = SimRand(0, N() - 1);
                t.aimAt = Targetable(ts) ? sides[ts].Cur().pos : Vector2{ARENA_W / 2, ARENA_H / 2};
            }
            AddText({ARENA_W / 2, ARENA_H * 0.1f}, "WARNING: wall cannons!", Color{255, 90, 80, 255}, 20);
            PlaySfx(Sfx::Status);
        }
        for (Turret& t : field.turrets) {
            if (t.warnT < 0) continue;
            t.warnT -= dt;
            if (t.warnT <= 0) {
                t.warnT = -1;
                Projectile p;
                p.hit = EnvSpec(Type::Normal, 60);
                p.hit.knock = 1.5f;
                Vector2 dir = SafeNorm(t.aimAt - t.pos, t.dir);
                p.pos = t.pos + dir * 6.0f;
                p.vel = dir * 300.0f;
                p.maxDist = 900;
                p.radius = 5;
                p.z = 10;
                projs.push_back(p);
                Burst(p.pos, Color{255, 200, 120, 255}, 10, 60, 10);
                Shake(1);
                PlaySfx(Sfx::Shoot);
            }
        }
    }

    for (int s = 0; s < N(); s++) FieldEffectsOn(s, dt);
}

void Battle::FieldEffectsOn(int s, float dt) {
    Fighter& f = sides[s].Cur();
    f.envSpeed = 1;
    f.envAtk = 1;
    f.portalCd = std::max(0.0f, f.portalCd - dt);
    if (f.state != FState::Active || f.burrowed || f.hp <= 0) return;
    const SpeciesDef* sp = f.sp;
    bool air = IsAirborne(*sp) || f.jumpT > 0;   // a jump clears the water, puddles and pools
    f.envTickT -= dt;
    bool tick = false;
    if (f.envTickT <= 0) { f.envTickT = 0.5f; tick = true; }
    int os = AnyFoe(s);

    switch (field.kind) {
        case ArenaKind::Water:
            if (!air && field.IsWater(f.pos)) {
                f.envSpeed = HasType(sp, Type::Water) ? 1.35f : 0.6f;
                if (f.moving && SimRand(0, 3) == 0) {
                    Particle p;
                    p.pos = f.pos + Vector2{Frand(-6, 6), Frand(-2, 2)};
                    p.vz = Frand(10, 30);
                    p.grav = true;
                    p.life = p.maxLife = 0.35f;
                    p.col = Color{170, 210, 255, 255};
                    p.size = 1;
                    parts.push_back(p);
                }
            }
            break;
        case ArenaKind::Electric: {
            float best = 1e9f;
            for (const Obstacle& o : field.obs)
                if (o.kind == Obstacle::Coil && o.alive) best = std::min(best, Dist(o.pos, f.pos));
            if (HasType(sp, Type::Electric)) {
                if (best < 60) {
                    f.envAtk = 1.3f;
                    f.envSpeed = 1.25f;
                    if (SimRand(0, 4) == 0) {
                        Particle p;
                        p.pos = f.pos + Vector2{Frand(-8, 8), Frand(-3, 3)};
                        p.z = Frand(2, 16);
                        p.vz = 30;
                        p.life = p.maxLife = 0.3f;
                        p.col = kYellow;
                        p.size = 1;
                        parts.push_back(p);
                    }
                }
            } else if (best < 48 && tick) {
                float eff = TypeEffectiveness(Type::Electric, sp->type1, sp->type2);
                if (eff > 0) DirectDamage(s, f.maxHp * 0.02f * std::min(eff, 2.0f), kYellow, os);
            }
        } break;
        case ArenaKind::Grass:
            if (HasType(sp, Type::Grass) && field.InZone(f.pos, Zone::TallGrass)) {
                f.envAtk = 1.3f;
                if (f.hp < f.maxHp) {
                    f.regenAcc += f.maxHp * 0.008f * dt;
                    if (f.regenAcc >= 1) {
                        float a = std::floor(f.regenAcc);
                        f.hp = std::min((float)f.maxHp, f.hp + a);
                        f.regenAcc -= a;
                    }
                }
                if (SimRand(0, 6) == 0) {
                    Particle p;
                    p.pos = f.pos + Vector2{Frand(-7, 7), Frand(-3, 3)};
                    p.z = Frand(0, 6);
                    p.vz = Frand(20, 35);
                    p.life = p.maxLife = 0.6f;
                    p.col = Color{140, 230, 110, 255};
                    p.size = 1;
                    parts.push_back(p);
                }
            }
            break;
        case ArenaKind::Poison:
            if (!HasType(sp, Type::Poison) && !air) {
                if (tick && field.InZone(f.pos, Zone::PoisonPool)) {
                    DirectDamage(s, f.maxHp * 0.01f, Color{190, 90, 220, 255}, os);
                    if (f.state == FState::Active && Chance(0.12f)) TryStatus(s, Status::Poison, os);
                }
                if (field.InSpikes(f.pos) && f.state == FState::Active) {
                    Vector2 in = SafeNorm(Vector2{ARENA_W / 2, ARENA_H / 2} - f.pos);
                    f.pos += in * (40.0f * dt);
                    if (tick) {
                        DirectDamage(s, f.maxHp * 0.025f, Color{220, 140, 240, 255}, os);
                        f.knock += in * 90.0f;
                    }
                }
            }
            break;
        case ArenaKind::Fire:
            if (!HasType(sp, Type::Fire) && !air && field.InZone(f.pos, Zone::LavaPool)) {
                f.envSpeed = 0.8f;
                if (tick) {
                    DirectDamage(s, f.maxHp * 0.02f, Color{255, 140, 40, 255}, os);
                    if (f.state == FState::Active && Chance(0.15f)) TryStatus(s, Status::Burn, os);
                }
            }
            break;
        case ArenaKind::Psychic:
            if (f.portalCd <= 0 && field.portals.size() > 1 && !f.burrowed) {
                for (size_t k = 0; k < field.portals.size(); k++) {
                    if (Dist(f.pos, field.portals[k].pos) > 13) continue;
                    size_t j = (k + 1 + SimRand(0, (int)field.portals.size() - 2)) % field.portals.size();
                    Vector2 out = SafeNorm(f.moveVel, f.aim);
                    Burst(f.pos, kPink, 12, 60, 6);
                    f.pos = field.portals[j].pos + out * 20.0f;
                    f.dashing = false;
                    f.portalCd = 1.2f;
                    Burst(f.pos, kPink, 12, 60, 6);
                    PlaySfx(Sfx::Shield);
                    break;
                }
            }
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- Obstacles

void Battle::ObstacleImpact(Obstacle& o, const HitSpec& h, Vector2 dir, bool physical) {
    if (!o.Blocks()) return;
    if (h.type == Type::Fire && o.kind == Obstacle::Tree && o.burnT <= 0) {
        o.burnT = 3.0f;
        AddText(o.pos, "The tree caught fire!", Color{255, 150, 60, 255}, 40);
        PlaySfx(Sfx::Fire);
    }
    if (o.kind == Obstacle::Crate) {   // wooden crates: smashed by hits, fire burns them fast
        if (h.power <= 0) return;
        bool fire = h.type == Type::Fire;
        o.hp -= std::max(15.0f, h.power * (fire ? 3.0f : 1.0f) * (physical || h.heavy ? 1.4f : 1.0f));
        for (int k = 0; k < 6; k++)
            Emit(fire ? PK::Ember : PK::Debris, o.pos + Vector2{Frand(-o.radius, o.radius), Frand(-3, 3)}, Frand(4, o.height), Rot(Vector2{1, 0}, Frand(0, 2 * PI_F)) * Frand(25, 60),
                 Frand(15, 45), Frand(0.4f, 0.8f), Frand(1.5f, 2.5f), fire ? Color{255, 150, 50, 255} : Color{170, 120, 70, 255}, true);
        if (o.hp <= 0) {
            o.alive = false;
            Burst(o.pos, Color{170, 120, 70, 255}, 16, 70, 8);
            AddText(o.pos, fire ? "The crate burned up!" : "Smashed the crate!", UI_TEXT, 36);
            PlaySfx(Sfx::Thud);
        }
        return;
    }
    if (o.kind == Obstacle::SnowMound) {   // snow: knocked apart by hits, melted by fire
        if (h.power <= 0) return;
        bool fire = h.type == Type::Fire;
        o.hp -= std::max(15.0f, h.power * (fire ? 4.0f : 1.0f) * (physical ? 1.25f : 1.0f));
        for (int k = 0; k < 8; k++)
            Emit(fire ? PK::Smoke : PK::Snow, o.pos + Vector2{Frand(-o.radius, o.radius), Frand(-3, 3)}, Frand(4, o.height), Rot(Vector2{1, 0}, Frand(0, 2 * PI_F)) * Frand(20, 50),
                 Frand(10, 40), Frand(0.4f, 0.8f), Frand(1.5f, 3), fire ? Color{230, 235, 245, 200} : Color{245, 250, 255, 255}, !fire);
        if (o.hp <= 0) {
            AddText(o.pos, fire ? "The snow melted!" : "Smashed the snow mound!", fire ? Color{255, 170, 90, 255} : UI_TEXT, 36);
            MoundGone(o, fire);
            PlaySfx(Sfx::Thud);
        }
        return;
    }
    if (!h.heavy || h.power <= 0) return;
    if (o.kind == Obstacle::Tree && (physical || h.power >= 60)) {
        o.fallT = 0;
        o.burnT = 0;
        o.fallDir = SafeNorm(dir);
        Burst(o.pos, Color{90, 160, 60, 255}, 16, 60, 20);
        AddText(o.pos, "Smashed the tree!", UI_TEXT, 40);
        Shake(2);
        PlaySfx(Sfx::Thud);
    } else if (o.kind == Obstacle::Pillar && physical) {
        o.alive = false;
        o.respawnT = 20.0f;
        HitSpec t = h;
        t.type = Type::Rock;
        t.special = false;
        t.power = 70;
        t.status = Status::None;
        t.statusChance = 0;
        t.lowers = false;
        t.drain = t.recoil = 0;
        t.patchPower = 0;
        t.hex = false;
        t.light = false;
        t.knock = 1.4f;
        t.name = "Thrown Pillar";
        Projectile p;
        p.hit = t;
        p.pos = o.pos + dir * 4.0f;
        p.vel = dir * 260.0f;
        p.maxDist = 420;
        p.radius = 9;
        p.z = 14;
        p.thrown = true;
        projs.push_back(p);
        Burst(o.pos, Color{150, 130, 110, 255}, 18, 70, 10);
        AddText(o.pos, "Pillar thrown!", UI_GOLD, 40);
        Shake(3);
        PlaySfx(Sfx::Thud);
    }
}

void Battle::ObstaclesInArea(Vector2 c, float r, const HitSpec& h, bool physical, Vector2 fromPos) {
    for (Obstacle& o : field.obs)
        if (o.Blocks() && Dist(c, o.pos) < r + o.radius) ObstacleImpact(o, h, SafeNorm(o.pos - fromPos, Vector2{1, 0}), physical);
}

float Battle::BeamLength(Vector2 from, Vector2 dir, float range, const HitSpec& h) {
    float best = range;
    Obstacle* hit = nullptr;
    for (Obstacle& o : field.obs) {
        if (!o.Blocks()) continue;
        float t = Dot(o.pos - from, dir);
        if (t <= 0 || t > best + o.radius) continue;
        float lat = Len(o.pos - (from + dir * t));
        if (lat >= o.radius) continue;
        float th = t - std::sqrt(o.radius * o.radius - lat * lat);
        if (th < best) { best = std::max(4.0f, th); hit = &o; }
    }
    if (hit) {
        Burst(from + dir * best, TypeColor(h.type), 8, 50, 10);
        ObstacleImpact(*hit, h, dir, false);
    }
    return best;
}

void Battle::TryElectrify(Vector2 at, const HitSpec& h) { FieldReact(at, h, 0); }

// A move landed (or passed) here: the field reacts the way Pokemon types would.
//   Electric: water conducts - puddles and the Water Gym's pool shock everyone standing in them.
//   Ice: puddles and the Water Gym's pool freeze into slippery ice.
//   Fire: ice melts, puddles boil away.     Water: splashes leave a small puddle.
void Battle::FieldReact(Vector2 at, const HitSpec& h, float radius) {
    if (h.power <= 0) return;
    switch (h.type) {
        case Type::Electric: {
            for (Puddle& pd : field.puddles)
                if (pd.ice <= 0 && pd.shockCd <= 0 && pd.Contains(at, radius + 4)) ShockPuddle(pd, h);
            if (field.kind != ArenaKind::Water || field.electrifyCd > 0 || !field.IsWater(at) || OnIce(at)) break;
            field.electrifyCd = 1.2f;
            field.waterFlash = 0.5f;
            AddText(at, "The water is electrified!", kYellow, 30);
            PlaySfx(Sfx::Zap);
            Shake(2);
            for (int ts = 0; ts < N(); ts++) {
                if (!Hittable(ts)) continue;
                Fighter& t = sides[ts].Cur();
                if (IsAirborne(*t.sp) || t.jumpT > 0 || !field.IsWater(t.pos) || OnIce(t.pos) || t.shieldT > 0) continue;
                float eff = TypeEffectiveness(Type::Electric, t.sp->type1, t.sp->type2);
                if (eff <= 0) { AddText(t.pos, "No effect", UI_DIM); continue; }
                Fx fx;
                fx.kind = FxKind::Bolt;
                fx.maxLife = 0.3f;
                fx.col = kYellow;
                Vector2 a = t.pos + Vector2{Frand(-14, 14), Frand(-8, 8)};
                for (int i = 0; i <= 4; i++) fx.pts.push_back(LerpV(a, t.pos, i / 4.0f) + Vector2{Frand(-4, 4), Frand(-4, 4)});
                fxs.push_back(fx);
                bool electric = HasType(t.sp, Type::Electric);
                DirectDamage(ts, t.maxHp * 0.07f * eff, kYellow, h.side >= 0 && Foes(h.side, ts) ? h.side : AnyFoe(ts));
                if (!electric && t.state == FState::Active) {
                    t.st.paraStunT = std::max(t.st.paraStunT, 0.7f);
                    t.pendingMove = -1;
                    t.dashing = false;
                }
            }
        } break;
        case Type::Ice: {
            bool froze = false;
            for (Puddle& pd : field.puddles) {
                if (!pd.Contains(at, radius + 6)) continue;
                if (pd.ice <= 0) froze = true;
                pd.ice = std::max(pd.ice, 12.0f);
            }
            if (field.kind == ArenaKind::Water && field.IsWater(at)) {
                bool near = false;
                for (const Puddle& pd : field.puddles)
                    if (pd.sheet && Dist(pd.pos, at) < 20) near = true;
                if (!near) {
                    Puddle pd;
                    pd.seed = (uint32_t)SimRand(1, 0x3fffffff);
                    pd.pos = at;
                    pd.r = pd.maxR = 26 + std::min(14.0f, radius * 0.5f);
                    pd.ice = 12;
                    pd.sheet = true;
                    field.puddles.push_back(pd);
                    froze = true;
                }
            }
            if (froze) {
                AddText(at, "It froze solid - slippery ice!", Color{170, 230, 255, 255}, 30);
                Burst(at, Color{200, 240, 255, 255}, 12, 50, 2);
                PlaySfx(Sfx::Shield);
            }
        } break;
        case Type::Fire: {
            bool melted = false, steam = false;
            for (Puddle& pd : field.puddles) {
                if (!pd.Contains(at, radius + 6)) continue;
                if (pd.ice > 0) {
                    pd.ice = 0;
                    if (pd.sheet) pd.r = 0;
                    melted = true;
                } else {
                    pd.r *= 0.5f;
                    steam = true;
                }
                for (int k = 0; k < 8; k++)
                    Emit(PK::Smoke, pd.pos + Vector2{Frand(-pd.r, pd.r) * 0.7f, Frand(-pd.r, pd.r) * 0.5f}, 2, {Frand(-8, 8), 0}, Frand(20, 40),
                         Frand(0.6f, 1.0f), Frand(2, 4), Color{230, 235, 245, 200});
            }
            if (melted) AddText(at, "The ice melted!", Color{255, 170, 90, 255}, 30);
            else if (steam) AddText(at, "The puddle boiled away!", Color{230, 235, 245, 255}, 30);
        } break;
        case Type::Water: {
            if (h.light || h.side < 0 || field.IsWater(at)) break;
            for (Puddle& pd : field.puddles)
                if (pd.ice <= 0 && pd.Contains(at, 6)) {
                    pd.maxR = std::min(30.0f, std::max(pd.maxR, pd.r + 3));
                    pd.r = std::min(pd.maxR, pd.r + 3);
                    return;
                }
            if (field.puddles.size() >= 14) break;
            Puddle pd;
            pd.seed = (uint32_t)SimRand(1, 0x3fffffff);
            pd.pos = {std::max(12.0f, std::min(ARENA_W - 12, at.x)), std::max(12.0f, std::min(ARENA_H - 12, at.y))};
            pd.r = pd.maxR = std::min(16.0f, 8.0f + h.power / 15.0f + radius * 0.2f);
            field.puddles.push_back(pd);
        } break;
        default: break;
    }
}

void Battle::FieldReactLine(Vector2 a, Vector2 b, const HitSpec& h) {
    if (h.power <= 0 || (h.type != Type::Electric && h.type != Type::Ice && h.type != Type::Fire)) return;
    for (size_t i = 0; i < field.puddles.size(); i++) {
        Puddle& pd = field.puddles[i];
        if (DistPointSeg(pd.pos, a, b) < pd.r) FieldReact(pd.pos, h, 0);
    }
}

void Battle::ShockPuddle(Puddle& pd, const HitSpec& h) {
    pd.shockCd = 1.0f;
    pd.shockT = 0.45f;
    field.waterFlash = std::max(field.waterFlash, 0.01f);
    AddText(pd.pos, "The puddle is electrified!", kYellow, 26);
    PlaySfx(Sfx::Zap);
    Shake(2);
    for (int k = 0; k < 3; k++) {   // arcs across the water
        Fx fx;
        fx.kind = FxKind::Bolt;
        fx.maxLife = 0.35f;
        fx.col = kYellow;
        Vector2 a = pd.pos + Rot(Vector2{pd.r * 0.9f, 0}, Frand(0, 2 * PI_F));
        Vector2 b = pd.pos + Rot(Vector2{pd.r * 0.9f, 0}, Frand(0, 2 * PI_F));
        for (int i = 0; i <= 5; i++) fx.pts.push_back(LerpV(a, b, i / 5.0f) + Vector2{Frand(-4, 4), Frand(-3, 3)});
        fxs.push_back(fx);
    }
    for (int ts = 0; ts < N(); ts++) {
        if (!Hittable(ts)) continue;
        Fighter& t = sides[ts].Cur();
        if (IsAirborne(*t.sp) || t.burrowed || t.leaping || t.jumpT > 0 || !pd.Contains(t.pos, BODY_R * 0.6f) || t.shieldT > 0) continue;
        float eff = TypeEffectiveness(Type::Electric, t.sp->type1, t.sp->type2);
        if (eff <= 0) { AddText(t.pos, "No effect", UI_DIM); continue; }
        int credit = h.side >= 0 && Foes(h.side, ts) ? h.side : AnyFoe(ts);
        Fx fx;
        fx.kind = FxKind::Bolt;
        fx.maxLife = 0.3f;
        fx.col = kYellow;
        Vector2 a = t.pos + Vector2{Frand(-12, 12), Frand(-6, 6)};
        for (int i = 0; i <= 4; i++) fx.pts.push_back(LerpV(a, t.pos, i / 4.0f) + Vector2{Frand(-4, 4), Frand(-4, 4)});
        fxs.push_back(fx);
        DirectDamage(ts, t.maxHp * 0.08f * std::min(eff, 2.0f), kYellow, credit);
        if (!HasType(t.sp, Type::Electric) && t.state == FState::Active) {
            t.st.paraStunT = std::max(t.st.paraStunT, 0.5f);
            t.pendingMove = -1;
            t.dashing = false;
            if (Chance(0.3f)) TryStatus(ts, Status::Paralyze, credit);
        }
    }
}

bool Battle::OnIce(Vector2 p) const {
    for (const Puddle& pd : field.puddles)
        if (pd.ice > 0 && pd.Contains(p)) return true;
    return false;
}

void Battle::MoundGone(Obstacle& o, bool melted) {
    o.alive = false;
    Burst(o.pos, Color{240, 248, 255, 255}, 18, 60, 8);
    if (melted) {   // the snow turns into a puddle
        Puddle pd;
        pd.seed = (uint32_t)SimRand(1, 0x3fffffff);
        pd.pos = o.pos;
        pd.r = pd.maxR = o.radius * 1.1f;
        field.puddles.push_back(pd);
    }
}

// Rain slowly makes puddles; snow piles up mounds. Both change as the weather changes.
void Battle::UpdateWeatherField(float dt) {
    Weather w = weather;
    bool live = winner < 0 && introT <= 0;
    auto freeSpot = [&](Vector2 p, float r, bool avoidFighters) {
        if (field.IsWater(p) || field.InZone(p, Zone::LavaPool, r) || field.InZone(p, Zone::PoisonPool, r)) return false;
        for (const Obstacle& o : field.obs)
            if (o.alive && Dist(o.pos, p) < o.radius + r + 18) return false;
        if (avoidFighters)
            for (int s = 0; s < N(); s++)
                if (Dist(sides[s].Cur().pos, p) < r + 42) return false;
        return true;
    };

    // Puddles
    for (Puddle& pd : field.puddles) {
        pd.shockCd = std::max(0.0f, pd.shockCd - dt);
        pd.shockT = std::max(0.0f, pd.shockT - dt);
        if (pd.ice > 0) {
            if (w == Weather::Snow && !pd.sheet) pd.ice = std::max(pd.ice, 2.0f);   // stays frozen while it snows
            pd.ice -= dt * (w == Weather::Sun ? 3.0f : 1.0f);
            if (pd.ice <= 0) {
                pd.ice = 0;
                if (pd.sheet) pd.r = 0;   // the pool's ice is gone
                Burst(pd.pos, Color{170, 220, 255, 255}, 6, 30, 2);
            }
        } else if (w == Weather::Rain) {
            pd.r = std::min(pd.maxR, pd.r + 1.0f * dt);
        } else if (w == Weather::Snow) {
            pd.ice = 2.0f;   // freezes over
        } else {
            pd.r -= dt * (w == Weather::Sun ? 2.5f : 0.3f);
        }
    }
    field.puddles.erase(std::remove_if(field.puddles.begin(), field.puddles.end(), [](const Puddle& p) { return p.r < 2.5f; }),
                        field.puddles.end());
    if (w == Weather::Rain && live) {
        field.puddleT -= dt;
        int n = 0;
        for (const Puddle& pd : field.puddles) n += !pd.sheet;
        if (field.puddleT <= 0) {
            field.puddleT = Frand(2.5f, 4.5f);
            if (n < 9) {
                for (int tries = 0; tries < 14; tries++) {
                    Vector2 p{Frand(30, ARENA_W - 30), Frand(22, ARENA_H - 22)};
                    float maxR = Frand(15, 28);
                    bool ok = freeSpot(p, maxR, false);
                    for (const Puddle& o : field.puddles)
                        if (Dist(o.pos, p) < o.maxR + maxR + 10) ok = false;
                    if (!ok) continue;
                    Puddle pd;
                    pd.seed = (uint32_t)SimRand(1, 0x3fffffff);
                    pd.pos = p;
                    pd.r = 3;
                    pd.maxR = maxR;
                    field.puddles.push_back(pd);
                    break;
                }
            }
        }
    }

    // Snow mounds
    int mounds = 0;
    for (Obstacle& o : field.obs) {
        if (o.kind != Obstacle::SnowMound || !o.alive) continue;
        mounds++;
        o.grow = std::min(1.0f, o.grow + dt / 1.5f);
        if (w != Weather::Snow) {
            o.hp -= dt * (w == Weather::Sun ? 30.0f : w == Weather::Rain ? 10.0f : 5.0f);
            if (o.hp <= 0) MoundGone(o, true);
        }
    }
    field.obs.erase(std::remove_if(field.obs.begin(), field.obs.end(),
                                   [](const Obstacle& o) { return o.kind == Obstacle::SnowMound && !o.alive; }),
                    field.obs.end());
    if (w == Weather::Snow && live) {
        field.moundT -= dt;
        if (field.moundT <= 0) {
            field.moundT = Frand(5.0f, 8.0f);
            if (mounds < 6) {
                for (int tries = 0; tries < 14; tries++) {
                    Vector2 p{Frand(40, ARENA_W - 40), Frand(30, ARENA_H - 30)};
                    float r = Frand(11, 15);
                    if (!freeSpot(p, r, true)) continue;
                    Obstacle o;
                    o.kind = Obstacle::SnowMound;
                    o.seed = (uint32_t)SimRand(1, 0x3fffffff);
                    o.pos = p;
                    o.radius = r;
                    o.height = Frand(16, 22);
                    o.hp = o.maxHp = 150;
                    o.grow = 0;
                    field.obs.push_back(o);
                    Burst(p, Color{240, 248, 255, 255}, 10, 30, 2);
                    break;
                }
            }
        }
    }

    // What's underfoot: puddles slow you down, ice keeps you sliding.
    for (int s = 0; s < N(); s++) {
        Fighter& f = sides[s].Cur();
        bool grounded = f.state == FState::Active && !f.burrowed && !f.leaping && f.jumpT <= 0 && !IsAirborne(*f.sp);
        bool wasIce = f.onIce;
        f.onIce = false;
        if (grounded) {
            for (const Puddle& pd : field.puddles) {
                if (!pd.Contains(f.pos, 2)) continue;
                if (pd.ice > 0) {
                    if (!HasType(f.sp, Type::Ice)) f.onIce = true;
                    f.envSpeed = std::max(f.envSpeed, 1.0f);   // walking on the ice, not wading
                } else if (!HasType(f.sp, Type::Water)) {
                    f.envSpeed = std::min(f.envSpeed, 0.6f);
                    if (f.moving && SimRand(0, 3) == 0)
                        Emit(PK::Pixel, f.pos + Vector2{Frand(-6, 6), Frand(-2, 2)}, 0, {Frand(-15, 15), Frand(-8, 8)}, Frand(15, 35), 0.35f, 1,
                             Color{170, 200, 240, 255}, true);
                }
            }
        }
        if (f.onIce && !f.slid) {   // no input this frame: keep gliding
            f.pos += f.slide * dt;
            f.slide = f.slide * std::max(0.0f, 1.0f - 0.9f * dt);
            if (Len(f.slide) > 20 && SimRand(0, 4) == 0)
                Emit(PK::Pixel, f.pos, 0, f.slide * -0.2f, 6, 0.3f, 1, Color{220, 240, 255, 255});
        } else if (!f.onIce && !f.slid) {
            f.slide = {0, 0};
        }
        if (wasIce && !f.onIce && !f.slid) f.slide = {0, 0};
        f.slid = false;
    }
}

// ---------------------------------------------------------------- CPU awareness

Vector2 Battle::FieldPull(int s) const {
    Vector2 pull{0, 0};
    const Fighter& f = sides[s].Cur();
    const SpeciesDef* sp = f.sp;
    bool air = IsAirborne(*sp);

    for (const Obstacle& o : field.obs) {
        if (!o.Blocks()) continue;
        float d = Dist(f.pos, o.pos);
        if (d < o.radius + 16) pull += SafeNorm(f.pos - o.pos) * 0.8f;
    }
    if (!air) {   // keep out of puddles (they slow you) and off the ice (you slide)
        for (const Puddle& pd : field.puddles) {
            bool avoid = pd.ice > 0 ? !HasType(sp, Type::Ice) : !HasType(sp, Type::Water);
            if (avoid && pd.Contains(f.pos, 10)) pull += SafeNorm(f.pos - pd.pos, Vector2{0, 1}) * (pd.ice > 0 ? 0.9f : 0.6f);
        }
    }
    if (field.kind == ArenaKind::Stadium) return pull;
    auto nearestZone = [&](Zone::Kind k, float& dist) -> const Zone* {
        const Zone* best = nullptr;
        dist = 1e9f;
        for (const Zone& z : field.zones) {
            if (z.kind != k) continue;
            float d = Dist(z.pos, f.pos);
            if (d < dist) { dist = d; best = &z; }
        }
        return best;
    };
    auto avoidZones = [&](Zone::Kind k) {
        for (const Zone& z : field.zones)
            if (z.kind == k && z.Contains(f.pos, 14)) pull += SafeNorm(f.pos - z.pos, Vector2{0, 1}) * 1.3f;
    };

    switch (field.kind) {
        case ArenaKind::Electric: {
            const Obstacle* coil = nullptr;
            float best = 1e9f;
            for (const Obstacle& o : field.obs)
                if (o.kind == Obstacle::Coil && o.alive && Dist(o.pos, f.pos) < best) { best = Dist(o.pos, f.pos); coil = &o; }
            if (!coil) break;
            if (HasType(sp, Type::Electric)) {
                if (best > 38) pull += SafeNorm(coil->pos - f.pos) * 1.1f;  // camp the coils for the boost
            } else if (TypeEffectiveness(Type::Electric, sp->type1, sp->type2) > 0 && best < 62) {
                pull += SafeNorm(f.pos - coil->pos) * 1.3f;
            }
        } break;
        case ArenaKind::Grass:
            if (HasType(sp, Type::Grass) && !field.InZone(f.pos, Zone::TallGrass)) {
                float d;
                const Zone* z = nearestZone(Zone::TallGrass, d);
                if (z && d < 200) pull += SafeNorm(z->pos - f.pos) * 0.7f;
            }
            break;
        case ArenaKind::Poison:
            if (!HasType(sp, Type::Poison) && !air) {
                avoidZones(Zone::PoisonPool);
                float m = field.spikeBand + 16;
                if (f.pos.x < m) pull.x += 1.5f;
                if (f.pos.x > ARENA_W - m) pull.x -= 1.5f;
                if (f.pos.y < m) pull.y += 1.5f;
                if (f.pos.y > ARENA_H - field.spikeBand * 0.7f - 16) pull.y -= 1.5f;
            }
            break;
        case ArenaKind::Fire:
            if (!HasType(sp, Type::Fire) && !air) avoidZones(Zone::LavaPool);
            break;
        case ArenaKind::Water:
            if (!air && !HasType(sp, Type::Water) && field.IsWater(f.pos)) {
                float d;
                const Zone* z = nearestZone(Zone::Platform, d);
                if (z) pull += SafeNorm(z->pos - f.pos) * 0.6f;
            }
            break;
        case ArenaKind::Ground:
            for (const Turret& t : field.turrets) {
                if (t.warnT < 0) continue;
                Vector2 dir = SafeNorm(t.aimAt - t.pos, t.dir);
                Vector2 rel = f.pos - t.pos;
                float along = Dot(rel, dir);
                if (along < 0) continue;
                Vector2 lat = rel - dir * along;
                if (Len(lat) < 24) pull += SafeNorm(lat, Vector2{-dir.y, dir.x}) * 1.6f;
            }
            break;
        default: break;
    }
    return pull;
}

// ---------------------------------------------------------------- Drawing

void Battle::DrawObstacle(const Obstacle& o) const {
    Vector2 b = ToScreen(o.pos);
    int x = (int)std::round(b.x), y = (int)std::round(b.y);
    int r = (int)o.radius, h = (int)o.height;
    switch (o.kind) {
        case Obstacle::Pillar: {
            if (!o.alive) return;
            // The look depends on where it stands, and its seed picks a variant.
            int style = 0;   // 0 column, 1 broken column, 2 stacked boulders, 3 jagged spire, 4 round boulder
            Venue vn = field.venue;
            if (vn == Venue::Tower) style = 10;
            else if (vn == Venue::Mansion) style = 11;
            else if (vn == Venue::IceCave) style = 12;
            else if (vn == Venue::Cave) style = 13 + (int)(o.seed % 2);           // stalagmite / rock
            else if (vn == Venue::Mountain) style = o.seed % 2 ? 3 : 2;
            else style = (int)(o.seed % 5);
            auto rv = [&](int k) { return VRand(o.seed, (uint32_t)k); };
            DrawEllipse(x, y + 1, (float)r + 3, (r + 3) * 0.45f, Fade(BLACK, 0.35f));
            Color c{150, 136, 118, 255}, dk{104, 92, 80, 255}, lt{184, 170, 150, 255};
            int tint = (int)(rv(1) * 24) - 12;   // every stone a slightly different colour
            c = Color{(unsigned char)(c.r + tint), (unsigned char)(c.g + tint), (unsigned char)(c.b + tint), 255};
            switch (style) {
                case 0: case 1: {   // carved column (1: snapped off at the top)
                    int hh = style == 1 ? (int)(h * (0.5f + rv(2) * 0.2f)) : h;
                    DrawRectangle(x - r, y - hh, r * 2, hh, c);
                    DrawRectangle(x - r, y - hh, 3, hh, lt);
                    DrawRectangle(x + r - 3, y - hh, 3, hh, dk);
                    for (int k = 1; k < 4; k++) DrawRectangle(x - r, y - hh + k * hh / 4, r * 2, 1, dk);
                    DrawRectangle(x - r - 2, y - 4, r * 2 + 4, 4, dk);   // base plinth
                    if (style == 1) {   // jagged break
                        for (int k = 0; k < r * 2; k += 3) {
                            int jh = (int)(rv(10 + k) * 6);
                            DrawRectangle(x - r + k, y - hh - jh, 3, jh + 1, k < r ? lt : c);
                        }
                        DrawRectangle(x + r + 2, y - 3, 4, 3, dk);   // fallen chunk
                    } else {
                        DrawEllipse(x, y - hh, (float)r + 1, (r + 1) * 0.45f, lt);
                        EllipsePix((float)x, (float)(y - hh), (float)r + 1, (r + 1) * 0.45f, dk);
                    }
                    DrawRectangle(x - r + (int)(rv(3) * r), y - hh + hh / 3, 1, hh / 4, dk);   // a crack
                } break;
                case 2: {   // stacked boulders
                    float b1 = r * 1.05f, b2 = r * (0.75f + rv(4) * 0.15f), b3 = r * (0.5f + rv(5) * 0.15f);
                    int ox = (int)((rv(6) - 0.5f) * 5);
                    DrawEllipse(x, y - (int)(b1 * 0.55f), b1, b1 * 0.6f, dk);
                    DrawEllipse(x - 1, y - (int)(b1 * 0.6f), b1 - 1, b1 * 0.55f, c);
                    DrawEllipse(x + ox, y - (int)(b1 * 1.05f + b2 * 0.5f), b2, b2 * 0.6f, dk);
                    DrawEllipse(x + ox - 1, y - (int)(b1 * 1.05f + b2 * 0.55f), b2 - 1, b2 * 0.55f, c);
                    DrawEllipse(x - ox, y - (int)(b1 * 1.05f + b2 * 1.05f + b3 * 0.5f), b3, b3 * 0.65f, c);
                    DrawEllipse(x - ox - 2, y - (int)(b1 * 1.05f + b2 * 1.05f + b3 * 0.75f), b3 * 0.5f, b3 * 0.3f, lt);
                    DrawEllipse(x - (int)(b1 * 0.4f), y - (int)(b1 * 0.85f), b1 * 0.35f, b1 * 0.2f, lt);
                } break;
                case 3: {   // jagged rock spire
                    float lean = (rv(7) - 0.5f) * 8;
                    Vector2 base1{(float)x - r, (float)y}, base2{(float)x + r, (float)y}, tip{(float)x + lean, (float)(y - h - 4)};
                    Vector2 mid1{(float)x - r * 0.6f, (float)y - h * 0.55f}, mid2{(float)x + r * 0.7f, (float)y - h * 0.45f};
                    DrawTriangle(base1, base2, mid2, dk);
                    DrawTriangle(base1, mid2, mid1, c);
                    DrawTriangle(mid1, mid2, tip, c);
                    DrawTriangle(mid1, tip, {(float)x - r * 0.2f, (float)y - h * 0.6f}, lt);
                    DrawLineEx(mid1, {mid1.x + 6, mid1.y + 8}, 1, dk);
                } break;
                case 4: default: {   // big round boulder with moss
                    DrawEllipse(x, y - (int)(h * 0.38f), r + 3.0f, h * 0.42f, dk);
                    DrawEllipse(x - 1, y - (int)(h * 0.42f), r + 2.0f, h * 0.38f, c);
                    DrawEllipse(x - r / 3, y - (int)(h * 0.62f), r * 0.6f, h * 0.15f, lt);
                    if (o.seed % 3 == 0) DrawEllipse(x + r / 3, y - (int)(h * 0.72f), r * 0.5f, h * 0.1f, Color{90, 140, 70, 255});
                } break;
                case 10: {   // Pokemon Tower: a gravestone
                    int gw = r + 1, gh = (int)(h * (0.9f + rv(8) * 0.3f));
                    Color gs{120, 116, 134, 255}, gd{80, 76, 94, 255};
                    DrawRectangle(x - gw, y - gh + gw / 2, gw * 2, gh - gw / 2, gs);
                    DrawCircle(x, y - gh + gw / 2, (float)gw, gs);
                    DrawRectangle(x + gw - 2, y - gh + gw / 2, 2, gh - gw / 2, gd);
                    DrawRectangle(x - 1, y - gh + 3, 2, 8, gd);
                    DrawRectangle(x - 4, y - gh + 5, 8, 2, gd);
                    DrawRectangle(x - gw - 2, y - 3, gw * 2 + 4, 3, gd);
                    if (o.seed % 2) { DrawCircle(x + gw + 3, y - 2, 2, Color{230, 120, 160, 255}); DrawCircle(x + gw + 5, y - 3, 1.5f, Color{250, 220, 120, 255}); }
                } break;
                case 11: {   // Cinnabar Mansion: a cracked marble statue plinth
                    Color m{214, 206, 196, 255}, md{160, 150, 140, 255};
                    int hh = (int)(h * (0.7f + rv(9) * 0.3f));
                    DrawRectangle(x - r - 2, y - 5, r * 2 + 4, 5, md);
                    DrawRectangle(x - r + 2, y - hh, r * 2 - 4, hh - 5, m);
                    DrawRectangle(x + r - 5, y - hh, 3, hh - 5, md);
                    DrawRectangle(x - r, y - hh - 3, r * 2, 4, m);
                    DrawLineEx({(float)x - 3, (float)y - hh + 2}, {(float)x + 2, (float)y - hh / 2}, 1, md);
                    DrawLineEx({(float)x + 2, (float)y - hh / 2}, {(float)x - 1, (float)y - 6}, 1, md);
                } break;
                case 12: {   // ice crystal
                    Color ic{170, 220, 250, 230}, idk{110, 170, 220, 255}, il{235, 250, 255, 255};
                    for (int k = 0; k < 3; k++) {
                        float ox = (k - 1) * r * 0.55f, hh = h * (k == 1 ? 1.0f : 0.55f + rv(20 + k) * 0.2f);
                        Vector2 a{(float)x + ox - r * 0.35f, (float)y}, b2{(float)x + ox + r * 0.35f, (float)y}, tp{(float)x + ox + (rv(30 + k) - 0.5f) * 4, (float)y - hh};
                        DrawTriangle(a, b2, tp, k == 1 ? ic : idk);
                        DrawLineEx(tp, {a.x + 2, a.y}, 1, il);
                    }
                } break;
                case 13: case 14: {   // cave stalagmite / rock
                    Color cc{112, 100, 92, 255}, cd{74, 66, 60, 255}, cl{140, 128, 118, 255};
                    if (style == 13) {
                        Vector2 a{(float)x - r, (float)y}, b2{(float)x + r, (float)y}, tp{(float)x + (rv(11) - 0.5f) * 5, (float)y - h - 2};
                        DrawTriangle(a, b2, tp, cc);
                        DrawTriangle({(float)x + 1, (float)y}, b2, tp, cd);
                        for (int k = 1; k < 4; k++) DrawRectangle(x - r + k * r / 2, y - k * h / 4, r / 2, 1, cl);
                    } else {
                        DrawEllipse(x, y - h / 3, r + 2.0f, h * 0.36f, cd);
                        DrawEllipse(x - 1, y - h / 3 - 1, r + 1.0f, h * 0.32f, cc);
                        DrawEllipse(x - r / 3, y - h / 2, r * 0.5f, h * 0.12f, cl);
                    }
                } break;
            }
        } break;
        case Obstacle::MetalPillar: {
            Color c{120, 124, 134, 255}, dk{70, 72, 80, 255}, lt{180, 184, 194, 255};
            DrawEllipse(x, y + 1, (float)r + 2, (r + 2) * 0.45f, Fade(BLACK, 0.35f));
            DrawRectangle(x - r, y - h, r * 2, h, c);
            DrawRectangle(x - r, y - h, 2, h, lt);
            DrawRectangle(x + r - 2, y - h, 2, h, dk);
            DrawRectangle(x - r, y - h / 2 - 2, r * 2, 4, Color{170, 30, 30, 255});
            DrawText("R", x - 3, y - h / 2 - 5, 10, WHITE);
            for (int k = 0; k < 3; k++) {
                DrawRectangle(x - r + 3, y - h + 3 + k * 9, 1, 1, lt);
                DrawRectangle(x + r - 4, y - h + 3 + k * 9, 1, 1, lt);
            }
            DrawEllipse(x, y - h, (float)r, r * 0.45f, lt);
            EllipsePix((float)x, (float)(y - h), (float)r, r * 0.45f, dk);
        } break;
        case Obstacle::Boulder: {
            bool hot = field.kind == ArenaKind::Fire || field.venue == Venue::Volcano;
            auto rv = [&](int k) { return VRand(o.seed, (uint32_t)k); };
            Color c = hot ? Color{86, 70, 66, 255} : field.venue == Venue::Beach ? Color{150, 146, 140, 255} : Color{120, 112, 104, 255};
            Color dk = ColorBrightness(c, -0.4f), lt = ColorBrightness(c, 0.3f);
            float sx = 1.0f + (rv(1) - 0.5f) * 0.5f, sy = 1.0f + (rv(2) - 0.5f) * 0.5f;   // squat or tall
            DrawEllipse(x, y + 1, (r + 2) * sx, (r + 2) * 0.45f, Fade(BLACK, 0.4f));
            DrawEllipse(x, y - (int)(h / 2 * sy), (r + 1) * sx, h * 0.6f * sy, dk);
            DrawEllipse(x, y - (int)(h / 2 * sy) - 1, r * sx, h * 0.55f * sy, c);
            if (o.seed % 2) DrawEllipse(x + (int)(r * 0.7f), y - 2, r * 0.45f, h * 0.3f, c);   // a smaller rock beside it
            DrawRectangle(x - (int)(r / 2 * sx), y - (int)(h * sy) + 2, (int)(r / 2 * sx) + 1, 2, lt);
            if (hot) {
                DrawRectangle(x + 1, y - h / 2, 2, 1, Color{240, 120, 40, 255});
                if (o.seed % 3 == 0) DrawRectangle(x - 3, y - h / 3, 1, 2, Color{255, 170, 60, 255});
            }
        } break;
        case Obstacle::Crate: {
            if (!o.alive) return;
            float life = std::max(0.2f, o.hp / std::max(1.0f, o.maxHp));
            int style = (int)(o.seed % 3);   // 0 crate, 1 barrel, 2 two stacked boxes
            bool metal = field.venue == Venue::Hideout, paper = field.venue == Venue::Office;
            Color wd = metal ? Color{110, 116, 126, 255} : paper ? Color{196, 160, 110, 255} : Color{158, 112, 64, 255};
            Color dk = ColorBrightness(wd, -0.35f), lt = ColorBrightness(wd, 0.25f);
            DrawEllipse(x, y + 1, r + 3.0f, (r + 3) * 0.45f, Fade(BLACK, 0.35f));
            if (style == 1) {
                DrawRectangle(x - r + 1, y - h, r * 2 - 2, h, wd);
                DrawEllipse(x, y - h, r - 1.0f, (r - 1) * 0.4f, lt);
                DrawRectangle(x - r + 1, y - h + 3, r * 2 - 2, 2, dk);
                DrawRectangle(x - r + 1, y - 5, r * 2 - 2, 2, dk);
                DrawRectangle(x + r - 4, y - h, 3, h, dk);
            } else {
                int bh = style == 2 ? h / 2 + 2 : h;
                DrawRectangle(x - r, y - bh, r * 2, bh, wd);
                DrawRectangle(x - r, y - bh, r * 2, 2, lt);
                DrawRectangleLines(x - r, y - bh, r * 2, bh, dk);
                if (!paper) { DrawLineEx({(float)x - r, (float)y - bh}, {(float)x + r, (float)y}, 1, dk); DrawLineEx({(float)x + r, (float)y - bh}, {(float)x - r, (float)y}, 1, dk); }
                else DrawRectangle(x - 1, y - bh, 2, bh, Color{220, 200, 150, 255});
                if (style == 2) {
                    int w2 = r - 2;
                    DrawRectangle(x - w2 + 2, y - bh - bh + 2, w2 * 2, bh - 2, wd);
                    DrawRectangle(x - w2 + 2, y - bh - bh + 2, w2 * 2, 2, lt);
                    DrawRectangleLines(x - w2 + 2, y - bh - bh + 2, w2 * 2, bh - 2, dk);
                }
                if (metal) DrawText("R", x - 3, y - bh / 2 - 5, 10, Color{200, 40, 40, 255});
            }
            if (life < 0.6f) DrawLineEx({(float)x - 3, (float)y - h + 3}, {(float)x + 2, (float)y - 4}, 1, Fade(BLACK, 0.6f));
        } break;
        case Obstacle::Coil: {
            Color base{60, 60, 70, 255}, cu{200, 120, 60, 255}, cuD{140, 80, 40, 255};
            DrawEllipse(x, y + 1, (float)r + 3, (r + 3) * 0.45f, Fade(BLACK, 0.4f));
            DrawRectangle(x - r, y - 6, r * 2, 6, base);
            DrawRectangle(x - r, y - 6, r * 2, 1, Color{100, 100, 110, 255});
            DrawRectangle(x - 3, y - h + 6, 6, h - 12, Color{90, 90, 100, 255});
            for (int k = 0; k < (h - 14) / 3; k++) DrawRectangle(x - 5, y - 9 - k * 3, 10, 2, (k % 2) ? cu : cuD);
            float pulse = 0.5f + 0.5f * std::sin(time * 9 + o.pos.x);
            DrawCircle(x, y - h + 2, 5.5f, Color{170, 170, 190, 255});
            DrawCircle(x, y - h + 2, 4, ColorAlpha(kYellow, 0.6f + 0.4f * pulse));
            DrawCircle(x - 1, y - h + 1, 1.5f, WHITE);
            if (SimRand(0, 3) == 0) {  // stray sparks
                Vector2 a{(float)x, (float)(y - h + 2)};
                Vector2 c = a + Vector2{Frand(-14, 14), Frand(-6, 10)};
                DrawLineEx(a, c, 1, kYellow);
            }
        } break;
        case Obstacle::TempWall: {
            Color c = TypeColor(o.wallType);
            float a = std::min(1.0f, o.life * 2.0f) * std::min(1.0f, (o.maxLife - o.life) * 8.0f + 0.2f);
            if (o.life < 0.8f && ((int)(time * 12) % 2)) a *= 0.5f;
            int hh = (int)(h * std::min(1.0f, (o.maxLife - o.life) * 6.0f + 0.2f));
            DrawEllipse(x, y + 1, (float)r + 2, (r + 2) * 0.45f, Fade(BLACK, 0.3f * a));
            DrawRectangle(x - r, y - hh, r * 2, hh, Fade(ColorBrightness(c, -0.25f), 0.85f * a));
            DrawRectangle(x - r, y - hh, 2, hh, Fade(ColorBrightness(c, 0.4f), a));
            DrawRectangle(x - r, y - hh, r * 2, 2, Fade(ColorBrightness(c, 0.5f), a));
            DrawRectangle(x - r + 3, y - hh + 4, 1, std::max(1, hh - 8), Fade(WHITE, 0.35f * a));
        } break;
        case Obstacle::SnowMound: {
            if (!o.alive) return;
            float g = 0.3f + 0.7f * o.grow, life = std::max(0.25f, o.hp / std::max(1.0f, o.maxHp));
            float rw = o.radius * (0.6f + 0.4f * g) * (0.75f + 0.25f * life), hh = o.height * g * (0.45f + 0.55f * life);
            Color sh{168, 188, 224, 255}, body{234, 242, 252, 255}, lt{255, 255, 255, 255};
            auto rv = [&](int k) { return VRand(o.seed, (uint32_t)k); };
            int style = (int)(o.seed % 4);   // 0 dome, 1 twin lumps, 2 long drift, 3 heap with a rock or twig
            float bw = rw * (style == 2 ? 1.7f : 1.25f), tall = style == 2 ? 0.6f : 1.0f;
            DrawEllipse(x, y + 1, bw + 3, (bw + 3) * 0.4f, Fade(Color{60, 80, 120, 255}, 0.3f));
            DrawEllipse(x, y - 1, bw, bw * 0.38f, sh);
            DrawEllipse(x, y - (int)(hh * 0.35f * tall), bw * 0.92f, hh * 0.62f * tall, sh);
            DrawEllipse(x - 1, y - (int)(hh * 0.4f * tall) - 1, bw * 0.86f, hh * 0.58f * tall, body);
            if (style == 1) {
                DrawEllipse(x + (int)(bw * 0.4f), y - (int)(hh * 0.55f), bw * 0.5f, hh * 0.45f, sh);
                DrawEllipse(x + (int)(bw * 0.38f), y - (int)(hh * 0.6f), bw * 0.46f, hh * 0.4f, body);
            } else if (style == 2) {
                DrawEllipse(x - (int)(bw * 0.45f), y - (int)(hh * 0.3f), bw * 0.4f, hh * 0.3f, body);   // wind-blown tail
            } else {
                DrawEllipse(x + (int)(bw * (rv(1) - 0.3f) * 0.6f), y - (int)(hh * 0.62f), bw * 0.45f, hh * 0.32f, body);
            }
            DrawEllipse(x - (int)(bw * 0.3f), y - (int)(hh * 0.62f * tall), bw * 0.4f, hh * 0.25f * tall, lt);
            if (style == 3) {
                if (o.seed % 8 < 4) {   // a rock peeking out
                    DrawEllipse(x + (int)(bw * 0.45f), y - 3, 4, 3, Color{110, 104, 98, 255});
                    DrawRectangle(x + (int)(bw * 0.45f) - 2, y - 5, 3, 1, Color{160, 154, 148, 255});
                } else {   // a twig
                    DrawLineEx({(float)x - 2, (float)y - hh * 0.8f}, {(float)x + 3, (float)y - hh * 1.25f}, 1, Color{100, 70, 44, 255});
                    DrawLineEx({(float)x + 1, (float)y - hh * 1.0f}, {(float)x + 5, (float)y - hh * 1.05f}, 1, Color{100, 70, 44, 255});
                }
            }
            for (int k = 0; k < 3; k++)   // sparkles
                if (std::fmod(time * 0.8f + rv(40 + k) * 5, 2.5f) < 0.2f)
                    DrawRectangle(x + (int)((rv(50 + k) - 0.5f) * bw * 1.4f), y - (int)(rv(60 + k) * hh * 0.8f) - 2, 1, 1, WHITE);
            if (life < 0.6f) {   // cracks
                DrawLineEx({(float)x - bw * 0.4f, (float)y - hh * 0.4f}, {(float)x, (float)y - hh * 0.25f}, 1, sh);
                DrawLineEx({(float)x, (float)y - hh * 0.25f}, {(float)x + bw * 0.3f, (float)y - hh * 0.6f}, 1, sh);
            }
            if (life < 0.3f) DrawLineEx({(float)x - bw * 0.1f, (float)y - hh * 0.8f}, {(float)x + bw * 0.15f, (float)y - hh * 0.15f}, 1, sh);
        } break;
        case Obstacle::Tree: {
            if (!o.alive) return;
            float u = o.fallT >= 0 ? std::min(1.0f, o.fallT / 0.45f) : 0.0f;
            u = u * u;
            Vector2 lean = o.fallDir * (u * o.height);
            Vector2 top = ToScreen(o.pos + lean, o.height * (1.0f - u));
            Vector2 mid = ToScreen(o.pos + lean * 0.5f, o.height * 0.5f * (1.0f - u));
            DrawEllipse(x, y + 1, (float)r + 3, (r + 3) * 0.45f, Fade(BLACK, 0.35f));
            DrawLineEx({(float)x, (float)y}, mid, 5, Color{100, 70, 44, 255});
            DrawLineEx({(float)x - 1, (float)y}, mid, 1, Color{130, 96, 60, 255});
            bool burning = o.burnT > 0;
            // Each tree has its own shape (round, pine, tall bush, blossom) and dresses for the season.
            int style = (int)(o.seed % 4);
            int season = field.season;
            Color leaf{56, 128, 56, 255}, leafD{36, 92, 44, 255}, leafL{96, 172, 76, 255};
            if (style == 1) { leaf = Color{40, 104, 70, 255}; leafD = Color{26, 74, 52, 255}; leafL = Color{70, 140, 96, 255}; }
            if (season == 0 && style == 3) { leaf = Color{236, 160, 196, 255}; leafD = Color{200, 116, 160, 255}; leafL = Color{252, 214, 230, 255}; }
            if (season == 2 && style != 1) {
                bool red = o.seed % 2;
                leaf = red ? Color{196, 84, 48, 255} : Color{214, 146, 46, 255};
                leafD = red ? Color{140, 56, 36, 255} : Color{160, 100, 30, 255};
                leafL = red ? Color{236, 130, 70, 255} : Color{246, 196, 90, 255};
            }
            if (burning) { leaf = Color{120, 110, 40, 255}; leafD = Color{80, 60, 30, 255}; leafL = Color{200, 120, 40, 255}; }
            float cr = o.radius + 4;
            if (style == 1) {   // pine
                for (int k = 0; k < 3; k++) {
                    float w = cr * (1.15f - k * 0.3f), yy = top.y + 14 - k * 7;
                    DrawTriangle({top.x - w, yy}, {top.x + w, yy}, {top.x, yy - 12}, leafD);
                    DrawTriangle({top.x - w + 2, yy - 1}, {top.x + w - 3, yy - 1}, {top.x - 1, yy - 11}, leaf);
                }
                DrawTriangle({top.x - 3, top.y + 1}, {top.x, top.y - 5}, {top.x - 1, top.y + 1}, leafL);
            } else if (style == 2) {   // tall bushy
                DrawEllipse((int)top.x, (int)(top.y + 6), cr * 0.85f, cr * 1.2f, leafD);
                DrawEllipse((int)top.x - 1, (int)(top.y + 4), cr * 0.75f, cr * 1.05f, leaf);
                DrawEllipse((int)top.x - 3, (int)(top.y - 1), cr * 0.3f, cr * 0.4f, leafL);
            } else {   // round crown (blossom tree in spring)
                DrawCircle((int)top.x, (int)(top.y + 6), cr + 1, leafD);
                DrawCircle((int)top.x - 5, (int)(top.y + 8), cr * 0.65f, leaf);
                DrawCircle((int)top.x + 5, (int)(top.y + 8), cr * 0.65f, leaf);
                DrawCircle((int)top.x, (int)(top.y + 3), cr * 0.75f, leaf);
                DrawCircle((int)top.x - 3, (int)(top.y + 1), cr * 0.3f, leafL);
                if (style == 3 && season != 0 && !burning)   // fruit
                    for (int k = 0; k < 4; k++) DrawCircle((int)(top.x + (VRand(o.seed, k) - 0.5f) * cr * 1.4f), (int)(top.y + 4 + VRand(o.seed, k + 9) * 8), 1.5f, Color{220, 60, 60, 255});
            }
            if (season == 3 && !burning) {   // snow on the branches
                DrawEllipse((int)top.x - 2, (int)(top.y + (style == 1 ? 2 : -1)), cr * 0.55f, 3, Color{240, 246, 255, 255});
                DrawEllipse((int)top.x + 4, (int)(top.y + 7), cr * 0.35f, 2, Color{240, 246, 255, 255});
            }
            if (burning) {
                for (int k = 0; k < 6; k++) {
                    int fx = (int)top.x + SimRand(-(int)cr, (int)cr);
                    int fy = (int)top.y + SimRand(-2, 10);
                    DrawRectangle(fx, fy - 3, 2, 4, SimRand(0, 1) ? Color{255, 150, 40, 255} : Color{255, 220, 90, 255});
                }
            }
        } break;
    }
}

void Battle::DrawFieldFloor() const {
    const float t = time;
    switch (field.kind) {
        case ArenaKind::Water: {
            for (int i = 0; i < 110; i++) {   // wave crests rolling slowly across the pool
                float h1 = VRand(31, i), h2 = VRand(32, i), h3 = VRand(33, i);
                float wx = std::fmod(h1 * ARENA_W + t * (5 + h2 * 6), ARENA_W);
                float wy = h3 * ARENA_H + std::sin(t * 1.1f + i) * 3;
                if (!field.IsWater({wx, wy})) continue;
                float a = 0.5f + 0.5f * std::sin(t * 1.7f + i * 1.3f);
                Vector2 sp = ToScreen({wx, wy});
                int len = 3 + (int)(h2 * 6);
                DrawRectangle((int)sp.x - len / 2, (int)sp.y, len, 1, Fade(Color{150, 200, 250, 255}, 0.45f * a));
                DrawRectangle((int)sp.x - len / 2 + 1, (int)sp.y - 1, len - 2, 1, Fade(Color{220, 240, 255, 255}, 0.3f * a));
            }
            for (int k = 0; k < 7; k++) {   // ripples where something stirs the surface
                float ph = t * 0.6f + k * 0.37f;
                uint32_t cyc = (uint32_t)ph;
                float u = ph - cyc;
                Vector2 w{30 + VRand(40 + cyc, k) * (ARENA_W - 60), 30 + VRand(50 + cyc, k) * (ARENA_H - 60)};
                if (!field.IsWater(w)) continue;
                Vector2 sp = ToScreen(w);
                float rr = 2 + u * 16;
                VRingE(sp, rr, rr * DEPTH, 0.8f, Fade(Color{200, 230, 255, 255}, 0.55f * (1 - u)), 20);
                if (u > 0.25f) VRingE(sp, rr * 0.6f, rr * 0.6f * DEPTH, 0.6f, Fade(Color{200, 230, 255, 255}, 0.35f * (1 - u)), 16);
            }
            for (const Zone& z : field.zones) {   // foam lapping at the islands
                if (z.kind != Zone::Platform) continue;
                for (int k = 0; k < 48; k++) {
                    float a = k * 6.2831853f / 48, wob = Wobble(z.seed, a);
                    float out = 2.5f + 1.8f * std::sin(t * 2.2f + k * 0.9f + z.pos.x);
                    Vector2 w{z.pos.x + std::cos(a) * (z.rx * wob + out), z.pos.y + std::sin(a) * (z.ry * wob + out)};
                    Vector2 sp = ToScreen(w);
                    float al = 0.35f + 0.35f * std::sin(t * 3 + k);
                    DrawRectangle((int)sp.x, (int)sp.y, 2, 1, Fade(WHITE, al));
                }
            }
            for (int i = 0; i < 70; i++) {  // drifting glints
                float wx = std::fmod(i * 97.0f + t * (8 + i % 5), ARENA_W);
                float wy = std::fmod(i * 53.0f + std::sin(t + i) * 6 + 400, ARENA_H);
                Vector2 w{wx, wy};
                if (!field.IsWater(w)) continue;
                Vector2 sp = ToScreen(w);
                if (((int)(t * 2 + i) % 3) == 0) continue;
                DrawRectangle((int)sp.x, (int)sp.y, 3, 1, Fade(Color{170, 210, 255, 255}, 0.7f));
            }
            if (field.waterFlash > 0) {
                float a = field.waterFlash / 0.5f;
                for (int i = 0; i < 90; i++) {
                    Vector2 w{Frand(0, ARENA_W), Frand(0, ARENA_H)};
                    if (!field.IsWater(w)) continue;
                    Vector2 sp = ToScreen(w);
                    Vector2 e = sp + Vector2{Frand(-6, 6), Frand(-3, 3)};
                    DrawLineEx(sp, e, 1, Fade(kYellow, a));
                    DrawRectangle((int)e.x, (int)e.y, 1, 1, Fade(WHITE, a));
                }
            }
        } break;
        case ArenaKind::Psychic:
            for (const Portal& po : field.portals) {
                Vector2 c = ToScreen(po.pos);
                float grow = std::min(1.0f, po.age / 0.4f);
                float r = 16 * grow;
                if (r < 1) continue;
                DrawEllipse((int)c.x, (int)c.y, r + 3, (r + 3) * DEPTH, Fade(kPink, 0.18f + 0.08f * std::sin(t * 5)));
                DrawEllipse((int)c.x, (int)c.y, r, r * DEPTH, Color{30, 6, 48, 255});
                DrawEllipse((int)c.x, (int)c.y, r * 0.6f, r * 0.6f * DEPTH, Color{90, 30, 130, 255});
                EllipsePix(c.x, c.y, r, r * DEPTH, kPink);
                EllipsePix(c.x, c.y, r - 1, (r - 1) * DEPTH, Fade(kPink, 0.6f));
                EllipsePix(c.x, c.y, r * 0.65f, r * 0.65f * DEPTH, Fade(Color{150, 220, 255, 255}, 0.8f));
                for (int k = 0; k < 6; k++) {
                    float a = t * 4 + k * PI_F / 3;
                    float rr = r * (0.3f + 0.6f * std::fmod(t * 0.8f + k * 0.17f, 1.0f));
                    DrawRectangle((int)(c.x + std::cos(a) * rr), (int)(c.y + std::sin(a) * rr * DEPTH), 2, 1, WHITE);
                }
                if (field.portalT < 2.0f && ((int)(t * 8) % 2)) EllipsePix(c.x, c.y, r + 2, (r + 2) * DEPTH, WHITE);
            }
            break;
        case ArenaKind::Ground:
            for (const Turret& tu : field.turrets) {
                if (tu.warnT < 0) continue;
                Vector2 dir = SafeNorm(tu.aimAt - tu.pos, tu.dir);
                Vector2 a = ToScreen(tu.pos), b = ToScreen(tu.pos + dir * 900.0f);
                float blink = ((int)(t * 10) % 2) ? 0.9f : 0.45f;
                DrawLineEx(a, b, 3, Fade(Color{255, 40, 40, 255}, 0.18f));
                DrawLineEx(a, b, 1, Fade(Color{255, 60, 60, 255}, blink));
            }
            break;
        default: break;
    }
    // Fallen trees and pillar rubble lie on the floor.
    for (const Obstacle& o : field.obs) {
        if (o.alive) continue;
        if (o.kind == Obstacle::Tree) {
            Vector2 a = ToScreen(o.pos), b = ToScreen(o.pos + o.fallDir * o.height);
            DrawLineEx(a, b, 4, Color{90, 62, 40, 255});
            DrawLineEx(a, b, 1, Color{60, 42, 28, 255});
            DrawCircle((int)b.x, (int)b.y, 5, Color{60, 50, 40, 255});
        } else if (o.kind == Obstacle::Pillar) {
            Vector2 a = ToScreen(o.pos);
            DrawEllipse((int)a.x, (int)a.y, o.radius, o.radius * 0.45f, Color{110, 98, 86, 255});
            DrawRectangle((int)a.x - 5, (int)a.y - 3, 4, 3, Color{140, 126, 110, 255});
            DrawRectangle((int)a.x + 2, (int)a.y - 2, 3, 2, Color{130, 116, 100, 255});
            if (o.respawnT < 3) EllipsePix(a.x, a.y, o.radius, o.radius * 0.45f, Fade(WHITE, 0.5f + 0.5f * std::sin(t * 12)));
        }
    }
}

void Battle::DrawFieldTop() const {
    if (field.kind == ArenaKind::Ground) {
        for (const Turret& tu : field.turrets) {
            Vector2 c = ToScreen(tu.pos, tu.dir.y > 0 ? 10.0f : 6.0f);
            bool warn = tu.warnT >= 0;
            Color body{70, 70, 78, 255};
            if (tu.dir.y > 0) {  // back wall
                DrawRectangle((int)c.x - 6, (int)c.y - 6, 12, 8, body);
                DrawRectangle((int)c.x - 2, (int)c.y + 2, 4, 4, Color{40, 40, 46, 255});
            } else {
                int sx = tu.dir.x > 0 ? (int)c.x - 8 : (int)c.x;
                DrawRectangle(sx, (int)c.y - 5, 8, 9, body);
                DrawRectangle(tu.dir.x > 0 ? sx + 8 : sx - 4, (int)c.y - 2, 4, 3, Color{40, 40, 46, 255});
            }
            Color light = warn ? (((int)(time * 10) % 2) ? Color{255, 40, 40, 255} : Color{120, 20, 20, 255}) : Color{60, 120, 60, 255};
            DrawRectangle((int)c.x - 1, (int)c.y - 4, 2, 2, light);
        }
    }
    if (field.kind == ArenaKind::Electric) {  // arcs from coils to creatures in range
        for (const Obstacle& o : field.obs) {
            if (o.kind != Obstacle::Coil || !o.alive) continue;
            for (int s = 0; s < N(); s++) {
                const Fighter& f = sides[s].Cur();
                if (f.state != FState::Active || f.burrowed) continue;
                bool elec = HasType(f.sp, Type::Electric);
                float d = Dist(o.pos, f.pos);
                if (d > (elec ? 60.0f : 48.0f) || SimRand(0, 2) != 0) continue;
                Vector2 a = ToScreen(o.pos, o.height - 2), b = ToScreen(f.pos, 8);
                Vector2 prev = a;
                for (int i = 1; i <= 4; i++) {
                    Vector2 p = LerpV(a, b, i / 4.0f) + (i < 4 ? Vector2{Frand(-4, 4), Frand(-4, 4)} : Vector2{0, 0});
                    DrawLineEx(prev, p, elec ? 2.0f : 1.0f, elec ? Color{140, 220, 255, 255} : kYellow);
                    prev = p;
                }
            }
        }
    }
}
