#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "raylib.h"
#include "types.h"

// Arenas. Stadium is the plain one; the rest are the eight gym stadiums.
enum class ArenaKind { Stadium, Rock, Water, Electric, Grass, Poison, Psychic, Fire, Ground,
                       Ice, Fighting, Ghost, Dragon, Champion, Count };   // Ice..Champion: the Pokemon League
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
    enum Kind { Pillar, Tree, Coil, Boulder, MetalPillar, TempWall, SnowMound, Crate } kind = Pillar;
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
    enum Kind { Platform, TallGrass, PoisonPool, LavaPool } kind = Platform;
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
    Vector2 spot{0, 0}, spotTo{0, 0};   // Champion's stadium: the roaming spotlight
    float spotT = 0;
    static constexpr float SPOT_R = 44;
    bool InSpot(Vector2 p) const { float dx = p.x - spot.x, dy = p.y - spot.y; return kind == ArenaKind::Champion && dx * dx + dy * dy < SPOT_R * SPOT_R; }

    void Build(ArenaKind k, Venue v = Venue::Stadium, int season = -1);  // random layout
    bool IsWater(Vector2 p) const;
    bool InZone(Vector2 p, Zone::Kind k, float pad = 0) const;
    bool InSpikes(Vector2 p) const;
    void PlacePortals();
};
