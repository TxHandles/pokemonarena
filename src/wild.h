#pragma once
#include <string>
#include <vector>
#include "battle.h"

// Wild Pokemon by place (wild.txt, generated from the FireRed/LeafGreen encounter tables).
// Training battles happen in one area the story has reached; rare encounters come from it too.
struct WildEntry {
    int species = -1;
    std::string method;   // walk, surf, old-rod, good-rod, super-rod, rock-smash
    int rate = 0;         // percent of that method's encounters
    int lo = 2, hi = 5;   // level range in the games
    int badges = 0;       // needs this many badges
    bool rare = false;    // only as a rare encounter
};
struct WildArea {
    std::string name;
    std::string unlock;   // story stage that opens it ("@done" = after the last story battle)
    std::vector<WildEntry> mons;
};

void LoadWild(std::string& log);
const std::vector<WildArea>& WildAreas();
int WildAreaIndex(const std::string& name);                 // -1 if unknown
bool WildAreaOpen(int area, int storyStage);
std::vector<int> OpenWildAreas(int storyStage);             // in story order
int DefaultWildArea(int storyStage);                        // the newest open area
int ResolveWildArea(const std::string& chosen, int storyStage);   // the chosen one if open, else the newest
std::string WildMethodName(const std::string& method);     // "walk" -> "grass", "super-rod" -> "Super Rod"...

// Where a species lives: area names (open ones first), and how many more lie further on.
std::vector<std::string> WildPlacesFor(int species, int storyStage, int& lockedCount);

// Training context: set before building a training battle (MakeCpuTeam(nullptr, ...) and
// PickRareSpecies then use the area's table). area < 0 turns it off.
void SetWildContext(int area, int storyStage, int badges);
bool WildActive();
int WildContextArea();
std::vector<CombatantSpec> MakeWildTeam(int n, int avgLevel);
int PickRareWild(const std::vector<std::string>& owned);    // -1 if the area has nothing
bool WildAvailableNow(int species, int storyStage, int badges);   // catchable (not rare-only) somewhere already reached
