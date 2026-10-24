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

// The Pokedex's "where to find it" list: one row per area and method the species turns up with.
struct WildSpot {
    int area = -1;
    std::string place;
    bool open = false;          // the story has reached the area
    std::string opensAt;        // the story stage that opens it
    std::string method;         // walk, surf, ...
    bool methodOpen = true;     // rods / Surf / Rock Smash unlocked
    std::string methodAt;       // the stage that unlocks the method
    int rate = 0, lo = 0, hi = 0, badges = 0;
    bool rare = false;          // only as a rare encounter
    float share = 0;            // of this area's training encounters right now (0..1), 0 if not available yet
};
std::vector<WildSpot> WildSpotsFor(int species, int storyStage, int badges);

// Everything that can turn up in an area right now (one row per species): its share of the
// training encounters (0..1, 0 for rare-only ones), whether it's only a rare find, and the level range.
struct WildAreaMon { int species = -1; float share = 0; bool rare = false; int lo = 0, hi = 0; std::string method; };
std::vector<WildAreaMon> WildAreaMons(int area, int storyStage, int badges);

// Training context: set before building a training battle (MakeCpuTeam(nullptr, ...) and
// PickRareSpecies then use the area's table). area < 0 turns it off.
void SetWildContext(int area, int storyStage, int badges);
bool WildActive();
int WildContextArea();
std::vector<CombatantSpec> MakeWildTeam(int n, int avgLevel);
void SetWildMethod(const std::string& method);   // the overworld: "surf", "old-rod"... only that method's Pokemon ("" any)
bool WildAreaHasMethod(const std::string& area, const std::string& method);
int PickRareWild(const std::vector<std::string>& owned);    // -1 if the area has nothing
bool WildAvailableNow(int species, int storyStage, int badges);
// HeartGold's radio and swarms (overworld.cpp): the next wild Pokemon is this species (-1: as the area has it);
// a species from another region's wild areas (Hoenn / Sinnoh Sound); Unown letters you haven't seen come more often.
void SetWildOverride(int species);
int RandomRegionWild(int region);
void SetWildUnseenForms(const std::vector<int>& forms);   // catchable (not rare-only) somewhere already reached
