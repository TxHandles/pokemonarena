#include "ui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

Color TeamColor(int side) { return side == 0 ? Color{90, 170, 255, 255} : Color{255, 96, 96, 255}; }

int TextW(const char* text, int size) { return MeasureText(text, size); }

void DrawTextOutlined(const char* text, int x, int y, int size, Color c, Color outline) {
    DrawText(text, x - 1, y, size, outline);
    DrawText(text, x + 1, y, size, outline);
    DrawText(text, x, y - 1, size, outline);
    DrawText(text, x, y + 1, size, outline);
    DrawText(text, x, y, size, c);
}

void DrawTextShadow(const char* text, int x, int y, int size, Color c) {
    DrawText(text, x + 1, y + 1, size, UI_INK);
    DrawText(text, x, y, size, c);
}

void DrawTextCentered(const char* text, int cx, int y, int size, Color c, bool outlined) {
    int x = cx - TextW(text, size) / 2;
    if (outlined) DrawTextOutlined(text, x, y, size, c);
    else DrawText(text, x, y, size, c);
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

void DrawPanel(int x, int y, int w, int h, Color fill, Color border) {
    DrawRectangle(x, y, w, h, fill);
    RectLines(x, y, w, h, border);
    DrawRectangle(x + 1, y + 1, w - 2, 1, Fade(WHITE, 0.08f));
}

void DrawHpBar(int x, int y, int w, int h, float frac) {
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    Color c = frac > 0.5f ? Color{96, 216, 96, 255} : (frac > 0.2f ? Color{240, 200, 60, 255} : Color{232, 72, 64, 255});
    DrawRectangle(x - 1, y - 1, w + 2, h + 2, UI_INK);
    DrawRectangle(x, y, w, h, Color{60, 56, 72, 255});
    int fw = (int)(w * frac + 0.5f);
    DrawRectangle(x, y, fw, h, c);
    if (h > 2) DrawRectangle(x, y, fw, 1, ColorBrightness(c, 0.35f));
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
    DrawText(t, x + 26, y + 2, 10, n == 2 ? Color{140, 230, 150, 255} : n == 1 ? UI_GOLD : UI_DIM);
}

void DrawStatBar(const char* label, int value, int x, int y, int barW, int natureMark) {
    DrawText(label, x, y, 10, natureMark > 0 ? Color{255, 110, 100, 255} : natureMark < 0 ? Color{110, 160, 255, 255} : UI_DIM);
    char buf[8];
    std::snprintf(buf, sizeof buf, "%d", value);
    DrawText(buf, x + 26, y, 10, UI_TEXT);
    float f = value / 130.0f;
    if (f > 1) f = 1;
    Color c = value >= 95 ? Color{110, 220, 120, 255} : (value >= 70 ? Color{240, 200, 70, 255} : Color{240, 130, 80, 255});
    DrawRectangle(x + 46, y + 2, barW, 5, Color{60, 56, 72, 255});
    DrawRectangle(x + 46, y + 2, (int)(barW * f), 5, c);
}

int DrawWrapped(const char* text, int x, int y, int w, int size, Color c, int lineH) {
    std::string s = text, line, word;
    auto flush = [&]() {
        if (!line.empty()) { DrawText(line.c_str(), x, y, size, c); y += lineH; line.clear(); }
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
