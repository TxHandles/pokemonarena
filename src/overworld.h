#pragma once
// The overworld (test build): walk around Pallet Town, Route 1 and Viridian City, and reach the
// story's battles, wild Pokemon, the Pokemon Center and the Mart on foot. The maps, their walls and
// what's in them come from World/world.txt; the pictures from World/*.png.
//
// Solo: one walker. Local co-op: two walkers on a split screen. Online co-op: you walk on your own
// screen and your partner shows up wherever they are (their game sends where they walk). Story and
// gym battles are always fought together; wild Pokemon in the grass are fought alone.
#include <memory>
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
};

struct OwState {
    bool active = false;          // the World screen is the story's home (Esc goes to the menu)
    bool wildBattle = false;      // the next training battle came from tall grass (one wild Pokemon)
    std::string wildArea;         // ...and the route's own wild Pokemon (wild.txt), however far the story is
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
};

bool WorldAvailable();                    // World/world.txt loaded
void LoadWorld(std::string& log);         // after InitWindow
void EnterWorld(Game& g);                 // from the menu: where you left off (or your room)
void UpdateWorld(Game& g, float dt);
void DrawWorld(const Game& g);
void WorldPreDraw(const Game& g);           // before the frame: the halves' own games, into their pictures
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
void WorldXpShare(Game& g, int xp);       // online: your partner's Exp. All shared this much XP with your party   // online team-up: 1 asked, 2 yes, 3 no, 0 split up
