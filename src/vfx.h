#pragma once
#include <cstdint>
#include <string>
#include "moves.h"
#include "raylib.h"
#include "types.h"

// Battle visual effects: how every move looks (its "look"), the procedural textures, and the
// drawing of projectiles, particles, beams, bolts and explosions. Spawning happens inside the
// battle simulation (battle_vfx.cpp) so it stays in step online; this file only draws.

// Particle kinds
enum class PK : uint8_t { Pixel, Glow, Spark, Smoke, Flame, Leaf, Shard, Drop, Debris, Star, Bubble, Wisp, Feather, Petal, Ring, Zzz, Note, Heart, Coin, Snow, Ember, Arrow, Plus, Shield };
bool PKAdditive(PK k);

// Projectile shapes
enum class Shape : uint8_t {
    Orb, Fireball, ElecOrb, Shard, Leaf, Rock, Bubble, Star, Needle, Crescent, Droplet, ShadowOrb, Moon, Sludge, Seed,
    SoundRing, Comet, Wisp, Petal, Bone, Web, Coin, Egg, Tornado, Triangle, Heart, Gem, Note, Snow
};

// What a hit looks like
enum class Impact : uint8_t { Pow, Fire, Spark, Splash, Leaves, Frost, Debris, Poison, Psy, Shadow, Dragon, Fairy, Metal, Wind, Bug, Sand, Sound };

// Melee animation
enum class MeleeAnim : uint8_t { Arc, Claw, Fist, Kick, Bite, Chop, Poke, Whip };

// Beam decoration
enum class BeamDeco : uint8_t { Plain, Fire, Ice, Psy, Bubbles, Solar, Hyper, Wind, Electric, Dark, Rainbow, Water, Gem, Drain };

// Special casting effects
enum class Cast : uint8_t { None, Explosion, Discharge, Sparkle, Flames, Quake, Sound, Powder, Sky };

struct Look {
    Shape shape = Shape::Orb;
    Color core = WHITE, glow = WHITE;
    float size = 1.0f;          // projectile size multiplier
    PK trail = PK::Glow;        // particles left behind
    float trailRate = 0.6f;     // chance per tick
    Impact impact = Impact::Pow;
    bool boom = false;          // explodes where it lands / hits
    float boomR = 0;            // explosion radius
    MeleeAnim melee = MeleeAnim::Arc;
    BeamDeco beam = BeamDeco::Plain;
    float beamW = 1.0f;
    bool skyStrike = false;     // Thunder: lightning from the sky
    int boltBranches = 2;       // electric chains
    float boltWidth = 2.0f;
    Cast cast = Cast::None;
};

// A move's look, keyed by a hash of its name (the same on every computer). 0 = no move.
uint32_t LookKey(const std::string& name, Type t, MoveKind k);
const Look& GetLook(uint32_t key, Type fallback);
Color ImpactColor(Impact im, int which);   // explosion palette: 0 bright core, 1 body, 2 edge

void LoadVfx();
void UnloadVfx();

// Draw helpers. Positions are screen positions (after ToScreen).
uint32_t VHash(uint32_t a, uint32_t b = 0);
float VRand(uint32_t seed, uint32_t i);                 // 0..1, stable for a seed
void VAdd();                                            // additive blending on
void VNorm();                                           // normal blending
void VGlow(Vector2 p, float r, Color c, float alpha);   // soft light (additive)
void VSoft(Vector2 p, float r, Color c, float alpha);   // soft blob (normal)
void VShadow(Vector2 p, float rx, float ry, float alpha);   // soft contact shadow on the floor
void VRingE(Vector2 c, float rx, float ry, float thick, Color col, int segs = 40);  // ellipse ring
void VBolt(Vector2 a, Vector2 b, uint32_t seed, float width, Color c, float alpha, int branches);
void VExplosion(Vector2 p, float r, float t, Impact im, uint32_t seed);
void VBeam(Vector2 a, Vector2 b, float width, float t, float time, const Look& L, uint32_t seed);
void VProjectile(Vector2 sp, Vector2 vel, float radius, float spin, float time, const Look& L, uint32_t seed);
void VParticleBody(PK kind, Vector2 sp, Vector2 svel, float size, float rot, float lifeFrac, Color c, float time);  // normal-blend part of additive kinds
void VParticle(PK kind, Vector2 sp, Vector2 svel, float size, float rot, float lifeFrac, Color c, float time);
void VMelee(Vector2 from, Vector2 to, float t, MeleeAnim anim, const Look& L, uint32_t seed);
void VImpactStar(Vector2 p, float r, float t, Color c, uint32_t seed);
void VStatus(Vector2 base, float h, int status, float time, uint32_t seed);  // ongoing burn flames, poison bubbles...
void VDashAura(Vector2 base, float h, Vector2 dir, const Look& L, float time);
void VCharge(Vector2 p, float r, float t, Color c, uint32_t seed, float time);
void DrawCrackFx(Vector2 worldOrigin, float radius, float t, uint32_t seed, int arms, Color c);  // ground cracks (battle_vfx.cpp)
