#include "arena.h"
#include <cmath>
#include "field.h"

namespace {

Texture2D gMenuTex{}, gBattleTex{};
bool gBattleLoaded = false;
ArenaKind gBattleKind = ArenaKind::Stadium;
Venue gBattleVenue = Venue::Stadium;
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

void FloorPixel(Image* img, float wx, float wy, Color c) {
    int sx = ARENA_SX + (int)wx;
    int sy = FLOOR_Y + (int)(wy * DEPTH);
    if (sx >= ARENA_SX && sx < ARENA_SX + (int)ARENA_W && sy >= FLOOR_Y && sy < FLOOR_BOTTOM)
        ImageDrawPixel(img, sx, sy, c);
}

void FloorRing(Image* img, float cx, float cy, float r, Color c) {
    for (int i = 0; i < 720; i++) {
        float a = i * 3.14159265f / 360.0f;
        FloorPixel(img, cx + std::cos(a) * r, cy + std::sin(a) * r, c);
        FloorPixel(img, cx + std::cos(a) * (r + 1.5f), cy + std::sin(a) * (r + 1.5f), c);
    }
}

// Fill a floor ellipse (world coords) pixel by pixel; 'rim' is drawn on the outer edge.
template <typename F>
void FloorEllipse(Image* img, const Zone& z, F colorAt, Color rim, float rimW = 2.0f) {
    int sx0 = ARENA_SX + (int)(z.pos.x - z.rx * 1.25f) - 1, sx1 = ARENA_SX + (int)(z.pos.x + z.rx * 1.25f) + 1;
    int sy0 = FLOOR_Y + (int)((z.pos.y - z.ry * 1.25f) * DEPTH) - 1, sy1 = FLOOR_Y + (int)((z.pos.y + z.ry * 1.25f) * DEPTH) + 1;
    for (int sy = sy0; sy <= sy1; sy++) {
        if (sy < FLOOR_Y || sy >= FLOOR_BOTTOM) continue;
        for (int sx = sx0; sx <= sx1; sx++) {
            if (sx < ARENA_SX || sx >= ARENA_SX + (int)ARENA_W) continue;
            float wx = (float)(sx - ARENA_SX) + 0.5f, wy = (sy - FLOOR_Y + 0.5f) / DEPTH;
            float dx = (wx - z.pos.x) / z.rx, dy = (wy - z.pos.y) / z.ry;
            float wob = Wobble(z.seed, std::atan2(dy, dx));
            float d = std::sqrt(dx * dx + dy * dy) / wob;
            if (d > 1.0f) continue;
            float edge = (1.0f - d) * std::min(z.rx, z.ry * DEPTH) * wob;
            ImageDrawPixel(img, sx, sy, edge < rimW ? rim : colorAt(sx, sy, wx, wy, edge));
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

Color FloorBase(ArenaKind k, int sx, int sy) {
    float wx = (float)(sx - ARENA_SX), wy = (sy - FLOOR_Y) / DEPTH;
    int tx = (int)(wx / 40), ty = (int)(wy / 40);
    bool lineX = std::fmod(wx, 40.0f) < 1.0f, lineY = std::fmod(wy, 40.0f) < 1.0f / DEPTH;
    int n = R(100);
    switch (k) {
        case ArenaKind::Rock: {
            Color c = ((tx * 7 + ty * 13) % 3 == 0) ? Color{150, 138, 120, 255} : Color{140, 128, 112, 255};
            if (lineX || lineY) c = Color{100, 90, 78, 255};
            if (n < 6) c = Shade(c, -18);
            return c;
        }
        case ArenaKind::Water: {
            Color c = Color{48, 104, 176, 255};
            if (n < 4) c = Color{90, 150, 220, 255};
            else if (n < 10) c = Color{40, 92, 160, 255};
            return c;
        }
        case ArenaKind::Electric: {
            Color c = ((tx + ty) % 2) ? Color{86, 90, 98, 255} : Color{78, 82, 90, 255};
            if (lineX || lineY) c = Color{50, 52, 58, 255};
            float lx = std::fmod(wx, 40.0f), ly = std::fmod(wy, 40.0f);
            if ((lx > 3 && lx < 5 && ly > 3 && ly < 7) || (lx > 35 && lx < 37 && ly > 33 && ly < 37)) c = Color{130, 134, 140, 255};
            if (n < 3) c = Shade(c, 10);
            if (wx < 10 || wx > ARENA_W - 10 || wy < 12 || wy > ARENA_H - 12) c = (((int)(wx + wy) / 6) % 2) ? Color{240, 200, 40, 255} : Color{30, 30, 30, 255};
            return c;
        }
        case ArenaKind::Grass: {
            Color c = ((tx + ty) % 2) ? Color{168, 196, 160, 255} : Color{158, 186, 150, 255};
            if (lineX || lineY) c = Color{130, 160, 128, 255};
            if (n < 4) c = Shade(c, -14);
            return c;
        }
        case ArenaKind::Poison: {
            Color c = Color{88, 92, 58, 255};
            float v = std::sin(wx * 0.07f) * std::cos(wy * 0.05f);
            if (v > 0.5f) c = Color{78, 84, 52, 255};
            if (n < 8) c = Shade(c, -12);
            else if (n < 11) c = Color{110, 112, 70, 255};
            return c;
        }
        case ArenaKind::Psychic: {
            Color c = ((tx + ty) % 2) ? Color{44, 30, 70, 255} : Color{36, 24, 60, 255};
            if (lineX || lineY) c = Color{220, 90, 210, 255};
            if (n < 2) c = Shade(c, 20);
            return c;
        }
        case ArenaKind::Fire: {
            Color c = Color{58, 46, 44, 255};
            float v = std::sin(wx * 0.11f + wy * 0.03f) * std::sin(wy * 0.09f - wx * 0.02f);
            if (v > 0.975f) c = Color{200, 90, 40, 255};
            else if (v > 0.9f) c = Color{110, 54, 38, 255};
            if (n < 6) c = Shade(c, -10);
            return c;
        }
        case ArenaKind::Ground: {
            Color c = ((tx + ty) % 2) ? Color{70, 66, 68, 255} : Color{64, 60, 62, 255};
            if (lineX || lineY) c = Color{44, 40, 42, 255};
            float r = std::sqrt((wx - ARENA_W / 2) * (wx - ARENA_W / 2) + (wy - ARENA_H / 2) * (wy - ARENA_H / 2));
            if (r < 62 && r > 56) c = Color{110, 30, 34, 255};
            else if (r <= 56) c = Shade(c, -8);
            if (n < 4) c = Shade(c, -10);
            return c;
        }
        case ArenaKind::Ice: {   // frosted ice tiles
            Color c = ((tx + ty) % 2) ? Color{196, 222, 240, 255} : Color{186, 214, 236, 255};
            c = Shade(c, (int)((Fbm(wx * 0.04f, wy * 0.06f) - 0.5f) * 30));
            if (lineX || lineY) c = Color{230, 244, 255, 255};
            if (std::fabs(Noise(wx * 0.05f, wy * 0.08f) - 0.5f) < 0.01f) c = Color{150, 186, 214, 255};   // cracks in the frost
            if (n < 3) c = WHITE;
            return c;
        }
        case ArenaKind::Fighting: {   // the ring's canvas, a dark apron, and the centre circle
            float edge = std::min(std::min(wx, ARENA_W - wx), std::min(wy, ARENA_H - wy) * 1.2f);
            if (edge < 10) return Shade(Color{40, 60, 120, 255}, n < 10 ? -10 : 0);
            Color c = Color{226, 222, 208, 255};
            c = Shade(c, (int)((Fbm(wx * 0.05f, wy * 0.05f) - 0.5f) * 18) - (n < 6 ? 10 : 0));
            if (edge < 16) c = Color{60, 90, 170, 255};
            float d = std::sqrt((wx - ARENA_W / 2) * (wx - ARENA_W / 2) + (wy - ARENA_H / 2) * (wy - ARENA_H / 2));
            if (d < 72 && d > 68) c = Color{200, 50, 40, 255};
            else if (d < 68) c = Mix(c, Color{240, 200, 190, 255}, 0.3f);
            if (std::fabs(wx - ARENA_W / 2) < 1.2f && d >= 72) c = Shade(c, -25);
            return c;
        }
        case ArenaKind::Ghost: {   // cracked dark flagstones in a cold mist
            int col = (int)(wx / 30), row = (int)(wy / 26);
            float lx = std::fmod(wx + (row % 2) * 15, 30.0f), ly = std::fmod(wy, 26.0f);
            Color c = Shade(Color{58, 50, 72, 255}, (int)((Hash2(col, row) - 0.5f) * 20));
            if (lx < 1.0f || ly < 1.0f / DEPTH) c = Color{30, 24, 40, 255};
            float mist = Fbm(wx * 0.02f + 3, wy * 0.03f);
            if (mist > 0.56f) c = Mix(c, Color{120, 100, 160, 255}, (mist - 0.56f) * 1.8f);
            if (std::fabs(Noise(wx * 0.06f, wy * 0.08f) - 0.5f) < 0.008f) c = Color{20, 16, 28, 255};
            return c;
        }
        case ArenaKind::Dragon: {   // dark stone with a gold dragon seal in the middle
            Color c = ((tx + ty) % 2) ? Color{54, 48, 74, 255} : Color{48, 42, 66, 255};
            if (lineX || lineY) c = Color{32, 28, 46, 255};
            if (n < 4) c = Shade(c, 14);
            float dx = wx - ARENA_W / 2, dy = wy - ARENA_H / 2, d = std::sqrt(dx * dx + dy * dy);
            if (d < 84 && d > 80) c = Color{210, 168, 70, 255};
            else if (d < 74 && d > 72) c = Color{170, 130, 50, 255};
            float a = std::atan2(dy, dx);
            float spiral = std::fmod(a * 3 / 6.2831853f + d / 40.0f + 10, 1.0f);
            if (d < 70 && d > 14 && spiral < 0.08f) c = Color{200, 160, 66, 255};   // a coiled dragon
            if (d < 12) c = Color{180, 40, 50, 255};
            if (std::fabs(wy - ARENA_H / 2) < 6 && d > 84) c = Mix(c, Color{130, 30, 40, 255}, 0.6f);   // a red runner
            return c;
        }
        case ArenaKind::Champion: {   // polished marble, gold inlay, a giant Poke Ball and a red carpet
            Color c = ((tx + ty) % 2) ? Color{236, 230, 216, 255} : Color{220, 212, 196, 255};
            if (std::fabs(Noise(wx * 0.07f, wy * 0.07f) - 0.5f) < 0.012f) c = Shade(c, -26);   // veins in the marble
            if (lineX || lineY) c = Color{214, 170, 70, 255};
            float edge = std::min(std::min(wx, ARENA_W - wx), std::min(wy, ARENA_H - wy) * 1.2f);
            if (edge < 8) c = Color{200, 150, 50, 255};
            else if (edge < 12) c = Color{120, 30, 40, 255};
            if (std::fabs(wy - ARENA_H / 2) < 16 && (wx < ARENA_W / 2 - 90 || wx > ARENA_W / 2 + 90)) {
                c = std::fabs(wy - ARENA_H / 2) > 13 ? Color{230, 190, 80, 255} : Shade(Color{170, 30, 40, 255}, (int)((Noise(wx * 0.3f, wy * 0.3f) - 0.5f) * 16));
            }
            float dx = wx - ARENA_W / 2, dy = wy - ARENA_H / 2, d = std::sqrt(dx * dx + dy * dy);
            if (d < 86) {
                if (d > 82) c = Color{230, 190, 80, 255};
                else if (d > 78) c = Color{40, 36, 50, 255};
                else if (std::fabs(dy) < 5) c = Color{40, 36, 50, 255};
                else c = dy < 0 ? Shade(Color{210, 50, 50, 255}, n < 8 ? 12 : 0) : Shade(Color{244, 242, 236, 255}, n < 8 ? -8 : 0);
                if (d < 20) c = d > 15 ? Color{40, 36, 50, 255} : (d > 10 ? Color{250, 250, 252, 255} : Color{230, 190, 80, 255});
            }
            return c;
        }
        default: {
            const Color tileA{206, 182, 132, 255}, tileB{196, 170, 120, 255};
            Color c = ((tx + ty) % 2) ? tileA : tileB;
            if (n < 3) c = Shade(c, -22);
            else if (n < 5) c = Shade(c, 16);
            return c;
        }
    }
}


// ---------------------------------------------------------------- Venues (non-gym battles)

Color Grass(int season, float wx, float wy, int n) {
    Color c = season == 0 ? Color{104, 176, 84, 255} : season == 2 ? Color{150, 146, 78, 255} : season == 3 ? Color{220, 228, 238, 255} : Color{86, 156, 66, 255};
    c = Shade(c, (int)((Fbm(wx * 0.025f, wy * 0.04f) - 0.5f) * 40) + ((((int)(wx / 40)) % 2) ? 5 : 0));
    if (season == 3) {   // snow with grass poking through
        if (n < 7) c = Color{120, 150, 112, 255};
        else if (n < 10) c = Color{196, 210, 230, 255};
        return c;
    }
    if (n < 3) c = Shade(c, -18);
    else if (n < 5) c = Shade(c, 14);
    if (season == 0 && n == 99) c = (((int)wx + (int)wy) % 3 == 0) ? Color{250, 170, 200, 255} : (((int)wx % 2) ? Color{255, 240, 120, 255} : WHITE);
    if (season == 2 && n >= 92) c = n >= 97 ? Color{210, 90, 40, 255} : n >= 94 ? Color{230, 160, 50, 255} : Color{150, 90, 50, 255};
    return c;
}

Color Dirt(float wx, float wy, int n, int season) {
    Color c = Shade(Color{176, 146, 104, 255}, (int)((Fbm(wx * 0.05f, wy * 0.07f) - 0.5f) * 36));
    if (n < 5) c = Color{140, 132, 120, 255};
    else if (n < 8) c = Shade(c, 20);
    if (season == 3 && Noise(wx * 0.06f, wy * 0.09f) > 0.45f) c = Mix(c, Color{230, 236, 246, 255}, 0.75f);
    return c;
}

Color Tiles(float wx, float wy, float tw, float th, Color a, Color b, Color grout, bool offset, int vary) {
    int row = (int)(wy / th);
    float ox = offset && (row % 2) ? tw / 2 : 0;
    int col = (int)((wx + ox) / tw);
    float lx = std::fmod(wx + ox, tw), ly = std::fmod(wy, th);
    if (lx < 1.0f || ly < 1.0f / DEPTH) return grout;
    Color c = ((col + row) % 2) ? a : b;
    return Shade(c, (int)((Hash2(col, row) - 0.5f) * vary));
}

Color VenueFloor(Venue v, int season, float wx, float wy) {
    int n = R(100);
    switch (v) {
        case Venue::Route: {
            float cy = ARENA_H * 0.5f + std::sin(wx * 0.012f + 1.3f) * 55, d = std::fabs(wy - cy);
            if (d < 18) return Dirt(wx, wy, n, season);
            if (d < 23) return Mix(Dirt(wx, wy, n, season), Grass(season, wx, wy, n), (d - 18) / 5);
            return Grass(season, wx, wy, n);
        }
        case Venue::Forest: {
            Color c = Grass(season, wx, wy, n);
            c = Shade(c, season == 3 ? -10 : -34);
            float lt = Noise(wx * 0.04f, wy * 0.07f);
            if (lt > 0.68f) c = Shade(c, 26);   // sunlight through the leaves
            if (std::fabs(Noise(wx * 0.02f + 40, wy * 0.03f) - 0.5f) < 0.005f && Noise(wx * 0.05f, wy * 0.05f + 9) > 0.55f) c = Color{90, 64, 40, 255};   // roots
            if (season != 3 && n >= 88) c = n >= 95 ? Color{120, 90, 50, 255} : Color{70, 110, 50, 255};   // leaf litter, moss
            return c;
        }
        case Venue::Park: {
            float px = std::fabs(wx - ARENA_W / 2), py = std::fabs(wy - ARENA_H / 2);
            if (px < 15 || py < 13) {
                Color c = Tiles(wx, wy, 14, 12, Color{196, 190, 176, 255}, Color{186, 180, 166, 255}, Color{150, 144, 132, 255}, true, 18);
                return season == 3 && n < 40 ? Mix(c, WHITE, 0.6f) : c;
            }
            const float bx[2] = {110, ARENA_W - 110}, by[2] = {95, ARENA_H - 95};
            for (float x : bx)
                for (float y : by) {
                    float dx = wx - x, dy = wy - y, d = std::sqrt(dx * dx + dy * dy);
                    if (d < 28) {
                        if (d > 25) return Color{150, 140, 128, 255};   // stone edging
                        if (season == 3) return n < 70 ? Color{232, 238, 246, 255} : Color{110, 80, 60, 255};
                        if (n < 22) return n % 3 == 0 ? Color{240, 90, 110, 255} : n % 3 == 1 ? Color{255, 220, 90, 255} : Color{200, 140, 230, 255};
                        return n < 50 ? Color{70, 130, 60, 255} : Color{110, 80, 60, 255};
                    }
                }
            return Grass(season, wx, wy, n);
        }
        case Venue::Mountain: {
            Color c = Shade(Color{150, 132, 104, 255}, (int)((Fbm(wx * 0.03f, wy * 0.05f) - 0.5f) * 50));
            float slab = Noise(wx * 0.025f + 9, wy * 0.04f);
            if (slab > 0.66f) c = Shade(Color{140, 136, 128, 255}, (int)((slab - 0.66f) * 80) + (std::fabs(slab - 0.66f) < 0.01f ? -40 : 0));
            if (Noise(wx * 0.03f + 77, wy * 0.05f) > (season == 3 ? 0.42f : 0.66f)) c = season == 3 ? Color{232, 238, 246, 255} : Grass(season, wx, wy, n);
            if (n < 6) c = Shade(c, -25);
            else if (n < 9) c = Shade(c, 22);
            return c;
        }
        case Venue::Beach: {
            Color c = season == 3 ? Color{214, 204, 176, 255} : Color{228, 208, 154, 255};
            c = Shade(c, (int)((Fbm(wx * 0.02f, wy * 0.03f) - 0.5f) * 24));
            if (std::sin(wy * 0.4f + Noise(wx * 0.03f, wy * 0.03f) * 7) > 0.75f) c = Shade(c, 12);   // wind ripples
            float shore = 34 + Noise(wx * 0.02f, 3) * 16;
            if (wy < shore) c = Mix(c, Color{150, 140, 110, 255}, 0.5f);                            // wet sand
            if (std::fabs(wy - shore) < 1.5f / DEPTH) c = Color{240, 246, 250, 255};               // foam line
            if (wy < shore - 14) c = Mix(Color{70, 150, 200, 255}, Color{110, 190, 220, 255}, Noise(wx * 0.05f, wy * 0.2f));   // the sea's edge
            if (wy > shore && n == 0) c = Color{246, 200, 210, 255};   // shells
            if (wy > shore && n == 1) c = Color{150, 146, 140, 255};   // pebbles
            return c;
        }
        case Venue::City: {
            Color c = Tiles(wx, wy, 22, 16, Color{150, 150, 156, 255}, Color{142, 142, 148, 255}, Color{108, 108, 114, 255}, true, 22);
            float ix = std::fmod(wx - 30, ARENA_W - 60), iy = wy - 28;
            bool court = wx > 30 && wx < ARENA_W - 30 && wy > 28 && wy < ARENA_H - 28 &&
                         (wx < 32 || wx > ARENA_W - 32 || wy < 30 / DEPTH * 0 + 30 || wy > ARENA_H - 30 || std::fabs(wx - ARENA_W / 2) < 1);
            (void)ix; (void)iy;
            if (court) c = Color{220, 220, 210, 255};
            float mx = wx - ARENA_W * 0.72f, my = wy - ARENA_H * 0.28f, md = std::sqrt(mx * mx + my * my);
            if (md < 12) c = md > 10.5f ? Color{70, 70, 76, 255} : (((int)(wx / 3) % 2) ? Color{96, 96, 104, 255} : Color{84, 84, 92, 255});   // manhole
            if (n == 0) c = Color{90, 90, 96, 255};
            return c;
        }
        case Venue::Bridge: {
            if (wy < 20 || wy > ARENA_H - 20) {   // water under the bridge
                Color c = Mix(Color{60, 120, 180, 255}, Color{90, 160, 210, 255}, Noise(wx * 0.06f, wy * 0.3f));
                return n < 3 ? Color{200, 230, 250, 255} : c;
            }
            if (wy < 26 || wy > ARENA_H - 26) return Color{100, 70, 44, 255};   // edge beams
            int row = (int)(wy / 10);
            if (std::fmod(wy, 10.0f) < 1.0f / DEPTH) return Color{70, 48, 30, 255};
            Color c = Shade(Color{164, 118, 76, 255}, (int)((Hash2(row, 7) - 0.5f) * 40));
            c = Shade(c, (int)((Noise(wx * 0.15f, row * 3.0f) - 0.5f) * 20));
            float nx = std::fmod(wx + row * 37, 80.0f);
            if (nx < 2 && std::fmod(wy, 10.0f) > 4 && std::fmod(wy, 10.0f) < 6) c = Color{70, 70, 76, 255};   // nails
            if (season == 3 && Noise(wx * 0.05f, wy * 0.08f) > 0.6f) c = Mix(c, WHITE, 0.6f);
            return c;
        }
        case Venue::Ship: {
            int row = (int)(wy / 8);
            if (std::fmod(wy, 8.0f) < 1.0f / DEPTH) return Color{60, 44, 32, 255};
            float joint = std::fmod(wx + Hash2(row, 3) * 200, 160.0f);
            if (joint < 1.0f) return Color{60, 44, 32, 255};
            Color c = Shade(Color{184, 136, 90, 255}, (int)((Hash2(row, (int)((wx + Hash2(row, 3) * 200) / 160)) - 0.5f) * 30));
            c = Shade(c, (int)((Noise(wx * 0.2f, row * 2.0f) - 0.5f) * 16));
            if (joint > 2 && joint < 4 && std::fmod(wy, 8.0f) > 3 && std::fmod(wy, 8.0f) < 5) c = Color{120, 120, 126, 255};
            return c;
        }
        case Venue::Dojo: {
            // tatami: 80x40 mats, every other block turned
            int bx = (int)(wx / 80), by = (int)(wy / 80);
            bool turn = (bx + by) % 2;
            float lx = turn ? std::fmod(wy, 80.0f) : std::fmod(wx, 80.0f), ly = turn ? std::fmod(wx, 80.0f) : std::fmod(wy, 80.0f);
            if (std::fmod(ly, 40.0f) < 2.5f || lx < 2.5f) return Color{48, 64, 44, 255};   // cloth edging
            Color c = Color{196, 190, 132, 255};
            if (((int)(turn ? wx : wy * DEPTH)) % 2) c = Shade(c, -8);   // straw weave
            return Shade(c, (int)((Hash2(bx, by) - 0.5f) * 16));
        }
        case Venue::Office: {
            Color c = Tiles(wx, wy, 40, 40, Color{122, 132, 152, 255}, Color{114, 124, 144, 255}, Color{100, 108, 126, 255}, false, 8);
            if (std::fabs(wy - ARENA_H / 2) < 46) {   // a long carpet runner with a border
                c = std::fabs(wy - ARENA_H / 2) > 40 ? Color{200, 170, 90, 255} : Shade(Color{150, 60, 70, 255}, (int)((Noise(wx * 0.2f, wy * 0.2f) - 0.5f) * 14));
            }
            float dx = wx - ARENA_W / 2, dy = wy - ARENA_H / 2, d = std::sqrt(dx * dx + dy * dy);
            if (d < 30 && d > 26) c = Color{200, 170, 90, 255};   // company crest
            if (d < 22 && std::fabs(dx + dy * 0.3f) < 4) c = Color{200, 170, 90, 255};
            return c;
        }
        case Venue::Tower: {
            Color c = Tiles(wx, wy, 32, 30, Color{92, 82, 104, 255}, Color{84, 76, 96, 255}, Color{56, 50, 66, 255}, true, 20);
            float mist = Fbm(wx * 0.02f + 5, wy * 0.03f);
            if (mist > 0.58f) c = Mix(c, Color{170, 150, 200, 255}, (mist - 0.58f) * 1.6f);
            if (std::fabs(Noise(wx * 0.05f, wy * 0.07f) - 0.5f) < 0.01f) c = Color{40, 34, 48, 255};
            return c;
        }
        case Venue::Mansion: {
            int col = (int)(wx / 30), row = (int)(wy / 30);
            if (Hash2(col, row) < 0.07f) return n < 50 ? Color{60, 50, 50, 255} : Color{90, 80, 76, 255};   // broken tile
            Color c = ((col + row) % 2) ? Color{160, 56, 56, 255} : Color{214, 204, 186, 255};
            if (std::fabs(Noise(wx * 0.08f, wy * 0.08f) - 0.5f) < 0.015f) c = Shade(c, -40);   // marble veins / cracks
            if (n < 4) c = Shade(c, -30);
            return c;
        }
        case Venue::Volcano: {
            Color c = Shade(Color{64, 54, 52, 255}, (int)((Fbm(wx * 0.04f, wy * 0.06f) - 0.5f) * 30));
            float k = std::fabs(Noise(wx * 0.035f, wy * 0.05f) - 0.5f);
            if (k < 0.008f) c = Color{240, 130, 50, 255};
            else if (k < 0.02f) c = Color{130, 64, 42, 255};
            if (n < 3) c = Color{120, 116, 112, 255};   // ash
            return c;
        }
        case Venue::Hideout: {
            if (wx < 10 || wx > ARENA_W - 10 || wy < 14 || wy > ARENA_H - 14)
                return (((int)(wx + wy) / 6) % 2) ? Color{230, 190, 40, 255} : Color{40, 40, 40, 255};
            int col = (int)(wx / 40), row = (int)(wy / 40);
            float lx = std::fmod(wx, 40.0f), ly = std::fmod(wy, 40.0f);
            if (Hash2(col, row) < 0.045f) {   // spinner arrow tiles
                bool arrow = (std::fabs(lx - 20) < (ly - 12) * 0.6f && ly > 12 && ly < 24) || (std::fabs(lx - 20) < 2.5f && ly >= 24 && ly < 32);
                return arrow ? Color{240, 240, 240, 255} : Color{70, 110, 170, 255};
            }
            Color c = Shade(Color{108, 112, 120, 255}, (int)((Hash2(col, row) - 0.5f) * 14));
            if (lx < 1.0f || ly < 1.0f / DEPTH) c = Color{66, 68, 74, 255};
            if ((lx > 3 && lx < 5 && ly > 4 && ly < 7) || (lx > 35 && lx < 37 && ly > 33 && ly < 36)) c = Color{150, 154, 162, 255};
            return c;
        }
        case Venue::Lab: {
            Color c = Tiles(wx, wy, 24, 24, Color{232, 234, 238, 255}, Color{222, 224, 230, 255}, Color{196, 200, 208, 255}, false, 6);
            float dx = wx - ARENA_W / 2, dy = wy - ARENA_H / 2, d = std::sqrt(dx * dx + dy * dy);
            if (d < 64) {   // a Poke Ball painted on the lab floor
                if (d > 60) c = Color{60, 60, 70, 255};
                else if (std::fabs(dy) < 4) c = Color{60, 60, 70, 255};
                else c = dy < 0 ? Color{220, 70, 70, 255} : Color{240, 240, 244, 255};
                if (d < 16) c = d > 12 ? Color{60, 60, 70, 255} : Color{250, 250, 252, 255};
            } else if (std::fabs(wy - ARENA_H / 2) < 5) c = Color{120, 170, 230, 255};
            return c;
        }
        case Venue::Cave: case Venue::IceCave: {
            bool ice = v == Venue::IceCave;
            Color c = ice ? Color{180, 206, 230, 255} : Color{98, 90, 84, 255};
            c = Shade(c, (int)((Fbm(wx * 0.03f, wy * 0.05f) - 0.5f) * 50));
            if (std::fabs(Noise(wx * 0.04f, wy * 0.06f) - 0.5f) < 0.012f) c = ice ? Color{240, 250, 255, 255} : Color{58, 52, 48, 255};
            if (n < 4) c = Shade(c, ice ? 30 : 24);
            if (!ice && n == 99) c = Color{120, 220, 240, 255};   // a glinting crystal
            float edge = std::min(std::min(wx, ARENA_W - wx), std::min(wy, ARENA_H - wy) * 1.2f);
            if (edge < 40) c = Shade(c, -(int)((40 - edge) * 1.1f));   // dark toward the walls
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
    if (venue != Venue::Stadium) {
        gRng = 7654321u + (unsigned)venue * 104729u;
        VenueSurroundings(&img, venue, season);
        for (int sy = FLOOR_Y; sy < FLOOR_BOTTOM; sy++)
            for (int sx = ARENA_SX; sx < ARENA_SX + (int)ARENA_W; sx++)
                ImageDrawPixel(&img, sx, sy, VenueFloor(venue, season, (float)(sx - ARENA_SX) + 0.5f, (sy - FLOOR_Y + 0.5f) / DEPTH));
    } else {
        for (int sy = FLOOR_Y; sy < FLOOR_BOTTOM; sy++)
            for (int sx = ARENA_SX; sx < ARENA_SX + (int)ARENA_W; sx++) ImageDrawPixel(&img, sx, sy, FloorBase(kind, sx, sy));
    }

    if (f) {
        for (const Zone& z : f->zones) {
            switch (z.kind) {
                case Zone::Platform: {   // islands: each its own sand colour, wet shore, grass and pebbles
                    int tint = (int)(z.seed % 21) - 10;
                    bool grassy = z.seed % 3 != 0, rocky = z.seed % 4 == 1;
                    FloorEllipse(&img, z, [&](int sx, int sy, float wx, float wy, float edge) {
                        int n = R(100);
                        Color c = ((sx / 5 + sy / 3) % 2) ? Color{196, 176, 132, 255} : Color{188, 168, 124, 255};
                        c = Shade(c, tint);
                        if (edge < 5) c = Shade(Mix(c, Color{120, 110, 90, 255}, 0.45f), 0);   // wet sand at the shore
                        else if (grassy && edge > 9 && Noise(wx * 0.08f + z.seed % 97, wy * 0.08f) > 0.55f) c = n < 30 ? Color{96, 156, 70, 255} : Color{80, 140, 60, 255};
                        if (n < 5) c = Shade(c, -20);
                        if (rocky && n > 97 && edge > 4) c = Color{130, 126, 120, 255};
                        if (n == 50 && edge > 4) c = Color{240, 200, 210, 255};   // a shell
                        return c;
                    }, Color{110, 96, 72, 255}, 2.5f);
                } break;
                case Zone::TallGrass:
                    FloorEllipse(&img, z, [](int sx, int sy, float, float, float) {
                        int n = R(100);
                        Color c = Color{72, 140, 60, 255};
                        if (n < 25 && (sx + sy) % 3 == 0) c = Color{110, 180, 80, 255};
                        else if (n < 35) c = Color{56, 112, 48, 255};
                        return c;
                    }, Color{56, 110, 48, 255}, 1.5f);
                    break;
                case Zone::PoisonPool:
                    FloorEllipse(&img, z, [](int, int, float, float, float) {
                        int n = R(100);
                        return n < 6 ? Color{190, 110, 220, 255} : (n < 20 ? Color{120, 50, 150, 255} : Color{140, 64, 170, 255});
                    }, Color{60, 40, 40, 255}, 2.0f);
                    break;
                case Zone::LavaPool:
                    FloorEllipse(&img, z, [](int, int, float, float, float edge) {
                        int n = R(100);
                        if (edge < 5) return n < 30 ? Color{200, 70, 30, 255} : Color{150, 50, 26, 255};   // cooling crust at the edge
                        return n < 10 ? Color{255, 230, 120, 255} : (n < 40 ? Color{255, 150, 40, 255} : Color{240, 100, 30, 255});
                    }, Color{70, 30, 20, 255}, 2.5f);
                    break;
            }
        }
        if (f->spikeBand > 0) {  // toxic spikes along the edges
            for (int sy = FLOOR_Y; sy < FLOOR_BOTTOM; sy++)
                for (int sx = ARENA_SX; sx < ARENA_SX + (int)ARENA_W; sx++) {
                    float wx = (float)(sx - ARENA_SX), wy = (sy - FLOOR_Y) / DEPTH;
                    if (f->InSpikes({wx, wy})) ImageDrawPixel(&img, sx, sy, Shade(GetImageColor(img, sx, sy), -40));
                }
            const Color sd{110, 40, 140, 255}, sm{170, 80, 200, 255}, sl{235, 170, 250, 255};
            auto spike = [&](int x, int y, int h) {  // upward pixel spike, base at y
                for (int i = 0; i < h; i++) {
                    int w = std::max(1, (h - i) * 5 / h);
                    ImageDrawRectangle(&img, x - w / 2, y - i, w, 1, i < h / 3 ? sd : sm);
                }
                ImageDrawPixel(&img, x, y - h + 1, sl);
                ImageDrawPixel(&img, x - 1, y - h / 2, sl);
            };
            int topB = FLOOR_Y + (int)(f->spikeBand * DEPTH), botT = FLOOR_Y + (int)((ARENA_H - f->spikeBand * 0.7f) * DEPTH);
            for (int row = 0; row < 2; row++)
                for (int x = ARENA_SX + 4 + row * 4; x < ARENA_SX + (int)ARENA_W - 3; x += 8) spike(x, FLOOR_Y + 5 + row * 6, 6);
            for (int x = ARENA_SX + 4; x < ARENA_SX + (int)ARENA_W - 3; x += 8) spike(x, FLOOR_BOTTOM - 2, 6);
            int lx = ARENA_SX + (int)f->spikeBand, rx = ARENA_SX + (int)ARENA_W - (int)f->spikeBand;
            for (int y = topB + 6; y < botT; y += 7)
                for (int k = 0; k < 3; k++) {
                    spike(ARENA_SX + 4 + k * 8, y + (k % 2) * 3, 6);
                    spike(rx + 4 + k * 8, y + (k % 2) * 3, 6);
                }
            (void)lx;
        }
    }

    for (int i = 0; i < 6; i++)
        for (int sx = ARENA_SX; sx < ARENA_SX + (int)ARENA_W; sx++) {
            Color c = GetImageColor(img, sx, FLOOR_Y + i);
            ImageDrawPixel(&img, sx, FLOOR_Y + i, Shade(c, -(36 - i * 6)));
        }

    if (venue != Venue::Stadium) {   // trainer spots, chalked faintly
        FloorRing(&img, 120, ARENA_H / 2, 22, Mix(Color{150, 180, 220, 255}, VenueFloor(venue, season, 120, ARENA_H / 2 + 30), 0.55f));
        FloorRing(&img, ARENA_W - 120, ARENA_H / 2, 22, Mix(Color{225, 150, 150, 255}, VenueFloor(venue, season, ARENA_W - 120, ARENA_H / 2 + 30), 0.55f));
    } else if (kind == ArenaKind::Stadium) {
        const Color line{232, 220, 190, 255};
        for (float wy = 0; wy < ARENA_H; wy += 1.0f) {
            FloorPixel(&img, ARENA_W / 2 - 1, wy, line);
            FloorPixel(&img, ARENA_W / 2, wy, line);
        }
        FloorRing(&img, ARENA_W / 2, ARENA_H / 2, 70, line);
        FloorRing(&img, 120, ARENA_H / 2, 26, Color{150, 180, 220, 255});
        FloorRing(&img, ARENA_W - 120, ARENA_H / 2, 26, Color{225, 150, 150, 255});
    } else if (kind != ArenaKind::Water) {
        FloorRing(&img, 120, ARENA_H / 2, 22, Mix(Color{150, 180, 220, 255}, FloorBase(kind, 0, FLOOR_Y + 10), 0.3f));
        FloorRing(&img, ARENA_W - 120, ARENA_H / 2, 22, Mix(Color{225, 150, 150, 255}, FloorBase(kind, 0, FLOOR_Y + 10), 0.3f));
    }

    if (venue != Venue::Stadium) return img;
    ImageDrawRectangle(&img, ARENA_SX, FLOOR_Y, (int)ARENA_W, 1, th.trim);
    ImageDrawRectangle(&img, ARENA_SX - 1, FLOOR_Y, 1, FLOOR_BOTTOM - FLOOR_Y, Shade(th.trim, -20));
    ImageDrawRectangle(&img, ARENA_SX + (int)ARENA_W, FLOOR_Y, 1, FLOOR_BOTTOM - FLOOR_Y, Shade(th.trim, -20));
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
    gBattleTex = ToTex(BuildImage(kind, &f));
    gBattleLoaded = true;
    gBattleKind = kind;
    gBattleVenue = f.venue;
}

void UnloadArena() {
    UnloadTexture(gMenuTex);
    if (gBattleLoaded) UnloadTexture(gBattleTex);
    gBattleLoaded = false;
}

void DrawArena(float time) {
    DrawTexture(gMenuTex, (int)gShake.x, (int)gShake.y, WHITE);
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
        default: return Color{230, 200, 90, 255};
    }
}

namespace {
float H01(int a, int b) { return Hash2(a * 31 + 7, b * 17 + 3); }

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
        Color c = Fade(Color{255, 250, 220, 255}, 0.05f + hype * 0.05f);
        DrawTriangle(src, {aim.x - nrm.x, aim.y - nrm.y}, {aim.x + nrm.x, aim.y + nrm.y}, c);
        DrawTriangle(src, {aim.x + nrm.x, aim.y + nrm.y}, {aim.x - nrm.x, aim.y - nrm.y}, c);
        DrawEllipse((int)aim.x, (int)aim.y, 36, 36 * DEPTH, Fade(Color{255, 250, 220, 255}, 0.07f + hype * 0.05f));
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
        int count = (int)(VW / (5.0f * scale));
        for (int i = 0; i < count; i++) {
            float x = i * 5.0f * scale + H01(i, r) * 3;
            float jump = std::max(0.0f, std::sin(time * (5 + H01(i, r + 11) * 5) + i * 0.7f + r)) * (0.5f + hype * 2.0f) * scale;
            int hy = y - (int)jump;
            Color sh = shirts[(int)(H01(i, r + 2) * 9) % 9];
            int w = std::max(2, (int)(3 * scale));
            DrawRectangle((int)x, hy + 3, w, 4, Fade(sh, 0.7f + 0.3f * scale));
            DrawRectangle((int)x + w / 3, hy + 1, std::max(1, w - 1), 2, Color{230, 190, 150, 255});
            if (H01(i, r + 6) > 0.9f) {
                float wv = std::sin(time * 6 + i) * 2;
                DrawRectangle((int)x + w, hy - 5, 1, 7, Color{100, 80, 60, 255});
                DrawTriangle({x + w + 1, hy - 5.0f}, {x + w + 1, hy - 1.0f}, {x + w + 8 + wv, hy - 3.0f}, team);
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

void DrawBattleArena(float time, float hype) {
    if (!gBattleLoaded) { DrawArena(time); return; }
    DrawTexture(gBattleTex, (int)gShake.x, (int)gShake.y, WHITE);
    if (gBattleVenue == Venue::Stadium) DrawTorches(time, gBattleKind);
    if (gBattleKind != ArenaKind::Stadium) DrawGymSpectacle(time, gBattleKind, hype);
}
