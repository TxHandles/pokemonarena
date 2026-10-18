#include "rng.h"
#include "raylib.h"

SimRng* gSimRng = nullptr;

int SimRand(int lo, int hi) {
    if (!gSimRng) return GetRandomValue(lo, hi);
    if (lo > hi) { int t = lo; lo = hi; hi = t; }
    uint32_t span = (uint32_t)(hi - lo) + 1u;
    return lo + (int)(gSimRng->Next() % span);
}

int gLocalInput = -1;   // see battle.h
