#pragma once
#include "raylib.h"

// The 18 elemental types.
enum class Type : int {
    Normal, Fire, Water, Grass, Electric, Ice, Fighting, Poison, Ground,
    Flying, Psychic, Bug, Rock, Ghost, Dragon, Dark, Steel, Fairy,
    Count,
    None = -1
};
constexpr int TYPE_COUNT = (int)Type::Count;

const char* TypeName(Type t);
Color TypeColor(Type t);

// Multiplier for one attacking type against one defending type (0, 0.5, 1 or 2).
float TypeEffectiveness(Type attack, Type defend);
// Combined multiplier against a (possibly dual-typed) defender. Pass Type::None for def2 if single-typed.
float TypeEffectiveness(Type attack, Type def1, Type def2);
