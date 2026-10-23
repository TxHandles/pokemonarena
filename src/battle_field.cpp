// Gym arena mechanics: obstacles, zones, portals, turrets, lava and electrified water.
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

constexpr float BODY_R = 9.0f;
constexpr float PI_F = 3.14159265f;

float Frand(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }
bool Chance(float p) { return p > 0 && Frand(0, 1) < p; }
bool HasType(const SpeciesDef* sp, Type t) { return sp->type1 == t || sp->type2 == t; }

const Color kYellow{250, 225, 70, 255};
const Color kPink{230, 120, 240, 255};
const Color kWisp{150, 120, 255, 255};
const Color kSpot{255, 236, 170, 255};

} // namespace

// ---------------------------------------------------------------- Per-frame field update

void Battle::UpdateField(float dt) {
    UpdateCenterpiece(dt);
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
                    if (Hittable(ts) && DistPointSeg(sides[ts].Cur().pos, a, b) < HitR(sides[ts].Cur()) + 6) ApplyHit(h, ts, o.fallDir);
                Burst(b, Color{90, 160, 60, 255}, 16, 70, 4);
                Shake(2);
                PlaySfx(Sfx::Thud);
            }
            if (o.fallT >= 0.7f) { o.alive = false; o.fallT = -1; }
        }
        if (!o.alive && o.kind == Obstacle::Pillar && (field.Rules() == ArenaKind::Rock || field.Rules() == ArenaKind::Ice || field.kind == ArenaKind::SinnohChampion ||
                                                       field.kind == ArenaKind::SpearTime || field.kind == ArenaKind::SpearSpace)) {   // the Stone Gym's pillars (and Lorelei's ice) grow back
            o.respawnT -= dt;
            bool clear = true;
            for (int s = 0; s < N(); s++)
                if (Dist(sides[s].Cur().pos, o.pos) < o.radius + BODY_R + 4) clear = false;
            if (o.respawnT <= 0 && clear) {
                o.alive = true;
                bool ice = field.Rules() == ArenaKind::Ice;
                Burst(o.pos, ice ? Color{200, 240, 255, 255} : Color{150, 130, 110, 255}, 14, 50, 4);
                AddText(o.pos, ice ? "The ice pillar froze again!" : "The pillar rises again!", UI_DIM, 36);
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

    if (field.Rules() == ArenaKind::Fire && winner < 0 && introT <= 0) {
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

    bool live = winner < 0 && introT <= 0;
    if (field.Rules() == ArenaKind::Ghost && live) UpdateWisps(dt);
    if ((field.kind == ArenaKind::Dragon || field.kind == ArenaKind::JohtoChampion) && live) UpdateStatues(dt);
    if (field.Rules() == ArenaKind::Champion) UpdateSpotlight(dt);
    if ((field.kind == ArenaKind::Flying || field.kind == ArenaKind::SkyPillar) && live) UpdateWind(dt);
    if (field.Rules() == ArenaKind::Dark) UpdateLights(dt);
    if (IsLegendArena(field.kind) && live) UpdateLegendField(dt);
    UpdateHoenn(dt);
    UpdateGen45(dt);

    for (int s = 0; s < N(); s++) FieldEffectsOn(s, dt);
}

// Will-o'-wisps rise from the graves and drift toward the nearest creature that isn't a Ghost.
void Battle::UpdateWisps(float dt) {
    for (Wisp& w : field.wisps) {
        if (w.life <= 0) {
            w.respawnT -= dt;
            if (w.respawnT > 0) continue;
            std::vector<const Obstacle*> graves;
            for (const Obstacle& o : field.obs) if (o.kind == Obstacle::Pillar && o.alive) graves.push_back(&o);
            Vector2 p{Frand(150, ARENA_W - 150), Frand(40, ARENA_H - 40)};
            if (!graves.empty()) { const Obstacle* gr = graves[SimRand(0, (int)graves.size() - 1)]; p = gr->pos + Vector2{Frand(-10, 10), 8}; }
            w.pos = p;
            w.vel = {0, 0};
            w.life = Frand(10, 14);
            Burst(p, kWisp, 10, 40, 10);
            continue;
        }
        w.life -= dt;
        int best = -1;
        float bd = 1e9f;
        for (int s = 0; s < N(); s++) {
            const Fighter& f = sides[s].Cur();
            if (!Targetable(s) || f.burrowed || HasType(f.sp, Type::Ghost)) continue;
            float d = Dist(f.pos, w.pos);
            if (d < bd) { bd = d; best = s; }
        }
        Vector2 want = best >= 0 ? SafeNorm(sides[best].Cur().pos - w.pos) * 40.0f
                                 : Vector2{std::sin(time * 0.8f + (w.seed % 7)), std::cos(time * 0.6f + (w.seed % 5))} * 20.0f;
        w.vel = w.vel + (want - w.vel) * std::min(1.0f, dt * 1.4f);
        w.pos += w.vel * dt;
        w.pos.x = std::max(10.0f, std::min(ARENA_W - 10, w.pos.x));
        w.pos.y = std::max(10.0f, std::min(ARENA_H - 10, w.pos.y));
        if (best >= 0 && bd < BODY_R + 6 && Hittable(best)) {
            HitSpec h = EnvSpec(Type::Ghost, 32, Status::Burn, 0.25f);
            h.special = true;
            ApplyHit(h, best, SafeNorm(sides[best].Cur().pos - w.pos, Vector2{1, 0}));
            AddText(w.pos, "Will-o'-wisp!", kWisp, 30);
            Burst(w.pos, kWisp, 16, 60, 10);
            PlaySfx(Sfx::Fire);
            w.life = 0;
            w.respawnT = Frand(4.5f, 7.0f);
        } else if (w.life <= 0) {
            Burst(w.pos, kWisp, 6, 25, 10);
            w.respawnT = 1.5f;
        }
    }
}

// Lance's dragon statues: one glows, then breathes a fan of dragonfire at someone.
void Battle::UpdateStatues(float dt) {
    if (field.turrets.empty()) return;
    field.turretT -= dt;
    if (field.turretT <= 0) {
        field.turretT = Frand(7.0f, 9.0f);
        Turret& t = field.turrets[SimRand(0, (int)field.turrets.size() - 1)];
        std::vector<int> live;
        for (int s = 0; s < N(); s++) if (Targetable(s)) live.push_back(s);
        t.warnT = 1.6f;
        t.aimAt = live.empty() ? Vector2{ARENA_W / 2, ARENA_H / 2} : sides[live[SimRand(0, (int)live.size() - 1)]].Cur().pos;
        AddText({ARENA_W / 2, ARENA_H * 0.1f}, "A dragon statue's eyes are glowing...", Color{255, 150, 90, 255}, 20);
        PlaySfx(Sfx::Status);
    }
    for (Turret& t : field.turrets) {
        if (t.warnT < 0) continue;
        t.warnT -= dt;
        if (t.warnT > 0) continue;
        t.warnT = -1;
        Vector2 dir = SafeNorm(t.aimAt - t.pos, t.dir);
        for (int k = -1; k <= 1; k++) {
            Projectile p;
            p.hit = EnvSpec(Type::Fire, 50, Status::Burn, 0.2f);
            p.hit.special = true;
            Vector2 d = Rot(dir, k * 0.17f);
            p.pos = t.pos + d * 10.0f;
            p.vel = d * 230.0f;
            p.maxDist = 900;
            p.radius = 7;
            p.z = 10;
            projs.push_back(p);
        }
        Burst(t.pos + dir * 10.0f, Color{255, 140, 60, 255}, 18, 80, 12);
        Shake(2);
        PlaySfx(Sfx::Fire);
    }
}

// Legendary lairs: icicles / lightning / burning rocks / boulders fall where the markers glow
// (only on the challengers), Mewtwo's psychic pulse, and Mew teleporting around its island.
void Battle::UpdateLegendField(float dt) {
    int boss = -1;
    for (int s = 0; s < N(); s++) if (sides[s].teamId == 1) { boss = s; break; }
    if (boss < 0) return;
    Fighter& bf = sides[boss].Cur();
    bool up = bf.state == FState::Active && bf.hp > 0 && !bf.weakened;
    if (!up) return;
    struct Strike { Type t; int power; Status st; float chance; int count; float every; float radius; };
    Strike c{Type::Normal, 0, Status::None, 0, 0, 5, 15};
    switch (field.kind) {
        case ArenaKind::Seafoam: c = {Type::Ice, 50, Status::Freeze, 0.1f, 2, 5.0f, 15}; break;
        case ArenaKind::PowerPlant: c = {Type::Electric, 55, Status::Paralyze, 0.3f, 2, 4.5f, 15}; break;
        case ArenaKind::Ember: c = {Type::Fire, 50, Status::Burn, 0.3f, 2, 5.0f, 15}; break;
        case ArenaKind::Unknown: c = {Type::Psychic, 60, Status::None, 0, 1, 3.5f, 18}; break;
        case ArenaKind::DesertRuins: c = {Type::Rock, 50, Status::None, 0, 2, 5.0f, 16}; break;
        case ArenaKind::AncientTomb: c = {Type::Steel, 50, Status::None, 0, 2, 5.0f, 15}; break;
        case ArenaKind::MarineCave: c = {Type::Water, 55, Status::None, 0, 2, 4.5f, 17}; break;
        case ArenaKind::SkyPillar: c = {Type::Dragon, 55, Status::None, 0, 2, 5.0f, 16}; break;
        case ArenaKind::BirthIsland: c = {Type::Psychic, 55, Status::None, 0, 2, 5.0f, 15}; break;
        case ArenaKind::SpearTime: c = {Type::Dragon, 55, Status::None, 0, 2, 5.0f, 15}; break;
        case ArenaKind::Newmoon: c = {Type::Dark, 55, Status::None, 0, 2, 5.0f, 15}; break;
        case ArenaKind::HallOrigin: c = {(Type)field.xI[0], 55, Status::None, 0, 2, 5.0f, 16}; break;
        case ArenaKind::Dragonspiral: c = {bf.sp->name == "Reshiram" ? Type::Fire : Type::Electric, 55, bf.sp->name == "Reshiram" ? Status::Burn : Status::Paralyze, 0.2f, 2, 4.5f, 15}; break;
        case ArenaKind::GiantChasm: c = {Type::Ice, 55, Status::Freeze, 0.1f, 2, 5.0f, 15}; break;
        default: break;
    }
    if (c.count > 0) {
        field.strikeT -= dt;
        if (field.strikeT <= 0) {
            field.strikeT = c.every * Frand(0.85f, 1.15f);
            std::vector<int> foes;
            for (int s = 0; s < N(); s++) if (sides[s].teamId == 0 && Targetable(s)) foes.push_back(s);
            for (int k = 0; k < c.count && !foes.empty(); k++) {
                int ts = foes[SimRand(0, (int)foes.size() - 1)];
                Projectile p;
                p.hit = EnvSpec(c.t, c.power, c.st, c.chance);
                p.hit.side = boss;   // only the challengers get hit
                p.hit.special = true;
                p.drop = true;
                Vector2 at = sides[ts].Cur().pos + Vector2{Frand(-22, 22), Frand(-16, 16)};
                at.x = std::max(6.0f, std::min(ARENA_W - 6, at.x));
                at.y = std::max(6.0f, std::min(ARENA_H - 4, at.y));
                p.target = p.pos = at;
                p.dur = 1.25f;
                p.t = -k * 0.3f;
                p.z = field.kind == ArenaKind::MarineCave ? 0.0f : 110.0f;   // Kyogre's spouts burst up from below
                if (field.kind == ArenaKind::MarineCave) p.hit.knock = 2.2f;
                p.radius = c.radius;
                p.spin = Frand(0, 6);
                projs.push_back(p);
            }
            if (field.kind == ArenaKind::Unknown) AddText(bf.pos, bf.Name() + " hurls a boulder with its mind!", Color{220, 140, 255, 255}, 30);
        }
    }
    if (field.kind == ArenaKind::Unknown) {   // a psychic pulse ripples out from Mewtwo
        field.pulseT -= dt;
        if (field.pulseT <= 0) {
            field.pulseT = Frand(9.0f, 12.0f);
            ShockWave w;
            w.hit = EnvSpec(Type::Psychic, 45);
            w.hit.side = boss;
            w.hit.special = true;
            w.origin = bf.pos;
            w.maxR = 160;
            w.speed = 150;
            waves.push_back(w);
            AddText(bf.pos, "A wave of psychic power!", Color{220, 140, 255, 255}, 34);
            Shake(2);
            PlaySfx(Sfx::Buff);
        }
    }
    if (field.kind == ArenaKind::BirthIsland) {   // Deoxys teleports across its island
        field.blinkT -= dt;
        if (field.blinkT <= 0) {
            field.blinkT = Frand(8.0f, 11.0f);
            Vector2 to{Frand(90, ARENA_W - 90), Frand(60, ARENA_H - 60)};
            for (int tries = 0; tries < 8; tries++) {
                bool ok = Dist(to, bf.pos) > 120;
                for (int s = 0; s < N(); s++) if (sides[s].teamId == 0 && Dist(sides[s].Cur().pos, to) < 70) ok = false;
                if (ok) break;
                to = {Frand(90, ARENA_W - 90), Frand(60, ARENA_H - 60)};
            }
            Burst(bf.pos, Color{255, 120, 90, 255}, 16, 70, 10);
            bf.pos = to;
            bf.pendingMove = -1;
            bf.dashing = false;
            bf.knock = {0, 0};
            Burst(bf.pos, Color{120, 220, 255, 255}, 16, 70, 10);
            AddText(bf.pos, bf.Name() + " teleported!", Color{120, 220, 255, 255}, 34);
            PlaySfx(Sfx::Shield);
        }
    }
    if (field.kind == ArenaKind::Faraway) {   // Mew teleports to another patch of tall grass
        field.blinkT -= dt;
        if (field.blinkT <= 0) {
            field.blinkT = Frand(8.0f, 11.0f);
            std::vector<const Zone*> grass;
            for (const Zone& z : field.zones) if (z.kind == Zone::TallGrass && Dist(z.pos, bf.pos) > 80) grass.push_back(&z);
            if (!grass.empty()) {
                const Zone* z = grass[SimRand(0, (int)grass.size() - 1)];
                Burst(bf.pos, kPink, 16, 70, 10);
                bf.pos = z->pos;
                bf.pendingMove = -1;
                bf.dashing = false;
                bf.knock = {0, 0};
                Burst(bf.pos, kPink, 16, 70, 10);
                AddText(bf.pos, bf.Name() + " teleported!", kPink, 34);
                PlaySfx(Sfx::Shield);
            }
        }
    }
}

// Falkner's gym: calm, then the windsocks swing round (a warning), then a gust shoves everyone on
// the ground across the deck. Hit the weather vane and the next gusts blow only on your foes.
void Battle::UpdateWind(float dt) {
    const Centerpiece& vane = field.center;
    int owner = vane.kind == Centerpiece::Vane && vane.t > 0 ? vane.owner : -1;
    field.windT -= dt;
    if (field.windOn == 0 && field.windT <= 0) {
        field.windOn = 1;
        field.windT = 1.6f;
        Vector2 d{SimRand(0, 1) ? 1.0f : -1.0f, Frand(-0.6f, 0.6f)};
        if (owner >= 0) {   // from the vane's team toward its foes
            Vector2 a{0, 0}, b{0, 0};
            int na = 0, nb = 0;
            for (int s = 0; s < N(); s++) {
                if (!Targetable(s)) continue;
                if (sides[s].teamId == owner) { a += sides[s].Cur().pos; na++; } else { b += sides[s].Cur().pos; nb++; }
            }
            if (na && nb) d = b * (1.0f / nb) - a * (1.0f / na);
        }
        field.windDir = SafeNorm(d, Vector2{1, 0});
        const char* from = field.windDir.x > 0.3f ? "the west" : field.windDir.x < -0.3f ? "the east" : field.windDir.y > 0 ? "the north" : "the south";
        AddText({ARENA_W / 2, ARENA_H * 0.1f}, owner >= 0 ? TextFormat("The vane calls a gust from %s!", from) : TextFormat("A gust is coming from %s!", from), Color{200, 230, 255, 255}, 20);
        PlaySfx(Sfx::Status);
    } else if (field.windOn == 1 && field.windT <= 0) {
        field.windOn = 2;
        field.windT = Frand(2.6f, 3.4f);
        Shake(1, 0.3f);
        PlaySfx(Sfx::Buff);
    } else if (field.windOn == 2 && field.windT <= 0) {
        field.windOn = 0;
        field.windT = Frand(6.5f, 9.0f);
    }
    if (field.windOn == 2 && SimRand(0, 1) == 0) {   // streaks of wind (and a leaf or two) blowing across
        Vector2 p{field.windDir.x > 0 ? 4.0f : field.windDir.x < 0 ? ARENA_W - 4 : Frand(0, ARENA_W), Frand(10, ARENA_H - 10)};
        if (std::fabs(field.windDir.x) < 0.3f) p = {Frand(10, ARENA_W - 10), field.windDir.y > 0 ? 4.0f : ARENA_H - 4};
        Emit(PK::Smoke, p, Frand(2, 30), field.windDir * Frand(220, 300), 0, Frand(1.6f, 2.4f), 1.2f, Color{235, 245, 255, 150}, false);
    }
}

// Karen's chamber: three pools of light drift to new places every few seconds.
void Battle::UpdateLights(float dt) {
    field.lightT -= dt;
    if (field.lightT <= 0) {
        field.lightT = Frand(5.0f, 8.0f);
        for (size_t i = 0; i < field.lightTo.size(); i++)
            field.lightTo[i] = {Frand(70, ARENA_W - 70), Frand(55, ARENA_H - 55)};
    }
    for (size_t i = 0; i < field.lights.size() && i < field.lightTo.size(); i++) {
        Vector2 d = field.lightTo[i] - field.lights[i];
        float l = Len(d);
        if (l > 0.5f) field.lights[i] += d * (std::min(l, 34.0f * dt) / l);
    }
}

// The Champion's spotlight glides to a new spot every few seconds.
void Battle::UpdateSpotlight(float dt) {
    field.spotT -= dt;
    if (field.spotT <= 0) {
        field.spotT = Frand(6.0f, 9.0f);
        field.spotTo = {Frand(110, ARENA_W - 110), Frand(60, ARENA_H - 60)};
    }
    Vector2 d = field.spotTo - field.spot;
    float l = Len(d);
    if (l > 0.5f) field.spot += d * (std::min(l, 55.0f * dt) / l);
}

void Battle::FieldEffectsOn(int s, float dt) {
    Fighter& f = sides[s].Cur();
    f.envSpeed = 1;
    f.envAtk = 1;
    f.envDef = 1;
    f.portalCd = std::max(0.0f, f.portalCd - dt);
    if (f.state != FState::Active || f.burrowed || f.hp <= 0) return;
    const SpeciesDef* sp = f.sp;
    bool air = IsAirborne(*sp) || f.jumpT > 0;   // a jump clears the water, puddles and pools
    f.envTickT -= dt;
    bool tick = false;
    if (f.envTickT <= 0) { f.envTickT = 0.5f; tick = true; }
    int os = AnyFoe(s);

    if (f.boss > 0 && field.kind == ArenaKind::Faraway && field.InZone(f.pos, Zone::TallGrass)) {   // Mew hides in the grass
        f.envSpeed = 1.1f;
        if (f.hp < f.maxHp && !f.weakened) {
            f.regenAcc += f.maxHp * 0.0015f * dt;
            if (f.regenAcc >= 1) { float a = std::floor(f.regenAcc); f.hp = std::min((float)f.maxHp, f.hp + a); f.regenAcc -= a; }
        }
    }
    switch (field.Rules()) {
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
        case ArenaKind::Ice:
            if (HasType(sp, Type::Ice) && !air && OnIce(f.pos)) {   // Ice types skate
                f.envSpeed = 1.3f;
                f.envAtk = 1.1f;
                if (f.moving && SimRand(0, 3) == 0) Emit(PK::Snow, f.pos + Vector2{Frand(-6, 6), Frand(-2, 2)}, 1, {Frand(-15, 15), 0}, Frand(10, 25), 0.4f, 1.5f, Color{220, 245, 255, 255}, true);
            }
            break;
        case ArenaKind::Fighting: {
            if (HasType(sp, Type::Fighting) && Dist(f.pos, Vector2{ARENA_W / 2, ARENA_H / 2}) < 72) {
                f.envAtk = 1.3f;
                if (SimRand(0, 6) == 0) Emit(PK::Spark, f.pos + Vector2{Frand(-7, 7), Frand(-3, 3)}, Frand(4, 14), {0, 0}, 25, 0.4f, 1.5f, Color{255, 170, 80, 255});
            }
            // The ropes: knocked into them, you bounce back into the ring.
            const float band = 20;
            Vector2 k = f.knock;
            bool bounce = false;
            if (f.portalCd <= 0) {
                if (f.pos.x < band && k.x < -40) { k.x = -k.x * 1.3f; bounce = true; }
                if (f.pos.x > ARENA_W - band && k.x > 40) { k.x = -k.x * 1.3f; bounce = true; }
                if (f.pos.y < band * 0.8f && k.y < -40) { k.y = -k.y * 1.3f; bounce = true; }
                if (f.pos.y > ARENA_H - band * 0.8f && k.y > 40) { k.y = -k.y * 1.3f; bounce = true; }
            }
            if (bounce) {
                f.knock = k;
                f.portalCd = 0.6f;
                AddText(f.pos, "Off the ropes!", Color{255, 190, 90, 255}, 30);
                Shake(1);
                PlaySfx(Sfx::Thud);
                DirectDamage(s, f.maxHp * 0.03f, Color{255, 190, 90, 255}, os);
            }
        } break;
        case ArenaKind::Ghost:
            if (HasType(sp, Type::Ghost)) f.envSpeed = 1.15f;
            break;
        case ArenaKind::Dragon:
            if (HasType(sp, Type::Dragon)) f.envAtk = 1.2f;
            break;
        case ArenaKind::Champion:
            if (field.InSpot(f.pos)) {
                f.envAtk = 1.25f;
                f.envSpeed = 1.15f;
                if (SimRand(0, 4) == 0) Emit(PK::Star, f.pos + Vector2{Frand(-9, 9), Frand(-3, 3)}, Frand(4, 20), {0, 0}, 20, 0.5f, 1.5f, kSpot);
            }
            break;
        case ArenaKind::Flying: {
            const Centerpiece& vane = field.center;
            int owner = vane.kind == Centerpiece::Vane && vane.t > 0 ? vane.owner : -1;
            if (HasType(sp, Type::Flying)) f.envSpeed = 1.25f;
            if (field.windOn != 2) break;
            if (owner >= 0 && sides[s].teamId == owner) { f.envSpeed = std::max(f.envSpeed, 1.15f); break; }   // a tailwind
            if (air || HasType(sp, Type::Flying) || f.dashing || Held(s)) break;
            bool heavy = HasType(sp, Type::Steel) || HasType(sp, Type::Rock);
            Vector2 np = f.pos + field.windDir * ((heavy ? 32.0f : 64.0f) * dt);
            f.pos = {std::max(BODY_R, std::min(ARENA_W - BODY_R, np.x)), std::max(BODY_R, std::min(ARENA_H - BODY_R, np.y))};
            if (f.moving && Dot(f.moveVel, field.windDir) < 0) f.envSpeed = std::min(f.envSpeed, 0.8f);   // walking into the wind
            if (SimRand(0, 5) == 0) Emit(PK::Smoke, f.pos, 2, field.windDir * 60.0f, 4, 0.4f, 2.0f, Color{220, 225, 230, 200}, false).grow = 3;
        } break;
        case ArenaKind::Bug:
            if (!air && field.InZone(f.pos, Zone::Web)) {
                if (HasType(sp, Type::Bug)) {
                    f.envSpeed = 1.25f;
                    f.envAtk = 1.15f;
                } else {
                    f.envSpeed = 0.55f;
                    if (f.moving && SimRand(0, 7) == 0) Emit(PK::Glow, f.pos + Vector2{Frand(-6, 6), Frand(-2, 2)}, Frand(2, 10), {0, 0}, 4, 0.4f, 1.0f, Color{240, 240, 230, 255}, false);
                }
            }
            break;
        case ArenaKind::Normal:
            if (HasType(sp, Type::Normal)) f.envAtk = 1.15f;
            break;
        case ArenaKind::Steel:
            if (field.InBeam(f.pos)) {
                if (HasType(sp, Type::Steel)) {
                    f.envAtk = 1.25f;
                    if (SimRand(0, 4) == 0) Emit(PK::Star, f.pos + Vector2{Frand(-8, 8), Frand(-3, 3)}, Frand(4, 18), {0, 0}, 18, 0.4f, 1.4f, Color{255, 245, 200, 255});
                } else {
                    f.envSpeed = 0.75f;   // dazzled
                }
            }
            break;
        case ArenaKind::Dark: {
            bool lit = field.InLight(f.pos);
            if (HasType(sp, Type::Dark)) {
                if (!lit) { f.envAtk = 1.2f; f.envSpeed = 1.15f; }
            } else if (lit) {
                f.envAtk = 1.1f;
            }
        } break;
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
    HoennEffectsOn(s, dt);
    Gen45EffectsOn(s, dt);
}

// ---------------------------------------------------------------- Obstacles

void Battle::ObstacleImpact(Obstacle& o, const HitSpec& h, Vector2 dir, bool physical) {
    if (!o.Blocks()) return;
    if (o.kind == Obstacle::Center) { CenterHit(h, dir, physical); return; }
    if (field.Rules() == ArenaKind::Ice && o.kind == Obstacle::Pillar && h.type == Type::Fire && h.power > 0) {
        o.alive = false;
        o.respawnT = 12.0f;
        for (int k = 0; k < 10; k++)
            Emit(PK::Smoke, o.pos + Vector2{Frand(-o.radius, o.radius), Frand(-3, 3)}, Frand(4, o.height), {Frand(-10, 10), 0}, Frand(20, 40), Frand(0.6f, 1.0f), Frand(2, 4), Color{230, 235, 245, 200});
        AddText(o.pos, "The ice pillar melted!", Color{255, 170, 90, 255}, 36);
        PlaySfx(Sfx::Fire);
        return;
    }
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
    CenterReact(at, h, radius);
    HoennFieldReact(at, h, radius);
    Gen45FieldReact(at, h, radius);
    if (h.type == Type::Fire && field.kind == ArenaKind::Bug) {   // Bugsy's webs go up in flames
        for (const Zone& z : field.zones) {
            if (z.kind != Zone::Web || !z.Contains(at, radius + 6)) continue;
            for (int k = 0; k < 14; k++)
                Emit(PK::Flame, z.pos + Vector2{Frand(-z.rx, z.rx) * 0.8f, Frand(-z.ry, z.ry) * 0.8f}, Frand(2, 8), {Frand(-10, 10), 0}, Frand(30, 60), Frand(0.4f, 0.8f), Frand(1.6f, 2.6f), Color{255, 150, 50, 255}, false);
            AddText(z.pos, "The web burned away!", Color{255, 160, 80, 255}, 34);
        }
        field.zones.erase(std::remove_if(field.zones.begin(), field.zones.end(), [&](const Zone& z) { return z.kind == Zone::Web && z.Contains(at, radius + 6); }), field.zones.end());
    }
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
                pd.thawT = 0;
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
                    if (pd.rink) pd.thawT = 7.0f;   // the rink freezes over again in a while
                    melted = true;
                } else if (pd.rink) {
                    pd.thawT = std::max(pd.thawT, 5.0f);
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
        if (pd.rink) {   // Lorelei's rinks stay frozen; melted ones freeze again
            if (pd.ice > 0) pd.ice = std::max(pd.ice, 2.0f);
            else if ((pd.thawT -= dt) <= 0) {
                pd.ice = 2.0f;
                Burst(pd.pos, Color{200, 240, 255, 255}, 10, 40, 2);
                AddText(pd.pos, "The rink froze over again!", Color{170, 230, 255, 255}, 26);
            }
            continue;
        }
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
    pull += CenterAvoid(s);

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

    switch (field.Rules()) {
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
        case ArenaKind::Ice:
            if (HasType(sp, Type::Ice) && !OnIce(f.pos)) {
                const Puddle* best = nullptr;
                float bd = 1e9f;
                for (const Puddle& pd : field.puddles) if (pd.rink && pd.ice > 0 && Dist(pd.pos, f.pos) < bd) { bd = Dist(pd.pos, f.pos); best = &pd; }
                if (best && bd < 220) pull += SafeNorm(best->pos - f.pos) * 0.7f;
            }
            break;
        case ArenaKind::Fighting: {
            Vector2 c{ARENA_W / 2, ARENA_H / 2};
            if (HasType(sp, Type::Fighting) && Dist(f.pos, c) > 60) pull += SafeNorm(c - f.pos) * 0.8f;
        } break;
        case ArenaKind::Ghost:
            if (!HasType(sp, Type::Ghost))
                for (const Wisp& w : field.wisps)
                    if (w.life > 0 && Dist(w.pos, f.pos) < 75) pull += SafeNorm(f.pos - w.pos, Vector2{0, 1}) * 1.3f;
            break;
        case ArenaKind::Champion:
            if (!field.InSpot(f.pos) && Dist(field.spot, f.pos) < 260) pull += SafeNorm(field.spot - f.pos) * 0.8f;
            if (field.kind == ArenaKind::JohtoChampion)   // and keep out of the dragonfire
                for (const Turret& t : field.turrets) {
                    if (t.warnT < 0) continue;
                    Vector2 dir = SafeNorm(t.aimAt - t.pos, t.dir);
                    Vector2 rel = f.pos - t.pos;
                    float along = Dot(rel, dir);
                    if (along < 0) continue;
                    Vector2 lat = rel - dir * along;
                    if (Len(lat) < 24 + along * 0.17f) pull += SafeNorm(lat, Vector2{-dir.y, dir.x}) * 1.6f;
                }
            break;
        case ArenaKind::Bug:
            if (air) break;
            if (HasType(sp, Type::Bug)) {
                float d;
                const Zone* z = nearestZone(Zone::Web, d);
                if (z && !z->Contains(f.pos) && d < 200) pull += SafeNorm(z->pos - f.pos) * 0.7f;
            } else {
                avoidZones(Zone::Web);
            }
            break;
        case ArenaKind::Normal: {
            const Centerpiece& c = field.center;
            if (c.kind == Centerpiece::MilkTank && c.phase == 1 && f.hp < f.maxHp * 0.7f) {
                const Vector2* best = nullptr;
                float bd = 1e9f;
                for (const Vector2& m : c.marks) if (Dist(m, f.pos) < bd) { bd = Dist(m, f.pos); best = &m; }
                if (best && bd > 10 && bd < 240) pull += SafeNorm(*best - f.pos) * 1.0f;
            }
        } break;
        case ArenaKind::Steel:
            if (!HasType(sp, Type::Steel) && field.InBeam(f.pos)) {   // step out of the light, ahead of the sweep
                Vector2 rel = f.pos - field.center.pos;
                Vector2 side{-rel.y, rel.x};
                pull += SafeNorm(side * (field.center.spinRate >= 0 ? 1.0f : -1.0f), Vector2{0, 1}) * 1.0f;
            }
            break;
        case ArenaKind::Dark: {
            const Vector2* best = nullptr;
            float bd = 1e9f;
            for (const Vector2& l : field.lights) if (Dist(l, f.pos) < bd) { bd = Dist(l, f.pos); best = &l; }
            if (!best) break;
            if (HasType(sp, Type::Dark)) {
                if (field.InLight(f.pos)) pull += SafeNorm(f.pos - *best, Vector2{0, 1}) * 1.0f;
            } else if (!field.InLight(f.pos) && bd < 220) {
                pull += SafeNorm(*best - f.pos) * 0.7f;
            }
        } break;
        case ArenaKind::Dragon:
            for (const Turret& t : field.turrets) {
                if (t.warnT < 0) continue;
                Vector2 dir = SafeNorm(t.aimAt - t.pos, t.dir);
                Vector2 rel = f.pos - t.pos;
                float along = Dot(rel, dir);
                if (along < 0) continue;
                Vector2 lat = rel - dir * along;
                if (Len(lat) < 24 + along * 0.17f) pull += SafeNorm(lat, Vector2{-dir.y, dir.x}) * 1.6f;
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
    pull += HoennPull(s);
    pull += Gen45Pull(s);
    return pull;
}

// ---------------------------------------------------------------- Drawing

void Battle::DrawObstacle(const Obstacle& o) const {
    Vector2 b = ToScreen(o.pos);
    int x = (int)std::round(b.x), y = (int)std::round(b.y);
    int r = (int)o.radius, h = (int)o.height;
    switch (o.kind) {
        case Obstacle::Center: DrawCenterBody(o); return;
        case Obstacle::Pillar: {
            if (!o.alive) return;
            // The look depends on where it stands, and its seed picks a variant.
            int style = 0;   // 0 column, 1 broken column, 2 stacked boulders, 3 jagged spire, 4 round boulder
            Venue vn = field.venue;
            if (field.kind == ArenaKind::Unknown) style = 16;
            else if (field.Rules() == ArenaKind::Ice) style = 12;
            else if (field.kind == ArenaKind::Ghost) style = 10;
            else if (field.kind == ArenaKind::Fighting) style = 2;
            else if (field.kind == ArenaKind::Dragon) style = 15;
            else if (field.kind == ArenaKind::Flying) style = 17;
            else if (field.kind == ArenaKind::Dark) style = 18;
            else if (vn == Venue::Tower) style = 10;
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
                case 16: {   // Cerulean Cave: a glowing violet crystal cluster
                    Color cc{150, 110, 220, 235}, cd{96, 64, 160, 255}, cl{235, 210, 255, 255};
                    DrawEllipse(x, y + 1, (float)r + 2, (r + 2) * 0.45f, Fade(BLACK, 0.3f));
                    for (int k = 0; k < 3; k++) {
                        float ox = (k - 1) * r * 0.6f, hh = h * (k == 1 ? 1.0f : 0.6f + rv(40 + k) * 0.2f);
                        Vector2 a{(float)x + ox - r * 0.38f, (float)y}, b2{(float)x + ox + r * 0.38f, (float)y}, tp{(float)x + ox + (rv(50 + k) - 0.5f) * 5, (float)y - hh};
                        DrawTriangle(a, b2, tp, k == 1 ? cc : cd);
                        DrawLineEx(tp, {a.x + 2, a.y}, 1, cl);
                    }
                    VAdd();
                    VGlow({(float)x, (float)(y - h / 2)}, r + 6, Color{170, 110, 255, 255}, 0.25f + 0.1f * std::sin(time * 3 + o.seed % 7));
                    VNorm();
                } break;
                case 15: {   // Dragon's Den: a dark column with gold bands and a dragon's-eye gem
                    Color dc{70, 56, 90, 255}, dd{44, 34, 60, 255}, dl{110, 92, 136, 255}, gold{230, 186, 70, 255};
                    DrawRectangle(x - r, y - h, r * 2, h, dc);
                    DrawRectangle(x - r, y - h, 3, h, dl);
                    DrawRectangle(x + r - 3, y - h, 3, h, dd);
                    for (int k = 0; k < 3; k++) DrawRectangle(x - r, y - h + 4 + k * (h / 3), r * 2, 2, gold);
                    DrawRectangle(x - r - 2, y - 4, r * 2 + 4, 4, dd);
                    DrawEllipse(x, y - h, (float)r + 1, (r + 1) * 0.45f, dl);
                    DrawCircle(x, y - h / 2, 2.5f, Color{220, 60, 70, 255});
                    DrawRectangle(x - 1, y - h / 2 - 1, 1, 1, WHITE);
                } break;
                case 17: {   // Violet Gym: a wooden mooring post with rope lashings
                    Color wc{150, 108, 66, 255}, wd{104, 72, 44, 255}, wl{186, 144, 96, 255}, rope{214, 196, 150, 255};
                    DrawRectangle(x - r, y - h, r * 2, h, wc);
                    DrawRectangle(x - r, y - h, 2, h, wl);
                    DrawRectangle(x + r - 2, y - h, 2, h, wd);
                    for (int k = 0; k < 4; k++) DrawRectangle(x - r + 3, y - h + 6 + k * 5, 1, 3, wd);   // grain
                    for (int k = 0; k < 2; k++) {
                        int ry = y - h + 6 + k * (h / 2);
                        DrawRectangle(x - r - 1, ry, r * 2 + 2, 2, rope);
                        DrawRectangle(x - r - 1, ry + 2, r * 2 + 2, 1, Color{160, 140, 100, 255});
                    }
                    DrawEllipse(x, y - h, (float)r, r * 0.45f, wl);
                    EllipsePix((float)x, (float)(y - h), (float)r * 0.5f, r * 0.22f, wd);
                } break;
                case 18: {   // Karen's chamber: a jagged obsidian shard with a violet sheen
                    Color oc{44, 38, 56, 255}, od{24, 20, 32, 255}, ol{96, 80, 130, 255};
                    float lean = (rv(12) - 0.5f) * 6;
                    Vector2 a{(float)x - r, (float)y}, b2{(float)x + r, (float)y}, tp{(float)x + lean, (float)y - h - 4};
                    DrawTriangle(a, b2, tp, oc);
                    DrawTriangle({(float)x + 1, (float)y}, b2, tp, od);
                    DrawTriangle({(float)x - r + 2, (float)y}, {(float)x - 1, (float)y}, {tp.x - 1, tp.y + 6}, Fade(ol, 0.7f));
                    DrawLineEx({(float)x - r + 3, (float)y - 3}, {tp.x - 1, tp.y + 8}, 1, Fade(Color{190, 150, 255, 255}, 0.5f + 0.3f * std::sin(time * 2 + o.pos.x)));
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
            if (field.kind == ArenaKind::Steel) {   // Jasmine's: polished steel with brass bands and a ship's porthole
                DrawRectangle(x - r, y - h + 4, r * 2, 2, Color{210, 170, 80, 255});
                DrawRectangle(x - r, y - 6, r * 2, 2, Color{210, 170, 80, 255});
                DrawCircle(x, y - h / 2, 4, Color{210, 170, 80, 255});
                DrawCircle(x, y - h / 2, 3, Color{90, 140, 190, 255});
                DrawRectangle(x - 1, y - h / 2 - 2, 1, 1, WHITE);
            } else {
                DrawRectangle(x - r, y - h / 2 - 2, r * 2, 4, Color{170, 30, 30, 255});
                GDrawText("R", x - 3, y - h / 2 - 5, 10, WHITE);
            }
            for (int k = 0; k < 3; k++) {
                DrawRectangle(x - r + 3, y - h + 3 + k * 9, 1, 1, lt);
                DrawRectangle(x + r - 4, y - h + 3 + k * 9, 1, 1, lt);
            }
            DrawEllipse(x, y - h, (float)r, r * 0.45f, lt);
            EllipsePix((float)x, (float)(y - h), (float)r, r * 0.45f, dk);
        } break;
        case Obstacle::Boulder: {
            bool hot = field.Rules() == ArenaKind::Fire || field.venue == Venue::Volcano;
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
            if (field.kind == ArenaKind::Normal) {   // Whitney's hay bales: round or square
                Color hy{222, 186, 96, 255}, hd{176, 138, 60, 255}, hl{246, 218, 140, 255};
                DrawEllipse(x, y + 1, r + 3.0f, (r + 3) * 0.45f, Fade(BLACK, 0.35f));
                if (o.seed % 2) {   // a round bale lying on its side
                    DrawEllipse(x, y - h / 2, (float)r + 1, h * 0.55f, hd);
                    DrawEllipse(x, y - h / 2 - 1, (float)r, h * 0.5f, hy);
                    for (int k = 1; k <= 3; k++) EllipsePix((float)x + r * 0.35f, (float)(y - h / 2 - 1), r * 0.5f * k / 3, h * 0.42f * k / 3, hd);
                    DrawRectangle(x - r + 2, y - h + 2, r, 1, hl);
                } else {
                    DrawRectangle(x - r, y - h, r * 2, h, hy);
                    DrawRectangle(x - r, y - h, r * 2, 2, hl);
                    DrawRectangle(x + r - 3, y - h, 3, h, hd);
                    DrawRectangle(x - r + r / 2, y - h, 2, h, Color{150, 60, 50, 255});   // twine
                    DrawRectangle(x + r / 2 - 2, y - h, 2, h, Color{150, 60, 50, 255});
                }
                for (int k = 0; k < 5; k++) DrawRectangle(x - r + (int)(VRand(o.seed, k) * r * 2), y - h - 1 + (int)(VRand(o.seed, k + 9) * 3), 1, 2, hl);   // stray straws
                if (life < 0.6f) DrawRectangle(x - 2, y - h + 3, 4, 3, hd);
                break;
            }
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
                if (metal) GDrawText("R", x - 3, y - bh / 2 - 5, 10, Color{200, 40, 40, 255});
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
            VShadow({(float)x + 3, (float)y + 1}, (float)r + 6, (r + 6) * 0.42f, 0.9f);
            DrawLineEx({(float)x, (float)y}, mid, 5, Color{100, 70, 44, 255});
            DrawLineEx({(float)x + 1.2f, (float)y}, {mid.x + 1.2f, mid.y}, 1.6f, Color{74, 50, 32, 255});
            DrawLineEx({(float)x - 1.3f, (float)y}, {mid.x - 1.3f, mid.y}, 1, Color{140, 104, 66, 255});
            DrawTriangle({(float)x - 4, (float)y + 0.5f}, {(float)x, (float)y - 3}, {(float)x - 1, (float)y + 0.5f}, Color{100, 70, 44, 255});   // root flare
            DrawTriangle({(float)x + 1, (float)y + 0.5f}, {(float)x, (float)y - 3}, {(float)x + 4, (float)y + 0.5f}, Color{86, 60, 38, 255});
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
            } else {   // a leafy crown made of clumps: shadow side, body, sunlit clumps (tall bush / round / blossom)
                bool tall = style == 2;
                float sy = tall ? 1.25f : 0.95f;
                Vector2 c{top.x, top.y + (tall ? 5.0f : 6.0f)};
                const int N = 9;
                for (int pass = 0; pass < 3; pass++) {
                    Color col = pass == 0 ? leafD : pass == 1 ? leaf : leafL;
                    for (int k = 0; k < N; k++) {
                        float a = k * 6.2832f / N + VRand(o.seed, k) * 0.5f;
                        float rr = cr * (0.55f + VRand(o.seed, k + 20) * 0.25f);
                        Vector2 p{c.x + std::cos(a) * cr * 0.5f, c.y + std::sin(a) * cr * 0.5f * sy};
                        if (pass == 0) { p.x += 1.2f; p.y += 1.6f; DrawCircleV(p, rr * 1.08f, col); }
                        else if (pass == 1) DrawCircleV(p, rr * 0.92f, col);
                        else if (std::cos(a) + std::sin(a) < -0.2f) DrawCircleV({p.x - rr * 0.2f, p.y - rr * 0.25f}, rr * 0.45f, col);
                    }
                    if (pass == 1) DrawEllipse((int)c.x, (int)c.y, cr * 0.75f, cr * 0.75f * sy, col);
                }
                for (int k = 0; k < 7; k++) {   // leaf glints
                    Vector2 p{c.x + (VRand(o.seed, 40 + k) - 0.6f) * cr * 1.3f, c.y + (VRand(o.seed, 50 + k) - 0.7f) * cr * 1.2f * sy};
                    DrawRectangleV(p, {1.0f, 1.0f}, Fade(Color{235, 255, 200, 255}, 0.55f));
                }
                if (style == 3 && season != 0 && !burning)   // fruit
                    for (int k = 0; k < 4; k++) {
                        Vector2 p{top.x + (VRand(o.seed, k) - 0.5f) * cr * 1.3f, top.y + 4 + VRand(o.seed, k + 9) * 8};
                        DrawCircleV(p, 1.6f, Color{200, 40, 40, 255});
                        DrawCircleV({p.x - 0.5f, p.y - 0.5f}, 0.6f, Color{255, 180, 170, 255});
                    }
            }
            if (season == 3 && !burning) {   // snow on the branches
                DrawEllipse((int)top.x - 2, (int)(top.y + (style == 1 ? 2 : -1)), cr * 0.55f, 3, Color{240, 246, 255, 255});
                DrawEllipse((int)top.x + 4, (int)(top.y + 7), cr * 0.35f, 2, Color{240, 246, 255, 255});
            }
            if (burning) {
                uint32_t fr = (uint32_t)(time * 14);   // (drawing must not touch the battle's random numbers)
                for (int k = 0; k < 6; k++) {
                    int fx = (int)(top.x + (VRand(o.seed + fr, k) * 2 - 1) * cr);
                    int fy = (int)(top.y - 2 + VRand(o.seed + fr, k + 10) * 12);
                    DrawRectangle(fx, fy - 3, 2, 4, VRand(o.seed + fr, k + 20) < 0.5f ? Color{255, 150, 40, 255} : Color{255, 220, 90, 255});
                }
            }
        } break;
    }
}

void Battle::DrawFieldFloor() const {
    const float t = time;
    switch (field.Rules()) {
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
        case ArenaKind::Dragon:
            for (const Turret& tu : field.turrets) {
                if (tu.warnT >= 0) {   // where the dragonfire will go
                    Vector2 dir = SafeNorm(tu.aimAt - tu.pos, tu.dir);
                    float blink = ((int)(t * 10) % 2) ? 0.85f : 0.4f;
                    for (int k = -1; k <= 1; k++) {
                        Vector2 d = Rot(dir, k * 0.17f);
                        Vector2 a = ToScreen(tu.pos), b = ToScreen(tu.pos + d * 700.0f);
                        DrawLineEx(a, b, 3, Fade(Color{255, 120, 40, 255}, 0.15f));
                        DrawLineEx(a, b, 1, Fade(Color{255, 160, 60, 255}, blink));
                    }
                }
                DrawDragonStatue(tu);
            }
            break;
        case ArenaKind::Bug:
            for (const Zone& z : field.zones) {   // spider webs: spokes and a spiral, strung between pegs
                if (z.kind != Zone::Web) continue;
                Vector2 c = ToScreen(z.pos);
                const int spokes = 10;
                Color wc = Fade(Color{235, 235, 225, 255}, 0.55f);
                DrawEllipse((int)c.x, (int)c.y, z.rx, z.ry * DEPTH, Fade(Color{200, 200, 190, 255}, 0.07f));
                for (int k = 0; k < spokes; k++) {
                    float a = k * 2 * PI_F / spokes + (z.seed % 7) * 0.1f;
                    float w = Wobble(z.seed, a);
                    DrawLineEx(c, {c.x + std::cos(a) * z.rx * w, c.y + std::sin(a) * z.ry * w * DEPTH}, 1, wc);
                    DrawRectangle((int)(c.x + std::cos(a) * z.rx * w) - 1, (int)(c.y + std::sin(a) * z.ry * w * DEPTH) - 1, 2, 2, Color{120, 100, 70, 255});   // pegs
                }
                for (int ring = 1; ring <= 4; ring++) {
                    float u = ring / 4.5f;
                    for (int k = 0; k < spokes; k++) {
                        float a0 = k * 2 * PI_F / spokes + (z.seed % 7) * 0.1f, a1 = a0 + 2 * PI_F / spokes;
                        float w0 = Wobble(z.seed, a0) * u, w1 = Wobble(z.seed, a1) * u;
                        Vector2 p0{c.x + std::cos(a0) * z.rx * w0, c.y + std::sin(a0) * z.ry * w0 * DEPTH};
                        Vector2 p1{c.x + std::cos(a1) * z.rx * w1, c.y + std::sin(a1) * z.ry * w1 * DEPTH};
                        DrawLineEx(p0, p1 + Vector2{0, 0.8f}, 1, wc);
                    }
                }
                float gl = 0.5f + 0.5f * std::sin(t * 2 + z.pos.x);   // a glint of dew
                DrawRectangle((int)(c.x + std::cos(t * 0.3f + z.seed) * z.rx * 0.5f), (int)(c.y + std::sin(t * 0.3f + z.seed) * z.ry * 0.5f * DEPTH), 1, 1, Fade(WHITE, gl));
            }
            break;
        case ArenaKind::Champion: {
            if (field.kind == ArenaKind::JohtoChampion)   // Lance's statues and their warning lines
                for (const Turret& tu : field.turrets) {
                    if (tu.warnT >= 0) {
                        Vector2 dir = SafeNorm(tu.aimAt - tu.pos, tu.dir);
                        float blink = ((int)(t * 10) % 2) ? 0.85f : 0.4f;
                        for (int k = -1; k <= 1; k++) {
                            Vector2 d = Rot(dir, k * 0.17f);
                            Vector2 a = ToScreen(tu.pos), b = ToScreen(tu.pos + d * 700.0f);
                            DrawLineEx(a, b, 3, Fade(Color{255, 120, 40, 255}, 0.15f));
                            DrawLineEx(a, b, 1, Fade(Color{255, 160, 60, 255}, blink));
                        }
                    }
                    DrawDragonStatue(tu);
                }
            Vector2 c = ToScreen(field.spot);
            float R = Field::SPOT_R * (1.0f + 0.04f * std::sin(t * 5));
            VAdd();
            DrawEllipse((int)c.x, (int)c.y, R, R * DEPTH, Fade(kSpot, 0.20f));
            DrawEllipse((int)c.x, (int)c.y, R * 0.65f, R * 0.65f * DEPTH, Fade(kSpot, 0.14f));
            VNorm();
            for (int k = 0; k < 24; k++) {   // a turning dashed ring
                if (k % 2) continue;
                float a0 = t * 0.8f + k * PI_F / 12, a1 = a0 + PI_F / 12;
                DrawLineEx({c.x + std::cos(a0) * R, c.y + std::sin(a0) * R * DEPTH}, {c.x + std::cos(a1) * R, c.y + std::sin(a1) * R * DEPTH}, 1, Fade(Color{255, 210, 90, 255}, 0.8f));
            }
        } break;
        case ArenaKind::Fighting:
            DrawRingRopes(false);
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
    DrawHoennFloor();
    DrawGen45Floor();
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
    DrawCenterTop();
    if (field.kind == ArenaKind::Fighting) DrawRingRopes(true);
    if (field.kind == ArenaKind::Ghost || (field.kind == ArenaKind::Hearthome && field.xT[2] <= 0)) {   // lights out: only the candles, the creatures and the wisps glow
        float dark = 0.40f + 0.03f * std::sin(time * 7) + 0.02f * std::sin(time * 13.3f);
        DrawRectangle(0, FLOOR_Y - 24, VW, FLOOR_BOTTOM - FLOOR_Y + 32, Fade(Color{8, 4, 20, 255}, dark));
        VAdd();
        for (int k = 0; k < 7; k++) {   // candles along the back wall
            Vector2 p = ToScreen({40 + k * 90.0f, 2}, 14);
            float fl = 0.35f + 0.1f * std::sin(time * 9 + k * 2.1f);
            VGlow(p, 18, Color{255, 170, 90, 255}, fl);
        }
        for (int s = 0; s < N(); s++) {
            const Fighter& f = sides[s].Cur();
            if (f.state != FState::Active && f.state != FState::Entering) continue;
            VGlow(ToScreen(f.pos, 8), 24, Color{120, 110, 170, 255}, 0.22f);
        }
        for (const Wisp& w : field.wisps)
            if (w.life > 0) VGlow(ToScreen(w.pos, 12), 26, kWisp, 0.45f);
        VNorm();
        for (const Wisp& w : field.wisps) {   // the flames themselves
            if (w.life <= 0) continue;
            float bob = std::sin(time * 4 + (w.seed % 11)) * 2;
            Vector2 c = ToScreen(w.pos, 12 + bob);
            float fade = std::min(1.0f, w.life);
            for (int k = 0; k < 3; k++) {
                float fx = c.x + std::sin(time * 11 + k * 2 + (w.seed % 5)) * 1.5f;
                DrawTriangle({fx - 3.0f + k, c.y + 2}, {fx + 3.0f - k, c.y + 2}, {fx, c.y - 7.0f + k * 2 - std::fabs(std::sin(time * 8 + k)) * 3}, Fade(k == 2 ? WHITE : kWisp, fade * (k == 0 ? 0.6f : 0.9f)));
            }
            DrawCircleV({c.x, c.y + 1}, 2.5f, Fade(Color{200, 190, 255, 255}, fade));
        }
    }
    if (field.Rules() == ArenaKind::Dark) {   // lights out, but for three drifting pools of light
        DrawRectangle(0, FLOOR_Y - 24, VW, FLOOR_BOTTOM - FLOOR_Y + 32, Fade(Color{6, 4, 14, 255}, 0.48f));
        VAdd();
        for (size_t i = 0; i < field.lights.size(); i++) {
            Vector2 c = ToScreen(field.lights[i]);
            float R = Field::LIGHT_R;
            Vector2 src{c.x * 0.6f + VW * 0.2f, -10};
            DrawTriangle(src, {c.x - R, c.y}, {c.x + R, c.y}, Fade(Color{190, 170, 255, 255}, 0.05f));
            DrawTriangle(src, {c.x + R, c.y}, {c.x - R, c.y}, Fade(Color{190, 170, 255, 255}, 0.05f));
            for (int k = 0; k < 4; k++) {
                float rr = Field::LIGHT_R * (1.0f - k * 0.2f);
                DrawEllipse((int)c.x, (int)c.y, rr, rr * DEPTH / 1.2f, Fade(Color{200, 190, 255, 255}, 0.06f + 0.02f * k));
            }
        }
        for (int s = 0; s < N(); s++) {   // whoever stands in the light shows up
            const Fighter& f = sides[s].Cur();
            if (f.state != FState::Active && f.state != FState::Entering) continue;
            if (field.InLight(f.pos)) VGlow(ToScreen(f.pos, 8), 22, Color{200, 190, 255, 255}, 0.18f);
            else if (HasType(f.sp, Type::Dark)) VGlow(ToScreen(f.pos, 10), 6, Color{255, 60, 80, 255}, 0.5f);   // glowing eyes in the dark
        }
        VNorm();
        for (size_t i = 0; i < field.lights.size(); i++) {
            Vector2 c = ToScreen(field.lights[i]);
            float R = Field::LIGHT_R;
            for (int k = 0; k < 28; k++) {   // a dashed rim
                if (k % 2) continue;
                float a0 = time * 0.5f + k * PI_F / 14, a1 = a0 + PI_F / 14;
                DrawLineEx({c.x + std::cos(a0) * R, c.y + std::sin(a0) * R * DEPTH / 1.2f}, {c.x + std::cos(a1) * R, c.y + std::sin(a1) * R * DEPTH / 1.2f}, 1, Fade(Color{190, 170, 255, 255}, 0.5f));
            }
        }
    }
    if (field.kind == ArenaKind::Flying) {   // windsocks along the back wall show where the gust will blow
        for (int k = 0; k < 4; k++) {   // two down each side wall
            Vector2 p = ToScreen({k < 2 ? 6.0f : ARENA_W - 6, ARENA_H * (k % 2 ? 0.72f : 0.28f)}, 26);
            DrawRectangle((int)p.x, (int)p.y, 1, 26, Color{150, 150, 160, 255});
            float strength = field.windOn == 2 ? 1.0f : field.windOn == 1 ? 0.7f : 0.25f;
            float flap = std::sin(time * (6 + 8 * strength) + k) * (1.5f + strength);
            Vector2 d = SafeNorm(Vector2{field.windDir.x, field.windDir.y * DEPTH}, Vector2{1, 0});
            Vector2 tipDir = SafeNorm(d * strength + Vector2{0, 1} * (1 - strength), Vector2{0, 1});
            Vector2 a{p.x + 1, p.y + 1};
            for (int seg = 0; seg < 4; seg++) {
                Vector2 b = a + tipDir * 3.5f + Vector2{0, flap * 0.25f * seg};
                float w = 3.0f - seg * 0.5f;
                DrawLineEx(a, b, w, seg % 2 ? WHITE : Color{230, 90, 60, 255});
                a = b;
            }
        }
        if (field.windOn == 1 && ((int)(time * 6) % 2)) {   // a warning arrow
            Vector2 c = ToScreen({ARENA_W / 2, ARENA_H * 0.2f}, 0);
            Vector2 d = SafeNorm(Vector2{field.windDir.x, field.windDir.y * DEPTH}, Vector2{1, 0});
            Vector2 n{-d.y, d.x};
            Color ac = Fade(Color{200, 230, 255, 255}, 0.8f);
            DrawLineEx(c - d * 18.0f, c + d * 12.0f, 3, ac);
            DrawTriangle(c + d * 20.0f, c + d * 10.0f + n * 7.0f, c + d * 10.0f - n * 7.0f, ac);
            DrawTriangle(c + d * 20.0f, c + d * 10.0f - n * 7.0f, c + d * 10.0f + n * 7.0f, ac);
        }
        if (field.windOn == 2) {   // streaks across the deck
            for (int i = 0; i < 26; i++) {
                float ph = std::fmod(time * 1.4f + VRand(77, i), 1.0f);
                Vector2 w{VRand(78, i) * ARENA_W, VRand(79, i) * ARENA_H};
                w = w + field.windDir * ((ph - 0.5f) * 420.0f);
                if (w.x < 0 || w.x > ARENA_W || w.y < 0 || w.y > ARENA_H) continue;
                Vector2 a = ToScreen(w, 4 + VRand(80, i) * 20), b = ToScreen(w - field.windDir * 22.0f, 4 + VRand(80, i) * 20);
                DrawLineEx(a, b, 1, Fade(WHITE, 0.45f * std::sin(ph * PI_F)));
            }
        }
    }
    if (field.Rules() == ArenaKind::Champion) {
        Vector2 c = ToScreen(field.spot);
        float R = Field::SPOT_R;
        Vector2 src{VW / 2 + (c.x - VW / 2) * 0.4f, -10};
        VAdd();
        DrawTriangle(src, {c.x - R, c.y}, {c.x + R, c.y}, Fade(kSpot, 0.07f));
        DrawTriangle(src, {c.x + R, c.y}, {c.x - R, c.y}, Fade(kSpot, 0.07f));
        float ph = std::fmod(time, 14.0f);   // pyrotechnics at the four corners
        if (ph < 1.4f) {
            const Vector2 cs[4] = {{12, 12}, {ARENA_W - 12, 12}, {12, ARENA_H - 12}, {ARENA_W - 12, ARENA_H - 12}};
            float a = ph < 0.2f ? ph / 0.2f : 1.0f - (ph - 0.2f) / 1.2f;
            for (const Vector2& w : cs) {
                for (int k = 0; k < 6; k++) {
                    float z = 6 + k * 9 + std::fmod(time * 60 + k * 13, 9.0f);
                    Vector2 q = ToScreen(w + Vector2{std::sin(time * 17 + k) * 3, 0}, z);
                    VGlow(q, 9 - k * 0.8f, k < 2 ? Color{255, 240, 160, 255} : Color{255, 130, 40, 255}, 0.7f * a);
                }
            }
        }
        VNorm();
    }
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
    if (field.Rules() == ArenaKind::Electric) {  // arcs from coils to creatures in range
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
    DrawHoennTop();
    DrawGen45Top();
}

// Bruno's ring: corner posts and three ropes (red, white, blue). The back and sides go under
// everything; the front rope and posts go on top.
void Battle::DrawRingRopes(bool front) const {
    const float in = 3;
    Vector2 c[4] = {{in, in}, {ARENA_W - in, in}, {ARENA_W - in, ARENA_H - in}, {in, ARENA_H - in}};
    const Color rope[3] = {{220, 60, 60, 255}, {240, 240, 240, 255}, {70, 110, 220, 255}};
    auto seg = [&](Vector2 a, Vector2 b) {
        for (int k = 0; k < 3; k++) {
            float z = 7.0f + k * 6;
            Vector2 p = ToScreen(a, z), q = ToScreen(b, z);
            DrawLineEx({p.x, p.y + 1}, {q.x, q.y + 1}, 1, Fade(BLACK, 0.4f));
            DrawLineEx(p, q, 1.5f, rope[k]);
        }
    };
    auto post = [&](Vector2 a) {
        Vector2 b = ToScreen(a), tp = ToScreen(a, 24);
        DrawRectangle((int)b.x - 2, (int)tp.y, 4, (int)(b.y - tp.y), Color{200, 200, 210, 255});
        DrawRectangle((int)b.x + 1, (int)tp.y, 1, (int)(b.y - tp.y), Color{130, 130, 140, 255});
        for (int k = 0; k < 3; k++) { Vector2 pp = ToScreen(a, 7.0f + k * 6); DrawRectangle((int)pp.x - 3, (int)pp.y - 2, 6, 4, rope[k]); }
    };
    if (!front) {
        seg(c[0], c[1]);
        seg(c[0], c[3]);
        seg(c[1], c[2]);
        post(c[0]);
        post(c[1]);
    } else {
        seg(c[3], c[2]);
        post(c[3]);
        post(c[2]);
    }
}

// Lance's statues: stone dragon heads on the walls; the eyes glow before they breathe.
void Battle::DrawDragonStatue(const Turret& tu) const {
    bool back = tu.dir.y > 0;
    Vector2 c = ToScreen(tu.pos, back ? 16.0f : 10.0f);
    bool warn = tu.warnT >= 0;
    Color st{96, 84, 120, 255}, sd{62, 52, 82, 255}, sl{140, 126, 166, 255};
    Color eye = warn ? (((int)(time * 12) % 2) ? Color{255, 80, 40, 255} : Color{255, 200, 80, 255}) : Color{120, 40, 50, 255};
    if (back) {
        DrawRectangle((int)c.x - 9, (int)c.y - 10, 18, 12, st);   // the head
        DrawRectangle((int)c.x - 9, (int)c.y - 10, 18, 2, sl);
        DrawTriangle({c.x - 6, c.y + 2}, {c.x + 6, c.y + 2}, {c.x, c.y + 9}, sd);   // snout, pointing down
        DrawTriangle({c.x - 9, c.y - 10}, {c.x - 12, c.y - 17}, {c.x - 5, c.y - 10}, sl);   // horns
        DrawTriangle({c.x + 5, c.y - 10}, {c.x + 12, c.y - 17}, {c.x + 9, c.y - 10}, sl);
        DrawRectangle((int)c.x - 6, (int)c.y - 6, 3, 2, eye);
        DrawRectangle((int)c.x + 3, (int)c.y - 6, 3, 2, eye);
    } else {
        int s = tu.dir.x > 0 ? 1 : -1;
        DrawRectangle((int)c.x - (s > 0 ? 12 : 0), (int)c.y - 8, 12, 12, st);
        DrawTriangle({c.x, c.y - 5}, {c.x, c.y + 4}, {c.x + s * 9.0f, c.y}, sd);
        if (s < 0) DrawTriangle({c.x, c.y - 5}, {c.x + s * 9.0f, c.y}, {c.x, c.y + 4}, sd);
        DrawTriangle({c.x - s * 8.0f, c.y - 8}, {c.x - s * 12.0f, c.y - 15}, {c.x - s * 3.0f, c.y - 8}, sl);
        DrawRectangle((int)c.x - s * 6 - 1, (int)c.y - 5, 3, 2, eye);
    }
    if (warn) { VAdd(); VGlow(c, 14, Color{255, 120, 40, 255}, 0.5f); VNorm(); }
}
