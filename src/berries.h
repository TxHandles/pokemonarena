#pragma once
#include <string>

// Hoenn's berries (Emerald): how they grow on the berry trees, and their flavours (for Pokeblocks and Contests).
struct BerryInfo {
    const char* name;      // "Oran Berry"
    int hours;             // hours per growth stage (four stages: sprout, growing, flowering, berries)
    int maxYield, minYield;
    int flavor[5];         // spicy, dry, sweet, bitter, sour
    int smooth;            // smoothness (a Pokeblock's feel)
};
int BerryCount();
const BerryInfo& BerryAt(int i);       // in Emerald's order (the rows of World/berrytrees.png)
int BerryIndexOf(const std::string& name);   // "Oran" or "Oran Berry"; -1 if it isn't one
const char* FlavorName(int k);         // "Spicy", "Dry", "Sweet", "Bitter", "Sour"
