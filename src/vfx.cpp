// How moves look: procedural textures, a "look" for every move, and the drawing of
// projectiles, beams, bolts, explosions, melee strikes and particles.
#include "vfx.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>
#include "vec.h"

namespace {

constexpr float PI_F = 3.14159265f;

Color C(int r, int g, int b, int a = 255) { return Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; }
Color Mix(Color a, Color b, float t) {
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return C((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t), (int)(a.a + (b.a - a.a) * t));
}
float Clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
float EaseOut(float t) { t = Clamp01(t); return 1 - (1 - t) * (1 - t) * (1 - t); }
bool Has(const std::string& s, const char* w) { return s.find(w) != std::string::npos; }
float Deg(Vector2 d) { return std::atan2(d.y, d.x) * 57.2958f; }

// ---------------------------------------------------------------- Textures

enum TX { T_BEAM, T_GLOW, T_SMOKE, T_RING, T_LEAF, T_SHARD, T_ROCK, T_STAR, T_BUBBLE, T_FEATHER, T_PETAL, T_MOON, T_CRESCENT,
          T_FIST, T_BOOT, T_JAW, T_HEART, T_COIN, T_NOTE, T_SEED, T_BONE, T_EGG, T_STREAK, T_COUNT };
Texture2D gTex[T_COUNT];
bool gLoaded = false;

// Pixel art in grey levels (tinted when drawn): # outline, d dark, o main, + light, w white.
Image FromArt(const std::vector<const char*>& rows) {
    int h = (int)rows.size(), w = 0;
    for (auto r : rows) w = std::max(w, (int)std::strlen(r));
    Image img = GenImageColor(w, h, BLANK);
    for (int y = 0; y < h; y++) {
        int n = (int)std::strlen(rows[y]);
        for (int x = 0; x < n; x++) {
            int g = -1;
            switch (rows[y][x]) {
                case '#': g = 70; break;
                case 'd': g = 150; break;
                case 'o': g = 205; break;
                case '+': g = 238; break;
                case 'w': g = 255; break;
                default: break;
            }
            if (g >= 0) ImageDrawPixel(&img, x, y, C(g, g, g));
        }
    }
    return img;
}

float H01(uint32_t x) { return (VHash(x) & 0xffffff) / (float)0x1000000; }

// Smooth value noise for smoke.
float Noise2(float x, float y, uint32_t seed) {
    int xi = (int)std::floor(x), yi = (int)std::floor(y);
    float fx = x - xi, fy = y - yi;
    auto at = [&](int a, int b) { return H01(VHash((uint32_t)(a * 73856093) ^ (uint32_t)(b * 19349663), seed)); };
    float sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    float v0 = at(xi, yi) + (at(xi + 1, yi) - at(xi, yi)) * sx;
    float v1 = at(xi, yi + 1) + (at(xi + 1, yi + 1) - at(xi, yi + 1)) * sx;
    return v0 + (v1 - v0) * sy;
}

Image Procedural(int w, int h, uint32_t kind) {
    Image img = GenImageColor(w, h, BLANK);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float u = (x + 0.5f) / w * 2 - 1, v = (y + 0.5f) / h * 2 - 1;
            float d = std::sqrt(u * u + v * v);
            Color c = BLANK;
            switch (kind) {
                case T_BEAM: {
                    float a = Clamp01((1 - std::fabs(v)) * 2.2f);
                    a = a * a * (3 - 2 * a);
                    c = C(255, 255, 255, (int)(Clamp01(a) * 255));
                } break;
                case T_STREAK: {   // a comet tail: fades in from the back, soft across, round at the head
                    float along = (u + 1) * 0.5f;   // 0 tail .. 1 head
                    float across = std::fabs(v) / (0.25f + 0.75f * along);
                    float a = Clamp01(1 - across);
                    a = a * a * (3 - 2 * a) * along * along;
                    if (along > 0.9f) a *= Clamp01((1 - along) / 0.1f + 0.2f);
                    c = C(255, 255, 255, (int)(Clamp01(a) * 255));
                } break;
                case T_GLOW: {   // a soft gaussian with a hotter centre
                    float g = std::exp(-d * d * 4.2f) * 0.85f + std::exp(-d * d * 14.0f) * 0.15f;
                    float a = Clamp01((g - 0.012f) / 0.988f) * Clamp01((1 - d) * 6);
                    c = C(255, 255, 255, (int)(a * 255));
                } break;
                case T_SMOKE: {
                    float n = Noise2(u * 3 + 7, v * 3 + 3, 11) * 0.65f + Noise2(u * 6, v * 6, 5) * 0.35f;
                    float a = Clamp01((1 - d * 1.1f) * 1.6f) * Clamp01(n * 1.6f - 0.25f);
                    int g = (int)(200 + 55 * n);
                    c = C(g, g, g, (int)(a * 255));
                } break;
                case T_RING: {
                    float a = Clamp01(1 - std::fabs(d - 0.82f) / 0.14f);
                    c = C(255, 255, 255, (int)(a * a * 255));
                } break;
                case T_STAR: {
                    float ang = std::atan2(v, u) + PI_F / 2;
                    float k = std::fmod(ang + 10 * PI_F, 2 * PI_F / 5) / (2 * PI_F / 5);
                    float edge = 0.42f + 0.55f * std::fabs(k - 0.5f) * 2;
                    edge = 1.0f - 0.58f * (1 - std::fabs(k - 0.5f) * 2);
                    if (d < edge) {
                        int g = d > edge - 0.18f ? 90 : (d < 0.3f ? 255 : 225);
                        c = C(g, g, g);
                    }
                } break;
                case T_BUBBLE: {
                    if (d < 1) {
                        float rim = Clamp01((d - 0.72f) / 0.28f);
                        float a = 0.18f + 0.75f * rim * rim;
                        float hx = u + 0.38f, hy = v + 0.38f;
                        if (hx * hx + hy * hy < 0.05f) a = 1;
                        c = C(255, 255, 255, (int)(a * 255));
                    }
                } break;
                case T_MOON: {
                    if (d < 0.95f) {
                        float cr = Noise2(u * 3.2f + 2, v * 3.2f + 9, 3);
                        int g = (int)(225 + 30 * (1 - d) - (cr > 0.62f ? 40 : 0));
                        if (d > 0.82f) g = 150;
                        c = C(std::min(255, g), std::min(255, g), std::min(255, g));
                    }
                } break;
                case T_CRESCENT: {
                    float ox = u + 0.45f;
                    float d2 = std::sqrt(ox * ox + v * v);
                    if (d < 0.95f && d2 > 0.85f) {
                        float a = Clamp01((0.95f - d) / 0.12f) * Clamp01((d2 - 0.85f) / 0.12f);
                        c = C(255, 255, 255, (int)(a * 255));
                    }
                } break;
                case T_HEART: {
                    float X = u * 1.25f, Y = -v * 1.25f + 0.25f;
                    float q = X * X + Y * Y - 1;
                    if (q * q * q - X * X * Y * Y * Y <= 0) {
                        float qq = (X * 1.08f) * (X * 1.08f) + (Y * 1.08f) * (Y * 1.08f) - 1;
                        bool inner = qq * qq * qq - (X * 1.08f) * (X * 1.08f) * (Y * 1.08f) * (Y * 1.08f) * (Y * 1.08f) <= 0;
                        int g = inner ? ((u < -0.2f && v < -0.2f) ? 255 : 215) : 90;
                        c = C(g, g, g);
                    }
                } break;
                case T_COIN: {
                    if (d < 1) {
                        int g = d > 0.78f ? 110 : (d > 0.58f ? 235 : ((std::fabs(u) < 0.14f && std::fabs(v) < 0.45f) ? 150 : 215));
                        c = C(g, g, g);
                    }
                } break;
                case T_SEED: {
                    float e = u * u + v * v * 0.45f;
                    if (e < 0.8f) {
                        int g = e > 0.55f ? 70 : ((u < -0.1f && v < -0.2f) ? 238 : 190);
                        c = C(g, g, g);
                    }
                } break;
                case T_EGG: {
                    float yy = v < 0 ? v * 0.85f : v;
                    float e = u * u * 1.35f + yy * yy;
                    if (e < 0.9f) {
                        bool spot = Noise2(u * 4 + 1, v * 4 + 1, 7) > 0.7f;
                        int g = e > 0.7f ? 90 : (spot ? 150 : 245);
                        c = C(g, g, g);
                    }
                } break;
                case T_PETAL: {
                    float e = u * u * 0.6f + v * v * 1.6f;
                    if (e < 0.8f) {
                        int g = e > 0.6f ? 120 : (e < 0.2f ? 255 : 225);
                        c = C(g, g, g);
                    }
                } break;
                default: break;
            }
            if (c.a) ImageDrawPixel(&img, x, y, c);
        }
    }
    return img;
}

void MakeTex(int id, Image img, bool smooth) {
    gTex[id] = LoadTextureFromImage(img);
    SetTextureFilter(gTex[id], smooth ? TEXTURE_FILTER_BILINEAR : TEXTURE_FILTER_POINT);
    UnloadImage(img);
}

// Draw a texture centred at p, 'size' logical units across its larger side.
void Tex(int id, Vector2 p, float size, float rotDeg, Color c, float alpha, float sx = 1, float sy = 1) {
    if (!gLoaded) { DrawCircleV(p, size * 0.4f, Fade(c, alpha)); return; }
    const Texture2D& t = gTex[id];
    float k = size / (float)std::max(t.width, t.height);
    float w = t.width * k * sx, h = t.height * k * sy;
    DrawTexturePro(t, Rectangle{0, 0, (float)t.width, (float)t.height}, Rectangle{p.x, p.y, std::fabs(w), std::fabs(h)},
                   Vector2{std::fabs(w) / 2, std::fabs(h) / 2}, rotDeg, Fade(c, alpha));
}

// ---------------------------------------------------------------- Looks

Look TypeLook(Type t) {
    Look L;
    switch (t) {
        case Type::Fire:     L.shape = Shape::Fireball; L.core = C(255, 236, 150); L.glow = C(255, 110, 30); L.trail = PK::Flame; L.impact = Impact::Fire; L.beam = BeamDeco::Fire; L.trailRate = 0.9f; break;
        case Type::Water:    L.shape = Shape::Droplet; L.core = C(200, 235, 255); L.glow = C(60, 140, 255); L.trail = PK::Drop; L.impact = Impact::Splash; L.beam = BeamDeco::Water; break;
        case Type::Electric: L.shape = Shape::ElecOrb; L.core = C(255, 255, 210); L.glow = C(255, 220, 40); L.trail = PK::Spark; L.impact = Impact::Spark; L.beam = BeamDeco::Electric; L.trailRate = 0.8f; break;
        case Type::Grass:    L.shape = Shape::Leaf; L.core = C(150, 235, 90); L.glow = C(80, 210, 60); L.trail = PK::Leaf; L.impact = Impact::Leaves; L.beam = BeamDeco::Solar; L.trailRate = 0.35f; break;
        case Type::Ice:      L.shape = Shape::Shard; L.core = C(225, 250, 255); L.glow = C(120, 215, 255); L.trail = PK::Snow; L.impact = Impact::Frost; L.beam = BeamDeco::Ice; break;
        case Type::Fighting: L.shape = Shape::Orb; L.core = C(255, 220, 170); L.glow = C(235, 100, 50); L.trail = PK::Glow; L.impact = Impact::Pow; L.beam = BeamDeco::Plain; break;
        case Type::Poison:   L.shape = Shape::Sludge; L.core = C(200, 120, 230); L.glow = C(160, 60, 210); L.trail = PK::Bubble; L.impact = Impact::Poison; L.beam = BeamDeco::Plain; L.trailRate = 0.4f; break;
        case Type::Ground:   L.shape = Shape::Sludge; L.core = C(200, 160, 100); L.glow = C(160, 115, 60); L.trail = PK::Debris; L.impact = Impact::Sand; L.beam = BeamDeco::Plain; L.trailRate = 0.4f; break;
        case Type::Flying:   L.shape = Shape::Crescent; L.core = C(255, 255, 255); L.glow = C(170, 205, 255); L.trail = PK::Glow; L.impact = Impact::Wind; L.beam = BeamDeco::Wind; break;
        case Type::Psychic:  L.shape = Shape::Orb; L.core = C(255, 205, 240); L.glow = C(250, 90, 180); L.trail = PK::Ring; L.impact = Impact::Psy; L.beam = BeamDeco::Psy; L.trailRate = 0.3f; break;
        case Type::Bug:      L.shape = Shape::Needle; L.core = C(220, 245, 130); L.glow = C(150, 195, 40); L.trail = PK::Glow; L.impact = Impact::Bug; L.beam = BeamDeco::Rainbow; break;
        case Type::Rock:     L.shape = Shape::Rock; L.core = C(205, 185, 150); L.glow = C(150, 125, 90); L.trail = PK::Debris; L.impact = Impact::Debris; L.beam = BeamDeco::Gem; L.trailRate = 0.3f; break;
        case Type::Ghost:    L.shape = Shape::ShadowOrb; L.core = C(90, 40, 130); L.glow = C(170, 100, 255); L.trail = PK::Wisp; L.impact = Impact::Shadow; L.beam = BeamDeco::Dark; break;
        case Type::Dragon:   L.shape = Shape::Orb; L.core = C(210, 190, 255); L.glow = C(110, 70, 250); L.trail = PK::Glow; L.impact = Impact::Dragon; L.beam = BeamDeco::Plain; break;
        case Type::Dark:     L.shape = Shape::ShadowOrb; L.core = C(40, 30, 55); L.glow = C(130, 80, 160); L.trail = PK::Smoke; L.impact = Impact::Shadow; L.beam = BeamDeco::Dark; L.trailRate = 0.4f; break;
        case Type::Steel:    L.shape = Shape::Gem; L.core = C(240, 242, 250); L.glow = C(160, 185, 210); L.trail = PK::Spark; L.impact = Impact::Metal; L.beam = BeamDeco::Gem; L.trailRate = 0.4f; break;
        case Type::Fairy:    L.shape = Shape::Star; L.core = C(255, 230, 250); L.glow = C(255, 140, 210); L.trail = PK::Star; L.impact = Impact::Fairy; L.beam = BeamDeco::Rainbow; L.trailRate = 0.4f; break;
        default:             L.shape = Shape::Orb; L.core = C(255, 255, 255); L.glow = C(220, 220, 230); L.trail = PK::Glow; L.impact = Impact::Pow; L.beam = BeamDeco::Hyper; break;
    }
    return L;
}

MeleeAnim MeleeFor(const std::string& n) {
    static const char* fist[] = {"Punch", "Mash", "Arm Thrust", "Palm", "Hammer Arm", "Rage Fist", "Beat Up", "Brick Break", "Rock Smash", "Revenge", "Retaliate", "Payback", "Wake-Up Slap", "Smelling Salts", "Body Press", "Focus", "Strength", "Power-Up", "Superpower"};
    static const char* kick[] = {"Kick", "Stomp", "Low Sweep", "Triple Axel"};
    static const char* bite[] = {"Bite", "Fang", "Crunch", "Jaw", "Leech Life", "Lick", "Kiss", "Vise Grip", "Crush Grip", "Constrict", "Bug Bite", "Thief", "Pluck"};
    static const char* claw[] = {"Claw", "Scratch", "Slash", "Cut", "Scissor", "Blade", "Swipes", "Sword", "Razor Shell", "Cleave", "Axe", "Guillotine", "False Swipe", "Fury Cutter", "Fell Stinger", "Knock Off", "Assurance", "Punishment", "Wicked Blow", "Order Up", "Stone Axe", "Behemoth"};
    static const char* chop[] = {"Chop", "Hammer", "Club", "Bone Rush", "Iron Head", "Headbutt", "Zen Headbutt", "Submission", "Throw", "Circle Throw", "Storm Throw", "Bash"};
    static const char* poke[] = {"Horn", "Peck", "Jab", "Drill", "Sting", "Needle", "Poke", "Fury Attack", "Double Hit", "Astonish", "Tickle"};
    static const char* whip[] = {"Tail", "Whip", "Slam", "Lash", "Wing", "Slap", "Acrobatics", "Skitter", "Spirit Break", "Flail", "Thrash", "Rage", "Struggle"};
    for (auto w : fist) if (Has(n, w)) return MeleeAnim::Fist;
    for (auto w : kick) if (Has(n, w)) return MeleeAnim::Kick;
    for (auto w : bite) if (Has(n, w)) return MeleeAnim::Bite;
    for (auto w : claw) if (Has(n, w)) return MeleeAnim::Claw;
    for (auto w : chop) if (Has(n, w)) return MeleeAnim::Chop;
    for (auto w : poke) if (Has(n, w)) return MeleeAnim::Poke;
    for (auto w : whip) if (Has(n, w)) return MeleeAnim::Whip;
    return MeleeAnim::Arc;
}

bool IsSoundMove(const std::string& n) {
    static const char* w[] = {"Voice", "Growl", "Roar", "Screech", "Sing", "Snore", "Uproar", "Round", "Chatter", "Boomburst", "Buzz",
                              "Metal Sound", "Clanging", "Synchronoise", "Perish Song", "Sparkling Aria", "Drum", "Overdrive", "Supersonic", "Sonic", "Bug Buzz"};
    for (auto x : w) if (Has(n, x)) return true;
    return false;
}

Look BuildLook(const std::string& n, Type t, MoveKind k) {
    Look L = TypeLook(t);
    if (k == MoveKind::Melee || k == MoveKind::Dash) L.melee = MeleeFor(n);
    if (k == MoveKind::Rain) {
        switch (t) {
            case Type::Rock: case Type::Ground: L.shape = Shape::Rock; L.size = 1.7f; L.impact = Impact::Debris; break;
            case Type::Electric: L.skyStrike = true; break;
            case Type::Fire: case Type::Dragon: L.shape = Shape::Comet; L.size = 1.3f; L.boom = true; L.boomR = 18; break;
            case Type::Ice: L.shape = Shape::Shard; L.size = 1.8f; L.trail = PK::Snow; break;
            case Type::Water: L.shape = Shape::Droplet; L.size = 1.8f; break;
            case Type::Steel: L.shape = Shape::Coin; L.size = 1.5f; L.trail = PK::Spark; break;
            default: L.size = 1.3f; break;
        }
    }
    if (k == MoveKind::Lob) {
        if (t == Type::Fire || t == Type::Poison || t == Type::Grass || t == Type::Normal || t == Type::Fighting || t == Type::Fairy || t == Type::Dark) {
            L.boom = true;
            L.boomR = 30;
        }
        if (t == Type::Rock || t == Type::Ground || t == Type::Ice) L.size = 1.4f;
    }
    if (k == MoveKind::Burst && IsSoundMove(n)) L.cast = Cast::Sound;
    if (k == MoveKind::Wave) {
        if (t == Type::Ground) L.cast = Cast::Quake;
        else if (IsSoundMove(n)) L.cast = Cast::Sound;
        else if (t == Type::Fire) L.cast = Cast::Flames;
    }
    if (k == MoveKind::Cone && IsSoundMove(n)) { L.cast = Cast::Sound; L.trail = PK::Ring; }
    if (k == MoveKind::Cone && t == Type::Fire) L.trail = PK::Flame;
    if (k == MoveKind::Chain && t == Type::Electric) { L.boltWidth = 2.2f; L.boltBranches = 2; }
    if (k == MoveKind::Boomerang) L.shape = Has(n, "Bone") ? Shape::Bone : Shape::Crescent;

    // ---- Named moves
    auto is = [&](const char* x) { return n == x; };
    // Normal
    if (is("Explosion")) { L.cast = Cast::Explosion; L.boomR = 80; L.impact = Impact::Fire; }
    if (is("Self-Destruct")) { L.cast = Cast::Explosion; L.boomR = 60; L.impact = Impact::Fire; }
    if (is("Misty Explosion")) { L.cast = Cast::Explosion; L.boomR = 60; L.impact = Impact::Fairy; }
    if (is("Swift")) { L.shape = Shape::Star; L.core = C(255, 250, 180); L.glow = C(255, 220, 80); L.trail = PK::Star; L.impact = Impact::Fairy; }
    if (is("Tri Attack")) { L.shape = Shape::Triangle; L.trail = PK::Spark; }
    if (is("Hyper Beam") || is("Giga Impact")) { L.beam = BeamDeco::Hyper; L.beamW = 1.5f; L.core = C(255, 250, 220); L.glow = C(255, 160, 60); }
    if (is("Egg Bomb")) { L.shape = Shape::Egg; L.size = 1.5f; L.boom = true; L.boomR = 34; }
    if (is("Sonic Boom") || is("Supersonic") || is("Vacuum Wave")) { L.shape = Shape::SoundRing; L.trail = PK::Ring; L.trailRate = 0.25f; L.impact = Impact::Sound; }
    if (is("Pay Day") || is("Make It Rain")) { L.shape = Shape::Coin; L.core = C(255, 215, 70); L.glow = C(255, 200, 60); L.trail = PK::Coin; L.trailRate = 0.3f; L.impact = Impact::Metal; }
    if (is("Spike Cannon")) { L.shape = Shape::Needle; }
    if (is("Barrage")) { L.shape = Shape::Egg; L.size = 1.1f; }
    if (is("Attract") || is("Sweet Kiss") || is("Draining Kiss") || is("Charm") || is("Lovely Kiss")) { L.shape = Shape::Heart; L.core = C(255, 170, 210); L.glow = C(255, 100, 170); L.trail = PK::Heart; L.trailRate = 0.3f; L.impact = Impact::Fairy; }
    if (is("Sing") || is("Round") || is("Perish Song") || is("Sparkling Aria") || is("Alluring Voice")) { L.shape = Shape::Note; L.trail = PK::Note; L.trailRate = 0.3f; L.impact = Impact::Sound; }
    if (is("Yawn") || is("Hypnosis")) { L.shape = Shape::SoundRing; L.trail = PK::Ring; L.impact = Impact::Psy; }
    if (is("Weather Ball")) { L.shape = Shape::Orb; L.size = 1.3f; }
    // Fire
    if (is("Ember")) { L.size = 0.85f; }
    if (is("Fire Blast")) { L.size = 1.7f; L.boom = true; L.boomR = 44; }
    if (is("Flamethrower")) { L.beamW = 1.25f; }
    if (is("Overheat") || is("Blue Flare")) { L.beamW = 1.6f; }
    if (is("Blue Flare")) { L.glow = C(60, 140, 255); L.core = C(200, 230, 255); }
    if (is("Will-O-Wisp") || is("Mystical Fire") || is("Infernal Parade")) { L.shape = Shape::Wisp; L.core = C(150, 200, 255); L.glow = C(70, 120, 255); L.trail = PK::Wisp; }
    if (is("Eruption") || is("Pyro Ball") || is("Sacred Fire") || is("Flame Burst")) { L.boom = true; L.boomR = std::max(L.boomR, 30.0f); }
    if (is("Lava Plume") || is("Burn Up") || is("Fire Spin") || is("Magma Storm")) { L.cast = Cast::Flames; }
    // Water
    if (is("Hydro Pump") || is("Hydro Cannon")) { L.beamW = 1.5f; }
    if (is("Bubble") || is("Bubble Beam") || is("Bubble Trap")) { L.shape = Shape::Bubble; L.trail = PK::Bubble; L.beam = BeamDeco::Bubbles; L.core = C(220, 245, 255); L.glow = C(120, 190, 255); }
    if (is("Octazooka")) { L.shape = Shape::ShadowOrb; L.core = C(30, 30, 40); L.glow = C(80, 80, 120); L.trail = PK::Smoke; }
    if (is("Scald") || is("Steam Eruption")) { L.trail = PK::Smoke; }
    if (is("Water Shuriken")) { L.shape = Shape::Star; L.trail = PK::Drop; }
    // Electric
    if (is("Thunderbolt")) { L.boltWidth = 3.2f; L.boltBranches = 4; }
    if (is("Thunder Shock")) { L.boltWidth = 1.7f; L.boltBranches = 1; }
    if (is("Thunder")) { L.skyStrike = true; L.boltWidth = 4; L.boltBranches = 5; }
    if (is("Zap Cannon")) { L.size = 2.0f; L.boom = true; L.boomR = 24; }
    if (is("Electro Ball")) { L.size = 1.5f; }
    if (is("Discharge") || is("Parabolic Charge") || is("Eerie Impulse") || is("Overdrive")) { L.cast = Cast::Discharge; }
    if (is("Thunder Wave")) { L.shape = Shape::SoundRing; L.trail = PK::Spark; }
    // Grass
    if (is("Razor Leaf") || is("Leaf Storm") || is("Magical Leaf") || is("Leaf Tornado")) { L.shape = Shape::Leaf; L.trail = PK::Leaf; L.trailRate = 0.5f; }
    if (is("Magical Leaf")) { L.glow = C(255, 160, 230); }
    if (is("Solar Beam") || is("Seed Flare")) { L.beam = BeamDeco::Solar; L.beamW = 1.7f; L.core = C(255, 255, 220); L.glow = C(255, 220, 90); }
    if (is("Giga Drain")) { L.beam = BeamDeco::Drain; }
    if (is("Absorb") || is("Mega Drain") || is("Energy Ball")) { L.shape = Shape::Orb; L.trail = PK::Glow; }
    if (is("Energy Ball")) { L.size = 1.5f; }
    if (is("Bullet Seed") || is("Leech Seed") || is("Worry Seed") || is("Seed Bomb")) { L.shape = Shape::Seed; L.trail = PK::Leaf; L.trailRate = 0.25f; }
    if (is("Seed Bomb") || is("Apple Acid")) { L.boom = true; L.boomR = 30; }
    if (is("Petal Dance") || is("Petal Blizzard") || is("Fairy Wind") || is("Floral Healing")) { L.shape = Shape::Petal; L.trail = PK::Petal; L.core = C(255, 190, 220); L.glow = C(255, 130, 190); }
    if (is("Sleep Powder") || is("Stun Spore") || is("Poison Powder") || is("Spore") || is("Rage Powder") || is("Cotton Spore") || is("Sweet Scent")) { L.cast = Cast::Powder; L.trail = PK::Glow; }
    if (is("Sleep Powder") || is("Spore")) { L.glow = C(120, 220, 160); }
    if (is("Stun Spore")) { L.glow = C(255, 230, 80); }
    if (is("Poison Powder")) { L.glow = C(190, 90, 230); }
    // Ice
    if (is("Ice Beam") || is("Freeze Shock") || is("Ice Burn")) { L.beam = BeamDeco::Ice; L.beamW = 1.2f; }
    if (is("Aurora Beam")) { L.beam = BeamDeco::Rainbow; }
    if (is("Blizzard") || is("Powder Snow") || is("Icy Wind")) { L.shape = Shape::Snow; L.trail = PK::Snow; L.trailRate = 0.9f; }
    if (is("Blizzard")) { L.size = 1.6f; }
    if (is("Icicle Crash") || is("Icicle Spear") || is("Ice Shard")) { L.shape = Shape::Shard; }
    if (is("Avalanche")) { L.shape = Shape::Rock; L.core = C(230, 245, 255); L.size = 1.8f; L.boom = true; L.boomR = 34; L.impact = Impact::Frost; }
    // Fighting
    if (is("Aura Sphere")) { L.size = 1.4f; L.core = C(220, 240, 255); L.glow = C(70, 140, 255); L.trail = PK::Glow; }
    if (is("Focus Blast")) { L.size = 1.9f; L.boom = true; L.boomR = 38; L.glow = C(255, 150, 60); }
    // Poison
    if (is("Sludge Bomb") || is("Gunk Shot")) { L.boom = true; L.boomR = 34; L.size = 1.5f; }
    if (is("Acid") || is("Acid Spray") || is("Belch")) { L.shape = Shape::Droplet; L.core = C(200, 240, 120); L.glow = C(150, 200, 60); }
    if (is("Poison Sting") || is("Barb Barrage") || is("Twineedle") || is("Pin Missile")) { L.shape = Shape::Needle; }
    if (is("Smog") || is("Poison Gas") || is("Clear Smog")) { L.trail = PK::Smoke; }
    // Ground
    if (is("Earthquake") || is("Magnitude") || is("Bulldoze") || is("Fissure") || is("Land's Wrath") || is("Precipice Blades")) L.cast = Cast::Quake;
    if (is("Mud-Slap") || is("Mud Shot") || is("Mud Bomb")) { L.shape = Shape::Sludge; L.core = C(150, 110, 70); L.glow = C(120, 90, 50); L.trail = PK::Drop; }
    if (is("Mud Bomb")) { L.boom = true; L.boomR = 28; L.impact = Impact::Sand; }
    if (is("Scorching Sands") || is("Sand Attack") || is("Sand Tomb")) { L.trail = PK::Smoke; L.impact = Impact::Sand; }
    // Rock
    if (is("Stone Edge") || is("Power Gem") || is("Diamond Storm") || is("Salt Cure")) { L.shape = Shape::Shard; L.core = C(255, 230, 190); L.glow = C(230, 180, 110); }
    if (is("Power Gem") || is("Meteor Beam")) { L.beam = BeamDeco::Gem; }
    if (is("Ancient Power")) { L.shape = Shape::Rock; L.glow = C(255, 200, 120); L.trail = PK::Glow; }
    // Psychic
    if (is("Psybeam")) { L.beam = BeamDeco::Psy; }
    if (is("Psystrike") || is("Psyshock")) { L.shape = Shape::Shard; L.core = C(255, 210, 245); }
    if (is("Future Sight") || is("Doom Desire")) { L.shape = Shape::Orb; L.boom = true; L.boomR = 22; L.size = 1.6f; }
    if (is("Mist Ball")) { L.shape = Shape::Orb; L.glow = C(255, 160, 220); }
    if (is("Luster Purge")) { L.shape = Shape::Orb; L.glow = C(255, 220, 140); }
    // Ghost
    if (is("Shadow Ball")) { L.size = 1.5f; L.boom = true; L.boomR = 22; }
    if (is("Confuse Ray")) { L.shape = Shape::Wisp; L.core = C(255, 255, 200); L.glow = C(255, 220, 100); }
    if (is("Hex") || is("Spirit Shackle")) { L.shape = Shape::Wisp; }
    if (is("Astral Barrage") || is("Ominous Wind")) { L.shape = Shape::Wisp; L.trail = PK::Wisp; }
    // Dragon
    if (is("Draco Meteor")) { L.shape = Shape::Comet; L.core = C(255, 220, 200); L.glow = C(255, 90, 60); L.size = 1.7f; L.boom = true; L.boomR = 30; L.impact = Impact::Fire; }
    if (is("Dragon Rage") || is("Dragon Breath")) { L.shape = Shape::Fireball; L.core = C(230, 210, 255); L.glow = C(130, 80, 255); L.trail = PK::Flame; L.impact = Impact::Dragon; }
    if (is("Twister")) { L.shape = Shape::Tornado; L.trail = PK::Glow; }
    if (is("Dragon Pulse") || is("Dynamax Cannon")) { L.beamW = 1.3f; }
    if (is("Dragon Darts") || is("Scale Shot")) { L.shape = Shape::Shard; }
    // Dark
    if (is("Dark Pulse") || is("Night Daze")) { L.cast = Cast::None; }
    if (is("Snarl")) { L.shape = Shape::SoundRing; L.trail = PK::Smoke; }
    // Fairy
    if (is("Moonblast")) { L.shape = Shape::Moon; L.size = 1.5f; L.boom = true; L.boomR = 24; L.core = C(255, 240, 250); }
    if (is("Dazzling Gleam") || is("Flash") || is("Sweet Scent") || is("Play Nice") || is("Charm")) { L.cast = Cast::Sparkle; }
    if (is("Fleur Cannon") || is("Light of Ruin")) { L.beam = BeamDeco::Rainbow; L.beamW = 1.5f; }
    // Steel
    if (is("Flash Cannon")) { L.beam = BeamDeco::Gem; L.beamW = 1.25f; L.core = C(255, 255, 255); L.glow = C(180, 210, 240); }
    if (is("Steel Beam")) { L.beam = BeamDeco::Hyper; L.beamW = 1.6f; L.glow = C(170, 200, 230); }
    if (is("Magnet Bomb")) { L.shape = Shape::Orb; L.boom = true; L.boomR = 20; }
    if (is("Mirror Shot") || is("Smart Strike") || is("Anchor Shot")) { L.shape = Shape::Gem; }
    // Bug
    if (is("String Shot") || is("Electroweb") || is("Spider Web") || is("Sticky Web")) { L.shape = Shape::Web; L.trail = PK::Pixel; }
    if (is("Signal Beam")) { L.beam = BeamDeco::Rainbow; }
    if (is("Silver Wind")) { L.shape = Shape::Crescent; L.core = C(230, 230, 240); L.glow = C(200, 200, 220); }
    if (is("Attack Order")) { L.shape = Shape::Needle; L.size = 1.3f; }
    // Flying
    if (is("Air Slash") || is("Air Cutter") || is("Razor Wind") || is("Aerial Ace")) { L.shape = Shape::Crescent; }
    if (is("Gust") || is("Bleakwind Storm")) { L.shape = Shape::Tornado; }
    if (is("Hurricane") || is("Aeroblast")) { L.beam = BeamDeco::Wind; L.beamW = 1.5f; }
    if (is("Feather Dance") || is("Roost")) { L.trail = PK::Feather; }
    if (is("Sky Attack") || is("Brave Bird")) { L.glow = C(255, 220, 120); }
    return L;
}

struct LookEntry { Look look; };
std::unordered_map<uint32_t, Look>& Looks() { static std::unordered_map<uint32_t, Look> m; return m; }
Look gTypeLooks[32];
bool gTypeLooksReady = false;

void ReadyTypeLooks() {
    if (gTypeLooksReady) return;
    for (int i = 0; i < (int)Type::Count; i++) gTypeLooks[i] = TypeLook((Type)i);
    gTypeLooks[31] = TypeLook(Type::Normal);
    gTypeLooksReady = true;
}

// Bolt geometry: midpoint displacement, stable for a seed.
void BoltPoints(std::vector<Vector2>& pts, Vector2 a, Vector2 b, uint32_t seed, int depth) {
    pts.clear();
    pts.push_back(a);
    pts.push_back(b);
    float disp = Dist(a, b) * 0.22f;
    uint32_t k = 1;
    for (int d = 0; d < depth; d++) {
        std::vector<Vector2> nx;
        nx.reserve(pts.size() * 2);
        for (size_t i = 0; i + 1 < pts.size(); i++) {
            Vector2 p = pts[i], q = pts[i + 1];
            Vector2 m = (p + q) * 0.5f;
            Vector2 dir = SafeNorm(q - p);
            Vector2 nrm{-dir.y, dir.x};
            m += nrm * ((VRand(seed, k++) - 0.5f) * 2 * disp);
            nx.push_back(p);
            nx.push_back(m);
        }
        nx.push_back(pts.back());
        pts.swap(nx);
        disp *= 0.52f;
    }
}

} // namespace

// ---------------------------------------------------------------- Public: looks

uint32_t LookKey(const std::string& name, Type t, MoveKind k) {
    uint32_t h = 2166136261u;
    for (char c : name) { h ^= (unsigned char)c; h *= 16777619u; }
    h ^= (uint32_t)t * 0x9E3779B9u;
    h ^= (uint32_t)k * 0x85EBCA6Bu;
    if (h == 0) h = 1;
    auto& m = Looks();
    if (m.find(h) == m.end()) m[h] = BuildLook(name, t, k);
    return h;
}

const Look& GetLook(uint32_t key, Type fallback) {
    ReadyTypeLooks();
    if (key) {
        auto& m = Looks();
        auto it = m.find(key);
        if (it != m.end()) return it->second;
    }
    int i = (int)fallback;
    return gTypeLooks[(i >= 0 && i < (int)Type::Count) ? i : 31];
}

Color ImpactColor(Impact im, int w) {
    static const Color pal[][3] = {
        {C(255, 255, 230), C(255, 210, 110), C(240, 120, 50)},   // Pow
        {C(255, 250, 200), C(255, 160, 40), C(220, 50, 20)},     // Fire
        {C(255, 255, 230), C(255, 230, 60), C(120, 200, 255)},   // Spark
        {C(240, 250, 255), C(110, 180, 255), C(40, 90, 220)},    // Splash
        {C(240, 255, 200), C(120, 220, 80), C(50, 150, 40)},     // Leaves
        {C(255, 255, 255), C(170, 230, 255), C(90, 160, 240)},   // Frost
        {C(240, 220, 190), C(170, 140, 100), C(110, 85, 60)},    // Debris
        {C(240, 200, 255), C(180, 80, 220), C(100, 30, 140)},    // Poison
        {C(255, 230, 250), C(250, 110, 200), C(170, 50, 170)},   // Psy
        {C(220, 190, 255), C(130, 70, 200), C(50, 20, 80)},      // Shadow
        {C(230, 220, 255), C(130, 90, 255), C(60, 30, 180)},     // Dragon
        {C(255, 245, 255), C(255, 160, 220), C(220, 90, 180)},   // Fairy
        {C(255, 255, 255), C(200, 215, 235), C(120, 135, 160)},  // Metal
        {C(255, 255, 255), C(200, 225, 255), C(140, 170, 220)},  // Wind
        {C(240, 255, 190), C(170, 210, 60), C(90, 130, 30)},     // Bug
        {C(240, 220, 170), C(200, 160, 100), C(140, 100, 60)},   // Sand
        {C(255, 255, 255), C(220, 220, 255), C(160, 160, 220)},  // Sound
    };
    int i = (int)im;
    if (i < 0 || i > 16) i = 0;
    return pal[i][std::max(0, std::min(2, w))];
}

bool PKAdditive(PK k) {
    return k == PK::Glow || k == PK::Spark || k == PK::Flame || k == PK::Ember || k == PK::Wisp;
}

// ---------------------------------------------------------------- Load

void LoadVfx() {
    if (gLoaded) return;
    MakeTex(T_BEAM, Procedural(4, 32, T_BEAM), true);
    MakeTex(T_GLOW, Procedural(128, 128, T_GLOW), true);
    MakeTex(T_SMOKE, Procedural(32, 32, T_SMOKE), true);
    MakeTex(T_RING, Procedural(128, 128, T_RING), true);
    MakeTex(T_STREAK, Procedural(96, 24, T_STREAK), true);
    MakeTex(T_STAR, Procedural(13, 13, T_STAR), false);
    MakeTex(T_BUBBLE, Procedural(24, 24, T_BUBBLE), true);
    MakeTex(T_MOON, Procedural(16, 16, T_MOON), false);
    MakeTex(T_CRESCENT, Procedural(32, 32, T_CRESCENT), true);
    MakeTex(T_HEART, Procedural(11, 10, T_HEART), false);
    MakeTex(T_COIN, Procedural(9, 9, T_COIN), false);
    MakeTex(T_SEED, Procedural(5, 7, T_SEED), false);
    MakeTex(T_EGG, Procedural(9, 11, T_EGG), false);
    MakeTex(T_PETAL, Procedural(7, 5, T_PETAL), false);
    MakeTex(T_LEAF, FromArt({
        "..####.....",
        ".#oo++##...",
        "#ooooo++##.",
        "#ddddddddd#",
        "#oooooooo#.",
        ".#oooo##...",
        "..####.....",
    }), false);
    MakeTex(T_SHARD, FromArt({
        "...###......",
        ".##ww+###...",
        "#ww+++oo####",
        ".##oooddd##.",
        "...####.....",
    }), false);
    MakeTex(T_ROCK, FromArt({
        "..#####.",
        ".#++ooo#",
        "#+oooood#",
        "#ooodood#",
        "#oddoood#",
        "#oodddd##",
        ".#dd###..",
        "..##.....",
    }), false);
    MakeTex(T_FEATHER, FromArt({
        "...#####..",
        ".##++++o#.",
        "#dddddddd+",
        ".#oooooo#.",
        "...#####..",
    }), false);
    MakeTex(T_FIST, FromArt({
        "..#######..",
        ".#ooo+oo+#.",
        "#o#o#o#o++#",
        "#o#o#o#oo+#",
        "#ooooooooo#",
        "#odddoooo##",
        ".#oooooo#..",
        "..#ooo##...",
        "..#####....",
    }), false);
    MakeTex(T_BOOT, FromArt({
        "..####....",
        "..#++#....",
        "..#oo#....",
        "..#oo#....",
        "..#oo####.",
        ".#ooo+++o#",
        "#oooooooo#",
        "##########",
    }), false);
    MakeTex(T_JAW, FromArt({
        ".############.",
        "#oooo++++oooo#",
        "#dddddddddddd#",
        "#w##ww##ww##w#",
        ".w..ww..ww..w.",
        "....w....w....",
    }), false);
    MakeTex(T_NOTE, FromArt({
        "...##..",
        "...#w#.",
        "...#.w#",
        "...#..#",
        "...#...",
        ".###...",
        "#www#..",
        "#www#..",
        ".###...",
    }), false);
    MakeTex(T_BONE, FromArt({
        ".##.....##.",
        "#ww#####ww#",
        ".#ooooooo#.",
        "#ww#####ww#",
        ".##.....##.",
    }), false);
    gLoaded = true;
}

void UnloadVfx() {
    if (!gLoaded) return;
    for (auto& t : gTex) UnloadTexture(t);
    gLoaded = false;
}

// ---------------------------------------------------------------- Primitive helpers

uint32_t VHash(uint32_t a, uint32_t b) {
    uint32_t x = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    x ^= x >> 16; x *= 0x85EBCA6Bu; x ^= x >> 13; x *= 0xC2B2AE35u; x ^= x >> 16;
    return x;
}
float VRand(uint32_t seed, uint32_t i) { return (VHash(seed, i) & 0xffffff) / (float)0x1000000; }

void VAdd() { BeginBlendMode(BLEND_ADDITIVE); }
void VNorm() { EndBlendMode(); }

// Saturated colours get a normal-blended body under the additive light, so effects still
// read on bright arena floors (pure additive light washes out to nothing on sand).
static bool Saturated(Color c) { return std::max(c.r, std::max(c.g, c.b)) - std::min(c.r, std::min(c.g, c.b)) > 50; }
static Color Deepen(Color c) {
    int mx = std::max(c.r, std::max(c.g, c.b)), mn = std::min(c.r, std::min(c.g, c.b));
    auto f = [&](int v) { return std::max(0, std::min(255, mn + (int)((v - mn) * 1.35f) - (mx - v) / 3)); };
    return C(f(c.r), f(c.g), f(c.b), c.a);
}
static Color BodyOf(Color c) { return Saturated(c) ? Deepen(c) : C(80, 80, 100, c.a); }
void VGlow(Vector2 p, float r, Color c, float alpha) {
    if (alpha <= 0.003f || r <= 0.05f) return;
    if (Saturated(c)) {
        VNorm();
        Tex(T_GLOW, p, r * 1.5f, 0, Deepen(c), Clamp01(alpha) * 0.6f);
    }
    VAdd();
    Tex(T_GLOW, p, r * 2, 0, c, Clamp01(alpha));
}
// Soft-edged line (current blend mode)
static void VLine(Vector2 a, Vector2 b, float w, Color c, float alpha) {
    float len = Dist(a, b);
    if (len < 0.1f || alpha <= 0.003f || !gLoaded) return;
    const Texture2D& t = gTex[T_BEAM];
    DrawTexturePro(t, Rectangle{0, 0, (float)t.width, (float)t.height}, Rectangle{a.x, a.y, len, w}, Vector2{0, w / 2},
                   std::atan2(b.y - a.y, b.x - a.x) * 57.2958f, Fade(c, Clamp01(alpha)));
}
void VSoft(Vector2 p, float r, Color c, float alpha) {
    if (alpha <= 0.003f || r <= 0.05f) return;
    VNorm();
    Tex(T_GLOW, p, r * 2, 0, c, Clamp01(alpha));
}

void VShadow(Vector2 p, float rx, float ry, float alpha) {
    if (alpha <= 0.003f || !gLoaded) return;
    VNorm();
    const Texture2D& t = gTex[T_GLOW];
    Rectangle src{0, 0, (float)t.width, (float)t.height};
    DrawTexturePro(t, src, Rectangle{p.x, p.y, rx * 2.6f, ry * 2.6f}, Vector2{rx * 1.3f, ry * 1.3f}, 0, Fade(Color{10, 8, 20, 255}, alpha * 0.55f));
    DrawTexturePro(t, src, Rectangle{p.x, p.y, rx * 1.5f, ry * 1.5f}, Vector2{rx * 0.75f, ry * 0.75f}, 0, Fade(Color{10, 8, 20, 255}, alpha * 0.6f));
}

void VRingE(Vector2 c, float rx, float ry, float thick, Color col, int segs) {
    if (rx <= 0.2f || col.a == 0) return;
    Vector2 prev{c.x + rx, c.y};
    for (int i = 1; i <= segs; i++) {
        float a = i * 2 * PI_F / segs;
        Vector2 p{c.x + std::cos(a) * rx, c.y + std::sin(a) * ry};
        DrawLineEx(prev, p, thick, col);
        prev = p;
    }
}

void VBolt(Vector2 a, Vector2 b, uint32_t seed, float width, Color c, float alpha, int branches) {
    if (alpha <= 0.01f) return;
    static std::vector<Vector2> pts;
    float len = Dist(a, b);
    int depth = len > 120 ? 5 : (len > 40 ? 4 : 3);
    BoltPoints(pts, a, b, seed, depth);
    std::vector<Vector2> local = pts;
    if (Saturated(c)) {
        VNorm();
        for (size_t i = 1; i < local.size(); i++) DrawLineEx(local[i - 1], local[i], width * 2.2f, Fade(Deepen(c), 0.45f * alpha));
    }
    VAdd();
    for (size_t i = 1; i < local.size(); i++) DrawLineEx(local[i - 1], local[i], width * 3.2f, Fade(c, 0.22f * alpha));
    for (size_t i = 1; i < local.size(); i++) DrawLineEx(local[i - 1], local[i], width * 1.5f, Fade(c, 0.85f * alpha));
    for (size_t i = 1; i < local.size(); i++) DrawLineEx(local[i - 1], local[i], std::max(0.4f, width * 0.5f), Fade(WHITE, alpha));
    VGlow(a, width * 3, c, 0.6f * alpha);
    VGlow(b, width * 4, c, 0.8f * alpha);
    for (int k = 0; k < branches; k++) {
        size_t n = local.size();
        if (n < 4) break;
        size_t i = n / 5 + (size_t)(VRand(seed, 900 + k) * (n * 3 / 5));
        Vector2 from = local[std::min(i, n - 1)];
        Vector2 dir = SafeNorm(b - a);
        float side = VRand(seed, 950 + k) < 0.5f ? -1.0f : 1.0f;
        Vector2 bd = Rot(dir, side * (0.45f + VRand(seed, 970 + k) * 0.6f));
        float bl = len * (0.15f + VRand(seed, 990 + k) * 0.22f);
        VBolt(from, from + bd * bl, VHash(seed, 77 + k), width * 0.5f, c, alpha * 0.8f, 0);
    }
}

void VImpactStar(Vector2 p, float r, float t, Color c, uint32_t seed) {
    if (t < 0 || t > 1) return;
    float a = 1 - t;
    float e = EaseOut(t * 1.6f);
    int n = 8;
    r *= 1.3f;
    {   // a shock ring and a hot flash behind the spikes
        float rr = r * (0.4f + 1.1f * EaseOut(t * 1.2f));
        VNorm();
        VRingE(p, rr, rr * 0.8f, 2.2f * a + 0.4f, Fade(BodyOf(c), 0.5f * a), 28);
        VAdd();
        VRingE(p, rr, rr * 0.8f, 1.2f * a + 0.3f, Fade(c, 0.9f * a), 28);
        if (t < 0.3f) VGlow(p, r * (1.4f + t * 3), WHITE, (1 - t / 0.3f) * 0.75f);
    }
    for (int pass = 0; pass < 2; pass++) {
        if (pass == 0) VNorm(); else VAdd();
        for (int i = 0; i < n; i++) {
            float ang = (i + VRand(seed, i) * 0.6f) * 2 * PI_F / n;
            float L = r * (0.7f + VRand(seed, 20 + i) * 0.7f) * e;
            Vector2 d{std::cos(ang), std::sin(ang) * 0.8f};
            Vector2 q = p + d * L, s = p + d * (L * 0.3f);
            if (pass == 0) { VLine(s, q, 3.4f * a, BodyOf(c), (Saturated(c) ? 0.85f : 0.5f) * a); continue; }
            VLine(s, q, 2.6f * a, c, 0.9f * a);
            VLine(s, q, 1.0f * a, WHITE, a);
        }
    }
    VGlow(p, r * 1.2f * (1 - t * 0.5f), c, a * 0.8f);
    VGlow(p, r * 0.5f * (1 - t), WHITE, a);
}

// ---------------------------------------------------------------- Explosion

void VExplosion(Vector2 p, float r, float t, Impact im, uint32_t seed) {
    if (t < 0 || t > 1) return;
    Color c0 = ImpactColor(im, 0), c1 = ImpactColor(im, 1), c2 = ImpactColor(im, 2);
    float e = EaseOut(t * 1.3f);
    float body = t < 0.45f ? 1.0f : 1.0f - (t - 0.45f) / 0.55f;
    // Ground shock ring
    VNorm();
    float rr = r * (0.3f + 1.2f * e);
    VRingE({p.x, p.y + r * 0.25f}, rr, rr * 0.55f, 2.5f * (1 - t) + 0.5f, Fade(c0, 0.7f * (1 - t)));
    float R = r * (0.35f + 0.65f * e);
    VNorm();   // solid body so it reads on bright floors
    for (int k = 0; k < 9; k++) {
        float ang = VRand(seed, k) * 2 * PI_F;
        float dd = R * (0.25f + VRand(seed, 10 + k) * 0.45f) * e;
        Vector2 q = p + Vector2{std::cos(ang) * dd, std::sin(ang) * dd * 0.8f - t * r * 0.25f};
        float lr = R * (0.45f + VRand(seed, 20 + k) * 0.35f);
        Tex(T_GLOW, q, lr * 1.7f, 0, Mix(Deepen(c1), Deepen(c2), t * 1.3f), 0.85f * body);
    }
    VAdd();
    if (t < 0.22f) VGlow(p, r * (1.2f + t * 5), WHITE, (1 - t / 0.22f) * 0.9f);  // flash
    VGlow(p, R * 1.7f, c2, 0.55f * body);
    for (int k = 0; k < 9; k++) {   // billowing lobes
        float ang = VRand(seed, k) * 2 * PI_F;
        float dd = R * (0.25f + VRand(seed, 10 + k) * 0.45f) * e;
        Vector2 q = p + Vector2{std::cos(ang) * dd, std::sin(ang) * dd * 0.8f - t * r * 0.25f};
        float lr = R * (0.45f + VRand(seed, 20 + k) * 0.35f);
        VGlow(q, lr, Mix(c1, c2, t * 1.4f), 0.75f * body);
    }
    VGlow(p, R * 0.95f * (1 - t * 0.6f), c1, 0.9f * body);
    VGlow(p, R * 0.6f * (1 - t), c0, 1.0f * (1 - t));
    VGlow(p, R * 0.3f * (1 - t), WHITE, 1.0f * (1 - t));
    VNorm();
}

// ---------------------------------------------------------------- Beam

void VBeam(Vector2 a, Vector2 b, float width, float t, float time, const Look& L, uint32_t seed) {
    float env = t < 0.12f ? t / 0.12f : 1.0f - (t - 0.12f) / 0.88f;
    env = Clamp01(env);
    float w = width * L.beamW * (0.55f + 0.7f * env) + 0.6f;
    Vector2 d = SafeNorm(b - a);
    Vector2 n{-d.y, d.x};
    float len = Dist(a, b);
    Color glow = L.glow, core = L.core;
    float flick = 0.85f + 0.15f * std::sin(time * 60);

    if (L.beam == BeamDeco::Dark) {
        VNorm();
        VLine(a, b, w * 4.5f, glow, 0.6f * env);
        VLine(a, b, w * 2.4f, core, 0.95f * env);
        for (int i = 0; i < 10; i++) {   // writhing shadow
            float u = std::fmod(i / 10.0f + time * 2.2f, 1.0f);
            Vector2 q = a + d * (u * len) + n * (std::sin(time * 14 + i * 2.1f) * w * 1.3f);
            DrawCircleV(q, w * (0.9f + 0.4f * std::sin(i + time * 9)), Fade(core, 0.8f * env));
        }
    } else {
        VNorm();
        VLine(a, b, w * 4.2f, Deepen(glow), 0.55f * env * flick);
        VLine(a, b, w * 2.6f, Deepen(glow), 0.9f * env);
        VAdd();
        VLine(a, b, w * 5.0f, glow, 0.2f * env * flick);
        VLine(a, b, w * 2.0f, core, env);
        VLine(a, b, std::max(0.8f, w * 0.9f), WHITE, env);
    }

    switch (L.beam) {
        case BeamDeco::Fire: {
            VAdd();
            for (int i = 0; i < 18; i++) {
                float u = std::fmod(i / 18.0f + time * 3.0f, 1.0f);
                Vector2 q = a + d * (u * len) + n * (std::sin(time * 23 + i * 1.7f) * w * (0.4f + u));
                VGlow(q, w * (1.1f + u * 1.6f), Mix(C(255, 190, 70), C(220, 40, 10), u), 0.75f * env);
            }
        } break;
        case BeamDeco::Ice: {
            VNorm();
            for (int i = 0; i < 12; i++) {
                float u = (i + 0.5f) / 12.0f;
                float side = (i % 2) ? 1.0f : -1.0f;
                Vector2 q = a + d * (u * len) + n * (side * w * (1.1f + VRand(seed, i) * 0.8f));
                Tex(T_SHARD, q, 8 + VRand(seed, 30 + i) * 5, Deg(d) + side * 50 + VRand(seed, 50 + i) * 30, C(170, 225, 255), env);
            }
            VAdd();
            for (int i = 0; i < 6; i++) {
                float u = std::fmod(VRand(seed, 70 + i) + time * 0.7f, 1.0f);
                VGlow(a + d * (u * len), w * 1.6f, WHITE, 0.6f * env * std::fabs(std::sin(time * 20 + i)));
            }
        } break;
        case BeamDeco::Psy: {
            VNorm();
            for (int i = 0; i < 6; i++) {
                float u = std::fmod(i / 6.0f + time * 2.4f, 1.0f);
                Vector2 q = a + d * (u * len);
                float rr = w * (1.6f + u * 1.5f);
                VRingE(q, rr, rr, 1.2f, Fade(i % 2 ? glow : core, env * (1 - u * 0.5f)), 20);
            }
        } break;
        case BeamDeco::Bubbles: {
            VNorm();
            for (int i = 0; i < 16; i++) {
                float u = std::fmod(i / 16.0f + time * 1.5f, 1.0f);
                Vector2 q = a + d * (u * len) + n * (std::sin(i * 2.7f + time * 6) * w * 1.6f);
                Tex(T_BUBBLE, q, w * (1.6f + VRand(seed, i) * 1.6f), 0, C(120, 190, 255), env);
            }
        } break;
        case BeamDeco::Solar: {
            VAdd();
            VLine(a, b, w * 7.0f, glow, 0.3f * env);
            for (int i = 0; i < 8; i++) {  // rays from the mouth
                float ang = i * PI_F / 4 + time * 1.5f;
                Vector2 rd{std::cos(ang), std::sin(ang)};
                DrawLineEx(a, a + rd * (w * 5), 1.2f, Fade(glow, 0.7f * env));
            }
            for (int i = 0; i < 10; i++) {
                float u = std::fmod(VRand(seed, i) + time * 1.2f, 1.0f);
                Vector2 q = a + d * (u * len) + n * ((VRand(seed, 40 + i) - 0.5f) * w * 3);
                VGlow(q, 2.2f, C(255, 255, 200), env * std::fabs(std::sin(time * 17 + i)));
            }
        } break;
        case BeamDeco::Hyper: {
            VAdd();
            VLine(a, b, w * 6.0f, glow, 0.14f * env);
            for (int s2 = -1; s2 <= 1; s2 += 2) {
                Vector2 prev = a + n * (s2 * w * 1.7f);
                for (int i = 1; i <= 20; i++) {
                    float u = i / 20.0f;
                    Vector2 q = a + d * (u * len) + n * (s2 * w * (1.7f + 0.5f * std::sin(u * 30 - time * 40)));
                    DrawLineEx(prev, q, 1.0f, Fade(core, 0.8f * env));
                    prev = q;
                }
            }
            VNorm();
            for (int i = 0; i < 3; i++) {
                float u = std::fmod(i / 3.0f + time * 3.0f, 1.0f);
                float rr = w * (2.0f + u * 2.0f);
                VRingE(a + d * (u * len * 0.3f), rr * 0.5f, rr, 1.2f, Fade(WHITE, env * (1 - u)), 18);
            }
        } break;
        case BeamDeco::Wind: {
            VAdd();
            for (int s2 = 0; s2 < 2; s2++) {
                Vector2 prev = a;
                for (int i = 1; i <= 28; i++) {
                    float u = i / 28.0f;
                    float ph = u * 24 - time * 35 + s2 * PI_F;
                    Vector2 q = a + d * (u * len) + n * (std::sin(ph) * w * (1.2f + u * 1.4f));
                    DrawLineEx(prev, q, 1.2f, Fade(WHITE, 0.8f * env));
                    prev = q;
                }
            }
        } break;
        case BeamDeco::Electric: {
            uint32_t fr = (uint32_t)(time * 30);
            VBolt(a, b, VHash(seed, fr), w * 0.6f, glow, env, 3);
            VBolt(a, b, VHash(seed + 99, fr), w * 0.35f, core, env * 0.7f, 1);
        } break;
        case BeamDeco::Rainbow: {
            VAdd();
            for (int k = 0; k < 3; k++) {
                Color hc = ColorFromHSV(std::fmod(time * 360 + k * 120, 360.0f), 0.6f, 1.0f);
                Vector2 off = n * ((k - 1) * w * 0.8f);
                VNorm();
                VLine(a + off, b + off, w * 1.1f, hc, 0.8f * env);
            }
        } break;
        case BeamDeco::Water: {
            VNorm();
            for (int i = 0; i < 20; i++) {
                float u = std::fmod(i / 20.0f + time * 2.6f, 1.0f);
                float ph = u * 18 - time * 22;
                Vector2 q = a + d * (u * len) + n * (std::sin(ph + i) * w * 1.4f);
                DrawCircleV(q, w * 0.55f, Fade(C(90, 160, 255), 0.85f * env));
                DrawCircleV(q - Vector2{w * 0.15f, w * 0.15f}, w * 0.25f, Fade(WHITE, 0.9f * env));
            }
        } break;
        case BeamDeco::Gem: {
            VNorm();
            for (int i = 0; i < 10; i++) {
                float u = std::fmod(VRand(seed, i) + time * 0.8f, 1.0f);
                Vector2 q = a + d * (u * len) + n * ((VRand(seed, 20 + i) - 0.5f) * w * 3);
                float s = 1.2f + 1.8f * std::fabs(std::sin(time * 13 + i));
                DrawPoly(q, 4, s, 45, Fade(WHITE, env));
            }
        } break;
        case BeamDeco::Drain: {
            VAdd();
            for (int i = 0; i < 10; i++) {
                float u = 1.0f - std::fmod(i / 10.0f + time * 1.6f, 1.0f);
                Vector2 q = a + d * (u * len) + n * (std::sin(i * 3.1f + time * 8) * w * 1.5f);
                VGlow(q, w * 1.4f, C(160, 255, 140), env);
            }
        } break;
        default: break;
    }
    VGlow(a, w * 3.4f, glow, 0.9f * env);
    VGlow(a, w * 1.6f, WHITE, env);
    VGlow(b, w * 4.0f, glow, 0.9f * env);
    VGlow(b, w * 1.8f, core, env);
    VNorm();
}

// ---------------------------------------------------------------- Projectiles

void VProjectile(Vector2 sp, Vector2 vel, float radius, float spin, float time, const Look& L, uint32_t seed) {
    float r = std::max(3.0f, radius * 1.35f * L.size);
    Vector2 d = SafeNorm(vel, {1, 0});
    Vector2 n{-d.y, d.x};
    Color core = L.core, glow = L.glow;
    float spd = Len(vel);
    bool streaky = spd > 20 && L.shape != Shape::Tornado && L.shape != Shape::Web && L.shape != Shape::Comet && L.shape != Shape::Fireball;
    if (streaky) {   // a motion streak: the shot reads as fast, not as a dot
        float sl = std::min(r * 7.0f, std::max(r * 2.5f, spd * 0.045f));
        auto streak = [&](float len, float w, Color c, float al) {
            const Texture2D& t = gTex[T_STREAK];
            Vector2 mid = sp - d * (len * 0.5f - r * 0.3f);
            DrawTexturePro(t, Rectangle{0, 0, (float)t.width, (float)t.height}, Rectangle{mid.x, mid.y, len, w}, Vector2{len / 2, w / 2}, Deg(d), Fade(c, al));
        };
        if (gLoaded) {
            VNorm();
            if (Saturated(glow)) streak(sl, r * 1.8f, Deepen(glow), 0.4f);
            VAdd();
            streak(sl, r * 2.6f, glow, 0.5f);
            streak(sl * 0.7f, r * 1.0f, core, 0.6f);
            VNorm();
        }
    }
    switch (L.shape) {
        case Shape::Orb: {
            VGlow(sp, r * 3.2f, glow, 0.45f);
            VGlow(sp, r * 1.7f, glow, 0.9f);
            VNorm();
            DrawCircleV(sp, r, core);
            DrawCircleV(sp - Vector2{r * 0.3f, r * 0.3f}, r * 0.4f, Fade(WHITE, 0.85f));
        } break;
        case Shape::Fireball: case Shape::Comet: {
            bool comet = L.shape == Shape::Comet;
            Vector2 back = comet ? Vector2{0.35f, -1} : -d;
            back = SafeNorm(back);
            int tail = comet ? 10 : 7;
            VGlow(sp, r * 3.8f, glow, 0.4f);
            for (int i = tail; i >= 1; i--) {
                float u = (float)i / tail;
                Vector2 q = sp + back * (i * r * (comet ? 0.9f : 0.65f)) + n * (std::sin(time * 30 + i * 1.3f) * r * 0.25f * u);
                VGlow(q, r * (1.5f - u * 0.9f), Mix(core, glow, u * 1.3f), 0.85f - u * 0.45f);
            }
            VGlow(sp, r * 1.6f, glow, 1.0f);
            VGlow(sp, r * 1.1f, core, 1.0f);
            VGlow(sp, r * 0.6f, WHITE, 1.0f);
            VNorm();
        } break;
        case Shape::ElecOrb: {
            VGlow(sp, r * 3.4f, glow, 0.5f);
            VGlow(sp, r * 1.6f, core, 1.0f);
            uint32_t fr = (uint32_t)(time * 24);
            for (int k = 0; k < 3; k++) {
                float ang = VRand(seed + fr, k) * 2 * PI_F;
                Vector2 e = sp + Vector2{std::cos(ang), std::sin(ang)} * (r * (2.0f + VRand(seed + fr, 10 + k) * 1.6f));
                VBolt(sp, e, VHash(seed, fr * 7 + k), 0.55f, glow, 0.95f, 0);
            }
            VNorm();
            DrawCircleV(sp, r * 0.55f, WHITE);
        } break;
        case Shape::Shard: {
            VGlow(sp, r * 2.4f, glow, 0.35f);
            VNorm();
            Tex(T_SHARD, sp, r * 3.8f, Deg(d), Mix(core, glow, 0.35f), 1);
        } break;
        case Shape::Leaf: {
            VNorm();
            Tex(T_LEAF, sp, r * 3.8f, spin * 57.3f, core, 1);
        } break;
        case Shape::Rock: {
            VNorm();
            Tex(T_ROCK, sp, r * 3.0f, spin * 40, core, 1);
        } break;
        case Shape::Bubble: {
            VNorm();
            float wob = 1 + 0.12f * std::sin(time * 14 + seed % 7);
            Tex(T_BUBBLE, sp, r * 2.6f, 0, core, 0.95f, wob, 2 - wob);
        } break;
        case Shape::Star: {
            VGlow(sp, r * 3.0f, glow, 0.55f);
            VNorm();
            Tex(T_STAR, sp, r * 3.6f, spin * 80, core, 1);
        } break;
        case Shape::Needle: {
            VNorm();
            DrawLineEx(sp - d * (r * 2.6f), sp + d * (r * 1.3f), 1.1f, ColorBrightness(glow, -0.2f));
            DrawLineEx(sp - d * (r * 2.0f), sp + d * (r * 1.3f), 0.5f, core);
            VGlow(sp + d * r, r * 1.4f, glow, 0.8f);
            VNorm();
        } break;
        case Shape::Crescent: {
            VGlow(sp, r * 2.8f, glow, 0.4f);
            for (int k = 2; k >= 0; k--) {
                VAdd();
                if (k == 0) { VNorm(); Tex(T_CRESCENT, sp, r * 4.4f, Deg(d) + 180, Deepen(glow), 0.8f); VAdd(); }
                Tex(T_CRESCENT, sp - d * (k * r * 1.1f), r * 4.0f, Deg(d) + 180, k ? glow : core, k ? 0.35f / k : 1.0f);
            }
            VNorm();
        } break;
        case Shape::Droplet: {
            VGlow(sp, r * 2.6f, glow, 0.45f);
            VNorm();
            Color deep = Deepen(glow);
            DrawTriangle(sp + n * (r * 1.05f), sp - d * (r * 3.0f), sp - n * (r * 1.05f), Fade(deep, 0.9f));
            DrawTriangle(sp - n * (r * 1.05f), sp - d * (r * 3.0f), sp + n * (r * 1.05f), Fade(deep, 0.9f));
            DrawCircleV(sp, r * 1.08f, deep);
            DrawTriangle(sp + n * (r * 0.7f), sp - d * (r * 2.3f), sp - n * (r * 0.7f), Fade(glow, 0.95f));
            DrawTriangle(sp - n * (r * 0.7f), sp - d * (r * 2.3f), sp + n * (r * 0.7f), Fade(glow, 0.95f));
            DrawCircleV(sp, r * 0.85f, glow);
            DrawCircleV(sp + d * (r * 0.1f), r * 0.55f, core);
            DrawCircleV(sp + Vector2{-r * 0.3f, -r * 0.38f}, r * 0.28f, WHITE);
            for (int k = 1; k <= 3; k++) {   // trailing droplets
                float u = std::fmod(time * 6 + k * 0.33f + (seed % 7) * 0.1f, 1.0f);
                Vector2 q = sp - d * (r * (2.6f + k * 1.4f + u)) + n * (std::sin(time * 25 + k * 2) * r * 0.5f);
                DrawCircleV(q, r * (0.42f - k * 0.08f), Fade(glow, 0.9f - k * 0.2f));
            }
        } break;
        case Shape::ShadowOrb: {
            VGlow(sp, r * 3.4f, glow, 0.55f);
            for (int k = 0; k < 4; k++) {
                float ang = time * 7 + k * PI_F / 2;
                VGlow(sp + Vector2{std::cos(ang), std::sin(ang) * 0.8f} * (r * 1.3f), r * 0.8f, glow, 0.8f);
            }
            VNorm();
            DrawCircleV(sp, r * 1.15f, ColorBrightness(core, -0.5f));
            DrawCircleV(sp, r * 0.85f, core);
            VGlow(sp - Vector2{r * 0.25f, r * 0.25f}, r * 0.6f, glow, 0.6f);
            VNorm();
        } break;
        case Shape::Moon: {
            VGlow(sp, r * 3.6f, glow, 0.55f);
            VNorm();
            Tex(T_MOON, sp, r * 2.6f, 0, core, 1);
            VGlow(sp, r * 1.2f, WHITE, 0.4f);
            VNorm();
        } break;
        case Shape::Sludge: {   // a glossy, wobbling glob that drips as it flies
            r *= 1.15f;
            VGlow(sp, r * 2.4f, glow, 0.35f);
            VNorm();
            Color dk = Deepen(ColorBrightness(core, -0.35f)), md = core, lt = ColorBrightness(core, 0.35f);
            for (int k = 0; k < 5; k++) {
                Vector2 o{std::sin(time * 11 + k * 1.7f) * r * 0.32f, std::cos(time * 9 + k * 2.3f) * r * 0.26f};
                DrawCircleV(sp + o + Vector2{0.4f, 0.6f}, r * 0.86f, dk);
            }
            for (int k = 0; k < 5; k++) {
                Vector2 o{std::sin(time * 11 + k * 1.7f) * r * 0.28f, std::cos(time * 9 + k * 2.3f) * r * 0.22f};
                DrawCircleV(sp + o, r * 0.68f, md);
            }
            DrawCircleV(sp + Vector2{-r * 0.2f, -r * 0.25f}, r * 0.45f, lt);
            DrawCircleV(sp + Vector2{-r * 0.36f, -r * 0.4f}, r * 0.18f, Fade(WHITE, 0.9f));
            for (int k = 1; k <= 3; k++) {   // drips trailing behind
                float u = std::fmod(time * 5 + k * 0.33f + (seed % 5) * 0.2f, 1.0f);
                Vector2 q = sp - d * (r * (1.2f + k * 1.1f)) + Vector2{0, u * r * 0.8f};
                DrawCircleV(q, r * (0.36f - k * 0.07f), dk);
                DrawCircleV(q - Vector2{r * 0.06f, r * 0.06f}, r * (0.26f - k * 0.05f), md);
            }
        } break;
        case Shape::Seed: {
            VNorm();
            Tex(T_SEED, sp, r * 2.4f, Deg(d) + 90, C(200, 170, 110), 1);
        } break;
        case Shape::SoundRing: {
            VNorm();
            for (int k = 0; k < 3; k++) {
                float R = r * (1.4f + k * 1.0f);
                Vector2 c = sp - d * (r * 2.2f + k * 0.5f);
                float base = std::atan2(d.y, d.x);
                Vector2 prev = c + Vector2{std::cos(base - 0.9f), std::sin(base - 0.9f)} * R;
                for (int i = 1; i <= 8; i++) {
                    float a2 = base - 0.9f + 1.8f * i / 8;
                    Vector2 q = c + Vector2{std::cos(a2), std::sin(a2)} * R;
                    DrawLineEx(prev, q, 1.1f, Fade(k ? glow : core, 1.0f - k * 0.28f));
                    prev = q;
                }
            }
        } break;
        case Shape::Wisp: {
            Vector2 w{std::sin(time * 9 + seed % 5) * r * 0.4f, std::cos(time * 7) * r * 0.3f};
            VGlow(sp + w, r * 2.8f, glow, 0.55f);
            for (int k = 1; k <= 3; k++) VGlow(sp + w + Vector2{std::sin(time * 13 + k) * r * 0.3f, -k * r * 0.7f}, r * (1.2f - k * 0.25f), glow, 0.6f);
            VGlow(sp + w, r * 1.2f, core, 1);
            VNorm();
        } break;
        case Shape::Petal: {
            VNorm();
            Tex(T_PETAL, sp, r * 2.4f, spin * 70, core, 1);
        } break;
        case Shape::Bone: {
            VNorm();
            Tex(T_BONE, sp, r * 3.4f, spin * 85, C(245, 240, 225), 1);
        } break;
        case Shape::Web: {
            VNorm();
            for (int k = 0; k < 4; k++) {
                float a2 = spin * 0.5f + k * PI_F / 4;
                Vector2 o{std::cos(a2) * r * 1.8f, std::sin(a2) * r * 1.8f};
                DrawLineEx(sp - o, sp + o, 0.6f, Fade(WHITE, 0.9f));
            }
            VRingE(sp, r * 1.1f, r * 1.1f, 0.5f, Fade(WHITE, 0.8f), 12);
            DrawLineEx(sp - d * (r * 5), sp, 0.4f, Fade(WHITE, 0.5f));
        } break;
        case Shape::Coin: {
            VGlow(sp, r * 2.2f, C(255, 210, 60), 0.4f);
            VNorm();
            Tex(T_COIN, sp, r * 2.4f, 0, C(255, 215, 70), 1, std::max(0.2f, std::fabs(std::cos(spin))), 1);
        } break;
        case Shape::Egg: {
            VNorm();
            Tex(T_EGG, sp, r * 2.6f, std::sin(spin) * 25, C(255, 250, 235), 1);
        } break;
        case Shape::Tornado: {
            VNorm();
            r *= 1.7f;
            for (int k = 0; k < 6; k++) {
                float u = k / 5.0f;
                Vector2 c{sp.x + std::sin(time * 12 + k) * r * 0.4f, sp.y + r * 1.4f - k * r * 0.65f};
                float rx = r * (0.5f + u * 1.3f);
                VRingE(c, rx, rx * 0.32f, 1.0f, Fade(k % 2 ? glow : core, 0.9f - u * 0.3f), 16);
            }
        } break;
        case Shape::Triangle: {
            float a2 = spin * 2;
            Vector2 v[3];
            for (int k = 0; k < 3; k++) v[k] = sp + Vector2{std::cos(a2 + k * 2.094f), std::sin(a2 + k * 2.094f)} * (r * 2.4f);
            VNorm();
            DrawTriangle(v[0], v[2], v[1], Fade(WHITE, 0.6f));
            DrawTriangle(v[0], v[1], v[2], Fade(WHITE, 0.6f));
            VGlow(v[0], r * 1.3f, C(255, 90, 40), 1);
            VGlow(v[1], r * 1.3f, C(80, 200, 255), 1);
            VGlow(v[2], r * 1.3f, C(255, 230, 50), 1);
            VNorm();
        } break;
        case Shape::Heart: {
            VGlow(sp, r * 2.6f, glow, 0.5f);
            VNorm();
            float pulse = 1 + 0.15f * std::sin(time * 16);
            Tex(T_HEART, sp, r * 2.6f * pulse, 0, core, 1);
        } break;
        case Shape::Gem: {
            VGlow(sp, r * 2.4f, glow, 0.45f);
            VNorm();
            DrawPoly(sp, 4, r * 1.5f, spin * 60, ColorBrightness(glow, -0.3f));
            DrawPoly(sp, 4, r * 1.1f, spin * 60, core);
            DrawPoly(sp - Vector2{r * 0.25f, r * 0.25f}, 4, r * 0.4f, spin * 60, WHITE);
        } break;
        case Shape::Note: {
            VGlow(sp, r * 2.2f, glow, 0.4f);
            VNorm();
            Tex(T_NOTE, sp + Vector2{0, std::sin(time * 12) * r * 0.4f}, r * 3.0f, 0, core, 1);
        } break;
        case Shape::Snow: {
            VGlow(sp, r * 2.6f, C(200, 235, 255), 0.5f);
            VNorm();
            for (int k = 0; k < 3; k++) {
                float a2 = spin + k * PI_F / 3;
                Vector2 o{std::cos(a2) * r * 1.5f, std::sin(a2) * r * 1.5f};
                DrawLineEx(sp - o, sp + o, 0.8f, WHITE);
            }
        } break;
    }
    VNorm();
}

// ---------------------------------------------------------------- Particles

void VParticleBody(PK kind, Vector2 sp, Vector2 sv, float size, float rot, float lf, Color c, float time) {
    float a = Clamp01(lf * 1.6f);
    switch (kind) {
        case PK::Glow: if (Saturated(c)) Tex(T_GLOW, sp, size * 2.6f, 0, Deepen(c), a * 0.55f); break;
        case PK::Ember: if (Saturated(c)) Tex(T_GLOW, sp, size * 2.0f, 0, Deepen(c), a * 0.7f); break;
        case PK::Spark: {
            float sl = Len(sv);
            Vector2 d = sl > 0.01f ? sv * (1 / sl) : Vector2{1, 0};
            float L = std::max(1.0f, std::min(9.0f, sl * 0.035f)) * size * 0.7f;
            if (Saturated(c)) DrawLineEx(sp - d * L, sp, size * 0.9f, Fade(Deepen(c), a * 0.8f));
        } break;
        case PK::Flame: {
            Color fc = lf > 0.55f ? C(255, 150, 40) : Mix(C(120, 30, 20), C(255, 150, 40), lf / 0.55f);
            fc = Mix(fc, Deepen(c), 0.3f);
            float k = 0.45f + lf * 0.7f;
            Tex(T_GLOW, sp, size * 3.4f * k, 0, fc, a * 0.75f);
        } break;
        case PK::Wisp: Tex(T_GLOW, sp + Vector2{std::sin(time * 8 + rot) * 1.5f, 0}, size * 3.0f, 0, Deepen(c), a * 0.6f); break;
        default: break;
    }
}

void VParticle(PK kind, Vector2 sp, Vector2 sv, float size, float rot, float lf, Color c, float time) {
    float a = Clamp01(lf * 1.6f);
    switch (kind) {
        case PK::Pixel: DrawRectangleV(sp, Vector2{size, size}, Fade(c, a)); break;
        case PK::Glow: Tex(T_GLOW, sp, size * 4.0f, 0, c, a * 0.95f); break;
        case PK::Spark: {
            float sl = Len(sv);
            Vector2 d = sl > 0.01f ? sv * (1 / sl) : Vector2{1, 0};
            float L = std::max(1.0f, std::min(9.0f, sl * 0.035f)) * size * 0.7f;
            DrawLineEx(sp - d * L, sp, size * 0.6f, Fade(c, a));
            DrawLineEx(sp - d * (L * 0.5f), sp, size * 0.3f, Fade(WHITE, a));
        } break;
        case PK::Smoke: Tex(T_SMOKE, sp, size * 2.0f, rot * 57.3f, c, a * 0.55f); break;
        case PK::Flame: {
            Color fc = lf > 0.65f ? Mix(C(255, 200, 90), C(255, 245, 190), (lf - 0.65f) / 0.35f)
                                  : Mix(C(170, 30, 15), C(255, 200, 90), lf / 0.65f);
            fc = Mix(fc, c, 0.25f);
            float k = 0.45f + lf * 0.7f;
            Tex(T_GLOW, sp, size * 3.6f * k, 0, Mix(fc, C(255, 90, 30), 0.35f), a * 0.55f);
            Tex(T_GLOW, sp, size * 1.4f * k, 0, C(255, 230, 190), a * lf * 0.8f);
        } break;
        case PK::Ember: {
            Tex(T_GLOW, sp, size * 3.0f, 0, c, a);
            Tex(T_GLOW, sp, size * 1.2f, 0, WHITE, a);
        } break;
        case PK::Wisp: {
            Vector2 w{std::sin(time * 8 + rot) * 1.5f, 0};
            Tex(T_GLOW, sp + w, size * 4.0f, 0, c, a * 0.85f);
            Tex(T_GLOW, sp + w, size * 1.6f, 0, WHITE, a * 0.45f);
        } break;
        case PK::Leaf: Tex(T_LEAF, sp, size * 3.2f, rot * 57.3f, c, a); break;
        case PK::Shard: Tex(T_SHARD, sp, size * 3.4f, rot * 57.3f, c, a); break;
        case PK::Debris: Tex(T_ROCK, sp, size * 2.4f, rot * 57.3f, c, a); break;
        case PK::Star: Tex(T_STAR, sp, size * 3.4f, rot * 57.3f, c, a); break;
        case PK::Feather: Tex(T_FEATHER, sp, size * 3.0f, rot * 57.3f, c, a); break;
        case PK::Petal: Tex(T_PETAL, sp, size * 2.4f, rot * 57.3f, c, a); break;
        case PK::Heart: Tex(T_HEART, sp, size * 2.6f, 0, c, a); break;
        case PK::Note: Tex(T_NOTE, sp, size * 3.0f, std::sin(rot) * 15, c, a); break;
        case PK::Coin: Tex(T_COIN, sp, size * 2.2f, 0, c, a, std::max(0.2f, std::fabs(std::cos(rot))), 1); break;
        case PK::Bubble: Tex(T_BUBBLE, sp, size * 2.6f, 0, c, a); break;
        case PK::Drop: {
            DrawCircleV(sp, size * 0.65f, Fade(c, a));
            DrawCircleV(sp - Vector2{size * 0.2f, size * 0.2f}, size * 0.22f, Fade(WHITE, a));
        } break;
        case PK::Ring: VRingE(sp, size, size * 0.75f, 0.9f, Fade(c, a), 18); break;
        case PK::Zzz: DrawText("z", (int)sp.x, (int)sp.y, 10, Fade(c, a)); break;
        case PK::Arrow: {   // a rising chevron (stat up); rot > 3 points down (stat down)
            float s2 = size * 2.2f;
            float dir = rot > 3.0f ? 1.0f : -1.0f;
            Vector2 tip = sp + Vector2{0, dir * s2 * 0.6f};
            Vector2 l = sp + Vector2{-s2, -dir * s2 * 0.4f}, rr = sp + Vector2{s2, -dir * s2 * 0.4f};
            VNorm();
            DrawLineEx(l, tip, size * 1.4f, Fade(Deepen(c), a));
            DrawLineEx(tip, rr, size * 1.4f, Fade(Deepen(c), a));
            VAdd();
            DrawLineEx(l, tip, size * 0.8f, Fade(c, a));
            DrawLineEx(tip, rr, size * 0.8f, Fade(c, a));
            Tex(T_GLOW, tip, s2 * 2.2f, 0, c, a * 0.5f);
            VNorm();
        } break;
        case PK::Plus: {
            float s2 = size * 1.6f;
            VNorm();
            DrawRectangleV(sp - Vector2{s2, s2 * 0.35f}, Vector2{s2 * 2, s2 * 0.7f}, Fade(Deepen(c), a));
            DrawRectangleV(sp - Vector2{s2 * 0.35f, s2}, Vector2{s2 * 0.7f, s2 * 2}, Fade(Deepen(c), a));
            VAdd();
            DrawRectangleV(sp - Vector2{s2 * 0.8f, s2 * 0.18f}, Vector2{s2 * 1.6f, s2 * 0.36f}, Fade(c, a));
            DrawRectangleV(sp - Vector2{s2 * 0.18f, s2 * 0.8f}, Vector2{s2 * 0.36f, s2 * 1.6f}, Fade(c, a));
            Tex(T_GLOW, sp, s2 * 3.0f, 0, c, a * 0.45f);
            VNorm();
        } break;
        case PK::Shield: {   // a hexagon flash (defence up)
            VAdd();
            DrawPolyLinesEx(sp, 6, size * 3.0f, rot * 20, size * 0.5f, Fade(c, a));
            DrawPoly(sp, 6, size * 2.6f, rot * 20, Fade(c, a * 0.18f));
            VNorm();
        } break;
        case PK::Snow: {
            for (int k = 0; k < 3; k++) {
                float a2 = rot + k * PI_F / 3;
                Vector2 o{std::cos(a2) * size, std::sin(a2) * size};
                DrawLineEx(sp - o, sp + o, 0.5f, Fade(c, a));
            }
        } break;
    }
}

// ---------------------------------------------------------------- Melee strikes

void VMelee(Vector2 from, Vector2 to, float t, MeleeAnim anim, const Look& L, uint32_t seed) {
    Vector2 dv = to - from;
    float dist = std::max(4.0f, Len(dv));
    Vector2 d = SafeNorm(dv);
    Vector2 n{-d.y, d.x};
    float fade = t < 0.6f ? 1.0f : 1.0f - (t - 0.6f) / 0.4f;
    Color glow = L.glow, core = L.core;
    switch (anim) {
        case MeleeAnim::Arc: {
            float p = Clamp01(t * 2.4f);
            float base = std::atan2(d.y, d.x / 1.0f);
            float R = dist * 1.05f + 6;
            int N = 18;
            int shown = (int)(N * p);
            for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < shown; i++) {
                if (pass == 0) VNorm(); else VAdd();
                Color gc = pass == 0 ? BodyOf(glow) : glow;
                float u0 = (float)i / N, u1 = (float)(i + 1) / N;
                float a0 = base - 1.15f + 2.3f * u0, a1 = base - 1.15f + 2.3f * u1;
                float th = 2.0f + 7.0f * std::sin(u1 * PI_F) * (0.4f + 0.6f * u1 / std::max(0.05f, p));
                Vector2 o0 = from + Vector2{std::cos(a0), std::sin(a0) * 0.75f} * R, o1 = from + Vector2{std::cos(a1), std::sin(a1) * 0.75f} * R;
                Vector2 i0 = from + Vector2{std::cos(a0), std::sin(a0) * 0.75f} * (R - th), i1 = from + Vector2{std::cos(a1), std::sin(a1) * 0.75f} * (R - th);
                DrawTriangle(o0, i0, o1, Fade(gc, 0.7f * fade));
                DrawTriangle(o0, o1, i0, Fade(gc, 0.7f * fade));
                DrawTriangle(i0, i1, o1, Fade(gc, 0.7f * fade));
                DrawTriangle(i0, o1, i1, Fade(gc, 0.7f * fade));
                if (pass) DrawLineEx(o0, o1, 1.0f, Fade(WHITE, fade));
            }
            VNorm();
        } break;
        case MeleeAnim::Claw: {
            float p = Clamp01(t * 4.5f);
            Vector2 sd = SafeNorm(Rot(d, 1.15f));
            VAdd();
            for (int k = -1; k <= 1; k++) {
                float delay = (k + 1) * 0.06f;
                float pk = Clamp01((t - delay) * 5.0f);
                if (pk <= 0) continue;
                Vector2 o = to + n * (k * 4.4f) + d * (k * 0.8f);
                Vector2 s = o - sd * 14.0f, e = o + sd * 14.0f;
                Vector2 m = s + (e - s) * pk;
                VNorm();
                VLine(s, m, 5.5f, BodyOf(glow), 0.8f * fade);
                VAdd();
                VLine(s, m, 7.0f, glow, 0.25f * fade);
                VLine(s, m, 3.4f, glow, 0.85f * fade);
                VLine(s, m, 1.5f, WHITE, fade);
                if (pk >= 1) VGlow(e, 4, WHITE, 0.6f * fade);
            }
            VNorm();
            (void)p;
        } break;
        case MeleeAnim::Fist: case MeleeAnim::Kick: {
            bool kick = anim == MeleeAnim::Kick;
            float u = Clamp01(t * 3.2f);
            Vector2 st = from + d * 4.0f;
            Vector2 pos = kick ? from + Rot(d, (1 - EaseOut(u)) * -1.3f) * (dist * (0.6f + 0.4f * EaseOut(u)))
                               : LerpV(st, to, EaseOut(u));
            float sc = 0.8f + 0.55f * u;
            if (u < 1) {   // motion lines
                VAdd();
                for (int k = -1; k <= 1; k++) DrawLineEx(pos - d * 10.0f + n * (k * 3.0f), pos - d * 3.0f + n * (k * 3.0f), 0.8f, Fade(glow, 0.7f));
                VNorm();
            }
            VNorm();
            float rot = Deg(d) + (kick ? (1 - u) * -60.0f : 0);
            bool flip = d.x < 0;
            Tex(kick ? T_BOOT : T_FIST, pos, 16 * sc, flip ? rot - 180 : rot, Mix(core, WHITE, 0.3f), fade, flip ? -1.0f : 1.0f, 1);
            if (u >= 1) VImpactStar(to, 11, (t - 0.31f) / 0.69f, glow, seed);
        } break;
        case MeleeAnim::Bite: {
            float u = Clamp01(t * 3.6f);
            float gap = 9.0f * (1 - EaseOut(u)) + 1.5f;
            VNorm();
            Tex(T_JAW, to + Vector2{0, -gap - 3.5f}, 19, 0, C(250, 250, 250), fade);
            Tex(T_JAW, to + Vector2{0, gap + 3.5f}, 19, 180, C(250, 250, 250), fade);
            if (u >= 1) VImpactStar(to, 9, (t - 0.28f) / 0.72f, glow, seed);
        } break;
        case MeleeAnim::Chop: {
            float p = Clamp01(t * 4.0f);
            Vector2 s = to + Vector2{-3, -16}, e = to + Vector2{3, 4};
            Vector2 m = s + (e - s) * EaseOut(p);
            VNorm();
            VLine(s, m, 5.0f, BodyOf(glow), 0.7f * fade);
            VAdd();
            VLine(s, m, 4.0f, glow, 0.7f * fade);
            VLine(s, m, 1.6f, WHITE, fade);
            VNorm();
            if (p >= 1) VImpactStar(e, 12, (t - 0.25f) / 0.75f, glow, seed);
        } break;
        case MeleeAnim::Poke: {
            float u = Clamp01(t * 3.6f);
            Vector2 tip = LerpV(from + d * 4.0f, to + d * 5.0f, EaseOut(u));
            VNorm();
            DrawTriangle(tip, tip - d * 14.0f + n * 1.6f, tip - d * 14.0f - n * 1.6f, Fade(glow, 0.7f * fade));
            DrawTriangle(tip, tip - d * 14.0f - n * 1.6f, tip - d * 14.0f + n * 1.6f, Fade(glow, 0.7f * fade));
            VAdd();
            DrawLineEx(tip - d * 10.0f, tip, 0.8f, Fade(WHITE, fade));
            VGlow(tip, 3.5f, glow, fade);
            VNorm();
            if (u >= 1) VImpactStar(to, 8, (t - 0.28f) / 0.72f, glow, seed);
        } break;
        case MeleeAnim::Whip: {
            float u = Clamp01(t * 2.8f);
            int N = 14;
            Vector2 prev = from;
            for (int i = 1; i <= N; i++) {
                float s = (float)i / N;
                float wave = std::sin(s * PI_F * 1.5f - u * 6) * 6 * (1 - u * 0.6f) * s;
                Vector2 q = from + d * (dist * 1.15f * s * (0.3f + 0.7f * EaseOut(u))) + n * wave;
                VNorm();
                DrawLineEx(prev, q, 3.4f - s * 1.6f, Fade(BodyOf(glow), 0.8f * fade));
                VAdd();
                DrawLineEx(prev, q, 0.9f, Fade(WHITE, fade));
                prev = q;
            }
            VNorm();
            if (u >= 1) VImpactStar(to, 10, (t - 0.36f) / 0.64f, glow, seed);
        } break;
    }
    VNorm();
}

// ---------------------------------------------------------------- Ongoing status and auras

void VStatus(Vector2 base, float h, int status, float time, uint32_t seed) {
    Status st = (Status)status;
    if (st == Status::Burn) {
        VAdd();
        for (int k = 0; k < 4; k++) {
            float ph = std::fmod(time * 1.7f + k * 0.25f + VRand(seed, k), 1.0f);
            float x = (VRand(seed, 10 + k) - 0.5f) * 14;
            Vector2 p{base.x + x + std::sin(time * 9 + k) * 1.2f, base.y - 2 - ph * h * 0.9f};
            float lf = 1 - ph;
            Tex(T_GLOW, p, 7 * lf + 2, 0, Mix(C(200, 40, 20), C(255, 200, 80), lf), 0.85f * lf);
            Tex(T_GLOW, p, 3 * lf + 1, 0, C(255, 240, 200), 0.7f * lf);
        }
        VNorm();
    } else if (st == Status::Poison || st == Status::Toxic) {
        Color pc = st == Status::Toxic ? C(150, 50, 200) : C(190, 100, 230);
        for (int k = 0; k < 3; k++) {
            float ph = std::fmod(time * 0.9f + k * 0.33f + VRand(seed, k), 1.0f);
            float x = (VRand(seed, 20 + k) - 0.5f) * 14;
            Vector2 p{base.x + x, base.y - h * 0.3f - ph * h * 0.8f};
            Tex(T_BUBBLE, p, 3 + ph * 2.5f, 0, pc, 1 - ph);
        }
    } else if (st == Status::Paralyze) {
        uint32_t fr = (uint32_t)(time * 14);
        if (VRand(seed, fr) < 0.55f) {
            Vector2 a{base.x + (VRand(seed, fr * 3 + 1) - 0.5f) * 18, base.y - VRand(seed, fr * 3 + 2) * h};
            Vector2 b = a + Vector2{(VRand(seed, fr * 5 + 3) - 0.5f) * 12, (VRand(seed, fr * 5 + 4) - 0.5f) * 10};
            VBolt(a, b, VHash(seed, fr), 0.5f, C(255, 230, 60), 1, 0);
        }
        VNorm();
    } else if (st == Status::Freeze) {
        VNorm();
        for (int k = 0; k < 4; k++) {
            float x = (k - 1.5f) * 5.5f;
            Tex(T_SHARD, Vector2{base.x + x, base.y - 3 - (k % 2) * 2}, 7, -90 + (k - 1.5f) * 18, C(200, 240, 255), 0.95f);
        }
        float tw = std::fabs(std::sin(time * 3 + seed % 13));
        VGlow(Vector2{base.x + 5, base.y - h * 0.7f}, 3, WHITE, tw);
        VNorm();
    } else if (st == Status::Sleep) {
        for (int k = 0; k < 2; k++) {
            float ph = std::fmod(time * 0.6f + k * 0.5f, 1.0f);
            Vector2 p{base.x + 6 + ph * 8 + std::sin(time * 3 + k) * 2, base.y - h - ph * 14};
            DrawText("z", (int)p.x, (int)p.y, 10, Fade(WHITE, 1 - ph));
        }
    }
}

void VDashAura(Vector2 base, float h, Vector2 dir, const Look& L, float time) {
    Vector2 c{base.x, base.y - h * 0.5f};
    Vector2 d = SafeNorm(dir);
    Vector2 n{-d.y, d.x};
    VAdd();
    for (int k = 0; k < 6; k++) {   // speed lines
        float off = (k - 2.5f) * h * 0.18f;
        float L0 = 8 + std::fmod(k * 7.3f + time * 90, 10.0f);
        Vector2 s = c + n * off - d * (h * 0.4f);
        DrawLineEx(s, s - d * L0, 0.8f, Fade(L.glow, 0.7f));
    }
    float pulse = 0.8f + 0.2f * std::sin(time * 40);
    VGlow(c, h * 0.95f * pulse, L.glow, 0.6f);
    VAdd();
    if (L.impact == Impact::Fire) {
        for (int k = 0; k < 6; k++) {
            float ph = std::fmod(time * 4 + k / 6.0f, 1.0f);
            Vector2 p = c - d * (ph * h * 0.9f) + n * (std::sin(k * 2.0f + time * 20) * h * 0.3f) + Vector2{0, -ph * 4};
            Tex(T_GLOW, p, h * 0.7f * (1 - ph), 0, Mix(C(255, 220, 100), C(230, 60, 20), ph), 0.9f * (1 - ph));
        }
    } else if (L.impact == Impact::Spark) {
        uint32_t fr = (uint32_t)(time * 20);
        for (int k = 0; k < 2; k++) {
            float a1 = VRand(fr, k) * 2 * PI_F, a2 = a1 + 1.5f + VRand(fr, 5 + k);
            Vector2 p1 = c + Vector2{std::cos(a1), std::sin(a1)} * (h * 0.55f), p2 = c + Vector2{std::cos(a2), std::sin(a2)} * (h * 0.55f);
            VBolt(p1, p2, VHash(fr, k), 0.6f, L.glow, 1, 0);
        }
        VAdd();
    } else if (L.impact == Impact::Splash) {
        for (int k = 0; k < 8; k++) {
            float a1 = time * 9 + k * PI_F / 4;
            Vector2 p = c + Vector2{std::cos(a1) * h * 0.6f, std::sin(a1) * h * 0.35f};
            VNorm();
            DrawCircleV(p, 1.3f, Fade(C(200, 230, 255), 0.9f));
        }
    }
    VNorm();
}

void VCharge(Vector2 p, float r, float t, Color c, uint32_t seed, float time) {
    VAdd();
    for (int k = 0; k < 12; k++) {
        float ph = std::fmod(t * 1.8f + VRand(seed, k), 1.0f);
        float ang = VRand(seed, 20 + k) * 2 * PI_F + time * 2;
        float R = r * 1.6f * (1 - ph);
        Vector2 q = p + Vector2{std::cos(ang) * R, std::sin(ang) * R * 0.8f};
        VGlow(q, 2.8f + ph * 2.0f, c, 0.95f * ph);
    }
    VNorm();
    float rr = r * 1.4f * (1 - t) + 3;
    VRingE(p, rr, rr * 0.75f, 1.6f, Fade(BodyOf(c), 0.7f * t), 28);
    VAdd();
    VRingE(p, rr, rr * 0.75f, 0.8f, Fade(c, t), 28);
    float g = 3 + t * r * 0.45f;
    VGlow(p, g * 1.4f, c, 0.5f + 0.5f * t);
    VGlow(p, g * 0.6f, WHITE, 0.7f * t);
    VNorm();
}
