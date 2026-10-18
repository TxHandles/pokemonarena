#pragma once
#include <string>
#include <vector>
#include "battle.h"
#include "input.h"
#include "profile.h"
#include "story.h"
#include <deque>
#include <map>

enum class Screen {
    Title, Settings, Controls,
    ProfileSelect, NameEntry, Starter, StoryHub, MyCreatures, StageIntro,
    RosterPick, Select, Battle, Results,
    StoryResults, Evolve, LearnMove, Reward, Catch, Heal, Map, Shop, DoublesMenu,
    CoopSelect, CoopHub, CoopCatch, NetMenu, Pokedex, Trade, Fossil, Gift, Revive, Slots
};

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
    int me = 0;                   // 0 = host, 1 = guest
    // Setup screen
    int page = 0;                 // 0 choose, 1 hosting, 2 joining
    int sel = 0, profSel = 0;
    std::string field;            // typed address / code
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
    int ctrlTab = 0, ctrlRow = 0, ctrlCol = 0;
    bool capturing = false;
    float captureT = 0;

    // Profiles
    std::vector<std::string> profiles;
    int profSel = 0;
    bool confirmDelete = false;
    std::string nameBuf;
    Profile profile;            // the Story Mode profile in use
    int starterSel = 0;
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
    bool ready[2] = {false, false};
    float startDelay = -1;
    int teamSize = 2;

    // Battle
    Battle battle;
    bool paused = false;

    // Story
    int stageIdx = -1;                      // -1 = training battle
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
    struct CatchCand { int species; int level; std::vector<std::string> moves; float hpFrac; float chance; int abSlot = 0; int nature = 0; };
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
    int swapFrom = -1;            // My Creatures: Box creature waiting to swap into a full party
    bool stonePick = false;       // My Creatures: choosing an evolution stone
    bool returnToParty = false;   // after a stone evolution, go back to Party & Box
    bool returnToShop = false;    // after the Move Relearner, go back to the shop
    bool tmPick = false;          // Party & Box: choosing a TM to teach
    bool showBoard = false;       // co-op hub: the leaderboard is open
    // Gifts (fossil revivals, Eevee): shown one at a time after a battle; the Pokemon is already added
    struct Gift { int who; int species; std::string text; std::string item; };   // item: an item gift (Old Amber) instead
    std::vector<Gift> giftQueue;
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
    OnlineSession on;
    // Pokedex
    int dexSel = 0, dexScroll = 0, dexWho = 0;
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
void SetTrainingWild(Game& g);      // training battles use the chosen area's wild Pokemon (wild.h)
int TrainingAreaIndex(const Game& g);
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
void OnlineSendCatch(int poolPick);
void OnlineCancelSelect(Game& g);

// Flow helpers
void GoTitle(Game& g);
void StartQuickFlow(Game& g, Mode m);       // Quick / TwoPlayer / Watch
void BeginTeamSelect(Game& g);              // after rosters are chosen
void StartBattleFromSelect(Game& g);
void EnterStoryHub(Game& g);
void ProcessStoryBattleEnd(Game& g);        // XP, levels, queues
void AdvanceStoryQueues(Game& g);           // catch -> evolve -> learn -> reward -> hub
void OpenHealingCenter(Game& g);
void EndCatchTurn(Game& g);
int HealthyCount(const Profile& p);         // healthy creatures in the party
bool IsStoryMode(Mode m);                   // solo or co-op story / training
Profile& Prof(Game& g, int who);            // co-op: that player's profile; solo: the profile
const Profile& Prof(const Game& g, int who);
void SaveWho(Game& g, int who);
void StartCoopFlow(Game& g);
Profile ProgressView(const Game& g);        // story progress (stage, badges) for the map
int BadgeCount(const Game& g);
int& StoryClock(Game& g);                   // shared time of day / season (co-op save or profile)
int StoryClock(const Game& g);
int ProfileStarter(const Profile& p);       // species index of the starter they chose (-1 unknown)
void StoryTimeStep(Game& g, bool announce); // one step of time (a battle or a Wait)
