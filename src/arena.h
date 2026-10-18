#pragma once
#include "raylib.h"

// Logical resolution. All game code draws in these units. The frame is rendered to a
// 3x hi-res canvas (1920x1080), which lets creature sprites use 4 canvas pixels per art
// pixel (4/3 logical units) while everything else uses 3.
constexpr int VW = 640;
constexpr int VH = 360;
constexpr int CANVAS_ZOOM = 3;
constexpr float SPRITE_PIXEL = 4.0f / 3.0f;  // logical units per creature-art pixel in battle

// The arena floor in world units. World Y is squashed by DEPTH on screen to give the
// slightly-angled, almost top-down view.
constexpr float ARENA_W = 620.0f;
constexpr float ARENA_H = 480.0f;
constexpr float DEPTH = 0.55f;
constexpr int ARENA_SX = 10;   // screen x of the floor's left edge
constexpr int FLOOR_Y = 36;    // screen y of the floor's back edge
constexpr int FLOOR_BOTTOM = FLOOR_Y + (int)(ARENA_H * DEPTH);  // 300
constexpr int STANDS_H = 24;   // crowd rows above the back wall

inline Vector2 gShake{0, 0};   // screen shake offset applied to the arena layer

// World position (x, y on the floor, z = height above it) to screen position.
inline Vector2 ToScreen(Vector2 w, float z = 0.0f) {
    return {ARENA_SX + w.x + gShake.x, FLOOR_Y + w.y * DEPTH - z + gShake.y};
}

enum class ArenaKind;
struct Field;

void LoadArena();                                    // menu stadium
void UnloadArena();
void DrawArena(float time);                          // menu stadium background
void SetBattleArena(ArenaKind kind, const Field& f); // bake the battle floor for a layout
void DrawBattleArena(float time, float hype = 0);   // hype 0..1: how loud the crowd is (gyms)
Color GymColor(ArenaKind k);                      // the gym's team colour (banners, fans)
void DrawGymStadium(float time, ArenaKind kind, float hype, int top, int bottom);   // a huge animated stadium (gym intros)
