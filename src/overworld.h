#pragma once
// The overworld (test build): walk around Pallet Town, Route 1 and Viridian City, and reach the
// story's battles, wild Pokemon, the Pokemon Center and the Mart on foot. The maps, their walls and
// what's in them come from World/world.txt; the pictures from World/*.png.
//
// Solo: one walker. Local co-op: two walkers on a split screen. Online co-op: you walk on your own
// screen and your partner shows up wherever they are (their game sends where they walk). Story and
// gym battles are always fought together; wild Pokemon in the grass are fought alone.
#include <memory>
#include "moves.h"
#include <array>
#include <string>
#include <vector>

struct Game;
class Battle;

// One person walking around.
struct OwWalker {
    int map = -1;
    int x = 0, y = 0, face = 0;   // tile, facing (0 down, 1 up, 2 left, 3 right)
    int fromX = 0, fromY = 0;     // a step under way: from here to (x, y)
    float stepT = 1;              // 0..1 through the step (1 = standing)
    bool hop = false;             // hopping down a ledge (two tiles)
    bool running = false;
    int fx = 0, fy = 0, fFromX = 0, fFromY = 0, fFace = 0;   // their lead Pokemon, a step behind
    int walkAnim = 0;             // which foot
    int stepsSinceWild = 0;
    float bumpT = 0;              // walked into a wall (the thud's cooldown)
    std::vector<std::string> msg; // a message box, one page per entry
    int msgPage = 0;
    std::string after;            // what happens when the message closes
    int approach = -1;            // a trainer who spotted them is walking over (npc index)
    float approachT = 0;
    float bannerT = 0;            // the map's name, shown on arrival
    float fadeT = 0;              // a door: the view fades
    bool here = false;            // online partner: walking around right now
    bool surfing = false;         // riding a Pokemon across the water (Surf)
    bool biking = false;          // on the Bicycle
    bool fishing = false;         // the rod is out (while the fishing message shows)
    float fishT = 0;
    int vsCharge = 100;           // the VS Seeker's battery: steps since it was last used (100 = charged)
    int flashMap = -1;            // Flash lit up this (dark) map
    int strengthMap = -1;         // Strength is on for this map: boulders can be pushed
    int spin = -1;                // on spin tiles: sliding this way until a stop tile
    std::vector<std::pair<std::string, std::string>> choices;   // the message's last page asks: {label, action}
    int choiceSel = 0;
};

struct OwState {
    bool active = false;          // the World screen is the story's home (Esc goes to the menu)
    std::vector<std::array<int, 3>> ghostQ;   // online: your partner's steps still to walk (x, y, face + running/bike/surf bits)
    bool wildBattle = false;      // the next training battle came from tall grass (one wild Pokemon)
    std::string wildArea;         // ...and the route's own wild Pokemon (wild.txt), however far the story is
    int staticLevel = 0;          // a Pokemon met on the map (Snorlax, the Power Plant's Electrode...): its level
    std::string staticFlag;       // ...and the flag set once it's beaten or caught (it's gone)
    std::string wildMethod = "walk";   // ...found how: walk (grass), surf, old-rod / good-rod / super-rod, rock-smash
    OwWalker w[2];                // [0] you; [1] local co-op's player 2, or your online partner
    // Co-op: a player's own wild battle, Healing Center or Mart runs as a little solo game of its own,
    // shown in their half of the screen (online: the whole screen) while the world goes on.
    std::shared_ptr<Game> sub[2];
    int inputPlayer = -1;         // (in such a game) whose keys it reads: local co-op's player 1 or 2
    int lastScreen = -1;          // (in such a game) to reset its screen timer on a change
    bool waitFight = false;       // online: a story battle - waiting for the partner to come over
    std::string waitName;         // the trainer
    // online: your partner was challenged and you're on your way
    bool incoming = false;
    int inStage = -1, inMap = -1, inX = 0, inY = 0;
    std::string inTrainer;
    // online: watching your partner's own battle (stand next to them, press Enter)
    bool watching = false;
    std::shared_ptr<Battle> watchBattle;   // their battle, replayed here from what their game sends
    bool watchers = false;        // your partner is watching yours
    bool watchSent = false;       // they have your battle's starting state
    float watchResync = 0;
    bool teamUp = false;          // co-op: teamed up - tall grass is a 2v2 wild battle for both
    bool menu = false;            // the pause menu is open (Pokedex, Pokemon, Bag, Map, Save, Settings)
    int menuSel = 0, menuWho = 0; // ...the row, and which walker opened it
    bool townMap = false;         // the Town Map (and Fly) is open
    int townSel = 0;
    bool hubOnly = false;         // (autotests) stepped out to the Story menu: don't send them straight back out
    float clockAcc = 0;           // the walked day: seconds toward the next clock second
};

bool WorldAvailable();                    // World/world.txt loaded
bool StageWalked(int stage);
bool StageSkipped(int stage);              // a story stage the walked map has no place for (it counts as done)               // this story stage is met on the walked map (its region is walked): it can be won out of order
bool WorldRequired(const Game& g);
extern bool gWorldHome;                    // the walked region has no Story menu (autotests turn it on themselves)
bool InWorldMode(const Game& g);           // the story lives in the world right now (walked Kanto, its post-game, a visit): no Story menu
bool WalkedOpening(int region);
bool WorldStartOf(int region, std::string& map, int& x, int& y);   // a walked region's start: your room there            // that region's story starts in your room (no starter screen: Oak's lab)
bool CanVisitWorld(const Game& g);         // the Story menu can travel back to the walked region (its League is done)
void VisitWorld(Game& g);                  // ...and does
Weather WorldBattleWeather(const Game& g, int clock);   // a battle out in the world: the map's weather, or the day's        // this part of the story is walked (world.txt "walked = Kanto"): the hub sends you out into the world
void LoadWorld(std::string& log);         // after InitWindow
void EnterWorld(Game& g);                 // from the menu: where you left off (or your room)
void UpdateWorld(Game& g, float dt);
void DrawWorld(const Game& g);
void WorldPreDraw(const Game& g);           // before the frame: the halves' own games, into their pictures
std::string WorldQuestInfo(const Game& g);   // (autotests) where the quest marker points
std::string WorldPlaceName(const Game& g);   // where you'll be (for the menu)
// Online: the partner walked / was challenged (from the network).
void WorldPartnerPos(Game& g, int map, int x, int y, int face);
void WorldPartnerFight(Game& g, int stage, int map, int x, int y, const std::string& trainer);
// Online spectating (from the network): they want to watch / their battle's state / one frame of it / it's over.
void WorldWatchRequest(Game& g, bool on);
void WorldWatchState(Game& g, const std::string& state);
void WorldWatchStep(Game& g, float dt, const struct PlayerInput in[2]);
void WorldWatchEnd(Game& g);
void WorldTeamMsg(Game& g, int what);
std::string WorldUseField(Game& g, const std::string& item);   // the Bag's Repels and Escape Rope (why not, or "")
std::string WorldToggleBike(Game& g);     // the Bag's Bicycle: on / off (a message why not, or "")
std::string WorldVsSeeker(Game& g);       // the Bag's VS Seeker: Trainers nearby who want a rematch (a message why not, or "")
std::string WorldMatchCall(Game& g);     // the Pokenav's Match Call: registered Trainers, and who wants a rematch
void WorldMapState(Game& g, int map, const std::string& state);   // online: your partner changed this map (Cut, Strength...)
void WorldXpShare(Game& g, int xp);       // online: your partner's Exp. All shared this much XP with your party   // online team-up: 1 asked, 2 yes, 3 no, 0 split up
