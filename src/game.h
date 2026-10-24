#pragma once
#include <string>
#include <vector>
#include "battle.h"
#include "input.h"
#include "profile.h"
#include "story.h"
#include "overworld.h"
#include "contest.h"
#include <deque>
#include <map>

enum class Screen {
    Title, Settings, Controls,
    ProfileSelect, NameEntry, Starter, StoryHub, MyCreatures, StageIntro,
    RosterPick, Select, Battle, Results,
    StoryResults, Evolve, LearnMove, Reward, Catch, Heal, Map, Shop, DoublesMenu,
    CoopSelect, CoopHub, CoopCatch, NetMenu, Pokedex, Trade, Fossil, Gift, Revive, Slots, CoopSetup, CoopStarter, BattleStats, Practice, HallOfFame, Legends, LegendCatch, Bag,
    RegionChoose, World, Creator, Nickname, Hatch, Daycare, Contest, Safari, UnownPuzzle, VoltorbFlip
};

// Online ready kinds (what a player picked in the co-op hub): 1 story, 2 training, 3+N a rematch
// with story stage N, KIND_LEGEND+id a legendary, KIND_TOWER the Battle Tower.
constexpr int KIND_LEGEND = 1000;
constexpr int KIND_TOWER = 2000;
constexpr int KIND_RAID = 3000;          // the Daily Raid (co-op)
inline bool IsLegendKind(int k) { return k >= KIND_LEGEND && k < KIND_TOWER; }
inline bool IsRematchKind(int k) { return k >= 3 && k < KIND_LEGEND; }

enum class Mode { Quick, TwoPlayer, Watch, Story, Training, Doubles, CoopStory, CoopTraining };

// Something a player can pick in team select.
struct RosterEntry {
    int species = 0;
    int level = FREE_PLAY_LEVEL;
    int xp = 0;
    std::vector<MoveDef> moves;
    int ownedId = -1;   // index in that player's profile, -1 = free play
    float hpFrac = 1;   // Story Mode: HP carried over (0 = fainted)
    std::string item;   // held item
    int abilitySlot = 0;
    int nature = 0;
    bool shiny = false;
    int iv[6] = {-1, -1, -1, -1, -1, -1};   // a Pokemon of yours: its IVs and EVs (-1: free play rolls them)
    int ev[6] = {0, 0, 0, 0, 0, 0};
    std::string nick;
};

struct ResultRow {
    int mon = -1;
    std::string name;
    int levelFrom = 0, levelTo = 0, xp = 0;
    std::string note;
    int who = 0;        // co-op: which player owns it
};

// Online co-op (screens_online.cpp). Each player runs their own game with their own menus; the
// battle runs on both computers in step with rollback (see OnlineBattleUpdate).
struct OnlineSession {
    bool active = false;          // in an online co-op session (connected, past the hello)
    bool reconnecting = false;    // the connection dropped: the session waits for the partner to come back
    float reconnectT = 0, retryAt = 0;
    bool resend = false;          // after a reconnect: tell the partner where we are again
    int me = 0;                   // 0 = host, 1 = guest
    // Setup screen
    int page = 0;                 // 0 choose, 1 hosting, 2 joining
    int sel = 0, profSel = 0;
    std::string field;            // typed address / code
    bool codeEdit = false;        // reconnecting: typing the friend's new code
    std::string newCode;
    std::string err;
    bool helloSent = false, gotHello = false, gotCoop = false;
    Profile mine;                 // my profile while setting up
    // Hub
    int myReady = 0, partnerReady = 0;   // 0 none, 1 story battle, 2 training
    std::string partnerActivity;
    int partnerCursor = -1;              // which hub menu item the partner has highlighted
    // Team select
    bool picksSent = false, partnerPicksIn = false;
    std::vector<int> partnerPicks;
    std::vector<int> partnerPickView;    // the partner's picks so far (shown while they choose)
    int partnerSelCursor = -1;           // the partner's highlighted creature in team select
    // Battle
    bool inBattle = false;
    int tick = 0, confirmed = 0;
    Battle confirmedState;
    std::map<int, PlayerInput> inputs[2];
    std::map<int, PlayerInput> predicted;   // what we guessed for the partner, by tick
    PlayerInput lastRemote, pending;
    float acc = 0;
    bool resim = false;
    int firstWrong = -1;
    std::map<int, uint64_t> myHash, theirHash;
    int desyncs = 0, rollbacks = 0;
    bool leaveAsk = false;
    float stallT = 0;
    // Catch draft: picks the partner made, waiting for our catch screen
    std::deque<int> partnerCatch;
    // Trading
    bool partnerInTrade = false, partnerTradeReady = false;
    int partnerOffer = -1;
    std::string partnerOfferMon;      // the offered creature, as save-file text
    std::string partnerFossil;        // the fossil the partner picked first (co-op fossil choice)
    // Watching the partner's catch: their highlighted creature and their throws
    int partnerCatchSel = 0;
    struct Throw { int species, level; bool ok; int shakes; bool fled; };
    std::deque<Throw> partnerThrows;
};

struct Game {
    Screen screen = Screen::Title;
    Mode mode = Mode::Quick;
    float t = 0;
    bool quit = false;
    int customCount = 0;
    std::string toast;
    float toastT = 0;

    // Menus
    int titleSel = 0, settingsSel = 0, pauseSel = 0, resultSel = 0, hubSel = 0;
    int titleMenu = 0;                      // title: 0 main, 1 Co-op, 2 Quick Battle
    float screenT = 0;            // seconds since the screen last changed (menu animations)
    int bagTab = 0, bagSel = 0;             // the Bag: pocket and item
    bool raid = false;           // the Daily Raid battle (your whole party at once vs one boss)
    bool owFight = false;        // a route trainer met on the map (not a story battle): owStage, and the flag a win sets
    StageDef owStage;
    std::string owFightFlag;
    std::vector<CombatantSpec> raidTeam1;   // solo raid: Team 1, picked first (the team screen then picks Team 2)
    std::vector<int> raidTaken;             // solo raid: roster entries already on Team 1
    std::string catchBall;     // the ball just thrown (its colours in the catch animation)
    float encounterT = 0;      // walking around: the wild-encounter wipe is playing, then the battle starts
    bool encounterBlank = false;   // (a co-op player's own little game: no map behind the wipe)
    bool bagAssign = false; int bagSlot = 0; // the Bag: putting a Medicine on an item hotkey (which slot)
    bool areaPick = false;                  // training team select: the "where to train" menu is open
    float swapHoldT = 0;                    // solo story battles: how long Swap has been held
    bool wheel = false, wheelRelease = false;   // the Quick Select wheel is open / wait for Swap to be let go
    int wheelSel = -1;
    int pendingSwap = -1;                   // a chosen swap that waits for the swap cooldown
    float pendingSwapT = 0;
    int areaSel = 0;
    int ctrlTab = 0, ctrlRow = 0, ctrlCol = 0;
    bool capturing = false;
    float captureT = 0;

    // Profiles
    std::vector<std::string> profiles;
    int profSel = 0;
    bool confirmDelete = false;
    int diffSel = 1;                        // the new profile's difficulty (Easy / Normal / Hard)
    bool quitAsk = false;                   // "Leave Story Mode?" (Yes / No) on the hub
    int quitSel = 1;
    std::string nameBuf;
    Profile profile;            // the Story Mode profile in use
    int starterSel = 0;
    int regionPick = 0;         // > 0: the starter screens are region (regionPick - 1)'s gift on arriving (Elm in Johto, Birch in Hoenn...)
    bool newJourney = false;    // that gift is a new story's first partner (Back: choose the region again)
    int journeyRegion = 0;      // a new co-op journey: the region it starts in (the starter screen's Pokemon)
    int regionSel = 0;          // the region screen's cursor
    int regionFor = 0;          // what it's choosing: 0 a new story's first region, 1 the next region, 2 a new co-op journey's, 3 co-op's next
    float regionRevealT = -1;   // >= 0: the rival has just taken their starters (shown, then the hub)
    std::string cardTitle, cardText, cardMon;   // a story card waiting to be read (cardTitle empty = none)
    int cardFor = -2;           // the stage whose pre-battle card was shown (-2 none)
    bool hubTower = false;      // hub: the first item shows the Battle Tower instead of the next story battle
    int myCreSel = 0;

    // Roster choice for Quick / 2-player (-1 = free play, else index into profiles)
    int rosterSrc[2] = {-1, -1};
    bool rosterReady[2] = {false, false};
    Profile rosterProfile[2];

    // Team select
    std::vector<RosterEntry> roster[2];
    bool shared = true;         // both players pick from the same grid at once
    int pickPhase = 0;          // when not shared: 0 = P1 picking, 1 = P2 picking
    int cursor[2] = {0, 0}, scroll[2] = {0, 0};
    std::vector<int> picks[2];
    int selBox[2] = {-1, -1};     // Story team select: the Box is open (cursor in it), -1 = closed
    int selBoxScroll[2] = {0, 0};
    bool ready[2] = {false, false};
    float startDelay = -1;
    int teamSize = 2;

    // Battle
    Battle battle;
    bool paused = false;

    // Story
    int stageIdx = -1;                      // -1 = training battle (or a Battle Tower floor)
    bool tower = false;                     // this battle is a Battle Tower floor (towerFloor)
    ContestState contest;                   // a Pokemon Contest (Lilycove's Contest Hall)
    struct SafariEnc {                      // a Safari Zone / Bug-Catching Contest encounter (screens_safari.cpp)
        int species = -1, level = 5, nature = -1;
        int iv[6] = {0, 0, 0, 0, 0, 0};
        bool shiny = false;
        float catchMul = 1, fleeMul = 1;    // bait / a rock
        int sel = 0, phase = 0;             // 0 choosing, 1 a ball flying and shaking, 2 over (caught / fled / out of balls)
        int shakes = 0;
        bool caught = false;
        float t = 0;
        std::string msg;
        int angry = 0, eating = 0;          // turns left (FireRed: a rock angers it, bait keeps it eating)
    } safariEnc;
    struct AlphPuzzle {                     // the Ruins of Alph's stone panel (screens_puzzle.cpp)
        std::string kind;                   // kabuto, omanyte, aerodactyl, hooh
        int board[16] = {};                 // which piece is in each square (solved: board[k] == k)
        int cur = 0, held = -1;             // the cursor, the square picked up (-1 none)
        bool solved = false;
        float t = 0;
        Texture2D tex{};
    } alph;
    int frontier = 0;                       // ...in a Battle Frontier facility (1 Tower .. 7 Pyramid, FrontierInfo): towerFloor is the battle of the streak
    int towerFloor = 0;
    uint32_t towerSeed = 0;
    int legend = -1;                        // this battle is a legendary encounter (legend.h), -1 = not
    int roamer = -1;                        // this wild battle is a roaming legendary (legend id), -1 = not
    bool coopSub = false;                   // a co-op player's own little game (the walked world): roamers come from the co-op save
    std::map<int, Profile::Roam> subRoamKeep;   // (that player's own solo roamers, put back when it ends)
    int legendSel = 0;                      // the Legendary Pokemon screen's cursor
    bool legendCatchPending = false;        // a legendary was weakened: its catch comes next
    struct LegendCatch {
        int id = -1, species = -1, level = 1, abSlot = -1, nature = 0;
        bool shiny = false;
        int iv[6] = {-1, -1, -1, -1, -1, -1};
        std::vector<std::string> moves;
        float hpFrac = 0.1f, chance = 0.3f;
        int order[2] = {0, 1};              // who throws first (the MVP) and second
        int players = 1;
        int throwNo = 0, maxThrows = 3;
        int phase = 0;                      // 0 pick a ball, 1 the throw, 2 its result, 3 the end (caught / fled)
        float t = 0;
        bool ok = false, passed = false, boxed = false;
        int shakes = 0, ballSel = 0, caughtBy = -1;
        int Thrower() const { return order[throwNo % players]; }
    } lc;
    bool hofPending = false;                // just became Champion: show the Hall of Fame
    int hofSel = 0;                         // which Hall of Fame entry is shown (0 = newest)
    float hofT = 0;
    Screen hofBack = Screen::StoryHub;
    std::vector<CombatantSpec> cpuTeam;
    std::vector<ResultRow> resultRows;
    std::vector<std::string> resultLines;
    bool won = false;
    struct Learn { int mon; std::string move; int who = 0; int src = 0; std::string item; };   // src: 0 level up, 1 TM (item), 2 Move Relearner
    std::vector<Learn> learnQueue;
    int learnSel = 0;
    struct Evo { int mon; int to; int who = 0; };
    std::vector<Evo> evoQueue;
    float evoT = 0;
    int rewardsPending = 0;
    std::vector<int> rewardWho;             // co-op: whose reward each pending one is
    std::vector<int> rewardChoices;
    int rewardSel = 0;
    // Catching after a win
    struct CatchCand { int species; int level; std::vector<std::string> moves; float hpFrac; float chance; int abSlot = 0; int nature = 0; bool shiny = false; };
    std::vector<CatchCand> catchCands;
    bool catchPending = false;
    int catchSel = 0, catchPhase = 0, catchShakes = 0;   // phase 0 choose, 1 throw/shake, 2 result
    float catchT = 0;
    bool catchOk = false;
    // Healing Center
    int doublesKind = 0;          // 2v2: 0 you + CPU vs 2 CPU, 1 P1 + P2 vs 2 CPU, 2 P1 + CPU vs P2 + CPU
    int doublesSel = 0;
    int healSel = 0;
    Screen healReturn = Screen::StoryHub;
    int ballSel = 0;              // which ball type to throw
    bool catchFled = false;
    // Catch draft (co-op): players take turns, each catching at most one, never the same creature
    std::vector<int> catchOrder;  // players in pick order
    int catchTurn = 0;
    int catchWho = 0;             // whose turn it is (whose balls and box)
    std::vector<CatchCand> catchPool;
    int catchPoolPick = -1;       // the pool entry this turn's player threw at
    bool catchBoxed = false;      // the last catch went to the Box
    // Training: every Nth battle is a rare creature
    bool rareBattle = false;
    int rareSpecies = -1;
    // Map, shop, held items
    int mapSel = 0;
    int shopSel = 0, shopTab = 0;
    bool itemPick = false;        // My Creatures: choosing a held item
    int itemSel = 0;
    int boxTab = 0;               // My Creatures: 0 party, 1 Box
    bool actMenu = false;         // My Creatures: action menu open
    int actSel = 0;
    int releaseAsk = -1, releaseSel = 0;   // Party & Box: releasing a Pokemon from the Box (index, No/Yes)
    int swapFrom = -1;            // My Creatures: Box creature waiting to swap into a full party
    bool stonePick = false;       // My Creatures: choosing an evolution stone
    bool returnToParty = false;   // after a stone evolution, go back to Party & Box
    bool returnToShop = false;    // after the Move Relearner, go back to the shop
    bool tmPick = false;          // Party & Box: choosing a TM to teach
    bool moveOrder = false;       // Party & Box: rearranging which button uses which move
    int moveOrdSel = 0, moveGrab = -1;
    bool showBoard = false;       // co-op hub: the leaderboard is open
    // Gifts (fossil revivals, Eevee): shown one at a time after a battle; the Pokemon is already added
    struct Gift { int who; int species; std::string text; std::string item; };   // item: an item gift (Old Amber) instead
    std::vector<Gift> giftQueue;
    // nicknames, Eggs hatching and the Day Care (screens_daycare.cpp)
    struct MonJob { int who = 0, mon = -1; };
    std::vector<MonJob> nickQueue;          // "Give a nickname to the caught X?"
    std::vector<MonJob> hatchQueue;         // Eggs that just hatched (the Pokemon each one became)
    int nickWho = 0, nickMon = -1, nickPhase = 0, nickSel = 0, nickGrid = 0;   // phase 0 "give it a nickname?", 1 typing
    std::string nickBuf;
    int nickReturn = 0;                     // where the nickname screen goes back to: 0 the story queues, 1 Party & Box, 2 the world
    float hatchT = 0;
    bool hatchFromWorld = false;
    int dcWho = 0, dcSel = 0, dcMode = 0, dcPickSel = 0;   // the Day Care screen: mode 0 the menu, 1 picking a Pokemon to leave
    Screen dcBack = Screen::StoryHub;
    bool judgeShow = false;                 // Party & Box: the Judge's look at a Pokemon's IVs and EVs
    // After-battle stats (damage dealt, damage taken, KOs) for everyone who fought
    struct StatRow { int team = 0, who = 0, species = -1, level = 1, dealt = 0, taken = 0, kos = 0; bool fainted = false; std::string tag; };
    std::vector<StatRow> statRows;
    int fossilSel = 0;            // pick screen: which of the two
    int pickKind = 0;             // pick screen: 0 Mt. Moon fossils, 1 Fighting Dojo prize
    float reviveT = -1;           // Cinnabar Lab: revival animation time (-1 = waiting for Enter)
    // Celadon Game Corner slots
    int slotBet = 1;              // coins per spin (1-3 lines)
    int slotPhase = 0;            // 0 idle, 1-3 = reels still spinning (next to stop), 4 = paying out
    float slotPos[3] = {0, 0, 0}; // reel positions (symbols)
    int slotStop[3] = {0, 0, 0};  // where each stopped reel will land
    bool slotStopping[3] = {false, false, false};
    int slotWin = 0;              // last payout
    float slotT = 0;              // payout flash timer
    std::vector<int> slotLines;   // the lines that won (0 middle, 1 top, 2 bottom, 3-4 diagonals)
    int relearnMon = -1;          // shop's Move Relearner: the creature chosen (-1 = choosing one)
    // Co-op Story
    bool coop = false;            // a co-op session is open
    CoopSave coopSave;
    Profile coopP[2];             // both players' profiles
    int coopActive = -1;          // which player's profile is loaded into 'profile' (Shop, Healing, Party & Box)
    int coopWho = 0;              // hub: whose Shop / Healing Center / Party & Box
    int coopPick[2] = {0, 0};     // co-op setup: profile index each player is on
    bool coopReady[2] = {false, false};
    bool coopNew[2] = {false, false};      // starting a brand-new trainer for this player
    std::string coopName[2];               // their names while typing
    int coopNameWho = 0;                   // whose name is being typed (local co-op)
    int coopStarterSel[2] = {0, 0};        // co-op starter picks
    int cr[7] = {0, 0, 0, 0, 0, 0, 0};    // the trainer creator: poses, head, hair colour, outfit, outfit colour, skin
    std::string bagSellArm;                // the Bag: a valuable you pressed once (press again to sell it)
    double bagSellT = 0;
    bool crReturn = false;                 // the creator was opened from the world's menu (back there after)
    int crSel = 0;                         // ... and the row picked (6 = Done)
    bool netNewTrainer = false;            // online menu: typing a new trainer's name
    Screen settingsBack = Screen::Title;   // where Settings returns to
    Screen practiceBack = Screen::StoryHub; // the hub the practice arena was opened from
    float practicePeak = 0;                 // practice: total damage dealt to the dummies
    OnlineSession on;
    OwState ow;                   // the overworld (overworld.cpp)
    // Pokedex
    int dexSel = 0, dexScroll = 0, dexWho = 0;
    bool dexWhere = false;        // Pokedex: the 'where to find it' page
    int dexWhereSel = 0;
    int dexMega = 0;          // Pokedex: 0 = the Pokemon, 1+ = showing its Mega form
    // Trading (co-op)
    int tradeCur[2] = {0, 0}, tradeScroll[2] = {0, 0}, tradeOffer[2] = {-1, -1};
    bool tradeReady[2] = {false, false};
    int tradePhase = 0;               // 0 choosing, 1 swapping, 2 done
    float tradeT = 0;
    std::string tradeSent[2], tradeGot[2];
    bool returnToTrade = false;
};

// Shared helpers (screens_common.cpp)
void Toast(Game& g, const std::string& msg);
void DrawToast(const Game& g);
void DrawMenuItem(const char* text, int cx, int y, int size, bool selected, bool dim = false);
void DrawControlsHelp(int x, int y, bool twoPlayer);
void DrawMoveLine(const MoveDef& m, int x, int y, int w, bool details);
void DrawCreatureCard(int species, int level, int x, int y, int w, int h, bool selected);
std::vector<RosterEntry> FreePlayRoster();
std::vector<RosterEntry> ProfileRoster(const Profile& p, bool partyOnly = false);

// Screens
void UpdateTitle(Game& g);           void DrawTitle(const Game& g);
void UpdateSettings(Game& g);        void DrawSettings(const Game& g);
void UpdateControls(Game& g);        void DrawControls(const Game& g);
void UpdateProfileSelect(Game& g);   void DrawProfileSelect(const Game& g);
void UpdateNameEntry(Game& g);       void DrawNameEntry(const Game& g);
void UpdateCreator(Game& g);         void DrawCreator(const Game& g);
void UpdateStarter(Game& g);         void DrawStarter(const Game& g);
void UpdateStoryHub(Game& g);        void DrawStoryHub(const Game& g);
void UpdateMyCreatures(Game& g);     void DrawMyCreatures(const Game& g);
void UpdateStageIntro(Game& g);      void DrawStageIntro(const Game& g);
void UpdateRosterPick(Game& g);      void DrawRosterPick(const Game& g);
void UpdateSelect(Game& g, float dt);void DrawSelect(const Game& g);
void UpdateBattleScreen(Game& g, float dt); void DrawBattleScreen(const Game& g);
void UpdateResults(Game& g);         void DrawResults(const Game& g);
void UpdateStoryResults(Game& g);    void DrawStoryResults(const Game& g);
void UpdateEvolve(Game& g, float dt);void DrawEvolve(const Game& g);
void UpdateLearnMove(Game& g);       void DrawLearnMove(const Game& g);
void UpdateFossil(Game& g);          void DrawFossil(const Game& g);
void UpdateGift(Game& g);            void DrawGift(const Game& g);
void UpdateRevive(Game& g);          void DrawRevive(const Game& g);
void UpdateCoopSetup(Game& g);       void DrawCoopSetup(const Game& g);
void UpdateBattleStats(Game& g);     void DrawBattleStats(const Game& g);
void UpdateCoopStarter(Game& g);     void DrawCoopStarter(const Game& g);
void CoopContinueSetup(Game& g);     // a new journey's region chosen: names, starters, then CoopBeginJourney
void CoopBeginJourney(Game& g);      // both profiles chosen and have a starter: open the co-op save, go to the hub
bool CoopNeedsStarter(const Game& g); // someone in the co-op session has no Pokemon yet
void OnlineSendCursor(int sel);
void UpdateSlots(Game& g);           void DrawSlots(const Game& g);
bool GameCornerOpen(const Game& g);                          // the story has reached Celadon
void UpdateGameCorner(Game& g, const struct PlayerInput& in);  // the shop's Game Corner tab
void DrawGameCorner(const Game& g);
int StoryNewMonLevel(const Profile& p);                      // level a new Pokemon joins at
void StorySaveProfile(Game& g);                              // save the active profile (toast on failure)
void OnlineSendFossil(const std::string& choice);
void UpdateReward(Game& g);          void DrawReward(const Game& g);
void UpdateCatch(Game& g, float dt); void DrawCatch(const Game& g);
void UpdateHeal(Game& g);            void DrawHeal(const Game& g);
void UpdateDoublesMenu(Game& g);     void DrawDoublesMenu(const Game& g);
bool TwoHumans(const Game& g);       // two people are playing (2 Players, co-op 2v2, co-op story)
bool IsCoop(Mode m);
void UpdateMap(Game& g);             void DrawMap(const Game& g);
void UpdateHallOfFame(Game& g);      void DrawHallOfFame(const Game& g);
void UpdateLegends(Game& g);         void DrawLegends(const Game& g);        // the Legendary Pokemon list
void UpdateLegendCatch(Game& g, float dt); void DrawLegendCatch(const Game& g);
void OpenLegends(Game& g);
bool MapTabSwitch(Game& g, bool toLegends);   // the Map's Route / Legendary Pokemon tabs
void DrawMapTabs(const Game& g, bool legends);
constexpr int HUB_GEAR = 100, HUB_GRAPH = 101;   // hub cursor on the Settings gear / Leaderboard graph icons
void OpenBag(Game& g);
void CoopOpenFor(Game& g, int who);   // co-op: that player's profile for the Bag / Party & Box (it goes back on EnterStoryHub)
void UpdateBag(Game& g);             void DrawBag(const Game& g);
struct ItemDef;
void DrawItemIcon(const ItemDef& d, float cx, float cy, float scale = 1.0f);
void BeginLegend(Game& g, int id);                    // set up the encounter (then its intro)
bool LegendOpen(const Game& g, int id, std::string* why = nullptr);   // unlocked and not caught yet
int LegendCaughtBy(const Game& g, int id);            // -1 not caught; else the player who has it (0 alone)
CombatantSpec LegendBoss(int id, int avgLevel, int brought, bool coop);   // the legendary as a boss for this battle
void LegendBattleOver(Game& g, const Battle& b, const float damage[2]);   // a won encounter: set up the catch
void DrawLegendIntro(const Game& g);
struct StageDef LegendStage(int id);           // a legendary encounter as a stage
struct StageDef ActiveStage(const Game& g);   // the trainer of the battle being set up (story stage, rematch or Tower floor)
bool StoryDone(const Game& g);                // every region's story is done (Johto too)
int CurStage(const Game& g);                  // the next story battle (solo or co-op)
bool RegionDone(const Game& g, int region);   // that region's Champion is beaten
bool KantoDone(const Game& g);                // the first region's Champion is beaten: the Battle Tower is open
int RegionBadgeCount(const Game& g, int region);   // that region's badges won
int NeedsRegionStarter(const Game& g);        // the story is in a region someone hasn't got its starter from yet (-1 = no)
inline int PickedRegion(const Game& g) { return g.regionPick - 1; }
std::vector<int> StarterPool(const Game& g);   // the starter screen's Pokemon (the region's own three)
bool CanChooseRegion(const Game& g);           // a region is done and another is waiting to be chosen
std::vector<int> RegionsLeft(const Game& g);   // the regions not played yet
void BeginRegionChoose(Game& g, int forWhat);  // to the region screen
void ApplySaveOrder(Game& g);            // lay the story out in the active save's region order
void UpdateRegionChoose(Game& g);    void DrawRegionChoose(const Game& g);
// ---- nicknames, the Day Care and Eggs (screens_daycare.cpp)
void UpdateNickname(Game& g);        void DrawNickname(const Game& g);
void UpdateHatch(Game& g, float dt); void DrawHatch(const Game& g);
void UpdateDaycare(Game& g);         void DrawDaycare(const Game& g);
void UpdateSafari(Game& g);          void DrawSafari(const Game& g);
void OpenUnownPuzzle(Game& g, const std::string& kind);   // the Ruins of Alph's panels (screens_puzzle.cpp)
void UpdateUnownPuzzle(Game& g);     void DrawUnownPuzzle(const Game& g);
void AlphSolved(Game& g, const std::string& kind);        // (overworld.cpp) the floor gives way
void OpenVoltorbFlip(Game& g);       // HeartGold's Goldenrod Game Corner (screens_voltorb.cpp)
void UpdateVoltorbFlip(Game& g);     void DrawVoltorbFlip(const Game& g);
void OpenSlotsFromWorld(Game& g);    // a slot machine on the map (screens_slots.cpp): back to the map after   // the Safari Zone / the Bug-Catching Contest
void OpenSafariEncounter(Game& g, int species, int level, bool shiny, int nature, const int iv[6]);
void SafariEnded(Game& g);           // out of balls: back to the gate (overworld.cpp)
void OpenDaycare(Game& g, int who, Screen back);
void OpenNickname(Game& g, int who, int mon, int returnTo, bool ask);   // returnTo: 0 the story queues, 1 Party & Box, 2 the world
bool NextNickOrHatch(Game& g);       // the story queues: a nickname to ask or an Egg hatching next? (true: on its way)
void HubDaycareSteps(Game& g);       // after a battle away from the walked world: it counts as 256 steps
bool WorldStepEggs(Game& g, int who);   // a step in the world: true if an Egg hatched (the Hatch screen is up)
void DrawJudge(const OwnedMon& m);   // the Judge's panel: IVs and EVs
// ---- roaming legendaries (screens_legend.cpp): Raikou and Entei, Latias and Latios, Mesprit and Cresselia, Tornadus and Thundurus
bool IsRoamer(int legendId);
void UpdateRoamers(Game& g, int who, bool move);      // start the ones whose time has come; move them all to new routes (move)
int RoamerAt(const Game& g, int who, const std::string& area);   // the roaming legendary on that route now (-1: none)
bool TryRoamerBattle(Game& g, const std::string& area, const std::string& method);   // a wild battle here: 1 in 4 it's the roamer
std::string RoamerWhere(const Game& g, int legendId);   // "Route 38" (empty: not roaming yet / caught)
void RoamerBattleOver(Game& g, const Battle& b);       // its HP stays down; caught: it's gone
void BeginRegionStarter(Game& g);              // to the starter screen for that region's gift
int RegionGiftLevel(const Game& g, int region, int who = 0);
std::string RegionRivalName(int region);                          // "Silver", "May"
std::string RegionWatchLine(int region, bool coop, int left);     // the rival watching before you choose
std::string RegionIntroLine(int region, const std::string& who, int level);   // the level that region's starter gift comes at
// After the region's starters are chosen: the rival takes theirs (yours on the left, theirs on the right).
void DrawRivalReveal(const Game& g, const std::vector<std::string>& yours, const std::vector<std::string>& his);
void BeginTowerFloor(Game& g, int floor, uint32_t seed);   // set up the next Tower battle (then the intro screen)
// The Battle Frontier (Hoenn, after its League): seven facilities, each with its own streak (kept in the save's flags,
// so co-op shares it) and its Frontier Brain at Emerald's streaks, who gives a Silver and then a Gold Symbol.
struct FrontierInfo { const char* name; const char* brain; const char* symbol; int silver, gold, teamSize; const char* rule; const char* team[2]; };
const FrontierInfo& Frontier(int fac);      // 1 Tower, 2 Dome, 3 Palace, 4 Arena, 5 Factory, 6 Pike, 7 Pyramid (Emerald);
                                            // 8 Tower, 9 Factory, 10 Arcade, 11 Castle, 12 Hall (HeartGold's, past Route 40);
                                            // 13 Slateport, 14 Verdanturf, 15 Fallarbor: Hoenn's Battle Tents (three in a row)
constexpr int FRONTIER_COUNT = 15;
// the rules a facility battles by: the Emerald facility it's like (a Battle Tent is a small Factory, Palace or Arena)
inline int FrontierKind(int fac) { return fac == 9 || fac == 13 ? 5 : fac == 14 ? 3 : fac == 15 ? 4 : fac == 8 ? 1 : fac; }
inline bool FrontierRentals(int fac) { return FrontierKind(fac) == 5; }   // the Factories: rental Pokemon
inline bool BattleTent(int fac) { return fac >= 13 && fac <= 15; }
const char* TentPrize(int fac);             // what three wins in a row at a Battle Tent bring
int ArcadeEffect(const Game& g);            // the Battle Arcade's board: what this battle's roll landed on (0 nothing)
const char* ArcadeEffectName(int e);
std::string HallType(int n);
int CastlePoints(const Game& g);            // the Battle Castle's points (a flag in the save)
void SetCastlePoints(Game& g, int cp);                // the Battle Hall: the n-th battle's type
int FrontierStreak(const Game& g, int fac, int* best = nullptr, int* runs = nullptr);
void SetFrontierStreak(Game& g, int fac, int streak, int best, int runs);
int FrontierBrainAt(int fac, int n);        // the n-th battle of a streak is the Brain's: 1 Silver, 2 Gold, 0 no
void BeginFrontierBattle(Game& g, int fac); // the next battle of that facility's streak
uint32_t TowerSeedFor(const Game& g);         // this run's floors (the same on both online computers)
void UpdateShop(Game& g);            void DrawShop(const Game& g);
void UpdateCoopSelect(Game& g);      void DrawCoopSelect(const Game& g);   // two players choose profiles
void UpdateCoopHub(Game& g);         void DrawCoopHub(const Game& g);
void UpdateNetMenu(Game& g);         void DrawNetMenu(const Game& g);
void UpdatePokedex(Game& g);         void DrawPokedex(const Game& g);
void UpdateTrade(Game& g, float dt); void DrawTrade(const Game& g);
void OpenPokedex(Game& g, int who);
void OpenTrade(Game& g);
// A trade: each player gives the creature at idx[p] (whose data is mon[p]) and gets the other one.
void ApplyTrade(Game& g, const int idx[2], const OwnedMon mon[2]);
std::string MonToText(const OwnedMon& m);
bool MonFromText(const std::string& text, OwnedMon& out);
constexpr float ENCOUNTER_WIPE_T = 1.35f;   // the wild-encounter flash and wipe (seconds)
void DrawEncounterWipe(const Game& g);
struct RaidInfo { int species = -1; bool shiny = false; int day = 0; };
int RaidToday();                           // days since 1970 in local time (the raid changes at midnight)
RaidInfo RaidOfDay(int day);               // that day's boss: any final form, legendary or starter
void StartRaid(Game& g);
void StartCoopRaid(Game& g);               // co-op: each player picks 6 on the team screen                   // the Daily Raid: straight in with the whole healthy party
void StartTrainingBattle(Game& g);  // a training battle in the current training area (the team screen first)
void StartCoopTrainingBattle(Game& g);   // the same, 2v2 for both co-op players
void SetTrainingWild(Game& g);      // training battles use the chosen area's wild Pokemon (wild.h)
int TrainingAreaIndex(const Game& g);
bool SetTrainArea(Game& g, int area);
void RepickRare(Game& g);                 // the training area changed: a new rare find from it
int TrainStage(const Game& g);
std::string TrainAreaName(const Game& g);
int TrainBadges(const Game& g);
int TrainingAreaIndex(const Game& g);   // pick where training battles happen (false + toast if not allowed)
void OnlineSendCoop(Game& g);        // shared story progress and clock (after Wait)
void OnlineSendTrade(Game& g);       // my presence at the trade machine, offer and ready
void OnlineTradeUpdate(Game& g);     // host: both ready -> do the trade
void StartOnlineFlow(Game& g);
void OnlinePoll(Game& g);            // every frame: network messages, connection state
bool OnlineBattleUpdate(Game& g, float dt);   // true if it handled the battle screen
void OnlineSendProfile(Game& g);
void OnlineSetReady(Game& g, int kind);
void OnlineLeave(Game& g, const std::string& why);
bool OnlineMine(const Game& g, int who);       // offline: always; online: only my own player
bool OnlineWaiting(Game& g);                   // online and reconnecting: shows a toast and returns true
bool OnlineCodeEntry(Game& g);      // reconnecting: the 'type their new code' box (true = it has the input)
void DrawOnlineOverlay(const Game& g);         // the 'reconnecting' banner over any screen
void OnlineSendCatch(int poolPick);
void OnlineSendThrow(int species, int level, bool ok, int shakes, bool fled);   // so the partner watches our throw
void OnlineCancelSelect(Game& g);

// Flow helpers
void GoTitle(Game& g);
void StartQuickFlow(Game& g, Mode m);       // Quick / TwoPlayer / Watch
void BeginTeamSelect(Game& g);              // after rosters are chosen
void StartBattleFromSelect(Game& g);
void EnterStoryHub(Game& g);
void StartPractice(Game& g);                 // the practice arena (dummies), from either story hub
void UpdatePractice(Game& g, float dt);
void DrawPractice(const Game& g);
void ProcessStoryBattleEnd(Game& g);        // XP, levels, queues
void AdvanceStoryQueues(Game& g);           // catch -> evolve -> learn -> reward -> hub
void OpenHealingCenter(Game& g);
void EndCatchTurn(Game& g);
int HealthyCount(const Profile& p);         // healthy creatures in the party
bool IsStoryMode(Mode m);                   // solo or co-op story / training
bool WorldBattleOnWater(const Game& g);   // a wild battle met surfing, fishing or under the sea (the Dive Ball)
Profile& Prof(Game& g, int who);            // co-op: that player's profile; solo: the profile
const Profile& Prof(const Game& g, int who);
void SaveWho(Game& g, int who);
void StartCoopFlow(Game& g);
Profile ProgressView(const Game& g);        // story progress (stage, badges) for the map
bool IsRematch(const Game& g);              // the story battle being set up is a stage already beaten
bool StartGymRematch(Game& g, int stage);   // from the Map: false (with a toast) if it can't start now
int BadgeCount(const Game& g);
int& StoryClock(Game& g);                   // shared time of day / season (co-op save or profile)
int StoryClock(const Game& g);
void EnsureRival(Game& g, int region = 0);  // the rival (Blue in Kanto, Silver in Johto, May in Hoenn) picks their starter(s) once per save
// Story cards: a full-screen page of the story (shown over the hub or a battle's intro until read).
void ShowStoryCard(Game& g, const std::string& titleAndText, const std::string& mon = "");
bool StoryCardUp(const Game& g);
bool UpdateStoryCard(Game& g);   // true while the card takes the input
void DrawStoryCard(const Game& g);
bool PostgameBeaten(const Game& g, int stage);   // an optional post-game battle (Red, Steven) has been won
int ProfileStarter(const Profile& p);       // species index of the starter they chose (-1 unknown)
void StoryTimeStep(Game& g, bool announce); // one step of time (a battle or a Wait)
int HallOfFameCount(const Game& g);
void DrawLeaderboard(const Game& g);           // co-op: the journey's leaderboard (an overlay)
void OpenCreator(Game& g, bool fromWorld);     // the trainer creator (from the world's menu: change your look)
