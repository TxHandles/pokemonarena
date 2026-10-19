#pragma once
#include "raylib.h"
#include "types.h"

constexpr Color UI_INK{20, 16, 30, 255};
constexpr Color UI_PANEL{38, 34, 56, 235};
constexpr Color UI_PANEL_LIGHT{58, 52, 82, 255};
constexpr Color UI_BORDER{150, 140, 180, 255};
constexpr Color UI_TEXT{236, 232, 244, 255};
constexpr Color UI_DIM{150, 144, 170, 255};
constexpr Color UI_GOLD{255, 214, 90, 255};

Color TeamColor(int side);

int TextW(const char* text, int size = 10);
void DrawTextOutlined(const char* text, int x, int y, int size, Color c, Color outline = UI_INK);
void DrawTextShadow(const char* text, int x, int y, int size, Color c);
void DrawTextCentered(const char* text, int cx, int y, int size, Color c, bool outlined = true);
int DrawTypeBadge(Type t, int x, int y);  // returns the badge width
// Pixel-thick outlines (raylib's *Lines functions draw hairlines, which get thin on the hi-res canvas).
void RectLines(int x, int y, int w, int h, Color c);
void EllipsePix(float cx, float cy, float rx, float ry, Color c);
void DrawPanel(int x, int y, int w, int h, Color fill = UI_PANEL, Color border = UI_BORDER);
void DrawHpBar(int x, int y, int w, int h, float frac);
// Co-op: two pips (player 1 gold, player 2 cyan) and "READY n/2", right-aligned at rightX.
void DrawReadyCount(int rightX, int y, bool p1Ready, bool p2Ready);
void DrawStatBar(const char* label, int value, int x, int y, int barW, int natureMark = 0);  // +1 red label, -1 blue
// Word-wrapped text; returns the y below the last line.
int DrawWrapped(const char* text, int x, int y, int w, int size, Color c, int lineH = 12);
// A little gold star: marks a shiny Pokemon (centre x, y; r = size).
void DrawShinyStar(float x, float y, float r = 3.5f);
