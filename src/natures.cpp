#include "natures.h"
#include "moves.h"

namespace {
const char* kNames[NATURE_COUNT] = {
    "Hardy", "Lonely", "Brave", "Adamant", "Naughty",
    "Bold", "Docile", "Relaxed", "Impish", "Lax",
    "Timid", "Hasty", "Serious", "Jolly", "Naive",
    "Modest", "Mild", "Quiet", "Bashful", "Rash",
    "Calm", "Gentle", "Sassy", "Careful", "Quirky",
};
bool Valid(int n) { return n >= 0 && n < NATURE_COUNT; }
} // namespace

const char* NatureName(int n) { return Valid(n) ? kNames[n] : "Hardy"; }
int NatureUp(int n) { return Valid(n) && n / 5 != n % 5 ? n / 5 : -1; }
int NatureDown(int n) { return Valid(n) && n / 5 != n % 5 ? n % 5 : -1; }
float NatureMul(int n, int stat) {
    if (NatureUp(n) == stat) return 1.1f;
    if (NatureDown(n) == stat) return 0.9f;
    return 1.0f;
}
int NatureByName(const std::string& name) {
    std::string k = Squash(name);
    for (int i = 0; i < NATURE_COUNT; i++)
        if (Squash(kNames[i]) == k) return i;
    return -1;
}
const char* NatureStatName(int s) {
    static const char* n[5] = {"Attack", "Defense", "Speed", "Sp. Atk", "Sp. Def"};
    return s >= 0 && s < 5 ? n[s] : "";
}
