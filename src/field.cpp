#include "rng.h"
#include "field.h"
#include <cctype>
#include <cmath>
#include "arena.h"

namespace {

const char* kNames[ARENA_COUNT] = {"Stadium", "Stone Gym", "Cascade Gym", "Power Plant Gym", "Greenhouse Gym",
                                   "Swamp Gym", "Trick Room Gym", "Volcano Gym", "Hideout Gym",
                                   "Frozen Chamber", "Brawler's Ring", "Haunted Hall", "Dragon's Den", "Champion's Stadium",
                                   "Seafoam Depths", "Power Plant", "Moltres's Nest", "Cerulean Cave", "Faraway Island"};
const char* kKeys[ARENA_COUNT] = {"stadium", "rock", "water", "electric", "grass", "poison", "psychic", "fire", "ground",
                                  "ice", "fighting", "ghost", "dragon", "champion",
                                  "seafoam", "powerplant", "ember", "unknown", "faraway"};

float Rf(float a, float b) { return a + (b - a) * (float)SimRand(0, 10000) / 10000.0f; }

const Vector2 kSpawns[2] = {{120, ARENA_H / 2}, {ARENA_W - 120, ARENA_H / 2}};

float Dist2(Vector2 a, Vector2 b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }

// Random point at least 'clear' from both spawns and 'spacing' from the given points.
bool Place(Vector2& out, const std::vector<Vector2>& taken, float spacing, float clear, float xmin, float xmax,
           float ymin, float ymax) {
    for (int tries = 0; tries < 200; tries++) {
        Vector2 p{Rf(xmin, xmax), Rf(ymin, ymax)};
        bool ok = Dist2(p, kSpawns[0]) > clear && Dist2(p, kSpawns[1]) > clear;
        for (const auto& t : taken) ok = ok && Dist2(p, t) > spacing;
        if (ok) { out = p; return true; }
    }
    return false;
}

void AddObstacles(Field& f, Obstacle::Kind kind, int n, float radius, float height, float spacing, float clear,
                  float xmin = 60, float xmax = ARENA_W - 60, float ymin = 50, float ymax = ARENA_H - 50) {
    std::vector<Vector2> taken;
    for (const auto& o : f.obs) taken.push_back(o.pos);
    for (int i = 0; i < n; i++) {
        Vector2 p;
        if (!Place(p, taken, spacing, clear, xmin, xmax, ymin, ymax)) continue;
        Obstacle o;
        o.kind = kind;
        o.pos = p;
        o.radius = radius;
        o.height = height;
        o.seed = (uint32_t)SimRand(1, 0x3fffffff);
        if (kind == Obstacle::Crate) o.hp = o.maxHp = 120;
        f.obs.push_back(o);
        taken.push_back(p);
    }
}

void AddZones(Field& f, Zone::Kind kind, int n, float rxMin, float rxMax, float clear, float spacing) {
    std::vector<Vector2> taken;
    for (const auto& z : f.zones) taken.push_back(z.pos);
    for (const auto& o : f.obs) taken.push_back(o.pos);
    for (int i = 0; i < n; i++) {
        Vector2 p;
        if (!Place(p, taken, spacing, clear, 70, ARENA_W - 70, 60, ARENA_H - 60)) continue;
        Zone z;
        z.kind = kind;
        z.pos = p;
        z.rx = Rf(rxMin, rxMax);
        z.ry = z.rx * Rf(0.7f, 0.95f);
        z.seed = (uint32_t)SimRand(1, 0x3fffffff);
        f.zones.push_back(z);
        taken.push_back(p);
    }
}

} // namespace

const char* ArenaName(ArenaKind k) { return kNames[(int)k]; }

const char* ArenaTwist(ArenaKind k) {
    switch (k) {
        case ArenaKind::Rock: return "Stone pillars block shots. Heavy Pokemon can smash them and hurl them across the field.";
        case ArenaKind::Water: return "Mostly water: Water types speed up, others slow down, flyers hover over it. Electric hits shock the whole pool!";
        case ArenaKind::Electric: return "Tesla coils zap anyone close by - but Electric types near a coil get stronger and faster.";
        case ArenaKind::Grass: return "Trees block shots, heavy hits knock them down, Fire burns them. Grass types power up in the tall grass.";
        case ArenaKind::Poison: return "Poison pools hurt anything but Poison types, and toxic spikes shrink the field.";
        case ArenaKind::Psychic: return "Four portals teleport anything that enters them - and they keep moving.";
        case ArenaKind::Fire: return "Lava pools burn non-Fire types and spit fireballs. Rocks give a little cover.";
        case ArenaKind::Ground: return "Steel pillars give cover. Every 10 seconds the wall cannons fire - watch for the red lines!";
        case ArenaKind::Ice: return "Endless snow. Ice rinks send you sliding (Ice types skate faster). Ice pillars melt under Fire and grow back.";
        case ArenaKind::Fighting: return "A boxing ring: knocked into the ropes, you bounce back in! Fighting types hit harder in the center ring.";
        case ArenaKind::Ghost: return "Lights out. Will-o'-wisps drift toward anything that isn't a Ghost and can burn it. Ghost types move faster in the dark.";
        case ArenaKind::Dragon: return "Dragon statues breathe fire across the den - watch the warning glow! Dragon types are stronger here.";
        case ArenaKind::Champion: return "The crowd's spotlight roams the floor: whoever stands in it hits harder and moves faster. Fight for the spotlight!";
        case ArenaKind::Seafoam: return "Endless snow and frozen currents: rinks send you sliding, and icicles crash down from the cave roof where the markers glow.";
        case ArenaKind::PowerPlant: return "A thunderstorm inside the plant: tesla coils power up Electric types, puddles conduct, and lightning strikes where the markers glow.";
        case ArenaKind::Ember: return "Moltres's nest: lava pools spit fire, the sun blazes, and burning rocks rain down where the markers glow.";
        case ArenaKind::Unknown: return "Mewtwo's cave: deep pools slow you, it hurls boulders with its mind, and its psychic pulse ripples out every few seconds.";
        case ArenaKind::Faraway: return "Mew's island: it hides and heals in the tall grass, and teleports around the jungle when you corner it.";
        default: return "A classic stadium. No tricks.";
    }
}
const char* ArenaKey(ArenaKind k) { return kKeys[(int)k]; }

bool ParseArena(const std::string& s, ArenaKind& out) {
    std::string l;
    for (char c : s) if (std::isalnum((unsigned char)c)) l += (char)std::tolower((unsigned char)c);
    for (int i = 0; i < ARENA_COUNT; i++) {
        std::string n;
        for (const char* c = kNames[i]; *c; c++) if (std::isalnum((unsigned char)*c)) n += (char)std::tolower((unsigned char)*c);
        if (l == kKeys[i] || l == n || l == std::string(kKeys[i]) + "gym") { out = (ArenaKind)i; return true; }
    }
    if (l == "default" || l == "none" || l.empty()) { out = ArenaKind::Stadium; return true; }
    return false;
}

namespace {
// Lorelei's / Articuno's frozen rinks (pool = true: Mewtwo's deep pools, water that stays water).
void AddRinks(Field& f, int rinks, bool pool) {
    for (int i = 0; i < rinks; i++) {
        for (int tries = 0; tries < 40; tries++) {
            Vector2 p{Rf(185, ARENA_W - 185), Rf(50, ARENA_H - 50)};
            if (i == 0) p = {ARENA_W / 2 + Rf(-30, 30), ARENA_H / 2 + Rf(-30, 30)};
            float r = i == 0 ? Rf(46, 54) : Rf(30, 40);
            bool ok = true;
            for (const Puddle& o : f.puddles) if (Dist2(o.pos, p) < o.maxR + r + 16) ok = false;
            for (const Obstacle& o : f.obs) if (Dist2(o.pos, p) < o.radius + r + 6) ok = false;
            if (!ok) continue;
            Puddle pd;
            pd.pos = p;
            pd.r = pd.maxR = r;
            pd.ice = pool ? 0 : 2;
            pd.rink = true;
            pd.thawT = pool ? 1e9f : 0;
            pd.seed = (uint32_t)SimRand(1, 0x3fffffff);
            f.puddles.push_back(pd);
            break;
        }
    }
}
} // namespace

void Field::Build(ArenaKind k, Venue v, int seasonIn) {
    *this = Field();
    kind = k;
    venue = k == ArenaKind::Stadium ? v : Venue::Stadium;
    season = seasonIn;
    if (k == ArenaKind::Stadium) {   // a venue's props (a few, so every place plays a little differently)
        int r = SimRand(0, 99);
        switch (venue) {
            case Venue::Forest: AddObstacles(*this, Obstacle::Tree, 3 + r % 3, 10, 36, 90, 70); break;
            case Venue::Park: AddObstacles(*this, Obstacle::Tree, 2, 10, 34, 140, 80); break;
            case Venue::Route: if (r < 50) AddObstacles(*this, Obstacle::Tree, 1 + r % 2, 10, 34, 160, 90); break;
            case Venue::Cave: case Venue::IceCave: AddObstacles(*this, Obstacle::Pillar, 3 + r % 2, 11, 26, 110, 75); break;
            case Venue::Mountain: AddObstacles(*this, Obstacle::Pillar, 2 + r % 2, 12, 30, 130, 80); break;
            case Venue::Tower: AddObstacles(*this, Obstacle::Pillar, 3, 9, 20, 120, 80); break;
            case Venue::Mansion: AddObstacles(*this, Obstacle::Pillar, 2, 10, 28, 150, 90); break;
            case Venue::Volcano: case Venue::Beach: AddObstacles(*this, Obstacle::Boulder, 2 + r % 2, 9, 12, 120, 70); break;
            case Venue::Ship: case Venue::Hideout: AddObstacles(*this, Obstacle::Crate, 3, 11, 18, 110, 75); break;
            case Venue::City: case Venue::Office: AddObstacles(*this, Obstacle::Crate, 1 + r % 2, 11, 18, 140, 85); break;
            default: break;
        }
    }
    switch (k) {
        case ArenaKind::Rock:
            AddObstacles(*this, Obstacle::Pillar, 7, 12, 30, 70, 70, 150, ARENA_W - 150);
            break;
        case ArenaKind::Water: {
            for (int s = 0; s < 2; s++) {  // landing platforms under each team's start
                Zone z;
                z.kind = Zone::Platform;
                z.pos = kSpawns[s];
                z.rx = 48;
                z.ry = 44;
                z.seed = (uint32_t)SimRand(1, 0x3fffffff);
                zones.push_back(z);
            }
            Zone mid;
            mid.kind = Zone::Platform;
            mid.pos = {ARENA_W / 2, ARENA_H / 2};
            mid.rx = 40;
            mid.ry = 34;
            mid.seed = (uint32_t)SimRand(1, 0x3fffffff);
            zones.push_back(mid);
            AddZones(*this, Zone::Platform, 5, 22, 34, 90, 95);
        } break;
        case ArenaKind::Electric:
            AddObstacles(*this, Obstacle::Coil, 4, 10, 34, 140, 110, 90, ARENA_W - 90, 70, ARENA_H - 70);
            break;
        case ArenaKind::Grass:
            AddZones(*this, Zone::TallGrass, 7, 45, 75, 40, 80);
            AddObstacles(*this, Obstacle::Tree, 10, 10, 36, 58, 65);
            break;
        case ArenaKind::Poison:
            spikeBand = 24;
            AddZones(*this, Zone::PoisonPool, 6, 26, 46, 80, 95);
            break;
        case ArenaKind::Psychic:
            PlacePortals();
            break;
        case ArenaKind::Fire:
            AddZones(*this, Zone::LavaPool, 5, 22, 36, 80, 100);
            AddObstacles(*this, Obstacle::Boulder, 6, 8, 12, 60, 60);
            lavaT = 3.0f;
            break;
        case ArenaKind::Ground: {
            AddObstacles(*this, Obstacle::MetalPillar, 4, 12, 32, 120, 90, 160, ARENA_W - 160, 90, ARENA_H - 90);
            const float ys[3] = {ARENA_H * 0.25f, ARENA_H * 0.5f, ARENA_H * 0.75f};
            for (float y : ys) {
                Turret l;
                l.pos = {0, y};
                l.dir = {1, 0};
                turrets.push_back(l);
                Turret r;
                r.pos = {ARENA_W, y};
                r.dir = {-1, 0};
                turrets.push_back(r);
            }
            const float xs[2] = {ARENA_W * 0.33f, ARENA_W * 0.67f};
            for (float x : xs) {
                Turret b;
                b.pos = {x, 0};
                b.dir = {0, 1};
                turrets.push_back(b);
            }
            turretT = 10.0f;
        } break;
        case ArenaKind::Seafoam:
            AddObstacles(*this, Obstacle::Pillar, 4, 11, 28, 100, 80, 150, ARENA_W - 150);
            AddRinks(*this, 4, false);
            strikeT = 4.0f;
            break;
        case ArenaKind::PowerPlant:
            AddObstacles(*this, Obstacle::Coil, 4, 10, 34, 140, 110, 90, ARENA_W - 90, 70, ARENA_H - 70);
            AddObstacles(*this, Obstacle::Crate, 2, 11, 18, 120, 90);
            strikeT = 4.0f;
            break;
        case ArenaKind::Ember:
            AddZones(*this, Zone::LavaPool, 6, 22, 38, 80, 100);
            AddObstacles(*this, Obstacle::Boulder, 4, 8, 12, 60, 60);
            lavaT = 3.0f;
            strikeT = 4.0f;
            break;
        case ArenaKind::Unknown:
            AddObstacles(*this, Obstacle::Pillar, 4, 11, 30, 100, 85, 150, ARENA_W - 150);
            AddRinks(*this, 3, true);
            strikeT = 3.5f;
            pulseT = 8.0f;
            break;
        case ArenaKind::Faraway:
            AddZones(*this, Zone::TallGrass, 6, 45, 75, 40, 80);
            AddObstacles(*this, Obstacle::Tree, 5, 10, 36, 58, 65);
            blinkT = 9.0f;
            break;
        case ArenaKind::Ice: {
            AddObstacles(*this, Obstacle::Pillar, 5, 11, 28, 90, 80, 150, ARENA_W - 150);
            int rinks = 4;
            for (int i = 0; i < rinks; i++) {   // big frozen rinks, away from where the teams start
                for (int tries = 0; tries < 40; tries++) {
                    Vector2 p{Rf(185, ARENA_W - 185), Rf(50, ARENA_H - 50)};
                    if (i == 0) p = {ARENA_W / 2 + Rf(-30, 30), ARENA_H / 2 + Rf(-30, 30)};
                    float r = i == 0 ? Rf(46, 54) : Rf(30, 40);
                    bool ok = true;
                    for (const Puddle& o : puddles) if (Dist2(o.pos, p) < o.maxR + r + 16) ok = false;
                    for (const Obstacle& o : obs) if (Dist2(o.pos, p) < o.radius + r + 6) ok = false;
                    if (!ok) continue;
                    Puddle pd;
                    pd.pos = p;
                    pd.r = pd.maxR = r;
                    pd.ice = 2;
                    pd.rink = true;
                    pd.seed = (uint32_t)SimRand(1, 0x3fffffff);
                    puddles.push_back(pd);
                    break;
                }
            }
        } break;
        case ArenaKind::Fighting: {   // four training boulders, one in each quarter of the ring
            const Vector2 q[4] = {{ARENA_W * 0.3f, ARENA_H * 0.25f}, {ARENA_W * 0.7f, ARENA_H * 0.25f}, {ARENA_W * 0.3f, ARENA_H * 0.75f}, {ARENA_W * 0.7f, ARENA_H * 0.75f}};
            for (int i = 0; i < 4; i++) {
                Obstacle o;
                o.kind = Obstacle::Pillar;
                o.pos = {q[i].x + Rf(-20, 20), q[i].y + Rf(-12, 12)};
                o.radius = 12;
                o.height = 26;
                o.seed = (uint32_t)SimRand(1, 0x3fffffff);
                obs.push_back(o);
            }
        } break;
        case ArenaKind::Ghost:
            AddObstacles(*this, Obstacle::Pillar, 6, 9, 22, 95, 80, 130, ARENA_W - 130);   // gravestones
            for (int i = 0; i < 3; i++) {
                Wisp w;
                w.seed = (uint32_t)SimRand(1, 0x3fffffff);
                w.respawnT = 2.0f + i * 2.5f;
                wisps.push_back(w);
            }
            break;
        case ArenaKind::Dragon: {
            AddObstacles(*this, Obstacle::Pillar, 4, 11, 30, 130, 95, 170, ARENA_W - 170, 70, ARENA_H - 70);
            const float xs[3] = {ARENA_W * 0.25f, ARENA_W * 0.5f, ARENA_W * 0.75f};   // dragon statues on the back wall
            for (float x : xs) {
                Turret t;
                t.pos = {x, 0};
                t.dir = {0, 1};
                turrets.push_back(t);
            }
            for (int k = 0; k < 2; k++) {   // and one on each side, low down
                Turret t;
                t.pos = {k ? ARENA_W : 0.0f, ARENA_H * 0.78f};
                t.dir = {k ? -1.0f : 1.0f, 0};
                turrets.push_back(t);
            }
            turretT = 6.0f;
        } break;
        case ArenaKind::Champion:
            spot = spotTo = {ARENA_W / 2, ARENA_H / 2};
            spotT = 5.0f;
            break;
        default: break;
    }
}

const char* VenueName(Venue v) {
    static const char* n[VENUE_COUNT] = {"Stadium", "Route", "Forest", "Cave", "Ice Cave", "Beach", "City", "Bridge", "Ship",
                                         "Park", "Dojo", "Office", "Tower", "Mansion", "Volcano", "Hideout", "Lab", "Mountain"};
    int i = (int)v;
    return i >= 0 && i < VENUE_COUNT ? n[i] : "Stadium";
}

bool VenueIndoors(Venue v) {
    switch (v) {
        case Venue::Cave: case Venue::IceCave: case Venue::Dojo: case Venue::Office: case Venue::Tower:
        case Venue::Mansion: case Venue::Hideout: case Venue::Lab: case Venue::Stadium: return true;
        default: return false;
    }
}

Venue VenueForPlace(const std::string& place) {
    std::string s;
    for (char c : place) s += (char)std::tolower((unsigned char)c);
    auto has = [&](const char* k) { return s.find(k) != std::string::npos; };
    if (has("seafoam") || has("ice")) return Venue::IceCave;
    if (has("cave") || has("mt.") || has("mt ") || has("tunnel") || has("victory road")) return Venue::Cave;
    if (has("forest") || has("woods")) return Venue::Forest;
    if (has("bridge") || has("cycling")) return Venue::Bridge;
    if (has("s.s.") || has("ship") || has("anne")) return Venue::Ship;
    if (has("park") || has("safari") || has("garden")) return Venue::Park;
    if (has("dojo")) return Venue::Dojo;
    if (has("silph") || has("office")) return Venue::Office;
    if (has("tower")) return Venue::Tower;
    if (has("mansion")) return Venue::Mansion;
    if (has("volcano")) return Venue::Volcano;
    if (has("hideout") || has("rocket") || has("plant")) return Venue::Hideout;
    if (has("lab")) return Venue::Lab;
    if (has("beach") || has("island") || has("sea") || has("shore")) return Venue::Beach;
    if (has("outskirts") || has("mountain") || has("plateau")) return Venue::Mountain;
    if (has("city") || has("town") || has("street") || has("gate")) return Venue::City;
    return Venue::Route;
}

Venue VenueByName(const std::string& name) {
    std::string a;
    for (char c : name) if (c != ' ' && c != '_') a += (char)std::tolower((unsigned char)c);
    for (int i = 0; i < VENUE_COUNT; i++) {
        std::string b;
        for (const char* p = VenueName((Venue)i); *p; p++) if (*p != ' ') b += (char)std::tolower((unsigned char)*p);
        if (a == b) return (Venue)i;
    }
    return VenueForPlace(name);
}

Venue RandomWildVenue(int roll) {
    static const Venue v[] = {Venue::Route, Venue::Route, Venue::Forest, Venue::Park, Venue::Beach, Venue::Mountain, Venue::Cave, Venue::Bridge};
    int n = (int)(sizeof v / sizeof v[0]);
    return v[((roll % n) + n) % n];
}

void Field::PlacePortals() {
    portals.clear();
    std::vector<Vector2> taken;
    for (int i = 0; i < 4; i++) {
        Vector2 p;
        if (!Place(p, taken, 130, 50, 60, ARENA_W - 60, 50, ARENA_H - 50)) p = {Rf(80, ARENA_W - 80), Rf(60, ARENA_H - 60)};
        Portal po;
        po.pos = p;
        portals.push_back(po);
        taken.push_back(p);
    }
    portalT = 12.0f;
}

bool Field::IsWater(Vector2 p) const {
    if (kind != ArenaKind::Water) return false;
    for (const auto& z : zones)
        if (z.kind == Zone::Platform && z.Contains(p)) return false;
    return true;
}

bool Field::InZone(Vector2 p, Zone::Kind k, float pad) const {
    for (const auto& z : zones)
        if (z.kind == k && z.Contains(p, pad)) return true;
    return false;
}

bool Field::InSpikes(Vector2 p) const {
    if (spikeBand <= 0) return false;
    return p.x < spikeBand || p.x > ARENA_W - spikeBand || p.y < spikeBand || p.y > ARENA_H - spikeBand * 0.7f;
}
