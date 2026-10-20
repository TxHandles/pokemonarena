#include "moves.h"
#include <cctype>
#include <cstdio>
#include "raylib.h"

std::string Squash(const std::string& s) {
    std::string o;
    for (char c : s)
        if (std::isalnum((unsigned char)c)) o += (char)std::tolower((unsigned char)c);
    return o;
}

namespace {

// Defaults for each kind, then chainable tweaks.
struct B {
    MoveDef m;
    B(const char* n, Type t, MoveKind k, int p, float cd) {
        m.name = n; m.type = t; m.kind = k; m.power = p; m.cooldown = cd;
        switch (k) {
            case MoveKind::Projectile: m.speed = 230; m.range = 220; m.radius = 4; m.windup = 0.05f; break;
            case MoveKind::Spread:     m.speed = 210; m.range = 170; m.radius = 3.5f; m.windup = 0.08f; break;
            case MoveKind::Melee:      m.range = 24; m.radius = 14; m.windup = 0.07f; break;
            case MoveKind::Dash:       m.range = 95; m.windup = 0.12f; break;
            case MoveKind::Burst:      m.radius = 36; m.windup = 0.3f; break;
            case MoveKind::Beam:       m.range = 240; m.radius = 5; m.windup = 0.4f; break;
            case MoveKind::Lob:        m.range = 150; m.radius = 26; m.windup = 0.1f; break;
            case MoveKind::Zone:       m.duration = 3.5f; m.radius = 24; m.range = 55; m.windup = 0.15f; break;
            case MoveKind::Buff:       m.stat = StatId::Atk; m.amount = 1.5f; m.duration = 6; m.windup = 0.1f; break;
            case MoveKind::Heal:       m.amount = 0.25f; m.windup = 0.2f; m.maxUses = 2; break;
            case MoveKind::Chain:      m.range = 170; m.windup = 0.1f; break;
            case MoveKind::Wave:       m.radius = 90; m.speed = 240; m.windup = 0.25f; break;
            case MoveKind::Rain:       m.range = 150; m.radius = 34; m.count = 5; m.blast = 14; m.duration = 0.9f; m.windup = 0.3f; break;
            case MoveKind::Blink:      m.range = 140; m.radius = 20; m.windup = 0.1f; break;
            case MoveKind::Shield:     m.duration = 1.0f; m.windup = 0.0f; break;
            case MoveKind::Burrow:     m.duration = 0.9f; m.range = 150; m.radius = 30; m.windup = 0.15f; break;
            case MoveKind::Boomerang:  m.speed = 210; m.range = 150; m.radius = 5; m.windup = 0.08f; break;
            case MoveKind::Orbit:      m.count = 3; m.radius = 8; m.range = 26; m.duration = 4; m.speed = 5; m.windup = 0.1f; break;
            case MoveKind::Wall:       m.count = 3; m.radius = 8; m.range = 34; m.duration = 4; m.windup = 0.05f; break;
            case MoveKind::Trap:       m.radius = 14; m.range = 90; m.duration = 25; m.windup = 0.15f; break;
            case MoveKind::Vortex:     m.radius = 34; m.range = 70; m.duration = 3.5f; m.amount = 70; m.windup = 0.2f; break;
            case MoveKind::Cone:       m.range = 75; m.radius = 35; m.windup = 0.15f; break;
            case MoveKind::Leap:       m.range = 130; m.radius = 34; m.duration = 0.7f; m.windup = 0.1f; break;
            case MoveKind::Weather:    m.duration = 10; m.windup = 0.2f; break;
            case MoveKind::Sweep:      m.range = 230; m.radius = 6; m.windup = 0.55f; m.arc = 1.4f; m.duration = 0.6f; break;
            case MoveKind::Tether:     m.range = 140; m.duration = 3.0f; m.windup = 0.12f; m.tether = 1; break;
            case MoveKind::Grapple:    m.speed = 320; m.range = 150; m.radius = 5; m.windup = 0.1f; m.amount = 1; break;
            case MoveKind::Throw:      m.range = 26; m.radius = 95; m.duration = 0.55f; m.windup = 0.1f; break;
            case MoveKind::Roll:       m.speed = 250; m.range = 200; m.radius = 10; m.duration = 2.0f; m.bounces = 4; m.amount = 0.15f; m.windup = 0.25f; break;
            default: break;
        }
    }
    B& st(Status s, float c) { m.status = s; m.statusChance = c; return *this; }
    B& low(StatId s, float c) { m.lowers = true; m.lowerStat = s; m.lowerChance = c; return *this; }
    B& self(StatId s) { m.selfDrop = true; m.selfStat = s; return *this; }
    B& drain(float d) { m.drain = d; return *this; }
    B& recoil(float r) { m.recoil = r; return *this; }
    B& knock(float k) { m.knock = k; return *this; }
    B& homing(float h) { m.homing = h; return *this; }
    B& hits(int n) { m.hits = n; return *this; }
    B& crit() { m.highCrit = true; return *this; }
    B& hex() { m.hex = true; return *this; }
    B& quick() { m.quick = true; return *this; }
    B& recharge(float r) { m.recharge = r; return *this; }
    B& rain(int n, float blastR) { m.count = n; m.blast = blastR; return *this; }
    B& patch(int p, float t, float r) { m.patchPower = p; m.patchTime = t; m.patchRadius = r; return *this; }
    B& range(float r) { m.range = r; return *this; }
    B& radius(float r) { m.radius = r; return *this; }
    B& speed(float s) { m.speed = s; return *this; }
    B& windup(float w) { m.windup = w; return *this; }
    B& dur(float d) { m.duration = d; return *this; }
    B& buff(StatId s, float mult = 1.5f) { m.stat = s; m.amount = mult; return *this; }
    B& heal(float frac) { m.amount = frac; return *this; }
    B& uses(int u) { m.maxUses = u; return *this; }
    B& bounce(int n) { m.bounces = n; return *this; }
    B& count(int n) { m.count = n; return *this; }
    B& phys() { m.cat = MoveCat::Physical; return *this; }
    B& spec() { m.cat = MoveCat::Special; return *this; }
    B& pull(float p) { m.amount = p; return *this; }
    B& weather(Weather w) { m.weather = w; return *this; }
    B& buff2(StatId s) { m.stat2 = (int)s; return *this; }
    B& fixed(int d) { m.fixedDmg = d; return *this; }
    B& pmod(PowerMod p) { m.powerMod = p; return *this; }
    B& force() { m.forceSwap = true; return *this; }
    B& ohko() { m.ohko = true; return *this; }
    B& selfko() { m.selfKO = true; return *this; }
    B& counter() { m.counter = true; return *this; }
    B& endure() { m.endure = true; return *this; }
    B& fswipe() { m.falseSwipe = true; return *this; }
    B& haze() { m.haze = true; return *this; }
    B& cure() { m.cure = true; return *this; }
    B& metro() { m.metronome = true; return *this; }
    B& copy() { m.copycat = true; return *this; }
    B& payday() { m.payDay = true; return *this; }
    B& xform() { m.transform = true; return *this; }
    B& abfx(int k) { m.abFx = (uint8_t)k; return *this; }
    B& arc(float a) { m.arc = a; return *this; }
    B& tether(int k) { m.tether = (uint8_t)k; return *this; }
    B& sleepOnly() { m.sleepOnly = true; return *this; }
    B& antiAir() { m.antiAir = true; return *this; }
    B& pinFor(float t) { m.pin = t; return *this; }
    B& boost(float b) { m.amount = b; return *this; }
    B& clears() { m.clears = true; return *this; }
    B& fireTrail() { m.trail = true; return *this; }
    B& toss(float dist) { m.radius = dist; return *this; }
    B& skyDrop() { m.throwKind = 1; m.duration = 1.0f; return *this; }
    B& pullFrac(float f) { m.amount = f; return *this; }
};

using T = Type;
using K = MoveKind;
using S = Status;
using St = StatId;

std::vector<MoveDef> Build() {
    std::vector<MoveDef> v;
    auto add = [&](B b) { v.push_back(b.m); };

    // ---------------- Normal
    add(B("Tackle", T::Normal, K::Dash, 40, 1.3f).range(70));
    add(B("Quick Attack", T::Normal, K::Dash, 40, 1.0f).quick().range(115));
    add(B("Scratch", T::Normal, K::Melee, 40, 0.45f));
    add(B("Swift", T::Normal, K::Spread, 25, 1.4f).homing(3));
    add(B("Growl", T::Normal, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Atk, 1));
    add(B("Leer", T::Normal, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Def, 1));
    add(B("Tail Whip", T::Normal, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Def, 1));
    add(B("Smokescreen", T::Normal, K::Zone, 0, 7).radius(30).dur(3.5f).low(St::Spd, 0.6f));
    add(B("Double Team", T::Normal, K::Blink, 0, 6).range(90));
    add(B("Rapid Spin", T::Normal, K::Roll, 50, 2.8f).speed(300).dur(0.9f).bounce(2).boost(0.1f).clears());
    add(B("Skull Bash", T::Normal, K::Dash, 130, 6).windup(0.8f).range(130));
    add(B("Nuzzle", T::Electric, K::Dash, 20, 2.5f).st(S::Paralyze, 1.0f).range(80));
    add(B("Flame Burst", T::Fire, K::Lob, 70, 2.2f).patch(10, 2.5f, 16));
    add(B("Headbutt", T::Normal, K::Melee, 70, 1.2f).st(S::Flinch, 0.3f));
    add(B("Body Slam", T::Normal, K::Dash, 85, 3.5f).st(S::Paralyze, 0.3f));
    add(B("Slash", T::Normal, K::Melee, 70, 1.0f).crit());
    add(B("Fury Swipes", T::Normal, K::Melee, 18, 1.4f).hits(4));
    add(B("Hyper Voice", T::Normal, K::Wave, 80, 4));
    add(B("Double-Edge", T::Normal, K::Dash, 120, 5).recoil(0.33f).range(110));
    add(B("Hyper Beam", T::Normal, K::Sweep, 150, 9).windup(0.7f).recharge(1.5f).radius(8).arc(1.6f).dur(0.75f));
    add(B("Harden", T::Normal, K::Buff, 0, 9).buff(St::Def));
    add(B("Growth", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.4f));
    add(B("Swords Dance", T::Normal, K::Buff, 0, 11).buff(St::Atk, 1.7f));
    add(B("Recover", T::Normal, K::Heal, 0, 14).heal(0.5f));
    add(B("Protect", T::Normal, K::Shield, 0, 6));

    // ---------------- Fire
    add(B("Ember", T::Fire, K::Projectile, 40, 0.7f).patch(8, 2.5f, 13).st(S::Burn, 0.1f));
    add(B("Fire Fang", T::Fire, K::Melee, 65, 0.9f).st(S::Burn, 0.1f));
    add(B("Fire Punch", T::Fire, K::Melee, 75, 1.1f).st(S::Burn, 0.1f));
    add(B("Flame Wheel", T::Fire, K::Roll, 60, 3.5f).speed(260).dur(1.6f).bounce(3).st(S::Burn, 0.1f).fireTrail());
    add(B("Fire Spin", T::Fire, K::Zone, 14, 5).dur(4).st(S::Burn, 0.1f));
    add(B("Will-O-Wisp", T::Fire, K::Projectile, 0, 6).homing(3).speed(150).st(S::Burn, 1));
    add(B("Flamethrower", T::Fire, K::Sweep, 90, 3.2f).windup(0.35f).range(200).arc(1.0f).dur(0.5f).st(S::Burn, 0.1f));
    add(B("Heat Wave", T::Fire, K::Wave, 90, 4.5f).st(S::Burn, 0.1f));
    add(B("Fire Blast", T::Fire, K::Lob, 110, 6).radius(30).patch(12, 3, 22).st(S::Burn, 0.3f));
    add(B("Flare Blitz", T::Fire, K::Dash, 120, 5).recoil(0.33f).st(S::Burn, 0.1f));

    // ---------------- Water
    add(B("Water Gun", T::Water, K::Projectile, 40, 0.6f).knock(2.2f));
    add(B("Bubble", T::Water, K::Spread, 25, 1.1f).speed(170).low(St::Spd, 0.1f));
    add(B("Bubble Beam", T::Water, K::Beam, 65, 2.5f).windup(0.3f).low(St::Spd, 0.1f));
    add(B("Aqua Jet", T::Water, K::Dash, 40, 1.0f).quick().range(115));
    add(B("Water Pulse", T::Water, K::Projectile, 60, 1.2f).radius(6).st(S::Confuse, 0.2f));
    add(B("Whirlpool", T::Water, K::Zone, 15, 5));
    add(B("Aqua Tail", T::Water, K::Melee, 90, 1.6f).radius(16));
    add(B("Surf", T::Water, K::Wave, 90, 4.5f).radius(100));
    add(B("Dive", T::Water, K::Burrow, 80, 4));
    add(B("Hydro Pump", T::Water, K::Sweep, 110, 5).windup(0.5f).radius(7).knock(2.5f).arc(1.2f).dur(0.55f));
    add(B("Withdraw", T::Water, K::Buff, 0, 9).buff(St::Def));
    add(B("Aqua Ring", T::Water, K::Heal, 0, 12).heal(0.3f));

    // ---------------- Grass
    add(B("Vine Whip", T::Grass, K::Grapple, 45, 1.2f).range(95).speed(360).pullFrac(0.45f));
    add(B("Absorb", T::Grass, K::Tether, 30, 1.6f).range(110).dur(2.0f).drain(0.5f));
    add(B("Mega Drain", T::Grass, K::Tether, 50, 3.0f).range(130).dur(2.6f).drain(0.5f));
    add(B("Giga Drain", T::Grass, K::Tether, 75, 5.0f).windup(0.3f).range(150).dur(3.2f).drain(0.5f));
    add(B("Razor Leaf", T::Grass, K::Spread, 35, 1.2f).crit());
    add(B("Bullet Seed", T::Grass, K::Projectile, 18, 1.3f).hits(4).speed(260));
    add(B("Leech Seed", T::Grass, K::Projectile, 0, 7).speed(180).st(S::Seed, 1));
    add(B("Sleep Powder", T::Grass, K::Burst, 0, 8).radius(40).windup(0.35f).st(S::Sleep, 1));
    add(B("Stun Spore", T::Grass, K::Burst, 0, 7).radius(40).windup(0.3f).st(S::Paralyze, 1));
    add(B("Seed Bomb", T::Grass, K::Lob, 80, 2.6f));
    add(B("Leaf Blade", T::Grass, K::Melee, 90, 1.4f).crit());
    add(B("Petal Blizzard", T::Grass, K::Wave, 90, 4.5f));
    add(B("Solar Beam", T::Grass, K::Sweep, 120, 6).windup(1.0f).radius(9).arc(1.3f).dur(0.65f));
    add(B("Synthesis", T::Grass, K::Heal, 0, 12).heal(0.35f));

    // ---------------- Electric
    add(B("Thunder Shock", T::Electric, K::Chain, 40, 0.9f).range(160).st(S::Paralyze, 0.1f));
    add(B("Thunder Wave", T::Electric, K::Projectile, 0, 7).speed(300).st(S::Paralyze, 1));
    add(B("Spark", T::Electric, K::Dash, 65, 2).st(S::Paralyze, 0.3f));
    add(B("Shock Wave", T::Electric, K::Projectile, 60, 1.0f).homing(6).speed(300));
    add(B("Thunder Punch", T::Electric, K::Melee, 75, 1.1f).st(S::Paralyze, 0.1f));
    add(B("Electro Ball", T::Electric, K::Projectile, 70, 1.6f).speed(200).radius(6));
    add(B("Discharge", T::Electric, K::Burst, 80, 4).radius(48).st(S::Paralyze, 0.3f));
    add(B("Thunderbolt", T::Electric, K::Chain, 90, 3.2f).range(220).st(S::Paralyze, 0.1f));
    add(B("Wild Charge", T::Electric, K::Dash, 90, 3).recoil(0.25f));
    add(B("Thunder", T::Electric, K::Rain, 110, 6).rain(1, 28).range(200).windup(0.35f).dur(0.6f).st(S::Paralyze, 0.3f));
    add(B("Charge", T::Electric, K::Buff, 0, 10).buff(St::Atk, 1.4f));

    // ---------------- Ice
    add(B("Powder Snow", T::Ice, K::Spread, 30, 1.0f).st(S::Freeze, 0.1f));
    add(B("Ice Shard", T::Ice, K::Projectile, 40, 0.6f).speed(340));
    add(B("Icy Wind", T::Ice, K::Spread, 45, 2).low(St::Spd, 1));
    add(B("Ice Fang", T::Ice, K::Melee, 65, 0.9f).st(S::Freeze, 0.1f));
    add(B("Ice Punch", T::Ice, K::Melee, 75, 1.1f).st(S::Freeze, 0.1f));
    add(B("Aurora Beam", T::Ice, K::Beam, 65, 2.4f).windup(0.3f).low(St::Atk, 0.1f));
    add(B("Avalanche", T::Ice, K::Lob, 70, 2.6f).radius(28));
    add(B("Freeze-Dry", T::Ice, K::Projectile, 70, 1.4f).radius(5).st(S::Freeze, 0.1f));
    add(B("Ice Beam", T::Ice, K::Beam, 90, 3.2f).st(S::Freeze, 0.1f));
    add(B("Blizzard", T::Ice, K::Rain, 30, 6).rain(6, 14).radius(40).st(S::Freeze, 0.1f));

    // ---------------- Fighting
    add(B("Karate Chop", T::Fighting, K::Melee, 50, 0.5f).crit());
    add(B("Double Kick", T::Fighting, K::Melee, 30, 1.0f).hits(2));
    add(B("Low Kick", T::Fighting, K::Melee, 60, 0.9f).low(St::Spd, 0.2f));
    add(B("Mach Punch", T::Fighting, K::Dash, 40, 1.0f).quick().range(115));
    add(B("Vacuum Wave", T::Fighting, K::Projectile, 40, 0.7f).speed(320));
    add(B("Brick Break", T::Fighting, K::Melee, 75, 1.2f));
    add(B("Aura Sphere", T::Fighting, K::Projectile, 80, 2.2f).homing(5).radius(6));
    add(B("Cross Chop", T::Fighting, K::Melee, 100, 2.4f).crit());
    add(B("Close Combat", T::Fighting, K::Dash, 120, 5).self(St::Def));
    add(B("Focus Blast", T::Fighting, K::Projectile, 120, 6).windup(0.5f).speed(180).radius(7));
    add(B("Bulk Up", T::Fighting, K::Buff, 0, 10).buff(St::Atk).buff2(St::Def));
    add(B("Detect", T::Fighting, K::Shield, 0, 6));

    // ---------------- Poison
    add(B("Poison Sting", T::Poison, K::Projectile, 35, 0.6f).speed(280).st(S::Poison, 0.3f));
    add(B("Acid", T::Poison, K::Spread, 40, 1.3f).low(St::Def, 0.1f));
    add(B("Smog", T::Poison, K::Zone, 15, 5).st(S::Poison, 0.4f));
    add(B("Sludge", T::Poison, K::Projectile, 65, 1.3f).radius(5).patch(6, 2.5f, 14).st(S::Poison, 0.3f));
    add(B("Poison Fang", T::Poison, K::Melee, 50, 0.8f).st(S::Toxic, 0.5f));
    add(B("Poison Jab", T::Poison, K::Melee, 80, 1.1f).st(S::Poison, 0.3f));
    add(B("Venoshock", T::Poison, K::Projectile, 65, 1.2f).hex());
    add(B("Toxic", T::Poison, K::Projectile, 0, 8).speed(180).st(S::Toxic, 1));
    add(B("Poison Powder", T::Poison, K::Burst, 0, 7).radius(40).windup(0.3f).st(S::Poison, 1));
    add(B("Sludge Bomb", T::Poison, K::Lob, 90, 3.2f).st(S::Poison, 0.3f));
    add(B("Gunk Shot", T::Poison, K::Projectile, 120, 6).windup(0.4f).radius(7).st(S::Poison, 0.3f));
    add(B("Acid Armor", T::Poison, K::Buff, 0, 9).buff(St::Def, 1.6f));

    // ---------------- Ground
    add(B("Mud-Slap", T::Ground, K::Projectile, 30, 0.7f).low(St::Spd, 0.5f));
    add(B("Mud Shot", T::Ground, K::Projectile, 55, 1.2f).radius(5).low(St::Spd, 1));
    add(B("Sand Tomb", T::Ground, K::Zone, 15, 5));
    add(B("Bone Club", T::Ground, K::Melee, 65, 1.0f).st(S::Flinch, 0.1f));
    add(B("Bonemerang", T::Ground, K::Boomerang, 55, 1.8f).range(140).phys());
    add(B("Bulldoze", T::Ground, K::Wave, 60, 2.5f).radius(80).low(St::Spd, 1));
    add(B("Magnitude", T::Ground, K::Wave, 70, 3.5f));
    add(B("Dig", T::Ground, K::Burrow, 80, 4));
    add(B("Earth Power", T::Ground, K::Rain, 90, 3.5f).rain(1, 24).range(200).dur(0.5f).low(St::Def, 0.1f));
    add(B("Earthquake", T::Ground, K::Wave, 100, 5).radius(110));

    // ---------------- Flying
    add(B("Peck", T::Flying, K::Melee, 35, 0.4f));
    add(B("Gust", T::Flying, K::Projectile, 40, 0.7f).knock(2.2f));
    add(B("Wing Attack", T::Flying, K::Melee, 60, 0.9f).radius(18));
    add(B("Aerial Ace", T::Flying, K::Projectile, 60, 1.2f).homing(8).speed(320));
    add(B("Air Cutter", T::Flying, K::Spread, 55, 1.6f).crit());
    add(B("Air Slash", T::Flying, K::Projectile, 75, 1.6f).radius(5).st(S::Flinch, 0.3f));
    add(B("Fly", T::Flying, K::Burrow, 90, 4.5f));
    add(B("Brave Bird", T::Flying, K::Dash, 120, 5).recoil(0.33f).range(110));
    add(B("Hurricane", T::Flying, K::Beam, 110, 5.5f).windup(0.5f).radius(8).st(S::Confuse, 0.3f));
    add(B("Tailwind", T::Flying, K::Buff, 0, 10).buff(St::Spd));
    add(B("Roost", T::Flying, K::Heal, 0, 14).heal(0.5f));

    // ---------------- Psychic
    add(B("Confusion", T::Psychic, K::Projectile, 50, 0.8f).homing(2).st(S::Confuse, 0.1f));
    add(B("Psybeam", T::Psychic, K::Beam, 65, 2.2f).windup(0.3f).st(S::Confuse, 0.1f));
    add(B("Psycho Cut", T::Psychic, K::Melee, 70, 0.9f).crit());
    add(B("Zen Headbutt", T::Psychic, K::Melee, 80, 1.3f).st(S::Flinch, 0.2f));
    add(B("Psychic", T::Psychic, K::Chain, 90, 3.2f).range(200).low(St::Def, 0.1f));
    add(B("Future Sight", T::Psychic, K::Rain, 120, 8).rain(1, 26).range(220).dur(1.8f));
    add(B("Hypnosis", T::Psychic, K::Projectile, 0, 8).speed(150).homing(2).st(S::Sleep, 1));
    add(B("Teleport", T::Psychic, K::Blink, 0, 4));
    add(B("Calm Mind", T::Psychic, K::Buff, 0, 10).buff(St::Atk, 1.4f).buff2(St::Def));
    add(B("Agility", T::Psychic, K::Buff, 0, 10).buff(St::Spd, 1.6f));
    add(B("Barrier", T::Psychic, K::Buff, 0, 9).buff(St::Def, 1.6f));
    add(B("Rest", T::Psychic, K::Heal, 0, 20).heal(1.0f).uses(1).st(S::Sleep, 1));

    // ---------------- Bug
    add(B("Bug Bite", T::Bug, K::Melee, 60, 0.8f));
    add(B("String Shot", T::Bug, K::Spread, 0, 4).low(St::Spd, 1));
    add(B("Twineedle", T::Bug, K::Projectile, 25, 0.9f).hits(2).st(S::Poison, 0.2f));
    add(B("Fury Cutter", T::Bug, K::Melee, 40, 0.6f));
    add(B("Pin Missile", T::Bug, K::Projectile, 25, 1.3f).hits(4).speed(280));
    add(B("Leech Life", T::Bug, K::Melee, 80, 1.4f).drain(0.5f));
    add(B("Silver Wind", T::Bug, K::Spread, 60, 2));
    add(B("Signal Beam", T::Bug, K::Beam, 75, 2.6f).windup(0.3f).st(S::Confuse, 0.1f));
    add(B("X-Scissor", T::Bug, K::Melee, 80, 1.1f).crit());
    add(B("Bug Buzz", T::Bug, K::Wave, 90, 4.5f).low(St::Def, 0.1f));
    add(B("Megahorn", T::Bug, K::Dash, 120, 4.5f));
    add(B("Quiver Dance", T::Bug, K::Buff, 0, 10).buff(St::Spd).buff2(St::Atk));

    // ---------------- Rock
    add(B("Rock Throw", T::Rock, K::Lob, 50, 1.4f).radius(18));
    add(B("Rock Tomb", T::Rock, K::Projectile, 60, 1.6f).radius(5).low(St::Spd, 1));
    add(B("Rock Blast", T::Rock, K::Projectile, 25, 1.4f).hits(4).radius(5));
    add(B("Ancient Power", T::Rock, K::Projectile, 60, 1.4f).radius(6));
    add(B("Rock Slide", T::Rock, K::Rain, 40, 4).rain(5, 14).radius(36).st(S::Flinch, 0.3f));
    add(B("Power Gem", T::Rock, K::Beam, 80, 2.6f).windup(0.3f));
    add(B("Stealth Rock", T::Rock, K::Trap, 60, 6).radius(16));
    add(B("Stone Edge", T::Rock, K::Projectile, 100, 3.5f).crit().speed(260).radius(6));
    add(B("Head Smash", T::Rock, K::Dash, 150, 6).recoil(0.5f));
    add(B("Rock Polish", T::Rock, K::Buff, 0, 10).buff(St::Spd));

    // ---------------- Ghost
    add(B("Lick", T::Ghost, K::Grapple, 30, 1.4f).range(75).speed(280).st(S::Paralyze, 0.3f));
    add(B("Astonish", T::Ghost, K::Melee, 30, 0.5f).st(S::Flinch, 0.3f));
    add(B("Night Shade", T::Ghost, K::Chain, 0, 2.0f).range(180).fixed(-1));
    add(B("Shadow Sneak", T::Ghost, K::Blink, 40, 1.4f));
    add(B("Confuse Ray", T::Ghost, K::Projectile, 0, 6).homing(3).speed(160).st(S::Confuse, 1));
    add(B("Hex", T::Ghost, K::Projectile, 65, 1.4f).hex());
    add(B("Shadow Claw", T::Ghost, K::Melee, 70, 0.9f).crit());
    add(B("Ominous Wind", T::Ghost, K::Spread, 60, 2.2f));
    add(B("Shadow Ball", T::Ghost, K::Projectile, 80, 1.8f).radius(6).low(St::Def, 0.2f));
    add(B("Phantom Force", T::Ghost, K::Burrow, 90, 4.5f));

    // ---------------- Dragon
    add(B("Dragon Rage", T::Dragon, K::Projectile, 0, 1.6f).fixed(16));
    add(B("Twister", T::Dragon, K::Spread, 40, 1.3f).st(S::Flinch, 0.2f));
    add(B("Dragon Breath", T::Dragon, K::Spread, 60, 2).range(120).st(S::Paralyze, 0.3f));
    add(B("Dragon Claw", T::Dragon, K::Melee, 80, 1.0f));
    add(B("Dragon Tail", T::Dragon, K::Melee, 60, 2.0f).knock(3).force());
    add(B("Dragon Pulse", T::Dragon, K::Beam, 85, 2.8f));
    add(B("Dragon Rush", T::Dragon, K::Dash, 100, 3.8f).st(S::Flinch, 0.2f));
    add(B("Outrage", T::Dragon, K::Dash, 120, 5).range(120));
    add(B("Draco Meteor", T::Dragon, K::Rain, 45, 8).rain(6, 16).radius(44).self(St::Atk));
    add(B("Dragon Dance", T::Dragon, K::Buff, 0, 10).buff(St::Atk).buff2(St::Spd));

    // ---------------- Dark
    add(B("Bite", T::Dark, K::Melee, 60, 0.8f).st(S::Flinch, 0.3f));
    add(B("Pursuit", T::Dark, K::Dash, 40, 1.0f).quick().range(115));
    add(B("Feint Attack", T::Dark, K::Projectile, 60, 1.1f).homing(8).speed(300));
    add(B("Thief", T::Dark, K::Melee, 60, 1.0f));
    add(B("Knock Off", T::Dark, K::Melee, 65, 1.0f).knock(2));
    add(B("Snarl", T::Dark, K::Spread, 55, 2).low(St::Atk, 1));
    add(B("Sucker Punch", T::Dark, K::Blink, 70, 2));
    add(B("Night Slash", T::Dark, K::Melee, 70, 0.9f).crit());
    add(B("Crunch", T::Dark, K::Melee, 80, 1.3f).low(St::Def, 0.2f));
    add(B("Dark Pulse", T::Dark, K::Wave, 80, 4).st(S::Flinch, 0.2f));
    add(B("Nasty Plot", T::Dark, K::Buff, 0, 10).buff(St::Atk, 1.6f));

    // ---------------- Steel
    add(B("Metal Claw", T::Steel, K::Melee, 50, 0.55f));
    add(B("Bullet Punch", T::Steel, K::Dash, 40, 1.0f).quick().range(115));
    add(B("Metal Sound", T::Steel, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Def, 1));
    add(B("Magnet Bomb", T::Steel, K::Projectile, 60, 1.2f).homing(6));
    add(B("Mirror Shot", T::Steel, K::Projectile, 65, 1.2f).radius(5));
    add(B("Steel Wing", T::Steel, K::Melee, 70, 1.0f).radius(18));
    add(B("Gyro Ball", T::Steel, K::Roll, 80, 4.5f).speed(220).dur(2.0f).bounce(4).radius(12));
    add(B("Flash Cannon", T::Steel, K::Beam, 80, 2.6f).low(St::Def, 0.1f));
    add(B("Iron Head", T::Steel, K::Melee, 80, 1.3f).st(S::Flinch, 0.3f));
    add(B("Meteor Mash", T::Steel, K::Dash, 90, 3.2f));
    add(B("Iron Tail", T::Steel, K::Melee, 100, 2.8f).low(St::Def, 0.3f));
    add(B("Iron Defense", T::Steel, K::Buff, 0, 9).buff(St::Def, 1.6f));

    // ---------------- Fairy
    add(B("Fairy Wind", T::Fairy, K::Projectile, 40, 0.7f).knock(1.6f));
    add(B("Disarming Voice", T::Fairy, K::Wave, 40, 1.8f).radius(70));
    add(B("Charm", T::Fairy, K::Burst, 0, 6).radius(70).windup(0.15f).low(St::Atk, 1));
    add(B("Sweet Kiss", T::Fairy, K::Projectile, 0, 6).homing(2).speed(170).st(S::Confuse, 1));
    add(B("Draining Kiss", T::Fairy, K::Melee, 50, 0.9f).drain(0.75f));
    add(B("Dazzling Gleam", T::Fairy, K::Burst, 80, 3.5f).radius(50));
    add(B("Play Rough", T::Fairy, K::Dash, 90, 3).low(St::Atk, 0.1f));
    add(B("Moonblast", T::Fairy, K::Projectile, 95, 3).radius(7).speed(200).low(St::Atk, 0.3f));
    add(B("Moonlight", T::Fairy, K::Heal, 0, 14).heal(0.5f));

    // ================= Expansion: more moves per type, using every move kind =================
    // ---------------- Normal
    add(B("Hyper Fang", T::Normal, K::Melee, 80, 1.6f).st(S::Flinch, 0.1f));
    add(B("Stomp", T::Normal, K::Leap, 65, 2.4f).range(100).st(S::Flinch, 0.3f));
    add(B("Take Down", T::Normal, K::Dash, 90, 2.4f).recoil(0.25f));
    add(B("Giga Impact", T::Normal, K::Dash, 150, 9).range(130).windup(0.4f).recharge(1.5f));
    add(B("Tri Attack", T::Normal, K::Spread, 50, 2.2f).st(S::Paralyze, 0.1f));
    add(B("Uproar", T::Normal, K::Cone, 90, 3.0f).range(80));
    add(B("Boomburst", T::Normal, K::Wave, 140, 7).radius(120).windup(0.5f));
    add(B("Wrap", T::Normal, K::Tether, 15, 6).tether(2).range(90).dur(3.5f).phys());
    add(B("Mega Kick", T::Normal, K::Melee, 120, 3.5f).windup(0.25f).knock(2));
    add(B("Spike Cannon", T::Normal, K::Projectile, 20, 1.6f).hits(4).speed(300));

    // ---------------- Fire
    add(B("Flame Charge", T::Fire, K::Dash, 50, 1.4f).quick().range(105));
    add(B("Inferno", T::Fire, K::Cone, 100, 5).range(70).st(S::Burn, 1.0f));
    add(B("Lava Plume", T::Fire, K::Burst, 80, 3.2f).radius(46).st(S::Burn, 0.3f));
    add(B("Overheat", T::Fire, K::Sweep, 130, 7).windup(0.5f).radius(8).self(St::Atk).arc(1.3f).dur(0.6f));
    add(B("Magma Storm", T::Fire, K::Vortex, 25, 7).radius(36).pull(70).st(S::Burn, 0.2f));
    add(B("Blaze Kick", T::Fire, K::Melee, 85, 1.8f).crit().st(S::Burn, 0.1f));
    add(B("Mystical Fire", T::Fire, K::Projectile, 75, 2.0f).low(St::Atk, 1));
    add(B("Searing Shot", T::Fire, K::Orbit, 55, 7).count(3).dur(4).st(S::Burn, 0.3f));
    add(B("Eruption", T::Fire, K::Rain, 45, 7).rain(7, 16).radius(44).st(S::Burn, 0.1f).pmod(PowerMod::Eruption));
    add(B("Heat Crash", T::Fire, K::Leap, 90, 3.2f).radius(40));
    add(B("Fire Lash", T::Fire, K::Melee, 80, 1.6f).range(40).low(St::Def, 1));

    // ---------------- Water
    add(B("Scald", T::Water, K::Projectile, 80, 2.0f).st(S::Burn, 0.3f));
    add(B("Waterfall", T::Water, K::Dash, 80, 1.9f).st(S::Flinch, 0.2f));
    add(B("Razor Shell", T::Water, K::Melee, 75, 1.4f).low(St::Def, 0.5f));
    add(B("Brine", T::Water, K::Lob, 65, 1.8f).pmod(PowerMod::Brine));
    add(B("Water Spout", T::Water, K::Rain, 40, 6).rain(7, 15).radius(42).pmod(PowerMod::Eruption));
    add(B("Muddy Water", T::Water, K::Wave, 90, 4.5f).low(St::Spd, 0.3f));
    add(B("Liquidation", T::Water, K::Melee, 85, 1.8f).low(St::Def, 0.2f));
    add(B("Water Shuriken", T::Water, K::Boomerang, 45, 1.2f).range(150).speed(260));
    add(B("Crabhammer", T::Water, K::Melee, 100, 2.4f).crit());
    add(B("Snipe Shot", T::Water, K::Projectile, 80, 2.2f).speed(400).range(300).crit());
    add(B("Bubble Trap", T::Water, K::Trap, 60, 5).low(St::Spd, 1));

    // ---------------- Grass
    add(B("Magical Leaf", T::Grass, K::Projectile, 60, 1.2f).homing(6));
    add(B("Leaf Storm", T::Grass, K::Spread, 110, 6).self(St::Atk).speed(240));
    add(B("Wood Hammer", T::Grass, K::Dash, 120, 4).recoil(0.33f));
    add(B("Power Whip", T::Grass, K::Grapple, 120, 4.0f).range(175).speed(340).windup(0.2f));
    add(B("Energy Ball", T::Grass, K::Projectile, 90, 2.4f).bounce(2).radius(5).low(St::Def, 0.1f));
    add(B("Petal Dance", T::Grass, K::Orbit, 60, 7).count(4).dur(4));
    add(B("Grass Knot", T::Grass, K::Trap, 70, 4).low(St::Spd, 1));
    add(B("Leaf Tornado", T::Grass, K::Vortex, 25, 5).radius(32).pull(65));
    add(B("Horn Leech", T::Grass, K::Melee, 75, 1.8f).drain(0.5f));
    add(B("Frenzy Plant", T::Grass, K::Wave, 150, 9).radius(115).windup(0.5f).recharge(1.5f));
    add(B("Spiky Shield", T::Grass, K::Wall, 0, 6).count(3));

    // ---------------- Electric
    add(B("Volt Tackle", T::Electric, K::Dash, 120, 4.5f).recoil(0.33f).st(S::Paralyze, 0.1f));
    add(B("Zap Cannon", T::Electric, K::Projectile, 120, 6).speed(140).radius(7).st(S::Paralyze, 1.0f));
    add(B("Thunder Fang", T::Electric, K::Melee, 65, 1.0f).st(S::Paralyze, 0.1f));
    add(B("Electroweb", T::Electric, K::Cone, 55, 2.4f).low(St::Spd, 1));
    add(B("Parabolic Charge", T::Electric, K::Tether, 65, 4.5f).range(130).dur(2.6f).drain(0.5f));
    add(B("Volt Switch", T::Electric, K::Boomerang, 70, 2.0f).range(160));
    add(B("Thunder Cage", T::Electric, K::Tether, 25, 7).tether(2).range(110).dur(4.0f).st(S::Paralyze, 0.2f).spec());
    add(B("Zing Zap", T::Electric, K::Dash, 80, 2.0f).st(S::Flinch, 0.3f));
    add(B("Fusion Bolt", T::Electric, K::Beam, 100, 4).windup(0.4f));
    add(B("Plasma Fists", T::Electric, K::Orbit, 50, 7).count(3).dur(4).st(S::Paralyze, 0.1f));
    add(B("Shock Mine", T::Electric, K::Trap, 70, 4).st(S::Paralyze, 0.5f));

    // ---------------- Ice
    add(B("Frost Breath", T::Ice, K::Cone, 60, 2.0f).crit());
    add(B("Icicle Spear", T::Ice, K::Projectile, 25, 1.6f).hits(4).speed(320));
    add(B("Icicle Crash", T::Ice, K::Rain, 85, 3.6f).rain(1, 22).range(170).dur(0.7f).st(S::Flinch, 0.3f));
    add(B("Ice Hammer", T::Ice, K::Melee, 100, 2.6f).self(St::Spd));
    add(B("Glaciate", T::Ice, K::Wave, 65, 3.5f).low(St::Spd, 1));
    add(B("Aurora Veil", T::Ice, K::Wall, 0, 7).count(5).dur(5));
    add(B("Ice Ball", T::Ice, K::Roll, 40, 4).speed(240).dur(2.4f).bounce(5).boost(0.3f).st(S::Freeze, 0.05f));
    add(B("Triple Axel", T::Ice, K::Melee, 30, 1.6f).hits(3));
    add(B("Ice Spinner", T::Ice, K::Orbit, 55, 7).count(3).dur(4).st(S::Freeze, 0.05f));
    add(B("Sheer Cold", T::Ice, K::Cone, 0, 9).range(70).ohko());

    // ---------------- Fighting
    add(B("High Jump Kick", T::Fighting, K::Leap, 130, 4.5f).range(140).recoil(0.25f));
    add(B("Drain Punch", T::Fighting, K::Melee, 75, 1.8f).drain(0.5f));
    add(B("Force Palm", T::Fighting, K::Melee, 60, 1.2f).st(S::Paralyze, 0.3f));
    add(B("Rock Smash", T::Fighting, K::Melee, 40, 0.8f).low(St::Def, 0.5f));
    add(B("Dynamic Punch", T::Fighting, K::Melee, 100, 4).windup(0.3f).st(S::Confuse, 1.0f));
    add(B("Mat Block", T::Fighting, K::Wall, 0, 6).count(3));
    add(B("Circle Throw", T::Fighting, K::Throw, 60, 2.6f).toss(120).force());
    add(B("Superpower", T::Fighting, K::Melee, 120, 4).self(St::Atk));
    add(B("Seismic Toss", T::Fighting, K::Throw, 0, 2.6f).toss(80).fixed(-1));
    add(B("Aura Wheel", T::Fighting, K::Orbit, 50, 7).count(4).dur(4));

    // ---------------- Poison
    add(B("Toxic Spikes", T::Poison, K::Trap, 0, 4).st(S::Toxic, 1.0f));
    add(B("Cross Poison", T::Poison, K::Melee, 70, 1.4f).crit().st(S::Poison, 0.1f));
    add(B("Sludge Wave", T::Poison, K::Wave, 95, 4.5f).st(S::Poison, 0.1f));
    add(B("Belch", T::Poison, K::Cone, 120, 6).range(70).windup(0.4f));
    add(B("Clear Smog", T::Poison, K::Projectile, 50, 1.2f).bounce(1));
    add(B("Poison Tail", T::Poison, K::Melee, 50, 1.0f).crit().st(S::Poison, 0.1f));
    add(B("Acid Spray", T::Poison, K::Lob, 40, 2.4f).low(St::Def, 1).patch(8, 2.5f, 14));
    add(B("Toxic Thread", T::Poison, K::Cone, 0, 5).st(S::Poison, 1.0f).low(St::Spd, 1));
    add(B("Baneful Bunker", T::Poison, K::Wall, 0, 7).count(3).dur(4));
    add(B("Poison Gas", T::Poison, K::Vortex, 0, 6).radius(34).pull(40).st(S::Poison, 0.3f));

    // ---------------- Ground
    add(B("Spikes", T::Ground, K::Trap, 55, 3.5f));
    add(B("Mud Bomb", T::Ground, K::Lob, 65, 1.8f).low(St::Spd, 0.3f));
    add(B("Drill Run", T::Ground, K::Dash, 80, 1.9f).crit());
    add(B("High Horsepower", T::Ground, K::Dash, 95, 2.4f).knock(2));
    add(B("Stomping Tantrum", T::Ground, K::Leap, 75, 2.6f).radius(40));
    add(B("Sand Attack", T::Ground, K::Cone, 0, 5).low(St::Spd, 1));
    add(B("Bone Rush", T::Ground, K::Melee, 25, 1.6f).hits(4));
    add(B("Scorching Sands", T::Ground, K::Projectile, 70, 1.8f).st(S::Burn, 0.3f));
    add(B("Thousand Arrows", T::Ground, K::Rain, 40, 5.5f).rain(7, 14).radius(46));
    add(B("Land's Wrath", T::Ground, K::Wave, 90, 4.5f).radius(105));
    add(B("Sandstorm", T::Rock, K::Weather, 0, 14).weather(Weather::Sand));

    // ---------------- Flying
    add(B("Drill Peck", T::Flying, K::Dash, 80, 1.9f));
    add(B("Sky Attack", T::Flying, K::Dash, 140, 7).windup(0.9f).range(140).crit().st(S::Flinch, 0.3f));
    add(B("Bounce", T::Flying, K::Leap, 85, 3.0f).range(150).dur(0.9f).st(S::Paralyze, 0.3f));
    add(B("Acrobatics", T::Flying, K::Melee, 55, 1.0f).hits(2));
    add(B("Aeroblast", T::Flying, K::Beam, 100, 4).crit().windup(0.4f));
    add(B("Feather Dance", T::Flying, K::Cone, 0, 6).low(St::Atk, 1));
    add(B("Dual Wingbeat", T::Flying, K::Melee, 40, 1.2f).hits(2).range(30));
    add(B("Oblivion Wing", T::Flying, K::Tether, 80, 5.0f).range(160).dur(2.6f).drain(0.75f));
    add(B("Bleakwind Storm", T::Flying, K::Vortex, 30, 6).radius(40).pull(75).low(St::Spd, 0.3f));
    add(B("Razor Wind", T::Flying, K::Boomerang, 60, 1.8f).crit());

    // ---------------- Psychic
    add(B("Psyshock", T::Psychic, K::Projectile, 80, 2.0f).homing(2));
    add(B("Extrasensory", T::Psychic, K::Projectile, 80, 2.0f).st(S::Flinch, 0.1f));
    add(B("Psycho Boost", T::Psychic, K::Beam, 140, 7).windup(0.6f).self(St::Atk));
    add(B("Reflect", T::Psychic, K::Wall, 0, 6).count(3).dur(5));
    add(B("Light Screen", T::Psychic, K::Wall, 0, 6).count(5).dur(3));
    add(B("Psywave", T::Psychic, K::Chain, 0, 2.0f).range(170).fixed(-1));
    add(B("Expanding Force", T::Psychic, K::Burst, 80, 3.0f).radius(50));
    add(B("Dream Eater", T::Psychic, K::Tether, 100, 5).range(160).dur(3.0f).drain(0.5f).sleepOnly());
    add(B("Psychic Fangs", T::Psychic, K::Melee, 85, 1.8f));
    add(B("Psyshield Bash", T::Psychic, K::Dash, 70, 1.8f));
    add(B("Mist Ball", T::Psychic, K::Orbit, 60, 7).count(3).dur(4).low(St::Atk, 0.5f));
    add(B("Psychic Trap", T::Psychic, K::Trap, 70, 4).st(S::Confuse, 0.5f));

    // ---------------- Bug
    add(B("Sticky Web", T::Bug, K::Trap, 0, 4).low(St::Spd, 1));
    add(B("Lunge", T::Bug, K::Dash, 80, 2.0f).low(St::Atk, 1));
    add(B("First Impression", T::Bug, K::Dash, 90, 3.0f).quick().range(120));
    add(B("U-turn", T::Bug, K::Boomerang, 70, 2.0f).range(150).phys());
    add(B("Attack Order", T::Bug, K::Rain, 30, 4.5f).rain(6, 12).radius(36).crit());
    add(B("Fell Stinger", T::Bug, K::Melee, 50, 1.2f));
    add(B("Struggle Bug", T::Bug, K::Cone, 50, 1.8f).low(St::Atk, 1));
    add(B("Infestation", T::Bug, K::Tether, 20, 6).tether(2).range(100).dur(3.5f).spec());
    add(B("Pollen Puff", T::Bug, K::Lob, 90, 2.6f));
    add(B("Steamroller", T::Bug, K::Roll, 65, 3.5f).speed(270).dur(1.7f).bounce(3).st(S::Flinch, 0.3f));
    add(B("Silk Barrier", T::Bug, K::Wall, 0, 6).count(3));

    // ---------------- Rock
    add(B("Rock Wrecker", T::Rock, K::Dash, 150, 9).range(130).windup(0.4f).recharge(1.5f));
    add(B("Smack Down", T::Rock, K::Grapple, 50, 1.8f).range(150).pullFrac(0.5f).antiAir());
    add(B("Rollout", T::Rock, K::Roll, 40, 4).speed(230).dur(2.4f).bounce(5).boost(0.3f));
    add(B("Diamond Storm", T::Rock, K::Rain, 35, 5).rain(7, 13).radius(40));
    add(B("Wide Guard", T::Rock, K::Wall, 0, 6).count(5).dur(4));
    add(B("Accelerock", T::Rock, K::Dash, 40, 1.0f).quick().range(115));
    add(B("Meteor Beam", T::Rock, K::Sweep, 120, 6).windup(0.9f).radius(8).arc(1.3f).dur(0.65f));
    add(B("Stone Axe", T::Rock, K::Melee, 65, 1.6f).crit());
    add(B("Rock Ricochet", T::Rock, K::Projectile, 60, 1.8f).bounce(3).speed(260));
    add(B("Rock Orbit", T::Rock, K::Orbit, 60, 7).count(3).dur(4));

    // ---------------- Ghost
    add(B("Shadow Punch", T::Ghost, K::Melee, 60, 1.2f).range(34));
    add(B("Poltergeist", T::Ghost, K::Rain, 110, 5).rain(1, 24).range(200).dur(1.2f));
    add(B("Shadow Bone", T::Ghost, K::Boomerang, 85, 2.4f).phys().low(St::Def, 0.2f));
    add(B("Moongeist Beam", T::Ghost, K::Sweep, 100, 4).windup(0.45f).arc(1.1f).dur(0.5f));
    add(B("Spirit Shackle", T::Ghost, K::Grapple, 80, 2.4f).range(160).low(St::Spd, 1).pinFor(3.0f));
    add(B("Infernal Parade", T::Ghost, K::Orbit, 60, 7).count(3).dur(4).st(S::Burn, 0.3f).hex());
    add(B("Bitter Malice", T::Ghost, K::Cone, 75, 2.6f).low(St::Atk, 1));
    add(B("Last Respects", T::Ghost, K::Melee, 50, 1.2f).hex());
    add(B("Curse Trap", T::Ghost, K::Trap, 80, 4.5f).hex());
    add(B("Ghost Vortex", T::Ghost, K::Vortex, 25, 6).radius(36).pull(60).st(S::Confuse, 0.2f));

    // ---------------- Dragon
    add(B("Dragon Darts", T::Dragon, K::Projectile, 50, 1.6f).hits(2).homing(3));
    add(B("Dual Chop", T::Dragon, K::Melee, 40, 1.2f).hits(2));
    add(B("Scale Shot", T::Dragon, K::Projectile, 25, 1.8f).hits(4).speed(300));
    add(B("Dragon Hammer", T::Dragon, K::Leap, 90, 3.0f).radius(40));
    add(B("Breaking Swipe", T::Dragon, K::Cone, 60, 2.0f).range(50).radius(55).low(St::Atk, 1).phys());
    add(B("Core Enforcer", T::Dragon, K::Wave, 100, 5).radius(110));
    add(B("Spacial Rend", T::Dragon, K::Beam, 100, 4).crit().windup(0.4f));
    add(B("Clanging Scales", T::Dragon, K::Burst, 110, 4.5f).radius(52).self(St::Def));
    add(B("Dragon Energy", T::Dragon, K::Cone, 150, 8).range(85).windup(0.6f));
    add(B("Dragon Orbit", T::Dragon, K::Orbit, 65, 7).count(3).dur(4));

    // ---------------- Dark
    add(B("Throat Chop", T::Dark, K::Melee, 80, 1.6f));
    add(B("Darkest Lariat", T::Dark, K::Orbit, 85, 7).count(2).dur(3).speed(8).range(22));
    add(B("Foul Play", T::Dark, K::Melee, 95, 2.2f));
    add(B("Beat Up", T::Dark, K::Melee, 15, 1.4f).hits(5));
    add(B("Fiery Wrath", T::Dark, K::Cone, 90, 3.2f).st(S::Flinch, 0.2f));
    add(B("Jaw Lock", T::Dark, K::Melee, 80, 1.8f).low(St::Spd, 0.5f));
    add(B("Fling", T::Dark, K::Lob, 60, 1.6f));
    add(B("Night Daze", T::Dark, K::Wave, 85, 3.8f).low(St::Def, 0.4f));
    add(B("Wicked Blow", T::Dark, K::Melee, 75, 2.0f).crit());
    add(B("Shadow Shuriken", T::Dark, K::Boomerang, 55, 1.5f).range(160).speed(250).crit());
    add(B("Snare", T::Dark, K::Trap, 65, 4).st(S::Flinch, 1.0f));

    // ---------------- Steel
    add(B("Smart Strike", T::Steel, K::Projectile, 70, 1.6f).homing(8).phys());
    add(B("Anchor Shot", T::Steel, K::Grapple, 80, 2.4f).range(160).speed(280).low(St::Spd, 1).pinFor(3.0f).phys());
    add(B("Heavy Slam", T::Steel, K::Leap, 100, 3.6f).radius(42));
    add(B("Steel Beam", T::Steel, K::Sweep, 140, 7).windup(0.6f).recoil(0.4f).arc(1.4f).dur(0.65f));
    add(B("Doom Desire", T::Steel, K::Rain, 140, 9).rain(1, 30).range(220).dur(2.0f));
    add(B("Sunsteel Strike", T::Steel, K::Dash, 100, 3.0f));
    add(B("Gear Grind", T::Steel, K::Boomerang, 50, 1.5f).range(140).phys());
    add(B("King's Shield", T::Steel, K::Wall, 0, 6).count(3).dur(5));
    add(B("Make It Rain", T::Steel, K::Rain, 35, 5).rain(7, 13).radius(42));
    add(B("Steel Roller", T::Steel, K::Roll, 130, 6).speed(290).dur(1.8f).bounce(3).boost(0.1f).clears());
    add(B("Mirror Ricochet", T::Steel, K::Projectile, 65, 1.8f).bounce(3).speed(280));

    // ---------------- Fairy
    add(B("Spirit Break", T::Fairy, K::Melee, 75, 1.6f).low(St::Atk, 1));
    add(B("Strange Steam", T::Fairy, K::Cone, 90, 3.2f).st(S::Confuse, 0.2f));
    add(B("Misty Explosion", T::Fairy, K::Burst, 100, 5).radius(56));
    add(B("Fleur Cannon", T::Fairy, K::Sweep, 130, 7).windup(0.5f).self(St::Atk).arc(1.3f).dur(0.6f));
    add(B("Light of Ruin", T::Fairy, K::Sweep, 140, 7).windup(0.6f).recoil(0.4f).arc(1.4f).dur(0.65f));
    add(B("Crafty Shield", T::Fairy, K::Wall, 0, 6).count(3));
    add(B("Sparkling Aria", T::Fairy, K::Wave, 90, 4).radius(100));
    add(B("Floral Healing", T::Fairy, K::Heal, 0, 12).heal(0.4f));
    add(B("Springtide Storm", T::Fairy, K::Vortex, 30, 6).radius(40).pull(65).low(St::Atk, 0.3f));
    add(B("Alluring Voice", T::Fairy, K::Projectile, 80, 2.0f).st(S::Confuse, 0.2f));
    add(B("Fairy Ring", T::Fairy, K::Orbit, 50, 7).count(4).dur(4));
    add(B("Baby-Doll Eyes", T::Fairy, K::Cone, 0, 5).low(St::Atk, 1));


    // ================= Classic moves (Gen 1-4 and favorites), for real learnsets =================
    // ---------------- Normal
    add(B("Pound", T::Normal, K::Melee, 40, 0.5f));
    add(B("Double Slap", T::Normal, K::Melee, 15, 1.2f).hits(4));
    add(B("Comet Punch", T::Normal, K::Melee, 18, 1.2f).hits(4));
    add(B("Mega Punch", T::Normal, K::Melee, 80, 2.0f).windup(0.15f));
    add(B("Pay Day", T::Normal, K::Projectile, 40, 1.2f).payday());
    add(B("Vise Grip", T::Normal, K::Melee, 55, 1.0f));
    add(B("Guillotine", T::Normal, K::Melee, 0, 9).ohko());
    add(B("Cut", T::Normal, K::Melee, 50, 0.9f).range(30));
    add(B("Bind", T::Normal, K::Tether, 15, 6).tether(2).range(90).dur(3.5f).phys());
    add(B("Slam", T::Normal, K::Melee, 80, 1.9f).range(36));
    add(B("Rolling Kick", T::Normal, K::Melee, 60, 1.4f).st(S::Flinch, 0.3f));
    add(B("Horn Attack", T::Normal, K::Dash, 65, 1.5f));
    add(B("Fury Attack", T::Normal, K::Melee, 15, 1.2f).hits(4));
    add(B("Horn Drill", T::Normal, K::Dash, 0, 9).ohko());
    add(B("Thrash", T::Normal, K::Melee, 120, 4).hits(2).st(S::Confuse, 0.3f));
    add(B("Roar", T::Normal, K::Cone, 0, 8).range(90).force());
    add(B("Whirlwind", T::Normal, K::Cone, 0, 8).range(90).force());
    add(B("Sing", T::Normal, K::Cone, 0, 7).range(80).st(S::Sleep, 1.0f));
    add(B("Supersonic", T::Normal, K::Projectile, 0, 5).st(S::Confuse, 1.0f));
    add(B("Sonic Boom", T::Normal, K::Projectile, 0, 1.2f).fixed(8).speed(320));
    add(B("Disable", T::Normal, K::Chain, 0, 8).low(St::Atk, 1));
    add(B("Strength", T::Normal, K::Melee, 80, 1.8f).knock(2));
    add(B("Rage", T::Normal, K::Melee, 20, 0.6f));
    add(B("Screech", T::Normal, K::Cone, 0, 6).range(90).low(St::Def, 1));
    add(B("Minimize", T::Normal, K::Buff, 0, 10).buff(St::Spd, 1.4f).buff2(St::Def));
    add(B("Defense Curl", T::Normal, K::Buff, 0, 8).buff(St::Def, 1.4f));
    add(B("Focus Energy", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.3f));
    add(B("Bide", T::Normal, K::Shield, 0, 8).dur(1.4f).counter());
    add(B("Metronome", T::Normal, K::Projectile, 0, 3).metro());
    add(B("Mirror Move", T::Flying, K::Projectile, 0, 3).copy());
    add(B("Self-Destruct", T::Normal, K::Burst, 200, 5).radius(64).windup(0.6f).selfko());
    add(B("Explosion", T::Normal, K::Burst, 250, 5).radius(76).windup(0.7f).selfko());
    add(B("Egg Bomb", T::Normal, K::Lob, 100, 3.0f));
    add(B("Soft-Boiled", T::Normal, K::Heal, 0, 14).heal(0.5f));
    add(B("Constrict", T::Normal, K::Melee, 10, 0.5f).low(St::Spd, 0.1f));
    add(B("Barrage", T::Normal, K::Projectile, 15, 1.4f).hits(4));
    add(B("Glare", T::Normal, K::Cone, 0, 7).range(85).st(S::Paralyze, 1.0f));
    add(B("Lovely Kiss", T::Normal, K::Melee, 0, 7).st(S::Sleep, 1.0f));
    add(B("Dizzy Punch", T::Normal, K::Melee, 70, 1.6f).st(S::Confuse, 0.2f));
    add(B("Flash", T::Normal, K::Burst, 0, 6).radius(70).low(St::Atk, 1));
    add(B("Splash", T::Normal, K::Buff, 0, 2).buff(St::Atk, 1.0f));
    add(B("Sharpen", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.4f));
    add(B("Super Fang", T::Normal, K::Melee, 0, 4).fixed(-2));
    add(B("Conversion", T::Normal, K::Buff, 0, 10).buff(St::Def, 1.3f));
    add(B("Substitute", T::Normal, K::Shield, 0, 9).dur(1.6f));
    add(B("Struggle", T::Normal, K::Melee, 50, 1.2f).recoil(0.25f));
    add(B("Snore", T::Normal, K::Cone, 50, 1.8f).st(S::Flinch, 0.3f));
    add(B("Flail", T::Normal, K::Melee, 40, 1.6f).pmod(PowerMod::Reversal));
    add(B("Scary Face", T::Normal, K::Cone, 0, 6).range(90).low(St::Spd, 1));
    add(B("Belly Drum", T::Normal, K::Buff, 0, 16).buff(St::Atk, 2.2f).uses(1));
    add(B("Endure", T::Normal, K::Shield, 0, 8).dur(1.4f).endure());
    add(B("False Swipe", T::Normal, K::Melee, 40, 0.9f).fswipe());
    add(B("Swagger", T::Normal, K::Projectile, 0, 6).st(S::Confuse, 1.0f));
    add(B("Milk Drink", T::Normal, K::Heal, 0, 14).heal(0.5f));
    add(B("Attract", T::Normal, K::Projectile, 0, 7).st(S::Confuse, 1.0f).homing(4));
    add(B("Return", T::Normal, K::Melee, 102, 2.2f));
    add(B("Frustration", T::Normal, K::Melee, 102, 2.2f));
    add(B("Present", T::Normal, K::Lob, 80, 2.0f));
    add(B("Safeguard", T::Normal, K::Shield, 0, 9).dur(1.2f));
    add(B("Pain Split", T::Normal, K::Chain, 0, 10).fixed(-2).range(140));
    add(B("Extreme Speed", T::Normal, K::Dash, 80, 1.6f).quick().range(140));
    add(B("Hidden Power", T::Normal, K::Projectile, 60, 1.4f));
    add(B("Psych Up", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.4f));
    add(B("Fake Out", T::Normal, K::Dash, 40, 3.0f).quick().st(S::Flinch, 1.0f));
    add(B("Stockpile", T::Normal, K::Buff, 0, 8).buff(St::Def, 1.4f));
    add(B("Spit Up", T::Normal, K::Projectile, 100, 4).speed(200).radius(6));
    add(B("Swallow", T::Normal, K::Heal, 0, 14).heal(0.4f));
    add(B("Facade", T::Normal, K::Melee, 70, 1.6f).pmod(PowerMod::Facade));
    add(B("Smelling Salts", T::Normal, K::Melee, 70, 1.6f).hex());
    add(B("Helping Hand", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.3f));
    add(B("Wish", T::Normal, K::Heal, 0, 14).heal(0.5f));
    add(B("Refresh", T::Normal, K::Heal, 0, 9).heal(0.05f).cure());
    add(B("Secret Power", T::Normal, K::Melee, 70, 1.6f).st(S::Paralyze, 0.3f));
    add(B("Slack Off", T::Normal, K::Heal, 0, 14).heal(0.5f));
    add(B("Weather Ball", T::Normal, K::Projectile, 50, 1.6f).pmod(PowerMod::WeatherBall));
    add(B("Tickle", T::Normal, K::Melee, 0, 6).low(St::Atk, 1));
    add(B("Block", T::Normal, K::Wall, 0, 6).count(3));
    add(B("Howl", T::Normal, K::Buff, 0, 9).buff(St::Atk, 1.4f));
    add(B("Covet", T::Normal, K::Dash, 60, 1.6f));
    add(B("Crush Claw", T::Normal, K::Melee, 75, 1.6f).low(St::Def, 0.5f));
    add(B("Feint", T::Normal, K::Dash, 30, 1.0f).quick());
    add(B("Last Resort", T::Normal, K::Dash, 140, 6));
    add(B("Double Hit", T::Normal, K::Melee, 35, 1.2f).hits(2));
    add(B("Crush Grip", T::Normal, K::Melee, 100, 2.8f).pmod(PowerMod::Eruption));
    add(B("Work Up", T::Normal, K::Buff, 0, 9).buff(St::Atk, 1.4f));
    add(B("Echoed Voice", T::Normal, K::Cone, 40, 1.0f));
    add(B("Retaliate", T::Normal, K::Melee, 70, 1.6f));
    add(B("Shell Smash", T::Normal, K::Buff, 0, 14).buff(St::Atk, 1.8f).buff2(St::Spd).self(St::Def));
    add(B("Haze", T::Ice, K::Burst, 0, 8).radius(80).haze());
    add(B("Mist", T::Ice, K::Shield, 0, 8).dur(1.0f));
    add(B("Hail", T::Ice, K::Weather, 0, 14).weather(Weather::Snow));
    add(B("Snowscape", T::Ice, K::Weather, 0, 14).weather(Weather::Snow));
    add(B("Rain Dance", T::Water, K::Weather, 0, 14).weather(Weather::Rain));
    add(B("Sunny Day", T::Fire, K::Weather, 0, 14).weather(Weather::Sun));
    add(B("Body Press", T::Fighting, K::Melee, 80, 1.9f));

    // Gen 1 learnset additions (real-time takes on the originals)
    add(B("Copycat", T::Normal, K::Projectile, 0, 3).copy());
    add(B("Mimic", T::Normal, K::Projectile, 0, 3).copy());
    add(B("Sleep Talk", T::Normal, K::Projectile, 0, 3).metro());
    add(B("Transform", T::Normal, K::Chain, 0, 4).range(220).xform());
    add(B("Endeavor", T::Normal, K::Dash, 0, 6).fixed(-2));
    add(B("Mean Look", T::Normal, K::Tether, 0, 8).tether(3).range(170).dur(4.0f).low(St::Spd, 1));
    add(B("Baton Pass", T::Normal, K::Blink, 0, 5));
    add(B("Encore", T::Normal, K::Chain, 0, 8).low(St::Atk, 1).range(150));
    add(B("Laser Focus", T::Normal, K::Buff, 0, 9).buff(St::Atk, 1.3f));
    add(B("Reflect Type", T::Normal, K::Buff, 0, 10).buff(St::Def, 1.3f));
    add(B("Yawn", T::Normal, K::Projectile, 0, 8).speed(110).st(S::Sleep, 0.6f));
    add(B("Lock-On", T::Normal, K::Buff, 0, 8).buff(St::Atk, 1.25f));
    add(B("Perish Song", T::Normal, K::Wave, 50, 6).low(St::Def, 0.5f).spec());
    add(B("Recycle", T::Normal, K::Heal, 0, 14).heal(0.2f));
    add(B("Round", T::Normal, K::Cone, 60, 1.6f));
    add(B("Acupressure", T::Normal, K::Buff, 0, 10).buff(St::Atk, 1.3f).buff2(St::Spd));
    add(B("After You", T::Normal, K::Buff, 0, 8).buff(St::Spd, 1.3f));
    add(B("Follow Me", T::Normal, K::Shield, 0, 7).dur(1.2f));
    add(B("Teeter Dance", T::Normal, K::Burst, 0, 9).radius(90).st(S::Confuse, 1.0f));
    add(B("Raging Bull", T::Normal, K::Dash, 90, 2.4f));
    add(B("Conversion 2", T::Normal, K::Buff, 0, 10).buff(St::Def, 1.3f));
    add(B("Quick Guard", T::Fighting, K::Shield, 0, 6).dur(1.0f).quick());
    add(B("Final Gambit", T::Fighting, K::Dash, 130, 6).windup(0.4f).selfko());
    add(B("Life Dew", T::Water, K::Heal, 0, 10).heal(0.25f));
    add(B("Soak", T::Water, K::Projectile, 0, 6).low(St::Def, 1));
    add(B("Grassy Terrain", T::Grass, K::Heal, 0, 14).heal(0.25f));
    add(B("Magnetic Flux", T::Electric, K::Buff, 0, 10).buff(St::Def, 1.3f));
    add(B("Imprison", T::Psychic, K::Chain, 0, 8).low(St::Atk, 1).range(150));
    add(B("Wonder Room", T::Psychic, K::Buff, 0, 10).buff(St::Def, 1.3f));
    add(B("Ally Switch", T::Psychic, K::Blink, 0, 5));
    add(B("Power Swap", T::Psychic, K::Chain, 0, 9).low(St::Atk, 1).range(150));
    add(B("Guard Swap", T::Psychic, K::Chain, 0, 9).low(St::Def, 1).range(150));
    add(B("Gravity", T::Psychic, K::Burst, 0, 10).radius(100).low(St::Spd, 1));

    // ---------------- Fire
    add(B("Sacred Fire", T::Fire, K::Lob, 100, 3.5f).st(S::Burn, 0.5f));
    add(B("Blast Burn", T::Fire, K::Wave, 150, 9).radius(115).windup(0.5f).recharge(1.5f));
    add(B("Fire Pledge", T::Fire, K::Wave, 80, 3.5f));
    add(B("Incinerate", T::Fire, K::Spread, 60, 1.8f));
    add(B("Burn Up", T::Fire, K::Burst, 130, 6).radius(56));
    add(B("Fiery Dance", T::Fire, K::Orbit, 60, 7).count(3).dur(4));
    add(B("Blue Flare", T::Fire, K::Sweep, 130, 6).windup(0.5f).st(S::Burn, 0.2f).arc(1.2f).dur(0.55f));
    add(B("Pyro Ball", T::Fire, K::Lob, 120, 4).st(S::Burn, 0.1f));
    add(B("Torch Song", T::Fire, K::Cone, 80, 2.6f));

    // ---------------- Water
    add(B("Clamp", T::Water, K::Tether, 20, 6).tether(2).range(80).dur(3.5f).phys());
    add(B("Octazooka", T::Water, K::Projectile, 65, 1.8f).low(St::Atk, 0.5f));
    add(B("Mirror Coat", T::Psychic, K::Shield, 0, 8).dur(1.4f).counter());
    add(B("Water Sport", T::Water, K::Burst, 0, 8).radius(60));
    add(B("Hydro Cannon", T::Water, K::Sweep, 150, 9).windup(0.6f).recharge(1.5f).radius(8).arc(1.6f).dur(0.75f));
    add(B("Water Pledge", T::Water, K::Wave, 80, 3.5f));
    add(B("Steam Eruption", T::Water, K::Lob, 110, 4).st(S::Burn, 0.3f));
    add(B("Flip Turn", T::Water, K::Boomerang, 60, 1.8f).phys());
    add(B("Wave Crash", T::Water, K::Dash, 120, 4).recoil(0.33f));
    add(B("Jet Punch", T::Water, K::Dash, 60, 1.2f).quick());

    // ---------------- Grass
    add(B("Cotton Spore", T::Grass, K::Zone, 0, 6).low(St::Spd, 1));
    add(B("Ingrain", T::Grass, K::Heal, 0, 12).heal(0.35f));
    add(B("Needle Arm", T::Grass, K::Melee, 60, 1.4f).st(S::Flinch, 0.3f));
    add(B("Grass Whistle", T::Grass, K::Cone, 0, 7).range(80).st(S::Sleep, 1.0f));
    add(B("Worry Seed", T::Grass, K::Projectile, 0, 6).low(St::Def, 1));
    add(B("Seed Flare", T::Grass, K::Beam, 120, 5).windup(0.5f).low(St::Def, 0.4f));
    add(B("Aromatherapy", T::Grass, K::Heal, 0, 10).heal(0.1f).cure());
    add(B("Spore", T::Grass, K::Melee, 0, 8).st(S::Sleep, 1.0f));
    add(B("Grass Pledge", T::Grass, K::Wave, 80, 3.5f));
    add(B("Drum Beating", T::Grass, K::Wave, 80, 3.2f).low(St::Spd, 1));
    add(B("Trailblaze", T::Grass, K::Dash, 50, 1.4f));
    add(B("Grassy Glide", T::Grass, K::Dash, 55, 1.4f).quick());
    add(B("Apple Acid", T::Grass, K::Lob, 80, 2.2f).low(St::Def, 1));

    // ---------------- Electric
    add(B("Charge Beam", T::Electric, K::Beam, 50, 1.8f));
    add(B("Magnet Rise", T::Electric, K::Buff, 0, 10).buff(St::Spd, 1.4f));
    add(B("Wild Bolt", T::Electric, K::Dash, 90, 2.4f).recoil(0.25f));
    add(B("Rising Voltage", T::Electric, K::Rain, 70, 3.6f).rain(1, 24).range(200));
    add(B("Eerie Impulse", T::Electric, K::Burst, 0, 7).radius(70).low(St::Atk, 1));
    add(B("Electric Terrain", T::Electric, K::Zone, 20, 7).dur(6).radius(40).st(S::Paralyze, 0.1f));
    add(B("Overdrive", T::Electric, K::Wave, 80, 3.5f));
    add(B("Supercell Slam", T::Electric, K::Leap, 100, 3.6f).radius(40));

    // ---------------- Ice
    add(B("Ice Burn", T::Ice, K::Beam, 140, 7).windup(0.8f).st(S::Burn, 0.3f));
    add(B("Freeze Shock", T::Ice, K::Beam, 140, 7).windup(0.8f).st(S::Paralyze, 0.3f));
    add(B("Mountain Gale", T::Ice, K::Rain, 100, 4).rain(1, 26).range(170).st(S::Flinch, 0.3f));
    add(B("Chilling Water", T::Water, K::Projectile, 50, 1.4f).low(St::Atk, 1));

    // ---------------- Fighting
    add(B("Jump Kick", T::Fighting, K::Leap, 100, 3.2f).recoil(0.25f));
    add(B("Submission", T::Fighting, K::Throw, 80, 2.4f).toss(70).recoil(0.25f));
    add(B("Counter", T::Fighting, K::Shield, 0, 8).dur(1.4f).counter());
    add(B("Triple Kick", T::Fighting, K::Melee, 20, 1.4f).hits(3));
    add(B("Reversal", T::Fighting, K::Melee, 40, 1.6f).pmod(PowerMod::Reversal));
    add(B("Mind Reader", T::Fighting, K::Buff, 0, 10).buff(St::Atk, 1.3f));
    add(B("Vital Throw", T::Fighting, K::Throw, 70, 2.2f).toss(110));
    add(B("Focus Punch", T::Fighting, K::Melee, 150, 6).windup(1.0f));
    add(B("Arm Thrust", T::Fighting, K::Melee, 15, 1.2f).hits(5));
    add(B("Revenge", T::Fighting, K::Melee, 60, 1.4f));
    add(B("Sky Uppercut", T::Fighting, K::Leap, 85, 2.8f).range(90).dur(0.55f));
    add(B("Hammer Arm", T::Fighting, K::Melee, 100, 2.6f).self(St::Spd));
    add(B("Wake-Up Slap", T::Fighting, K::Melee, 70, 1.6f).hex());
    add(B("Low Sweep", T::Fighting, K::Melee, 65, 1.4f).low(St::Spd, 1));
    add(B("Storm Throw", T::Fighting, K::Throw, 60, 2.2f).toss(100).crit());
    add(B("Sacred Sword", T::Fighting, K::Melee, 90, 2.0f));
    add(B("Secret Sword", T::Fighting, K::Beam, 85, 3.2f).phys());
    add(B("Power-Up Punch", T::Fighting, K::Melee, 40, 1.0f));
    add(B("Axe Kick", T::Fighting, K::Leap, 120, 4).recoil(0.2f).st(S::Confuse, 0.3f));
    add(B("Collision Course", T::Fighting, K::Dash, 100, 3.2f));

    // ---------------- Poison
    add(B("Gastro Acid", T::Poison, K::Projectile, 0, 6).low(St::Atk, 1));
    add(B("Coil", T::Poison, K::Buff, 0, 10).buff(St::Atk, 1.4f).buff2(St::Def));
    add(B("Shell Side Arm", T::Poison, K::Projectile, 90, 2.2f).st(S::Poison, 0.2f));
    add(B("Mortal Spin", T::Poison, K::Burst, 30, 2.0f).radius(40).st(S::Poison, 1.0f));
    add(B("Barb Barrage", T::Poison, K::Projectile, 60, 1.6f).hex().st(S::Poison, 0.5f));
    add(B("Dire Claw", T::Poison, K::Melee, 80, 1.8f).crit().st(S::Poison, 0.3f));

    // ---------------- Ground
    add(B("Fissure", T::Ground, K::Wave, 0, 9).radius(110).ohko());
    add(B("Mud Sport", T::Ground, K::Burst, 0, 8).radius(60));
    add(B("Shore Up", T::Ground, K::Heal, 0, 14).heal(0.5f));
    add(B("Precipice Blades", T::Ground, K::Wave, 120, 6).radius(115));
    add(B("Headlong Rush", T::Ground, K::Dash, 120, 4).self(St::Def));

    // ---------------- Flying
    add(B("Sky Drop", T::Flying, K::Throw, 60, 3.0f).range(30).toss(30).skyDrop());
    add(B("Pluck", T::Flying, K::Melee, 60, 1.2f));
    add(B("Defog", T::Flying, K::Burst, 0, 8).radius(80).haze());
    add(B("Chatter", T::Flying, K::Cone, 65, 2.0f).st(S::Confuse, 0.3f));
    add(B("Beak Blast", T::Flying, K::Dash, 100, 3.2f).st(S::Burn, 0.3f));
    add(B("Floaty Fall", T::Flying, K::Leap, 90, 3.0f).st(S::Flinch, 0.3f));

    // ---------------- Psychic
    add(B("Kinesis", T::Psychic, K::Chain, 0, 6).low(St::Spd, 1).range(160));
    add(B("Meditate", T::Psychic, K::Buff, 0, 9).buff(St::Atk, 1.4f));
    add(B("Amnesia", T::Psychic, K::Buff, 0, 9).buff(St::Def, 1.7f));
    add(B("Trick", T::Psychic, K::Blink, 0, 4).range(120));
    add(B("Role Play", T::Psychic, K::Buff, 0, 10).buff(St::Atk, 1.3f));
    add(B("Magic Coat", T::Psychic, K::Shield, 0, 8).dur(1.2f).counter());
    add(B("Cosmic Power", T::Psychic, K::Buff, 0, 10).buff(St::Def, 1.5f));
    add(B("Luster Purge", T::Psychic, K::Projectile, 70, 1.8f).low(St::Def, 0.5f));
    add(B("Heal Pulse", T::Psychic, K::Heal, 0, 14).heal(0.4f));
    add(B("Trick Room", T::Psychic, K::Burst, 0, 10).radius(90).low(St::Spd, 1));
    add(B("Stored Power", T::Psychic, K::Projectile, 60, 1.6f));
    add(B("Synchronoise", T::Psychic, K::Burst, 120, 4).radius(56));
    add(B("Psystrike", T::Psychic, K::Projectile, 100, 3.2f).speed(300).phys());
    add(B("Lunar Dance", T::Psychic, K::Heal, 0, 20).heal(1.0f).uses(1));
    add(B("Healing Wish", T::Psychic, K::Heal, 0, 20).heal(1.0f).uses(1));
    add(B("Hyperspace Hole", T::Psychic, K::Blink, 80, 2.6f));
    add(B("Esper Wing", T::Psychic, K::Boomerang, 80, 2.2f).crit());
    add(B("Lumina Crash", T::Psychic, K::Beam, 80, 3.2f).low(St::Def, 1));

    // ---------------- Bug
    add(B("Spider Web", T::Bug, K::Zone, 0, 6).low(St::Spd, 1));
    add(B("Tail Glow", T::Bug, K::Buff, 0, 10).buff(St::Atk, 1.8f));
    add(B("Defend Order", T::Bug, K::Buff, 0, 10).buff(St::Def, 1.5f));
    add(B("Heal Order", T::Bug, K::Heal, 0, 14).heal(0.5f));
    add(B("Rage Powder", T::Bug, K::Burst, 0, 7).radius(70).st(S::Confuse, 0.5f));
    add(B("Skitter Smack", T::Bug, K::Melee, 70, 1.6f).low(St::Atk, 1));
    add(B("Pounce", T::Bug, K::Leap, 50, 1.8f).range(110).low(St::Spd, 1));

    // ---------------- Rock
    add(B("Salt Cure", T::Rock, K::Projectile, 40, 1.6f).st(S::Poison, 1.0f));
    add(B("Rock Climb", T::Rock, K::Dash, 90, 2.4f).st(S::Confuse, 0.2f).phys());

    // ---------------- Ghost
    add(B("Curse", T::Ghost, K::Buff, 0, 10).buff(St::Atk, 1.4f).buff2(St::Def).self(St::Spd));
    add(B("Spite", T::Ghost, K::Chain, 0, 6).low(St::Atk, 1).range(160));
    add(B("Grudge", T::Ghost, K::Shield, 0, 9).dur(1.2f).counter());
    add(B("Nightmare", T::Ghost, K::Chain, 60, 3.0f).hex().range(170));
    add(B("Destiny Bond", T::Ghost, K::Shield, 0, 10).dur(1.5f).counter());
    add(B("Shadow Force", T::Ghost, K::Burrow, 120, 5).dur(1.1f));
    add(B("Rage Fist", T::Ghost, K::Melee, 50, 1.2f).pmod(PowerMod::Reversal));
    add(B("Astral Barrage", T::Ghost, K::Spread, 120, 5));

    // ---------------- Dragon
    add(B("Roar of Time", T::Dragon, K::Sweep, 150, 9).windup(0.7f).recharge(1.5f).radius(9).arc(1.6f).dur(0.75f));
    add(B("Dragon Ascent", T::Flying, K::Leap, 120, 4.5f).range(160).self(St::Def));
    add(B("Dynamax Cannon", T::Dragon, K::Sweep, 100, 4).windup(0.5f).arc(1.1f).dur(0.5f));
    add(B("Glaive Rush", T::Dragon, K::Dash, 120, 4).range(130));
    add(B("Order Up", T::Dragon, K::Melee, 80, 1.8f));

    // ---------------- Dark
    add(B("Torment", T::Dark, K::Chain, 0, 7).low(St::Atk, 1).range(160));
    add(B("Flatter", T::Dark, K::Projectile, 0, 7).st(S::Confuse, 1.0f));
    add(B("Memento", T::Dark, K::Burst, 0, 10).radius(80).low(St::Atk, 1).selfko());
    add(B("Taunt", T::Dark, K::Chain, 0, 7).low(St::Atk, 1).range(160));
    add(B("Fake Tears", T::Dark, K::Projectile, 0, 6).low(St::Def, 1));
    add(B("Payback", T::Dark, K::Melee, 50, 1.4f).pmod(PowerMod::Reversal));
    add(B("Assurance", T::Dark, K::Melee, 60, 1.4f).pmod(PowerMod::Brine));
    add(B("Embargo", T::Dark, K::Chain, 0, 7).low(St::Def, 1).range(160));
    add(B("Punishment", T::Dark, K::Melee, 60, 1.6f));
    add(B("Switcheroo", T::Dark, K::Blink, 0, 4).range(120));
    add(B("Dark Void", T::Dark, K::Burst, 0, 9).radius(70).st(S::Sleep, 0.8f));
    add(B("Hone Claws", T::Dark, K::Buff, 0, 9).buff(St::Atk, 1.4f));
    add(B("Brutal Swing", T::Dark, K::Burst, 60, 1.8f).radius(40));
    add(B("Kowtow Cleave", T::Dark, K::Blink, 85, 2.4f));
    add(B("False Surrender", T::Dark, K::Blink, 80, 2.2f));

    // ---------------- Steel
    add(B("Metal Burst", T::Steel, K::Shield, 0, 8).dur(1.4f).counter());
    add(B("Autotomize", T::Steel, K::Buff, 0, 9).buff(St::Spd, 1.7f));
    add(B("Shift Gear", T::Steel, K::Buff, 0, 10).buff(St::Spd, 1.5f).buff2(St::Atk));
    add(B("Double Iron Bash", T::Steel, K::Melee, 60, 2.0f).hits(2).st(S::Flinch, 0.3f));
    add(B("Behemoth Blade", T::Steel, K::Melee, 100, 2.6f).range(36));
    add(B("Behemoth Bash", T::Steel, K::Dash, 100, 2.6f));
    add(B("Spin Out", T::Steel, K::Burst, 100, 3.6f).radius(44).self(St::Spd));
    add(B("Tachyon Cutter", T::Steel, K::Boomerang, 50, 1.4f).range(160));

    // ---------------- Fairy
    add(B("Sweet Scent", T::Fairy, K::Burst, 0, 6).radius(70).low(St::Def, 1));
    add(B("Geomancy", T::Fairy, K::Buff, 0, 14).buff(St::Atk, 1.8f).buff2(St::Spd));
    add(B("Fairy Lock", T::Fairy, K::Vortex, 0, 8).radius(44).pull(80));
    add(B("Aromatic Mist", T::Fairy, K::Buff, 0, 9).buff(St::Def, 1.4f));
    add(B("Decorate", T::Fairy, K::Buff, 0, 10).buff(St::Atk, 1.5f).buff2(St::Def));
    add(B("Play Nice", T::Fairy, K::Burst, 0, 6).radius(70).low(St::Atk, 1));
    // Moves made up for this game (not in the Pokemon games), marked in MOVE-LIST.txt.
    static const char* originals[] = {"Bubble Trap", "Shock Mine", "Rock Ricochet", "Rock Orbit", "Dragon Orbit", "Shadow Shuriken", "Curse Trap", "Ghost Vortex", "Psychic Trap", "Snare", "Silk Barrier", "Fairy Ring", "Mirror Ricochet"};
    for (auto& m : v)
        for (const char* o : originals)
            if (m.name == o) m.original = true;
    auto like = [&](const char* n, const char* base, Type t) {
        for (size_t i = 0; i < v.size(); i++)
            if (v[i].name == base) { MoveDef c = v[i]; c.name = n; c.type = t; c.original = false; v.push_back(c); return; }
    };
#include "moves_more.inc"
    // Who these moves affect in a double battle, as in the main games.
    struct Sc { const char* name; uint8_t scope; };
    static const Sc kScopes[] = {
        {"Howl", 1}, {"Tailwind", 1}, {"Life Dew", 1}, {"Jungle Healing", 1}, {"Lunar Blessing", 1}, {"Aromatherapy", 1},
        {"Heal Bell", 1}, {"Safeguard", 1}, {"Mist", 1}, {"Lucky Chant", 1}, {"Quick Guard", 1},
        {"Helping Hand", 2}, {"Coaching", 2}, {"Aromatic Mist", 2}, {"Decorate", 2}, {"Hold Hands", 2}, {"Dragon Cheer", 2},
        {"Heal Pulse", 2}, {"Floral Healing", 2},
        {"Flower Shield", 3}, {"Rototiller", 3},
        {"Follow Me", 4}, {"Rage Powder", 4}, {"Spotlight", 4},
    };
    for (auto& m : v)
        for (const auto& sc : kScopes)
            if (m.name == sc.name) m.scope = sc.scope;
    for (auto& m : v)   // Rage Powder draws attacks like Follow Me (it doesn't confuse)
        if (m.name == "Rage Powder") { m.kind = MoveKind::Shield; m.duration = 1.2f; m.radius = 0; m.status = Status::None; m.statusChance = 0; }
    return v;
}

} // namespace

const std::vector<MoveDef>& MoveLibrary() {
    static const std::vector<MoveDef> lib = Build();
    return lib;
}

const MoveDef* FindMove(const std::string& name) {
    std::string key = Squash(name);
    // Older / alternate spellings from earlier games.
    static const char* alias[][2] = {{"hijumpkick", "High Jump Kick"}, {"vicegrip", "Vise Grip"}, {"vicegrip", "Vise Grip"},
                                     {"faintattack", "Feint Attack"}, {"smellingsalt", "Smelling Salts"},
                                     {"hijump", "High Jump Kick"}, {"sonicboom", "Sonic Boom"}, {"doubleslap", "Double Slap"}};
    for (auto& a : alias)
        if (key == a[0]) key = Squash(a[1]);
    for (const auto& m : MoveLibrary())
        if (Squash(m.name) == key) return &m;
    return nullptr;
}

MoveDef MakeMove(const std::string& name, Type t, MoveKind k, int power, float cd) {
    B b("", t, k, power, cd);
    b.m.name = name;
    return b.m;
}

const char* MoveKindName(MoveKind k) {
    switch (k) {
        case MoveKind::Projectile: return "Shot";
        case MoveKind::Spread:     return "Spread";
        case MoveKind::Melee:      return "Melee";
        case MoveKind::Dash:       return "Dash";
        case MoveKind::Burst:      return "Burst";
        case MoveKind::Beam:       return "Beam";
        case MoveKind::Lob:        return "Lob";
        case MoveKind::Zone:       return "Zone";
        case MoveKind::Buff:       return "Buff";
        case MoveKind::Heal:       return "Heal";
        case MoveKind::Chain:      return "Strike";
        case MoveKind::Wave:       return "Wave";
        case MoveKind::Rain:       return "Rain";
        case MoveKind::Blink:      return "Blink";
        case MoveKind::Shield:     return "Shield";
        case MoveKind::Burrow:     return "Burrow";
        case MoveKind::Boomerang:  return "Boomerang";
        case MoveKind::Orbit:      return "Orbit";
        case MoveKind::Wall:       return "Wall";
        case MoveKind::Trap:       return "Trap";
        case MoveKind::Vortex:     return "Vortex";
        case MoveKind::Cone:       return "Cone";
        case MoveKind::Leap:       return "Leap";
        case MoveKind::Weather:    return "Weather";
        case MoveKind::Sweep:      return "Sweep";
        case MoveKind::Tether:     return "Tether";
        case MoveKind::Grapple:    return "Grapple";
        case MoveKind::Throw:      return "Throw";
        case MoveKind::Roll:       return "Roll";
        default: return "";
    }
}

bool ParseMoveKind(const std::string& s, MoveKind& out) {
    std::string k = Squash(s);
    for (int i = 0; i < (int)MoveKind::Count; i++)
        if (Squash(MoveKindName((MoveKind)i)) == k) { out = (MoveKind)i; return true; }
    if (k == "projectile" || k == "shoot") { out = MoveKind::Projectile; return true; }
    if (k == "chain" || k == "lightning") { out = MoveKind::Chain; return true; }
    if (k == "teleport") { out = MoveKind::Blink; return true; }
    if (k == "protect") { out = MoveKind::Shield; return true; }
    if (k == "dig" || k == "fly") { out = MoveKind::Burrow; return true; }
    if (k == "jump" || k == "bounce") { out = MoveKind::Leap; return true; }
    if (k == "breath" || k == "spray") { out = MoveKind::Cone; return true; }
    if (k == "barrier") { out = MoveKind::Wall; return true; }
    if (k == "mine") { out = MoveKind::Trap; return true; }
    if (k == "whirl" || k == "pull") { out = MoveKind::Vortex; return true; }
    if (k == "sweepingbeam" || k == "sweepbeam") { out = MoveKind::Sweep; return true; }
    if (k == "drain" || k == "leash" || k == "link") { out = MoveKind::Tether; return true; }
    if (k == "hook" || k == "whip" || k == "lasso") { out = MoveKind::Grapple; return true; }
    if (k == "grab" || k == "toss") { out = MoveKind::Throw; return true; }
    if (k == "roll" || k == "spin" || k == "rollout") { out = MoveKind::Roll; return true; }
    return false;
}

const char* StatusName(Status s) {
    switch (s) {
        case Status::Burn: return "burn";
        case Status::Poison: return "poison";
        case Status::Toxic: return "bad poison";
        case Status::Paralyze: return "paralysis";
        case Status::Freeze: return "freeze";
        case Status::Sleep: return "sleep";
        case Status::Confuse: return "confusion";
        case Status::Flinch: return "flinch";
        case Status::Seed: return "leech seed";
        default: return "";
    }
}

const char* StatusShort(Status s) {
    switch (s) {
        case Status::Burn: return "BRN";
        case Status::Poison: return "PSN";
        case Status::Toxic: return "TOX";
        case Status::Paralyze: return "PAR";
        case Status::Freeze: return "FRZ";
        case Status::Sleep: return "SLP";
        case Status::Confuse: return "CNF";
        case Status::Flinch: return "";
        case Status::Seed: return "SEED";
        default: return "";
    }
}

bool IsMajorStatus(Status s) { return s >= Status::Burn && s <= Status::Sleep; }

bool ParseStatus(const std::string& s, Status& out) {
    std::string k = Squash(s);
    struct P { const char* n; Status s; };
    const P all[] = {{"burn", Status::Burn}, {"poison", Status::Poison}, {"toxic", Status::Toxic},
                     {"badpoison", Status::Toxic}, {"paralyze", Status::Paralyze}, {"paralysis", Status::Paralyze},
                     {"freeze", Status::Freeze}, {"sleep", Status::Sleep}, {"confuse", Status::Confuse},
                     {"confusion", Status::Confuse}, {"flinch", Status::Flinch}, {"seed", Status::Seed},
                     {"leechseed", Status::Seed}};
    for (const auto& p : all)
        if (k == p.n) { out = p.s; return true; }
    return false;
}

bool ParseType(const std::string& s, Type& out) {
    std::string k = Squash(s);
    for (int i = 0; i < TYPE_COUNT; i++)
        if (Squash(TypeName((Type)i)) == k) { out = (Type)i; return true; }
    if (k == "none" || k.empty()) { out = Type::None; return true; }
    return false;
}

bool IsSpecial(const MoveDef& m) {
    if (m.cat != MoveCat::Auto) return m.cat == MoveCat::Special;
    switch (m.kind) {
        case MoveKind::Projectile: case MoveKind::Spread: case MoveKind::Beam: case MoveKind::Lob:
        case MoveKind::Zone: case MoveKind::Chain: case MoveKind::Rain:
        case MoveKind::Boomerang: case MoveKind::Trap: case MoveKind::Vortex: case MoveKind::Cone:
        case MoveKind::Sweep: case MoveKind::Tether:
            return true;
        default:  // Melee, Dash, Burst, Wave, Blink, Burrow: up close
            return false;
    }
}

bool IsDamaging(const MoveDef& m) {
    return m.kind != MoveKind::Buff && m.kind != MoveKind::Heal && m.kind != MoveKind::Shield && m.power > 0;
}

std::string DescribeMove(const MoveDef& m) {
    std::string d;
    switch (m.kind) {
        case MoveKind::Projectile: d = m.homing > 4 ? "Homing shot" : (m.homing > 0 ? "Curving shot" : "Shot"); break;
        case MoveKind::Spread: d = "Three shots in a fan"; break;
        case MoveKind::Melee: d = m.range > 30 ? "Long-reach strike" : "Close strike"; break;
        case MoveKind::Dash: d = m.quick ? "Lightning-fast dash" : "Charging dash"; break;
        case MoveKind::Burst: d = "Burst around you"; break;
        case MoveKind::Beam: d = "Charged beam"; break;
        case MoveKind::Lob: d = "Arcing blast"; break;
        case MoveKind::Zone: d = "Damaging area at the target"; break;
        case MoveKind::Buff: d = TextFormat("Raises your %s", m.stat == StatId::Atk ? "attack" : (m.stat == StatId::Def ? "defense" : "speed")); break;
        case MoveKind::Heal: d = TextFormat("Restores %d%% HP", (int)(m.amount * 100 + 0.5f)); break;
        case MoveKind::Chain: d = m.type == Type::Electric ? "Lightning leaps to the target" : "Instant strike on the target"; break;
        case MoveKind::Wave: d = "Shockwave spreads from you"; break;
        case MoveKind::Rain: d = m.count > 1 ? "Rains down around the target" : "Strikes from above"; break;
        case MoveKind::Blink: d = m.power > 0 ? "Teleports to the target and strikes" : "Teleports away"; break;
        case MoveKind::Shield: d = "Blocks all attacks briefly"; break;
        case MoveKind::Burrow: d = "Vanishes, then erupts at the target"; break;
        case MoveKind::Boomerang: d = "Flies out and back, can hit twice"; break;
        case MoveKind::Orbit: d = TextFormat("%d orbs circle you, hitting on touch", m.count); break;
        case MoveKind::Wall: d = "Raises a barrier that blocks shots"; break;
        case MoveKind::Trap: d = "Sets a trap that springs when stepped on"; break;
        case MoveKind::Vortex: d = "Swirl that drags the foe in"; break;
        case MoveKind::Cone: d = "Sprays a cone in front of you"; break;
        case MoveKind::Leap: d = "Leaps up (can't be hit), slams down on the target"; break;
        case MoveKind::Sweep: d = "Beam that sweeps across the field (jump or take cover)"; break;
        case MoveKind::Tether:
            d = m.tether == 3 ? "Latches on: the foe can't get away or switch out"
              : m.tether == 2 ? "Latches on and binds the foe, hurting it while it holds"
              : m.sleepOnly ? "Latches onto a sleeping foe and drains it"
              : "Latches on and drains the foe while it stays in range";
            break;
        case MoveKind::Grapple:
            d = m.amount < 0.99f ? "Lashes out and tugs the foe toward you" : "Hooks the foe and yanks it to you";
            if (m.antiAir) d = "Knocks a jumping foe out of the air and pulls it in";
            break;
        case MoveKind::Roll:
            d = TextFormat("Rolls and ricochets off walls, rocks and foes (%d bounces)", m.bounces);
            if (m.amount >= 0.25f) d += ", harder every bounce";
            if (m.clears) d += ", sweeps away hazards";
            if (m.trail) d += ", leaves a trail of fire";
            break;
        case MoveKind::Throw: d = m.throwKind == 1 ? "Grabs the foe, carries it up and drops it" : "Grabs the foe and hurls it (harder into walls and rocks)"; break;
        case MoveKind::Weather:
            d = m.weather == Weather::Rain ? "Rain: Water x1.5, Fire x0.5, puddles form"
              : m.weather == Weather::Sun ? "Harsh sun: Fire x1.5, Water x0.5, no freezing"
              : m.weather == Weather::Sand ? "Sandstorm: hurts all but Rock, Ground, Steel"
              : "Snow: Ice types' Defense x1.5, snow mounds pile up";
            break;
        default: break;
    }
    if (m.hits > 1) d += TextFormat(", %d hits", m.hits);
    if (m.bounces > 0 && m.kind != MoveKind::Roll) d += TextFormat(", ricochets %d time%s", m.bounces, m.bounces > 1 ? "s" : "");
    if (m.metronome) d = "Uses a random move";
    if (m.copycat) d = "Copies the foe's last move";
    if (m.kind == MoveKind::Buff && m.amount <= 1.0f && !m.haze) d = "But nothing happened!";
    if (m.kind == MoveKind::Buff && m.stat2 >= 0)
        d += TextFormat(" and %s", m.stat2 == 0 ? "attack" : (m.stat2 == 1 ? "defense" : "speed"));
    if (m.counter) d = "Blocks hits and pays them back x1.5";
    if (m.endure) d = "Hangs on at 1 HP for a moment";
    if (m.fixedDmg > 0) d += TextFormat(", always %d damage", (int)(m.fixedDmg * 2.5f));
    if (m.fixedDmg == -1) d += ", damage = your level";
    if (m.fixedDmg == -2) d += ", halves the foe's HP";
    if (m.ohko) d += ", 30% one-hit KO";
    if (m.selfKO) d += ", you faint";
    if (m.forceSwap) d += ", forces a switch";
    if (m.pin > 0) d += ", stops switching";
    if (m.falseSwipe) d += ", never knocks out";
    if (m.haze) d = "Clears all stat changes";
    if (m.cure) d += ", cures your status";
    if (m.payDay) d += ", scatters coins";
    switch (m.powerMod) {
        case PowerMod::Facade: d += ", x2 if you have a status"; break;
        case PowerMod::Reversal: d += ", stronger at low HP"; break;
        case PowerMod::Eruption: d += ", weaker at low HP"; break;
        case PowerMod::Brine: d += ", x2 vs a foe under half HP"; break;
        case PowerMod::Acrobatics: d += ", x2 with no held item"; break;
        case PowerMod::WeatherBall: d += ", changes type with the weather"; break;
        default: break;
    }
    if (m.patchPower > 0) d += m.type == Type::Fire ? ", leaves a burning spot" : ", leaves a toxic puddle";
    if (m.status != Status::None) {
        if (m.kind == MoveKind::Heal) d += ", then you sleep";
        else if (m.statusChance >= 0.99f) d += TextFormat(", causes %s", StatusName(m.status));
        else d += TextFormat(", %d%% %s", (int)(m.statusChance * 100 + 0.5f), StatusName(m.status));
    }
    if (m.lowers) {
        const char* s = m.lowerStat == StatId::Atk ? "attack" : (m.lowerStat == StatId::Def ? "defense" : "speed");
        if (m.lowerChance >= 0.99f) d += TextFormat(", lowers %s", s);
        else d += TextFormat(", %d%% lower %s", (int)(m.lowerChance * 100 + 0.5f), s);
    }
    if (m.drain > 0) d += ", drains HP";
    if (m.recoil > 0) d += ", recoil";
    if (m.highCrit) d += ", high crit";
    if (m.hex) d += ", x2 vs status";
    if (m.knock >= 2) d += ", big knockback";
    if (m.recharge > 0) d += ", must recharge";
    if (m.selfDrop) d += TextFormat(", lowers your %s", m.selfStat == StatId::Atk ? "attack" : (m.selfStat == StatId::Def ? "defense" : "speed"));
    if (m.maxUses > 0) d += TextFormat(", %d use%s", m.maxUses, m.maxUses > 1 ? "s" : "");
    if (IsDamaging(m)) d += IsSpecial(m) ? " (special)" : " (physical)";
    return d;
}
