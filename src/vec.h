#pragma once
// Small Vector2 helpers (kept separate from raymath to avoid operator clashes between raylib versions).
#include "raylib.h"
#include <cmath>

inline Vector2 operator+(Vector2 a, Vector2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vector2 operator-(Vector2 a, Vector2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vector2 operator-(Vector2 a) { return {-a.x, -a.y}; }
inline Vector2 operator*(Vector2 a, float s) { return {a.x * s, a.y * s}; }
inline Vector2 operator*(float s, Vector2 a) { return {a.x * s, a.y * s}; }
inline Vector2& operator+=(Vector2& a, Vector2 b) { a.x += b.x; a.y += b.y; return a; }
inline Vector2& operator-=(Vector2& a, Vector2 b) { a.x -= b.x; a.y -= b.y; return a; }
inline Vector2& operator*=(Vector2& a, float s) { a.x *= s; a.y *= s; return a; }

inline float Len(Vector2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
inline float Dist(Vector2 a, Vector2 b) { return Len(a - b); }
inline float Dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
inline Vector2 SafeNorm(Vector2 v, Vector2 fallback = {1, 0}) {
    float l = Len(v);
    return l > 1e-4f ? v * (1.0f / l) : fallback;
}
inline Vector2 Rot(Vector2 v, float a) {
    float c = std::cos(a), s = std::sin(a);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}
inline Vector2 LerpV(Vector2 a, Vector2 b, float t) { return a + (b - a) * t; }
inline float DistPointSeg(Vector2 p, Vector2 a, Vector2 b) {
    Vector2 ab = b - a;
    float l2 = Dot(ab, ab);
    float t = l2 > 0 ? Dot(p - a, ab) / l2 : 0;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return Dist(p, a + ab * t);
}
