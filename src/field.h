#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "raylib.h"
#include "types.h"

// Arenas. Stadium is the plain one; the rest are the eight gym stadiums.
enum class ArenaKind { Stadium, Rock, Water, Electric, Grass, Poison, Psychic, Fire, Ground,
                       Ice, Fighting, Ghost, Dragon, Champion,               // the Pokemon League
                       Seafoam, PowerPlant, Ember, Unknown, Faraway,         // legendary encounters
                       Flying, Bug, Normal, Steel, Dark, JohtoChampion,      // Johto (Falkner, Bugsy, Whitney, Jasmine, Karen, Lance)
                       Count };
// A legendary's lair plays by an existing arena's rules (plus its own extras): Seafoam = Ice,
// Power Plant = Electric, Moltres's nest = Fire, Faraway Island = Grass. Cerulean Cave is its own.
inline ArenaKind ArenaRules(ArenaKind k) {
    switch (k) {
        case ArenaKind::Seafoam: return ArenaKind::Ice;
        case ArenaKind::PowerPlant: return ArenaKind::Electric;
        case ArenaKind::Ember: return ArenaKind::Fire;
        case ArenaKind::Faraway: return ArenaKind::Grass;
        case ArenaKind::JohtoChampion: return ArenaKind::Champion;   // Lance's hall: the spotlight, plus his dragon statues
        default: return k;
    }
}
inline bool IsLegendArena(ArenaKind k) { return k >= ArenaKind::Seafoam && k <= ArenaKind::Faraway; }
constexpr int ARENA_COUNT = (int)ArenaKind::Count;
const char* ArenaName(ArenaKind k);         // "Stone Gym" etc.
const char* ArenaKey(ArenaKind k);          // "rock" etc. (story.txt)
bool ParseArena(const std::string& s, ArenaKind& out);
const char* ArenaTwist(ArenaKind k);        // one-line description of the gym's twist

// Venues: how a non-gym battle looks (ArenaKind::Stadium with no gym rules). Story battles get
// one from where they happen, wild battles a random outdoor one.
enum class Venue : uint8_t { Stadium = 0, Route, Forest, Cave, IceCave, Beach, City, Bridge, Ship, Park, Dojo, Office,
                             Tower, Mansion, Volcano, Hideout, Lab, Mountain, Count };
constexpr int VENUE_COUNT = (int)Venue::Count;
const char* VenueName(Venue v);
bool VenueIndoors(Venue v);                          // no weather, lit at night
Venue VenueForPlace(const std::string& place);       // "Viridian Forest" -> Forest ...
Venue RandomWildVenue(int roll);                     // roll: any number (outdoor venues)
Venue VenueByName(const std::string& name);          // "ice cave" -> IceCave (unknown: Route)

// A shape's edge wobbles with the angle so no two islands, pools or puddles look alike.
// seed 0 = a plain ellipse. Returns the radius scale (about 0.8 - 1.15).
inline float Wobble(uint32_t seed, float ang) {
    if (!seed) return 1.0f;
    float a1 = 0.04f + (seed % 7) * 0.012f, a2 = 0.03f + ((seed >> 3) % 5) * 0.012f, a3 = ((seed >> 6) % 4) * 0.012f;
    float p1 = (seed >> 9) % 628 * 0.01f, p2 = (seed >> 12) % 628 * 0.01f, p3 = (seed >> 15) % 628 * 0.01f;
    int k1 = 2 + (seed >> 18) % 2, k2 = 3 + (seed >> 20) % 3;
    return 1.0f + a1 * std::sin(k1 * ang + p1) + a2 * std::sin(k2 * ang + p2) + a3 * std::sin(7 * ang + p3);
}

struct Obstacle {
    enum Kind { Pillar, Tree, Coil, Boulder, MetalPillar, TempWall, SnowMound, Crate, Center } kind = Pillar;   // Center: a gym's centerpiece (its body)
    uint32_t seed = 0;    // picks its look (shape variant, details)
    Vector2 pos{0, 0};
    float radius = 12, height = 28;
    bool alive = true;
    float burnT = 0;      // tree on fire: seconds left before it falls
    float fallT = -1;     // >= 0 while a tree is falling over
    Vector2 fallDir{1, 0};
    float respawnT = 0;   // pillars grow back
    float life = -1;      // TempWall: seconds left (made by a Wall move)
    float maxLife = 1;
    Type wallType = Type::Normal;
    float hp = 0, maxHp = 0;   // SnowMound / Crate: broken by hits (fire melts snow, burns crates)
    float grow = 1;            // SnowMound: 0..1 while it piles up
    bool Destructible() const { return kind == Pillar || kind == Tree || kind == SnowMound || kind == Crate; }
    bool Blocks() const { return alive && fallT < 0; }
};

// Ellipse areas on the floor (world units).
struct Zone {
    enum Kind { Platform, TallGrass, PoisonPool, LavaPool, Web } kind = Platform;   // Web: Bugsy's (drawn live: fire burns it away)
    Vector2 pos{0, 0};
    float rx = 30, ry = 30;
    uint32_t seed = 0;    // edge shape (Wobble)
    bool Contains(Vector2 p, float pad = 0) const {
        float dx = (p.x - pos.x) / (rx + pad), dy = (p.y - pos.y) / (ry + pad);
        float d2 = dx * dx + dy * dy;
        if (!seed) return d2 <= 1.0f;
        if (d2 > 1.4f) return false;
        float w = Wobble(seed, std::atan2(dy, dx));
        return d2 <= w * w;
    }
};

// Weather on the field: rain puddles (slow walkers down, conduct electricity, freeze into ice).
struct Puddle {
    Vector2 pos{0, 0};
    float r = 3, maxR = 20;    // grows toward maxR while it rains, shrinks as it dries
    float ice = 0;             // > 0: frozen for this many more seconds (slippery)
    float shockCd = 0;         // can't shock again until this reaches 0
    float shockT = 0;          // electric flash (drawing)
    bool sheet = false;        // ice sheet over the Water Gym's pool: gone when it thaws
    bool rink = false;         // Lorelei's ice rink: melted by fire, freezes over again
    float thawT = 0;           // rink: seconds until it freezes again
    uint32_t seed = 0;         // its shape
    bool Contains(Vector2 p, float pad = 0) const {
        float dx = p.x - pos.x, dy = p.y - pos.y;
        float rr = r * (seed ? Wobble(seed, std::atan2(dy, dx)) : 1.0f) + pad;
        return dx * dx + dy * dy <= rr * rr;
    }
};

struct Portal {
    Vector2 pos{0, 0};
    float age = 0;
};

struct Turret {
    Vector2 pos{0, 0};   // on the wall
    Vector2 dir{1, 0};   // firing direction
    float warnT = -1;    // >= 0: warning, fires when it reaches 0
    Vector2 aimAt{0, 0};
};

// A gym's signature centerpiece, in the middle of the arena.
//   Boulder (Brock): a heavy hit knocks it loose and it rolls, crushing what's in its path.
//   Drain (Misty): opens into a whirlpool that drags everyone in. Ice freezes it shut; Electric shocks everyone caught in it.
//   Generator (Surge): charges from hits (Electric most) and overloads in a paralysing shockwave. Ground drains it.
//   Bloom (Erika): opens and puffs sleep pollen. Fire burns it down; Grass types heal in the pollen.
//   Vat (Koga): a hit bursts it, spilling a wave of sludge away from whoever hit it.
//   Orb (Sabrina): fires at the foes of whoever hit it last. Dark moves knock it out for a while.
//   Vent (Blaine): rumbles, then erupts, raining lava bombs on marked spots. Water calms it; Fire sets it off early.
//   Spinner (Giovanni): a turntable that carries battlers around and flings them off when it reverses. Ground jams it.
//   Vane (Falkner): the weather vane. A hit turns the next gust against the hitter's foes.
//   Hive (Bugsy): a hit sends a homing swarm at the hitter's foes. Fire smokes it out for a while.
//   MilkTank (Whitney): a hit spills Moomoo Milk pools by the hitter that heal (Normal types twice as much).
//   Lamp (Jasmine): the lighthouse lamp sweeps a beam round; a hit reverses it and speeds it up.
struct Centerpiece {
    enum Kind : uint8_t { None, Boulder, Drain, Generator, Bloom, Vat, Orb, Vent, Spinner, Vane, Hive, MilkTank, Lamp } kind = None;
    Vector2 pos{0, 0}, home{0, 0}, vel{0, 0};
    float radius = 18;
    uint8_t phase = 0;        // what it's doing now (per kind)
    float t = 0;              // time left in this phase
    float charge = 0;         // Generator: 0..100; Bloom: growth; Vat: fill
    float spin = 0, spinRate = 0;   // Drain / Spinner / Boulder: turn angle and speed
    int owner = -1;           // Orb: the team it fights for (-1 none)
    float fireT = 0;          // Orb: next shot
    int mask = 0, bounces = 0;      // Boulder: who it has run over (per pass), wall bounces
    float hitCd = 0;          // short guard after it reacts, so one move doesn't set it off twice
    float flash = 0;          // a hit flash (drawing)
    std::vector<Vector2> marks;     // Vent: where the lava bombs will land
};

// Agatha's hall: a will-o'-wisp drifts toward anything that isn't a Ghost and burns it.
struct Wisp {
    Vector2 pos{0, 0}, vel{0, 0};
    float life = 0;       // > 0: out and hunting (seconds left)
    float respawnT = 0;   // gone: back after this
    uint32_t seed = 0;
};

struct Field {
    ArenaKind kind = ArenaKind::Stadium;
    Venue venue = Venue::Stadium;   // the look (and props) of a non-gym battle
    int season = -1;                // Story season for outdoor looks (0 spring .. 3 winter), -1 none
    std::vector<Obstacle> obs;
    std::vector<Zone> zones;
    std::vector<Portal> portals;
    std::vector<Turret> turrets;
    float portalT = 0, turretT = 0, lavaT = 0, tickT = 0;
    float electrifyCd = 0, waterFlash = 0;
    float spikeBand = 0;  // poison gym: toxic spikes along the edges
    std::vector<Puddle> puddles;
    float puddleT = 4, moundT = 6;   // next new puddle (rain) / snow mound (snow)
    std::vector<Wisp> wisps;         // Ghost hall
    Centerpiece center;              // the gym's centerpiece (kind None outside the eight gyms)
    Vector2 spot{0, 0}, spotTo{0, 0};   // Champion's stadium: the roaming spotlight
    float spotT = 0;
    float strikeT = 0, pulseT = 0, blinkT = 0;   // legendary lairs: falling strikes, Mewtwo's pulse, Mew's teleport
    Vector2 windDir{1, 0};          // Falkner's gym: where the next / current gust blows
    float windT = 0;                // time left in this wind phase
    uint8_t windOn = 0;             // 0 calm, 1 the windsocks warn, 2 a gust is blowing
    std::vector<Vector2> lights, lightTo;   // Karen's chamber: drifting pools of light
    float lightT = 0;
    static constexpr float SPOT_R = 44, LIGHT_R = 48, BEAM_HALF = 0.24f;
    ArenaKind Rules() const { return ArenaRules(kind); }
    bool InSpot(Vector2 p) const { float dx = p.x - spot.x, dy = p.y - spot.y; return Rules() == ArenaKind::Champion && dx * dx + dy * dy < SPOT_R * SPOT_R; }
    bool InLight(Vector2 p) const {
        for (const Vector2& l : lights) { float dx = p.x - l.x, dy = (p.y - l.y) * 1.2f; if (dx * dx + dy * dy < LIGHT_R * LIGHT_R) return true; }
        return false;
    }
    bool InBeam(Vector2 p) const {   // Jasmine's lighthouse beam
        if (center.kind != Centerpiece::Lamp) return false;
        float dx = p.x - center.pos.x, dy = p.y - center.pos.y;
        if (dx * dx + dy * dy < 30 * 30) return false;
        float d = std::atan2(dy, dx) - center.spin;
        d = std::remainder(d, 6.2831853f);
        return std::fabs(d) < BEAM_HALF;
    }

    void Build(ArenaKind k, Venue v = Venue::Stadium, int season = -1);  // random layout
    bool IsWater(Vector2 p) const;
    bool InZone(Vector2 p, Zone::Kind k, float pad = 0) const;
    bool InSpikes(Vector2 p) const;
    void PlacePortals();
    void PlaceCenterpiece();
};
