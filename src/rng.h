#pragma once
#include <cstdint>

// The battle's own random numbers. While a battle updates, all of its randomness comes from
// here, so two games given the same state and the same button presses play out the same way
// (online co-op runs the battle on both computers in step). Outside an update (drawing), the
// normal random generator is used.
struct SimRng {
    uint64_t s = 0x2545F4914F6CDD1Dull;
    uint32_t Next() {  // splitmix64
        uint64_t z = (s += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return (uint32_t)((z ^ (z >> 31)) >> 32);
    }
};

extern SimRng* gSimRng;
int SimRand(int lo, int hi);  // like GetRandomValue: lo..hi inclusive

struct SimRngScope {  // makes this battle's generator the active one while in scope
    SimRng* prev;
    explicit SimRngScope(SimRng& r) : prev(gSimRng) { gSimRng = &r; }
    ~SimRngScope() { gSimRng = prev; }
};
