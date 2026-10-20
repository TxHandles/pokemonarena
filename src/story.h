#pragma once
#include <set>
#include <string>
#include <vector>
#include "battle.h"

struct StageDef {
    std::string name;               // place, e.g. "Route 1"
    std::string trainer;            // e.g. "Youngster Ty"
    std::string say;                // line shown before the battle
    std::vector<std::string> team;  // species names, "random", or "random <Type>"
    int levelOffset = 0;            // relative to your team's average level (only without baseLevel)
    int baseLevel = 0;              // "lv": the level the battle is built for (your level nudges it a little)
    AILevel cpu = AILevel::Normal;
    bool reward = true;             // win: choose a new creature
    ArenaKind arena = ArenaKind::Stadium;
    std::string badge;              // set = this is a gym
    int catchMode = -1;             // -1 default (trainers yes, gyms no), 0 no, 1 yes
    int coins = -1;                 // -1 = worked out from the level
    float mapX = -1, mapY = -1;     // position on the story map image (pixels)
    int snorlax = 0;                // 1/2: a sleeping Snorlax encounter (beat it and it joins you)
    bool rival = false;             // a rival battle (team uses the rival tokens)
    bool outdoor = false;           // gyms are indoors (no weather, lit at night) unless this is set
    std::string venue;              // how a non-gym battle looks ("forest", "cave"...); empty = from the place name
    int league = 0;                 // Pokemon League: 1-4 the Elite Four (in order), 5 the Champion
    int towerFloor = 0;             // > 0: a Battle Tower floor (made by TowerStage, not story.txt)
    int legend = 0;                 // > 0: legendary encounter id+1 (made by LegendStage)
    int region = 0;                 // which region's story it belongs to (0 Kanto, 1 Johto...)
    bool join = false;              // a wild boss (Sudowoodo, the Red Gyarados): beat it and it joins you
    bool dbl = false;               // a double battle: the foes fight two at a time (alone, a CPU teammate fights beside you)
    std::string partner;            // a double battle where this trainer fights beside you (alone): "Lance"
    std::vector<std::string> partnerTeam;   // the partner's Pokemon (same format as team)
    int floor = 0;                  // > 0: the level the stage is built from never drops below this (a low-level tag-along can't drag it down)
    bool postgame = false;          // optional: the story moves on past it; it's fought from the Map (Red, Steven)
    std::string story;              // narration shown with the intro ("Team Magma has stolen the Meteorite...")
    std::string card;               // "Title | text": a story card shown the first time this battle comes up
    std::string cardAfter;          // "Title | text": a story card after the first win
    std::string cardMon;            // a Pokemon pictured on those cards (Groudon...)
    std::string gift;               // a Pokemon everyone gets after the first win (Castform, Beldum)
    bool IsLegend() const { return legend > 0; }
    bool IsGym() const { return !badge.empty(); }
    bool IsLeague() const { return league > 0; }
    bool IsChampion() const { return league >= 5; }
    bool IsBoss() const { return IsGym() || IsLeague(); }   // can be rematched from the Map
    bool IsTowerBoss() const { return towerFloor > 0 && towerFloor % 10 == 0; }
    bool BigIntro() const { return IsBoss() || IsTowerBoss() || (postgame && arena != ArenaKind::Stadium); }   // the packed-stadium intro
    bool Indoors() const { return (IsGym() || IsLeague() || arena != ArenaKind::Stadium) && !outdoor; }
    // Trainers' Pokemon are never catchable (you can't steal them): new Pokemon come from the wild.
    bool Catchable() const { return false; }
};

// A gym rematch: the same team, scaled to the challenger's level (the ace a few levels above
// their average), with the leader at full strength (Hard).
StageDef RematchStage(const StageDef& stage);

// The Pokemon League: the first Elite Four stage (where a lost run starts over), -1 if none.
int LeagueStartStage(int region = 0);
// Battle Tower: the trainer on a floor. Same floor and seed = the same trainer (both online
// players make it themselves). coop: two players (more foes).
StageDef TowerStage(int floor, uint32_t seed);
int TowerTeamSize(int floor);   // how many Pokemon each side brings on a floor

// Coins for a win at an average CPU level.
int WinCoins(const StageDef* stage, int cpuLevel);

struct StoryDef {
    std::vector<std::string> starters;
    int startLevel = 5;
    int rareEvery = 5;              // every Nth training battle is a rare creature
    int trainingMaxFoes = 3;        // most opponents in a normal training battle
    std::string mapImage;           // file next to story.txt (empty = drawn map)
    struct Town { std::string name; float x = 0, y = 0; };
    std::vector<Town> towns;
    std::vector<StageDef> stages;
    int rev = 1;                    // layout version (story_rev); old saves are moved to the new layout
    // Regions: Kanto first, then each [region] in story.txt (Johto). A region's stages are
    // stages[first, end); its map, towns and the level of the starter given when you arrive.
    struct Region {
        std::string name, mapImage;
        std::vector<Town> towns;
        int first = 0, end = 0;
        int startLevel = 5;
        int startAvg = 0;           // != 0: the starter gift's level follows your team (average + this), at least startLevel
        std::string rivalName;      // "Silver", "May"
        std::string intro;          // the starter screen's welcome ("Professor Elm in New Bark Town has...")
        std::string watch;          // the line about the rival before they choose
        std::string watchCoop;      // the same in co-op (%s = how many they choose from)
        std::string reveal;         // what happens when the rival takes theirs (%s = their Pokemon)
        std::string revealQuote;    // and what the rival says
        int dexLo = 0, dexHi = 0;   // its starters: the "starter = yes" Pokemon with Pokedex numbers in this range
        int bandLo = 5, bandHi = 60;   // the levels its story usually runs through (wild areas and legends follow them)
    };
    std::vector<Region> regions;
};

int StageRegion(int stage);           // the region a stage index belongs to (past the end: the last region)
int RegionCount();
const StoryDef::Region& RegionInfo(int region);

// Free region order. Every save plays the regions in its own order (you can start anywhere, and
// pick the next one when a region's Champion falls). The story's stages are laid out in that
// order - the regions not chosen yet come after, in their usual order - so a stage index is a
// place on that save's road. A region's levels follow its place in the order, not the region:
// whichever comes first plays like Kanto ("Map 1": a level 5 starter), the second like Johto, and
// so on (its trainers, starter gift, wild Pokemon and legendaries).
void ApplyRegionOrder(const std::vector<int>& order);   // empty = the usual order
const std::vector<int>& RegionOrder();       // the chosen regions, in order (as applied)
int RegionSlot(int region);                  // its place in the order: 0 = Map 1 (-1: not chosen yet)
bool RegionChosen(int region);
int ActiveRegion(int storyStage);            // the region the story is in (just after a Champion: that region)
bool BetweenRegions(int storyStage);         // a region is finished and the next one isn't chosen yet
int ProgressStageFor(int storyStage);        // for "has the story reached X": between regions, the last one's end
int SlotGiftLevelBase(int slot);             // the level a starter given at that place in the order starts at (alone: Map 1 = 5)
std::vector<int> RegionStarters(int region); // that region's starters (species indexes, Pokedex order)
int StageIndexByName(const std::string& name);   // a stage by name in the current layout (the first region that has it, -1 none)
int StageNameRegion(const std::string& name);    // which region a stage name belongs to (-1 none)
int SlotLevel(int region, int level);
int LevelTemplateRegion(int region);         // whose levels that region plays at (itself in the usual order)        // a level from that region's usual band, moved to the band of its place in the order
// An old save (no order saved): the regions it has reached, in the usual order.
std::vector<int> DeriveRegionOrder(int storyStage, const std::vector<bool>& hasRegionStarter);

// Rival: whose team "rival" tokens build. Set before MakeCpuTeam for a rival stage.
// rivalStarter = species index of his starter; coop = he also has his Pikachu.
// The rival's starters for the next team that's built: "rival" = main, "rival_partner" = second
// (co-op only; -1 when playing alone).
void SetRivalContext(int main, int second);
// Picked once per save: alone, one of the starters that beat yours (at random); in co-op, two of
// the starters neither player took (at random). Returns species indexes.
int PickRivalSolo(int mine, int region = 0);
void PickRivalCoop(int a, int b, int out[2], int region = 0);
int StoryStageForRev(int stage, int fromRev);                   // an old save's stage index in the current layout
// Post-game battles (postgame = yes) sit off the main road: the story's next stage skips them.
// oldSave: a save from before they were optional - the ones it had passed were beaten.
void FixPostgame(int& stage, std::set<std::string>& done, bool oldSave);
int NextStoryStage(int stage);   // after winning `stage`: the next one on the main road

// Story time. Every battle (or a Wait) moves the clock on one step: day and night alternate,
// and every 4 steps the season changes, Spring -> Summer -> Fall -> Winter.
inline bool IsNight(int clock) { return (clock & 1) != 0; }
inline int SeasonOf(int clock) { return (clock / 4) % 4; }
inline int StepsToNextSeason(int clock) { return 4 - clock % 4; }
const char* SeasonName(int season);
Weather RollSeasonWeather(int season);   // the day's weather for a battle (often none)

// A rare species for a training encounter (prefers ones the player doesn't own).
int PickRareSpecies(const std::vector<std::string>& ownedSpecies, int teamLevel = 100);
constexpr int LEGENDARY_LEVEL = 40;   // legendary rare encounters need a team this strong

const StoryDef& Story();
void LoadStory(std::string& log);   // story.txt next to the game, or the built-in ladder

// Build the CPU team for a stage (as written) or a random training team of teamSize if stage is null.
std::vector<CombatantSpec> MakeCpuTeam(const StageDef* stage, int playerAvgLevel, int teamSize);
// Random base-form species the profile doesn't own yet (up to n).
std::vector<int> RewardChoices(const std::vector<std::string>& ownedSpecies, int n, int storyStage = -1, int badges = 99);   // storyStage >= 0: only Pokemon that live in places reached
