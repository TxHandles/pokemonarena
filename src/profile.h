#pragma once
#include <string>
#include <map>
#include <set>
#include <vector>
#include "battle.h"

// A creature in a player's collection.
struct OwnedMon {
    std::string species;               // species name (survives the creatures folder being reordered)
    int level = 5;
    int xp = 0;
    std::vector<std::string> moves;    // up to 4 move names
    int battles = 0, wins = 0, kos = 0;
    float hp = 1.0f;                   // HP left (fraction); 0 = fainted, needs the Healing Center
    bool caught = false;               // caught from a trainer or wild battle
    std::string item;                  // held item (one per creature)
    bool boxed = false;                // stored in the Box instead of the party
    int friendship = START_FRIENDSHIP; // 0-255: wins raise it, fainting lowers it; some evolve when it's high
    int abilitySlot = -1;              // 0/1 = regular ability, 2 = hidden ability; -1 = not rolled yet
    std::string ot;                    // original trainer when received in a trade (empty = your own)
    int nature = -1;                   // natures.h (-1 = not rolled yet)
    std::vector<std::string> forgotten;   // moves it has forgotten (the Move Relearner can teach them again)
    bool shiny = false;                // a rare shiny (sprites in its shiny colours)
};

struct Profile {
    std::string name;
    int storyStage = 0;                // next story battle to fight
    int wins = 0, losses = 0;
    int trainingWins = 0;
    int coins = 100;
    std::vector<std::string> badges;   // e.g. "Boulder"
    std::map<std::string, int> bag;    // Poke Balls and unequipped held items
    std::vector<OwnedMon> mons;
    std::set<std::string> dexSeen, dexCaught;   // Pokedex (species names); creatures you own count as caught
    std::string starter;               // the starter species chosen (the rival picks against it)
    int storyRev = 0;                  // story layout this save's stage index belongs to
    int clock = 0;                     // Story time: +1 per battle or wait. Odd = night; season = (clock / 4) % 4
    std::string trainArea;             // where training battles happen (a wild.txt area; empty = the newest)
    std::string fossil;                // fossil chosen after Mt. Moon ("Helix Fossil" / "Dome Fossil"), empty = not yet
    bool fossilRevived = false;        // the Cinnabar Lab brought it back
    bool eeveeGift = false;            // got the Eevee for maxing a Pokemon's friendship
    bool amber = false;                // got the Old Amber at the Pewter Museum (after Brock)
    bool amberRevived = false;         // the Cinnabar Lab brought back its Aerodactyl
    std::string dojo;                  // Fighting Dojo prize taken ("Hitmonlee" / "Hitmonchan"), empty = not yet
    int snorlax = 0;                   // bit k = caught the Snorlax of encounter k (co-op: one each)
    int casino = 0;                    // Celadon Game Corner coins
    int boxSort = 0;                   // Box order: 0 caught, 1 level, 2 dex number, 3 name, 4 type, 5 newest
    std::string boxFilter;             // Box filter: "" all, "type:Fire", "evolve", "item", "hurt"
    int towerFloor = 0;                // Battle Tower: floors cleared in the current run (next = +1)
    int towerBest = 0;                 // the highest floor ever cleared
    int towerRuns = 0;                 // runs finished (a lost floor ends a run)
    std::vector<std::string> hof;      // Hall of Fame entries, oldest first: "partner|Species:Lv,Species:Lv,..."
    int legends = 0;                   // bit k: legendary k (legend.h) was caught - its encounter is closed
    std::string rival;                 // the rival's starter (picked once: one of the two that beat yours)
    std::string starter2;              // Johto: the starter Professor Elm gave you (empty = not in Johto yet)
    std::string rival2;                // Johto: Silver's starter (picked once, like Blue's)
    std::string starter3;              // Hoenn: the starter Professor Birch gave you
    std::string rival3;                // Hoenn: May's starter
    std::set<std::string> postDone;    // optional post-game battles beaten (their stage names: "Mt. Silver")
    std::vector<int> regionOrder;      // the regions in the order this story plays them (empty: a new story, not chosen yet)
};
// A region's starter gift and its rival's pick (region 1 Johto, 2 Hoenn; 0 = the first starter / Blue).
std::string& RegionStarter(Profile& p, int region);
const std::string& RegionStarter(const Profile& p, int region);
std::string& RegionRival(Profile& p, int region);
const std::string& RegionRival(const Profile& p, int region);
void DexSee(Profile& p, const std::string& species);
bool DexHasSeen(const Profile& p, const std::string& species);
bool DexHasCaught(const Profile& p, const std::string& species);
void DexRegisterOwned(Profile& p);    // everything in the collection counts as seen and caught
int RollAbilitySlot(int speciesIndex, bool allowHidden = true);   // 1 in 20 hidden
std::string MonAbility(const OwnedMon& m);

constexpr int START_COINS = 100;
constexpr int PARTY_SIZE = 6;          // creatures you carry; the rest wait in the Box
int PartyCount(const Profile& p);
bool AddMon(Profile& p, OwnedMon m);   // adds to the party, or the Box if the party is full; true = went to the Box
void FixParty(Profile& p);             // at most PARTY_SIZE in the party, and at least one if there are any

// Co-op Story: the progress two profiles share. Each player's creatures, coins and items
// stay in their own profile.
struct CoopSave {
    std::string p[2];                  // the two profile names
    int storyStage = 0;
    int wins = 0, losses = 0;
    int trainingWins = 0;
    std::vector<std::string> badges;
    int storyRev = 0;
    int clock = 0;                     // shared time of day / season
    std::string trainArea;             // where co-op training battles happen
    struct RunStats { int kos = 0, damage = 0, caught = 0, mvp = 0, faints = 0, battles = 0; };
    RunStats run[2];                   // each player's record over the whole co-op run (hub leaderboard)
    int snorlaxWho[2] = {-1, -1};      // who got the Snorlax of each encounter (-1 = nobody yet)
    int dojoFirst = -1;                // who picks first at the Fighting Dojo (the MVP of that battle)
    int towerFloor = 0, towerBest = 0, towerRuns = 0;   // the Battle Tower, climbed together
    int legends = 0;                   // bit k: legendary k was caught (by legendWho[k])
    std::string rival[2];              // the rival's two starters (picked once, from the four you didn't take)
    int legendWho[32] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
                         -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};   // by legend id (legend.h)
    std::string rival2[2];             // Johto: Silver's two starters (from the four the players didn't take there)
    std::string rival3[2];             // Hoenn: May's two
    std::set<std::string> postDone;    // optional post-game battles beaten together
    std::vector<int> regionOrder;      // the regions in the order this journey plays them (empty: not chosen yet)
};
std::string* CoopRival(CoopSave& c, int region);   // the rival's two picks for a region
const std::string* CoopRival(const CoopSave& c, int region);
std::string JoinOrder(const std::vector<int>& v);   // "2, 0"
std::vector<int> ReadOrder(const std::string& v);
bool LoadCoop(const std::string& a, const std::string& b, CoopSave& out);   // false = none yet (out is fresh)
bool SaveCoop(const CoopSave& c);
int HealCost(const OwnedMon& m);       // coins to fully heal (revive costs more)
bool HasBadge(const Profile& p, const std::string& badge);
int BagCount(const Profile& p, const std::string& item);
void BagAdd(Profile& p, const std::string& item, int n = 1);
bool BagTake(Profile& p, const std::string& item);   // false if none left
constexpr int START_BALLS = 5;

std::string SavesDir();
std::vector<std::string> ListProfiles();
bool LoadProfile(const std::string& name, Profile& out, std::string* problems = nullptr);
bool SaveProfile(const Profile& p);
std::string ProfileToText(const Profile& p);   // the save file's contents (also sent to an online partner)
bool ProfileFromText(const std::string& text, const std::string& name, Profile& out, std::string* problems = nullptr);
bool DeleteProfile(const std::string& name);
bool ProfileExists(const std::string& name);

OwnedMon NewMon(int speciesIndex, int level);
constexpr int SHINY_ODDS = 512;           // 1 in this many new Pokemon are shiny (wild ones, gifts, starters)
bool RollShiny();
int MonSpecies(const OwnedMon& m);                 // registry index, -1 if that species is gone
std::vector<MoveDef> MonMoves(const OwnedMon& m);  // resolved MoveDefs (unknown names skipped)
CombatantSpec MonCombatant(const Profile& p, int monIndex);
int TeamAverageLevel(const Profile& p, const std::vector<int>& monIndexes);

// XP gained outside a battle (win bonus). Fills in what happened.
struct Growth {
    int levelsGained = 0;
    std::vector<std::string> learned;   // learned into an empty slot
    std::vector<std::string> wantsToLearn;  // needs a slot: ask the player
    int evolveTo = -1;                  // species index it should now evolve into
};
void AddXp(OwnedMon& m, int xp, Growth& g);
// Apply an evolution (after the animation), then any moves the new form learns at this level.
void Evolve(OwnedMon& m, int toSpecies, Growth& g);
// Moves a creature learns at a level that it doesn't know yet.
void CollectNewMoves(OwnedMon& m, int speciesIndex, int level, Growth& g, bool evolving = false);
