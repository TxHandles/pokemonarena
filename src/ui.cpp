#include "ui.h"
#include <algorithm>
#include <cmath>
#include "vec.h"
#include <cmath>
#include <cstdio>
#include <string>

Color TeamColor(int side) { return side == 0 ? Color{90, 170, 255, 255} : Color{255, 96, 96, 255}; }

int TextW(const char* text, int size) { return GMeasureText(text, size); }

void DrawTextOutlined(const char* text, int x, int y, int size, Color c, Color outline) {
    GDrawText(text, x - 1, y, size, outline);
    GDrawText(text, x + 1, y, size, outline);
    GDrawText(text, x, y - 1, size, outline);
    GDrawText(text, x, y + 1, size, outline);
    GDrawText(text, x, y, size, c);
}

void DrawTextShadow(const char* text, int x, int y, int size, Color c) {
    GDrawText(text, x + 1, y + 1, size, UI_INK);
    GDrawText(text, x, y, size, c);
}

void DrawTextCentered(const char* text, int cx, int y, int size, Color c, bool outlined) {
    int x = cx - TextW(text, size) / 2;
    if (outlined) DrawTextOutlined(text, x, y, size, c);
    else GDrawText(text, x, y, size, c);
}

int DrawTypeBadge(Type t, int x, int y) {
    const char* name = TypeName(t);
    int w = TextW(name, 10) + 6;
    Color c = TypeColor(t);
    DrawRectangle(x, y, w, 11, c);
    DrawRectangle(x, y + 9, w, 2, ColorBrightness(c, -0.3f));
    RectLines(x, y, w, 11, ColorBrightness(c, -0.55f));
    DrawTextShadow(name, x + 3, y + 1, 10, WHITE);
    return w;
}

void RectLines(int x, int y, int w, int h, Color c) {
    DrawRectangle(x, y, w, 1, c);
    DrawRectangle(x, y + h - 1, w, 1, c);
    DrawRectangle(x, y + 1, 1, h - 2, c);
    DrawRectangle(x + w - 1, y + 1, 1, h - 2, c);
}

void EllipsePix(float cx, float cy, float rx, float ry, Color c) {
    if (rx < 0.5f || ry < 0.3f) return;
    int steps = (int)(2.0f * 3.14159f * std::max(rx, ry) * 1.2f) + 8;
    int lastX = -99999, lastY = -99999;
    for (int i = 0; i < steps; i++) {
        float a = i * 6.2831853f / steps;
        int x = (int)std::floor(cx + std::cos(a) * rx);
        int y = (int)std::floor(cy + std::sin(a) * ry);
        if (x == lastX && y == lastY) continue;
        DrawRectangle(x, y, 1, 1, c);
        lastX = x;
        lastY = y;
    }
}

// Panels: a soft drop shadow, a gentle top-to-bottom gradient, rounded corners, a lit top edge.
// Every screen uses these, so this is the game's whole panel style.
void DrawPanel(int x, int y, int w, int h, Color fill, Color border) {
    if (w <= 2 || h <= 2) { DrawRectangle(x, y, w, h, fill); return; }
    float r = std::min(4.0f, std::min(w, h) * 0.3f);
    float rr = r / (std::min(w, h) * 0.5f);
    Rectangle rc{(float)x, (float)y, (float)w, (float)h};
    if (fill.a > 120) DrawRectangleRounded({rc.x + 1, rc.y + 1.5f, rc.width, rc.height}, rr, 6, Fade(BLACK, 0.28f * fill.a / 255.0f));
    DrawRectangleRounded(rc, rr, 6, fill);
    Color top = ColorBrightness(fill, 0.10f), bot = ColorBrightness(fill, -0.10f);
    top.a = bot.a = (unsigned char)(fill.a * 0.9f);
    if (h > 2 * r) DrawRectangleGradientV(x + 1, (int)(y + r * 0.5f), w - 2, (int)(h - r), top, bot);
    DrawRectangleRoundedLinesEx(rc, rr, 6, 1.0f, border);
    DrawRectangleRoundedLinesEx({rc.x + 1, rc.y + 1, rc.width - 2, rc.height - 2}, rr, 6, 0.34f, Fade(WHITE, 0.10f));
    DrawRectangle(x + (int)r, y + 1, w - 2 * (int)r, 1, Fade(WHITE, 0.16f));
}

void DrawHpBar(int x, int y, int w, int h, float frac) { DrawHpBarFx(x, y, w, h, frac, frac, 0); }

// The HP bar: a dark trough, a red trail showing damage just taken, the bar in green / yellow /
// red with a gloss line and segment ticks, pulsing when low.
void DrawHpBarFx(int x, int y, int w, int h, float frac, float trail, float time) {
    frac = std::max(0.0f, std::min(1.0f, frac));
    trail = std::max(frac, std::min(1.0f, trail));
    Color c = frac > 0.5f ? Color{88, 214, 104, 255} : (frac > 0.2f ? Color{246, 196, 52, 255} : Color{236, 68, 60, 255});
    if (frac <= 0.2f && frac > 0 && time > 0) c = ColorBrightness(c, 0.25f * (0.5f + 0.5f * std::sin(time * 10)));
    float r = h > 3 ? 0.5f : 0.3f;
    DrawRectangleRounded({(float)x - 1, (float)y - 1, (float)w + 2, (float)h + 2}, r, 4, UI_INK);
    DrawRectangle(x, y, w, h, Color{52, 48, 66, 255});
    float tw = w * trail, fw = w * frac;
    if (tw > fw + 0.2f) DrawRectangleV({x + fw, (float)y}, {tw - fw, (float)h}, Color{255, 120, 90, 255});
    if (fw > 0.2f) {
        DrawRectangleV({(float)x, (float)y}, {fw, (float)h}, c);
        if (h > 2) {
            DrawRectangleV({(float)x, (float)y}, {fw, std::max(0.67f, h * 0.34f)}, ColorBrightness(c, 0.35f));
            DrawRectangleV({(float)x, (float)y + h - 0.67f}, {fw, 0.67f}, ColorBrightness(c, -0.3f));
        }
    }
    if (w >= 60 && h >= 4)   // tick marks every tenth
        for (int k = 1; k < 10; k++) DrawRectangleV({x + w * k / 10.0f, (float)y}, {0.34f, (float)h}, Fade(UI_INK, 0.35f));
}

// A Poke Ball: red top, white bottom, black band and button. 'open' 0..1 lifts the top half.
void DrawPokeBall(Vector2 c, float r, float rotDeg, float open, Color top, unsigned char alpha) {
    Color ink{20, 18, 30, alpha}, white{244, 244, 248, alpha};
    top.a = alpha;
    Vector2 d{std::cos(rotDeg * DEG2RAD), std::sin(rotDeg * DEG2RAD)};
    Vector2 up{d.y, -d.x};
    Vector2 lift = up * (open * r * 0.9f);
    DrawCircleV(c, r + std::max(0.6f, r * 0.1f), ink);
    DrawCircleSector(c, r, rotDeg, 180 + rotDeg, 24, white);
    DrawCircleSector(c, r * 0.92f, rotDeg + 20, 160 + rotDeg, 16, ColorBrightness(white, -0.12f));
    if (open > 0) {
        DrawCircleV(c, r * 0.8f, Color{255, 255, 255, alpha});   // light pouring out
        DrawCircleV(c + lift, r + std::max(0.6f, r * 0.1f), ink);
    }
    DrawCircleSector(c + lift, r, -180 + rotDeg, rotDeg, 24, top);
    DrawCircleV(c + lift - up * (r * 0.45f) - d * (r * 0.35f), r * 0.22f, Fade(WHITE, 0.55f * alpha / 255.0f));   // shine
    DrawLineEx(c + lift - d * r, c + lift + d * r, std::max(0.6f, r * 0.18f), ink);
    if (open <= 0) {
        DrawCircleV(c, r * 0.34f, ink);
        DrawCircleV(c, r * 0.21f, white);
    }
}

void DrawReadyCount(int rightX, int y, bool r0, bool r1) {
    int n = (r0 ? 1 : 0) + (r1 ? 1 : 0);
    const char* t = TextFormat("READY %d/2", n);
    int tw = TextW(t, 10), w = tw + 34;
    int x = rightX - w;
    Color edge = n == 2 ? Color{140, 230, 150, 255} : Color{90, 84, 116, 255};
    DrawRectangle(x, y, w, 14, Color{20, 18, 32, 230});
    DrawRectangleLines(x, y, w, 14, edge);
    Color pc[2] = {UI_GOLD, Color{120, 230, 255, 255}};
    bool rd[2] = {r0, r1};
    for (int p = 0; p < 2; p++) {
        int px = x + 4 + p * 10;
        if (rd[p]) DrawRectangle(px, y + 4, 7, 7, pc[p]);
        else DrawRectangleLines(px, y + 4, 7, 7, Fade(pc[p], 0.6f));
    }
    GDrawText(t, x + 26, y + 2, 10, n == 2 ? Color{140, 230, 150, 255} : n == 1 ? UI_GOLD : UI_DIM);
}

void DrawStatBar(const char* label, int value, int x, int y, int barW, int natureMark) {
    GDrawText(label, x, y, 10, natureMark > 0 ? Color{255, 110, 100, 255} : natureMark < 0 ? Color{110, 160, 255, 255} : UI_DIM);
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", value);
    GDrawText(buf, x + 26, y, 10, UI_TEXT);
    float f = value / 130.0f;
    if (f > 1) f = 1;
    Color c = value >= 95 ? Color{110, 220, 120, 255} : (value >= 70 ? Color{240, 200, 70, 255} : Color{240, 130, 80, 255});
    DrawRectangle(x + 46, y + 2, barW, 5, Color{60, 56, 72, 255});
    DrawRectangle(x + 46, y + 2, (int)(barW * f), 5, c);
}

int DrawWrapped(const char* text, int x, int y, int w, int size, Color c, int lineH) {
    std::string s = text, line, word;
    auto flush = [&]() {
        if (!line.empty()) { GDrawText(line.c_str(), x, y, size, c); y += lineH; line.clear(); }
    };
    for (size_t i = 0; i <= s.size(); i++) {
        char ch = i < s.size() ? s[i] : ' ';
        if (ch == ' ' || ch == '\n') {
            if (!word.empty()) {
                std::string t = line.empty() ? word : line + " " + word;
                if (TextW(t.c_str(), size) > w && !line.empty()) { flush(); line = word; }
                else line = t;
                word.clear();
            }
            if (ch == '\n') flush();
        } else {
            word += ch;
        }
    }
    flush();
    return y;
}

void DrawShinyStar(float x, float y, float r) {
    Color gold{255, 214, 80, 255};
    Vector2 c{x, y};
    for (int k = 0; k < 4; k++) {   // four points
        float a = k * 1.5708f;
        Vector2 tip{x + std::cos(a) * r, y + std::sin(a) * r};
        Vector2 s1{x + std::cos(a + 0.785f) * r * 0.32f, y + std::sin(a + 0.785f) * r * 0.32f};
        Vector2 s2{x + std::cos(a - 0.785f) * r * 0.32f, y + std::sin(a - 0.785f) * r * 0.32f};
        DrawTriangle(c, tip, s1, gold);
        DrawTriangle(c, s2, tip, gold);
        DrawTriangle(c, s1, tip, gold);
        DrawTriangle(c, tip, s2, gold);
    }
    DrawCircleV(c, r * 0.25f, WHITE);
}

// ------------------------------------------------------------------ menu motion and backdrops
#include "settings.h"
float gUiScreenT = 10.0f;

float UiEnterT(int index, float delay, float dur) {
    if (Cfg().fxLevel >= 2) return 1.0f;
    float u = (gUiScreenT - index * delay) / dur;
    if (u <= 0) return 0;
    if (u >= 1) return 1;
    float k = 1 - u;
    return 1 - k * k * k;   // ease out
}

static Color Shade(Color c, int d) {
    auto cl = [](int v) { return (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v); };
    return Color{cl(c.r + d), cl(c.g + d), cl(c.b + d), 255};
}

void DrawMenuBackdrop(Color base, float t) {
    ClearBackground(base);
    const int W = 640, H = 360;
    DrawRectangleGradientV(0, 0, W, H, Shade(base, 16), Shade(base, -8));
    if (Cfg().fxLevel >= 2) return;
    // slow diagonal light bands
    for (int i = 0; i < 5; i++) {
        float x = std::fmod(t * 9.0f + i * 170.0f, 900.0f) - 180.0f;
        Vector2 a{x, 0}, b{x + 60, 0}, c{x - 120 + 60, (float)H}, d{x - 120, (float)H};
        Color col = Fade(WHITE, 0.018f + 0.008f * (i % 2));
        DrawTriangle(a, d, c, col);
        DrawTriangle(a, c, b, col);
    }
    // drifting Poke Ball silhouettes
    for (int i = 0; i < 9; i++) {
        float sp = 4.0f + (i % 3) * 2.5f;
        float r = 14.0f + (i * 7) % 22;
        float x = std::fmod(i * 97.0f + t * sp, (float)W + 80) - 40;
        float y = std::fmod(i * 61.0f + 30 - t * sp * 0.6f + 4000.0f, (float)H + 80) - 40;
        float rot = t * (i % 2 ? 10.0f : -8.0f) + i * 40;
        Color ring = Fade(WHITE, 0.05f);
        DrawRing({x, y}, r - 1.6f, r, 0, 360, 36, ring);
        float ca = std::cos(rot * DEG2RAD), sa = std::sin(rot * DEG2RAD);
        DrawLineEx({x - ca * r, y - sa * r}, {x + ca * r, y + sa * r}, 1.6f, ring);
        DrawRing({x, y}, r * 0.22f, r * 0.22f + 1.5f, 0, 360, 20, ring);
    }
    // a soft vignette
    DrawRectangleGradientV(0, 0, W, 40, Fade(BLACK, 0.18f), Fade(BLACK, 0));
    DrawRectangleGradientV(0, H - 40, W, 40, Fade(BLACK, 0), Fade(BLACK, 0.22f));
}

void DrawStatHex(float cx, float cy, float r, const int v[6], Color fill, float grow) {
    Vector2 o[6], p[6];
    for (int k = 0; k < 6; k++) {
        float a = (-90 + k * 60) * DEG2RAD;
        o[k] = {cx + std::cos(a) * r, cy + std::sin(a) * r};
        float f = std::min(1.15f, std::max(0.06f, v[k] / 130.0f)) * grow;
        p[k] = {cx + std::cos(a) * r * f, cy + std::sin(a) * r * f};
    }
    for (int ring = 1; ring <= 3; ring++) {
        float s = ring / 3.0f;
        for (int k = 0; k < 6; k++) {
            Vector2 a{cx + (o[k].x - cx) * s, cy + (o[k].y - cy) * s}, b{cx + (o[(k + 1) % 6].x - cx) * s, cy + (o[(k + 1) % 6].y - cy) * s};
            DrawLineEx(a, b, ring == 3 ? 1.2f : 0.7f, Fade(UI_BORDER, ring == 3 ? 0.55f : 0.25f));
        }
    }
    for (int k = 0; k < 6; k++) DrawLineEx({cx, cy}, o[k], 0.7f, Fade(UI_BORDER, 0.25f));
    Color f = Fade(fill, 0.55f);
    for (int k = 0; k < 6; k++) {
        DrawTriangle({cx, cy}, p[(k + 1) % 6], p[k], f);
    }
    for (int k = 0; k < 6; k++) DrawLineEx(p[k], p[(k + 1) % 6], 1.3f, fill);
    for (int k = 0; k < 6; k++) DrawCircleV(p[k], 1.4f, WHITE);
}

// ------------------------------------------------------------------ the text font
// Pixelify Sans (SIL Open Font License, fonts/OFL.txt), baked into font_data.cpp. Every bit of text goes
// through GDrawText / GMeasureText, which take raylib's DrawText sizes: a size-10 line is drawn at 12 px
// so it has the same cap height as raylib's built-in font, and roughly the same width.
extern const unsigned int kFontDataSize;
extern const unsigned char kFontData[];
namespace {
constexpr int kAtlasN = 3;
constexpr int kAtlasPx[kAtlasN] = {36, 72, 108};   // canvas pixels for size 10, 20 and 30 text
Font gFonts[kAtlasN];
int gFontState = 0;   // 0 not loaded yet, 1 ready, -1 failed (use raylib's font)
constexpr float kFontScale = 1.2f;

bool FontReady() {
    if (gFontState != 0) return gFontState > 0;
    if (!IsWindowReady()) return false;
    gFontState = -1;
    for (int i = 0; i < kAtlasN; i++) {
        gFonts[i] = LoadFontFromMemory(".ttf", kFontData, (int)kFontDataSize, kAtlasPx[i], nullptr, 0);
        if (gFonts[i].texture.id == 0 || gFonts[i].glyphCount == 0) return false;
        SetTextureFilter(gFonts[i].texture, TEXTURE_FILTER_BILINEAR);
    }
    gFontState = 1;
    return true;
}

const Font& FontFor(float px) {
    float want = px * 3.0f;   // the canvas is 3x the logical screen
    for (int i = 0; i < kAtlasN; i++)
        if (kAtlasPx[i] >= want * 0.92f) return gFonts[i];
    return gFonts[kAtlasN - 1];
}
} // namespace

void GDrawText(const char* text, int x, int y, int size, Color c) {
    if (!text || !*text) return;
    if (!FontReady()) { DrawText(text, x, y, size, c); return; }
    float fs = size * kFontScale;
    DrawTextEx(FontFor(fs), text, Vector2{(float)x, y - size * 0.12f}, fs, size * 0.025f, c);
}

int GMeasureText(const char* text, int size) {
    if (!text || !*text) return 0;
    if (!FontReady()) return MeasureText(text, size);
    float fs = size * kFontScale;
    return (int)std::lround(MeasureTextEx(FontFor(fs), text, fs, size * 0.025f).x);
}
