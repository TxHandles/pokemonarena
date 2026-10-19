#include "arena.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>
#include "field.h"
#include "rlgl.h"

namespace {

Texture2D gMenuTex{}, gBattleTex{};
bool gBattleLoaded = false;
ArenaKind gBattleKind = ArenaKind::Stadium;
Venue gBattleVenue = Venue::Stadium;
int gBattleSeason = -1;
unsigned int gRng = 1234567u;

int R(int n) {
    gRng = gRng * 1103515245u + 12345u;
    return (int)((gRng >> 16) % (unsigned)n);
}

Color Shade(Color c, int d) {
    auto cl = [](int v) { return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
    return Color{cl(c.r + d), cl(c.g + d), cl(c.b + d), 255};
}

Color Mix(Color a, Color b, float t) {
    return Color{(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), 255};
}

// Smooth value noise 0..1 (for mottled floors).
float Hash2(int x, int y) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}
float Noise(float x, float y) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = Hash2(ix, iy), b = Hash2(ix + 1, iy), c = Hash2(ix, iy + 1), d = Hash2(ix + 1, iy + 1);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
float Fbm(float x, float y) { return Noise(x, y) * 0.6f + Noise(x * 2.1f, y * 2.1f) * 0.3f + Noise(x * 4.3f, y * 4.3f) * 0.1f; }

void Bricks(Image* img, int x0, int y0, int w, int h, Color base, int brickW, int brickH) {
    ImageDrawRectangle(img, x0, y0, w, h, base);
    Color mortar = Shade(base, -28);
    Color light = Shade(base, 14);
    for (int y = y0; y < y0 + h; y += brickH) {
        ImageDrawRectangle(img, x0, y, w, 1, mortar);
        int row = (y - y0) / brickH;
        int off = (row % 2) ? brickW / 2 : 0;
        for (int x = x0 - off; x < x0 + w; x += brickW) {
            if (x >= x0) ImageDrawRectangle(img, x, y, 1, brickH, mortar);
            if (x + 1 >= x0 && x + 1 < x0 + w && y + 1 < y0 + h) ImageDrawPixel(img, x + 1, y + 1, light);
        }
    }
}

struct Theme {
    Color standA, standB, wall, side, ledge, trim;
};

Theme ThemeFor(ArenaKind k) {
    switch (k) {
        case ArenaKind::Rock:     return {{60, 50, 44, 255}, {52, 44, 38, 255}, {120, 104, 88, 255}, {96, 84, 72, 255}, {130, 116, 100, 255}, {80, 66, 52, 255}};
        case ArenaKind::Water:    return {{40, 56, 84, 255}, {34, 48, 74, 255}, {80, 120, 160, 255}, {60, 96, 132, 255}, {150, 190, 210, 255}, {40, 70, 110, 255}};
        case ArenaKind::Electric: return {{44, 44, 52, 255}, {38, 38, 46, 255}, {96, 96, 108, 255}, {72, 72, 84, 255}, {120, 120, 132, 255}, {40, 40, 48, 255}};
        case ArenaKind::Grass:    return {{44, 66, 48, 255}, {38, 58, 42, 255}, {150, 180, 160, 255}, {100, 130, 110, 255}, {170, 200, 180, 255}, {60, 90, 60, 255}};
        case ArenaKind::Poison:   return {{46, 40, 56, 255}, {40, 34, 50, 255}, {80, 70, 90, 255}, {64, 56, 74, 255}, {96, 86, 106, 255}, {40, 36, 30, 255}};
        case ArenaKind::Psychic:  return {{40, 26, 60, 255}, {34, 22, 52, 255}, {90, 60, 130, 255}, {70, 46, 106, 255}, {120, 84, 160, 255}, {30, 18, 46, 255}};
        case ArenaKind::Fire:     return {{58, 30, 26, 255}, {50, 26, 22, 255}, {92, 56, 48, 255}, {70, 42, 36, 255}, {110, 70, 56, 255}, {40, 20, 18, 255}};
        case ArenaKind::Ground:   return {{36, 32, 34, 255}, {30, 26, 28, 255}, {70, 64, 66, 255}, {54, 50, 52, 255}, {86, 80, 82, 255}, {120, 30, 30, 255}};
        case ArenaKind::Ice:      return {{44, 60, 84, 255}, {38, 52, 74, 255}, {170, 200, 226, 255}, {130, 164, 196, 255}, {200, 226, 244, 255}, {90, 130, 170, 255}};
        case ArenaKind::Fighting: return {{60, 40, 36, 255}, {52, 34, 30, 255}, {120, 90, 70, 255}, {96, 70, 54, 255}, {140, 108, 84, 255}, {170, 50, 40, 255}};
        case ArenaKind::Ghost:    return {{26, 20, 38, 255}, {22, 16, 32, 255}, {64, 54, 80, 255}, {50, 42, 64, 255}, {76, 66, 94, 255}, {30, 22, 42, 255}};
        case ArenaKind::Dragon:   return {{34, 30, 52, 255}, {28, 24, 44, 255}, {74, 64, 100, 255}, {58, 50, 82, 255}, {92, 80, 120, 255}, {200, 160, 60, 255}};
        case ArenaKind::Champion: return {{70, 30, 36, 255}, {60, 24, 30, 255}, {150, 120, 70, 255}, {120, 94, 54, 255}, {200, 170, 100, 255}, {240, 200, 80, 255}};
        default:                  return {{58, 48, 72, 255}, {48, 40, 62, 255}, {104, 96, 112, 255}, {84, 76, 94, 255}, {112, 104, 120, 255}, {90, 76, 60, 255}};
    }
}

// ---------------------------------------------------------------- Texture toolkit (hi-res floors)
// The floor is baked at the canvas resolution (3 texels per logical unit), so every surface is
// sampled in world units with sub-pixel detail: grass blades, pebbles, wood grain, tile bevels.

uint32_t Hu(int x, int y, uint32_t s) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + s * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
float Hf(int x, int y, uint32_t s) { return (Hu(x, y, s) & 0xffff) / 65535.0f; }
// 0..99, constant over a small cell (about 2 texels): fine grain that doesn't shimmer.
int Speck(float wx, float wy, uint32_t s = 0) { return (int)(Hu((int)std::floor(wx * 1.5f), (int)std::floor(wy * 0.83f), s + 11) % 100); }
float Clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
float Smooth(float a, float b, float v) { float t = Clamp01((v - a) / (b - a)); return t * t * (3 - 2 * t); }
Color Lit(Color c, float k) {
    auto cl = [](float v) { return (unsigned char)(v < 0 ? 0 : (v > 255 ? 255 : v)); };
    return Color{cl(c.r * k), cl(c.g * k), cl(c.b * k), 255};
}
Color MixF(Color a, Color b, float t) { return Mix(a, b, Clamp01(t)); }
float N2(float x, float y) { return Noise(x, y); }

// Worley / cellular noise (input in cell units): nearest and second-nearest feature distances.
struct Wor { float f1, f2, px, py; uint32_t id; };
Wor Worley(float x, float y, uint32_t s) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    Wor w{9, 9, 0, 0, 0};
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = ix + dx, cy = iy + dy;
            uint32_t h = Hu(cx, cy, s);
            float fx = cx + 0.1f + 0.8f * ((h & 0xff) / 255.0f), fy = cy + 0.1f + 0.8f * (((h >> 8) & 0xff) / 255.0f);
            float d = std::sqrt((x - fx) * (x - fx) + (y - fy) * (y - fy));
            if (d < w.f1) { w.f2 = w.f1; w.f1 = d; w.px = fx; w.py = fy; w.id = h; }
            else if (d < w.f2) w.f2 = d;
        }
    return w;
}

// A scatter of small round things (pebbles, flowers, leaves) on a jittered grid of 'cell' world
// units. Returns 0 when (wx, wy) is outside every item; otherwise 1 + a shade code:
// 1 = body, 2 = lit top, 3 = shadow under it. 'id' gets the item's hash.
int Scatter(float wx, float wy, float cell, float chance, float rMin, float rMax, uint32_t salt, uint32_t* id = nullptr) {
    float x = wx / cell, y = wy / cell;
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            int cx = ix + dx, cy = iy + dy;
            uint32_t h = Hu(cx, cy, salt);
            if ((h & 0xffff) / 65535.0f >= chance) continue;
            float fx = (cx + 0.15f + 0.7f * (((h >> 16) & 0xff) / 255.0f)) * cell, fy = (cy + 0.15f + 0.7f * (((h >> 24) & 0xff) / 255.0f)) * cell;
            float r = rMin + (rMax - rMin) * (((h >> 4) & 0xff) / 255.0f);
            float ex = (wx - fx) / r, ey = (wy - fy) * DEPTH / (r * 0.8f);   // round on screen
            float d2 = ex * ex + ey * ey;
            if (d2 <= 1.0f) {
                if (id) *id = h;
                return ey < -0.35f && ex < 0.4f ? 2 : 1;
            }
            float sy = (wy - fy) * DEPTH / (r * 0.8f) - 0.45f;   // a soft shadow just below
            if (ex * ex + sy * sy <= 1.0f && ey > 0) { if (id) *id = h; return 3; }
        }
    return 0;
}

// Grass blades: thin strokes stretched up-and-down the screen, two layers. 0..1.
float Blades(float wx, float wy, uint32_t salt) {
    float warp = N2(wx * 0.06f + salt, wy * 0.06f) * 3.0f;
    float a = N2(wx * 1.25f + warp, wy * 0.21f + salt * 3.1f);
    float b = N2(wx * 0.9f + 37 + warp, wy * 0.16f - salt);
    return a * 0.6f + b * 0.4f;
}

// A beveled tile (grout lines, a lit top edge, a shaded bottom edge), sizes in world units.
Color BevelTile(float wx, float wy, float tw, float th, Color a, Color b, Color grout, bool offset, int vary, float* localOut = nullptr) {
    int row = (int)std::floor(wy / th);
    float ox = offset && (row & 1) ? tw / 2 : 0;
    int col = (int)std::floor((wx + ox) / tw);
    float lx = std::fmod(wx + ox, tw), ly = std::fmod(wy, th);
    if (lx < 0) lx += tw;
    if (ly < 0) ly += th;
    float gx = lx, gy = ly * DEPTH, gw = tw, gh = th * DEPTH;   // screen units within the tile
    if (localOut) { localOut[0] = lx / tw; localOut[1] = ly / th; localOut[2] = (float)col; localOut[3] = (float)row; }
    if (gx < 0.7f || gy < 0.6f) return grout;
    Color c = ((col + row) & 1) ? a : b;
    c = Shade(c, (int)((Hf(col, row, 5) - 0.5f) * vary));
    c = Lit(c, 0.96f + 0.08f * Fbm(wx * 0.05f + col, wy * 0.05f + row));
    if (gy < 1.5f) c = Lit(c, 1.12f);                // lit top edge
    else if (gh - gy < 1.0f) c = Lit(c, 0.82f);       // shaded bottom edge
    if (gx < 1.4f) c = Lit(c, 1.06f);
    else if (gw - gx < 1.0f) c = Lit(c, 0.9f);
    int n = Speck(wx, wy, 3);
    if (n < 6) c = Lit(c, 0.93f);
    else if (n > 95) c = Lit(c, 1.06f);
    return c;
}

// Cracks: thin dark lines along cell borders (Worley F2 - F1 small).
float CrackMask(float wx, float wy, float cell, uint32_t salt, float width) {
    Wor w = Worley(wx / cell, wy / cell, salt);
    float e = (w.f2 - w.f1) * cell;
    return e < width ? 1.0f - e / width : 0.0f;
}

// ---------------------------------------------------------------- Ground surfaces

Color Grass(int season, float wx, float wy) {
    if (season == 3) {   // snow, a few tufts poking through
        float m = Fbm(wx * 0.012f, wy * 0.02f);
        Color c = MixF(Color{214, 224, 240, 255}, Color{246, 249, 255, 255}, m * 1.4f - 0.2f);
        float drift = N2(wx * 0.03f + 5, wy * 0.05f);
        c = Lit(c, 0.95f + drift * 0.08f);
        float tuft = N2(wx * 0.07f + 40, wy * 0.11f);
        if (tuft > 0.7f && Blades(wx, wy, 7) > 0.55f) c = MixF(c, Color{110, 130, 104, 255}, (tuft - 0.7f) * 4);
        int n = Speck(wx, wy, 21);
        if (n == 0) c = WHITE;
        else if (n < 3) c = Lit(c, 0.94f);
        return c;
    }
    Color base = season == 0 ? Color{98, 172, 78, 255} : season == 2 ? Color{146, 140, 70, 255} : Color{82, 152, 62, 255};
    Color dry = season == 2 ? Color{176, 150, 80, 255} : Color{150, 170, 76, 255};
    Color lush = season == 2 ? Color{110, 116, 58, 255} : Color{58, 124, 52, 255};
    float m = Fbm(wx * 0.011f, wy * 0.018f);
    Color c = base;
    if (m > 0.58f) c = MixF(c, dry, (m - 0.58f) * 2.2f);
    else if (m < 0.42f) c = MixF(c, lush, (0.42f - m) * 2.4f);
    float v = Blades(wx, wy, 1);
    c = Lit(c, 0.80f + v * 0.36f);
    if (v > 0.74f) c = MixF(c, Color{200, 220, 120, 255}, (v - 0.74f) * 1.3f);   // sunlit tips
    // clover and small flowers / fallen leaves
    uint32_t id = 0;
    if (season == 0 || season == 1) {
        int f = Scatter(wx, wy, 13, season == 0 ? 0.10f : 0.035f, 0.9f, 1.4f, 101, &id);
        if (f == 1 || f == 2) {
            Color pet = (id % 4 == 0) ? Color{250, 250, 245, 255} : (id % 4 == 1) ? Color{255, 220, 90, 255} : (id % 4 == 2) ? Color{245, 150, 190, 255} : Color{180, 160, 240, 255};
            c = f == 2 ? Lit(pet, 1.08f) : pet;
            float cx = std::fmod(wx, 13.0f);
            (void)cx;
        } else if (f == 3) c = Lit(c, 0.85f);
    } else if (season == 2) {
        int f = Scatter(wx, wy, 9, 0.16f, 1.0f, 1.8f, 103, &id);
        if (f == 1 || f == 2) {
            Color lf = (id % 3 == 0) ? Color{214, 96, 40, 255} : (id % 3 == 1) ? Color{230, 160, 50, 255} : Color{160, 90, 50, 255};
            c = f == 2 ? Lit(lf, 1.1f) : lf;
        } else if (f == 3) c = Lit(c, 0.86f);
    }
    return c;
}

Color Dirt(float wx, float wy, int season, uint32_t salt = 0) {
    float m = Fbm(wx * 0.03f + salt, wy * 0.045f);
    Color c = MixF(Color{150, 118, 82, 255}, Color{190, 158, 112, 255}, m * 1.3f - 0.15f);
    float grain = N2(wx * 0.6f, wy * 0.35f);
    c = Lit(c, 0.94f + grain * 0.12f);
    int n = Speck(wx, wy, 31 + salt);
    if (n < 7) c = Lit(c, 0.86f);
    else if (n > 94) c = Lit(c, 1.1f);
    uint32_t id = 0;
    int p = Scatter(wx, wy, 7, 0.32f, 0.6f, 1.5f, 202 + salt, &id);
    if (p == 1 || p == 2) {
        Color st = (id % 3 == 0) ? Color{150, 144, 136, 255} : (id % 3 == 1) ? Color{124, 112, 98, 255} : Color{176, 164, 146, 255};
        c = p == 2 ? Lit(st, 1.18f) : st;
    } else if (p == 3) c = Lit(c, 0.8f);
    if (season == 3) {
        float sn = N2(wx * 0.06f, wy * 0.09f);
        if (sn > 0.45f) c = MixF(c, Color{232, 238, 248, 255}, (sn - 0.45f) * 6);
    }
    return c;
}

Color Sand(float wx, float wy, bool pale) {
    Color c = pale ? Color{214, 204, 176, 255} : Color{230, 210, 156, 255};
    c = Lit(c, 0.94f + Fbm(wx * 0.02f, wy * 0.03f) * 0.12f);
    float rip = std::sin(wy * 0.42f + N2(wx * 0.03f, wy * 0.03f) * 7 + wx * 0.02f);
    if (rip > 0.7f) c = Lit(c, 1.05f);
    else if (rip < -0.85f) c = Lit(c, 0.94f);
    int n = Speck(wx, wy, 41);
    if (n < 8) c = Lit(c, 0.92f);
    else if (n > 93) c = Lit(c, 1.07f);
    return c;
}

// Wood planks running left to right: grain, knots, seams and nails.
Color Planks(float wx, float wy, float plankW, float jointEvery, Color base, Color seam, int salt) {
    int row = (int)std::floor(wy / plankW);
    float ly = std::fmod(wy, plankW);
    if (ly < 0) ly += plankW;
    if (ly * DEPTH < 0.6f) return seam;
    float off = Hf(row, 3, salt) * jointEvery;
    float jx = std::fmod(wx + off, jointEvery);
    if (jx < 0) jx += jointEvery;
    int seg = (int)std::floor((wx + off) / jointEvery);
    if (jx < 0.6f) return seam;
    Color c = Shade(base, (int)((Hf(row, seg, salt + 1) - 0.5f) * 34));
    float g = std::sin((ly * 2.2f + N2(wx * 0.04f + seg * 7, row * 1.7f) * 6.0f) * 3.0f + wx * 0.01f);
    c = Lit(c, 0.93f + 0.07f * g + 0.06f * N2(wx * 0.25f, wy * 1.5f));
    if (ly * DEPTH < 1.3f) c = Lit(c, 1.1f);                    // lit edge
    else if ((plankW - ly) * DEPTH < 0.9f) c = Lit(c, 0.84f);   // shaded edge
    float kx = Hf(row, seg, salt + 2) * jointEvery, kd = std::fabs(jx - kx) + std::fabs(ly - plankW * 0.5f) * 1.6f;
    if (Hf(row, seg, salt + 3) < 0.35f && kd < 2.2f) c = Lit(c, 0.7f + kd * 0.1f);   // a knot
    if ((jx > 1.2f && jx < 2.6f) || (jointEvery - jx > 0.8f && jointEvery - jx < 2.2f))   // nails by the joints
        if (std::fabs(ly - plankW * 0.5f) < 1.0f) c = Color{90, 88, 92, 255};
    return c;
}

// Rough rock ground: slabs (cells) with cracks between them and gravel.
Color RockGround(float wx, float wy, Color a, Color b, uint32_t salt) {
    float warp = (N2(wx * 0.03f + salt, wy * 0.04f) - 0.5f) * 14;
    Wor w = Worley((wx + warp) / 44.0f, (wy - warp) / 50.0f, salt);
    float edge = (w.f2 - w.f1) * 44.0f;
    Color c = MixF(a, b, Fbm(wx * 0.02f + salt, wy * 0.03f) * 1.2f - 0.1f);
    c = Lit(c, 0.95f + 0.1f * ((w.id & 0xff) / 255.0f - 0.5f));
    c = Lit(c, 0.92f + 0.16f * Fbm(wx * 0.06f + salt, wy * 0.09f));
    float crackVis = Smooth(0.35f, 0.6f, N2(wx * 0.02f + 7, wy * 0.03f));   // cracks come and go
    if (edge < 0.8f) c = Lit(c, 1.0f - 0.32f * crackVis);
    else if (edge < 1.8f) c = Lit(c, 1.0f - 0.1f * crackVis);
    else if (edge < 2.6f) c = Lit(c, 1.0f + 0.05f * crackVis);
    int n = Speck(wx, wy, salt + 3);
    if (n < 9) c = Lit(c, 0.86f);
    else if (n > 93) c = Lit(c, 1.12f);
    return c;
}

// Basalt with glowing lava cracks.
Color LavaRock(float wx, float wy, uint32_t salt, float glow) {
    Color c = RockGround(wx, wy, Color{58, 48, 46, 255}, Color{74, 62, 58, 255}, salt);
    float k = CrackMask(wx, wy, 52, salt + 9, 3.0f) * Smooth(0.45f, 0.65f, N2(wx * 0.015f + salt, wy * 0.022f));
    float k2 = std::fabs(N2(wx * 0.03f + salt, wy * 0.045f) - 0.5f);
    float hot = std::max(k * 0.95f, k2 < 0.014f ? 1.0f - k2 / 0.014f : 0.0f) * glow;
    if (hot <= 0) {   // a dim warm glow near the hot seams
        float near = CrackMask(wx, wy, 52, salt + 9, 7.0f) * Smooth(0.45f, 0.65f, N2(wx * 0.015f + salt, wy * 0.022f));
        c = MixF(c, Color{110, 50, 34, 255}, near * 0.35f * glow);
    }
    if (hot > 0.0f) {
        c = MixF(c, Color{150, 50, 30, 255}, hot * 1.4f);
        if (hot > 0.55f) c = MixF(c, Color{255, 150, 50, 255}, (hot - 0.55f) * 2.6f);
        if (hot > 0.85f) c = MixF(c, Color{255, 230, 140, 255}, (hot - 0.85f) * 5);
    }
    int n = Speck(wx, wy, salt + 4);
    if (n == 0) c = Color{140, 132, 126, 255};   // ash
    return c;
}

// Ice: a smooth sheen, frost and pale cracks.
Color IceSurface(float wx, float wy, Color base, uint32_t salt) {
    float m = Fbm(wx * 0.015f + salt, wy * 0.025f);
    Color c = Lit(base, 0.92f + m * 0.14f);
    float sheen = std::sin(wx * 0.012f - wy * 0.02f + m * 2.0f);
    c = MixF(c, WHITE, Smooth(0.6f, 1.0f, sheen) * 0.22f);
    float k = CrackMask(wx, wy, 58, salt + 5, 0.9f) * Smooth(0.35f, 0.6f, N2(wx * 0.02f + salt, wy * 0.03f));
    if (k > 0) c = MixF(c, Color{250, 253, 255, 255}, k * 0.6f);
    float k2 = CrackMask(wx + 17, wy + 9, 23, salt + 6, 0.7f);
    if (k2 > 0) c = MixF(c, Lit(base, 0.8f), k2 * 0.5f);
    int n = Speck(wx, wy, salt + 7);
    if (n == 0) c = WHITE;
    return c;
}

Color FloorBase(ArenaKind k, float wx, float wy) {
    int tx = (int)std::floor(wx / 40), ty = (int)std::floor(wy / 40);
    float lxx = std::fmod(wx, 40.0f), lyy = std::fmod(wy, 40.0f);
    bool lineX = lxx < 0.8f, lineY = lyy * DEPTH < 0.6f;
    int n = Speck(wx, wy, (uint32_t)k * 13);
    switch (k) {
        case ArenaKind::Rock: {   // stone slabs, cracked and dusty
            float loc[4];
            Color c = BevelTile(wx, wy, 40, 40, Color{152, 140, 122, 255}, Color{142, 130, 114, 255}, Color{92, 82, 70, 255}, false, 16, loc);
            if (c.r == 92 && c.g == 82) return c;
            float cr = CrackMask(wx, wy, 30, 77, 0.8f);
            if (cr > 0 && Hf((int)loc[2], (int)loc[3], 8) < 0.5f) c = MixF(c, Color{96, 86, 74, 255}, cr);
            uint32_t id;
            int p = Scatter(wx, wy, 11, 0.12f, 0.6f, 1.2f, 303, &id);
            if (p == 1) c = Color{128, 118, 104, 255}; else if (p == 2) c = Color{176, 166, 150, 255}; else if (p == 3) c = Lit(c, 0.86f);
            return c;
        }
        case ArenaKind::Water: {   // a deep pool: depth shading and caustic light
            float d = Fbm(wx * 0.008f, wy * 0.012f);
            Color c = MixF(Color{30, 84, 160, 255}, Color{56, 124, 196, 255}, d * 1.3f - 0.1f);
            float c1 = 1.0f - std::fabs(N2(wx * 0.07f, wy * 0.1f) - 0.5f) * 2.0f;
            float c2 = 1.0f - std::fabs(N2(wx * 0.07f + 50, wy * 0.1f + 20) - 0.5f) * 2.0f;
            float caus = std::pow(c1 * c2, 6.0f);
            c = MixF(c, Color{150, 210, 250, 255}, caus * 0.8f);
            if (n < 2) c = Lit(c, 1.15f);
            return c;
        }
        case ArenaKind::Electric: {   // steel deck: diamond plate and plain plates, hazard border
            if (wx < 10 || wx > ARENA_W - 10 || wy < 12 || wy > ARENA_H - 12) {
                bool stripe = ((int)std::floor((wx + wy * DEPTH) / 6)) & 1;
                Color c = stripe ? Color{240, 200, 40, 255} : Color{34, 34, 38, 255};
                return Lit(c, 0.92f + 0.1f * N2(wx * 0.3f, wy * 0.3f));
            }
            float loc[4];
            Color c = BevelTile(wx, wy, 40, 40, Color{92, 96, 106, 255}, Color{82, 86, 96, 255}, Color{44, 46, 52, 255}, false, 8, loc);
            if (c.r == 44 && c.g == 46) return c;
            if ((tx + ty) & 1) {   // diamond plate
                float u = std::fmod(wx * 0.5f + wy * DEPTH * 0.5f, 3.0f), v = std::fmod(wx * 0.5f - wy * DEPTH * 0.5f + 300, 3.0f);
                bool ridge = ((int)((wx + wy * DEPTH) / 3.0f) & 1) ? (std::fabs(u - 1.5f) < 0.45f && std::fabs(v - 1.5f) < 1.1f) : false;
                if (ridge) c = Lit(c, 1.18f);
            }
            float rx = std::min(lxx, 40 - lxx), ry = std::min(lyy, 40 - lyy);
            if (rx > 2.2f && rx < 4.2f && ry * DEPTH > 1.4f && ry * DEPTH < 3.2f) c = Color{150, 154, 162, 255};   // rivets
            return c;
        }
        case ArenaKind::Grass: {   // a mown lawn under glass
            Color c = Grass(1, wx, wy);
            bool stripe = ((int)std::floor(wx / 40)) & 1;
            c = Lit(c, stripe ? 1.07f : 0.95f);
            c = MixF(c, Color{150, 200, 140, 255}, 0.12f);
            return c;
        }
        case ArenaKind::Poison: {   // swampy mud, wet sheen and moss
            float m = Fbm(wx * 0.02f, wy * 0.03f);
            Color c = MixF(Color{70, 74, 46, 255}, Color{100, 104, 64, 255}, m * 1.4f - 0.2f);
            float wet = Fbm(wx * 0.025f + 3, wy * 0.035f);
            c = MixF(c, Color{58, 58, 50, 255}, Smooth(0.55f, 0.75f, wet) * 0.8f);
            c = Lit(c, 1.0f + 0.2f * Smooth(0.7f, 0.8f, wet) * Smooth(0.6f, 0.8f, N2(wx * 0.3f, wy * 0.4f)));   // sheen on the wet mud
            float moss = Fbm(wx * 0.03f + 70, wy * 0.045f);
            c = MixF(c, Lit(Color{92, 118, 56, 255}, 0.8f + 0.3f * Blades(wx, wy, 9)), Smooth(0.58f, 0.74f, moss) * 0.7f);
            uint32_t id;
            int p = Scatter(wx, wy, 14, 0.07f, 0.5f, 1.0f, 404, &id);
            if (p == 1 || p == 2) c = p == 2 ? Color{220, 170, 240, 255} : Color{150, 90, 170, 255};   // little toxic bubbles
            if (n < 6) c = Lit(c, 0.88f);
            return c;
        }
        case ArenaKind::Psychic: {   // dark tiles with a glowing grid
            float gx = std::min(lxx, 40 - lxx), gy = std::min(lyy, 40 - lyy) * DEPTH;
            float d = std::min(gx, gy);
            Color c = ((tx + ty) & 1) ? Color{44, 30, 70, 255} : Color{36, 24, 60, 255};
            c = Lit(c, 0.92f + 0.16f * Fbm(wx * 0.03f, wy * 0.04f));
            if (d < 0.7f) return Color{255, 150, 240, 255};
            if (d < 3.0f) c = MixF(c, Color{220, 90, 210, 255}, (1 - (d - 0.7f) / 2.3f) * 0.75f);
            if (n == 0) c = Color{200, 160, 255, 255};
            return c;
        }
        case ArenaKind::Fire: return LavaRock(wx, wy, 61, 0.9f);
        case ArenaKind::Ground: {   // dark concrete, the boss's red circle
            float loc[4];
            Color c = BevelTile(wx, wy, 40, 40, Color{72, 68, 70, 255}, Color{66, 62, 64, 255}, Color{42, 40, 42, 255}, false, 10, loc);
            float r = std::sqrt((wx - ARENA_W / 2) * (wx - ARENA_W / 2) + (wy - ARENA_H / 2) * (wy - ARENA_H / 2));
            if (r < 62 && r > 56) c = Lit(Color{150, 34, 38, 255}, 0.95f + 0.1f * N2(wx * 0.3f, wy * 0.3f));
            else if (r <= 56) c = Lit(c, 0.9f);
            float stain = N2(wx * 0.03f + 9, wy * 0.04f);
            if (stain > 0.68f) c = Lit(c, 0.9f);
            return c;
        }
        case ArenaKind::Ice: {   // frosted ice tiles
            float loc[4];
            Color c = BevelTile(wx, wy, 40, 40, Color{198, 224, 242, 255}, Color{186, 214, 236, 255}, Color{232, 246, 255, 255}, false, 10, loc);
            if (c.r == 232 && c.g == 246) return c;
            Color ice = IceSurface(wx, wy, c, 3);
            return ice;
        }
        case ArenaKind::Fighting: {   // a canvas mat, a dark apron, and the centre circle
            float edge = std::min(std::min(wx, ARENA_W - wx), std::min(wy, ARENA_H - wy) * 1.2f);
            if (edge < 10) return Lit(Color{40, 60, 120, 255}, 0.9f + 0.15f * N2(wx * 0.2f, wy * 0.2f));
            Color c = Color{228, 224, 210, 255};
            float weave = ((int)std::floor(wx * 1.5f) + (int)std::floor(wy * DEPTH * 1.5f)) & 1 ? 1.03f : 0.98f;
            c = Lit(c, weave * (0.97f + 0.05f * Fbm(wx * 0.04f, wy * 0.04f)));
            c = Lit(c, 1.0f - 0.04f * Smooth(0.6f, 0.8f, N2(wx * 0.1f, wy * 0.03f)));
            if (edge < 16) c = Lit(Color{60, 90, 170, 255}, 0.95f + 0.1f * N2(wx * 0.3f, wy * 0.3f));
            float d = std::sqrt((wx - ARENA_W / 2) * (wx - ARENA_W / 2) + (wy - ARENA_H / 2) * (wy - ARENA_H / 2));
            if (d < 72 && d > 68) c = Color{200, 50, 40, 255};
            else if (d < 68) c = Mix(c, Color{240, 200, 190, 255}, 0.3f);
            if (std::fabs(wx - ARENA_W / 2) < 0.8f && d >= 72) c = Lit(c, 0.8f);
            return c;
        }
        case ArenaKind::Ghost: {   // irregular dark flagstones in a cold mist
            Wor w = Worley(wx / 24.0f, wy / 22.0f, 91);
            float e = (w.f2 - w.f1) * 22.0f;
            Color c = Shade(Color{60, 52, 76, 255}, (int)((w.id & 0xff) / 255.0f * 22) - 11);
            c = Lit(c, 0.9f + 0.18f * Fbm(wx * 0.05f, wy * 0.06f));
            if (e < 1.0f) c = Color{24, 20, 34, 255};
            else if (e < 2.0f) c = Lit(c, 0.8f);
            float mist = Fbm(wx * 0.015f + 3, wy * 0.025f);
            if (mist > 0.55f) c = MixF(c, Color{120, 104, 160, 255}, (mist - 0.55f) * 1.6f);
            if (n < 5) c = Lit(c, 0.85f);
            return c;
        }
        case ArenaKind::Dragon: {   // dark stone with a gold dragon seal in the middle
            Color c = BevelTile(wx, wy, 40, 40, Color{56, 50, 76, 255}, Color{48, 42, 66, 255}, Color{30, 26, 42, 255}, false, 10);
            float dx = wx - ARENA_W / 2, dy = wy - ARENA_H / 2, d = std::sqrt(dx * dx + dy * dy);
            auto gold = [&](float br) { return Lit(Color{214, 170, 72, 255}, br * (0.92f + 0.14f * N2(wx * 0.4f, wy * 0.4f))); };
            if (d < 84 && d > 80) c = gold(d > 83 || d < 81 ? 0.85f : 1.1f);
            else if (d < 74 && d > 72) c = gold(0.85f);
            float a = std::atan2(dy, dx);
            float spiral = std::fmod(a * 3 / 6.2831853f + d / 40.0f + 10, 1.0f);
            if (d < 70 && d > 14 && spiral < 0.08f) c = gold(spiral < 0.02f ? 1.15f : 0.95f);
            if (d < 12) c = Lit(Color{180, 40, 50, 255}, d < 8 ? 1.15f : 0.95f);
            if (std::fabs(wy - ARENA_H / 2) < 6 && d > 84) c = Mix(c, Color{130, 30, 40, 255}, 0.6f);
            return c;
        }
        case ArenaKind::Champion: {   // polished marble, gold inlay, a giant Poke Ball and a red carpet
            Color c = ((tx + ty) & 1) ? Color{238, 232, 220, 255} : Color{224, 216, 200, 255};
            float vein = std::fabs(N2(wx * 0.05f + tx * 3, wy * 0.05f + ty * 5) - 0.5f);
            if (vein < 0.01f) c = Lit(c, 0.9f + vein * 8);
            c = Lit(c, 0.97f + 0.06f * Fbm(wx * 0.04f, wy * 0.04f));
            float sheen = std::sin((wx - wy) * 0.02f);
            if (sheen > 0.8f) c = MixF(c, WHITE, (sheen - 0.8f) * 1.5f);
            if (lineX || lineY) c = Color{214, 170, 70, 255};
            float edge = std::min(std::min(wx, ARENA_W - wx), std::min(wy, ARENA_H - wy) * 1.2f);
            if (edge < 8) c = Lit(Color{200, 150, 50, 255}, 0.95f + 0.1f * N2(wx * 0.4f, wy * 0.4f));
            else if (edge < 12) c = Color{120, 30, 40, 255};
            if (std::fabs(wy - ARENA_H / 2) < 16 && (wx < ARENA_W / 2 - 90 || wx > ARENA_W / 2 + 90))
                c = std::fabs(wy - ARENA_H / 2) > 13 ? Color{230, 190, 80, 255} : Lit(Color{170, 30, 40, 255}, 0.92f + 0.12f * N2(wx * 0.5f, wy * 0.8f));
            float dx = wx - ARENA_W / 2, dy = wy - ARENA_H / 2, d = std::sqrt(dx * dx + dy * dy);
            if (d < 86) {
                if (d > 82) c = Color{230, 190, 80, 255};
                else if (d > 78) c = Color{40, 36, 50, 255};
                else if (std::fabs(dy) < 5) c = Color{40, 36, 50, 255};
                else {
                    c = dy < 0 ? Color{214, 52, 52, 255} : Color{244, 242, 236, 255};
                    float sh = (-dx * 0.4f - dy) / 78.0f;   // a glossy dome
                    c = Lit(c, 0.9f + sh * 0.12f);
                    float hx = (dx + 34) / 22, hy = (dy + 44) / 14;
                    if (dy < 0 && hx * hx + hy * hy < 1) c = MixF(c, WHITE, 0.35f * (1 - hx * hx - hy * hy));
                }
                if (d < 20) c = d > 15 ? Color{40, 36, 50, 255} : (d > 10 ? Color{250, 250, 252, 255} : Color{230, 190, 80, 255});
            }
            return c;
        }
        default: {   // the plain stadium: packed clay
            float m = Fbm(wx * 0.02f, wy * 0.03f);
            Color c = MixF(Color{196, 166, 120, 255}, Color{214, 186, 138, 255}, m * 1.3f - 0.15f);
            float grain = N2(wx * 0.5f, wy * 0.3f);
            c = Lit(c, 0.95f + grain * 0.1f);
            c = Lit(c, 1.0f - 0.05f * Smooth(0.6f, 0.8f, N2(wx * 0.06f + 9, wy * 0.02f)));   // scuffed lanes
            uint32_t id;
            int p = Scatter(wx, wy, 9, 0.12f, 0.5f, 0.9f, 909, &id);
            if (p == 1) c = Lit(c, 0.86f); else if (p == 2) c = Lit(c, 1.08f); else if (p == 3) c = Lit(c, 0.94f);
            if (n < 6) c = Lit(c, 0.88f);
            else if (n > 95) c = Lit(c, 1.08f);
            return c;
        }
    }
}

// ---------------------------------------------------------------- Venues (non-gym battles)

Color VenueFloor(Venue v, int season, float wx, float wy) {
    int n = Speck(wx, wy, (uint32_t)v * 7);
    switch (v) {
        case Venue::Route: {
            float cy = ARENA_H * 0.5f + std::sin(wx * 0.012f + 1.3f) * 55;
            float d = std::fabs(wy - cy) + (N2(wx * 0.12f, wy * 0.12f) - 0.5f) * 7;
            if (d < 18) {
                Color c = Dirt(wx, wy, season);
                float rut = std::min(std::fabs(wy - cy - 7), std::fabs(wy - cy + 7));
                if (rut < 2.2f) c = Lit(c, 0.88f + rut * 0.04f);
                if (d > 15) c = Lit(c, 0.92f);
                return c;
            }
            if (d < 22 && Blades(wx, wy, 1) < 0.35f + (d - 18) * 0.1f) return Lit(Dirt(wx, wy, season), 0.9f);
            return Grass(season, wx, wy);
        }
        case Venue::Forest: {   // humus and leaf litter, moss and grass patches, dappled light
            if (season == 3) {
                Color c = Grass(3, wx, wy);
                float lt = Fbm(wx * 0.025f, wy * 0.04f);
                return Lit(c, 0.88f + Smooth(0.5f, 0.7f, lt) * 0.14f);
            }
            float m = Fbm(wx * 0.02f, wy * 0.03f);
            Color c = MixF(Color{70, 56, 40, 255}, Color{96, 78, 54, 255}, m * 1.3f - 0.15f);
            c = Lit(c, 0.92f + 0.14f * N2(wx * 0.5f, wy * 0.3f));
            float gp = Fbm(wx * 0.014f + 50, wy * 0.022f);
            float gt = Smooth(0.44f, 0.66f, gp) * (0.55f + 0.45f * Blades(wx, wy, 6));
            if (gt > 0) c = MixF(c, Lit(Grass(season == 2 ? 2 : 1, wx, wy), 0.72f), gt);
            uint32_t id;
            float cell = 4.6f;
            float x = wx / cell, y = wy / cell;   // leaves: small rotated ovals, densely
            int ix = (int)std::floor(x), iy = (int)std::floor(y);
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    uint32_t h = Hu(ix + dx, iy + dy, 501);
                    if ((h & 0xff) > (gt > 0.5f ? 30u : 85u)) continue;
                    float fx = (ix + dx + ((h >> 8) & 0xff) / 255.0f) * cell, fy = (iy + dy + ((h >> 16) & 0xff) / 255.0f) * cell;
                    float ang = ((h >> 24) & 0xff) / 255.0f * 3.14159f;
                    float ux = wx - fx, uy = (wy - fy) * DEPTH * 1.4f;
                    float rx = ux * std::cos(ang) + uy * std::sin(ang), ry = -ux * std::sin(ang) + uy * std::cos(ang);
                    float d = rx * rx / 4.0f + ry * ry / 1.0f;
                    if (d > 1.0f) continue;
                    id = h;
                    Color lf = season == 2 ? (id % 4 == 0 ? Color{214, 96, 40, 255} : id % 4 == 1 ? Color{232, 160, 52, 255} : id % 4 == 2 ? Color{176, 70, 40, 255} : Color{150, 104, 54, 255})
                                           : (id % 3 == 0 ? Color{118, 88, 50, 255} : id % 3 == 1 ? Color{142, 108, 62, 255} : Color{88, 108, 50, 255});
                    c = Lit(lf, (ry < 0 ? 1.08f : 0.9f) * (std::fabs(ry) < 0.15f ? 0.82f : 1.0f));   // a lit side and a vein
                }
            float root = std::fabs(N2(wx * 0.02f + 40, wy * 0.03f) - 0.5f);
            if (root < 0.009f && N2(wx * 0.05f, wy * 0.05f + 9) > 0.55f) c = Lit(Color{96, 68, 44, 255}, root < 0.004f ? 1.15f : 0.8f);   // roots
            float lt = Fbm(wx * 0.025f, wy * 0.04f);
            c = Lit(c, 0.86f + Smooth(0.48f, 0.68f, lt) * 0.34f);   // sunlight through the leaves
            return c;
        }
        case Venue::Park: {
            float px = std::fabs(wx - ARENA_W / 2), py = std::fabs(wy - ARENA_H / 2);
            if (px < 15 || py < 13) {
                Color c = BevelTile(wx, wy, 14, 12, Color{198, 192, 178, 255}, Color{186, 180, 166, 255}, Color{140, 134, 122, 255}, true, 18);
                if (px < 15 && px > 13.6f) c = Color{150, 144, 132, 255};
                return season == 3 && N2(wx * 0.1f, wy * 0.1f) > 0.45f ? MixF(c, Color{236, 240, 248, 255}, 0.8f) : c;
            }
            const float bx[2] = {110, ARENA_W - 110}, by[2] = {95, ARENA_H - 95};
            for (float x : bx)
                for (float y : by) {
                    float dx = wx - x, dy = wy - y, d = std::sqrt(dx * dx + dy * dy);
                    if (d < 28) {
                        if (d > 25) return Lit(Color{156, 146, 134, 255}, (dy < 0 ? 1.12f : 0.88f) * (0.95f + 0.1f * N2(wx * 0.4f, wy * 0.4f)));   // stone edging
                        if (season == 3) return N2(wx * 0.2f, wy * 0.2f) > 0.35f ? Color{234, 240, 248, 255} : Color{110, 80, 60, 255};
                        Color soil = Lit(Color{104, 76, 56, 255}, 0.9f + 0.2f * N2(wx * 0.5f, wy * 0.5f));
                        uint32_t id;
                        int f = Scatter(wx, wy, 3.2f, 0.75f, 0.8f, 1.3f, 601, &id);
                        if (f == 1 || f == 2) {
                            Color pet = id % 3 == 0 ? Color{240, 90, 110, 255} : id % 3 == 1 ? Color{255, 220, 90, 255} : Color{200, 140, 230, 255};
                            return f == 2 ? Lit(pet, 1.12f) : pet;
                        }
                        if (Blades(wx, wy, 3) > 0.55f) return Lit(Color{70, 130, 60, 255}, 0.8f + Blades(wx, wy, 4) * 0.4f);
                        return f == 3 ? Lit(soil, 0.8f) : soil;
                    }
                }
            Color c = Grass(season, wx, wy);
            if (season != 3) c = Lit(c, ((int)std::floor((wx + 15) / 36) & 1) ? 1.05f : 0.96f);   // mowing stripes
            return c;
        }
        case Venue::Mountain: {
            Color c = RockGround(wx, wy, Color{150, 132, 104, 255}, Color{140, 136, 128, 255}, 701);
            float patch = N2(wx * 0.03f + 77, wy * 0.05f);
            patch = Fbm(wx * 0.012f + 77, wy * 0.02f);
            float lim = season == 3 ? 0.44f : 0.58f;
            if (patch > lim - 0.04f) {
                Color top = season == 3 ? Grass(3, wx, wy) : Lit(Grass(season, wx, wy), 0.85f);
                float t = Smooth(lim - 0.04f, lim + 0.06f, patch);
                if (season != 3) t *= 0.55f + 0.45f * Blades(wx, wy, 5);   // tufty edges
                c = MixF(c, top, t);
            }
            uint32_t id;
            int p = Scatter(wx, wy, 9, 0.25f, 0.7f, 1.6f, 702, &id);
            if (p == 1) c = Color{130, 120, 108, 255}; else if (p == 2) c = Color{186, 176, 160, 255}; else if (p == 3) c = Lit(c, 0.82f);
            return c;
        }
        case Venue::Beach: {
            Color c = Sand(wx, wy, season == 3);
            float shore = 34 + N2(wx * 0.02f, 3) * 16;
            if (wy < shore + 10) c = MixF(c, Color{168, 150, 110, 255}, (shore + 10 - wy) / 10 * 0.6f);   // wet sand
            float foam = std::fabs(wy - shore);
            if (foam < 2.2f) c = MixF(c, Color{244, 248, 252, 255}, 1 - foam / 2.2f);
            if (wy < shore - 2) {   // the sea's edge
                float u = Clamp01((shore - 2 - wy) / 16);
                Color sea = MixF(Color{110, 196, 214, 255}, Color{60, 140, 196, 255}, u);
                float rip = std::sin(wx * 0.15f + wy * 0.6f + N2(wx * 0.04f, wy * 0.1f) * 5);
                if (rip > 0.8f) sea = Lit(sea, 1.15f);
                c = MixF(c, sea, Smooth(0, 3, shore - 2 - wy));
            }
            uint32_t id;
            int p = wy > shore + 4 ? Scatter(wx, wy, 16, 0.12f, 0.6f, 1.1f, 801, &id) : 0;
            if (p == 1 || p == 2) c = id % 2 ? Lit(Color{246, 200, 210, 255}, p == 2 ? 1.08f : 1.0f) : Lit(Color{160, 154, 146, 255}, p == 2 ? 1.15f : 1.0f);
            else if (p == 3) c = Lit(c, 0.9f);
            return c;
        }
        case Venue::City: {
            float loc[4];
            Color c = BevelTile(wx, wy, 22, 16, Color{152, 152, 158, 255}, Color{142, 142, 148, 255}, Color{100, 100, 106, 255}, true, 22, loc);
            float stain = N2(wx * 0.03f + 11, wy * 0.04f);
            if (stain > 0.66f) c = Lit(c, 0.92f);
            bool court = wx > 30 && wx < ARENA_W - 30 && wy > 28 && wy < ARENA_H - 28 &&
                         (wx < 32 || wx > ARENA_W - 32 || wy < 30 || wy > ARENA_H - 30 || std::fabs(wx - ARENA_W / 2) < 1);
            if (court) c = Lit(Color{226, 226, 216, 255}, 0.94f + 0.08f * N2(wx * 0.6f, wy * 0.6f));
            float mx = wx - ARENA_W * 0.72f, my = wy - ARENA_H * 0.28f, md = std::sqrt(mx * mx + my * my);
            if (md < 12) {
                c = md > 10.5f ? Color{64, 64, 70, 255} : (((int)std::floor(wx / 2.5f)) & 1 ? Color{100, 100, 108, 255} : Color{82, 82, 90, 255});
                if (md < 10.5f && my < -5) c = Lit(c, 1.1f);
            }
            return c;
        }
        case Venue::Bridge: {
            if (wy < 20 || wy > ARENA_H - 20) {   // water under the bridge
                Color c = MixF(Color{50, 110, 170, 255}, Color{90, 160, 210, 255}, N2(wx * 0.06f, wy * 0.3f));
                float rip = std::sin(wx * 0.2f + N2(wx * 0.05f, wy * 0.2f) * 6);
                if (rip > 0.85f) c = Lit(c, 1.2f);
                return c;
            }
            if (wy < 26 || wy > ARENA_H - 26) return Planks(wx, wy, 6, 200, Color{104, 72, 46, 255}, Color{50, 34, 22, 255}, 3);   // edge beams
            Color c = Planks(wx, wy, 10, 80, Color{164, 118, 76, 255}, Color{62, 42, 26, 255}, 1);
            if (season == 3 && N2(wx * 0.05f, wy * 0.08f) > 0.6f) c = Mix(c, WHITE, 0.6f);
            return c;
        }
        case Venue::Ship: return Planks(wx, wy, 8, 160, Color{184, 136, 90, 255}, Color{56, 40, 28, 255}, 2);
        case Venue::Dojo: {   // tatami: 80x40 mats, every other block turned
            int bx = (int)std::floor(wx / 80), by = (int)std::floor(wy / 80);
            bool turn = (bx + by) & 1;
            float lx = turn ? std::fmod(wy, 80.0f) * DEPTH : std::fmod(wx, 80.0f), ly = turn ? std::fmod(wx, 80.0f) : std::fmod(wy, 80.0f) * DEPTH;
            float lyy = std::fmod(ly, turn ? 40.0f : 40.0f * DEPTH);
            float edgeW = 2.2f;
            if (lyy < edgeW || lx < edgeW) return Lit(Color{48, 64, 44, 255}, lyy < 0.7f || lx < 0.7f ? 0.7f : 1.0f);   // cloth edging
            Color c = Color{198, 192, 134, 255};
            float weave = std::fmod(turn ? wx * 1.4f : wy * DEPTH * 1.4f, 1.0f);
            c = Lit(c, weave < 0.5f ? 1.03f : 0.95f);
            c = Lit(c, 0.94f + 0.1f * N2(turn ? wx * 0.05f : wx * 0.6f, turn ? wy * 0.6f : wy * 0.05f));
            return Shade(c, (int)((Hf(bx, by, 9) - 0.5f) * 14));
        }
        case Venue::Office: {
            float lx = std::fmod(wx, 40.0f), ly = std::fmod(wy, 40.0f);
            Color c = (((int)std::floor(wx / 40) + (int)std::floor(wy / 40)) & 1) ? Color{122, 132, 152, 255} : Color{112, 122, 142, 255};
            c = Lit(c, 0.95f + 0.1f * N2(wx * 0.9f, wy * 0.5f));   // carpet pile
            if (lx < 0.6f || ly * DEPTH < 0.5f) c = Lit(c, 0.85f);
            if (std::fabs(wy - ARENA_H / 2) < 46) {   // a long carpet runner with a border
                float e = std::fabs(wy - ARENA_H / 2);
                c = e > 40 ? Lit(Color{206, 174, 92, 255}, e > 44.5f ? 0.85f : 1.0f) : Lit(Color{150, 58, 70, 255}, 0.92f + 0.14f * N2(wx * 0.8f, wy * 0.4f));
                if (e < 36 && e > 34) c = Color{180, 140, 80, 255};
            }
            float dx = wx - ARENA_W / 2, dy = wy - ARENA_H / 2, d = std::sqrt(dx * dx + dy * dy);
            if (d < 30 && d > 26) c = Color{206, 174, 92, 255};
            if (d < 22 && std::fabs(dx + dy * 0.3f) < 4) c = Color{206, 174, 92, 255};
            return c;
        }
        case Venue::Tower: {
            Color c = BevelTile(wx, wy, 32, 30, Color{94, 84, 106, 255}, Color{84, 76, 96, 255}, Color{50, 44, 60, 255}, true, 20);
            float mist = Fbm(wx * 0.015f + 5, wy * 0.025f);
            if (mist > 0.55f) c = MixF(c, Color{170, 150, 200, 255}, (mist - 0.55f) * 1.5f);
            float cr = CrackMask(wx, wy, 36, 909, 0.7f);
            if (cr > 0 && N2(wx * 0.02f, wy * 0.02f) > 0.5f) c = MixF(c, Color{40, 34, 48, 255}, cr);
            return c;
        }
        case Venue::Mansion: {
            int col = (int)std::floor(wx / 30), row = (int)std::floor(wy / 30);
            if (Hf(col, row, 1) < 0.07f) {   // a broken tile: rubble
                uint32_t id;
                int p = Scatter(wx, wy, 3, 0.8f, 0.6f, 1.2f, 1001, &id);
                return p == 2 ? Color{150, 140, 130, 255} : p == 1 ? Color{110, 100, 94, 255} : Color{58, 48, 48, 255};
            }
            Color c = BevelTile(wx, wy, 30, 30, Color{142, 52, 56, 255}, Color{214, 204, 186, 255}, Color{70, 56, 52, 255}, false, 10);
            float vein = std::fabs(N2(wx * 0.06f + col * 3, wy * 0.06f) - 0.5f);
            if (vein < 0.009f) c = Lit(c, 0.88f);
            float grime = Fbm(wx * 0.02f, wy * 0.03f);
            if (grime > 0.58f) c = MixF(c, Color{90, 76, 64, 255}, (grime - 0.58f) * 1.4f);
            return c;
        }
        case Venue::Volcano: return LavaRock(wx, wy, 1101, 0.75f);
        case Venue::Hideout: {
            if (wx < 10 || wx > ARENA_W - 10 || wy < 14 || wy > ARENA_H - 14) {
                bool stripe = ((int)std::floor((wx + wy * DEPTH) / 6)) & 1;
                return Lit(stripe ? Color{232, 192, 42, 255} : Color{38, 38, 40, 255}, 0.92f + 0.1f * N2(wx * 0.3f, wy * 0.3f));
            }
            int col = (int)std::floor(wx / 40), row = (int)std::floor(wy / 40);
            float lx = std::fmod(wx, 40.0f), ly = std::fmod(wy, 40.0f);
            if (Hf(col, row, 2) < 0.045f) {   // spinner arrow tiles
                bool arrow = (std::fabs(lx - 20) < (ly - 12) * 0.6f && ly > 12 && ly < 24) || (std::fabs(lx - 20) < 2.5f && ly >= 24 && ly < 32);
                Color c = arrow ? Color{240, 240, 240, 255} : Color{70, 110, 170, 255};
                if (lx < 0.8f || ly * DEPTH < 0.6f) return Color{50, 70, 110, 255};
                return Lit(c, ly * DEPTH < 1.5f ? 1.12f : 1.0f);
            }
            Color c = BevelTile(wx, wy, 40, 40, Color{110, 114, 122, 255}, Color{104, 108, 116, 255}, Color{62, 64, 70, 255}, false, 12);
            float rx = std::min(lx, 40 - lx), ry = std::min(ly, 40 - ly);
            if (rx > 2.2f && rx < 4.0f && ry * DEPTH > 1.4f && ry * DEPTH < 3.0f) c = Color{158, 162, 170, 255};
            return c;
        }
        case Venue::Lab: {
            Color c = BevelTile(wx, wy, 24, 24, Color{234, 236, 240, 255}, Color{224, 226, 232, 255}, Color{192, 196, 204, 255}, false, 6);
            float dx = wx - ARENA_W / 2, dy = wy - ARENA_H / 2, d = std::sqrt(dx * dx + dy * dy);
            if (d < 64) {   // a Poke Ball painted on the lab floor
                if (d > 60) c = Color{60, 60, 70, 255};
                else if (std::fabs(dy) < 4) c = Color{60, 60, 70, 255};
                else c = Lit(dy < 0 ? Color{222, 72, 72, 255} : Color{242, 242, 246, 255}, 0.94f + 0.08f * N2(wx * 0.3f, wy * 0.3f));
                if (d < 16) c = d > 12 ? Color{60, 60, 70, 255} : Color{250, 250, 252, 255};
            } else if (std::fabs(wy - ARENA_H / 2) < 5) c = Color{120, 170, 230, 255};
            return c;
        }
        case Venue::Cave: case Venue::IceCave: {
            bool ice = v == Venue::IceCave;
            Color c = ice ? IceSurface(wx, wy, Color{184, 210, 232, 255}, 1201) : RockGround(wx, wy, Color{98, 90, 84, 255}, Color{86, 80, 76, 255}, 1202);
            uint32_t id;
            int p = Scatter(wx, wy, 10, 0.2f, 0.6f, 1.5f, 1203, &id);
            if (p == 1) c = ice ? Color{200, 222, 240, 255} : Color{80, 74, 70, 255};
            else if (p == 2) c = ice ? WHITE : Color{130, 122, 116, 255};
            else if (p == 3) c = Lit(c, 0.82f);
            if (!ice && n == 99 && Hf((int)(wx * 0.5f), (int)(wy * 0.5f), 77) < 0.012f) c = Color{120, 220, 240, 255};   // a glinting crystal
            return c;
        }
        default: return Color{200, 180, 130, 255};
    }
}

// The view beyond the floor: sky and scenery outdoors, walls indoors.
void VenueSurroundings(Image* img, Venue v, int season) {
    auto sky = [&](int h) {
        Color top = season == 3 ? Color{170, 190, 214, 255} : season == 2 ? Color{230, 170, 120, 255} : Color{120, 180, 240, 255};
        Color bot = season == 3 ? Color{214, 224, 236, 255} : season == 2 ? Color{246, 210, 160, 255} : Color{190, 226, 250, 255};
        for (int y = 0; y < h; y++) ImageDrawRectangle(img, 0, y, VW, 1, Mix(top, bot, y / (float)std::max(1, h - 1)));
    };
    auto treeline = [&](int base, bool dense) {
        Color leaf = season == 2 ? Color{170, 96, 50, 255} : season == 3 ? Color{150, 170, 160, 255} : Color{50, 110, 56, 255};
        for (int x = -6; x < VW + 6; x += dense ? 6 : 10) {
            int h = 8 + (int)(Noise(x * 0.07f, 1.0f) * (dense ? 16 : 10));
            int r = dense ? 7 : 6;
            ImageDrawCircle(img, x, base - h + r, r, Shade(leaf, (int)(Hash2(x, 5) * 30) - 15));
            ImageDrawRectangle(img, x - r, base - h + r, r * 2, h, Shade(leaf, -20));
            if (season == 3) ImageDrawRectangle(img, x - r + 2, base - h + 1, r * 2 - 4, 2, Color{240, 246, 255, 255});
        }
    };
    auto fence = [&](int y, Color c) {
        ImageDrawRectangle(img, 0, y, VW, 2, c);
        ImageDrawRectangle(img, 0, y + 5, VW, 2, c);
        for (int x = 2; x < VW; x += 14) ImageDrawRectangle(img, x, y - 2, 3, 10, Shade(c, -20));
    };
    auto sides = [&](Color c, int brickW, bool bricks) {
        if (bricks) {
            Bricks(img, 0, FLOOR_Y, ARENA_SX, FLOOR_BOTTOM - FLOOR_Y + 8, c, brickW, 5);
            Bricks(img, ARENA_SX + (int)ARENA_W, FLOOR_Y, VW - ARENA_SX - (int)ARENA_W, FLOOR_BOTTOM - FLOOR_Y + 8, c, brickW, 5);
        } else {
            ImageDrawRectangle(img, 0, FLOOR_Y, ARENA_SX, FLOOR_BOTTOM - FLOOR_Y + 8, c);
            ImageDrawRectangle(img, ARENA_SX + (int)ARENA_W, FLOOR_Y, VW - ARENA_SX - (int)ARENA_W, FLOOR_BOTTOM - FLOOR_Y + 8, c);
            for (int y = FLOOR_Y; y < FLOOR_BOTTOM + 8; y += 3) {
                ImageDrawPixel(img, R(ARENA_SX), y, Shade(c, 20));
                ImageDrawPixel(img, ARENA_SX + (int)ARENA_W + R(VW - ARENA_SX - (int)ARENA_W), y + 1, Shade(c, -20));
            }
        }
    };
    auto ledge = [&](Color c) {
        ImageDrawRectangle(img, 0, FLOOR_BOTTOM, VW, 8, c);
        ImageDrawRectangle(img, 0, FLOOR_BOTTOM, VW, 1, Shade(c, 40));
        for (int x = 0; x < VW; x += 2) if (R(4) == 0) ImageDrawPixel(img, x, FLOOR_BOTTOM + 1 + R(6), Shade(c, R(2) ? 18 : -18));
        ImageDrawRectangle(img, 0, FLOOR_BOTTOM + 8, VW, 1, Color{10, 8, 18, 255});
    };
    auto rockWall = [&](Color c, int h) {
        for (int y = 0; y < h; y++)
            for (int x = 0; x < VW; x++) ImageDrawPixel(img, x, y, Shade(c, (int)((Fbm(x * 0.06f, y * 0.12f) - 0.5f) * 50) - (h - y) / 3));
        for (int x = 4; x < VW; x += 9 + R(14)) {   // stalactites
            int l = 3 + R(8);
            for (int k = 0; k < l; k++) ImageDrawRectangle(img, x - (l - k) / 4, h - 2 + k - l, std::max(1, (l - k) / 2), 1, Shade(c, -10));
        }
    };
    Color hedge = season == 3 ? Color{200, 210, 220, 255} : season == 2 ? Color{140, 100, 50, 255} : Color{56, 110, 50, 255};
    switch (v) {
        case Venue::Route: case Venue::Park:
            sky(FLOOR_Y);
            treeline(FLOOR_Y - 4, false);
            fence(FLOOR_Y - 8, Color{170, 130, 90, 255});
            sides(hedge, 0, false);
            ledge(hedge);
            break;
        case Venue::Forest:
            sky(FLOOR_Y);
            treeline(FLOOR_Y - 10, true);
            treeline(FLOOR_Y, true);
            sides(Shade(hedge, -20), 0, false);
            ledge(Shade(hedge, -20));
            break;
        case Venue::Mountain:
            sky(FLOOR_Y);
            for (int x = 0; x < VW; x++) {   // peaks
                int h = 6 + (int)(Noise(x * 0.02f, 3.0f) * 24);
                for (int y = FLOOR_Y - h; y < FLOOR_Y; y++)
                    ImageDrawPixel(img, x, y, y < FLOOR_Y - h + 4 ? Color{240, 244, 250, 255} : Shade(Color{120, 110, 104, 255}, (int)(Hash2(x, y) * 14)));
            }
            sides(Color{120, 106, 90, 255}, 0, false);
            ledge(Color{110, 96, 80, 255});
            break;
        case Venue::Beach:
            sky(FLOOR_Y - 10);
            for (int y = FLOOR_Y - 10; y < FLOOR_Y; y++) ImageDrawRectangle(img, 0, y, VW, 1, Mix(Color{60, 130, 190, 255}, Color{90, 160, 210, 255}, (y - FLOOR_Y + 10) / 10.0f));
            for (int x = 0; x < VW; x += 3) if (R(3) == 0) ImageDrawPixel(img, x, FLOOR_Y - 1 - R(9), Color{220, 240, 255, 255});
            sides(Color{214, 196, 146, 255}, 0, false);
            ledge(Color{200, 180, 130, 255});
            break;
        case Venue::Bridge:
            sky(FLOOR_Y - 6);
            treeline(FLOOR_Y - 8, false);
            for (int y = FLOOR_Y - 6; y < FLOOR_Y; y++) ImageDrawRectangle(img, 0, y, VW, 1, Color{70, 140, 196, 255});
            for (int x = 6; x < VW; x += 40) { ImageDrawRectangle(img, x, FLOOR_Y - 12, 3, 12, Color{110, 76, 46, 255}); }   // posts
            ImageDrawRectangle(img, 0, FLOOR_Y - 10, VW, 1, Color{200, 180, 140, 255});   // rope
            sides(Color{70, 140, 196, 255}, 0, false);
            ledge(Color{60, 120, 180, 255});
            break;
        case Venue::Ship:
            sky(FLOOR_Y - 8);
            for (int y = FLOOR_Y - 8; y < FLOOR_Y; y++) ImageDrawRectangle(img, 0, y, VW, 1, Color{50, 110, 170, 255});
            ImageDrawRectangle(img, 0, FLOOR_Y - 12, VW, 2, WHITE);   // the railing
            for (int x = 3; x < VW; x += 12) ImageDrawRectangle(img, x, FLOOR_Y - 12, 2, 12, Color{230, 230, 236, 255});
            ImageDrawCircle(img, 120, FLOOR_Y - 6, 5, Color{240, 90, 70, 255});   // a life ring
            ImageDrawCircle(img, 120, FLOOR_Y - 6, 2, Color{50, 110, 170, 255});
            sides(Color{236, 236, 240, 255}, 0, false);
            ledge(Color{220, 220, 226, 255});
            break;
        case Venue::City:
            sky(FLOOR_Y);
            for (int x = 0; x < VW;) {   // building fronts with windows
                int w = 30 + R(40), h = 16 + R(20);
                Color b = Shade(R(3) == 0 ? Color{190, 150, 120, 255} : R(2) ? Color{150, 160, 176, 255} : Color{200, 196, 186, 255}, R(20) - 10);
                ImageDrawRectangle(img, x, FLOOR_Y - h, w, h, b);
                for (int wy = FLOOR_Y - h + 3; wy < FLOOR_Y - 4; wy += 6)
                    for (int wx = x + 3; wx < x + w - 4; wx += 7) ImageDrawRectangle(img, wx, wy, 4, 3, R(5) == 0 ? Color{250, 230, 140, 255} : Color{90, 120, 160, 255});
                x += w + 2;
            }
            sides(Color{150, 150, 156, 255}, 10, true);
            ledge(Color{130, 130, 136, 255});
            break;
        case Venue::Cave: rockWall(Color{80, 72, 66, 255}, FLOOR_Y); sides(Color{70, 62, 58, 255}, 0, false); ledge(Color{66, 58, 54, 255}); break;
        case Venue::IceCave: rockWall(Color{150, 186, 214, 255}, FLOOR_Y); sides(Color{140, 176, 206, 255}, 0, false); ledge(Color{130, 166, 196, 255}); break;
        case Venue::Volcano:
            rockWall(Color{70, 50, 46, 255}, FLOOR_Y);
            for (int x = 0; x < VW; x += 2) if (R(6) == 0) ImageDrawPixel(img, x, FLOOR_Y - 1 - R(10), Color{240, 120, 40, 255});
            sides(Color{60, 46, 44, 255}, 0, false);
            ledge(Color{56, 42, 40, 255});
            break;
        case Venue::Dojo:
            for (int y = 0; y < FLOOR_Y; y++) ImageDrawRectangle(img, 0, y, VW, 1, Color{236, 228, 206, 255});
            for (int x = 0; x < VW; x += 20) ImageDrawRectangle(img, x, 0, 2, FLOOR_Y, Color{110, 76, 46, 255});   // shoji frames
            for (int y = 6; y < FLOOR_Y; y += 9) ImageDrawRectangle(img, 0, y, VW, 1, Color{150, 110, 70, 255});
            ImageDrawRectangle(img, 0, FLOOR_Y - 3, VW, 3, Color{110, 76, 46, 255});
            sides(Color{130, 92, 56, 255}, 12, true);
            ledge(Color{120, 84, 52, 255});
            break;
        case Venue::Office:
            for (int y = 0; y < FLOOR_Y; y++) ImageDrawRectangle(img, 0, y, VW, 1, Mix(Color{90, 140, 190, 255}, Color{160, 200, 230, 255}, y / (float)FLOOR_Y));
            for (int x = 0; x < VW; x += 32) ImageDrawRectangle(img, x, 0, 2, FLOOR_Y, Color{70, 76, 90, 255});   // window frames
            for (int x = 2; x < VW; x += 9) { int h = 6 + R(14); ImageDrawRectangle(img, x, FLOOR_Y - 3 - h, 7, h, Color{120, 150, 180, 255}); }   // skyline
            ImageDrawRectangle(img, 0, FLOOR_Y - 3, VW, 3, Color{70, 76, 90, 255});
            sides(Color{110, 116, 130, 255}, 0, false);
            ledge(Color{90, 96, 110, 255});
            break;
        case Venue::Tower:
            Bricks(img, 0, 0, VW, FLOOR_Y, Color{70, 62, 84, 255}, 12, 5);
            for (int x = 40; x < VW; x += 90) {   // candles
                ImageDrawRectangle(img, x, FLOOR_Y - 12, 3, 8, Color{230, 226, 210, 255});
                ImageDrawRectangle(img, x, FLOOR_Y - 15, 3, 3, Color{180, 140, 255, 255});
            }
            sides(Color{64, 56, 78, 255}, 10, true);
            ledge(Color{60, 52, 74, 255});
            break;
        case Venue::Mansion:
            for (int y = 0; y < FLOOR_Y; y++)
                for (int x = 0; x < VW; x++) ImageDrawPixel(img, x, y, ((x / 6) % 2) ? Color{120, 50, 54, 255} : Color{104, 42, 46, 255});
            for (int k = 0; k < 14; k++) { int x = R(VW), y = R(FLOOR_Y - 8); ImageDrawRectangle(img, x, y, 4 + R(10), 3 + R(6), Color{170, 150, 130, 255}); }   // peeling plaster
            ImageDrawRectangle(img, 0, FLOOR_Y - 4, VW, 4, Color{80, 60, 50, 255});
            sides(Color{110, 90, 80, 255}, 10, true);
            ledge(Color{100, 80, 70, 255});
            break;
        case Venue::Hideout:
            for (int y = 0; y < FLOOR_Y; y++) ImageDrawRectangle(img, 0, y, VW, 1, Color{74, 78, 88, 255});
            for (int x = 0; x < VW; x += 24) ImageDrawRectangle(img, x, 0, 1, FLOOR_Y, Color{50, 54, 62, 255});
            ImageDrawRectangle(img, 0, 8, VW, 3, Color{120, 124, 134, 255});   // pipes
            ImageDrawRectangle(img, 0, 18, VW, 2, Color{150, 70, 60, 255});
            for (int x = 60; x < VW; x += 160) { ImageDrawRectangle(img, x, 22, 14, 10, Color{170, 30, 30, 255}); }
            sides(Color{90, 94, 104, 255}, 0, false);
            ledge(Color{80, 84, 94, 255});
            break;
        case Venue::Lab:
            for (int y = 0; y < FLOOR_Y; y++) ImageDrawRectangle(img, 0, y, VW, 1, Color{226, 230, 236, 255});
            for (int x = 10; x < VW; x += 70) {   // shelves and machines
                ImageDrawRectangle(img, x, 8, 40, FLOOR_Y - 10, Color{190, 196, 206, 255});
                for (int k = 0; k < 4; k++) ImageDrawRectangle(img, x + 4 + k * 9, 12, 6, 6, k % 2 ? Color{90, 150, 230, 255} : Color{230, 90, 90, 255});
                ImageDrawRectangle(img, x + 4, 22, 32, 2, Color{120, 126, 136, 255});
            }
            sides(Color{200, 204, 212, 255}, 0, false);
            ledge(Color{180, 184, 192, 255});
            break;
        default: break;
    }
}

// A legendary's lair is drawn like a place, not a stadium: its surroundings and its floor.
Venue LairVenue(ArenaKind k) {
    switch (k) {
        case ArenaKind::Seafoam: return Venue::IceCave;
        case ArenaKind::PowerPlant: return Venue::Hideout;
        case ArenaKind::Ember: return Venue::Volcano;
        case ArenaKind::Unknown: return Venue::Cave;
        case ArenaKind::Faraway: return Venue::Forest;
        default: return Venue::Stadium;
    }
}
Color LairFloor(ArenaKind k, float wx, float wy) {
    switch (k) {
        case ArenaKind::PowerPlant: {   // steel plates, cable channels, warning stripes
            if (wx < 10 || wx > ARENA_W - 10 || wy < 12 || wy > ARENA_H - 12) {
                bool stripe = ((int)std::floor((wx + wy * DEPTH) / 6)) & 1;
                return Lit(stripe ? Color{240, 200, 40, 255} : Color{30, 30, 32, 255}, 0.92f + 0.1f * N2(wx * 0.3f, wy * 0.3f));
            }
            Color c = BevelTile(wx, wy, 40, 40, Color{76, 80, 88, 255}, Color{70, 74, 82, 255}, Color{42, 44, 50, 255}, false, 10);
            float ch = std::fabs(std::fmod(wy + 20, 120.0f) - 60);
            if (ch < 3) c = Lit(Color{40, 40, 44, 255}, ch < 2.2f ? 1.0f : 1.3f);   // cable channel
            if (ch < 1) c = Lit(Color{230, 190, 40, 255}, 0.9f + 0.2f * N2(wx * 0.2f, 1));
            float oil = N2(wx * 0.04f + 5, wy * 0.06f);
            if (oil > 0.7f) c = Lit(c, 0.88f);
            return c;
        }
        case ArenaKind::Unknown: {   // dark cave rock, violet crystal glints
            Color c = VenueFloor(Venue::Cave, 1, wx, wy);
            c = Mix(c, Color{60, 40, 90, 255}, 0.35f);
            float glow = N2(wx * 0.04f + 13, wy * 0.06f);
            if (glow > 0.7f) c = MixF(c, Color{120, 80, 170, 255}, (glow - 0.7f) * 1.5f);
            if (Speck(wx, wy, 4401) == 0 && Hf((int)wx, (int)wy, 3) < 0.25f) c = Color{210, 160, 255, 255};
            return c;
        }
        case ArenaKind::Faraway: return VenueFloor(Venue::Forest, 1, wx, wy);
        case ArenaKind::Seafoam: return VenueFloor(Venue::IceCave, 3, wx, wy);
        case ArenaKind::Ember: {
            Color c = VenueFloor(Venue::Volcano, 1, wx, wy);
            float sc = N2(wx * 0.05f + 30, wy * 0.07f);
            if (sc > 0.7f) c = MixF(c, Color{110, 46, 30, 255}, (sc - 0.7f) * 2.5f);   // scorched ground
            return c;
        }
        default: return Color{100, 100, 100, 255};
    }
}

// ---------------------------------------------------------------- Hi-res drawing on the baked floor

constexpr int S = CANVAS_ZOOM;
struct Canvas { Color* px; int w, h; };

inline void Blend(Color& d, Color s, float a) {
    if (a >= 1) { d = s; return; }
    if (a <= 0) return;
    d.r = (unsigned char)(d.r + (s.r - d.r) * a);
    d.g = (unsigned char)(d.g + (s.g - d.g) * a);
    d.b = (unsigned char)(d.b + (s.b - d.b) * a);
}
inline float TexWX(int X) { return (X + 0.5f) / S - ARENA_SX; }
inline float TexWY(int Y) { return ((Y + 0.5f) / S - FLOOR_Y) / DEPTH; }
const int FX0 = ARENA_SX * S, FX1 = (ARENA_SX + (int)ARENA_W) * S, FY0 = FLOOR_Y * S, FY1 = FLOOR_BOTTOM * S;

// A rectangle in logical units, drawn chunky (pixel art details over the floor: spikes, trims).
void SRect(Image* img, int x, int y, int w, int h, Color c) { ImageDrawRectangle(img, x * S, y * S, w * S, h * S, c); }

// Fill a floor ellipse (world coords) texel by texel; 'rim' is drawn on its outer edge (screen units).
template <typename F>
void HiEllipse(Canvas& cv, const Zone& z, F colorAt, Color rim, float rimW) {
    int x0 = std::max(FX0, (int)((ARENA_SX + z.pos.x - z.rx * 1.3f) * S) - 2), x1 = std::min(FX1 - 1, (int)((ARENA_SX + z.pos.x + z.rx * 1.3f) * S) + 2);
    int y0 = std::max(FY0, (int)((FLOOR_Y + (z.pos.y - z.ry * 1.3f) * DEPTH) * S) - 2), y1 = std::min(FY1 - 1, (int)((FLOOR_Y + (z.pos.y + z.ry * 1.3f) * DEPTH) * S) + 2);
    for (int Y = y0; Y <= y1; Y++)
        for (int X = x0; X <= x1; X++) {
            float wx = TexWX(X), wy = TexWY(Y);
            float dx = (wx - z.pos.x) / z.rx, dy = (wy - z.pos.y) / z.ry;
            float wob = Wobble(z.seed, std::atan2(dy, dx));
            float d = std::sqrt(dx * dx + dy * dy) / wob;
            float edge = (1.0f - d) * std::min(z.rx, z.ry * DEPTH) * wob;
            if (edge < -0.4f) continue;
            Color c = edge < rimW ? Lit(rim, 0.85f + 0.3f * Clamp01(edge / rimW)) : colorAt(wx, wy, edge);
            Blend(cv.px[Y * cv.w + X], c, edge < 0 ? 1.0f + edge / 0.4f : 1.0f);
        }
}

// A painted ring on the floor (world centre and radius), anti-aliased.
void HiRing(Canvas& cv, float cx, float cy, float r, float halfW, Color col, float alpha) {
    int x0 = std::max(FX0, (int)((ARENA_SX + cx - r - 3) * S)), x1 = std::min(FX1 - 1, (int)((ARENA_SX + cx + r + 3) * S));
    int y0 = std::max(FY0, (int)((FLOOR_Y + (cy - r - 3) * DEPTH) * S)), y1 = std::min(FY1 - 1, (int)((FLOOR_Y + (cy + r + 3) * DEPTH) * S));
    for (int Y = y0; Y <= y1; Y++)
        for (int X = x0; X <= x1; X++) {
            float wx = TexWX(X), wy = TexWY(Y);
            float d = std::sqrt((wx - cx) * (wx - cx) + (wy - cy) * (wy - cy));
            float e = std::fabs(d - r);
            if (e > halfW + 0.6f) continue;
            float a = Clamp01((halfW + 0.3f - e) / 0.6f) * alpha;
            a *= 0.85f + 0.15f * N2(wx * 0.5f, wy * 0.5f);   // a little worn
            Blend(cv.px[Y * cv.w + X], col, a);
        }
}

// Soft baked light: shade under the walls, a gentle vignette, cloud light outdoors.
float FloorLight(bool outdoor, float wx, float wy) {
    float k = 0.76f + 0.24f * Smooth(0, 44, wy);
    k *= 0.86f + 0.14f * Smooth(0, 18, std::min(wx, ARENA_W - wx));
    k *= 0.93f + 0.07f * Smooth(0, 22, ARENA_H - wy);
    float dx = (wx - ARENA_W / 2) / (ARENA_W / 2), dy = (wy - ARENA_H / 2) / (ARENA_H / 2);
    k *= 1.0f - 0.07f * (dx * dx + dy * dy);
    if (outdoor) k *= 0.94f + 0.12f * Fbm(wx * 0.006f + 3, wy * 0.01f);
    return k * 1.03f;
}

// Run 'rowFn(Y)' for every floor row on a few threads (the bake is pure: no shared state).
template <typename F>
void ForRows(int y0, int y1, F rowFn) {
    unsigned n = std::max(1u, std::min(8u, std::thread::hardware_concurrency()));
    std::vector<std::thread> ts;
    for (unsigned t = 0; t < n; t++)
        ts.emplace_back([=, &rowFn]() { for (int Y = y0 + (int)t; Y < y1; Y += (int)n) rowFn(Y); });
    for (auto& t : ts) t.join();
}

Image BuildImage(ArenaKind kind, const Field* f) {
    Image img = GenImageColor(VW, VH, Color{20, 18, 32, 255});
    Theme th = ThemeFor(kind);
    gRng = 1234567u + (unsigned)kind * 7919u;

    const Color shirts[] = {{220, 80, 80, 255},  {80, 140, 220, 255}, {240, 200, 80, 255}, {100, 190, 110, 255},
                            {200, 120, 200, 255}, {240, 240, 240, 255}, {240, 140, 60, 255}, {90, 200, 200, 255}};
    const Color skins[] = {{240, 200, 160, 255}, {200, 150, 110, 255}, {140, 95, 65, 255}, {250, 220, 190, 255}};
    for (int row = 0; row < STANDS_H / 6; row++) {
        int y = row * 6;
        ImageDrawRectangle(&img, 0, y, VW, 6, (row % 2) ? th.standA : th.standB);
        for (int x = 1 + R(3); x < VW - 3; x += 4 + R(3)) {
            if (R(10) < 7) {
                ImageDrawRectangle(&img, x, y + 3, 3, 3, shirts[R(8)]);
                ImageDrawRectangle(&img, x + 1, y + 1, 1, 2, skins[R(4)]);
            }
        }
    }
    ImageDrawRectangle(&img, 0, STANDS_H - 2, VW, 2, Shade(th.wall, 40));
    Bricks(&img, 0, STANDS_H, VW, FLOOR_Y - STANDS_H, th.wall, 12, 4);
    ImageDrawRectangle(&img, 0, STANDS_H, VW, 1, Shade(th.wall, 60));
    Bricks(&img, 0, FLOOR_Y, ARENA_SX, FLOOR_BOTTOM - FLOOR_Y + 8, th.side, 10, 5);
    Bricks(&img, ARENA_SX + (int)ARENA_W, FLOOR_Y, VW - ARENA_SX - (int)ARENA_W, FLOOR_BOTTOM - FLOOR_Y + 8, th.side, 10, 5);
    Bricks(&img, 0, FLOOR_BOTTOM, VW, 8, th.ledge, 14, 4);
    ImageDrawRectangle(&img, 0, FLOOR_BOTTOM, VW, 1, Shade(th.ledge, 60));
    ImageDrawRectangle(&img, 0, FLOOR_BOTTOM + 8, VW, 1, Color{10, 8, 18, 255});

    Venue venue = f && kind == ArenaKind::Stadium ? f->venue : Venue::Stadium;
    int season = f ? f->season : -1;
    bool lair = IsLegendArena(kind);
    bool outdoor = false;
    if (lair) {
        gRng = 1357911u + (unsigned)kind * 7919u;
        VenueSurroundings(&img, LairVenue(kind), kind == ArenaKind::Seafoam ? 3 : 1);
        outdoor = !VenueIndoors(LairVenue(kind));
    } else if (venue != Venue::Stadium) {
        gRng = 7654321u + (unsigned)venue * 104729u;
        VenueSurroundings(&img, venue, season);
        outdoor = !VenueIndoors(venue);
    }

    // Everything above is pixel art at the logical size; the floor is baked at full resolution.
    ImageResizeNN(&img, VW * S, VH * S);
    Canvas cv{(Color*)img.data, img.width, img.height};
    auto floorAt = [&](float wx, float wy) -> Color {
        if (lair) return LairFloor(kind, wx, wy);
        if (venue != Venue::Stadium) return VenueFloor(venue, season, wx, wy);
        return FloorBase(kind, wx, wy);
    };
    ForRows(FY0, FY1, [&](int Y) {
        float wy = TexWY(Y);
        for (int X = FX0; X < FX1; X++) cv.px[Y * cv.w + X] = floorAt(TexWX(X), wy);
    });

    if (f) {
        for (const Zone& z : f->zones) {
            switch (z.kind) {
                case Zone::Platform: {   // islands: each its own sand colour, wet shore, grass and pebbles
                    float tint = 1.0f + ((int)(z.seed % 21) - 10) * 0.008f;
                    bool grassy = z.seed % 3 != 0, rocky = z.seed % 4 == 1;
                    HiEllipse(cv, z, [&](float wx, float wy, float edge) {
                        Color c = Lit(Sand(wx, wy, false), tint * 0.9f);
                        if (edge < 6) c = MixF(c, Color{120, 110, 90, 255}, (1 - edge / 6) * 0.55f);   // wet sand at the shore
                        else if (grassy && edge > 9 && N2(wx * 0.08f + z.seed % 97, wy * 0.08f) > 0.55f) {
                            Color gr = Grass(1, wx, wy);
                            float t = Smooth(0.55f, 0.6f, N2(wx * 0.08f + z.seed % 97, wy * 0.08f));
                            if (Blades(wx, wy, 2) < 0.45f) t *= 0.3f;
                            c = MixF(c, gr, t);
                        }
                        uint32_t id;
                        int p = rocky && edge > 4 ? Scatter(wx, wy, 6, 0.35f, 0.7f, 1.6f, z.seed, &id) : 0;
                        if (p == 1) c = Color{130, 126, 120, 255}; else if (p == 2) c = Color{176, 172, 166, 255}; else if (p == 3) c = Lit(c, 0.85f);
                        if (edge > 4 && Speck(wx, wy, z.seed) == 0 && Hf((int)wx, (int)wy, z.seed) < 0.2f) c = Color{240, 200, 210, 255};   // a shell
                        return c;
                    }, Color{110, 96, 72, 255}, 2.0f);
                } break;
                case Zone::TallGrass:
                    HiEllipse(cv, z, [](float wx, float wy, float edge) {
                        float v = Blades(wx, wy, 11);
                        float v2 = Blades(wx * 0.8f + 9, wy, 12);
                        Color c = Lit(Color{66, 134, 56, 255}, 0.7f + v * 0.5f);
                        if (v2 > 0.66f) c = MixF(c, Color{140, 200, 90, 255}, (v2 - 0.66f) * 2.5f);
                        if (edge < 3) c = Lit(c, 0.85f);
                        return c;
                    }, Color{46, 96, 40, 255}, 1.2f);
                    break;
                case Zone::PoisonPool:
                    HiEllipse(cv, z, [](float wx, float wy, float edge) {
                        float sw = Fbm(wx * 0.06f + std::sin(wy * 0.05f) * 2, wy * 0.08f);
                        Color c = MixF(Color{110, 44, 140, 255}, Color{160, 74, 190, 255}, sw * 1.4f - 0.2f);
                        uint32_t id;
                        int p = Scatter(wx, wy, 6, 0.4f, 0.6f, 1.4f, 707, &id);
                        if (p == 2) c = Color{230, 180, 250, 255};
                        else if (p == 1) c = Lit(c, 1.15f);
                        if (edge < 3) c = Lit(c, 0.75f + edge * 0.08f);
                        float sheen = std::sin(wx * 0.15f - wy * 0.2f + sw * 4);
                        if (sheen > 0.85f) c = MixF(c, Color{240, 200, 255, 255}, (sheen - 0.85f) * 3);
                        return c;
                    }, Color{58, 38, 44, 255}, 1.8f);
                    break;
                case Zone::LavaPool:
                    HiEllipse(cv, z, [](float wx, float wy, float edge) {
                        Wor w = Worley(wx / 9.0f, wy / 11.0f, 808);
                        float e = (w.f2 - w.f1) * 9.0f;
                        Color c = MixF(Color{255, 120, 30, 255}, Color{255, 220, 110, 255}, Clamp01(1.0f - e * 0.6f));
                        if (e > 1.6f && (w.id & 3) == 0) c = MixF(Color{120, 40, 24, 255}, Color{170, 60, 30, 255}, N2(wx * 0.4f, wy * 0.4f));   // floating crust
                        if (edge < 5) c = MixF(c, Color{110, 36, 22, 255}, (1 - edge / 5) * 0.8f);   // cooling crust at the edge
                        return c;
                    }, Color{60, 26, 18, 255}, 2.0f);
                    break;
            }
        }
        if (f->spikeBand > 0) {  // toxic spikes along the edges
            ForRows(FY0, FY1, [&](int Y) {
                float wy = TexWY(Y);
                for (int X = FX0; X < FX1; X++)
                    if (f->InSpikes({TexWX(X), wy})) cv.px[Y * cv.w + X] = Lit(cv.px[Y * cv.w + X], 0.68f);
            });
            const Color sd{110, 40, 140, 255}, sm{170, 80, 200, 255}, sl{235, 170, 250, 255};
            auto spike = [&](int x, int y, int h) {  // upward pixel spike, base at y
                for (int i = 0; i < h; i++) {
                    int w = std::max(1, (h - i) * 5 / h);
                    SRect(&img, x - w / 2, y - i, w, 1, i < h / 3 ? sd : sm);
                }
                SRect(&img, x, y - h + 1, 1, 1, sl);
                SRect(&img, x - 1, y - h / 2, 1, 1, sl);
            };
            int topB = FLOOR_Y + (int)(f->spikeBand * DEPTH), botT = FLOOR_Y + (int)((ARENA_H - f->spikeBand * 0.7f) * DEPTH);
            for (int row = 0; row < 2; row++)
                for (int x = ARENA_SX + 4 + row * 4; x < ARENA_SX + (int)ARENA_W - 3; x += 8) spike(x, FLOOR_Y + 5 + row * 6, 6);
            for (int x = ARENA_SX + 4; x < ARENA_SX + (int)ARENA_W - 3; x += 8) spike(x, FLOOR_BOTTOM - 2, 6);
            int rx = ARENA_SX + (int)ARENA_W - (int)f->spikeBand;
            for (int y = topB + 6; y < botT; y += 7)
                for (int k = 0; k < 3; k++) {
                    spike(ARENA_SX + 4 + k * 8, y + (k % 2) * 3, 6);
                    spike(rx + 4 + k * 8, y + (k % 2) * 3, 6);
                }
        }
    }

    // Trainer spots and field markings.
    const Color blueSpot{150, 180, 225, 255}, redSpot{230, 150, 150, 255};
    if (venue != Venue::Stadium || lair) {
        HiRing(cv, 120, ARENA_H / 2, 22.75f, 1.0f, blueSpot, 0.5f);
        HiRing(cv, ARENA_W - 120, ARENA_H / 2, 22.75f, 1.0f, redSpot, 0.5f);
    } else if (kind == ArenaKind::Stadium) {
        const Color line{240, 232, 210, 255};
        ForRows(FY0, FY1, [&](int Y) {
            for (int X = FX0; X < FX1; X++) {
                float e = std::fabs(TexWX(X) - (ARENA_W / 2 - 0.5f));
                if (e < 1.6f) Blend(cv.px[Y * cv.w + X], line, Clamp01((1.3f - e) / 0.6f) * 0.92f);
            }
        });
        HiRing(cv, ARENA_W / 2, ARENA_H / 2, 70.75f, 1.1f, line, 0.92f);
        HiRing(cv, 120, ARENA_H / 2, 26.75f, 1.1f, blueSpot, 0.95f);
        HiRing(cv, ARENA_W - 120, ARENA_H / 2, 26.75f, 1.1f, redSpot, 0.95f);
    } else if (kind != ArenaKind::Water) {
        HiRing(cv, 120, ARENA_H / 2, 22.75f, 1.0f, blueSpot, 0.6f);
        HiRing(cv, ARENA_W - 120, ARENA_H / 2, 22.75f, 1.0f, redSpot, 0.6f);
    }

    // Baked light over the whole floor.
    ForRows(FY0, FY1, [&](int Y) {
        float wy = TexWY(Y);
        for (int X = FX0; X < FX1; X++) {
            Color& c = cv.px[Y * cv.w + X];
            c = Lit(c, FloorLight(outdoor, TexWX(X), wy));
        }
    });

    if (venue != Venue::Stadium || lair) return img;
    SRect(&img, ARENA_SX, FLOOR_Y, (int)ARENA_W, 1, th.trim);
    SRect(&img, ARENA_SX - 1, FLOOR_Y, 1, FLOOR_BOTTOM - FLOOR_Y, Shade(th.trim, -20));
    SRect(&img, ARENA_SX + (int)ARENA_W, FLOOR_Y, 1, FLOOR_BOTTOM - FLOOR_Y, Shade(th.trim, -20));
    return img;
}

Texture2D ToTex(Image img) {
    Texture2D t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return t;
}

void DrawTorches(float time, ArenaKind kind) {
    const int torchX[4] = {150, 260, 378, 488};
    Color flame = Color{240, 120, 40, 255}, core = Color{255, 210, 80, 255};
    if (kind == ArenaKind::Water) { flame = Color{80, 180, 255, 255}; core = Color{200, 240, 255, 255}; }
    else if (kind == ArenaKind::Electric) { flame = Color{240, 220, 60, 255}; core = WHITE; }
    else if (kind == ArenaKind::Psychic) { flame = Color{230, 90, 220, 255}; core = Color{255, 200, 250, 255}; }
    else if (kind == ArenaKind::Poison) { flame = Color{160, 80, 220, 255}; core = Color{220, 170, 255, 255}; }
    else if (kind == ArenaKind::Grass) { flame = Color{110, 210, 90, 255}; core = Color{220, 255, 200, 255}; }
    else if (kind == ArenaKind::Ground) { flame = Color{220, 40, 40, 255}; core = Color{255, 160, 160, 255}; }
    else if (kind == ArenaKind::Ice) { flame = Color{150, 220, 255, 255}; core = WHITE; }
    else if (kind == ArenaKind::Ghost) { flame = Color{140, 110, 255, 255}; core = Color{220, 210, 255, 255}; }
    else if (kind == ArenaKind::Dragon) { flame = Color{120, 90, 240, 255}; core = Color{255, 180, 90, 255}; }
    else if (kind == ArenaKind::Champion) { flame = Color{255, 200, 60, 255}; core = WHITE; }
    for (int i = 0; i < 4; i++) {
        int x = torchX[i] + (int)gShake.x, y = STANDS_H - 4 + (int)gShake.y;
        DrawRectangle(x, y + 6, 3, 12, Color{90, 60, 40, 255});
        DrawRectangle(x - 1, y + 5, 5, 2, Color{130, 120, 130, 255});
        int f = (int)(time * 10 + i * 3) % 3;
        DrawRectangle(x - 1, y - f, 5, 6 + f, flame);
        DrawRectangle(x, y + 1 - f / 2, 3, 4, core);
        DrawRectangle(x + 1, y + 3, 1, 2, Color{255, 250, 230, 255});
    }
}

} // namespace

void LoadArena() { gMenuTex = ToTex(BuildImage(ArenaKind::Stadium, nullptr)); }

void SetBattleArena(ArenaKind kind, const Field& f) {
    if (gBattleLoaded) UnloadTexture(gBattleTex);
    double t0 = GetTime();
    gBattleTex = ToTex(BuildImage(kind, &f));
    TraceLog(LOG_INFO, "ARENA: floor baked in %.0f ms", (GetTime() - t0) * 1000.0);
    gBattleLoaded = true;
    gBattleKind = kind;
    gBattleVenue = f.venue;
    gBattleSeason = f.season;
}

void UnloadArena() {
    UnloadTexture(gMenuTex);
    if (gBattleLoaded) UnloadTexture(gBattleTex);
    gBattleLoaded = false;
}

void DrawArena(float time) {
    DrawTexturePro(gMenuTex, {0, 0, (float)gMenuTex.width, (float)gMenuTex.height}, {gShake.x, gShake.y, (float)VW, (float)VH}, {0, 0}, 0, WHITE);
    DrawTorches(time, ArenaKind::Stadium);
}

Color GymColor(ArenaKind k) {
    switch (k) {
        case ArenaKind::Rock: return Color{176, 150, 100, 255};
        case ArenaKind::Water: return Color{70, 140, 230, 255};
        case ArenaKind::Electric: return Color{246, 206, 50, 255};
        case ArenaKind::Grass: return Color{110, 196, 80, 255};
        case ArenaKind::Poison: return Color{170, 90, 200, 255};
        case ArenaKind::Psychic: return Color{240, 100, 170, 255};
        case ArenaKind::Fire: return Color{240, 110, 50, 255};
        case ArenaKind::Ground: return Color{200, 60, 50, 255};
        case ArenaKind::Ice: return Color{140, 210, 250, 255};
        case ArenaKind::Fighting: return Color{220, 110, 60, 255};
        case ArenaKind::Ghost: return Color{150, 100, 220, 255};
        case ArenaKind::Dragon: return Color{110, 100, 240, 255};
        case ArenaKind::Champion: return Color{250, 205, 70, 255};
        case ArenaKind::Seafoam: return Color{150, 210, 255, 255};
        case ArenaKind::PowerPlant: return Color{250, 220, 60, 255};
        case ArenaKind::Ember: return Color{255, 130, 50, 255};
        case ArenaKind::Unknown: return Color{190, 120, 255, 255};
        case ArenaKind::Faraway: return Color{255, 150, 210, 255};
        default: return Color{230, 200, 90, 255};
    }
}

namespace {
float H01(int a, int b) { return Hash2(a * 31 + 7, b * 17 + 3); }

// A soft light cone: bright along the middle, fading to nothing at its edges and at its source.
void SoftCone(Vector2 src, Vector2 aim, float halfW, Color c, float alpha) {
    Vector2 d{aim.x - src.x, aim.y - src.y};
    float l = std::sqrt(d.x * d.x + d.y * d.y);
    if (l < 1) return;
    Vector2 n{-d.y / l * halfW, d.x / l * halfW};
    Vector2 lo{aim.x - n.x, aim.y - n.y}, li{aim.x - n.x * 0.45f, aim.y - n.y * 0.45f};
    Vector2 ri{aim.x + n.x * 0.45f, aim.y + n.y * 0.45f}, ro{aim.x + n.x, aim.y + n.y};
    unsigned char a0 = (unsigned char)(255 * alpha * 0.15f), a1 = (unsigned char)(255 * alpha);
    auto V = [&](Vector2 p, unsigned char a) { rlColor4ub(c.r, c.g, c.b, a); rlVertex2f(p.x, p.y); };
    rlBegin(RL_TRIANGLES);
    for (int side = 0; side < 2; side++) {   // both windings, so it shows whichever way it faces
        Vector2 tri[3][3] = {{src, lo, li}, {src, li, ri}, {src, ri, ro}};
        unsigned char al[3][3] = {{a0, 0, a1}, {a0, a1, a1}, {a0, a1, 0}};
        for (int k = 0; k < 3; k++) {
            if (side == 0) { V(tri[k][0], al[k][0]); V(tri[k][1], al[k][1]); V(tri[k][2], al[k][2]); }
            else { V(tri[k][0], al[k][0]); V(tri[k][2], al[k][2]); V(tri[k][1], al[k][1]); }
        }
    }
    rlEnd();
}

// Gym battles: the stands come alive (bouncing fans, waving team flags, camera flashes),
// banners hang on the wall and spotlights sweep the floor.
void DrawGymSpectacle(float time, ArenaKind kind, float hype) {
    Color team = GymColor(kind);
    float sx = gShake.x, sy = gShake.y;
    const Color shirts[] = {{220, 80, 80, 255}, {80, 140, 220, 255}, {240, 200, 80, 255}, {100, 190, 110, 255}, {240, 240, 240, 255}, team, team};
    for (int row = 0; row < STANDS_H / 6; row++) {   // fans: heads bob, some jump when the crowd roars
        for (int i = 0; i < 70; i++) {
            float x = i * 9.2f + H01(i, row) * 6;
            float jump = std::max(0.0f, std::sin(time * (6 + H01(i, row + 9) * 4) + i)) * (0.6f + hype * 2.4f);
            int y = row * 6 + 1 - (int)jump;
            Color sh = shirts[(int)(H01(i, row + 3) * 7) % 7];
            DrawRectangle((int)(x + sx), (int)(y + 3 + sy), 3, 3, sh);
            DrawRectangle((int)(x + 1 + sx), (int)(y + 1 + sy), 1, 2, Color{240, 200, 160, 255});
            if (hype > 0.3f && H01(i, row + 5) > 0.7f) DrawRectangle((int)(x + 3 + sx), (int)(y - 1 + sy), 1, 2, Color{240, 200, 160, 255});   // arms up
            if (H01(i, row + 7) > 0.86f) {   // a team flag on a stick
                float wv = std::sin(time * 7 + i) * 1.5f;
                DrawRectangle((int)(x + 3 + sx), (int)(y - 4 + sy), 1, 6, Color{90, 70, 50, 255});
                DrawTriangle({x + 4 + sx, y - 4 + sy}, {x + 4 + sx, y - 1 + sy}, {x + 9 + wv + sx, y - 2.5f + sy}, team);
            }
        }
    }
    for (int k = 0; k < 3 + (int)(hype * 10); k++) {   // camera flashes
        int seed = (int)(time * 6) * 13 + k * 101;
        if (H01(seed, 5) < 0.55f) continue;
        float fx = H01(seed, 1) * VW, fy = H01(seed, 2) * (STANDS_H - 4);
        DrawRectangle((int)(fx + sx), (int)(fy + sy), 2, 2, WHITE);
        DrawCircleV({fx + 1 + sx, fy + 1 + sy}, 3, Fade(WHITE, 0.35f));
    }
    for (int b = 0; b < 6; b++) {   // banners in the team colour on the wall
        float bx = 50 + b * 108.0f;
        int top = STANDS_H + 1;
        float wv = std::sin(time * 1.6f + b) * 1.5f;
        DrawRectangle((int)(bx - 7 + sx), (int)(top + sy), 14, 9, team);
        DrawTriangle({bx - 7 + sx, top + 9.0f + sy}, {bx + wv + sx, top + 13.0f + sy}, {bx + 7 + sx, top + 9.0f + sy}, team);
        DrawRectangle((int)(bx - 7 + sx), (int)(top + sy), 14, 2, ColorBrightness(team, 0.4f));
        DrawCircle((int)(bx + sx), (int)(top + 5 + sy), 2.5f, Color{255, 240, 200, 255});
    }
    BeginBlendMode(BLEND_ADDITIVE);   // spotlights sweeping the floor
    for (int k = 0; k < 2; k++) {
        float ang = std::sin(time * 0.45f + k * 2.4f) * 0.6f;
        Vector2 src{k ? VW - 70.0f : 70.0f, -10};
        Vector2 aim{VW / 2 + std::sin(time * 0.45f + k * 2.4f) * 230, FLOOR_Y + 150 + std::cos(time * 0.6f + k) * 60};
        (void)ang;
        Vector2 d{aim.x - src.x, aim.y - src.y};
        Vector2 nrm{-d.y, d.x};
        float l = std::sqrt(nrm.x * nrm.x + nrm.y * nrm.y);
        nrm = {nrm.x / l * 34, nrm.y / l * 34};
        SoftCone(src, aim, 40, Color{255, 250, 220, 255}, 0.07f + hype * 0.06f);
        for (int k2 = 0; k2 < 4; k2++)   // the pool of light on the floor, soft-edged
            DrawEllipse((int)aim.x, (int)aim.y, 40 - k2 * 8, (40 - k2 * 8) * DEPTH, Fade(Color{255, 250, 220, 255}, 0.025f + hype * 0.015f));
    }
    EndBlendMode();
    if (kind == ArenaKind::Champion) {   // fireworks over the stands, all battle long
        for (int k = 0; k < 4; k++) {
            float ph = std::fmod(time * 0.45f + k * 0.29f, 1.0f);
            int cyc = (int)(time * 0.45f + k * 0.29f);
            float fx = 40 + H01(cyc, k) * (VW - 80), fy = 4 + H01(cyc, k + 7) * (STANDS_H - 6);
            if (ph < 0.15f) continue;
            float u = (ph - 0.15f) / 0.85f;
            Color fc = k % 3 == 0 ? team : k % 3 == 1 ? Color{255, 120, 120, 255} : Color{140, 220, 255, 255};
            for (int r = 0; r < 14; r++) {
                float a = r * 6.2831853f / 14;
                DrawCircleV({fx + std::cos(a) * u * 22 + sx, fy + std::sin(a) * u * 14 + u * u * 6 + sy}, 1.2f, Fade(fc, 1 - u));
            }
        }
    }
}
} // namespace

// A wide stadium packed to the rafters, for the gym intro: tiers of fans, flags, flashes, lights.
void DrawGymStadium(float time, ArenaKind kind, float hype, int top, int bottom) {
    Color team = GymColor(kind);
    for (int y = top; y < bottom; y++) DrawRectangle(0, y, VW, 1, Mix(Color{14, 12, 30, 255}, Color{40, 30, 60, 255}, (y - top) / (float)(bottom - top)));
    int tiers = (bottom - top) / 9;
    const Color shirts[] = {{220, 80, 80, 255}, {80, 140, 220, 255}, {240, 200, 80, 255}, {100, 190, 110, 255}, {240, 240, 240, 255}, {200, 120, 200, 255}, team, team, team};
    for (int r = 0; r < tiers; r++) {
        int y = top + r * 9;
        float scale = 0.6f + 0.4f * r / std::max(1, tiers - 1);   // farther rows are smaller
        DrawRectangle(0, y + 7, VW, 2, Color{60, 50, 80, 255});
        int count = (int)(VW / (6.0f * scale));
        for (int i = 0; i < count; i++) {
            float x = i * 6.0f * scale + H01(i, r) * 3;
            float ph = time * (5 + H01(i, r + 11) * 5) + i * 0.7f + r;
            float jump = std::max(0.0f, std::sin(ph)) * (0.5f + hype * 2.4f) * scale;
            float hy = y - jump;
            Color sh = shirts[(int)(H01(i, r + 2) * 9) % 9];
            Color skin = H01(i, r + 4) > 0.5f ? Color{236, 196, 156, 255} : Color{176, 124, 90, 255};
            float bw = 3.4f * scale, bh = 4.2f * scale;
            float fade = 0.65f + 0.35f * scale;
            DrawRectangleRounded({x - bw / 2, hy + 2.4f * scale, bw, bh}, 0.5f, 4, Fade(sh, fade));          // body
            DrawCircleV({x, hy + 1.2f * scale}, 1.4f * scale, Fade(skin, fade));                                // head
            bool armsUp = jump > 0.6f * scale || H01(i, r + 6) > 0.8f;
            if (armsUp) {   // cheering
                DrawLineEx({x - bw / 2, hy + 3 * scale}, {x - bw / 2 - 1.2f * scale, hy - 0.8f * scale}, std::max(0.5f, 0.8f * scale), Fade(skin, fade));
                DrawLineEx({x + bw / 2, hy + 3 * scale}, {x + bw / 2 + 1.2f * scale, hy - 0.8f * scale}, std::max(0.5f, 0.8f * scale), Fade(skin, fade));
            }
            float pick = H01(i, r + 8);
            if (pick > 0.93f) {   // a sign held up high
                float wv = std::sin(time * 4 + i) * 0.8f;
                DrawRectangleRec({x - 4 * scale + wv, hy - 5 * scale, 8 * scale, 4.5f * scale}, team);
                DrawRectangleRec({x - 3 * scale + wv, hy - 3.6f * scale, 6 * scale, 1.2f * scale}, WHITE);
            } else if (pick > 0.86f) {   // a flag
                float wv = std::sin(time * 6 + i) * 2;
                DrawLineEx({x + bw / 2, hy + 3 * scale}, {x + bw / 2, hy - 5 * scale}, 0.6f, Color{100, 80, 60, 255});
                DrawTriangle({x + bw / 2, hy - 5 * scale}, {x + bw / 2, hy - 1.5f * scale}, {x + bw / 2 + 7 * scale + wv, hy - 3.2f * scale}, team);
            }
        }
    }
    for (int k = 0; k < 10 + (int)(hype * 20); k++) {   // camera flashes
        int seed = (int)(time * 8) * 7 + k * 131;
        if (H01(seed, 9) < 0.5f) continue;
        Vector2 p{H01(seed, 1) * VW, top + H01(seed, 2) * (bottom - top)};
        DrawCircleV(p, 3.5f, Fade(WHITE, 0.3f));
        DrawRectangle((int)p.x - 1, (int)p.y - 1, 2, 2, WHITE);
    }
    BeginBlendMode(BLEND_ADDITIVE);
    for (int k = 0; k < 4; k++) {   // sweeping spotlights
        float a = std::sin(time * 0.7f + k * 1.7f) * 0.5f;
        Vector2 src{80.0f + k * 160, (float)bottom};
        Vector2 tip1{src.x + std::sin(a - 0.08f) * 400, src.y - std::cos(a - 0.08f) * 400};
        Vector2 tip2{src.x + std::sin(a + 0.08f) * 400, src.y - std::cos(a + 0.08f) * 400};
        Color c = Fade(k % 2 ? team : Color{255, 250, 220, 255}, 0.10f);
        DrawTriangle(src, tip2, tip1, c);
        DrawTriangle(src, tip1, tip2, c);
    }
    EndBlendMode();
}

namespace {
float HF(int i, int k) { return Hash2(i * 131 + 7, k * 71 + 13); }

// The scenery comes alive: clouds drift, water glints, embers rise, leaves and petals blow across
// the field, lab lights blink. Purely for looks.
void DrawAmbience(float time, Venue v, int season, ArenaKind lair) {
    float sx = gShake.x, sy = gShake.y;
    bool outdoor = v == Venue::Route || v == Venue::Park || v == Venue::Forest || v == Venue::Mountain || v == Venue::Beach ||
                   v == Venue::Bridge || v == Venue::Ship || v == Venue::City;
    if (lair == ArenaKind::Faraway) { outdoor = true; v = Venue::Forest; }
    if (lair == ArenaKind::Ember) v = Venue::Volcano;
    if (lair == ArenaKind::Seafoam) v = Venue::IceCave;
    if (lair == ArenaKind::Unknown) v = Venue::Cave;
    if (lair == ArenaKind::PowerPlant) v = Venue::Hideout;
    if (outdoor && v != Venue::Forest) {   // clouds drifting across the sky
        for (int i = 0; i < 6; i++) {
            float x = std::fmod(HF(i, 1) * (VW + 160) + time * (4 + HF(i, 2) * 5), (float)(VW + 160)) - 80 + sx;
            float y = 4 + HF(i, 3) * (FLOOR_Y - 18) + sy;
            float w = 22 + HF(i, 4) * 26;
            Color cc = season == 2 ? Color{255, 236, 220, 255} : Color{255, 255, 255, 255};
            DrawEllipse((int)x, (int)y, w, w * 0.22f, Fade(cc, 0.45f));
            DrawEllipse((int)(x - w * 0.3f), (int)(y - 2), w * 0.45f, w * 0.18f, Fade(cc, 0.5f));
            DrawEllipse((int)(x + w * 0.25f), (int)(y - 1), w * 0.5f, w * 0.2f, Fade(cc, 0.45f));
        }
    }
    if (v == Venue::Beach || v == Venue::Bridge || v == Venue::Ship) {   // the sea glinting
        for (int i = 0; i < 26; i++) {
            float x = std::fmod(HF(i, 5) * VW + time * (6 + HF(i, 6) * 6), (float)VW) + sx;
            float y = FLOOR_Y - 2 - HF(i, 7) * 8 + sy;
            float a = 0.5f + 0.5f * std::sin(time * 3 + i * 1.7f);
            DrawRectangleV({x, y}, {2.5f, 0.7f}, Fade(WHITE, 0.7f * a));
        }
    }
    if (v == Venue::Volcano) {   // embers rising, a heat glow
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 40; i++) {
            float ph = std::fmod(time * (0.25f + HF(i, 8) * 0.3f) + HF(i, 9), 1.0f);
            float x = HF(i, 10) * VW + std::sin(time * 1.5f + i) * 8 + sx;
            float y = FLOOR_BOTTOM - ph * (FLOOR_BOTTOM + 10) + sy;
            DrawCircleV({x, y}, 0.8f + HF(i, 11), Fade(Color{255, 150, 60, 255}, 0.8f * std::sin(ph * 3.14159f)));
        }
        DrawRectangleGradientV(0, FLOOR_Y - 10, VW, 30, Fade(Color{255, 90, 30, 255}, 0.12f + 0.04f * std::sin(time * 2)), Fade(Color{255, 90, 30, 255}, 0));
        EndBlendMode();
    }
    if (v == Venue::Forest || ((v == Venue::Route || v == Venue::Park) && (season == 0 || season == 2))) {   // leaves or petals blowing across
        int n = v == Venue::Forest ? 16 : 10;
        for (int i = 0; i < n; i++) {
            float ph = std::fmod(time * (0.05f + HF(i, 12) * 0.04f) + HF(i, 13), 1.0f);
            float x = -20 + ph * (VW + 40) + sx;
            float y = FLOOR_Y + HF(i, 14) * (FLOOR_BOTTOM - FLOOR_Y - 20) + std::sin(time * 2 + i) * 10 + sy;
            float r = time * 3 + i;
            Color lc = season == 0 ? Color{250, 190, 215, 255} : season == 2 ? (i % 2 ? Color{220, 110, 40, 255} : Color{240, 170, 60, 255}) : Color{110, 170, 70, 255};
            Vector2 d{std::cos(r) * 2.2f, std::sin(r) * 1.0f};
            DrawTriangle({x - d.x, y - d.y}, {x + d.y, y - d.x * 0.4f}, {x + d.x, y + d.y}, lc);
            DrawTriangle({x - d.x, y - d.y}, {x + d.x, y + d.y}, {x + d.y, y - d.x * 0.4f}, lc);
        }
    }
    if ((v == Venue::Route || v == Venue::Park) && season == 1) {   // butterflies in summer
        for (int i = 0; i < 4; i++) {
            float x = std::fmod(HF(i, 15) * VW + time * 12 + std::sin(time * 0.7f + i) * 40, (float)VW) + sx;
            float y = FLOOR_Y + 20 + HF(i, 16) * 200 + std::sin(time * 1.3f + i * 2) * 14 + sy;
            float flap = std::fabs(std::sin(time * 14 + i));
            Color bc = i % 2 ? Color{255, 240, 120, 255} : WHITE;
            DrawEllipse((int)(x - 1.5f), (int)y, 1.6f * flap + 0.3f, 1.4f, bc);
            DrawEllipse((int)(x + 1.5f), (int)y, 1.6f * flap + 0.3f, 1.4f, bc);
        }
    }
    if (season == 3 && outdoor) {   // a little snow drifting even on a clear day
        for (int i = 0; i < 40; i++) {
            float ph = std::fmod(time * (0.04f + HF(i, 17) * 0.03f) + HF(i, 18), 1.0f);
            DrawCircleV({HF(i, 19) * VW + std::sin(time + i) * 6 + sx, ph * FLOOR_BOTTOM + sy}, 0.8f, Fade(WHITE, 0.8f));
        }
    }
    if (v == Venue::Cave || v == Venue::IceCave) {   // drips and glinting crystals
        for (int i = 0; i < 8; i++) {
            float ph = std::fmod(time * 0.5f + HF(i, 20), 1.0f);
            float x = HF(i, 21) * VW + sx;
            DrawRectangleV({x, FLOOR_Y - 8 + ph * 18 + sy}, {0.7f, 1.6f}, Fade(Color{180, 220, 255, 255}, 0.7f * (1 - ph)));
        }
        BeginBlendMode(BLEND_ADDITIVE);
        for (int i = 0; i < 10; i++) {
            float tw = std::pow(std::max(0.0f, std::sin(time * (1 + HF(i, 22)) + i * 2.1f)), 8.0f);
            DrawCircleV({HF(i, 23) * VW + sx, 4 + HF(i, 24) * (FLOOR_Y - 8) + sy}, 2.5f, Fade(lair == ArenaKind::Unknown ? Color{210, 150, 255, 255} : Color{150, 220, 255, 255}, 0.7f * tw));
        }
        EndBlendMode();
    }
    if (v == Venue::Lab || v == Venue::Office || v == Venue::Hideout) {   // blinking lights
        for (int i = 0; i < 12; i++) {
            bool on = std::sin(time * (2 + HF(i, 25) * 4) + i * 3) > 0.2f;
            if (!on) continue;
            Color lc = i % 3 == 0 ? Color{120, 255, 140, 255} : i % 3 == 1 ? Color{255, 90, 80, 255} : Color{120, 190, 255, 255};
            DrawRectangle((int)(HF(i, 26) * VW + sx), (int)(6 + HF(i, 27) * (FLOOR_Y - 14) + sy), 2, 1, lc);
        }
    }
    if (v == Venue::Tower || v == Venue::Mansion) {   // candle flicker
        BeginBlendMode(BLEND_ADDITIVE);
        for (int x = 40; x < VW; x += 90) {
            float f = 0.6f + 0.4f * std::sin(time * 9 + x) * std::sin(time * 5.3f + x * 0.3f);
            DrawCircleV({(float)x + 1.5f + sx, FLOOR_Y - 14 + sy}, 7 * f, Fade(Color{200, 160, 255, 255}, 0.18f * f));
        }
        EndBlendMode();
    }
}
} // namespace

void DrawBattleArena(float time, float hype) {
    if (!gBattleLoaded) { DrawArena(time); return; }
    DrawTexturePro(gBattleTex, {0, 0, (float)gBattleTex.width, (float)gBattleTex.height}, {gShake.x, gShake.y, (float)VW, (float)VH}, {0, 0}, 0, WHITE);
    DrawAmbience(time, gBattleKind == ArenaKind::Stadium ? gBattleVenue : Venue::Stadium, gBattleSeason, IsLegendArena(gBattleKind) ? gBattleKind : ArenaKind::Stadium);
    if (IsLegendArena(gBattleKind)) return;   // a legendary's lair: no crowd, no torches
    if (gBattleVenue == Venue::Stadium) DrawTorches(time, gBattleKind);
    if (gBattleKind != ArenaKind::Stadium) DrawGymSpectacle(time, gBattleKind, hype);
}
