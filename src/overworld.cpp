// The overworld (test build). Maps are the game's own pictures (World/*.png) with a hidden layer of
// rules from World/world.txt: which 16-pixel tiles are solid, tall grass or a ledge, where the
// doors, signs and people are, and how the maps join at their edges. Walking up to the story's
// trainers, into tall grass or through a Pokemon Center's door uses the same battles and screens
// as the menu.
#include "overworld.h"
#include "berries.h"
#include "breeding.h"
#include "contest.h"
#include "look.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include "arena.h"
#include "audio.h"
#include "battle.h"
#include "net_state.h"
#include "embedded.h"
#include "game.h"
#include "input.h"
#include "items.h"
#include "rng.h"
#include "sprites.h"
#include "story.h"
#include "legend.h"
#include "storymap.h"
#include "ui.h"
#include "settings.h"
#include "wild.h"

void OnlineSendWorldPos(bool here, int map, int x, int y, int face);
void OnlineSendWorldFight(int stage, int map, int x, int y, const std::string& trainer);
void UpdateScreenOf(Game& g, float dt);
void OnlineSendWatch(bool on);
void OnlineSendWatchState(const Battle& b);
void OnlineSendWatchStep(float dt, const PlayerInput* in);
void OnlineSendWatchEnd();
void OnlineSendTeam(int what);
void OnlineSendXpShare(int xp);
void OnlineSendWorldMap(int map, const std::string& state);
extern void (*gBattleStepHook)(const Battle&, float, const PlayerInput*);
void DrawScreenOf(const Game& g);

#ifdef ARENA_AUTOTEST
extern int gAutoMenu;
PlayerInput gOwTestIn[2];   // autotest: scripted walking for each walker
bool gOwTestOn = false;
#endif

namespace {

constexpr int T = 16;        // tile size in map pixels
constexpr float Z = 2.0f;    // map pixels -> screen units
constexpr float WALK = 0.24f, RUN = 0.13f, BIKE = 0.085f;   // seconds per tile

struct OwNpc {
    int x = 0, y = 0, face = 0, sight = 0;
    int sprite = 0;
    std::string kind;                 // talk, trainer (story), fight (a route trainer), give, heal, shop
    std::vector<std::string> args;    // talk: text; trainer: stage, before, after; fight: name, team, before, after; give: text, item, n, flag, after
    std::string ifF, unlessF;         // shown only while that flag is set / not set (see HasFlag)
    bool wander = false;              // facing "wander": strolls around near where they stand
    int mon = -1;                     // sprite "mon:Name": a Pokemon standing there (a legendary in its lair); -3 an Apricorn tree
    std::string tree;                 // sprite "apricorn:Color": an Apricorn tree (HeartGold) of that colour
    std::string needItem, needText, usedText;   // "need = Item|text without it|text using it": Snorlax and the Poke Flute
    std::vector<std::string> rematch; // "rematch = team / team": Emerald's stronger teams for Match Call's rematches (tier 1, 2, ...)
    std::string opts;                 // "fightopts = scale double": levelled to your party / a double battle (the Trainer Tower)
    std::string winThen;              // "won = action": done once you've beaten them (the Winstrates come out one after another)
};
struct OwLink { int map = -1; int off = 0; std::string msg; };
struct OwMap {
    std::string name, image, wild;
    std::string wildIf;                // "wildif = flag": no wild Pokemon until it's set (the Ruins of Alph's Unown)
    bool indoor = false;
    int w = 0, h = 0;
    std::string cells;
    Texture2D tex{};
    std::string overImage;            // "over = file": the parts drawn over people (tree tops, roofs)
    Texture2D over{};
    bool objSprites = false;          // "objects = sprites": Cut trees, rocks, boulders and item balls are drawn, not in the picture
    double used = -1;                 // pictures are loaded when needed (the last time it was drawn)
    OwLink link[4];                   // down (south), up (north), left (west), right (east)
    struct Act { int x, y; std::string act; std::string ifF, unlessF; };
    std::vector<std::pair<std::string, std::string>> autos;   // "auto = condition|action": done as soon as the condition holds here
    std::vector<Act> doors, spots;
    std::vector<Act> steps;           // "step = x y action": done when you step onto that square (the Trick House's buttons)
    std::string img2;                 // "img2 = picture": a second picture of the map (the Trick House: its buttons pressed)...
    Texture2D tex2{};
    struct Swap { int x, y; std::string flag; char cell = 0; };
    std::vector<Swap> swaps;          // ..."swap = x y flag [c]": that square comes from img2 while the flag is set (and is c on the grid)
    std::map<int, std::pair<int, int>> rtiles;   // "rtile = x y colour dir": Emerald's rotating tile puzzle (Mossdeep Gym, Trick House 7)
    struct WildAt { int x0, y0, x1, y1; std::string area; };
    std::vector<WildAt> wildAt;       // "wildat = x0 y0 x1 y1 Area": walking there, that area's Pokemon (Mirage Island's Wynaut)
    struct RGate { int x, y, shape, orient; };
    std::vector<RGate> rgates;        // "rgate = x y shape orientation": Emerald's rotating gates (Fortree Gym, the Trick House)
    std::vector<OwNpc> npcs;
    struct Item { int x, y; std::string item; int n = 1; bool hidden = false; std::string ifF, unlessF; };
    std::vector<Item> items;          // item balls (and hidden items) to pick up, once each
    struct Gate { int x, y; std::string flag; };
    struct Block { int x, y; std::string flag, text, act; };   // act: done after the text (">warp|..." in world.txt)
    std::vector<Block> blocks;        // "block = x y flag|text": you're turned back there until the flag (FireRed's guards, the old man)
    std::vector<Gate> gates;          // solid until the flag is set (a guard's gone, a switch is pressed)
    bool dark = false;                // a cave you can barely see in until you use Flash
    std::string weather;              // rain, snow, fog, sand, ash, shade, bubbles: falls on this map (outdoors)
    struct Stand { int x, y; std::string move, flag, text; };   // "stand = x y Move|flag|text": use a move standing here (the Regis' tombs)
    std::vector<Stand> stands;
    std::vector<std::pair<int, int>> lap;   // "lap = flag|gx gy|x,y x,y ...": a lap along these tiles, back to (gx, gy) (Island Cave)
    std::string lapFlag;
    int lapGX = -1, lapGY = -1;
    struct BerryTree { int x, y, id; std::string berry; std::string ifF; };   // "berry = x y tree Oran" (Soil: nothing planted at first)
    std::vector<BerryTree> berries;   // Hoenn's berry trees
    struct WxZone { int x0, y0, x1, y1; std::string wx; };
    std::vector<WxZone> wxZones;      // "wxzone = x0 y0 x1 y1 sand": just there (Route 111's desert)
    bool bike = false;                // Cycling Road: Bicycles only
    bool cycling = false;             // "cycling = yes": indoors, but you can ride a Bicycle (caves, the Sky Pillar)
    std::string altFlag, altImage, altOver;   // "alt = flag|picture|over": shown instead while the flag's set (the Mansion's switch)
    bool altShown = false;
    bool underwater = false;          // "underwater = yes": under the sea (Dive) - you swim on your Pokemon
    std::string diveTo, emergeTo;     // "dive = Map": Dive here goes down to that map (the same spot, or "Map|x|y" a fixed one);
    int diveX = -1, diveY = -1, emergeX = -1, emergeY = -1;   // "emerge = Map": back up to the surface
    struct EmergeAt { int x, y; std::string map; int tx, ty; };
    std::vector<EmergeAt> emergeAt;   // "emergeat = x y Map|tx|ty": surfacing on that one square goes somewhere else (the Sealed Chamber)
    struct DiveSpot { int x0, y0, x1, y1; std::string flag, map; int tx, ty; };
    std::vector<DiveSpot> diveSpots;  // "divespot = x0 y0 x1 y1 flag|Map|tx|ty": diving there (while the flag's set) goes there (Marine Cave)
    std::string town;                 // a town you can Fly to (its name on the Town Map)
    int flyX = -1, flyY = -1;         // ...where you land (outside its Pokemon Center)
    int startX = -1, startY = -1;
    // HeartGold / SoulSilver maps (tools/hgss_world.py): rendered from the 3D models, so the ground has heights
    int region = 0;                   // "region = Johto": whose story / levels it belongs to (route trainers scale with it)
    int imgOffY = 0;                  // "imgy = n": the picture starts this far above the map's top row
    std::string hts;                  // "heights:" one char per tile, 48 + lift / 4 (how far up the screen people stand)
    mutable int grassSample = -2;     // rendered maps: a tall-grass square in the middle of a patch (its picture goes over people)
    bool hasPos = false;              // "pos = x y": where it sits in the region (maps that touch join up, any number per side)
    int gx = 0, gy = 0;
    struct Near { int map, dx, dy; };
    std::vector<Near> near;           // (worked out from pos) the maps touching this one, and where they are from here
    float Lift(int x, int y) const {
        if (hts.empty() || w <= 0 || h <= 0) return 0;
        x = x < 0 ? 0 : x >= w ? w - 1 : x; y = y < 0 ? 0 : y >= h ? h - 1 : y;
        return ((unsigned char)hts[y * w + x] - 48) * 4.0f;
    }
    char At(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? '#' : cells[y * w + x]; }
};
struct NpcRt { int x, y, face; float wt = 0, st = 1; int fromX = 0, fromY = 0; };   // wanderers: next stroll, mid-step     // where a person is right now (they turn and walk over)
// What changed on a map since you came in (it comes back when you leave, like the games): trees cut,
// rocks smashed, boulders pushed.
struct MapRt { std::set<int> cleared; std::vector<int> boulders; int can1 = -1, can2 = -1; bool gotCan1 = false; };   // + the trash-can puzzle (Vermilion Gym)
std::vector<MapRt> gRt;
struct Rustle { int map, x, y; float t; };   // grass shaking where someone just stepped
std::vector<Rustle> gRustle;
void UpdateRustle(float dt);

std::vector<OwMap> gMaps;
std::vector<std::vector<NpcRt>> gNpc;
std::vector<std::string> gSpriteNames;
std::set<int> gWalkedRegions;
std::set<std::string> gSkipStages;   // story stages with no place on the walked map (they count as done)                    // regions whose story is walked, not picked from the menu
std::map<std::string, std::string> gStoryTown;
struct Ferry { std::string label, map; int x, y; std::string cond; };
std::vector<Ferry> gFerries;   // the Seagallop: where it sails (world.txt "ferry = Label | Map | x y | flag")   // story stage -> the town you'd be in before it (a save that never walked starts there)
Texture2D gChars{};
Texture2D gActions{};   // the player's bike / surf / fishing frames (32x32): rows red bike, green bike, red surf, green surf, red fish, green fish
Texture2D gFieldObj{};
Texture2D gBerryTex{};
bool gLapOn[2] = {false, false};      // Island Cave's lap (Regice): running it now, and the path tiles stepped on
uint64_t gLapMask[2] = {0, 0};   // World/berrytrees.png: a row per berry (soil, sprout x2, growing x2, flowering x2, berries x2), 16x32 cells
Texture2D gWxRain{}, gWxSnow[2]{}, gWxFog{}, gWxSand{};   // FireRed's weather sprites (tools/frlg_weather.py)  // Cut tree, smashable rock, boulder, item ball
bool gLoaded = false;

const int DX[4] = {0, 0, -1, 1}, DY[4] = {1, -1, 0, 0};
int Opp(int f) { return f == 0 ? 1 : f == 1 ? 0 : f == 2 ? 3 : 2; }
std::string WorldWeatherAt(const Game& g, const OwMap& m, int x = -1, int y = -1);
constexpr int HALF_DAY_SEC = 20 * 60;   // the walked world: 20 minutes of day, then 20 of night

std::vector<std::string> Split(const std::string& s, char c) {
    std::vector<std::string> v;
    std::string cur;
    for (char ch : s) { if (ch == c) { v.push_back(cur); cur.clear(); } else cur += ch; }
    v.push_back(cur);
    return v;
}
std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
int FaceOf(const std::string& s) { return s == "up" ? 1 : s == "left" ? 2 : s == "right" ? 3 : 0; }
int MapIndex(const std::string& n) {
    for (size_t i = 0; i < gMaps.size(); i++) if (gMaps[i].name == n) return (int)i;
    return -1;
}
int SpriteIndex(const std::string& n) {
    for (size_t i = 0; i < gSpriteNames.size(); i++) if (gSpriteNames[i] == n) return (int)i;
    return 0;
}
Texture2D LoadTex(const std::string& path) {
    Texture2D t{};
    if (!FileExists(path.c_str())) return t;
    Image img = LoadImage(path.c_str());
    if (!img.data) return t;
    t = LoadTextureFromImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_POINT);
    UnloadImage(img);
    return t;
}

// ---- who's who
bool Online(const Game& g) { return g.coop && g.on.active; }
bool LocalCoop(const Game& g) { return g.coop && !g.on.active; }
int Driven(const Game& g) { return LocalCoop(g) ? 2 : 1; }   // walkers this computer moves
// The co-op player a walker is (0/1); solo: 0.
int WhoOf(const Game& g, int i) {
    if (!g.coop) return 0;
    if (g.on.active) return i == 0 ? g.on.me : 1 - g.on.me;
    return i;
}
const Profile& WP(const Game& g, int i) { return Prof(g, WhoOf(g, i)); }
Profile& WP(Game& g, int i) { return Prof(g, WhoOf(g, i)); }
int StoryStageNow(const Game& g) { return g.coop ? g.coopSave.storyStage : g.profile.storyStage; }
int WalkerSprite(const Game& g, int i) { return SpriteIndex(WhoOf(g, i) == 1 ? "leaf" : "player"); }
bool Shown(const Game& g, int i) { return i < Driven(g) || (Online(g) && i == 1 && g.ow.w[1].here); }

const OwMap& MapOf(const OwWalker& w) { return gMaps[w.map]; }
// Online: your partner is in a battle of their own (or picking their team for one) right now.
bool PartnerBattling(const Game& g) {
    const std::string& a = g.on.partnerActivity;
    return Online(g) && (a == "in battle" || a == "choosing their team" || a == "catching" || a == "busy");
}
void SaveOrWarnP(Game& g) { if (!SaveProfile(g.profile)) Toast(g, "Could not save the profile!"); }

// ---- what has happened: flags (solo: the profile; co-op: the shared save)
std::set<std::string>& Flags(Game& g) { return g.coop ? g.coopSave.owFlags : g.profile.owFlags; }
const std::set<std::string>& Flags(const Game& g) { return g.coop ? g.coopSave.owFlags : g.profile.owFlags; }
const char* kKantoBadges[9] = {"", "Boulder", "Cascade", "Thunder", "Rainbow", "Soul", "Marsh", "Volcano", "Earth"};
// The badge that lets you use HM n on that region's maps (Johto: HeartGold's - Cut needs the Hive Badge there)
std::string FieldBadge(int n, int region) {
    static const char* kJohto[8] = {"", "Hive", "Storm", "Fog", "Plain", "Zephyr", "Zephyr", "Rising"};
    if (region == 1 && n >= 1 && n <= 7) return kJohto[n];
    // Emerald: Cut - Stone, Fly - Feather, Surf - Balance, Strength - Heat, Flash - Knuckle, Rock Smash - Dynamo, Waterfall - Rain, Dive - Mind
    static const char* kHoenn[9] = {"", "Stone", "Feather", "Balance", "Heat", "Knuckle", "Dynamo", "Rain", "Mind"};
    if (n >= 1 && n <= 8 && region >= 0 && region < RegionCount() && RegionInfo(region).name == "Hoenn") return kHoenn[n];
    return kKantoBadges[std::min(8, HmBadges(n))];
}
int MapRegionOf(const Game& g, int i);
bool StageBeaten(const Game& g, int s);
bool HasBadge(const Game& g, const std::string& b) {
    const auto& v = g.coop ? g.coopSave.badges : g.profile.badges;
    return std::find(v.begin(), v.end(), b) != v.end();
}
// A condition: a flag that's been set, or "badge:Cascade", "stage:Cerulean Gym" (the story is past it),
// "item:Bicycle" (this walker carries it); "!" in front turns it around.
int MapRegionOf(const Game& g, int i) { int m = g.ow.w[i].map; return m >= 0 && m < (int)gMaps.size() ? gMaps[m].region : 0; }
float WorldHour(const Game& g);
bool HasFlag(const Game& g, int i, const std::string& f0) {
    if (f0.empty()) return true;
    if (size_t amp = f0.find('&'); amp != std::string::npos) return HasFlag(g, i, f0.substr(0, amp)) && HasFlag(g, i, f0.substr(amp + 1));   // all of them
    if (size_t comma = f0.find(','); comma != std::string::npos) return HasFlag(g, i, f0.substr(0, comma)) || HasFlag(g, i, f0.substr(comma + 1));   // any of them
    if (f0[0] == '!') return !HasFlag(g, i, f0.substr(1));
    if (f0.rfind("done:", 0) == 0) { for (int r = 0; r < RegionCount(); r++) if (RegionInfo(r).name == f0.substr(5)) return RegionDone(g, r); return false; }
    if (f0.rfind("badge:", 0) == 0) return HasBadge(g, f0.substr(6));
    if (f0.rfind("badges:", 0) == 0) return (int)(g.coop ? g.coopSave.badges : g.profile.badges).size() >= std::atoi(f0.c_str() + 7);
    if (f0.rfind("stage:", 0) == 0) { int s = StageIndexByName(f0.substr(6)); return s >= 0 && StageBeaten(g, s); }
    if (f0.rfind("item:", 0) == 0) return BagCount(WP(g, i), f0.substr(5)) > 0;
    if (f0 == "biking") return i >= 0 && i < 2 && g.ow.w[i].biking;   // (cracked floors: a bike gets across)
    if (f0 == "safari") return g.ow.safari.on && !g.ow.safari.contest;   // in the Safari Zone's game
    if (f0 == "bugcontest") return g.ow.safari.on && g.ow.safari.contest;   // in the Bug-Catching Contest
    if (f0.rfind("own:", 0) == 0) return Flags(g).count(f0.substr(4) + ":" + WP(g, i).name) > 0;   // this player's own (co-op: each of you)
    if (f0.rfind("weekday:", 0) == 0) { time_t now = time(nullptr); return localtime(&now)->tm_wday == std::atoi(f0.c_str() + 8); }   // the real day (0 Sunday)
    if (f0 == "mirage") {   // Mirage Island off Route 130: there on some days (Emerald: when a Pokemon's number matches the day's)
        uint32_t d = (uint32_t)(StoryClock(g) / 2) * 2654435761u;
        return ((d >> 13) % 5) == 0;
    }
    if (f0 == "hightide") { float h = WorldHour(g); return (h >= 3 && h < 9) || (h >= 15 && h < 21); }   // Shoal Cave (Emerald's tide times)
    if (f0 == "pokemon") return !WP(g, i).mons.empty();   // this walker has a Pokemon (Oak's starter)
    if (f0.rfind("rstarter:", 0) == 0) {   // the region's professor has given you theirs (rstarter:1 = Elm's)
        int r = std::atoi(f0.c_str() + 9);
        if (r == 0) return HasFlag(g, i, "pokemon");
        return g.coop ? !RegionStarter(g.coopP[0], r).empty() && !RegionStarter(g.coopP[1], r).empty() : !RegionStarter(g.profile, r).empty();
    }
    if (f0 == "allpokemon") return g.coop ? !g.coopP[0].mons.empty() && !g.coopP[1].mons.empty() : !g.profile.mons.empty();   // everyone has
    return Flags(g).count(f0) > 0;
}
bool CondOk(const Game& g, int i, const std::string& ifF, const std::string& unlessF) {
    return (ifF.empty() || HasFlag(g, i, ifF)) && (unlessF.empty() || !HasFlag(g, i, unlessF));
}
void SetFlag(Game& g, const std::string& f) {
    if (f.empty()) return;
    Flags(g).insert(f);
    if (Online(g)) OnlineSendCoop(g);
}

// ---- the move tutors: who can learn what (World/tutors.txt - Emerald's tutor tables, the starters' ultimate moves).
// A move the file doesn't list (Headbutt), or a Pokemon newer than the lists: anyone who could learn it some other way.
}  // namespace
bool MonCanLearnTM(const OwnedMon& m, const std::string& move);   // (screens_story.cpp)
bool TutorCanLearn(const OwnedMon& m, const std::string& move);
void LoadTutors();
namespace {
std::map<std::string, std::set<std::string>> gTutorCompat;
std::set<std::string> gTutorKnown;   // species the lists cover
}  // namespace
int PartyCountOf(const Profile& P) { int n = 0; for (const auto& m : P.mons) n += !m.boxed; return n; }
// ---- HeartGold's Pokegear radio: the station you're tuned to (each player their own), today's swarm, Buena's word
int RegionByName(const std::string& n) { for (int r = 0; r < RegionCount(); r++) if (RegionInfo(r).name == n) return r; return -1; }
std::string RadioStation(const Game& g, int i) {
    const std::string tail = "|" + WP(g, i).name;
    for (const auto& f : Flags(g)) if (f.rfind("radiost|", 0) == 0 && f.size() > tail.size() && f.compare(f.size() - tail.size(), tail.size(), tail) == 0) return f.substr(8, f.size() - 8 - tail.size());
    return "";
}
bool InJohto(const Game& g, int i) { const OwWalker& w = g.ow.w[i]; return w.map >= 0 && RegionInfo(gMaps[w.map].region).name == "Johto"; }
struct Swarm { const char* map; const char* species; bool water; };
const Swarm kSwarms[] = {   // HeartGold's Johto swarms (Prof. Oak's Pokemon Talk names one each day)
    {"Route 27", "Luvdisc", true}, {"Route 32", "Qwilfish", true}, {"Route 34", "Ralts", false}, {"Route 35", "Yanma", false},
    {"Route 38", "Snubbull", false}, {"Route 44", "Remoraid", true}, {"Route 45", "Swablu", false}, {"Route 47", "Ditto", false},
    {"Dark Cave", "Dunsparce", false}, {"Mt. Mortar", "Marill", false}, {"Violet City", "Whiscash", true}};
const Swarm& TodaysSwarm() { uint32_t h = (uint32_t)RaidToday() * 2654435761u; return kSwarms[(h >> 11) % (sizeof(kSwarms) / sizeof(kSwarms[0]))]; }
const char* kBuenaWords[] = {"Cyndaquil", "Totodile", "Chikorita", "Pikachu", "Togepi", "Marill", "Wobbuffet", "Sudowoodo", "Lugia", "Ho-Oh",
                             "Fresh Water", "Lemonade", "Rage Candy Bar", "Apricorn", "Bell Tower", "Radio Tower", "Gym Leader", "Rival",
                             "Hyper Beam", "Thunderbolt", "Flamethrower", "Surf", "Psychic", "Lullaby", "March", "Pokegear", "Pokedex", "Hiker",
                             "Kimono Girl", "Silver Wing"};
constexpr int kBuenaN = sizeof(kBuenaWords) / sizeof(kBuenaWords[0]);
int BuenaWord(int day) { return (int)(((uint32_t)day * 2246822519u >> 7) % kBuenaN); }
// ---- Emerald's town extras: the Lilycove Lady's quizzes and favors (World/lilycove_lady.txt), the outbreaks on TV
struct LadyQuiz { std::string q, a[3], answer, prize; };
struct LadyFavor { std::string thing, prize; std::vector<std::string> items; };
std::vector<LadyQuiz> gLadyQuiz;
std::vector<LadyFavor> gLadyFavor;
void LoadLilycoveLady() {
    gLadyQuiz.clear(); gLadyFavor.clear();
    char* txt = LoadFileText((DataDir() + "World/lilycove_lady.txt").c_str());
    if (!txt) return;
    std::string s = txt;
    UnloadFileText(txt);
    for (const auto& line : Split(s, '\n')) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> f;
        size_t at = 0;
        while (true) { size_t k = line.find(" ~ ", at); f.push_back(Trim(line.substr(at, k == std::string::npos ? std::string::npos : k - at))); if (k == std::string::npos) break; at = k + 3; }
        if (f[0] == "quiz" && f.size() >= 7) gLadyQuiz.push_back({f[1], {f[2], f[3], f[4]}, f[5], f[6]});
        else if (f[0] == "favor" && f.size() >= 4) { LadyFavor fv; fv.thing = f[1]; fv.prize = f[3]; for (const auto& it : Split(f[2], ',')) fv.items.push_back(Trim(it)); gLadyFavor.push_back(fv); }
    }
}
struct Outbreak { const char* map; const char* species; int level; };
const Outbreak kOutbreaks[] = {{"Route 102", "Seedot", 3}, {"Route 114", "Nuzleaf", 15}, {"Route 117", "Seedot", 13}, {"Route 120", "Seedot", 25}, {"Route 116", "Skitty", 8}};   // (Emerald's tv.c)
const Outbreak& TodaysOutbreak() { uint32_t h = (uint32_t)RaidToday() * 2246822519u; return kOutbreaks[(h >> 13) % 5]; }
int FlagNum(const Game& g, const std::string& key) { for (const auto& f : Flags(g)) if (f.rfind(key, 0) == 0) return std::atoi(f.c_str() + key.size()); return -1; }
std::string FlagStr(const Game& g, const std::string& key) { for (const auto& f : Flags(g)) if (f.rfind(key, 0) == 0) return f.substr(key.size()); return ""; }
void SetFlagVal(Game& g, const std::string& key, const std::string& v) {
    auto& fl = Flags(g);
    for (auto it = fl.begin(); it != fl.end();) it = it->rfind(key, 0) == 0 ? fl.erase(it) : std::next(it);
    fl.insert(key + v);
}
std::string DewfordTrend(const Game& g) {   // the trendy saying in Dewford (yours, once you've set one)
    std::string t = FlagStr(g, "trend|");
    if (!t.empty()) return t;
    return EasyChatWordOf("Conditions", 7) + " " + EasyChatWordOf("Hobbies", 11);
}
// ---- the in-game trades (World/trades.txt, tools/ingame_trades.py)
struct NpcTradeDef { std::string give, get, nick, ot, item, ask, thanks, decline, wrong, after; int iv[6] = {20, 20, 20, 20, 20, 20}; };
std::map<std::string, NpcTradeDef> gTrades;
void LoadTrades() {
    gTrades.clear();
    char* txt = LoadFileText((DataDir() + "World/trades.txt").c_str());
    if (!txt) return;
    std::string s = txt;
    UnloadFileText(txt);
    for (const auto& line : Split(s, '\n')) {
        if (line.empty() || line[0] == '#') continue;
        std::vector<std::string> f;
        size_t at = 0;
        while (true) { size_t k = line.find(" ~ ", at); f.push_back(Trim(line.substr(at, k == std::string::npos ? std::string::npos : k - at))); if (k == std::string::npos) break; at = k + 3; }
        if (f.size() < 12) continue;
        NpcTradeDef d;
        d.give = f[1]; d.get = f[2]; d.nick = f[3]; d.ot = f[4]; d.item = f[6];
        std::sscanf(f[5].c_str(), "%d %d %d %d %d %d", &d.iv[0], &d.iv[1], &d.iv[2], &d.iv[3], &d.iv[4], &d.iv[5]);
        d.ask = f[7]; d.thanks = f[8]; d.decline = f[9]; d.wrong = f[10]; d.after = f[11];
        gTrades[f[0]] = d;
    }
}
void LoadTutors() {
    gTutorCompat.clear(); gTutorKnown.clear();
    char* txt = LoadFileText((DataDir() + "World/tutors.txt").c_str());
    if (!txt) return;
    std::string s = txt;
    UnloadFileText(txt);
    for (const auto& line : Split(s, '\n')) {
        std::string l = Trim(line);
        if (l.empty() || l[0] == '#') continue;
        size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        std::string mv = Trim(l.substr(0, eq));
        for (const auto& sp : Split(l.substr(eq + 1), ',')) {
            std::string n = Trim(sp);
            if (n.empty()) continue;
            if (mv == "covers") gTutorKnown.insert(n); else gTutorCompat[mv].insert(n);
        }
    }
}
bool TutorCanLearn(const OwnedMon& m, const std::string& move) {
    auto it = gTutorCompat.find(move);
    if (it == gTutorCompat.end()) return true;   // (Headbutt: anyone)
    if (it->second.count(m.species)) return true;
    if (gTutorKnown.count(m.species)) return false;
    int si = MonSpecies(m);
    if (si < 0) return false;
    for (const auto& lm : AllSpecies()[si].learnset) if (lm.move.name == move) return true;
    return MonCanLearnTM(m, move);
}
namespace {
// ---- the field moves: someone in your party knows it, and you have the badge
bool PartyKnows(const Profile& p, const std::string& mv, std::string* who = nullptr) {
    for (const auto& m : p.mons)
        if (!m.boxed && std::find(m.moves.begin(), m.moves.end(), mv) != m.moves.end()) { if (who) *who = m.species; return true; }
    return false;
}
int HmIndex(const std::string& mv) { for (int n = 1; n <= 8; n++) if (mv == HmMove(n)) return n; return 0; }
bool CanField(const Game& g, int i, const std::string& mv, std::string* who = nullptr) {
    int n = HmIndex(mv);
    if (n <= 0 || !PartyKnows(WP(g, i), mv, who)) return false;
    return HasBadge(g, FieldBadge(n, gMaps.empty() || g.ow.w[i].map < 0 ? 0 : gMaps[g.ow.w[i].map].region));
}
std::string FieldNeed(const std::string& mv, int region = 0) {   // "You need the Cascade Badge and a Pokemon that knows Cut."
    int n = HmIndex(mv);
    return std::string("You need the ") + FieldBadge(n, region) + " Badge and a Pokemon that knows " + mv + ".";
}

// ---- the story's trainers
int TrainerStage(const OwNpc& n) { return n.kind == "trainer" && !n.args.empty() ? StageIndexByName(n.args[0]) : -1; }
// VS Seeker rematches: "rematch:<beat flag>:<tier>" once it's asked, "<beat flag>#<tier>" once that rematch is won.
int RematchTier(const Game& g, const OwNpc& n) {   // the rematch tier waiting (0 = none)
    if (n.kind != "fight" || n.args.size() < 5) return 0;
    const auto& f = Flags(g);
    for (int t = 1; t <= 4; t++) {
        if (f.count(n.args[4] + "#" + std::to_string(t))) continue;
        return f.count("rematch:" + n.args[4] + ":" + std::to_string(t)) ? t : 0;
    }
    return 0;
}
int RematchesWon(const Game& g, const OwNpc& n) {
    int t = 0;
    while (t < 4 && Flags(g).count(n.args[4] + "#" + std::to_string(t + 1))) t++;
    return t;
}
// A story battle on the walked map is there until it's won, in whatever order the map lets you reach it
// (FireRed's order); the League's rooms go in order.
bool StageBeaten(const Game& g, int s) { return StoryStageNow(g) > s || StageSkipped(s) || Flags(g).count("story:" + Story().stages[s].name) > 0; }
bool TrainerReady(const Game& g, const OwNpc& n) {   // the story is at this battle now (a route trainer: not beaten yet)
    if (n.kind == "fight") return n.args.size() > 4 && (!Flags(g).count(n.args[4]) || RematchTier(g, n) > 0);
    int s = TrainerStage(n);
    if (s < 0 || s >= (int)Story().stages.size()) return false;
    if (StageWalked(s)) {
        if (StageBeaten(g, s)) return false;
        if (!Story().stages[s].IsLeague()) return true;
        return s == 0 || !Story().stages[s - 1].IsLeague() || StageBeaten(g, s - 1);   // the League's rooms, in order
    }
    return s == StoryStageNow(g);
}
bool TrainerBeaten(const Game& g, const OwNpc& n) {
    if (n.kind == "fight") return n.args.size() > 4 && Flags(g).count(n.args[4]) && RematchTier(g, n) == 0;
    int s = TrainerStage(n);
    return s >= 0 && s < (int)Story().stages.size() && StageBeaten(g, s);
}
// Your rival only waits where the story has him now.
int LegendIdOf(const std::string& species) {
    for (int id = 0; id < LEGEND_COUNT; id++) if (Legend(id).species == species) return id;
    return -1;
}
bool NpcHere(const Game& g, const OwNpc& n) {
    if (!CondOk(g, 0, n.ifF, n.unlessF)) return false;
    if (n.kind == "encounter") return n.args.size() < 3 || !Flags(g).count(n.args[2]);   // gone once beaten or caught
    if (n.kind == "legend") { int id = LegendIdOf(n.args.empty() ? "" : n.args[0]); return id >= 0 && LegendCaughtBy(g, id) < 0; }   // there until it's caught
    if (n.kind != "trainer") return true;
    if (gSpriteNames.size() > (size_t)n.sprite && gSpriteNames[n.sprite] == "rival") return TrainerReady(g, n);
    return true;
}

std::string gTrendNow = "";   // (Dewford's trendy saying right now: set when you walk into a map)
std::string gPlayerName = "you";   // {PLAYER} in what people say: the one walking
void Say(OwWalker& w, const std::string& text, const std::string& after = "") {
    std::string t = text;
    for (size_t at; (at = t.find("{PLAYER}")) != std::string::npos;) t.replace(at, 8, gPlayerName);   // FireRed's lines name you
    if (t.find("{TREND}") != std::string::npos) { std::string tr = gTrendNow; for (size_t at; (at = t.find("{TREND}")) != std::string::npos;) t.replace(at, 7, tr); }   // Dewford's trendy saying
    w.msg = Split(t, '|');
    w.msgPage = 0;
    w.after = after;
    PlaySfx(Sfx::MenuMove);
}

void ResetNpcs(int map) {
    if (map < 0) return;
    auto& v = gNpc[map];
    v.clear();
    for (const auto& n : gMaps[map].npcs) v.push_back({n.x, n.y, n.face});
    MapRt& r = gRt[map];
    r.cleared.clear();
    r.boulders.clear();
    const OwMap& m = gMaps[map];
    for (int k = 0; k < (int)m.cells.size(); k++) if (m.cells[k] == 'B') r.boulders.push_back(k);
}

// Online: a map's changes travel as "cleared;boulders" (cell indices), so both screens show the same trees and boulders.
std::string MapRtText(int map) {
    std::string s;
    const MapRt& r = gRt[map];
    for (int k : r.cleared) s += std::to_string(k) + ",";
    s += ";";
    for (int k : r.boulders) s += std::to_string(k) + ",";
    return s;
}
void ShareMap(const Game& g, int map) { if (Online(g) && map >= 0) OnlineSendWorldMap(map, MapRtText(map)); }
// Online: your partner is standing on this map too, so it isn't put back the way it was.
bool PartnerOn(const Game& g, int map) { return Online(g) && g.ow.w[1].here && g.ow.w[1].map == map; }

// The tile as it is right now: cut trees and smashed rocks are gone, boulders are where they've been
// pushed, gates open once their flag is set.
bool gReachMode = false;   // autotest: OwReach's walk (no people, blocks or gates in the way)
char Cell(const Game& g, int i, int map, int x, int y) {
    const OwMap& m = gMaps[map];
    char c = m.At(x, y);
    if (c == '#' && (x < 0 || y < 0 || x >= m.w || y >= m.h)) return c;
    int k = y * m.w + x;
    const MapRt& r = gRt[map];
    if ((c == 'T' || c == 'R') && r.cleared.count(k)) c = '.';
    if (c == 'B') c = '.';
    if (!gReachMode && std::find(r.boulders.begin(), r.boulders.end(), k) != r.boulders.end()) return 'B';
    for (const auto& gt : m.gates) if (gt.x == x && gt.y == y && !HasFlag(g, i, gt.flag) && !gReachMode) return '#';
    for (const auto& sw : m.swaps) if (sw.cell && sw.x == x && sw.y == y && HasFlag(g, i, sw.flag)) return sw.cell;
    return c;
}
bool Walkable(char c) { return c == '.' || c == 'i' || c == '^' || c == 'g' || c == 'c' || c == 'D' || c == 's' || c == 'u' || c == '0' || (c >= '1' && c <= '4'); }   // (s: a slope or steps, 3D maps; u: under the sea, where you can surface; ^: floor with a wall along its top edge, Crystal's caves; i: slippery - you slide on)
bool IsWater(char c) { return c == 'w' || c == 'W' || c == 'd'; }   // (d: deep water - you can Dive there)
int ItemAt(const Game& g, int i, int map, int x, int y, bool hidden) {   // an item not picked up yet (-1 none)
    const OwMap& m = gMaps[map];
    for (size_t k = 0; k < m.items.size(); k++) {
        const auto& it = m.items[k];
        if (it.x != x || it.y != y || it.hidden != hidden || !CondOk(g, i, it.ifF, it.unlessF)) continue;
        if (Flags(g).count("got:" + m.name + ":" + std::to_string(x) + "," + std::to_string(y))) continue;
        return (int)k;
    }
    return -1;
}

int NpcAt(const Game& g, int map, int x, int y) {
    if (gReachMode) return -1;
    const OwMap& m = gMaps[map];
    for (size_t i = 0; i < m.npcs.size(); i++)
        if (NpcHere(g, m.npcs[i]) && gNpc[map][i].x == x && gNpc[map][i].y == y) return (int)i;
    return -1;
}
// Someone (another walker) standing there?
bool WalkerAt(const Game& g, int self, int map, int x, int y) {
    for (int k = 0; k < 2; k++) {
        if (k == self || !Shown(g, k)) continue;
        const OwWalker& o = g.ow.w[k];
        if (o.map == map && o.x == x && o.y == y) return true;
    }
    return false;
}

void SavePos(Game& g) {
    for (int i = 0; i < Driven(g); i++) {
        const OwWalker& w = g.ow.w[i];
        if (w.map < 0) continue;
        if (!g.coop) {
            g.profile.owMap = gMaps[w.map].name;
            g.profile.owX = w.x; g.profile.owY = w.y; g.profile.owFace = w.face;
        } else {
            int who = WhoOf(g, i);
            g.coopSave.owMap[who] = gMaps[w.map].name;
            g.coopSave.owX[who] = w.x; g.coopSave.owY[who] = w.y; g.coopSave.owFace[who] = w.face;
        }
    }
}
void SaveAll(Game& g) {
    SavePos(g);
    if (!g.coop) SaveOrWarnP(g);
    else SaveCoop(g.coopSave);
}

void SendPos(Game& g) {
    if (!Online(g)) return;
    const OwWalker& w = g.ow.w[0];
    OnlineSendWorldPos(true, w.map, w.x, w.y, (w.face & 3) | (w.running ? 4 : 0) | (w.biking ? 8 : 0) | (w.surfing ? 16 : 0));
}

void ResetGates(int map);
const OwMap::DiveSpot* DiveSpotAt(const Game& g, int i, const OwMap& m, int x, int y);
void PlaceAt(Game& g, int i, int map, int x, int y) {
    OwWalker& w = g.ow.w[i];
    bool newMap = map != w.map;
    if (newMap && map >= 0) ResetGates(map);   // (rotating gates are as they were when you come back in)
    bool someoneThere = false;
    for (int k = 0; k < Driven(g); k++) if (k != i && g.ow.w[k].map == map) someoneThere = true;
    w.map = map;
    w.x = w.fromX = w.fx = w.fFromX = x;
    w.y = w.fromY = w.fy = w.fFromY = y;
    w.fFace = w.face;
    w.stepT = 1;
    w.hop = false;
    w.approach = -1;
    if (PartnerOn(g, map)) someoneThere = true;
    if (newMap) { if (!someoneThere) ResetNpcs(map); w.bannerT = 2.2f; }
    if (i == 0) SendPos(g);
}

// A free tile next to (x, y) on a map: behind first (away from `face`), then the sides.
bool FreeBeside(const Game& g, int self, int map, int x, int y, int face, int& ox, int& oy) {
    const OwMap& m = gMaps[map];
    int order[4] = {Opp(face), face == 0 || face == 1 ? 2 : 0, face == 0 || face == 1 ? 3 : 1, face};
    for (int d : order) {
        int nx = x + DX[d], ny = y + DY[d];
        char c = m.At(nx, ny);
        if ((c == '.' || c == 'g') && NpcAt(g, map, nx, ny) < 0 && !WalkerAt(g, self, map, nx, ny)) { ox = nx; oy = ny; return true; }
    }
    return false;
}

// A trainer who can see a walker walks over and challenges them.
bool CheckSight(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    for (size_t k = 0; k < m.npcs.size(); k++) {
        const OwNpc& n = m.npcs[k];
        if (n.sight <= 0 || !NpcHere(g, n) || !TrainerReady(g, n) || WP(g, i).mons.empty()) continue;
        const NpcRt& r = gNpc[w.map][k];
        for (int d = 1; d <= n.sight; d++) {
            int cx = r.x + DX[r.face] * d, cy = r.y + DY[r.face] * d;
            if (cx == w.x && cy == w.y) {
                w.approach = (int)k;
                w.approachT = -0.7f;   // the "!" first
                PlaySfx(Sfx::Status);
                return true;
            }
            if (m.At(cx, cy) == '#' || NpcAt(g, w.map, cx, cy) >= 0) break;
        }
    }
    return false;
}

// A story battle. Solo: straight in. Local co-op: your partner hurries over and you fight together.
// Online: your partner's game is told; it starts when they're there too.
Game& StartSub(Game& g, int i);

// A route trainer's battle: their own team ("Pidgey 9, Rattata 8"), at their levels (nudged a little by yours).
StageDef FightStage(const OwMap& m, const OwNpc& n, int tier = 0) {
    StageDef st;
    st.name = m.name;
    st.trainer = n.args.size() > 0 ? n.args[0] : "Trainer";
    st.say = n.args.size() > 2 ? n.args[2] : "";
    std::vector<std::pair<std::string, int>> team;
    int top = 2;
    bool own = tier > 0 && (int)n.rematch.size() >= tier;   // Emerald's Match Call: the trainer's own rematch team
    for (const auto& e0 : Split(own ? n.rematch[tier - 1] : n.args.size() > 1 ? n.args[1] : "", ',')) {
        std::string e = Trim(e0);
        size_t sp = e.find_last_of(' ');
        if (e.empty() || sp == std::string::npos) continue;
        int lv = std::atoi(e.c_str() + sp + 1) + (own ? 0 : 7 * tier);   // a rematch: stronger each time, and evolved to match
        std::string name = Trim(e.substr(0, sp));
        if (tier > 0 && !own) { int si = SpeciesIndex(name); if (si >= 0) name = AllSpecies()[EvolvedFormAt(si, lv)].name; }
        team.push_back({name, std::max(2, lv)});
        top = std::max(top, lv);
    }
    for (const auto& [sp, lv] : team) st.team.push_back(lv < top ? sp + " -" + std::to_string(top - lv) : sp);
    st.baseLevel = top;
    st.orig = top;   // FireRed's level (regions after your first scale it to your party)
    st.cpu = AILevel::Normal;
    st.catchMode = 0;
    st.reward = false;
    st.outdoor = !m.indoor;
    st.region = m.region;
    if (n.opts.find("scale") != std::string::npos) {   // the Trainer Tower: levelled to your party, all at one level
        st.team.clear();
        for (const auto& [sp, lv] : team) st.team.push_back(sp);
        st.baseLevel = 0;
        st.orig = 0;
        st.levelOffset = 2;
        st.cpu = AILevel::Hard;
    }
    if (n.opts.find("double") != std::string::npos) st.dbl = true;
    return st;
}
// Straight into a battle with your party as it stands (the flash-and-wipe first), like a wild one.
void QuickBattle(Game& t) {
    BeginTeamSelect(t);
    for (int k = 0; k < (int)t.roster[0].size() && (int)t.picks[0].size() < t.teamSize; k++)
        if (t.roster[0][k].hpFrac > 0) t.picks[0].push_back(k);
    t.ready[0] = t.ready[1] = true;
    t.screen = t.encounterBlank ? Screen::Select : Screen::World;
    t.encounterT = ENCOUNTER_WIPE_T;
    PlaySfx(Sfx::Status);
}
void NoteBattleWeather(const Game& g, int i);
void StartRouteFightNpc(Game& g, int i, const OwNpc& n);
void StartRouteFight(Game& g, int i, int npc) { StartRouteFightNpc(g, i, MapOf(g.ow.w[i]).npcs[npc]); }
// (a route trainer, or a team an action made up: the Winstrates, Selphy)
void StartRouteFightNpc(Game& g, int i, const OwNpc& n) {
    NoteBattleWeather(g, i);
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    if (HealthyCount(WP(g, i)) == 0) { Say(w, "Your Pokemon are all too tired to battle!|Rest them at a Pokemon Center first."); return; }
    SaveAll(g);
    Game& t = g.coop ? StartSub(g, i) : g;   // co-op: your own battle, in your half of the screen
    t.owFight = true;
    int tier = RematchTier(g, n);
    t.owStage = FightStage(m, n, tier);
    t.owFightFlag = n.args.size() > 4 ? n.args[4] + (tier > 0 ? "#" + std::to_string(tier) : "") : "";
    t.owWinThen = n.winThen.empty() ? "" : "winthen|" + std::to_string(i) + "|" + n.winThen;   // (screens_story.cpp keeps it as a flag on a win)
    t.stageIdx = -1;
    t.legend = -1;
    t.tower = false;
    t.frontier = 0;
    t.raid = false;
    t.mode = Mode::Story;
    t.rareBattle = false;
    t.ow.wildBattle = false;
    t.encounterBlank = g.coop;
    QuickBattle(t);
}

void StartStoryFight(Game& g, int i, int npc) {
    NoteBattleWeather(g, i);
    OwWalker& w = g.ow.w[i];
    const OwNpc& n = MapOf(w).npcs[npc];
    if (n.kind == "fight") { StartRouteFight(g, i, npc); return; }
    int s = TrainerStage(n);
    if (s < 0) return;
    std::string trainer = Story().stages[s].trainer;
    if (!g.coop) {
        if (HealthyCount(g.profile) == 0) { Say(w, "Your Pokemon are all too tired to battle!|Rest them at a Pokemon Center first."); return; }
        SaveAll(g);
        g.stageIdx = s;
        g.mode = Mode::Story;
        g.rareBattle = false;
        g.screen = Screen::StageIntro;
        return;
    }
    for (int p = 0; p < 2; p++)
        if (HealthyCount(Prof(g, p)) == 0) { Say(w, Prof(g, p).name + "'s Pokemon are all too tired to battle!|Rest them at a Pokemon Center first."); return; }
    if (Online(g)) {
        OnlineSendWorldFight(s, w.map, w.x, w.y, trainer);
        OnlineSetReady(g, 1);
        g.ow.waitFight = true;
        g.ow.waitName = trainer;
        SaveAll(g);
        return;
    }
    // local: player 2 (or 1) runs over - once they're done with what they're doing
    int o = 1 - i;
    if (g.ow.sub[o] || !g.ow.w[o].msg.empty()) { Say(w, WP(g, o).name + " is busy right now!|Battle " + trainer + " together when they're done.", ""); return; }
    int ox, oy;
    if (FreeBeside(g, o, w.map, w.x, w.y, w.face, ox, oy)) { PlaceAt(g, o, w.map, ox, oy); g.ow.w[o].face = w.face; }
    SaveAll(g);
    g.stageIdx = std::min(g.coopSave.storyStage, (int)Story().stages.size() - 1);
    g.mode = Mode::CoopStory;
    g.rareBattle = false;
    g.screen = Screen::StageIntro;
}

void RunAction(Game& g, int i, const std::string& act);
// The Fame Checker learns something ("KEY:n"): kept as "fame:KEY:n"; a word about it if you carry one
void FameNote(Game& g, int i, const std::string& note) {
    std::string f = "fame:" + note;
    if (Flags(g).count(f)) return;
    Flags(g).insert(f);
    if (BagCount(WP(g, i), "Fame Checker") > 0) Toast(g, "The Fame Checker noted something about " + FamePersonName(note.substr(0, note.find(':'))) + ".");
}

void Interact(Game& g, int i, int npc) {
    OwWalker& w = g.ow.w[i];
    const OwNpc& n = MapOf(w).npcs[npc];
    for (const auto& note : FameNotesAt(MapOf(w).name, n.x, n.y)) FameNote(g, i, note);   // (the Fame Checker: what they told you)
    gNpc[w.map][npc].face = Opp(w.face);   // turn to you
    std::string line = n.args.empty() ? "..." : n.args[0];
    if (n.kind == "talk") Say(w, line);
    else if (n.kind == "legend") Say(w, n.args.size() > 1 ? n.args[1] : n.args[0] + ": ...!", "legend|" + n.args[0]);
    else if (n.kind == "heal") {   // the last Pokemon Center you used is where you go if your team faints
        std::string spot = "center:" + MapOf(w).name + ":" + std::to_string(w.x) + ":" + std::to_string(w.y);
        if (!Flags(g).count(spot)) {
            for (auto it = Flags(g).begin(); it != Flags(g).end();) it = it->rfind("center:", 0) == 0 ? Flags(g).erase(it) : std::next(it);
            SetFlag(g, spot);
        }
        Say(w, line, "heal");
    }
    else if (n.kind == "shop") Say(w, line, "shop");
    else if (n.kind == "trainer") {
        if (TrainerReady(g, n) && !n.needItem.empty() && BagCount(WP(g, i), n.needItem) == 0) Say(w, n.needText.empty() ? "..." : n.needText);
        else if (TrainerReady(g, n) && !n.needItem.empty()) Say(w, (n.usedText.empty() ? "" : n.usedText + "|") + (n.args.size() > 1 ? n.args[1] : "Let's battle!"), "battle:" + std::to_string(npc));
        else if (TrainerReady(g, n)) Say(w, n.args.size() > 1 ? n.args[1] : "Let's battle!", "battle:" + std::to_string(npc));
        else if (TrainerBeaten(g, n)) Say(w, n.args.size() > 2 ? n.args[2] : "You're strong!");
        else Say(w, "Let's battle another time!");
    } else if (n.kind == "fight") {   // a route trainer: name|team|before|after|flag
        if (TrainerReady(g, n)) Say(w, n.args.size() > 2 ? n.args[2] : "Let's battle!", "battle:" + std::to_string(npc));
        else Say(w, n.args.size() > 3 ? n.args[3] : "You're strong!");
    } else if (n.kind == "give") {    // text|item|n|flag|after: a gift, once
        if (n.args.size() >= 4 && !HasFlag(g, i, n.args[3])) Say(w, n.args[0], "give|" + n.args[1] + "|" + n.args[2] + "|" + n.args[3]);
        else Say(w, n.args.size() > 4 ? n.args[4] : line);
    } else {   // anything else is an action of its own (exchange, quiz, raid, tower...)
        std::string act = n.kind;
        for (const auto& x : n.args) act += "|" + x;
        RunAction(g, i, act);
    }
}

void Warp(Game& g, int i, const std::vector<std::string>& a) {
    if (a.size() < 4) return;
    int m = MapIndex(a[1]);
    if (m < 0) return;
    if (a.size() > 4 && !a[4].empty()) g.ow.w[i].face = FaceOf(a[4].c_str());   // which way you face as you come in
    g.ow.w[i].surfing = gMaps[m].underwater;   // (a way in under the sea: you're still swimming)
    if (gMaps[m].underwater) g.ow.w[i].biking = false;
    PlaceAt(g, i, m, std::atoi(a[2].c_str()), std::atoi(a[3].c_str()));
    g.ow.w[i].fadeT = 0.35f;
    PlaySfx(Sfx::Door);
    SavePos(g);
}

// Co-op: this walker's player gets a little solo game of their own (a wild battle, the Healing
// Center, the Mart) - in their half of the screen, or the whole screen online.
Game& StartSub(Game& g, int i) {
    int who = WhoOf(g, i);
    auto s = std::make_shared<Game>();
    s->customCount = g.customCount;
    s->t = g.t;
    s->profile = Prof(g, who);
    s->profile.owFlags.clear();   // (it collects what happens in this battle for the co-op save)
    s->coop = false;
    if (g.coop) {   // the roaming legendaries are the co-op save's: they come along (and go back after)
        s->coopSub = true;
        s->subRoamKeep = s->profile.roam;
        s->profile.roam = g.coopSave.roam;
    }
    s->mode = Mode::Story;
    s->ow.active = true;   // its "back to the hub" is the end of it
    s->ow.w[0].map = 0;
    s->ow.inputPlayer = LocalCoop(g) ? who : -1;
    s->screen = Screen::World;
    g.ow.sub[i] = s;
    return *s;
}
bool AnySub(const Game& g) { return g.ow.sub[0] || g.ow.sub[1]; }

void TryStep(Game& g, int i, int dir);
void WildEncounter(Game& g, int i, const std::string& method = "walk");
void StaticEncounter(Game& g, int i, const std::string& species, int level, const std::string& flag);
std::string BugJudging(Game& g);

std::string PocketHint(const std::string& item) {
    const ItemDef* d = FindItem(item);
    if (!d) return "You put it in your Bag.";
    if (IsHM(*d)) return "Teach it to a Pokemon from the menu's POKEMON. It's never used up.";
    if (d->fx == ItemFx::KeyItem) return "You put it in the Key Items pocket.";
    return "You put it in your Bag.";
}

// A field move on the tile in front of you (or the whole map: Strength, Flash).
void UseField(Game& g, int i, const std::string& mv) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    int fx = w.x + DX[w.face], fy = w.y + DY[w.face];
    std::string who;
    if (!CanField(g, i, mv, &who)) { Say(w, FieldNeed(mv, MapRegionOf(g, i))); return; }
    PlaySfx(Sfx::Buff);
    if (mv == "Cut" || mv == "Rock Smash") {
        gRt[w.map].cleared.insert(fy * m.w + fx);
        ShareMap(g, w.map);
        if (mv == "Rock Smash" && WildAreaHasMethod(m.wild, "rock-smash") && SimRand(0, 99) < 30)
            Say(w, who + " used Rock Smash!|A wild Pokemon was hiding under the rock!", "wild|rock-smash");
        else Say(w, who + (mv == "Cut" ? " used Cut! The tree was cut down." : " used Rock Smash! The rock crumbled."));
    } else if (mv == "Strength") {
        w.strengthMap = w.map;
        Say(w, who + " used Strength!|" + who + "'s Strength made it possible to move boulders around!");
    } else if (mv == "Flash") {
        w.flashMap = w.map;
        Say(w, who + " used Flash! The cave lit up!");
    } else if (mv == "Surf") {
        w.surfing = true;
        w.biking = false;
        Say(w, who + " used Surf! You hopped on its back.", "surfstep");
    } else if (mv == "Dive") {   // down to the sea floor (or back up): the same spot on the map below, or Emerald's fixed one
        const OwMap& here = MapOf(w);
        bool down = !here.underwater;
        std::string to = down ? here.diveTo : here.emergeTo;
        int tx = down ? here.diveX : here.emergeX, ty = down ? here.diveY : here.emergeY;
        if (!down)
            for (const auto& e : here.emergeAt) if (e.x == w.x && e.y == w.y) { to = e.map; tx = e.tx; ty = e.ty; }
        if (down)
            if (const auto* d = DiveSpotAt(g, i, here, w.x, w.y)) { to = d->map; tx = d->tx; ty = d->ty; }
        if (tx < 0) { tx = w.x; ty = w.y; }
        if (MapIndex(to) < 0) return;
        int face = w.face;
        Warp(g, i, {"warp", to, std::to_string(tx), std::to_string(ty)});
        w.face = face;
        w.surfing = true;   // (under the sea you swim on your Pokemon too)
        w.biking = false;
        Say(w, down ? who + " used Dive! You dove deep under the sea." : who + " used Dive! You came back up to the surface.");
    } else if (mv == "Waterfall") {
        int y = w.y;
        while (Cell(g, i, w.map, w.x, y - 1) == 'W') y--;
        if (IsWater(Cell(g, i, w.map, w.x, y - 1))) y--;
        if (y != w.y) { w.fromX = w.x; w.fromY = y; w.y = y; w.fx = w.x; w.fy = y + 1; w.fFromX = w.fx; w.fFromY = w.fy; w.stepT = 1; w.fadeT = 0.3f; }
        Say(w, who + " used Waterfall! You climbed the waterfall.");
    }
}

// Fishing with a rod, facing the water.
// Emerald's Feebas: six of Route 119's water tiles, picked by Dewford's trendy saying (a new saying, new tiles)
bool FeebasTile(const Game& g, const OwMap& m, int x, int y) {
    if (m.name != "Route 119" || m.At(x, y) != 'w') return false;
    static std::string seedFor;
    static std::set<int> tiles;
    std::string seed = DewfordTrend(g);
    if (seed != seedFor || tiles.empty()) {
        seedFor = seed;
        tiles.clear();
        std::vector<int> water;
        for (int yy = 0; yy < m.h; yy++) for (int xx = 0; xx < m.w; xx++) if (m.At(xx, yy) == 'w') water.push_back(yy * m.w + xx);
        uint32_t h = 2166136261u;
        for (char c : seed) h = (h ^ (uint8_t)c) * 16777619u;
        for (int n = 0; n < 6 && !water.empty(); n++) {
            h = h * 1103515245u + 12345u;
            tiles.insert(water[(h >> 8) % water.size()]);
        }
    }
    return tiles.count(y * m.w + x) > 0;
}

void Fish(Game& g, int i, const std::string& rod) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    std::string method = rod == "Old Rod" ? "old-rod" : rod == "Good Rod" ? "good-rod" : "super-rod";
    PlaySfx(Sfx::MenuOk);
    w.fishing = true;
    w.fishT = 0;
    if (FeebasTile(g, m, w.x + DX[w.face], w.y + DY[w.face]) && SimRand(0, 99) < 50) {   // Route 119's six secret tiles: Feebas, half the time
        SetWildOverride(SpeciesIndex("Feebas"));
        Say(w, "You cast the " + rod + "...|Oh! A bite!", "wild|super-rod");   // (the route's fishing levels, whichever rod)
        return;
    }
    if (!WildAreaHasMethod(m.wild, method) || SimRand(0, 99) >= 75) Say(w, "You cast the " + rod + "...|Not even a nibble...");
    else Say(w, "You cast the " + rod + "...|Oh! A bite!", "wild|" + method);
}

void Ask(OwWalker& w, const std::string& text, std::vector<std::pair<std::string, std::string>> choices);
void TrashCan(Game& g, int i, const std::string& flag);
// The Raid Den, the Battle Tower's counter and a legendary in its lair: the same battles as the menus.
// Online, both of you have to agree, so it opens that page of the co-op menu.
void WorldChallenge(Game& g, int i, const std::vector<std::string>& a) {
    OwWalker& w = g.ow.w[i];
    const std::string& k = a[0];
    bool online = Online(g);
    auto tired = [&]() {
        for (int p = 0; p < (g.coop ? 2 : 1); p++)
            if ((!online || p == g.on.me) && HealthyCount(g.coop ? Prof(g, p) : g.profile) == 0) {
                Say(w, (g.coop ? Prof(g, p).name + "'s" : std::string("Your")) + " Pokemon are all too tired to battle!|Rest them at a Pokemon Center first.");
                return true;
            }
        return false;
    };
    if (k == "raid") {
        int day = g.coop ? g.coopSave.raidDay : g.profile.raidDay;
        if (day == RaidToday()) { Say(w, "The den is quiet... The raid boss has gone for today.|A new one will appear tomorrow!"); return; }
        if (tired()) return;
        SaveAll(g);
        if (online) { g.hubSel = 12; g.screen = Screen::CoopHub; return; }
        if (g.coop) StartCoopRaid(g); else StartRaid(g);
        return;
    }
    if (k == "frontier" && a.size() > 1) {   // frontier|n: a Battle Frontier facility's receptionist
        int fac = std::atoi(a[1].c_str());
        const FrontierInfo& F = Frontier(fac);
        bool johto = fac >= 8 && fac <= 12;
        if (!BattleTent(fac) && !HasFlag(g, i, johto ? "done:Johto" : "done:Hoenn")) { Say(w, std::string("The Battle Frontier is open to Trainers who've conquered ") + (johto ? "Johto" : "Hoenn") + "'s Pokemon League."); return; }
        if (online) { Say(w, std::string("Welcome to the ") + F.name + "!|Playing online, the challenge for two is the Battle Tower in your co-op menu."); return; }
        int best = 0, runs = 0, n = FrontierStreak(g, fac, &best, &runs);
        bool silver = HasFlag(g, i, std::string("symbol:") + F.symbol + ":silver"), gold = HasFlag(g, i, std::string("symbol:") + F.symbol + ":gold");
        std::string rec = BattleTent(fac) ? (n > 0 ? TextFormat("You've won %d of 3. Ready for battle %d?", n, n + 1) : "Would you like to take the challenge?")
                        : n > 0 ? TextFormat("You're on a %d-win streak (record %d). Carry on?", n, best) : best > 0 ? TextFormat("Your record here: %d wins in a row. Take the challenge?", best) : "Would you like to take the challenge?";
        std::string sym = gold ? std::string("|You hold the ") + F.symbol + " Symbol in gold!" : silver ? std::string("|You hold the ") + F.symbol + " Symbol in silver." : "";
        if (fac == 11) {   // the Castle: its points, and what they buy
            int cp = CastlePoints(g);
            rec += TextFormat("|You have %d Castle Points.", cp);
            Ask(w, std::string("Welcome to the ") + F.name + "!|" + F.rule + sym + "|" + rec,
                {{"Take the challenge", "frontiergo|" + a[1]}, {"Restore my team (5 CP)", "castleheal"}, {"Not now", ""}});
            return;
        }
        Ask(w, std::string("Welcome to the ") + F.name + "!|" + F.rule + sym + "|" + rec, {{"Take the challenge", "frontiergo|" + a[1]}, {"Not now", ""}});
        return;
    }
    if (k == "castleheal") {
        int cp = CastlePoints(g);
        if (cp < 5) { Say(w, TextFormat("I'm sorry - that takes 5 Castle Points, and you have %d.", cp)); return; }
        SetCastlePoints(g, cp - 5);
        for (int p = 0; p < (g.coop ? 2 : 1); p++) for (auto& m : (g.coop ? Prof(g, p) : g.profile).mons) m.hp = 1;
        PlaySfx(Sfx::CenterHeal);
        SaveAll(g);
        Say(w, TextFormat("Your Pokemon are fully restored! (%d Castle Points left)", cp - 5));
        return;
    }
    if ((k == "frontiergo" || k == "pikedoor") && a.size() > 1) {
        int fac = std::atoi(a[1].c_str());
        int best = 0, runs = 0, n = FrontierStreak(g, fac, &best, &runs);
        if (tired()) return;
        if (fac == 11 && HealthyCount(g.profile) == 0) { Say(w, "Your Pokemon can't battle! Restore them here with Castle Points, or at a Pokemon Center."); return; }
        if (fac == 6 && k == "frontiergo" && !FrontierBrainAt(fac, n + 1)) {   // the Pike: three doors in every room
            Ask(w, TextFormat("Room %d. Three doors stand before you... Which one?", n + 1),
                {{"Left door", "pikedoor|6|0"}, {"Middle door", "pikedoor|6|1"}, {"Right door", "pikedoor|6|2"}, {"Leave for now", ""}});
            return;
        }
        if (k == "pikedoor") {   // what's behind it (a battle most of the time)
            uint32_t h = TowerSeedFor(g) ^ ((uint32_t)(n + 1) * 2654435761u) ^ ((uint32_t)std::atoi(a.size() > 2 ? a[2].c_str() : "0") * 40503u) ^ ((uint32_t)runs * 97u);
            h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12;
            int r = (int)(h % 10);
            if (r >= 7) {   // no battle: a rest, or an empty room - on to the next
                SetFrontierStreak(g, fac, n + 1, std::max(best, n + 1), runs);
                SaveAll(g);
                if (r == 7) { for (int p = 0; p < (g.coop ? 2 : 1); p++) for (auto& m : (g.coop ? Prof(g, p) : g.profile).mons) m.hp = 1; PlaySfx(Sfx::CenterHeal); }
                Say(w, (r == 7 ? std::string("A kind woman was waiting in the room. She restored your Pokemon to full health!") : std::string("The room is empty... Lucky!")) +
                       TextFormat("|Room %d cleared.", n + 1), "frontiergo|6");
                return;
            }
        }
        SaveAll(g);
        BeginFrontierBattle(g, fac);
        return;
    }
    if (k == "tower") {
        if (!KantoDone(g)) { Say(w, "The Battle Tower is open to Trainers who've beaten the Pokemon League."); return; }
        if (tired()) return;
        SaveAll(g);
        if (g.coop) {
            g.hubTower = true; g.hubSel = 0;
            if (online) { g.screen = Screen::CoopHub; return; }
            BeginTowerFloor(g, g.coopSave.towerFloor + 1, TowerSeedFor(g));
        } else BeginTowerFloor(g, g.profile.towerFloor + 1, TowerSeedFor(g));
        return;
    }
    // a legendary: legend|Species
    int id = LegendIdOf(a.size() > 1 ? a[1] : "");
    std::string why;
    if (id < 0) return;
    if (!LegendOpen(g, id, &why)) { Say(w, why.empty() ? "It doesn't seem interested in battling right now." : why); return; }
    if (tired()) return;
    SaveAll(g);
    if (online) { g.legendSel = id; OpenLegends(g); return; }
    BeginLegend(g, id);
}
// ---- Hoenn's berry trees (Emerald): planted, they sprout, grow, flower and bear berries, a stage every few hours
// (an Emerald hour is 100 s of the story clock). Watering with the Wailmer Pail in a stage means more berries.
// A tree's state is a flag in the save ("bt:<id>|Berry|planted at|stages watered"), so co-op shares the trees.
constexpr int BERRY_HOUR_SEC = 100;
struct TreeState { std::string berry; long long planted = 0; int water = 0; bool set = false; };
long long StorySeconds(const Game& g) { return (long long)StoryClock(g) * HALF_DAY_SEC + (g.coop ? g.coopSave.clockSec : g.profile.clockSec); }
const OwMap::BerryTree* TreeById(int id) {
    for (const auto& m : gMaps) for (const auto& t : m.berries) if (t.id == id) return &t;
    return nullptr;
}
TreeState TreeOf(const Game& g, const OwMap::BerryTree& t) {
    TreeState s;
    std::string key = "bt:" + std::to_string(t.id) + "|";
    for (const auto& f : Flags(g))
        if (f.rfind(key, 0) == 0) {
            auto p = Split(f.substr(key.size()), '|');
            s.set = true;
            s.berry = p[0];
            if (p.size() > 1) s.planted = std::atoll(p[1].c_str());
            if (p.size() > 2) s.water = std::atoi(p[2].c_str());
            return s;
        }
    if (t.berry != "Soil") { s.berry = t.berry; s.planted = -1000000000LL; }   // untouched: Emerald's own berries, ripe
    return s;
}
int TreeStage(const Game& g, const TreeState& s) {   // -1 bare soil, 0 planted, 1 sprout, 2 growing, 3 flowering, 4 berries
    int bi = BerryIndexOf(s.berry);
    if (s.berry.empty() || bi < 0) return -1;
    long long dur = (long long)BerryAt(bi).hours * BERRY_HOUR_SEC;
    return (int)std::min<long long>(4, std::max<long long>(0, (StorySeconds(g) - s.planted) / std::max(1LL, dur)));
}
int TreeYield(const TreeState& s) {
    int bi = BerryIndexOf(s.berry);
    if (bi < 0) return 0;
    const BerryInfo& b = BerryAt(bi);
    int w = 0;
    for (int k = 0; k < 4; k++) w += (s.water >> k) & 1;
    return b.minYield + ((b.maxYield - b.minYield) * w + 2) / 4;
}
void SetTree(Game& g, int id, const std::string& berry, long long planted, int water) {
    std::string key = "bt:" + std::to_string(id) + "|";
    auto& f = Flags(g);
    for (auto it = f.begin(); it != f.end();) it = it->rfind(key, 0) == 0 ? f.erase(it) : std::next(it);
    SetFlag(g, key + berry + "|" + std::to_string(planted) + "|" + std::to_string(water));
}
// ---- HeartGold's Apricorn trees: one Apricorn a day each ("apri:<tree>|<day>" in the flags); Kurt makes balls of them
long long ApriDay(const Game& g) { return StorySeconds(g) / (2 * HALF_DAY_SEC); }
bool ApriPicked(const Game& g, const std::string& id) {
    std::string key = "apri:" + id + "|";
    for (const auto& f : Flags(const_cast<Game&>(g))) if (f.rfind(key, 0) == 0) return std::atoll(f.c_str() + key.size()) >= ApriDay(g);
    return false;
}
const char* ApriBall(const std::string& color) {
    static const std::pair<const char*, const char*> k[] = {{"Red", "Level Ball"}, {"Yellow", "Moon Ball"}, {"Blue", "Lure Ball"}, {"Green", "Friend Ball"},
                                                            {"Pink", "Love Ball"}, {"White", "Fast Ball"}, {"Black", "Heavy Ball"}};
    for (const auto& [c, b] : k) if (color == c) return b;
    return "Poke Ball";
}
int TreeAt(const Game& g, int i, int map, int x, int y) {
    const auto& v = gMaps[map].berries;
    for (size_t k = 0; k < v.size(); k++) if (v[k].x == x && v[k].y == y && HasFlag(g, i, v[k].ifF)) return (int)k;
    return -1;
}
bool BerryTreeHere(Game& g, int i, int fx, int fy) {   // A at a berry tree: pick, water, or plant
    OwWalker& w = g.ow.w[i];
    int k = TreeAt(g, i, w.map, fx, fy);
    if (k < 0) return false;
    const auto& t = gMaps[w.map].berries[k];
    TreeState s = TreeOf(g, t);
    int st = TreeStage(g, s);
    std::string id = std::to_string(t.id);
    bool pail = BagCount(WP(g, i), "Wailmer Pail") > 0;
    if (st < 0) {
        std::vector<std::pair<std::string, std::string>> ch;
        for (int b = 0; b < BerryCount() && ch.size() < 8; b++)
            if (BagCount(WP(g, i), BerryAt(b).name) > 0) ch.push_back({std::string(BerryAt(b).name) + " (" + std::to_string(BagCount(WP(g, i), BerryAt(b).name)) + ")", "berryplant|" + id + "|" + BerryAt(b).name});
        if (ch.empty()) { Say(w, "It's soft, loamy soil.|(If you had a Berry, you could plant it here.)"); return true; }
        ch.push_back({"Cancel", ""});
        Ask(w, "It's soft, loamy soil. Want to plant a Berry?", ch);
        return true;
    }
    std::string nm = s.berry + " Berry";
    if (st >= 4) {
        int n = TreeYield(s);
        Ask(w, "It's a tree with " + std::to_string(n) + " " + s.berry + (n == 1 ? " Berry" : " Berries") + " on it! Want to pick " + (n == 1 ? "it" : "them") + "?",
            {{"Pick", "berrypick|" + id}, {"No", ""}});
        return true;
    }
    static const char* kStage[4] = {"One " , "A sprout from ", "A ", "This "};
    std::string text = st == 0 ? "One " + nm + " was planted here." : st == 1 ? nm + " has sprouted." :
                       st == 2 ? "This " + nm + " plant is growing taller." : "These " + nm + " flowers are blooming very beautifully.";
    (void)kStage;
    if (pail && !((s.water >> st) & 1)) Ask(w, text + " Want to water it with the Wailmer Pail?", {{"Water it", "berrywater|" + id}, {"No", ""}});
    else Say(w, text + (pail ? "|The soil is already damp." : ""));
    return true;
}
void RunAction(Game& g, int i, const std::string& act) {
    OwWalker& w = g.ow.w[i];
    auto a = Split(act, '|');
    const std::string& k = a[0];
    if (k == "warp") Warp(g, i, a);
    else if (k == "msg") Say(w, act.substr(4));
    else if (k == "heal" || k == "shop") {
        SaveAll(g);
        Game& t = g.coop ? StartSub(g, i) : g;
        if (k == "heal") OpenHealingCenter(t);
        else { t.shopSel = 0; t.shopTab = 0; t.relearnMon = -1; t.screen = Screen::Shop; }
    }
    else if (k == "relearner") Say(w, a.size() > 1 && !a[1].empty() ? act.substr(10) : "I'm the Move Relearner. I can make Pokemon remember moves they've forgotten.", "relearnopen");
    else if (k == "relearnopen") {   // the shop's Move Relearner tab
        SaveAll(g);
        Game& t = g.coop ? StartSub(g, i) : g;
        t.shopSel = 0; t.shopTab = 4; t.relearnMon = -1; t.screen = Screen::Shop;
    }
    else if (k == "shopmsg" || k == "healmsg") Say(w, act.substr(k.size() + 1), k == "shopmsg" ? "shop" : "heal");
    else if (k == "daycare") Say(w, a.size() > 1 && !a[1].empty() ? a[1] : "I'm the Day Care man. Shall we raise a Pokemon for you?", "daycareopen");
    else if (k == "daycareopen") {
        SaveAll(g);
        Game& t = g.coop ? StartSub(g, i) : g;
        OpenDaycare(t, 0, Screen::World);
    }
    else if (k == "headbutt" && a.size() > 1) {   // headbutt|<its wild area>: a tree with Pokemon in it (HeartGold)
        std::string who;
        if (!PartyKnows(WP(g, i), "Headbutt", &who)) { Say(w, "It's a big tree. A Pokemon might be living in it...|(Teach a Pokemon Headbutt to shake it.)"); return; }
        Ask(w, "It's a big tree. A Pokemon might be living in it...|Want to use Headbutt?", {{"Yes", "headbuttgo|" + a[1]}, {"No", ""}});
    }
    else if (k == "headbuttgo" && a.size() > 1) {
        std::string who;
        PartyKnows(WP(g, i), "Headbutt", &who);
        PlaySfx(Sfx::Thud);
        if (SimRand(0, 99) < 55 && WildAreaHasMethod(a[1], "headbutt")) { Say(w, who + " did a Headbutt!|A wild Pokemon fell out of the tree!", "wild|headbutt@" + a[1]); return; }
        Say(w, who + " did a Headbutt!|...Nothing fell out.");
    }
    else if (k == "tutorpick" && a.size() > 1) {   // tutorpick|Move,Move,...|what they say: you choose which (Cape Brink's starter moves)
        std::vector<std::pair<std::string, std::string>> ch;
        for (const auto& mv : Split(a[1], ',')) ch.push_back({Trim(mv), "tutor|" + Trim(mv)});
        ch.push_back({"Never mind", ""});
        std::string intro;
        for (size_t q = 2; q < a.size(); q++) if (!a[q].empty()) intro += a[q] + "|";
        Ask(w, intro + "Which move would you like to learn?", ch);
    }
    else if (k == "deleter") {   // the Move Deleter: a Pokemon forgets a move (it has to keep one)
        std::vector<std::pair<std::string, std::string>> ch;
        const Profile& P = WP(g, i);
        for (int m = 0; m < (int)P.mons.size() && ch.size() < 6; m++)
            if (!P.mons[m].boxed && P.mons[m].moves.size() > 1) ch.push_back({P.mons[m].nick.empty() ? P.mons[m].species : P.mons[m].nick, "deletepick|" + std::to_string(m)});
        if (ch.empty()) { Say(w, "Uh... Oh, yes, I'm the Move Deleter.|Your Pokemon each know just one move - I can't make them forget that!"); return; }
        ch.push_back({"Never mind", ""});
        Ask(w, "Uh... Oh, yes, I'm the Move Deleter. I can make Pokemon forget their moves.|Which Pokemon should forget a move?", ch);
    }
    else if (k == "deletepick" && a.size() > 1) {
        const Profile& P = WP(g, i);
        int m = std::atoi(a[1].c_str());
        if (m < 0 || m >= (int)P.mons.size()) return;
        std::vector<std::pair<std::string, std::string>> ch;
        for (int q = 0; q < (int)P.mons[m].moves.size(); q++) ch.push_back({P.mons[m].moves[q], "deletego|" + a[1] + "|" + std::to_string(q)});
        ch.push_back({"Never mind", ""});
        Ask(w, "Which move should it forget?", ch);
    }
    else if (k == "deletego" && a.size() > 2) {
        Profile& P = WP(g, i);
        int m = std::atoi(a[1].c_str()), q = std::atoi(a[2].c_str());
        if (m < 0 || m >= (int)P.mons.size() || q < 0 || q >= (int)P.mons[m].moves.size() || P.mons[m].moves.size() <= 1) return;
        OwnedMon& om = P.mons[m];
        std::string old = om.moves[q];
        om.moves.erase(om.moves.begin() + q);
        if (std::find(om.forgotten.begin(), om.forgotten.end(), old) == om.forgotten.end()) om.forgotten.push_back(old);
        SaveAll(g);
        PlaySfx(Sfx::MenuOk);
        Say(w, std::string("Hm! ") + (om.nick.empty() ? om.species : om.nick) + " has forgotten " + old + " completely.|(A Move Relearner can teach it again.)");
    }
    else if (k == "tutor" && a.size() > 1) {   // tutor|Move|what they say: teaches it to one of your party (World/tutors.txt: who can)
        std::vector<std::pair<std::string, std::string>> ch;
        const Profile& P = WP(g, i);
        int cannot = 0;
        for (int m = 0; m < (int)P.mons.size(); m++) {
            const OwnedMon& om = P.mons[m];
            if (om.boxed || std::find(om.moves.begin(), om.moves.end(), a[1]) != om.moves.end()) continue;
            if (!TutorCanLearn(om, a[1])) { cannot++; continue; }
            ch.push_back({om.nick.empty() ? om.species : om.nick, "tutorgo|" + a[1] + "|" + std::to_string(m)});
            if (ch.size() >= 5) break;
        }
        std::string intro;   // (what they say: the rest, one page each)
        for (size_t q = 2; q < a.size(); q++) if (!a[q].empty()) intro += a[q] + "|";
        if (ch.empty()) { Say(w, intro + (cannot ? "Hmm... none of your Pokemon can learn " + a[1] + "." : "Hmm... all of your Pokemon already know " + a[1] + ".")); return; }
        ch.push_back({"Never mind", ""});
        Ask(w, intro + "Which Pokemon should learn " + a[1] + "?", ch);
    }
    else if (k == "tutorgo" && a.size() > 2) {
        Profile& P = WP(g, i);
        int m = std::atoi(a[2].c_str());
        if (m < 0 || m >= (int)P.mons.size()) return;
        OwnedMon& om = P.mons[m];
        std::string nm = om.nick.empty() ? om.species : om.nick;
        if (om.moves.size() < 4) {
            om.moves.push_back(a[1]);
            SaveAll(g);
            PlaySfx(Sfx::LevelUp);
            Say(w, nm + " learned " + a[1] + "!");
            return;
        }
        std::vector<std::pair<std::string, std::string>> ch;
        for (int k2 = 0; k2 < (int)om.moves.size(); k2++) ch.push_back({om.moves[k2], "tutorforget|" + a[1] + "|" + a[2] + "|" + std::to_string(k2)});
        ch.push_back({"Don't learn it", ""});
        Ask(w, nm + " already knows four moves. Which one should it forget for " + a[1] + "?", ch);
    }
    else if (k == "tutorforget" && a.size() > 3) {
        Profile& P = WP(g, i);
        int m = std::atoi(a[2].c_str()), mv = std::atoi(a[3].c_str());
        if (m < 0 || m >= (int)P.mons.size() || mv < 0 || mv >= (int)P.mons[m].moves.size()) return;
        OwnedMon& om = P.mons[m];
        std::string old = om.moves[mv];
        om.moves[mv] = a[1];
        if (std::find(om.forgotten.begin(), om.forgotten.end(), old) == om.forgotten.end()) om.forgotten.push_back(old);
        SaveAll(g);
        PlaySfx(Sfx::LevelUp);
        Say(w, std::string("1, 2 and... Poof! ") + (om.nick.empty() ? om.species : om.nick) + " forgot " + old + " and learned " + a[1] + "!");
    }
    else if (k == "ttower" || k == "ttgo" || k == "ttquit" || k == "ttfinish" || k == "thill" || k == "thgo" || k == "thquit" || k == "thfinish" || k == "thup") {
        // the Trainer Tower (FireRed) and the Trainer Hill (Emerald): their challenges, against the clock
        bool hill = k[1] == 'h';
        const std::string P = hill ? "th:" : "tt:", start = hill ? "thstart|" : "ttstart|", bestKey = hill ? "thbest:" : "ttbest:";
        auto& fl = Flags(g);
        auto clear = [&]() { for (auto it = fl.begin(); it != fl.end();) it = (it->rfind(P, 0) == 0 || it->rfind(start, 0) == 0) ? fl.erase(it) : std::next(it); };
        auto flagNum = [&](const std::string& key) { for (const auto& f : fl) if (f.rfind(key, 0) == 0) return std::atoll(f.c_str() + key.size()); return -1LL; };
        static const char* kTypes[2][4] = {{"single", "double", "knockout", "mixed"}, {"normal", "variety", "unique", "expert"}};
        static const char* kNames[2][4] = {{"Single Battle", "Double Battle", "Knockout", "Mixed"}, {"Normal", "Variety", "Unique", "Expert"}};
        static const char* kPrize[4] = {"Metal Coat", "Mental Herb", "Choice Band", "King's Rock"};
        static const char* kHillTop[4] = {"TM049 Sunny Day", "Elixir", "TM111 Giga Drain", "TM058 Brick Break"};   // (Emerald's prize under 12 minutes)
        const char* const* types = kTypes[hill];
        const char* const* names = kNames[hill];
        int type = -1;
        for (int t = 0; t < 4; t++) if (fl.count(P + types[t])) type = t;
        auto clock = [](long long s) { return std::string(TextFormat("%lld:%02lld", s / 60, s % 60)); };
        if (k == "ttower" || k == "thill") {
            if (fl.count(P + "on") && type >= 0) {
                Ask(w, std::string("You're taking the ") + names[type] + " challenge. The clock is running!|Would you like to give up?", {{"Keep going", ""}, {"Give up", hill ? "thquit" : "ttquit"}});
                return;
            }
            std::string best;
            for (int t = 0; t < 4; t++) if (long long b = flagNum(bestKey + types[t] + "|"); b >= 0) best += std::string("|Your best ") + names[t] + " time: " + clock(b) + ".";
            std::string go = hill ? "thgo|" : "ttgo|";
            if (hill)
                Ask(w, "Welcome to the Trainer Hill! Two Trainers wait on each of its four floors.|Beat them all and climb to the roof as fast as you can!" + best + "|Which mode would you like?",
                    {{"Normal", go + "0"}, {"Variety", go + "1"}, {"Unique", go + "2"}, {"Expert", go + "3"}, {"Not now", ""}});
            else
                Ask(w, "Welcome to the Trainer Tower! Trainers from all over wait on every floor.|Beat them all and reach the roof as fast as you can!" + best + "|Which challenge would you like?",
                    {{"Single Battle", go + "0"}, {"Double Battle", go + "1"}, {"Knockout", go + "2"}, {"Mixed", go + "3"}, {"Not now", ""}});
        } else if ((k == "ttgo" || k == "thgo") && a.size() > 1) {
            int t = std::max(0, std::min(3, std::atoi(a[1].c_str())));
            clear();
            fl.insert(P + "on");
            fl.insert(P + types[t]);
            fl.insert(start + std::to_string(StorySeconds(g)));
            SaveAll(g);
            Say(w, std::string("The ") + names[t] + (hill ? " mode" : " challenge") + "! The clock starts... now!|Please go " + (hill ? "up the stairs" : "through the door behind me") + ". Good luck!");
        } else if (k == "thup") {   // the stairs from the lobby: to the first floor of your mode
            if ((!fl.count(P + "on") || type < 0) && !gReachMode) { Say(w, "(Please sign up at the counter for a challenge first.)"); return; }
            if (type < 0) type = 0;
            Warp(g, i, Split(std::string("warp|Trainer Hill 1F (") + names[type] + ")|" + (a.size() > 2 ? a[1] + "|" + a[2] : "2|2") + "|down", '|'));
        } else if (k == "ttquit" || k == "thquit") {
            clear();
            SaveAll(g);
            Say(w, "You've given up the challenge. Please come back anytime!");
        } else {   // the owner on the roof
            if (!fl.count(P + "on") || type < 0) {
                Say(w, hill ? "I'm the owner of the Trainer Hill. Ah, the wind up here...|Come back when you've climbed it as a challenger, and I'll have a prize for you."
                            : "Hello... I am the owner of this Tower...|How the sky soars above this rooftop... Come back when you've climbed it as a challenger.");
                return;
            }
            long long st = flagNum(start);
            long long secs = std::max(0LL, StorySeconds(g) - std::max(0LL, st));
            std::string key = bestKey + types[type] + "|";
            long long best = flagNum(key);
            std::string rec = best < 0 || secs < best ? "|That's a new record!" : "|Your record is " + clock(best) + ".";
            if (best < 0 || secs < best) {
                for (auto it = fl.begin(); it != fl.end();) it = it->rfind(key, 0) == 0 ? fl.erase(it) : std::next(it);
                fl.insert(key + std::to_string(secs));
            }
            clear();
            std::string prize = kPrize[type];
            if (hill) {   // Emerald: the faster, the better
                long long mins = secs / 60;
                prize = mins < 12 ? kHillTop[type] : mins < 13 ? "Ether" : mins < 14 ? "Max Potion" : mins < 16 ? "Revive" : mins < 18 ? "Hyper Potion" : "Great Ball";
            }
            BagAdd(WP(g, i), prize);
            SaveAll(g);
            PlaySfx(Sfx::ItemGet);
            Say(w, std::string(hill ? "I'm the owner of the Trainer Hill. You've climbed all the way up!" : "Hello... I am the owner of this Tower... You've climbed to the top!") +
                   "|Your " + names[type] + " time: " + clock(secs) + "." + rec + "|As a reward, please take this.|You got the " + prize + "!");
        }
    }
    else if (k == "daily" && a.size() > 3) {   // daily|Item|key|what's there once it's taken: one a day (Shoal Cave's salt and shells)
        long long day = StoryClock(g) / 2;
        std::string key = "daily:" + a[2] + "|";
        auto& fl = Flags(g);
        for (const auto& f : fl) if (f.rfind(key, 0) == 0 && std::atoll(f.c_str() + key.size()) == day) { Say(w, a[3]); return; }
        for (auto it = fl.begin(); it != fl.end();) it = it->rfind(key, 0) == 0 ? fl.erase(it) : std::next(it);
        fl.insert(key + std::to_string(day));
        BagAdd(WP(g, i), a[1]);
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, "You found a " + a[1] + "!");
    }
    else if (k == "mirageman") Say(w, HasFlag(g, i, "mirage") ? "Oh! Oh my! I can see Mirage Island today!" : "I can't see Mirage Island today...");   // Pacifidlog's watcher
    else if (k == "shellbell") {   // Shoal Cave's old man: four Shoal Salt and four Shoal Shells make a Shell Bell
        Profile& P = WP(g, i);
        if (BagCount(P, "Shoal Salt") >= 4 && BagCount(P, "Shoal Shell") >= 4) {
            for (int n = 0; n < 4; n++) { BagTake(P, "Shoal Salt"); BagTake(P, "Shoal Shell"); }
            BagAdd(P, "Shell Bell");
            SaveAll(g);
            PlaySfx(Sfx::ItemGet);
            Say(w, "Oh! Four Shoal Salt and four Shoal Shells! I'll make you a Shell Bell right away.|...There! You got the Shell Bell!|"
                   "A Pokemon holding it gets back a little of the damage it deals. Its chime will soothe you.");
        } else if (BagCount(P, "Shoal Salt") + BagCount(P, "Shoal Shell") > 0)
            Say(w, "If you bring me four each of the Shoal Salt and Shoal Shells, I can make you a Shell Bell.|"
                   "(You have " + std::to_string(BagCount(P, "Shoal Salt")) + " Shoal Salt and " + std::to_string(BagCount(P, "Shoal Shell")) + " Shoal Shells.)");
        else
            Say(w, "Are you planning on going in there? The tide rises and falls - high tide from 3 to 9, morning and evening. You can reach different places at low tide.|"
                   "If you bring me four each of the Shoal Salt and Shoal Shells, I can make you a Shell Bell.");
    }
    else if (k == "slots" || k == "voltorbflip") {   // a Game Corner's machines: the slots, HeartGold's Voltorb Flip
        if (g.coop) { Say(w, "(The Game Corner's machines are for playing on your own.)"); return; }
        if (k == "slots" && g.profile.casino <= 0) { Say(w, "It's a slot machine. You need Game Corner coins to play - buy some at the counter."); return; }
        SaveAll(g);
        if (k == "slots") OpenSlotsFromWorld(g); else OpenVoltorbFlip(g);
    }
    else if (k == "islandtickets") {   // the S.S. Tidal's island tours: a ticket for each island, once you're ready for it
        struct Tk { const char* item; const char* why; };
        auto caught = [&](const char* sp) { int id = LegendIdOf(sp); return id >= 0 && LegendCaughtBy(g, id) >= 0; };
        std::string got;
        auto give = [&](const char* item, bool ok) { if (ok && BagCount(WP(g, i), item) == 0) { BagAdd(WP(g, i), item); got += std::string("|You got the ") + item + "!"; } };
        give("Eon Ticket", HasFlag(g, i, "done:Hoenn"));
        give("Old Sea Map", caught("Mewtwo"));
        give("Aurora Ticket", caught("Rayquaza"));
        give("Mystic Ticket", HasFlag(g, i, "done:Johto") && HasFlag(g, i, "done:Hoenn"));
        if (!got.empty()) { SaveAll(g); PlaySfx(Sfx::ItemGet); Say(w, "Hello! The S.S. Tidal also sails to some faraway islands - with the right ticket.|Some mail came for you!" + got + "|Show it at the ferry counter, and off you go!"); return; }
        Say(w, "Hello! The S.S. Tidal also sails to some faraway islands - with the right ticket.|Tickets turn up for Trainers who've done great things: conquering Hoenn's League, catching Mewtwo or Rayquaza, conquering Johto too...");
    }
    else if (k == "abweather" || k == "abwback") {   // Emerald's abnormal weather: Groudon's drought, Kyogre's heavy rain (Terra / Marine Cave)
        struct Loc { const char* id; const char* route; bool marine; int bx, by; };   // where the cave's way in is (and where you come back out)
        static const Loc kLoc[16] = {{"114n", "Route 114", false, 7, 5}, {"114s", "Route 114", false, 6, 47}, {"115w", "Route 115", false, 21, 7},
                                     {"115e", "Route 115", false, 36, 11}, {"116n", "Route 116", false, 59, 14}, {"116s", "Route 116", false, 79, 7},
                                     {"118e", "Route 118", false, 42, 7}, {"118w", "Route 118", false, 9, 7},
                                     {"105n", "Route 105", true, 11, 29}, {"105s", "Route 105", true, 21, 54}, {"125w", "Route 125", true, 9, 17},
                                     {"125e", "Route 125", true, 54, 19}, {"127n", "Route 127", true, 58, 10}, {"127s", "Route 127", true, 62, 31},
                                     {"129w", "Route 129", true, 17, 15}, {"129e", "Route 129", true, 43, 20}};
        auto& fl = Flags(g);
        int cur = -1;
        for (int l = 0; l < 16; l++) if (fl.count(std::string("abw:") + kLoc[l].id)) cur = l;
        if (k == "abwback") {   // out of the cave: back where you came in
            if (cur < 0) cur = 0;
            const Loc& L = kLoc[cur];
            Warp(g, i, {"warp", L.route, std::to_string(L.bx), std::to_string(L.by), "down"});
            if (L.marine) w.surfing = true;
            return;
        }
        int groudon = LegendIdOf("Groudon"), kyogre = LegendIdOf("Kyogre");
        bool gDone = groudon >= 0 && LegendCaughtBy(g, groudon) >= 0, kDone = kyogre >= 0 && LegendCaughtBy(g, kyogre) >= 0;
        if (!HasFlag(g, i, "done:Hoenn")) { Say(w, "We're studying the weather here. Hoenn's weather can change in strange ways..."); return; }
        if (gDone && kDone) {
            for (auto it = fl.begin(); it != fl.end();) it = it->rfind("abw:", 0) == 0 ? fl.erase(it) : std::next(it);
            Say(w, "The weather all over Hoenn has been calm lately. Nothing unusual to report!");
            return;
        }
        if (cur < 0 || (kLoc[cur].marine ? kDone : gDone)) {   // a new report: wherever the one still sleeping stirs
            for (auto it = fl.begin(); it != fl.end();) it = it->rfind("abw:", 0) == 0 ? fl.erase(it) : std::next(it);
            bool marine = gDone ? true : kDone ? false : SimRand(0, 1) == 1;
            cur = (marine ? 8 : 0) + SimRand(0, 7);
            SetFlag(g, std::string("abw:") + kLoc[cur].id);
            SaveAll(g);
        }
        const Loc& L = kLoc[cur];
        Say(w, std::string("Oh, it's you! We've been tracking something odd.|") +
               (L.marine ? "There's been a sudden, terrible downpour on " : "A severe drought has struck ") + L.route + "!|" +
               (L.marine ? "Somewhere out on the sea there, the water is said to be churning..." : "Somewhere there, a cave has opened where there wasn't one before...") +
               "|Could it have something to do with " + (L.marine ? "Kyogre" : "Groudon") + "?");
    }
    else if (k == "dexrate") {   // dexrate|greeting: one of Prof. Oak's aides looks at your Pokedex
        int n = DexCaughtCount(WP(g, i));
        const char* say = n < 10 ? "Just getting started! Catch Pokemon in the tall grass and the caves - the Pokedex fills in as you go."
                        : n < 30 ? "That's a fine start! Try fishing and Surfing too - different Pokemon live on the water."
                        : n < 60 ? "Very good! Trading and evolving will fill in the gaps."
                        : n < 120 ? "Impressive! Prof. Oak will be thrilled when he sees this."
                        : "Incredible! You're a true Pokemon Professor in the making!";
        Say(w, (a.size() > 1 && !a[1].empty() ? a[1] + "|" : std::string()) + TextFormat("Let me see your Pokedex... You've caught %d kinds of Pokemon.|", n) + say);
    }
    else if (k == "alphpuzzle" && a.size() > 1) {   // the Ruins of Alph: a stone panel, a picture in pieces
        if (HasFlag(g, i, "alph:" + a[1])) { Say(w, "The stones are fixed in place. The picture is complete."); return; }
        if (g.coop) { Say(w, "There's a puzzle of stone panels here...|(The puzzle is one Trainer at a time - try it on your own.)"); return; }
        Ask(w, "Patterns appeared on the panels. They're pieces of a picture, all jumbled up...|Try to put it back together?", {{"Yes", "alphgo|" + a[1]}, {"No", ""}});
    }
    else if (k == "alphgo" && a.size() > 1) { SaveAll(g); OpenUnownPuzzle(g, a[1]); }
    else if (k == "safari") {   // the Safari Zone's gate: 500 coins, 30 Safari Balls, 600 steps
        if (g.coop) { Say(w, "Welcome to the Safari Zone!|(The Safari game is one Trainer at a time - play it on your own.)"); return; }
        if (g.ow.safari.on) { Say(w, "Good luck out there! You have " + std::to_string(g.ow.safari.balls) + " Safari Balls and " + std::to_string(g.ow.safari.steps) + " steps left."); return; }
        Ask(w, act.size() > 7 ? act.substr(7) : "Welcome to the Safari Zone!|For just 500 coins, you can catch all the Pokemon you want in the park!|Would you like to join the hunt?",
            {{"Yes (500 coins)", "safarigo"}, {"No thanks", ""}});
    }
    else if (k == "safarigo") {
        if (WP(g, i).coins < 500) { Say(w, "Oops! You don't have enough coins."); return; }
        WP(g, i).coins -= 500;
        auto& run = g.ow.safari;
        run = OwState::SafariRun();
        run.on = true; run.balls = 30; run.steps = 600;
        run.gate = MapOf(w).name; run.gateX = w.x; run.gateY = w.y;
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, "That'll be 500 coins, please!|Here are your 30 Safari Balls.|You have 600 steps. When you run out of balls or steps, the game's over. Good luck!");
    }
    else if (k == "bugcontest") {   // the National Park gate: the Bug-Catching Contest
        auto& run = g.ow.safari;
        if (g.coop) { Say(w, "Today's the Bug-Catching Contest!|(It's one Trainer at a time - play it on your own.)"); return; }
        if (run.on && run.contest) { Ask(w, "Do you want to end the contest now and see the judging?", {{"End it", "bugend"}, {"Keep going", ""}}); return; }
        Ask(w, act.size() > 11 ? act.substr(11) : "Today's the Bug-Catching Contest!|Catch the best Bug-type Pokemon you can in the National Park. You get 20 Park Balls, and 500 steps.|Would you like to enter?",
            {{"Enter the contest", "buggo"}, {"No thanks", ""}});
    }
    else if (k == "buggo") {
        auto& run = g.ow.safari;
        run = OwState::SafariRun();
        run.on = true; run.contest = true; run.balls = 20; run.steps = 500;
        run.gate = MapOf(w).name; run.gateX = w.x; run.gateY = w.y;
        PlaySfx(Sfx::ItemGet);
        Say(w, "Here are 20 Park Balls. You can keep only one bug - catch another and choose which to keep.|The contest ends after 500 steps, or come back here to end it early. Good luck!");
    }
    else if (k == "bugend") {
        auto& run = g.ow.safari;
        std::string j = BugJudging(g);
        run.on = false;
        Say(w, j);
    }
    else if (k == "apricorn" && a.size() > 2) {   // apricorn|tree|Color
        if (ApriPicked(g, a[1])) { Say(w, "It's an Apricorn tree... There's nothing left on it. Come back tomorrow!"); return; }
        std::string key = "apri:" + a[1] + "|";
        auto& f = Flags(g);
        for (auto it = f.begin(); it != f.end();) it = it->rfind(key, 0) == 0 ? f.erase(it) : std::next(it);
        SetFlag(g, key + std::to_string(ApriDay(g)));
        BagAdd(WP(g, i), a[2] + " Apricorn", 1);
        SaveAll(g);
        if (g.coop) SaveProfile(WP(g, i));
        PlaySfx(Sfx::ItemGet);
        Say(w, "It's an Apricorn tree!|You picked a " + a[2] + " Apricorn!|" + PocketHint(a[2] + " Apricorn"));
    }
    else if (k == "kurt") {   // Kurt: an Apricorn colour -> that many of its ball
        std::vector<std::pair<std::string, std::string>> ch;
        for (const char* c : {"Red", "Yellow", "Blue", "Green", "Pink", "White", "Black"}) {
            int n = BagCount(WP(g, i), std::string(c) + " Apricorn");
            if (n > 0) ch.push_back({TextFormat("%s Apricorn x%d -> %s", c, n, ApriBall(c)), std::string("kurtball|") + c});
        }
        std::string hello = act.size() > 5 ? act.substr(5) : "Kurt: Bring me Apricorns and I'll make you Poke Balls!";
        if (ch.empty()) { Say(w, hello + "|Kurt: You don't have any Apricorns. They grow on trees along Johto's routes - one a day."); return; }
        ch.push_back({"Not now", ""});
        Ask(w, hello + "|Kurt: Which Apricorns shall I make into Balls?", ch);
    }
    else if (k == "kurtball" && a.size() > 1) {
        std::string apri = a[1] + " Apricorn";
        int n = BagCount(WP(g, i), apri);
        if (n <= 0) return;
        for (int q = 0; q < n; q++) BagTake(WP(g, i), apri);
        BagAdd(WP(g, i), ApriBall(a[1]), n);
        SaveAll(g);
        if (g.coop) SaveProfile(WP(g, i));
        PlaySfx(Sfx::ItemGet);
        Say(w, TextFormat("Kurt: Hm! Hmm... Tink, tink, tink...|Here you go! %d %s%s, fresh from my workshop.", n, ApriBall(a[1]), n == 1 ? "" : "s") + std::string("|") + PocketHint(ApriBall(a[1])), "kurt");
    }
    else if (k == "switch" && a.size() > 1) {   // switch|flag|text: a secret switch (the Pokemon Mansion's statues)
        std::string text = act.size() > 8 + a[1].size() ? act.substr(8 + a[1].size()) : "A switch!|Press it?";
        Ask(w, text, {{"Yes", "toggle|" + a[1]}, {"No", ""}});
    }
    else if (k == "press" && a.size() > 1) {   // press|flag[|toggle]: a button underfoot (the Trick House): sets the flag (or flips it)
        bool on = Flags(g).count(a[1]) > 0;
        if (a.size() > 2 && a[2] == "toggle" && on) { Flags(g).erase(a[1]); if (Online(g)) OnlineSendCoop(g); }
        else if (!on) SetFlag(g, a[1]);
        else return;
        PlaySfx(Sfx::Door);
    }
    else if (k == "rtile" && a.size() > 1) {   // rtile|colour: a switch - everyone on that colour's arrows moves along and turns left
        const OwMap& m = MapOf(w);
        int c = std::atoi(a[1].c_str());
        auto& v = gNpc[w.map];
        static const int kDx[4] = {1, 0, -1, 0}, kDy[4] = {0, 1, 0, -1};   // right, down, left, up
        static const int kLeft[4] = {3, 2, 0, 1};   // facing down/up/left/right -> right/left/down/up (a quarter turn to the left)
        bool moved = false;
        for (size_t n = 0; n < v.size() && n < m.npcs.size(); n++) {
            if (!NpcHere(g, m.npcs[n])) continue;
            auto it = m.rtiles.find(v[n].y * 100000 + v[n].x);
            if (it == m.rtiles.end() || it->second.first != c) continue;
            int d = it->second.second;
            v[n].fromX = v[n].x; v[n].fromY = v[n].y;
            v[n].x += kDx[d]; v[n].y += kDy[d];
            v[n].st = 0;
            v[n].face = kLeft[v[n].face & 3];
            moved = true;
        }
        PlaySfx(moved ? Sfx::Door : Sfx::MenuMove);
    }
    else if ((k == "trade" || k == "tradepick" || k == "tradego") && a.size() > 1) {   // an in-game trade: they want one of yours for theirs
        auto it = gTrades.find(a[1]);
        if (it == gTrades.end()) return;
        const NpcTradeDef& d = it->second;
        Profile& P = WP(g, i);
        auto fill = [&](std::string s) {
            for (size_t at; (at = s.find("{GIVE}")) != std::string::npos;) s.replace(at, 6, d.give);
            for (size_t at; (at = s.find("{GET}")) != std::string::npos;) s.replace(at, 5, d.get);
            return s;
        };
        std::string flag = "traded:" + a[1] + ":" + P.name;   // (co-op: each of you trades once)
        if (Flags(g).count(flag)) { Say(w, fill(d.after.empty() ? "How's my old {GET} doing?" : d.after)); return; }
        std::vector<int> ok;
        for (int m = 0; m < (int)P.mons.size(); m++)
            if (!P.mons[m].boxed && P.mons[m].eggSteps <= 0 && P.mons[m].species == d.give) ok.push_back(m);
        if (k == "trade") {
            Ask(w, fill(d.ask), {{"Yes", "tradepick|" + a[1]}, {"No", "msg|" + fill(d.decline.empty() ? "Oh, well..." : d.decline)}});
        } else if (k == "tradepick") {
            if (ok.empty()) { Say(w, fill(d.wrong.empty() ? "That's no {GIVE}!" : d.wrong) + "|(Bring a " + d.give + " in your party.)"); return; }
            if (ok.size() == 1) { RunAction(g, i, "tradego|" + a[1] + "|" + std::to_string(ok[0])); return; }
            std::vector<std::pair<std::string, std::string>> ch;
            for (int m : ok) ch.push_back({MonName(P.mons[m]) + " Lv" + std::to_string(P.mons[m].level), "tradego|" + a[1] + "|" + std::to_string(m)});
            ch.push_back({"Never mind", ""});
            Ask(w, "Which " + d.give + " will you trade?", ch);
        } else {
            int m = a.size() > 2 ? std::atoi(a[2].c_str()) : -1;
            int gi = SpeciesIndex(d.get);
            if (m < 0 || m >= (int)P.mons.size() || P.mons[m].species != d.give || gi < 0) return;
            OwnedMon old = P.mons[m];
            OwnedMon nm = NewMon(gi, old.level);
            nm.nick = d.nick.substr(0, NICK_MAX);
            nm.ot = d.ot;
            for (int s2 = 0; s2 < 6; s2++) nm.iv[s2] = std::max(0, std::min(31, d.iv[s2]));
            if (!d.item.empty() && FindItem(d.item)) nm.item = d.item;
            nm.metAt = "Traded from " + d.ot;
            nm.caught = true;
            if (!old.item.empty()) BagAdd(P, old.item);   // (whatever yours was holding comes back to your Bag)
            P.mons[m] = nm;
            P.dexSeen.insert(d.get); P.dexCaught.insert(d.get);
            SetFlag(g, flag);
            SaveAll(g);
            Say(w, fill(d.thanks.empty() ? "Thanks!" : d.thanks));
            Game& t = g.coop ? StartSub(g, i) : g;
            OpenNpcTrade(t, MonSpecies(old), old.shiny, MonName(old), gi, nm.nick.empty() ? d.get : nm.nick, d.ot);
        }
    }
    else if ((k == "giftmon" || k == "giftegg" || k == "giftbuy" || k == "giftpick") && a.size() > 3) {
        // a Pokemon (or an Egg) someone gives you - once each (co-op: each of you):
        //   giftmon|Species|level|key|text|after[|nickname|original trainer]   giftegg|Species|key|text|after
        //   giftbuy|Species|level|key|price|text|after|no thanks             giftpick|key|A,B|level|text|after (one of them)
        Profile& P = WP(g, i);
        auto pages = [](std::string t) { for (char& ch : t) if (ch == '/') ch = '|'; return t; };
        std::string key = k == "giftmon" || k == "giftbuy" ? a[3] : a[k == "giftpick" ? 1 : 2];
        std::string flag = "gift:" + key + ":" + P.name;
        std::string after = k == "giftmon" ? (a.size() > 5 ? a[5] : "") : k == "giftegg" ? (a.size() > 4 ? a[4] : "") : k == "giftbuy" ? (a.size() > 6 ? a[6] : "") : (a.size() > 5 ? a[5] : "");
        if (Flags(g).count(flag)) { Say(w, pages(after.empty() ? "Take good care of it!" : after)); return; }
        if (k == "giftpick") {
            std::vector<std::pair<std::string, std::string>> ch;
            for (const auto& sp : Split(a[2], ',')) ch.push_back({Trim(sp), "giftmon|" + Trim(sp) + "|" + a[3] + "|" + key + "||" + after});
            ch.push_back({"Not now", ""});
            Ask(w, pages(a.size() > 4 ? a[4] : "Which one will you take?"), ch);
            return;
        }
        if (k == "giftbuy" && (a.size() < 9 || a[8] != "yes")) {   // (asked first; "yes" on the end once you agree)
            int price = std::atoi(a[4].c_str());
            std::string yes = "giftbuy|" + a[1] + "|" + a[2] + "|" + a[3] + "|" + a[4] + "|" + a[5] + "|" + after + "|" + (a.size() > 7 ? a[7] : "") + "|yes";
            Ask(w, pages(a[5]) + TextFormat("|(It's %d coins. You have %d.)", price, P.coins), {{"Buy it", yes}, {"No thanks", "msg|" + pages(a.size() > 7 && !a[7].empty() ? a[7] : "No? Too bad...")}});
            return;
        }
        if (k == "giftbuy") {
            int price = std::atoi(a[4].c_str());
            if (P.coins < price) { Say(w, "You'll need more coins than that, kid!"); return; }
            P.coins -= price;
        }
        std::string msg;
        if (k == "giftegg") {
            if ((int)P.eggs.size() >= MAX_EGGS) { Say(w, "You have no room for an Egg! (6 at most)"); return; }
            OwnedMon egg = NewMon(SpeciesIndex(a[1]), 1);
            egg.eggSteps = EggCycles(a[1]) * EGG_CYCLE_STEPS;
            egg.metAt = MapOf(w).name;
            P.eggs.push_back(egg);
            msg = pages(a.size() > 3 ? a[3] : "") + "|You received an Egg!|(Walk around, or battle, and it'll hatch.)";
        } else {
            int si = SpeciesIndex(a[1]);
            if (si < 0) return;
            OwnedMon m = NewMon(si, std::max(1, std::atoi(a[2].c_str())));
            if (k == "giftmon" && a.size() > 6 && !a[6].empty()) m.nick = a[6].substr(0, NICK_MAX);
            if (k == "giftmon" && a.size() > 7 && !a[7].empty()) m.ot = a[7];
            m.metAt = MapOf(w).name;
            bool boxed = AddMon(P, m);
            P.dexSeen.insert(a[1]); P.dexCaught.insert(a[1]);
            std::string text = k == "giftbuy" ? "" : (a.size() > 4 ? a[4] : "");
            msg = (text.empty() ? "" : pages(text) + "|") + "You received " + (m.nick.empty() ? a[1] : m.nick) + "!" + (boxed ? "|(Your party is full - it went to the Box.)" : "");
        }
        SetFlag(g, flag);
        SaveAll(g);
        if (g.coop) SaveProfile(P);
        PlaySfx(Sfx::LevelUp);
        Say(w, msg);
    }
    else if (k == "kenya" || k == "kenyamail" || k == "shuckie" || k == "shuckieback") {
        // HeartGold's loans: Randy's Spearow, Kenya, with Mail for his friend on Route 31; Kirk's Shuckle, Shuckie
        Profile& P = WP(g, i);
        auto own = [&](const std::string& f) { return Flags(g).count(f + ":" + P.name) > 0; };
        auto setOwn = [&](const std::string& f) { SetFlag(g, f + ":" + P.name); };
        auto find = [&](const std::string& nick, const std::string& ot) { for (int m = 0; m < (int)P.mons.size(); m++) if (!P.mons[m].boxed && P.mons[m].nick == nick && P.mons[m].ot == ot) return m; return -1; };
        if (k == "kenya") {
            int km = find("KENYA", "Randy");
            if (own("kenya:done")) { Say(w, "Everything is fine!"); return; }
            if (own("kenya:delivered") && km >= 0) {
                P.mons.erase(P.mons.begin() + km);
                BagAdd(P, "HP Up");
                setOwn("kenya:done"); SaveAll(g); PlaySfx(Sfx::ItemGet);
                Say(w, "Thank you for making the delivery!|...I'll take Kenya back now.|I want you to have this for your kindness!|You got an HP Up!");
                return;
            }
            if (own("kenya:given")) { Say(w, "My friend is on Route 31.|He looks obviously eager to read letters, so you'll know who he is right away!"); return; }
            Ask(w, "Excuse me, kid! I received this letter the other day from a friend of mine.|Here's the reply that I wrote.|I am stuck here and was wondering if you could deliver it...|He's on Route 31.|Can you help a guy out?",
                {{"Yes", "kenyamail|give"}, {"No", "msg|Oh... Never mind, then... You must be busy, too."}});
        } else if (k == "kenyamail" && a.size() > 1 && a[1] == "give") {
            if (PartyCountOf(P) >= 6) { Say(w, "Huh? Your party is full!"); return; }
            OwnedMon m = NewMon(SpeciesIndex("Spearow"), 20);
            m.nick = "KENYA"; m.ot = "Randy"; m.metAt = "Route 35 Goldenrod Gate";
            AddMon(P, m);
            setOwn("kenya:given"); SaveAll(g); PlaySfx(Sfx::LevelUp);
            Say(w, "Wow, thanks!|You received the Spearow with Mail - Kenya!|My friend is on Route 31. He looks obviously eager to read letters, so you'll know who he is right away!");
        } else if (k == "kenyamail") {   // the friend on Route 31
            if (own("kenya:delivered")) { Say(w, "My pal Randy... he always sends me the best letters. Zzz..."); return; }
            if (!own("kenya:given") || find("KENYA", "Randy") < 0) { Say(w, "...Hnuurg! ...Huh?|I feel like reading some words on a piece of paper or something...|...Zzz..."); return; }
            setOwn("kenya:delivered");
            BagAdd(P, "TM085 Rest");
            SaveAll(g); PlaySfx(Sfx::ItemGet);
            Say(w, "...Hnuurg! ...Huh?|{PLAYER} showed him the Mail Kenya is carrying.|Let's see... \"Dear Mike, I hope you're getting enough sleep...\"|Ha! Thanks a lot! Here, take this!|You got TM085 Rest!|(Take Kenya back to Randy in the Route 35 gate.)");
        } else if (k == "shuckie") {   // Kirk, Cianwood City
            int sm = find("SHUCKIE", "Kirk");
            if (own("shuckie:done")) { Say(w, "For Pokemon, happiness is being with a person who treats them well."); return; }
            if (own("gift:shuckiegift") && sm >= 0) {
                if (P.mons[sm].friendship >= 150) { setOwn("shuckie:done"); SaveAll(g); Say(w, "Hi! How's my Pokemon?|My Pokemon has come to like you.|All right, you should keep it. But promise to be good to it!"); return; }
                Ask(w, "Hi! How's my Pokemon?|I think I'm safe now, so may I have it back?", {{"Yes", "shuckieback"}, {"No", "msg|Oh, no, no... That's the same as being robbed."}});
                return;
            }
            if (own("gift:shuckiegift")) { Say(w, "Hi! How's my Pokemon?"); return; }
            Ask(w, "I... I'm in shock!|A guy about your age with piercing eyes and long hair came in.|He scared me into giving him my prized Pokemon!|I still have one left, but what if he comes back?|You look strong. Could you look after my Pokemon for a while?",
                {{"Yes", PartyCountOf(P) >= 6 ? "msg|Your Pokemon party is full." : "giftmon|Shuckle|15|shuckiegift|Oh, thank you! Take good care of it, please!||SHUCKIE|Kirk"}, {"No", "msg|Oh, no... What'll I do if he comes back?"}});
        } else {   // shuckieback
            int sm = find("SHUCKIE", "Kirk");
            int left = 0; for (const auto& m : P.mons) left += !m.boxed;
            if (sm < 0) return;
            if (left <= 1) { Say(w, "Huh? You...only have one Pokemon!|You can't give it back to me then."); return; }
            if (!P.mons[sm].item.empty()) BagAdd(P, P.mons[sm].item);
            P.mons.erase(P.mons.begin() + sm);
            setOwn("shuckie:done"); SaveAll(g);
            Say(w, "Thank you!");
        }
    }
    else if (k == "radio") {   // the Pokegear's radio (Johto)
        std::vector<std::pair<std::string, std::string>> ch = {{"Prof. Oak's Pokemon Talk", "radioon|oak"}, {"Pokemon Music: March", "radioon|march"},
                                                               {"Pokemon Music: Lullaby", "radioon|lullaby"}, {"Variety: Buena's Password", "radioon|buena"}};
        if (HasFlag(g, i, "done:Johto")) { ch.push_back({"Hoenn Sound", "radioon|hoenn"}); ch.push_back({"Sinnoh Sound", "radioon|sinnoh"}); }
        if (MapOf(w).name.find("Ruins of Alph") != std::string::npos) ch.push_back({"?????? (a strange signal)", "radioon|unown"});
        ch.push_back({"Turn it off", "radioon|off"});
        std::string now = RadioStation(g, i);
        Ask(w, std::string("Pokegear Radio.") + (now.empty() || now == "off" ? "" : " (Playing now: " + now + ")") + "|Tune in to...", ch);
    }
    else if (k == "radioon" && a.size() > 1) {
        const std::string tail = "|" + WP(g, i).name;
        auto& fl = Flags(g);
        for (auto it = fl.begin(); it != fl.end();) it = (it->rfind("radiost|", 0) == 0 && it->size() > tail.size() && it->compare(it->size() - tail.size(), tail.size(), tail) == 0) ? fl.erase(it) : std::next(it);
        const std::string& st = a[1];
        bool lasting = st == "march" || st == "lullaby" || st == "hoenn" || st == "sinnoh" || st == "unown";
        if (lasting) SetFlag(g, "radiost|" + st + tail);
        SaveAll(g);
        PlaySfx(Sfx::MenuOk);
        if (st == "oak") {
            std::string t = "Prof. Oak's Pokemon Talk! With Mary!|Mary: Professor, what's up today?";
            int sp = -1;
            const OwMap& m = MapOf(w);
            int area = WildAreaIndex(m.wild);
            if (area >= 0) { auto v = WildAreaMons(area, (int)Story().stages.size(), 99); if (!v.empty()) sp = v[SimRand(0, (int)v.size() - 1)].species; }
            if (sp >= 0) t += "|Oak: " + AllSpecies()[sp].name + " lives around " + m.wild + ". Its habits are fascinating!";
            if (HasFlag(g, i, "done:Johto")) { const Swarm& sw = TodaysSwarm(); t += std::string("|Oak: Here's some news! Today, ") + sw.species + " has been sighted in large numbers in " + sw.map + "!"; }
            else t += "|Oak: Catch lots of Pokemon, and fill up your Pokedex!";
            Say(w, t);
        }
        else if (st == "march") Say(w, "Pokemon Music! Today's track: the Pokemon March!|(While it plays, wild Pokemon in Johto come out more often.)");
        else if (st == "lullaby") Say(w, "Pokemon Music! Today's track: the Pokemon Lullaby...|(While it plays, wild Pokemon in Johto come out less often.)");
        else if (st == "hoenn") Say(w, "Hoenn Sound - music from a faraway land!|(While it plays, Pokemon from Hoenn may turn up in Johto's grass.)");
        else if (st == "sinnoh") Say(w, "Sinnoh Sound - music from a faraway land!|(While it plays, Pokemon from Sinnoh may turn up in Johto's grass.)");
        else if (st == "unown") Say(w, "...Hsss... ...hsss... ...an eerie signal...|(While it plays, Unown shapes you haven't seen yet turn up more often here.)");
        else if (st == "buena") Say(w, std::string("Buena: Hi, everybody! It's Buena's Password!|Buena: Today's password is... ") + kBuenaWords[BuenaWord(RaidToday())] +
                                    "!|Buena: Don't forget it! Come and tell me at the Radio Tower in Goldenrod!");
        else Say(w, "You turned the radio off.");
    }
    else if (k == "buena" || k == "buenaans") {   // Buena at the Radio Tower: today's password for a point on the Blue Card
        Profile& P = WP(g, i);
        auto& fl = Flags(g);
        auto num = [&](const std::string& key) { for (const auto& f : fl) if (f.rfind(key, 0) == 0) return std::atoi(f.c_str() + key.size()); return -1; };
        auto put = [&](const std::string& key, int v) { for (auto it = fl.begin(); it != fl.end();) it = it->rfind(key, 0) == 0 ? fl.erase(it) : std::next(it); fl.insert(key + std::to_string(v)); };
        std::string pk = "buenapts:" + P.name + "|", dk = "buenaday:" + P.name + "|";
        int pts = std::max(0, num(pk)), today = RaidToday();
        if (k == "buena") {
            if (num(dk) == today) { Say(w, TextFormat("Buena: You already told me today's password! See you tomorrow!|(Blue Card: %d points.)", pts)); return; }
            int right = BuenaWord(today);
            std::vector<int> opts = {right, (right + 7) % kBuenaN, (right + 19) % kBuenaN};
            std::swap(opts[0], opts[today % 3]);
            std::vector<std::pair<std::string, std::string>> ch;
            for (int o : opts) ch.push_back({kBuenaWords[o], "buenaans|" + std::to_string(o)});
            Ask(w, "Buena: Hi! Did you tune in to my radio show?|Buena: Do you remember today's password?", ch);
            return;
        }
        put(dk, today);
        if (a.size() < 2 || std::atoi(a[1].c_str()) != BuenaWord(today)) { SaveAll(g); Say(w, "Buena: Oh, that's not it... Too bad! Listen in again tomorrow!"); return; }
        pts = std::min(30, pts + 1);
        put(pk, pts);
        struct Prize { int at; const char* item; };
        static const Prize kPrizes[] = {{1, "Poke Ball"}, {3, "Full Restore"}, {5, "Protein"}, {10, "Iron"}, {15, "Carbos"}, {20, "Calcium"}, {25, "HP Up"}, {30, "Rare Candy"}};
        std::string got;
        for (const auto& pr : kPrizes) if (pr.at == pts) { BagAdd(P, pr.item); got = std::string("|Buena: And for ") + std::to_string(pts) + " points, here's a prize!|You got " + pr.item + "!"; }
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, TextFormat("Buena: That's it! You got it!|You got a point on your Blue Card! (%d points)", pts) + got);
    }
    else if (k == "lottery" && a.size() > 1) {   // a Lottery Corner (Goldenrod's Lucky Number Show, Lilycove's): this week's number against your Pokemon's IDs
        Profile& P = WP(g, i);
        auto idOf = [](const std::string& trainer) { uint32_t h = 2166136261u; for (char c : trainer) h = (h ^ (uint8_t)c) * 16777619u; return (int)(h % 100000); };
        int week = (RaidToday() + 3) / 7;   // (a new number each Monday)
        int lucky = (int)(((uint32_t)week * 2654435761u) % 100000u);
        std::string key = "lottery:" + a[1] + ":" + P.name + "|";
        for (const auto& f : Flags(g)) if (f.rfind(key, 0) == 0 && std::atoi(f.c_str() + key.size()) == week) {
            Say(w, TextFormat("This week's Lucky Number is %05d. You've already had your draw this week - please come again next week!", lucky)); return;
        }
        int best = 0; std::string who;
        for (const auto& m : P.mons) {   // party and Box: your own Pokemon carry your ID, traded ones their trainer's
            int id = idOf(m.ot.empty() ? P.name : m.ot), n = 0;
            for (int d = 1; d <= 100000 && n < 5 && (id / d) % 10 == (lucky / d) % 10; d *= 10) n++;
            if (n > best) { best = n; who = MonName(m); }
        }
        auto& fl = Flags(g);
        for (auto it = fl.begin(); it != fl.end();) it = it->rfind(key, 0) == 0 ? fl.erase(it) : std::next(it);
        fl.insert(key + std::to_string(week));
        static const char* kPrize[6] = {"", "", "PP Up", "Exp. All", "Max Revive", "Master Ball"};
        std::string t = TextFormat("This week's Lucky Number is %05d!|Let's check the ID numbers of all your Pokemon... (Your own ID No. is %05d.)", lucky, idOf(P.name));
        if (best >= 2) {
            BagAdd(P, kPrize[best]);
            t += TextFormat("|Congratulations! The ID number of %s matches the last %d digits!|You won the %s!", who.c_str(), best, kPrize[best]);
            PlaySfx(Sfx::ItemGet);
        } else t += "|I'm sorry... none of your Pokemon's ID numbers matched.|(Two matching digits win a prize - trade for Pokemon with other ID numbers!)";
        SaveAll(g);
        Say(w, t);
    }
    else if (k == "mauvilleman" || k == "bardsing" || k == "bardlyrics" || k == "bardwrite" || k == "giddy") {
        // Mauville's Pokemon Center: one of five men, a different one each day (Emerald's Mauville old man)
        Profile& P = WP(g, i);
        int day = RaidToday();
        std::string song = FlagStr(g, "bard|");
        if (song.empty()) song = "Shake It Do The Diet Dance";   // (Emerald's default lyrics)
        if (k == "bardsing") {
            Ask(w, "The Bard sang:|\"" + song + "...\"|Bard: How was it? It's a pretty good song, right?|Bard: Would you like to write some new lyrics for me?",
                {{"Yes", "bardwrite"}, {"No", "msg|Oh... Well, come by anytime."}});
            return;
        }
        if (k == "bardwrite") { Game& t = g.coop ? StartSub(g, i) : g; OpenEasyChat(t, i, 6, "Write the Bard's song (6 words)", "bardlyrics", Split(song, ' ')); return; }
        if (k == "bardlyrics") {
            if (a.size() < 3 || a[2] != "ok") { Say(w, "Oh... You don't want to? That's too bad."); return; }
            std::string lyr;
            for (const auto& wd : Split(a[1], '+')) if (!wd.empty()) lyr += (lyr.empty() ? "" : " ") + wd;
            SetFlagVal(g, "bard|", lyr);
            SaveAll(g);
            PlaySfx(Sfx::LevelUp);
            Say(w, "Bard: Okay! I'll sing it!|The Bard sang:|\"" + lyr + "...\"|Bard: Thanks! This song's going to be a hit!");
            return;
        }
        if (k == "giddy") {
            if (a.size() > 1 && a[1] == "yes") Say(w, "Giddy: I knew it! I think so, too! You and I, we're two of a kind!");
            else Say(w, "Giddy: Oh, really? Hmm... I'll have to think about that one some more.");
            return;
        }
        switch (day % 5) {
            case 0: Ask(w, "Hi, I'm the Bard. Would you like to hear my song?", {{"Yes", "bardsing"}, {"No", "msg|Oh... Well, come by anytime."}}); break;
            case 1: {   // the Hipster: the hippest word of the day
                std::string wd = EasyChatWordOf("Trendy Saying", (uint32_t)day);
                Say(w, "Hipster: Hey, kid! Do you know the word \"" + wd + "\"?|It's the hippest word around! Use it and everyone will think you're cool.|(You learned a new word: " + wd + ".)");
                break;
            }
            case 2: Say(w, "Trader: Hi! I'm the Trader. I trade decorations for Secret Bases.|...But I've already packed up all my wares. Sorry! Come again some other day."); break;
            case 3: {   // the Storyteller: tales of Trainers... yours
                int caught = DexCaughtCount(P), ribbons = 0;
                for (const auto& m : P.mons) ribbons += (int)m.ribbons.size();
                std::string tale;
                switch ((day / 5) % 4) {
                    case 0: tale = TextFormat("there was a Trainer named %s who caught %d kinds of Pokemon!", P.name.c_str(), caught); break;
                    case 1: tale = TextFormat("there was a Trainer named %s who won %d battles!", P.name.c_str(), P.wins + P.trainingWins); break;
                    case 2: tale = TextFormat("there was a Trainer named %s whose Pokemon won %d Contest ribbons!", P.name.c_str(), ribbons); break;
                    default: tale = TextFormat("there was a Trainer named %s who collected %d Gym Badges!", P.name.c_str(), (int)P.badges.size()); break;
                }
                Say(w, "Storyteller: I'm the Storyteller. I tell tales of great Trainers.|Storyteller: Long, long ago... " + tale + "|Storyteller: ...Truly, a legend for the ages.");
                break;
            }
            default: {   // Giddy: a question built from Easy Chat words
                std::string a1 = EasyChatWordOf("Pokemon", (uint32_t)day * 3 + 1), a2 = EasyChatWordOf("Conditions", (uint32_t)day * 5 + 2);
                Ask(w, "Giddy: Hi! I'm Giddy! Hey, listen... I think " + a1 + " is " + a2 + "!|Giddy: Don't you think so too?", {{"Yes", "giddy|yes"}, {"No", "giddy|no"}});
            }
        }
    }
    else if (k == "lilycovelady" || k == "ladyquiz" || k == "ladyfavor" || k == "ladyblock") {
        // the Lilycove Lady (Lilycove's Pokemon Center): a quiz, a favor or her Contest Pokemon - a different one each week
        Profile& P = WP(g, i);
        int week = (RaidToday() + 3) / 7;
        std::string done = "lady:" + std::to_string(week) + ":" + P.name;
        if (k == "ladyquiz") {
            if (gLadyQuiz.empty()) return;
            const LadyQuiz& q = gLadyQuiz[week % gLadyQuiz.size()];
            if (a.size() < 3 || a[2] != "ok") { Say(w, "Quiz Lady: Oh? You don't know? That's too bad!"); return; }
            std::string ans = Trim(Split(a[1], '+')[0]);
            SetFlag(g, done);
            if (ans == q.answer) {
                if (FindItem(q.prize)) BagAdd(P, q.prize);
                SaveAll(g); PlaySfx(Sfx::ItemGet);
                Say(w, "Quiz Lady: " + ans + "! That's right! Wonderful!|Quiz Lady: Here's your prize!|You got " + q.prize + "!");
            } else { SaveAll(g); Say(w, "Quiz Lady: " + ans + "...? Oh, I'm sorry! The answer was " + q.answer + ".|Quiz Lady: Come back next week for a new quiz!"); }
            return;
        }
        if (k == "ladyfavor") {
            if (gLadyFavor.empty() || a.size() < 2) return;
            const LadyFavor& f = gLadyFavor[week % gLadyFavor.size()];
            if (BagCount(P, a[1]) <= 0) return;
            BagTake(P, a[1]);
            SetFlag(g, done);
            bool good = std::find(f.items.begin(), f.items.end(), a[1]) != f.items.end();
            if (good) { if (FindItem(f.prize)) BagAdd(P, f.prize); PlaySfx(Sfx::ItemGet); }
            SaveAll(g);
            Say(w, good ? "Favor Lady: Oh! A " + a[1] + "! That's exactly the " + f.thing + " thing I wanted!|Favor Lady: Thank you! Please, take this.|You got " + f.prize + "!"
                        : "Favor Lady: A " + a[1] + "...? Hmm, it's not quite " + f.thing + "... But thank you anyway!");
            return;
        }
        static const char* kCat[5] = {"Coolness", "Beauty", "Cuteness", "Smartness", "Toughness"};
        static const char* kMon[5] = {"Zigzagoon", "Skitty", "Poochyena", "Kecleon", "Pikachu"};
        static const char* kNick[5] = {"Handsome", "Vinny", "Moreme", "Ironhard", "Muscle"};
        int cat = week % 5;
        if (k == "ladyblock") {
            if (a.size() < 2 || BagCount(P, a[1]) <= 0) return;
            const ItemDef* d = FindItem(a[1]);
            BagTake(P, a[1]);
            SetFlag(g, done);
            int bi = d ? d->boost - 100 : -1;
            bool liked = false;
            if (bi >= 0 && bi < BerryCount()) { int best = 0; for (int c2 = 0; c2 < 5; c2++) if (BerryAt(bi).flavor[c2] > BerryAt(bi).flavor[best]) best = c2; liked = best == cat && BerryAt(bi).flavor[cat] > 0; }
            std::string key = "ladyblocks:" + P.name + "|";
            int n = std::max(0, FlagNum(g, key)) + 1;
            SetFlagVal(g, key, std::to_string(n));
            if (n == 5) SetFlag(g, "tv:contestlady:" + P.name);
            SaveAll(g);
            PlaySfx(Sfx::Heal);
            Say(w, std::string("Contest Lady: Thank you! ") + kNick[cat] + " the " + kMon[cat] + " gobbled it up!|" +
                   (liked ? std::string("Contest Lady: Oh, look! Its ") + kCat[cat] + " really improved! Thank you so much!" : "Contest Lady: Hmm... it doesn't seem to have changed much. Thanks anyway!") +
                   (n == 5 ? "|Contest Lady: You've given it five Pokeblocks now! I'll tell everyone - you might even see it on TV!" : ""));
            return;
        }
        if (Flags(g).count(done)) { Say(w, "Thank you so much for this week! Please come again next week - I'll have something new for you."); return; }
        switch (week % 3) {
            case 0: {
                if (gLadyQuiz.empty()) return;
                const LadyQuiz& q = gLadyQuiz[week % gLadyQuiz.size()];
                std::string opts;
                for (const auto& o : q.a) if (!o.empty()) opts += (opts.empty() ? "" : " / ") + o;
                Ask(w, "Quiz Lady: I'm the Quiz Lady! Answer my quiz correctly and win a prize: " + q.prize + "!|Quiz Lady: " + q.q + "|(" + opts + ")",
                    {{"Answer it", "ezchat|1|Answer the Quiz Lady|ladyquiz"}, {"Not now", "msg|Quiz Lady: Oh? Come back anytime!"}});
                break;
            }
            case 1: {
                if (gLadyFavor.empty()) return;
                const LadyFavor& f = gLadyFavor[week % gLadyFavor.size()];
                std::vector<std::pair<std::string, std::string>> ch;
                std::vector<std::string> own;
                for (const auto& [it, n] : P.bag) if (n > 0 && FindItem(it) && FindItem(it)->fx != ItemFx::KeyItem && !IsTM(*FindItem(it))) own.push_back(it);
                std::vector<std::string> good, other;
                for (const auto& it : own) (std::find(f.items.begin(), f.items.end(), it) != f.items.end() ? good : other).push_back(it);
                for (size_t q = 0; q < good.size() && ch.size() < 4; q++) ch.push_back({good[q], "ladyfavor|" + good[q]});
                for (size_t q = 0; q < other.size() && ch.size() < 8; q++) ch.push_back({other[(q * 7 + week) % other.size()], "ladyfavor|" + other[(q * 7 + week) % other.size()]});
                std::sort(ch.begin(), ch.end());
                ch.erase(std::unique(ch.begin(), ch.end()), ch.end());
                ch.push_back({"Never mind", "msg|Favor Lady: Oh... If you find something " + f.thing + ", please bring it to me!"});
                Ask(w, "Favor Lady: I'm looking for something... " + f.thing + ".|Favor Lady: Do you have anything " + f.thing + " you could give me?", ch);
                break;
            }
            default: {
                std::vector<std::pair<std::string, std::string>> ch;
                for (const auto& [it, n] : P.bag) if (n > 0 && it.rfind("Pokeblock (", 0) == 0 && ch.size() < 8) ch.push_back({it, "ladyblock|" + it});
                ch.push_back({"Never mind", "msg|Contest Lady: Oh... What a cheapskate!"});
                std::string intro = std::string("Contest Lady: My friend ") + kNick[cat] + " the " + kMon[cat] + " is training to show off its " + kCat[cat] + " in Contests!|";
                if (ch.size() > 1) Ask(w, intro + "Contest Lady: Would you give it one of your Pokeblocks?", ch);
                else Say(w, intro + "Contest Lady: If you had a Pokeblock, I'd ask you for one... (Make them in Lilycove's Berry Blenders.)");
            }
        }
    }
    else if (k == "ezchat" && a.size() > 3) {   // ezchat|slots|title|then: open the Easy Chat word picker
        Game& t = g.coop ? StartSub(g, i) : g;
        OpenEasyChat(t, i, std::atoi(a[1].c_str()), a[2], a[3]);
    }
    else if (k == "trendy" || k == "trendset") {   // Dewford's trendy saying: tell the boy something new
        if (k == "trendy") {
            Ask(w, "Hey! Do you know the latest trendy saying? Right now it's \"" + DewfordTrend(g) + "\".|Tell me if you know something trendier!",
                {{"Tell him", "ezchat|2|What's trendy in Dewford?|trendset"}, {"No", "msg|Oh... If you hear anything hip, let me know!"}});
            return;
        }
        if (a.size() < 3 || a[2] != "ok") { Say(w, "Oh... If you hear anything hip, let me know!"); return; }
        std::string t;
        for (const auto& wd : Split(a[1], '+')) if (!wd.empty()) t += (t.empty() ? "" : " ") + wd;
        SetFlagVal(g, "trend|", t);
        SaveAll(g);
        PlaySfx(Sfx::LevelUp);
        Say(w, "\"" + t + "\"!? Whoa! That's so hip!|I'm going to tell everyone! Soon all of Dewford will be saying it!");
    }
    else if (k == "glassworks" || k == "glassmake") {   // Route 113's Glass Workshop: flutes from the volcanic ash in your Soot Sack
        Profile& P = WP(g, i);
        if (BagCount(P, "Soot Sack") <= 0) {
            BagAdd(P, "Soot Sack");
            SaveAll(g); PlaySfx(Sfx::ItemGet);
            Say(w, "Glassworker: Hah? You haven't got a Soot Sack? Here, take this one.|You got the Soot Sack!|Glassworker: Walk through the ash-covered grass on Route 113 and it'll fill up.|Bring the ash to me and I'll make you a glass flute!");
            return;
        }
        std::string key = "ash:" + P.name + "|";
        int ash = std::max(0, FlagNum(g, key));
        struct Flute { const char* name; int cost; };
        static const Flute kFlutes[5] = {{"Blue Flute", 250}, {"Yellow Flute", 500}, {"Red Flute", 500}, {"White Flute", 1000}, {"Black Flute", 1000}};
        if (k == "glassmake" && a.size() > 1) {
            int f = std::max(0, std::min(4, std::atoi(a[1].c_str())));
            if (ash < kFlutes[f].cost) { Say(w, TextFormat("Glassworker: You need %d steps of ash for that. You've got %d.", kFlutes[f].cost, ash)); return; }
            SetFlagVal(g, key, std::to_string(ash - kFlutes[f].cost));
            BagAdd(P, kFlutes[f].name);
            SaveAll(g); PlaySfx(Sfx::ItemGet);
            Say(w, std::string("Glassworker: Hold on... Hup! There we go - one ") + kFlutes[f].name + "!|You got the " + kFlutes[f].name + "!");
            return;
        }
        std::vector<std::pair<std::string, std::string>> ch;
        for (int f = 0; f < 5; f++) ch.push_back({TextFormat("%s (%d ash)", kFlutes[f].name, kFlutes[f].cost), "glassmake|" + std::to_string(f)});
        ch.push_back({"Nothing", ""});
        Ask(w, TextFormat("Glassworker: You've got %d steps' worth of ash in your Soot Sack.|Glassworker: What shall I make for you?", ash), ch);
    }
    else if (k == "tv") {   // a TV (Emerald): today's show
        Profile& P = WP(g, i);
        const OwMap& m = MapOf(w);
        uint32_t h = (uint32_t)RaidToday() * 31 + (uint32_t)m.name.size() * 7 + (uint32_t)(w.x + w.y);
        std::string show;
        if (Flags(g).count("tv:contestlady:" + P.name) && h % 3 == 0) {
            show = "It's \"The Contest Lady's Pokeblock Diary\"!|Thanks to the Pokeblocks from a Trainer named " + P.name + ", her Pokemon is ready to shine in Contests!";
        } else switch (h % 4) {
            case 0: { const Outbreak& ob = TodaysOutbreak(); show = std::string("Breaking news! A mass outbreak!|Many ") + ob.species + " have been sighted on " + ob.map + "!|Trainers, this is your chance to catch one!"; break; }
            case 1: show = TextFormat("It's \"Trainer Fan Club\"!|Today's star: %s! %d Gym Badges, and %d kinds of Pokemon caught! What a Trainer!", P.name.c_str(), (int)P.badges.size(), DexCaughtCount(P)); break;
            case 2: { std::string lead; for (const auto& mo : P.mons) if (!mo.boxed) { lead = MonName(mo); break; }
                      show = lead.empty() ? std::string("It's \"Pokemon Today\"! Today's Pokemon is... Zigzagoon! It loves picking up items!") : "It's \"Pokemon Today\"!|Today's featured Pokemon: " + lead + ", partner of " + P.name + "! Such a fine Pokemon!"; break; }
            default: show = "It's \"Dewford Trends\"! The hottest saying in Dewford right now is \"" + DewfordTrend(g) + "\"!"; break;
        }
        Say(w, "(The TV is on.)|" + show);
    }
    // ================================================================ round 6: side activities from the real games
    else if (k == "buy" && a.size() > 1) {   // buy|what they say (pages)|Item:price,Item:price: a little shop of its own
        std::vector<std::pair<std::string, std::string>> ch;
        for (const auto& e : Split(a.back(), ',')) {
            auto p = Split(e, ':');
            if (Trim(p[0]) == "shop") { ch.push_back({"See the other goods", "shop"}); continue; }
            if (p.size() < 2 || !FindItem(Trim(p[0]))) continue;
            ch.push_back({Trim(p[0]) + TextFormat("  (%d)", std::atoi(p[1].c_str())), "buygo|" + Trim(p[0]) + "|" + Trim(p[1])});
        }
        ch.push_back({"No thanks", "msg|Come by again sometime."});
        std::string text;
        for (size_t q = 1; q + 1 < a.size(); q++) if (!a[q].empty()) text += (text.empty() ? "" : "|") + a[q];
        Ask(w, (text.empty() ? std::string("What would you like?") : text) + TextFormat("|(You have %d coins.)", WP(g, i).coins), ch);
    }
    else if (k == "buygo" && a.size() > 2) {
        Profile& P = WP(g, i);
        int price = std::atoi(a[2].c_str());
        if (P.coins < price) { Say(w, "You'll need more money than that."); return; }
        P.coins -= price;
        BagAdd(P, a[1]);
        SaveAll(g);
        PlaySfx(Sfx::Shop);
        Say(w, "Thank you! Here's your " + a[1] + ".");
    }
    else if (k == "vending" || k == "vendgo") {   // a vending machine (Celadon's roof, Goldenrod's 6F, Lilycove's rooftop)
        static const char* kDrink[3] = {"Fresh Water", "Soda Pop", "Lemonade"};
        static const int kPrice[3] = {200, 300, 350};
        std::string region = RegionInfo(MapOf(w).region).name;
        Profile& P = WP(g, i);
        if (k == "vending") {
            std::vector<std::pair<std::string, std::string>> ch;
            for (int d = 0; d < 3; d++) ch.push_back({std::string(kDrink[d]) + TextFormat("  (%d)", kPrice[d]), "vendgo|" + std::to_string(d)});
            ch.push_back({"Nothing", region == "Kanto" ? "msg|Not thirsty!" : "msg|Decided against buying a drink."});
            Ask(w, std::string(region == "Kanto" ? "A vending machine! What does it have?" : "It's a vending machine. Which drink would you like?") +
                       TextFormat("|(You have %d coins.)", P.coins), ch);
            return;
        }
        int d = std::max(0, std::min(2, a.size() > 1 ? std::atoi(a[1].c_str()) : 0));
        if (P.coins < kPrice[d]) { Say(w, region == "Kanto" ? "Oops, not enough money!" : "Not enough money..."); return; }
        P.coins -= kPrice[d];
        BagAdd(P, kDrink[d]);
        std::string t = region == "Kanto" ? std::string("A can of ") + kDrink[d] + " popped out!" : std::string("Clang!|A can of ") + kDrink[d] + " dropped down.";
        if (region != "Kanto" && SimRand(0, 63) == 0) {   // (Emerald, HeartGold: one time in 64, a second can)
            BagAdd(P, kDrink[d]);
            t += std::string("|Clang!|Score! An extra can of ") + kDrink[d] + " dropped down!";
        }
        SaveAll(g);
        PlaySfx(Sfx::Shop);
        Say(w, t);
    }
    else if (k == "thirstygirl" || k == "thirstygive") {   // Celadon's rooftop: a drink for a TM (FireRed: TM16, TM20, TM33)
        // (Scarlet / Violet have no Safeguard TM: Misty Terrain keeps status problems away instead)
        static const char* kDrink[3] = {"Fresh Water", "Soda Pop", "Lemonade"};
        static const char* kTm[3] = {"TM075 Light Screen", "TM139 Misty Terrain", "TM074 Reflect"};
        static const char* kWhat[3] = {"It's a move that weakens the power of special attacks by your foe.",
                                       "It keeps your Pokemon on the ground safe from status problems.",
                                       "It's a move that weakens the power of physical attacks by your foe."};
        Profile& P = WP(g, i);
        auto got = [&](int d) { return Flags(g).count(std::string("thirsty:") + kDrink[d] + ":" + P.name) > 0; };
        if (k == "thirstygirl") {
            std::vector<std::pair<std::string, std::string>> ch;
            for (int d = 0; d < 3; d++) if (BagCount(P, kDrink[d]) > 0) ch.push_back({kDrink[d], "thirstygive|" + std::to_string(d)});
            if (ch.empty()) { Say(w, "I'm thirsty! I want something to drink!"); return; }
            ch.push_back({"Don't give her one", ""});
            Ask(w, "I'm thirsty! I want something to drink!|Give her which drink?", ch);
            return;
        }
        int d = std::max(0, std::min(2, a.size() > 1 ? std::atoi(a[1].c_str()) : 0));
        if (got(d)) { Say(w, "No, thank you! I'm not thirsty after all!"); return; }
        BagTake(P, kDrink[d]);
        BagAdd(P, kTm[d]);
        SetFlag(g, std::string("thirsty:") + kDrink[d] + ":" + P.name);
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, std::string("Yay!|") + kDrink[d] + "!|Thank you! You can have this from me!|You received " + kTm[d] + " from the little girl.|" + kTm[d] + " contains " +
                   (kTm[d] + 6) + ".|" + kWhat[d]);
    }
    else if (k == "namerater" || k == "nrpick" || k == "nrgo") {   // the Name Rater (Lavender, Goldenrod, Slateport)
        Profile& P = WP(g, i);
        if (k == "namerater") {
            std::string hi = a.size() > 1 && !a[1].empty() ? a[1] : "Hello, hello! I am the official Name Rater!";
            Ask(w, hi + "|Want me to rate the nicknames of your Pokemon?", {{"Yes", "nrpick"}, {"No", "msg|I see. Do come visit again."}});
            return;
        }
        if (k == "nrpick") {
            std::vector<std::pair<std::string, std::string>> ch;
            for (int m = 0; m < (int)P.mons.size(); m++) if (!P.mons[m].boxed) ch.push_back({MonName(P.mons[m]), "nrgo|" + std::to_string(m)});
            ch.push_back({"Never mind", "msg|I see. Do come visit again."});
            Ask(w, "Which Pokemon's nickname should I critique?", ch);
            return;
        }
        int m = a.size() > 1 ? std::atoi(a[1].c_str()) : -1;
        if (m < 0 || m >= (int)P.mons.size()) return;
        std::string nm = MonName(P.mons[m]);
        if (a.size() > 2 && a[2] == "yes") {   // the nickname screen, then back here
            Game& t = g.coop ? StartSub(g, i) : g;
            OpenNickname(t, 0, m, 2, false);
            return;
        }
        if (!P.mons[m].ot.empty() && P.mons[m].ot != P.name) {   // (a traded Pokemon keeps its name)
            Say(w, nm + ", is it? That is a truly impeccable name!|Take good care of " + nm + "!");
            return;
        }
        Ask(w, nm + ", is it? That is a decent nickname!|But, would you like me to give it a nicer name?|How about it?",
            {{"Yes", "nrgo|" + a[1] + "|yes"}, {"No", "msg|I see. Do come visit again."}});
    }
    else if (k == "copycat") {   // Saffron's Copycat: a Poke Doll for her, and she teaches Mimic (FireRed)
        Profile& P = WP(g, i);
        bool male = ParseLook(P.look).type == 0;
        if (Flags(g).count("copycat:" + P.name)) {
            Say(w, male ? "{PLAYER}: Hi! Thanks for earlier!|{PLAYER}: Pardon?|{PLAYER}: Is it that fun to mimic my every move?|Copycat: You bet! It's a scream!"
                        : "{PLAYER}: Hi! Thanks for what you did for us.|{PLAYER}: Pardon?|{PLAYER}: Is it really that fun to copy what I say and do?|Copycat: You bet! It's a scream!");
            return;
        }
        if (BagCount(P, "Poke Doll") > 0) {
            Ask(w, "Oh wow! A Poke Doll!|For me? Thank you!|You know what? I can teach the move Mimic.", {{"Learn Mimic", "copycatgo"}, {"No thanks", "msg|Don't you like Mimic?"}});
            return;
        }
        Say(w, male ? "{PLAYER}: Hi! Do you like Pokemon?|{PLAYER}: Uh, no, I just asked you.|{PLAYER}: Huh? You're strange!|Copycat: Hmm? Quit mimicking? But, that's my favorite hobby!"
                    : "{PLAYER}: Hi. You must like Pokemon.|{PLAYER}: No, not me. I asked you.|{PLAYER}: Pardon? You're weird!|Copycat: Hmm? Quit mimicking? But that's my favorite hobby!");
    }
    else if (k == "copycatgo") {
        Profile& P = WP(g, i);
        if (!BagTake(P, "Poke Doll")) return;
        SetFlag(g, "copycat:" + P.name);
        SaveAll(g);
        RunAction(g, i, "tutor|Mimic|I really love Mimic! Who's going to learn it?");
    }
    else if (k == "teachyman") {   // Viridian's old man: his grandson's Teachy TV (FireRed)
        Profile& P = WP(g, i);
        if (BagCount(P, "Teachy TV") > 0) {
            Say(w, "Well, now, I've had my coffee, and that's what I need to get going!|But I made it too strong. It gave me a headache...|"
                   "Wahaha! It's my grandson on the show! Since he's the one doing the teaching, you're bound to learn.|(Use the Teachy TV from your Bag.)");
            return;
        }
        BagAdd(P, "Teachy TV");
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, "Well, now, I've had my coffee, and that's what I need to get going!|Hm? What is that red box you have? Ah, so you're working on your Pokedex.|"
               "Then let me give you this, too.|You received the Teachy TV!|If there's something you don't understand, watch that.|"
               "It will teach you about the basics of being a Pokemon Trainer.");
    }
    else if (k == "famegift") {   // Cerulean City: Blue's "little present" after your battle (FireRed's Fame Checker)
        Profile& P = WP(g, i);
        if (BagCount(P, "Fame Checker") > 0) return;
        BagAdd(P, "Fame Checker");
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, "Blue: Oh, yeah, right. I feel sorry for you. No, really. You're always plodding behind me.|So here, I'll give you a little present as a favor.|"
               "You received the Fame Checker!|Blue: A chatty gossip like you... That thing's perfect.|I don't need it because I don't give a hoot about others.|"
               "(Use the Fame Checker from your Bag: it notes what people tell you about the famous folk of Kanto.)");
    }
    else if (k == "selphycave" || k == "selphyhome") {   // Five Island's Lost Cave: Lady Selphy got lost (FireRed)
        if (k == "selphycave") {
            if (Flags(g).count("selphy:found")) return;
            Say(w, "W-what do you intend to do with me?", "teamfightgo|Lady Selphy|Persian 49, Persian 49|Oh... I was afraid that you meant to harm me.|selphy:battle|selphyhome");
            return;
        }
        SetFlag(g, "selphy:found");
        SaveAll(g);
        Say(w, "I am so glad that you happened here.|I grew rather bored of walking, and I am feeling fatigued.|May I ask you to take me home from here?|...|"
               "Selphy: Thank you. You may go now.", "warp|Resort Gorgeous|39|9|up");
    }
    else if (k == "selphy" || k == "selphygo") {   // ...and at home: she wants to see a Pokemon you've caught, and her butler thanks you
        Profile& P = WP(g, i);
        std::string key = "selphywish:" + P.name + "|";
        std::string cur = FlagStr(g, key);
        auto setWish = [&](const std::string& v) {
            auto& fl = Flags(g);
            for (auto it = fl.begin(); it != fl.end();) it = it->rfind(key, 0) == 0 ? fl.erase(it) : std::next(it);
            if (!v.empty()) fl.insert(key + v);
        };
        if (cur.empty()) {   // a new wish: one of the Pokemon in your Pokedex
            std::vector<std::string> seen(P.dexCaught.begin(), P.dexCaught.end());
            if (seen.empty()) { Say(w, "Selphy: I wish you the best of luck."); return; }
            cur = seen[SimRand(0, (int)seen.size() - 1)];
            setWish(cur);
            SaveAll(g);
            Say(w, "Selphy: Oh? Do I know you from somewhere?|Oh, but that doesn't matter.|I am exquisitely bored. Please, hear my wish.|"
                   "I wish to see a Pokemon. A darling " + cur + ".|I want to see one right away. Please, make my wish come true.");
            return;
        }
        bool has = false;
        for (const auto& m : P.mons) has |= !m.boxed && m.species == cur;
        if (!has) { Say(w, "Selphy: I wish to see a Pokemon. A darling " + cur + ".|I want to see one right away. Please, make my wish come true."); return; }
        static const char* kDeluxe[6] = {"Big Pearl", "Pearl", "Stardust", "Star Piece", "Nugget", "Rare Candy"};
        std::string prize = SimRand(0, 99) >= 30 ? "Luxury Ball" : kDeluxe[SimRand(0, 5)];
        if (!FindItem(prize)) prize = "Nugget";
        BagAdd(P, prize);
        setWish("");
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, "Selphy: Oh, hello, there.|Oh, my gracious. That must be the " + cur + " you caught for me.|Giggle... I see a resemblance to you.|Thank you so kindly.|"
               "... ... ...|Is something the matter? You may leave.|... ... ...|Oh, fine. I understand now. Sebastian! I need you here this instant!|"
               "Butler: Yes, my lady.|Selphy: See to it that this person is given a token of appreciation.|Butler: I sincerely thank you for pleasing Lady Selphy.|"
               "Please accept this as her thanks...|You received a " + prize + "!");
    }
    else if (k == "teamfight" || k == "teamfightgo") {   // teamfight|name|team|before|after|flag|then: a battle an action sets up
        // (teamfightgo: straight in - name|team|after|flag|then)
        OwNpc n;
        n.kind = "fight";
        if (k == "teamfight") {
            if (a.size() < 4) return;
            std::string go = "teamfightgo|" + a[1] + "|" + a[2] + "|" + (a.size() > 4 ? a[4] : "") + "|" + (a.size() > 5 ? a[5] : "");
            for (size_t q = 6; q < a.size(); q++) go += "|" + a[q];
            Say(w, a[3], go);
            return;
        }
        if (a.size() < 3) return;
        n.args = {a[1], a[2], "", a.size() > 3 ? a[3] : "", a.size() > 4 && !a[4].empty() ? a[4] : "fought:" + a[1]};
        for (size_t q = 5; q < a.size(); q++) n.winThen += (q > 5 ? "|" : "") + a[q];   // (the rest: an action of its own, with its own "|"s)
        StartRouteFightNpc(g, i, n);
    }
    else if (k == "winstrate" || k == "winstratego") {   // Route 111: the Winstrates, four battles in a row (Emerald)
        struct W4 { const char* name; const char* team; const char* intro; const char* after; };
        static const W4 kFam[4] = {
            {"Winstrate Victor", "Taillow 16, Zigzagoon 16", "That's the spirit! I like you!", "Hey, all! I've found a pretty strong Trainer!"},
            {"Winstrate Victoria", "Roselia 17", "Oh, my goodness! Aren't you young?|You must be quite the Trainer to beat my husband, though.|It's my turn to battle now!",
             "There's a strong Trainer here! This one's really strong!"},
            {"Winstrate Vivi", "Marill 15, Shroomish 15, Numel 15", "You're stronger than Mommy? Wow!|But I'm strong, too! Really! Honestly!", "This stinks...|...Snivel... Grandma!"},
            {"Winstrate Vicky", "Meditite 18", "How dare you make my granddaughter cry!|For that, I'm going to smack you! Prepare to lose!",
             "If you're not in any hurry, visit with us awhile."}};
        if (Flags(g).count("winstrates:done")) { Say(w, "Victor: You beat all four of us! Come in and visit with us awhile."); return; }
        int n = k == "winstrate" ? -1 : std::atoi(a.size() > 1 ? a[1].c_str() : "0");
        if (n < 0) {
            Ask(w, "Victor: Hello! I take it you're a traveler.|How's this? What do you say to taking on our family of four in a series of Pokemon battles?",
                {{"Yes", "winstratego|0"}, {"No", "msg|Victor: Is that so? Drop in, if you change your mind!"}});
            return;
        }
        if (n >= 4) {
            SetFlag(g, "winstrates:done");
            SaveAll(g);
            Say(w, kFam[3].after);
            return;
        }
        std::string prev = n > 0 ? std::string(kFam[n - 1].after) + "|" : "";
        std::string flag = "winstrate:" + std::to_string(n);
        Flags(g).erase(flag);   // (a new run of four: lose one and it's back to Victor)
        Say(w, prev + kFam[n].intro, std::string("teamfightgo|") + kFam[n].name + "|" + kFam[n].team + "||" + flag + "|winstratego|" + std::to_string(n + 1));
    }
    else if (k == "effortribbon") {   // the Effort Ribbon (Slateport; HeartGold's Blackthorn): for a Pokemon with all the EVs it can have
        Profile& P = WP(g, i);
        int lead = -1;
        for (int m = 0; m < (int)P.mons.size() && lead < 0; m++) if (!P.mons[m].boxed) lead = m;
        if (lead < 0) return;
        OwnedMon& om = P.mons[lead];
        std::string nm = MonName(om);
        if (std::find(om.ribbons.begin(), om.ribbons.end(), "Effort Ribbon") != om.ribbons.end()) { Say(w, "Oh! Your " + nm + ", that Effort Ribbon looks good on it!"); return; }
        int ev = 0;
        for (int s2 = 0; s2 < 6; s2++) ev += om.ev[s2];
        if (ev < EV_TOTAL_MAX) {
            Say(w, "Oh? Your " + nm + "...|You have to go for it a little harder.|If you do, I'll give your Pokemon something nice." + TextFormat("|(Its EVs: %d of %d.)", ev, EV_TOTAL_MAX));
            return;
        }
        om.ribbons.push_back("Effort Ribbon");
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, "Oh? Your " + nm + "...|Went for it stupendously!|As its reward, please give it this Effort Ribbon.|{PLAYER} received the Effort Ribbon.|{PLAYER} put the Effort Ribbon on " + nm + ".");
    }
    else if (k == "sibling" && a.size() > 1) {   // HeartGold's Week Siblings: one for each day; an item each, then ribbons
        struct Sib { const char* name; int day; const char* item; const char* ribbon; const char* hi; const char* about; const char* later; const char* wrong; };
        static const Sib kSib[7] = {
            {"Monica", 1, "Sharp Beak", "Alert Ribbon", "Monica: Glad to meet you. I'm Monica of Monday.|As a token of our friendship, I have a gift for you!",
             "Monica: It's an item that raises the power of Flying-type moves.|You should equip a bird Pokemon with that item.",
             "Monica: My brothers and sisters are all over the place.|See if you could find them all!", "Monica: Oh no! Today isn't Monday. See you another time!"},
            {"Tuscany", 2, "Twisted Spoon", "Shock Ribbon", "Tuscany: Please allow me to introduce myself. I am Tuscany of Tuesday.|By way of introduction, please accept this gift, a TwistedSpoon.",
             "Tuscany: Wouldn't you agree that it is most adorable?|It strengthens Psychic-type moves.|I am certain it will be of use.",
             "Tuscany: Have you met Monica, my older sister?|Or my younger brother, Wesley?|I am the second of seven children.", "Tuscany: Tuesday was over before I even noticed...|I'll see you again next Tuesday..."},
            {"Wesley", 3, "Black Belt", "Downcast Ribbon", "Wesley: I'm Wesley of Wednesday.|Pleased to meet you. Please take a souvenir.",
             "Wesley: Black Belt beefs up the power of Fighting-type moves.", "Wesley: Since you found me, you must have met my brothers and sisters.|Or did you just get lucky?",
             "Wesley: No way! Wednesday's already over!|See you next week!"},
            {"Arthur", 4, "Hard Stone", "Careless Ribbon", "Arthur: Who are you?|I'm Arthur of Thursday.|Here, take this!",
             "Arthur: If you use Rock-type moves, you should give that to your Pokemon.|It raises the strength of Rock-type moves.",
             "Arthur: I'm Arthur of Thursday! I'm the second son in a line of seven siblings!",
             "Arthur: Do you think that the middle of the week is Wednesday?|Or do you think it's Thursday?|Go decide, and I'll see you next Thursday!"},
            {"Frieda", 5, "Poison Barb", "Relax Ribbon", "Frieda: Yahoo! It's Friday!|I'm Frieda of Friday! Nice to meet you!|Here's a Poison Barb for you!",
             "Frieda: Give it to a Pokemon that has Poison-type moves.|Oh! It's wicked!|You'll be shocked how good it makes Poison-type moves!",
             "Frieda: Hiya! What day do you like?|I love Friday. No doubt about it!|Don't you think it's great too?",
             "Frieda: Oh, today's not Friday?|Man... I wish every day were Friday, that'd be so much fun!"},
            {"Santos", 6, "Soft Sand", "Snooze Ribbon", "Santos: ...It's Saturday...|I'm Santos of Saturday...|Take this...",
             "Santos: ...|Soft Sand...|Ground-type moves get stronger... It will startle you...", "Santos: ...|See you again on another Saturday...",
             "Santos: Today's Saturday... No... Wait... Huh?|I'll see you next Saturday then..."},
            {"Sunny", 0, "Magnet", "Smile Ribbon", "Sunny: Hi! I'm Sunny of Sunday, meaning it's Sunday today!|I was told to give you this if I saw you!",
             "Sunny: That thing...|Um...|...What was it now...|...|Oh! I remember now!|A Pokemon that knows Electric-type moves should hold it.|My sis Monica said it powers up Electric-type moves!",
             "Sunny: My sister Monica... My sister Tuscany... My brother Wesley... My brother Arthur... My sister Frieda... And my brother Santos.|They're all older than me!",
             "Sunny: Huh? Isn't today Sunday? Um... I forgot!"}};
        int s = std::max(0, std::min(6, std::atoi(a[1].c_str())));
        const Sib& S = kSib[s];
        Profile& P = WP(g, i);
        auto met = [&](int q) { return Flags(g).count(std::string("sib:") + kSib[q].name + ":" + P.name) > 0; };
        int metAll = 0;
        for (int q = 0; q < 7; q++) metAll += met(q);
        time_t now = time(nullptr);
        bool today = localtime(&now)->tm_wday == S.day;
        int lead = -1;
        for (int m = 0; m < (int)P.mons.size() && lead < 0; m++) if (!P.mons[m].boxed) lead = m;
        if (metAll == 7 && today && lead >= 0) {   // friends now: a ribbon for your lead Pokemon, once a week
            OwnedMon& om = P.mons[lead];
            std::string nm = MonName(om);
            if (std::find(om.ribbons.begin(), om.ribbons.end(), S.ribbon) != om.ribbons.end()) { Say(w, std::string(S.name) + ": That " + S.ribbon + " looks great on your Pokemon..."); return; }
            std::string wk = TextFormat("sibrib:%s:%s|%d", S.name, P.name.c_str(), (RaidToday() + 3) / 7);
            if (Flags(g).count(wk)) { Say(w, "You can only get one ribbon per week...|If you want another ribbon, come find me again next week!"); return; }
            SetFlag(g, wk);
            om.ribbons.push_back(S.ribbon);
            SaveAll(g);
            PlaySfx(Sfx::ItemGet);
            Say(w, std::string(S.name) + ": Hey, you! The one who found all of my siblings!|We're pretty good friends now, don't you agree?|"
                   "Since we're such good friends, I'll give you this " + S.ribbon + "!|" + nm + " put on the " + S.ribbon + "!");
            return;
        }
        if (met(s)) { Say(w, S.later); return; }
        if (!today) { Say(w, S.wrong); return; }
        std::string item = FindItem(S.item) ? S.item : (std::string(S.item) == "Twisted Spoon" && FindItem("TwistedSpoon") ? "TwistedSpoon" : S.item);
        BagAdd(P, item);
        SetFlag(g, std::string("sib:") + S.name + ":" + P.name);
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, std::string(S.hi) + "|You received the " + item + "!|" + S.about);
    }
    else if (k == "moomoo" || k == "moomoofeed" || k == "moomoogirl" || k == "moomoofarmer" || k == "moomoowife") {   // Moomoo Farm (HeartGold)
        Profile& P = WP(g, i);
        bool healed = Flags(g).count("moomoo:healed") > 0;
        int fed = 0;
        { std::string v = FlagStr(g, "moomoo|"); if (!v.empty()) fed = std::atoi(v.c_str()); }
        if (k == "moomoogirl") { Say(w, healed ? "Moomoo is totally healthy again!|We sell Moomoo Milk at home. Don't forget to drop by!" : "Moomoo is sick...|She needs lots of Oran Berries."); return; }
        if (k == "moomoofarmer") {
            if (!healed) { Say(w, "My Miltank ain't givin' me milk n'more.|This here farm's got famous milk. Most everyone wants a drink.|It'll give me lots o' milk if'n I feed it lots o' Berries, I reckon."); return; }
            Ask(w, TextFormat("How'd you like my Moomoo Milk?|It's my pride and joy, there. Give it to Pokemon to restore HP!|I'll give it to ya fer just 500.|(You have %d coins.)", P.coins),
                {{"1 bottle (500)", "buygo|Moomoo Milk|500"}, {"1 dozen (6000)", "moomoodozen"}, {"No thanks", "msg|You don't want it? Come again, hear?"}});
            return;
        }
        if (k == "moomoowife") {
            if (!healed) { Say(w, "Our milk even goes out to Kanto.|So if our own Miltank won't give us any milk, we're in trouble."); return; }
            if (Flags(g).count("moomoo:gift:" + P.name)) { Say(w, "That there's Natural Gift.|Its move type and Power will depend on the Berry yer Pokemon holds.|When you use it once, the Berry's gone."); return; }
            SetFlag(g, "moomoo:gift:" + P.name);
            SaveAll(g);
            RunAction(g, i, "tutor|Natural Gift|You fixed our Miltank, hon. Now it gives Moomoo Milk again.|Here's somethin' fer your trouble - I'll teach one of yer Pokemon Natural Gift.");
            return;
        }
        if (healed) { Say(w, "Miltank: Mooo!|It's brimming with energy."); return; }
        if (k == "moomoo") {
            Ask(w, "Miltank: ...Moo...|Its cry is weak...|Give an Oran Berry to Miltank?", {{"Yes", "moomoofeed"}, {"No", "msg|{PLAYER} wouldn't give an Oran Berry.|Miltank looks sad."}});
            return;
        }
        if (!BagTake(P, "Oran Berry")) { Say(w, "{PLAYER} has no Oran Berries..."); return; }
        fed++;
        auto& fl = Flags(g);
        for (auto it = fl.begin(); it != fl.end();) it = it->rfind("moomoo|", 0) == 0 ? fl.erase(it) : std::next(it);
        fl.insert("moomoo|" + std::to_string(fed));
        std::string t = "{PLAYER} gave an Oran Berry to Miltank.";
        if (fed >= 7) { SetFlag(g, "moomoo:healed"); t += "|Miltank became totally healthy!"; PlaySfx(Sfx::Heal); }
        else if (fed == 5) t += "|Miltank is getting healthier!";
        else if (fed == 3) t += "|Miltank became a little healthier!";
        SaveAll(g);
        Say(w, t);
    }
    else if (k == "moomoodozen") {
        Profile& P = WP(g, i);
        if (P.coins < 6000) { Say(w, "Sorry, there. No cash, no sale!"); return; }
        P.coins -= 6000;
        for (int n = 0; n < 12; n++) BagAdd(P, "Moomoo Milk");
        SaveAll(g);
        PlaySfx(Sfx::Shop);
        Say(w, "Here ya go! Drink up'n enjoy!|You received 12 Moomoo Milk!");
    }
    else if ((k == "haircut" || k == "haircutgo") && a.size() > 1) {   // the Haircut Brothers in the Goldenrod Tunnel (HeartGold)
        Profile& P = WP(g, i);
        bool older = a[1] == "older";
        int price = older ? 500 : 300;
        std::string today = TextFormat("haircut:%s|%d", P.name.c_str(), RaidToday());
        if (k == "haircut") {
            if (Flags(g).count(today)) { Say(w, older ? "I do only one haircut a day, sorry. I'm done for today." : "I have a feeling that it won't go well next time. Let me close for the day!"); return; }
            std::vector<std::pair<std::string, std::string>> ch;
            for (int m = 0; m < (int)P.mons.size(); m++) if (!P.mons[m].boxed) ch.push_back({MonName(P.mons[m]), "haircutgo|" + a[1] + "|" + std::to_string(m)});
            ch.push_back({"No thanks", older ? "msg|Is that right? That's a shame!" : "msg|No? How disappointing!"});
            Ask(w, std::string(older ? "Welcome! I run the Pokemon Salon!|I'm the older and better of the two Haircut Brothers.|I can make your Pokemon beautiful for just 500."
                                     : "Welcome to the Pokemon Salon!|I'm the younger and less expensive of the two Haircut Brothers.|I'll spruce up your Pokemon for just 300.") +
                       "|Grooming gives a better look. Your Pokemon will become friendlier and more beautiful.|" + (older ? "Which Pokemon should I work on?" : "OK, which Pokemon should I spruce up?"),
                ch);
            return;
        }
        int m = a.size() > 2 ? std::atoi(a[2].c_str()) : -1;
        if (m < 0 || m >= (int)P.mons.size()) return;
        if (P.coins < price) { Say(w, older ? "You'll need more money than that." : "You're a little short on funds."); return; }
        P.coins -= price;
        SetFlag(g, today);
        // (the older brother: 60% a big boost; the younger: 60% a small one)
        int r = SimRand(0, 99), up = older ? (r >= 40 ? 10 : r >= 10 ? 5 : 1) : (r >= 40 ? 1 : r >= 10 ? 5 : 10);
        OwnedMon& om = P.mons[m];
        om.friendship = std::min(MAX_FRIENDSHIP, om.friendship + up);   // (HeartGold's +1 / +5 / +10)
        SaveAll(g);
        PlaySfx(Sfx::Heal);
        std::string nm = MonName(om);
        Say(w, std::string(older ? "OK! Watch it become beautiful!|...|There! All done!" : "OK! I hope I do a good job!|...|There we go! All done!") + "|" +
                   nm + (up >= 10 ? " looks delighted!" : up >= 5 ? " looks happy." : " looks a little happier."));
    }
    else if (k == "sundaylady") {   // Goldenrod Dept. Store 5F, Sundays: a move for a Pokemon who loves you - or doesn't (Return / Frustration)
        Profile& P = WP(g, i);
        int lead = -1;
        for (int m = 0; m < (int)P.mons.size() && lead < 0; m++) if (!P.mons[m].boxed) lead = m;
        if (lead < 0) return;
        std::string wk = TextFormat("sunday:%s|%d", P.name.c_str(), (RaidToday() + 3) / 7);
        if (Flags(g).count(wk)) { Say(w, "There are sure to be TMs that are just perfect for your Pokemon."); return; }
        const OwnedMon& om = P.mons[lead];
        if (om.friendship >= 150) { SetFlag(g, wk); RunAction(g, i, "tutor|Return|Hello. Oh, your Pokemon...|It's very attached to you.|This move should be perfect for a pair like you."); return; }
        if (om.friendship <= 50) { SetFlag(g, wk); RunAction(g, i, "tutor|Frustration|Hello. Oh, your Pokemon...|It looks a little bit evil. How about this move for it?"); return; }
        Say(w, "Hello. Oh, your Pokemon...|It's adorable!|You should teach it good TM moves.");
    }
    else if (k == "castform") {   // the Weather Institute's scientist: Castform, for chasing Team Aqua off (Emerald)
        Profile& P = WP(g, i);
        if (!HasFlag(g, i, "stage:Weather Institute") || Flags(g).count("gift:castform:" + P.name)) { RunAction(g, i, "abweather"); return; }
        RunAction(g, i, "giftmon|Castform|25|castform|Thanks! Thanks to you, we're safe!/It might be an odd way of thanking you, but take this Pokemon.|"
                        "That Pokemon changes shape according to the weather conditions./There're plenty of them in the Institute--go ahead and take it.");
        if (Flags(g).count("gift:castform:" + P.name)) {   // (holding Mystic Water)
            for (auto it = P.mons.rbegin(); it != P.mons.rend(); ++it) if (it->species == "Castform") { if (it->item.empty() && FindItem("Mystic Water")) it->item = "Mystic Water"; break; }
            SaveAll(g);
        }
    }
    else if (k == "berrymaster") {   // Route 123: the Berry Master's two berries a day (Emerald: Pomeg to Nomel)
        static const char* kB[] = {"Pomeg Berry", "Kelpsy Berry", "Qualot Berry", "Hondew Berry", "Grepa Berry", "Tamato Berry", "Cornn Berry", "Magost Berry", "Rabuta Berry", "Nomel Berry"};
        Profile& P = WP(g, i);
        std::string today = TextFormat("berrymaster:%s|%d", P.name.c_str(), RaidToday());
        if (Flags(g).count(today)) { Say(w, "I'm done for today. Come again another day.|Let flowers fill the world!"); return; }
        std::vector<std::string> have;
        for (const char* b : kB) if (FindItem(b)) have.push_back(b);
        if (have.empty()) return;
        std::string b1 = have[SimRand(0, (int)have.size() - 1)], b2 = have[SimRand(0, (int)have.size() - 1)];
        BagAdd(P, b1); BagAdd(P, b2);
        SetFlag(g, today);
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, "You may call me the Berry Master.|I dream of filling the world with beautiful flowers, so I raise Berries and hand them out to everyone.|You're deserving of one!|"
               "You received a " + b1 + "!|Why be stingy? Take another!|You received a " + b2 + "!|Be sure to visit the Pretty Petal flower shop near Rustboro.|Let flowers fill the world!");
    }
    else if (k == "berrywife" || k == "berrywifesaid") {   // ...and his wife: a good saying (Easy Chat) for a berry; the right ones, a rare one
        Profile& P = WP(g, i);
        std::string today = TextFormat("berrywife:%s|%d", P.name.c_str(), RaidToday());
        if (k == "berrywife") {
            if (Flags(g).count(today)) { Say(w, "Our four grandchildren should become more accomplished than my husband.|Joy never goes out of my life!"); return; }
            Say(w, "The way my husband grows Berries, oh, he's the best in the world.|He makes me proud, that he does.|Incidentally, child, have you heard a good saying lately?",
                "ezchat|2|A good saying for the Berry Master's wife|berrywifesaid");
            return;
        }
        if (a.size() > 2 && a[2] == "cancel") { Say(w, "Ah...|Our four grandchildren should become more accomplished than my husband.|Joy never goes out of my life!"); return; }
        std::string said = a.size() > 1 ? a[1] : "";
        struct Ph { const char* words; const char* berry; };
        static const Ph kPh[] = {{"Great+Battle", "Spelon Berry"}, {"Challenge+Contest", "Pamtre Berry"}, {"Overwhelming+Latias", "Watmel Berry"},
                                 {"Cool+Latios", "Durin Berry"}, {"Super+Hustle", "Belue Berry"}};
        std::string berry;
        for (const auto& ph : kPh)
            if (said == ph.words && FindItem(ph.berry) && !Flags(g).count(std::string("berrywife:") + ph.berry + ":" + P.name)) { berry = ph.berry; SetFlag(g, std::string("berrywife:") + ph.berry + ":" + P.name); }
        bool special = !berry.empty();
        if (!special) {
            static const char* kB[] = {"Cheri Berry", "Chesto Berry", "Pecha Berry", "Rawst Berry", "Aspear Berry", "Leppa Berry", "Oran Berry", "Persim Berry", "Lum Berry", "Sitrus Berry"};
            std::vector<std::string> have;
            for (const char* b : kB) if (FindItem(b)) have.push_back(b);
            berry = have.empty() ? "Oran Berry" : have[SimRand(0, (int)have.size() - 1)];
        }
        BagAdd(P, berry);
        SetFlag(g, today);
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, std::string(special ? "Ah! What a remarkable saying! Inspirational, it is!" : "Oh! A good saying it is. You're quite remarkable.") + "|I want you to have this.|You received a " + berry +
                   "!|Our four grandchildren should become more accomplished than my husband.|Joy never goes out of my life!");
    }
    else if (k == "shardman" || k == "shardtrade") {   // Route 124's Diving Treasure Hunter: a Shard for an evolution stone (Emerald)
        static const char* kShard[4] = {"Red Shard", "Yellow Shard", "Blue Shard", "Green Shard"};
        static const char* kStone[4] = {"Fire Stone", "Thunder Stone", "Water Stone", "Leaf Stone"};
        Profile& P = WP(g, i);
        if (k == "shardman") {
            std::vector<std::pair<std::string, std::string>> ch;
            for (int s2 = 0; s2 < 4; s2++) if (BagCount(P, kShard[s2]) > 0) ch.push_back({std::string(kShard[s2]) + " -> " + kStone[s2], "shardtrade|" + std::to_string(s2)});
            if (ch.empty()) { Say(w, "I'm the Diving Treasure Hunter!|I'm the awesome dude who makes deep-sea dives to gather treasures resting at the bottom.|Tell me, have you seen any Shards of tools made in ancient times?|You haven't got any treasures for me...|If you see any Shards, like the Red Shard, you've got to trade it with me!"); return; }
            ch.push_back({"Not now", "msg|No? That's a downer. Well, if you change your mind, come back."});
            Ask(w, "Oh, hey! That... That's a Shard! I'm looking for those!|Oh, man, you've got to trade that with me! I'll give you something good!|What do you want to trade?", ch);
            return;
        }
        int s2 = std::max(0, std::min(3, a.size() > 1 ? std::atoi(a[1].c_str()) : 0));
        if (!BagTake(P, kShard[s2])) return;
        BagAdd(P, kStone[s2]);
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, std::string("It's a done deal! Use that wisely!|You traded your ") + kShard[s2] + " for a " + kStone[s2] + "!");
    }
    else if (k == "roulette") {   // Mauville's roulette tables (Emerald): roulette|0 the 1-coin table, |1 the 3-coin one
        if (g.coop) { Say(w, "(The Game Corner's tables are for playing on your own.)"); return; }
        int table = a.size() > 1 ? std::atoi(a[1].c_str()) : 0;
        if (g.profile.casino < (table ? 3 : 1)) { Say(w, "It's a roulette table. You need Game Corner coins to play - buy some at the counter."); return; }
        SaveAll(g);
        OpenRoulette(g, table);
    }
    else if (k == "statue") {}   // (a stone statue: nothing to say)
    else if (k == "quiz3" && a.size() > 3) {   // quiz3|flag|x,y|q1~q2~q3 (each "correct^question^A^B^C"): a Mechadoll asks; wrong: back to x, y
        if (HasFlag(g, i, a[1])) return;
        auto qs = Split(a[3], '~');
        auto q = Split(qs[SimRand(0, (int)qs.size() - 1)], '^');
        if (q.size() < 3) return;
        int right = std::atoi(q[0].c_str());
        std::string text = q[1];
        for (char& ch : text) if (ch == '/') ch = '|';
        std::vector<std::pair<std::string, std::string>> ch;
        for (size_t c = 2; c < q.size(); c++) ch.push_back({q[c], (int)c - 2 == right ? "quizright|" + a[1] : "quizwrong|" + a[2]});
        w.spin = -1;
        Ask(w, text, ch);
    }
    else if (k == "quizright" && a.size() > 1) { SetFlag(g, a[1]); PlaySfx(Sfx::ItemGet); Say(w, "Congratulations. Correct you are. Go through, please."); }
    else if (k == "quizwrong" && a.size() > 1) {   // back to the start of the room
        PlaySfx(Sfx::Bump);
        auto xy = Split(a[1], ',');
        Say(w, "Disappointment. Error you are.|Wahahahaha!|Wait for the next challenge!",
            "warp|" + MapOf(w).name + "|" + xy[0] + "|" + (xy.size() > 1 ? xy[1] : "0") + "|up");
    }
    else if (k == "trickgo" || k == "trickscroll" || k == "trickreward") {   // the Trick House (Emerald): the Trick Master's eight puzzles
        auto& fl = Flags(g);
        int level = 0;
        for (const auto& f : fl) if (f.rfind("tricklevel|", 0) == 0) level = std::atoi(f.c_str() + 11);
        auto setLevel = [&](int n) {
            for (auto it = fl.begin(); it != fl.end();) it = it->rfind("tricklevel|", 0) == 0 ? fl.erase(it) : std::next(it);
            fl.insert("tricklevel|" + std::to_string(n));
        };
        static const char* kBadge[9] = {"", "", "Dynamo", "Heat", "Balance", "Feather", "Mind", "Rain", "Rain"};   // what each puzzle waits for
        if (k == "trickgo") {
            int n = level + 1;
            if (n > 8) { Say(w, "There's a note: \"I, the Trick Master, have gone on a journey to devise new tricks. Wait for me!\""); return; }
            if (n >= 2 && !HasBadge(g, kBadge[n])) { Say(w, "Trick Master: I'm still preparing the next trick! Come back once you've earned more Badges."); return; }
            for (auto it = fl.begin(); it != fl.end();) it = it->rfind("thp:", 0) == 0 ? fl.erase(it) : std::next(it);   // (its buttons as they start)
            Warp(g, i, {"warp", TextFormat("Route 110 Trick House Puzzle %d", n), "0", "21", "up"});
        } else if (k == "trickscroll" && a.size() > 1) {
            int n = std::atoi(a[1].c_str());
            std::string f = "trick:scroll" + std::to_string(n);
            if (fl.count(f)) { Say(w, "{PLAYER} reread the secret code on the scroll: \"Trick Master is fabulous.\""); return; }
            SetFlag(g, f);
            PlaySfx(Sfx::ItemGet);
            Say(w, "{PLAYER} found a scroll.|{PLAYER} memorized the secret code written on the scroll.|\"Trick Master is fabulous.\"|...The door to the next room should open with it.");
        } else {   // the Trick Master at the end: that puzzle's prize
            static const char* kPrize[9] = {"", "Rare Candy", "Timer Ball", "Hard Stone", "Smoke Ball", "TM087 Taunt", "Magnet", "PP Max", "Nugget"};
            int n = level + 1;
            if (n > 8 || !fl.count("trick:scroll" + std::to_string(n))) { Say(w, "Trick Master: Aak! You've made it to me? Hmmm... You're sharp!"); return; }
            setLevel(n);
            std::string prize = kPrize[n];
            if (!FindItem(prize)) prize = "Nugget";
            BagAdd(WP(g, i), prize);
            SaveAll(g);
            PlaySfx(Sfx::ItemGet);
            Say(w, std::string("Trick Master: Aak! You've made it to me? Hmmm... You're sharp!|Fine! You've earned the reward for getting through my Trick House!|You got the ") + prize + "!" +
                   (n < 8 ? "|Next time, there'll be an even trickier trick waiting for you!" : "|You've solved all my tricks... I'll go on a journey to think up new ones!"));
        }
    }
    else if (k == "toggle" && a.size() > 1) {
        if (Flags(g).count(a[1])) { Flags(g).erase(a[1]); if (Online(g)) OnlineSendCoop(g); }
        else SetFlag(g, a[1]);
        PlaySfx(Sfx::Door);
        Say(w, "Who wouldn't press it?|*Click*|Somewhere in the building, the barriers moved!");
    }
    else if (k == "contest") {   // Lilycove's Contest Hall
        if (Online(g)) { Say(w, "Welcome to the Pokemon Contest Hall!|(Contests are for playing on your own or in local co-op.)"); return; }
        Ask(w, act.size() > 8 ? act.substr(8) : "Hello! This is the Pokemon Contest reception.|Would you like to enter a Contest?", {{"Enter a Contest", "contestopen"}, {"No thanks", ""}});
    }
    else if (k == "contestopen") {
        SaveAll(g);
        Game& t = g.coop ? StartSub(g, i) : g;
        OpenContest(t, 0);
    }
    else if (k == "blender") {   // the Berry Blender: a berry in, a Pokeblock out
        Profile& P = WP(g, i);
        std::vector<std::pair<int, std::string>> have;
        for (const auto& [name, n] : P.bag) if (n > 0 && BerryIndexOf(name) >= 0) have.push_back({n, name});
        if (have.empty()) { Say(w, "You have no Berries. The Berry Blender can't be used."); return; }
        std::sort(have.begin(), have.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
        std::vector<std::pair<std::string, std::string>> ch;
        for (int k2 = 0; k2 < (int)have.size() && k2 < 8; k2++) ch.push_back({TextFormat("%s (%d)", have[k2].second.c_str(), have[k2].first), "blend|" + have[k2].second});
        ch.push_back({"Not now", ""});
        Ask(w, act.size() > 8 ? act.substr(8) : "Let's blend! Which Berry will you put in the Berry Blender?", ch);
    }
    else if (k == "blend" && a.size() > 1) {
        Profile& P = WP(g, i);
        int bi = BerryIndexOf(a[1]);
        if (bi < 0 || !BagTake(P, a[1])) return;
        std::string nm = BerryAt(bi).name;
        std::string block = "Pokeblock (" + nm.substr(0, nm.find(' ')) + ")";
        BagAdd(P, block, 1);
        SaveAll(g);
        if (g.coop) SaveProfile(P);
        PlaySfx(Sfx::ItemGet);
        Say(w, "Whirrr... The Berry Blender spun the " + a[1] + " into a " + block + "!|Feed it to a Pokemon from Party & Box to make it shine in Contests.", "blender");
    }
    else if (k == "flag" && a.size() > 1) SetFlag(g, a[1]);
    else if (k == "standuse" && a.size() > 1) {   // a tomb's puzzle, solved
        for (const auto& st : MapOf(w).stands)
            if (st.flag == a[1] && !HasFlag(g, i, st.flag)) {
                SetFlag(g, st.flag);
                PlaySfx(Sfx::Thud);
                Say(w, st.text.empty() ? "There's a rumbling... Something opened!" : st.text);
                SaveAll(g);
                break;
            }
    }
    else if (k == "lapstart" && a.size() > 1) {   // Island Cave's braille: the lap starts here
        gLapOn[i & 1] = !HasFlag(g, i, MapOf(w).lapFlag);
        gLapMask[i & 1] = 0;
        Say(w, act.substr(9));
    }
    else if (k == "regicheck" && a.size() > 2) {   // regicheck|flag|braille: the Sealed Chamber - Wailord first, Relicanth last
        const Profile& P = WP(g, i);
        std::vector<std::string> party;
        for (const auto& mm : P.mons) if (!mm.boxed && mm.eggSteps <= 0) party.push_back(mm.species);
        std::string text = act.substr(k.size() + a[1].size() + 2);
        if (!HasFlag(g, i, a[1]) && !party.empty() && party.front() == "Wailord" && party.back() == "Relicanth") {
            SetFlag(g, a[1]);
            SaveAll(g);
            PlaySfx(Sfx::Thud);
            Say(w, text + "|...!|The ground shook! It sounded like a door opened somewhere far away...");
        } else Say(w, text);
    }
    else if ((k == "berrypick" || k == "berrywater" || k == "berryplant") && a.size() > 1) {
        const OwMap::BerryTree* t = TreeById(std::atoi(a[1].c_str()));
        if (!t) return;
        TreeState s = TreeOf(g, *t);
        int st = TreeStage(g, s);
        Profile& P = WP(g, i);
        if (k == "berrypick" && st >= 4) {
            int n = TreeYield(s);
            BagAdd(P, s.berry + " Berry", n);
            SetTree(g, t->id, "", 0, 0);
            if (g.coop) SaveProfile(P);
            SaveAll(g);
            PlaySfx(Sfx::ItemGet);
            Say(w, "{PLAYER} picked " + std::to_string(n) + " " + s.berry + " Berr" + (n == 1 ? "y" : "ies") + ".|{PLAYER} put away the Berr" + (n == 1 ? "y" : "ies") + " in the Bag.|The soil returned to its soft and loamy state.");
        } else if (k == "berrywater" && st >= 0 && st < 4) {
            SetTree(g, t->id, s.berry, s.planted, s.water | (1 << st));
            PlaySfx(Sfx::Buff);
            Say(w, "{PLAYER} watered the " + s.berry + " Berry.|The plant seems to be delighted.");
        } else if (k == "berryplant" && a.size() > 2 && st < 0 && BagTake(P, a[2])) {
            std::string b = a[2].substr(0, a[2].size() > 6 ? a[2].size() - 6 : a[2].size());
            SetTree(g, t->id, b, StorySeconds(g), 0);
            if (g.coop) SaveProfile(P);
            SaveAll(g);
            PlaySfx(Sfx::MenuOk);
            Say(w, "{PLAYER} planted one " + a[2] + " in the soft, loamy soil.");
        }
    }
    else if (k == "event" && a.size() >= 3) {   // event|flag|text/text: a little scene, then the flag is set (Gideon snatching the Sapphire)
        if (HasFlag(g, i, a[1])) return;
        std::string t;
        for (size_t q = 2; q < a.size(); q++) t += (q > 2 ? "|" : "") + a[q];
        for (char& ch : t) if (ch == '/') ch = '|';
        Say(w, t, "flag|" + a[1]);
    }
    else if (k == "give" && a.size() >= 4) {   // give|item|n|flag
        if (HasFlag(g, i, a[3])) return;
        int n = std::max(1, std::atoi(a[2].c_str()));
        BagAdd(WP(g, i), a[1], n);
        SetFlag(g, a[3]);
        SaveAll(g);
        if (g.coop) SaveProfile(WP(g, i));
        PlaySfx(Sfx::ItemGet);
        Say(w, (n > 1 ? TextFormat("You got %d x %s!", n, a[1].c_str()) : "You got the " + a[1] + "!") + std::string("|") + PocketHint(a[1]));
    }
    else if (k == "field" && a.size() > 1) UseField(g, i, a[1]);
    else if (k == "fish" && a.size() > 1) Fish(g, i, a[1]);
    else if (k == "fly") { g.ow.townMap = true; g.ow.townSel = 0; g.ow.menuWho = i; }
    else if (k == "trashcan" && a.size() > 1) TrashCan(g, i, a[1]);
    else if (k == "raid" || k == "tower" || k == "legend" || k == "frontier" || k == "frontiergo" || k == "pikedoor" || k == "castleheal") WorldChallenge(g, i, a);
    else if (k == "encounter" && a.size() >= 4) {   // encounter|Species|level|flag|text|item needed|text without it
        if (HasFlag(g, i, a[3])) return;
        for (auto& x : a) for (char& ch : x) if (ch == '/') ch = '|';   // ("/" between pages)
        if (a.size() > 6 && !a[5].empty() && BagCount(WP(g, i), a[5]) == 0) { Say(w, a[6]); return; }
        Say(w, a.size() > 4 && !a[4].empty() ? a[4] : "The wild " + a[1] + " attacked!", "static|" + a[1] + "|" + a[2] + "|" + a[3]);
    }
    else if (k == "deliver" && a.size() >= 4) {   // deliver|Item|flag|text on handing it over|reward item|n|text without it|text after
        for (auto& x : a) for (char& ch : x) if (ch == '/') ch = '|';   // ("/" between pages)
        if (HasFlag(g, i, a[2])) { Say(w, a.size() > 7 && !a[7].empty() ? a[7] : "Thank you!"); return; }
        if (BagCount(WP(g, i), a[1]) == 0) { Say(w, a.size() > 6 && !a[6].empty() ? a[6] : "..."); return; }
        BagTake(WP(g, i), a[1]);
        SetFlag(g, a[2]);
        std::string t = a[3];
        if (a.size() > 5 && !a[4].empty()) {
            int n = std::max(1, std::atoi(a[5].c_str()));
            BagAdd(WP(g, i), a[4], n);
            t += "|" + (n > 1 ? TextFormat("You got %d x %s!", n, a[4].c_str()) : "You got the " + a[4] + "!") + std::string("|") + PocketHint(a[4]);
            PlaySfx(Sfx::ItemGet);
        }
        SaveAll(g);
        if (g.coop) SaveProfile(WP(g, i));
        Say(w, t);
    }
    else if (k == "starter" && a.size() > 2 && std::atoi(a[2].c_str()) > 0) {   // starter|Species|region: Elm's table (Johto), ...
        int r = std::atoi(a[2].c_str());
        std::string prof = r == 1 ? "Elm" : r == 2 ? "Birch" : r == 3 ? "Rowan" : "Juniper";
        if (!RegionStarter(WP(g, i), r).empty()) { Say(w, "That's Prof. " + prof + "'s Pokemon..."); return; }
        if (Flags(g).count("ball:" + a[1])) return;
        int sp = SpeciesIndex(a[1]);
        if (sp < 0) return;
        std::string type = TypeName(AllSpecies()[sp].type1);
        Ask(w, prof + ": You'll take " + a[1] + ", the " + type + "-type Pokemon?",
            {{"Yes", "starteryes|" + a[1] + "|" + a[2]}, {"No", ""}});
    }
    else if (k == "starteryes" && a.size() > 2 && std::atoi(a[2].c_str()) > 0) {
        int r = std::atoi(a[2].c_str());
        std::string prof = r == 1 ? "Elm" : r == 2 ? "Birch" : r == 3 ? "Rowan" : "Juniper";
        Profile& P = WP(g, i);
        int sp = SpeciesIndex(a[1]);
        if (sp < 0 || !RegionStarter(P, r).empty() || Flags(g).count("ball:" + a[1])) return;
        int who = g.coop ? WhoOf(g, i) : 0;
        bool boxed = AddMon(P, NewMon(sp, P.mons.empty() ? Story().startLevel : RegionGiftLevel(g, r, who)));
        RegionStarter(P, r) = a[1];
        if (!g.coop) RegionRival(P, r).clear();
        DexRegisterOwned(P);
        SetFlag(g, "ball:" + a[1]);
        if (g.coop) { SaveProfile(P); OnlineSendProfile(g); }
        EnsureRival(g, r);   // the rival takes the one that beats yours (co-op: once you both have yours)
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, prof + ": I think that's a great Pokemon too!|{PLAYER} received the " + a[1] + " from Prof. " + prof + "!" +
                   (boxed ? "|(Your party is full: it was sent to your Box.)" : ""));
    }
    else if (k == "starter" && a.size() > 1) {   // starter|Species: a Poke Ball on Oak's table
        if (!WP(g, i).mons.empty()) { Say(w, "That's Prof. Oak's last Pokemon..."); return; }
        if (Flags(g).count("ball:" + a[1])) return;
        int sp = SpeciesIndex(a[1]);
        if (sp < 0) return;
        std::string type = TypeName(AllSpecies()[sp].type1);
        Ask(w, "Oak: Ah! " + a[1] + " is your choice.|So, {PLAYER}, you want to go with the " + type + " Pokemon " + a[1] + "?",
            {{"Yes", "starteryes|" + a[1]}, {"No", ""}});
    }
    else if (k == "starteryes" && a.size() > 1) {
        Profile& P = WP(g, i);
        int sp = SpeciesIndex(a[1]);
        if (sp < 0 || !P.mons.empty() || Flags(g).count("ball:" + a[1])) return;
        P.mons.push_back(NewMon(sp, Story().startLevel));
        RegionStarter(P, 0) = a[1];
        if (!g.coop) { P.starter = a[1]; P.rival.clear(); }
        DexRegisterOwned(P);
        SetFlag(g, "ball:" + a[1]);
        if (g.coop) { SaveProfile(P); OnlineSendProfile(g); }
        if (!g.coop || (!g.coopP[0].mons.empty() && !g.coopP[1].mons.empty())) EnsureRival(g, 0);   // Blue takes the one that beats yours (co-op: once you both have yours)
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        std::string rv = g.coop ? g.coopSave.rival[0] : g.profile.rival;
        Say(w, "Oak: This Pokemon is really quite energetic!|{PLAYER} received the " + a[1] + " from Prof. Oak!" +
                   (!rv.empty() && rv != a[1] ? "|Blue: I'll take this one, then!|Blue received the " + rv + " from Prof. Oak!" : std::string()));
    }
    else if (k == "sailto" && a.size() >= 5) {   // sailto|question|condition|refusal|Label;Map;x;y;face;condition|...: Mr. Briney's boat, the S.S. Tidal
        if (!a[2].empty() && !HasFlag(g, i, a[2])) { Say(w, a[3].empty() ? "Sorry, we can't sail right now." : a[3]); return; }
        std::vector<std::pair<std::string, std::string>> ch;
        for (size_t q = 4; q < a.size(); q++) {
            auto p = Split(a[q], ';');
            if (p.size() > 5 && !p[5].empty() && !HasFlag(g, i, p[5])) continue;   // not yet (that region's League)
            if (p.size() >= 4 && MapIndex(p[1]) >= 0 && p[1] != MapOf(w).name)
                ch.push_back({p[0], "boat|" + p[1] + "|" + p[2] + "|" + p[3] + "|" + (p.size() > 4 ? p[4] : "down") + "|" + p[0]});
        }
        if (ch.empty()) { Say(w, a[3].empty() ? "Sorry, that service isn't running." : a[3]); return; }
        ch.push_back({"Not now", ""});
        Ask(w, a[1], ch);
    }
    else if (k == "boat" && a.size() >= 6) {   // boat|Map|x|y|face|Label: off you sail
        Warp(g, i, {"warp", a[1], a[2], a[3], a[4]});
        Say(w, "Off we go! Full speed ahead!|...|Land ho! Here we are at " + a[5] + "!");
    }
    else if (k == "starterbag" && a.size() >= 3) {   // starterbag|region|A,B,C|text: Prof. Birch's bag on Route 101 - pick one
        std::vector<std::pair<std::string, std::string>> ch;
        for (const auto& sp : Split(a[2], ',')) if (SpeciesIndex(Trim(sp)) >= 0 && !Flags(g).count("ball:" + Trim(sp))) ch.push_back({Trim(sp), "starter|" + Trim(sp) + "|" + a[1]});
        int r = std::atoi(a[1].c_str());
        if (!RegionStarter(WP(g, i), r).empty() || ch.empty()) { Say(w, "It's Prof. Birch's bag..."); return; }
        ch.push_back({"Not yet", ""});
        Ask(w, a.size() > 3 && !a[3].empty() ? a[3] : "There's a Poke Ball in the bag! Which one?", ch);
    }
    else if (k == "travel" && a.size() >= 5) {   // travel|question|Map|x|y|face|condition|refusal: the S.S. Aqua, the Magnet Train
        std::string cond = a.size() > 6 ? a[6] : "";
        if (!cond.empty() && !HasFlag(g, i, cond)) { Say(w, a.size() > 7 && !a[7].empty() ? a[7] : "Sorry, you can't go there yet."); return; }
        if (MapIndex(a[2]) < 0) { Say(w, "Sorry, that service isn't running."); return; }
        Ask(w, a[1], {{"Yes", "warp|" + a[2] + "|" + a[3] + "|" + a[4] + "|" + (a.size() > 5 ? a[5] : "down")}, {"No", ""}});
    }
    else if (k == "ferry") {   // ferry|text: the Seagallop to the places your passes allow
        std::vector<std::pair<std::string, std::string>> ch;
        for (const auto& f : gFerries)
            if (f.map != MapOf(w).name && MapIndex(f.map) >= 0 && (f.cond.empty() || HasFlag(g, i, f.cond)))
                ch.push_back({f.label, "sail|" + f.map + "|" + std::to_string(f.x) + "|" + std::to_string(f.y) + "|" + f.label});
        if (ch.empty()) { Say(w, "Sorry, you need a pass to board the Seagallop."); return; }
        ch.push_back({"Cancel", ""});
        Ask(w, a.size() > 1 && !a[1].empty() ? a[1] : "Where would you like to sail?", ch);
    }
    else if (k == "sail" && a.size() >= 5) { Warp(g, i, {"warp", a[1], a[2], a[3]}); Say(w, "All aboard! The Seagallop is setting sail...|Here we are! " + a[4] + "!"); }
    else if (k == "static" && a.size() >= 4) StaticEncounter(g, i, a[1], std::atoi(a[2].c_str()), a[3]);
    else if (k == "quiz" && a.size() >= 4) {   // quiz|question|yes or no|flag|trainer (a wrong answer: you battle them)
        if (HasFlag(g, i, a[3])) { Say(w, "The gate is already open."); return; }
        std::string rest = a[2] + "|" + a[3] + "|" + (a.size() > 4 ? a[4] : "");
        Ask(w, a[1], {{"Yes", "quizans|yes|" + rest}, {"No", "quizans|no|" + rest}});
    }
    else if (k == "quizans" && a.size() >= 4) {
        if (a[1] == a[2]) { PlaySfx(Sfx::Buff); SetFlag(g, a[3]); Say(w, "Ding ding ding! You're absolutely right!|The gate opened!"); return; }
        PlaySfx(Sfx::MenuBack);
        const OwMap& m = MapOf(w);
        for (int n = 0; n < (int)m.npcs.size(); n++)
            if (a.size() > 4 && !a[4].empty() && m.npcs[n].kind == "fight" && m.npcs[n].args[0] == a[4] && TrainerReady(g, m.npcs[n])) {
                Say(w, "Bzzzt! Sorry, that's wrong!|" + a[4] + ": Then you'll have to battle me!", "battle:" + std::to_string(n));
                return;
            }
        Say(w, "Bzzzt! Sorry, that's wrong!|Try again!");
    }
    else if (k == "exchange" && a.size() >= 4) {   // exchange|give item|get item|text: the Bike Shop takes the Bike Voucher
        for (auto& x : a) for (char& ch : x) if (ch == '/') ch = '|';
        if (BagCount(WP(g, i), a[2]) > 0) { Say(w, a.size() > 4 ? a[4] : "Thanks again!"); return; }
        if (BagCount(WP(g, i), a[1]) == 0) { Say(w, a[3].empty() ? "Come back when you have a " + a[1] + "!" : a[3]); return; }
        BagTake(WP(g, i), a[1]);
        BagAdd(WP(g, i), a[2]);
        SaveAll(g);
        if (g.coop) SaveProfile(WP(g, i));
        PlaySfx(Sfx::ItemGet);
        Say(w, "Oh, that's a " + a[1] + "!|OK! Here you go!|You exchanged the " + a[1] + " for a " + a[2] + "!|" + PocketHint(a[2]));
    }
}

// Vermilion Gym: two hidden switches under the trash cans; the second is next to the first. A wrong
// second guess resets them (and moves the first, like the games).
void TrashCan(Game& g, int i, const std::string& flag) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    MapRt& rt = gRt[w.map];
    if (HasFlag(g, i, flag)) { Say(w, "Nope! There's only trash here."); return; }
    std::vector<int> cans;
    for (const auto& s : m.spots) if (s.act.rfind("trashcan", 0) == 0) cans.push_back(s.y * m.w + s.x);
    int here = (w.y + DY[w.face]) * m.w + (w.x + DX[w.face]);
    auto near = [&](int a) {   // the cans next to this one (the closest ones)
        std::vector<int> v;
        int best = 1 << 30;
        for (int c : cans) if (c != a) best = std::min(best, std::abs(c % m.w - a % m.w) + std::abs(c / m.w - a / m.w));
        for (int c : cans) if (c != a && std::abs(c % m.w - a % m.w) + std::abs(c / m.w - a / m.w) == best) v.push_back(c);
        return v;
    };
    auto roll = [&]() {
        rt.can1 = cans[SimRand(0, (int)cans.size() - 1)];
        auto v = near(rt.can1);
        rt.can2 = v.empty() ? rt.can1 : v[SimRand(0, (int)v.size() - 1)];
        rt.gotCan1 = false;
    };
    if (cans.empty()) return;
    if (rt.can1 < 0) roll();
    if (!rt.gotCan1) {
        if (here == rt.can1) { rt.gotCan1 = true; PlaySfx(Sfx::Buff); Say(w, "Hey! There's a switch under the trash!|Press it?|The first electric lock opened!"); }
        else Say(w, "Nope! There's only trash here.");
        return;
    }
    if (here == rt.can1) { Say(w, "The first lock is already open."); return; }
    if (here == rt.can2) { PlaySfx(Sfx::LevelUp); SetFlag(g, flag); Say(w, "There's another switch under the trash!|Press it?|The second electric lock opened!|The motorized door opened!"); return; }
    roll();
    PlaySfx(Sfx::MenuBack);
    Say(w, "Nope! There's only trash here.|Hey! The electric locks were reset!");
}

void LeaveWorldForQuit(Game& g) { g.ow.active = false; g.ow.menu = false; if (Online(g)) OnlineSendWorldPos(false, -1, 0, 0, 0); }
void AfterMessage(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    std::string a = w.after;
    w.after.clear();
    if (a.empty()) return;
    if (a.rfind("battle:", 0) == 0) StartStoryFight(g, i, std::atoi(a.c_str() + 7));
    else if (a.rfind("wild|", 0) == 0) WildEncounter(g, i, a.substr(5));
    else if (a == "surfstep") TryStep(g, i, w.face);
    else if (a == "joinfight") { OnlineSetReady(g, 1); g.ow.waitFight = true; }
    else if (a == "joinwild") { OnlineSetReady(g, 2); g.ow.waitFight = true; g.ow.wildBattle = true; g.ow.waitName = "the wild Pokemon"; }
    else if (a == "teamyes") { g.ow.teamUp = true; OnlineSendTeam(2); PlaySfx(Sfx::Buff); }
    else if (a == "quitgame") {
        SaveAll(g);
        if (Online(g)) { LeaveWorldForQuit(g); OnlineLeave(g, "Left the online game. Your progress is saved."); return; }
        if (g.coop) { SaveWho(g, 0); SaveWho(g, 1); SaveCoop(g.coopSave); }
        LeaveWorldForQuit(g);
        GoTitle(g);
    }
    else RunAction(g, i, a);
}

// Teamed up: a 2v2 wild battle for both players (your partner comes over). False: they're busy,
// so it's your own battle after all.
// "headbutt@Route 30 (Headbutt)": a method with its own wild area (a tree's Pokemon); plain: the map's area
std::string WildMethodArea(const OwMap& m, const std::string& method, std::string* meth) {
    size_t at = method.find('@');
    if (meth) *meth = at == std::string::npos ? method : method.substr(0, at);
    return at == std::string::npos ? m.wild : method.substr(at + 1);
}
bool TeamWild(Game& g, int i, const std::string& method = "walk") {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    std::string meth, area = WildMethodArea(m, method, &meth);
    for (int p = 0; p < 2; p++) if (HealthyCount(Prof(g, p)) == 0) return false;
    if (Online(g)) {
        if (PartnerBattling(g) || !g.ow.w[1].here) return false;
        g.ow.wildBattle = true;
        g.ow.wildArea = area;
        g.ow.wildMethod = meth;
        OnlineSendWorldFight(-1, w.map, w.x, w.y, "wild|" + method);
        OnlineSetReady(g, 2);
        g.ow.waitFight = true;
        g.ow.waitName = "the wild Pokemon";
        return true;
    }
    int o = 1 - i;
    if (g.ow.sub[o] || !g.ow.w[o].msg.empty()) return false;
    int ox, oy;
    if (FreeBeside(g, o, w.map, w.x, w.y, w.face, ox, oy)) { PlaceAt(g, o, w.map, ox, oy); g.ow.w[o].face = w.face; }
    SaveAll(g);
    g.ow.wildBattle = true;
    g.ow.wildArea = area;
    g.ow.wildMethod = meth;
    StartCoopTrainingBattle(g);
    return true;
}

// Wild Pokemon: a training battle on this route. Co-op: the player who stepped in the grass fights
// alone, in their own little game (their half of the screen) while their partner walks on.
// A Pokemon met on the map: that one, at its level, one battle (it's gone once beaten or caught).
void StaticEncounter(Game& g, int i, const std::string& species, int level, const std::string& flag) {
    OwWalker& w = g.ow.w[i];
    int sp = SpeciesIndex(species);
    if (sp < 0) return;
    if (HealthyCount(WP(g, i)) == 0) { Say(w, "Your Pokemon are all too tired to battle!|Rest them at a Pokemon Center first."); return; }
    NoteBattleWeather(g, i);
    SaveAll(g);
    Game& t = g.coop ? StartSub(g, i) : g;
    t.ow.wildBattle = true;
    t.ow.wildArea = MapOf(w).wild;
    t.ow.wildMethod = "";
    t.ow.staticLevel = std::max(1, level);
    t.ow.staticFlag = flag;
    t.encounterBlank = g.coop;
    t.stageIdx = -1; t.legend = -1; t.tower = false; t.raid = false;
    t.mode = Mode::Training;
    t.rareSpecies = sp;
    t.rareBattle = true;
    QuickBattle(t);
}

// ---- The Safari Zone (Kanto, Hoenn) and the Bug-Catching Contest (Johto's National Park): balls and steps, a
// screen of their own for the Pokemon you meet (screens_safari.cpp), and back to the gate when they run out.
bool SafariArea(const Game& g, const OwWalker& w) {
    const auto& run = g.ow.safari;
    if (!run.on || w.map < 0) return false;
    const OwMap& m = gMaps[w.map];
    return run.contest ? m.name == "National Park" : m.wild == "Safari Zone";
}
void SafariEncounter(Game& g, int i, const std::string& method) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    std::string area = g.ow.safari.contest ? "Bug-Catching Contest" : m.wild;
    if (WildAreaIndex(area) < 0) return;
    w.stepsSinceWild = 0;
    g.ow.wildArea = area;
    g.ow.wildMethod = method;
    SetWildContext(WildAreaIndex(area), TrainStage(g), TrainBadges(g));
    SetWildMethod(method);
    int sum = 0, n = 0;
    for (const auto& mm : g.profile.mons) if (!mm.boxed && mm.eggSteps <= 0) { sum += mm.level; n++; }
    auto team = MakeWildTeam(1, n ? sum / n : 10);
    SetWildContext(-1, 0, 0);
    if (team.empty()) return;
    const CombatantSpec& c = team[0];
    int iv[6];
    for (int k = 0; k < 6; k++) iv[k] = c.iv[k] >= 0 ? c.iv[k] : SimRand(0, 31);
    SaveAll(g);
    OpenSafariEncounter(g, c.species, c.level, c.shiny || RollShiny(), c.nature >= 0 ? c.nature : SimRand(0, 24), iv);
}
std::string BugJudging(Game& g);
void SafariOver(Game& g) {   // out of balls or steps: the PA calls you back to the gate
    auto& run = g.ow.safari;
    if (!run.on) return;
    OwWalker& w = g.ow.w[0];
    bool contest = run.contest;
    std::string text = contest ? "PA: Ding-dong! Time's up! The Bug-Catching Contest is over. Please return to the gate for the judging!"
                               : "PA: Ding-dong! " + std::string(run.balls <= 0 ? "You're out of Safari Balls!" : "Time's up!") + " Your Safari game is over!";
    std::string judge = contest ? BugJudging(g) : "";
    run.on = false;
    if (MapIndex(run.gate) >= 0) Warp(g, 0, {"warp", run.gate, std::to_string(run.gateX), std::to_string(run.gateY), "down"});
    Say(w, text + (judge.empty() ? "" : "|" + judge));
}
void SafariStep(Game& g, int i) {
    auto& run = g.ow.safari;
    if (!run.on || i != 0) return;
    OwWalker& w = g.ow.w[0];
    if (SafariArea(g, w)) {
        if (--run.steps <= 0) SafariOver(g);
    } else if (w.map >= 0 && gMaps[w.map].name != run.gate && !SafariArea(g, w)) {
        if (run.contest) { std::string j = BugJudging(g); run.on = false; Say(w, "You left the National Park - the contest is over for you.|" + j); }
        else run.on = false;   // walked out through the gate: the game's over
    }
}
// The judges look at your bug: its level, its IVs, its HP and how rare it is (HeartGold weighs much the same).
std::string BugJudging(Game& g) {
    auto& run = g.ow.safari;
    if (run.bug < 0) return "Judge: You didn't catch a bug? Better luck next time!";
    const SpeciesDef& sp = AllSpecies()[run.bug];
    int ivs = 0;
    for (int k = 0; k < 6; k++) ivs += run.bugIv[k];
    static const std::pair<const char*, int> kRare[] = {{"Scyther", 70}, {"Pinsir", 70}, {"Butterfree", 45}, {"Beedrill", 45}, {"Venomoth", 45}, {"Parasect", 40},
                                                        {"Venonat", 25}, {"Paras", 25}, {"Metapod", 15}, {"Kakuna", 15}, {"Caterpie", 10}, {"Weedle", 10}};
    int rare = 20;
    for (const auto& [nm, v] : kRare) if (sp.name == nm) rare = v;
    int score = run.bugLevel * 3 + ivs / 2 + rare + (run.bugShiny ? 50 : 0);
    static const char* kRivals[] = {"Bug Catcher Don", "Bug Catcher Benny", "Camper Barry", "Picnicker Cindy", "School Kid Kipp", "Pokefan William"};
    int better = 0;
    for (int k = 0; k < 5; k++) if (SimRand(80, 220) + SimRand(0, 40) > score) better++;
    int place = better + 1;
    std::string prize = place == 1 ? "Sun Stone" : place == 2 ? "Everstone" : place == 3 ? "Sitrus Berry" : "Oran Berry";
    BagAdd(g.profile, prize, 1);
    OwnedMon m = NewMon(run.bug, run.bugLevel);
    m.caught = true;
    m.shiny = run.bugShiny;
    if (run.bugNature >= 0) m.nature = run.bugNature;
    for (int k = 0; k < 6; k++) m.iv[k] = run.bugIv[k];
    m.metAt = "National Park";
    bool boxed = AddMon(g.profile, m);
    g.profile.dexCaught.insert(m.species);
    SaveProfile(g.profile);
    std::string head = place == 1 ? "The winner is... {PLAYER}, who caught a " + sp.name + "!" : TextFormat("{PLAYER} came in place %d with a %s.", place, sp.name.c_str());
    if (place > 1) head = std::string("The winner is ") + kRivals[SimRand(0, 5)] + "!|" + head;
    run.bug = -1;
    return "Judge: The results are in! " + head + TextFormat(" (Score %d)", score) + "|You got the " + prize + "!|" + sp.name +
           (boxed ? " was sent to the Box." : " joined your party.");
}

void WildEncounter(Game& g, int i, const std::string& method0) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    std::string method, area = WildMethodArea(m, method0, &method);
    if (RegionInfo(m.region).name == "Hoenn" && method0 == "walk") {   // Emerald's mass outbreak (the TV news names it)
        const Outbreak& ob = TodaysOutbreak();
        if (m.name == ob.map && SimRand(0, 99) < 50) SetWildOverride(SpeciesIndex(ob.species));
    }
    if (InJohto(g, i)) {   // a swarm (after the League), Hoenn / Sinnoh Sound, the Ruins' signal
        std::string st = RadioStation(g, i);
        bool water = method0.rfind("surf", 0) == 0 || method0.find("rod") != std::string::npos;
        const Swarm& sw = TodaysSwarm();
        if (HasFlag(g, i, "done:Johto") && m.name.rfind(sw.map, 0) == 0 && sw.water == water && SimRand(0, 99) < 40) SetWildOverride(SpeciesIndex(sw.species));
        else if ((st == "hoenn" || st == "sinnoh") && method0 == "walk" && HasFlag(g, i, "done:Johto") && SimRand(0, 99) < 25)
            SetWildOverride(RandomRegionWild(RegionByName(st == "hoenn" ? "Hoenn" : "Sinnoh")));
        if (st == "unown" && m.name.find("Ruins of Alph") != std::string::npos) {
            std::vector<int> unseen;
            int u = SpeciesIndex("Unown");
            if (u >= 0 && !WP(g, i).dexSeen.count("Unown")) unseen.push_back(u);
            for (int f : FormsOf(u)) if (!WP(g, i).dexSeen.count(AllSpecies()[f].name) && !WP(g, i).dexCaught.count(AllSpecies()[f].name)) unseen.push_back(f);
            SetWildUnseenForms(unseen);
        }
    }
    if (method0 == "walk")
        for (const auto& z : m.wildAt) if (w.x >= z.x0 && w.x <= z.x1 && w.y >= z.y0 && w.y <= z.y1) area = WildMethodArea(m, "walk@" + z.area, &method);
    if (Online(g) && (g.ow.incoming || g.ow.waitFight)) return;   // already on your way into a battle together
    if (WildAreaIndex(area) < 0 || HealthyCount(WP(g, i)) == 0) return;
    if (SafariArea(g, w)) { SafariEncounter(g, i, method); return; }
    NoteBattleWeather(g, i);
    w.stepsSinceWild = 0;
    SaveAll(g);
    g.ow.wildMethod = method;
    // co-op: teamed up (always, online: you both go in at once, so neither of you walks into another one meanwhile)
    if (g.coop && (Online(g) || (g.ow.teamUp && method == "walk")) && TeamWild(g, i, method0)) return;
    Game& t = g.coop ? StartSub(g, i) : g;
    if (t.profile.trainArea != area) { t.profile.trainArea = area; t.rareSpecies = -1; }
    t.ow.wildBattle = true;
    t.ow.wildArea = area;
    t.ow.wildMethod = method;
    t.encounterBlank = g.coop;
    if (!SaveProfile(t.profile)) Toast(g, "Could not save the profile!");
    StartTrainingBattle(t);
}

// The Exp. All: `xp` split between the healthy Pokemon in a party (they level up and evolve right away).
std::string ShareXp(Profile& p, int xp) {
    std::vector<OwnedMon*> party;
    for (auto& m : p.mons) if (!m.boxed && m.hp > 0) party.push_back(&m);
    if (party.empty() || xp <= 0) return "";
    int each = std::max(1, xp / (int)party.size());
    std::string note;
    for (OwnedMon* m : party) {
        Growth gr;
        int lv = m->level;
        AddXp(*m, each, gr);
        if (gr.evolveTo >= 0) {
            std::string from = m->species;
            Growth g2;
            Evolve(*m, gr.evolveTo, g2);
            note += " " + from + " evolved into " + m->species + "!";
        } else if (m->level > lv) note += TextFormat(" %s grew to Lv%d!", m->species.c_str(), m->level);
    }
    return note;
}
// A solo wild battle is over: if its player carries the Exp. All, their partner's party gets a share.
void ExpAllShare(Game& g, int i, const Game& s) {
    const ItemDef* ea = FindItem("Exp. All");
    if (!ea || BagCount(s.profile, ea->name) <= 0 || s.ow.wildArea.empty()) return;   // (a wild battle, not the Mart)
    int total = 0;
    for (const auto& r : s.resultRows) total += std::max(0, r.xp);
    int share = (int)(total * ea->value);
    if (share <= 0) return;
    const std::string& pn = s.profile.name;
    if (Online(g)) { OnlineSendXpShare(share); return; }
    int o = 1 - i;
    Profile& target = g.ow.sub[o] ? g.ow.sub[o]->profile : g.coopP[WhoOf(g, o)];
    std::string note = ShareXp(target, share);
    if (!g.ow.sub[o]) SaveProfile(target);
    Toast(g, TextFormat("Exp. All: %s's party got %d XP from %s's battle.%s", target.name.c_str(), share, pn.c_str(), note.c_str()));
}

// One frame of a player's own game; when it's back at its "hub" it's over and their profile
// (and anything caught, healed or bought) goes back where it belongs.
void RunSub(Game& g, int i, float dt) {
    std::shared_ptr<Game> s = g.ow.sub[i];
    if (!s) return;
    if (s->screen == Screen::World) {
        int who = WhoOf(g, i);
        ExpAllShare(g, i, *s);
        for (const auto& f : s->profile.owFlags) g.coopSave.owFlags.insert(f);   // a route trainer beaten in their own battle
        if (s->coopSub) {   // the roaming legendaries: where they went, and any one caught
            bool roamChanged = g.coopSave.roam.size() != s->profile.roam.size();
            g.coopSave.roam = s->profile.roam;
            s->profile.roam = s->subRoamKeep;
            for (auto it = g.coopSave.owFlags.begin(); it != g.coopSave.owFlags.end();) {
                if (it->rfind("roamcaught:", 0) == 0) {
                    int id = std::atoi(it->c_str() + 11);
                    if (id >= 0 && id < 64) { g.coopSave.legends |= 1ull << id; g.coopSave.legendWho[id] = who; g.coopSave.roam.erase(id); }
                    roamChanged = true;
                    it = g.coopSave.owFlags.erase(it);
                } else ++it;
            }
            if (roamChanged) { if (Online(g)) OnlineSendCoop(g); else SaveCoop(g.coopSave); }
        }
        bool newFlags = !s->profile.owFlags.empty();
        s->profile.owFlags = g.coopP[who].owFlags;   // (their own solo story's flags, untouched)
        if (newFlags) { if (Online(g)) OnlineSendCoop(g); else SaveCoop(g.coopSave); }
        g.coopP[who] = s->profile;
        SaveProfile(g.coopP[who]);
        g.ow.sub[i].reset();
        g.ow.w[i].fadeT = 0.35f;
        if (Online(g) && g.ow.watchSent) OnlineSendWatchEnd();
        g.ow.watchSent = false;
        g.ow.watchers = false;
        if (Online(g)) OnlineSendProfile(g);
        return;
    }
    s->t += dt;
    if ((int)s->screen != s->ow.lastScreen) { s->ow.lastScreen = (int)s->screen; s->screenT = 0; }
    s->screenT += dt;
    s->toastT = std::max(0.0f, s->toastT - dt);
    float keepUi = gUiScreenT;
    gUiScreenT = s->screenT;
    gInputOnly = LocalCoop(g) ? WhoOf(g, i) : -1;
    // online: your partner may be watching this battle - send where it starts, then every frame
    bool stream = Online(g) && g.ow.watchers;
    if (stream && s->screen == Screen::Battle) {
        g.ow.watchResync -= dt;
        if (!g.ow.watchSent || g.ow.watchResync <= 0) { OnlineSendWatchState(s->battle); g.ow.watchSent = true; g.ow.watchResync = 3.0f; }
    }
    if (stream && g.ow.watchSent) gBattleStepHook = [](const Battle&, float d, const PlayerInput* in) { OnlineSendWatchStep(d, in); };
    UpdateScreenOf(*s, dt);
    gBattleStepHook = nullptr;
    gInputOnly = -1;
    gUiScreenT = keepUi;
    if (stream && g.ow.watchSent && s->screen != Screen::Battle) {   // the battle's over
        OnlineSendWatchEnd();
        g.ow.watchSent = false;
        g.ow.watchers = false;
    }
}

// Match Call (Emerald's Pokenav): the Trainers you've beaten who have rematch teams are registered; every few
// hundred steps one of them calls - they want a rematch, and wait where you met them. Each rematch needs more
// of that region's badges (2, 4, 6, 8), like the VS Seeker.
int gMatchCallSteps = 0;
bool McRegistered(const Game& g, const OwNpc& n) { return n.kind == "fight" && !n.rematch.empty() && n.args.size() > 4 && Flags(g).count(n.args[4]); }
int McNextTier(const Game& g, const OwNpc& n, int region) {   // the rematch they'd call about (0: none)
    int next = RematchesWon(g, n) + 1;
    if (next > (int)n.rematch.size() || next > 4 || RematchTier(g, n) > 0) return 0;
    return RegionBadgeCount(g, region) >= 2 * next ? next : 0;
}
void MatchCallStep(Game& g, int i) {
    if ((BagCount(WP(g, i), "Pokenav") <= 0 && BagCount(WP(g, i), "Pokegear") <= 0) || ++gMatchCallSteps < 300) return;
    gMatchCallSteps = 0;
    std::vector<std::pair<int, int>> can;
    for (int mi = 0; mi < (int)gMaps.size(); mi++)
        for (int k = 0; k < (int)gMaps[mi].npcs.size(); k++)
            if (McRegistered(g, gMaps[mi].npcs[k]) && McNextTier(g, gMaps[mi].npcs[k], gMaps[mi].region) > 0 &&
                BagCount(WP(g, i), RegionInfo(gMaps[mi].region).name == "Johto" ? "Pokegear" : "Pokenav") > 0) can.push_back({mi, k});   // (Johto's call the Pokegear)
    if (can.empty()) return;
    auto [mi, k] = can[SimRand(0, (int)can.size() - 1)];
    const OwNpc& n = gMaps[mi].npcs[k];
    SetFlag(g, "rematch:" + n.args[4] + ":" + std::to_string(McNextTier(g, n, gMaps[mi].region)));
    PlaySfx(Sfx::MenuOk);
    Toast(g, std::string(RegionInfo(gMaps[mi].region).name == "Johto" ? "Ring ring! " : "Match Call! ") + n.args[0] + ": \"Let's battle again! I'll be waiting on " + gMaps[mi].name + ".\"");
}

// Island Cave's lap (Regice): read the braille, then run one lap close to the wall, back to it.
void LapStep(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    if (!gLapOn[i & 1] || m.lap.empty()) { gLapOn[i & 1] = false; return; }
    int k = -1;
    for (int j = 0; j < (int)m.lap.size(); j++) if (m.lap[j].first == w.x && m.lap[j].second == w.y) k = j;
    if (k < 0) { gLapOn[i & 1] = false; return; }   // off the path: start over at the braille
    gLapMask[i & 1] |= 1ull << k;
    if (gLapMask[i & 1] == (m.lap.size() >= 64 ? ~0ull : (1ull << m.lap.size()) - 1) && w.x == m.lapGX && w.y == m.lapGY) {
        gLapOn[i & 1] = false;
        SetFlag(g, m.lapFlag);
        PlaySfx(Sfx::Thud);
        Say(w, "...|There's a rumbling... The wall in front of you crumbled away!");
    }
}
// A step finished: grass, trainers.
void Arrived(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    w.stepsSinceWild++;
    w.vsCharge = std::min(100, w.vsCharge + 1);
    MatchCallStep(g, i);
    LapStep(g, i);
    SafariStep(g, i);
    if (m.At(w.x, w.y) == 'g') gRustle.push_back({w.map, w.x, w.y, 0});
    if (i == 0) SendPos(g);
    if (!m.town.empty() && !Flags(g).count("visit:" + m.town)) SetFlag(g, "visit:" + m.town);   // somewhere to Fly to
    if (m.indoor && !m.cycling) w.biking = false;
    char c = Cell(g, i, w.map, w.x, w.y);
    if (c >= '1' && c <= '4') w.spin = c == '1' ? 1 : c == '2' ? 0 : c == '3' ? 2 : 3;   // a spin tile: off you go
    else if (c == '0') w.spin = -1;
    else if (c == 'i') w.spin = w.face;   // slippery: on you slide, the way you were going
    else if (w.spin >= 0 && m.At(w.fromX, w.fromY) == 'i') w.spin = -1;   // (off the slippery floor: you stop)
    for (const auto& st : m.steps)   // stepped onto a button
        if (st.x == w.x && st.y == w.y && CondOk(g, i, st.ifF, st.unlessF)) { w.spin = -1; RunAction(g, i, st.act); return; }
    if (w.spin >= 0) return;
    {   // a new map: the roaming legendaries move on too (as in the games, whenever you change routes)
        static int lastMap[2] = {-1, -1};
        if (lastMap[i & 1] != w.map) {
            if (lastMap[i & 1] >= 0) UpdateRoamers(g, 0, true);
            lastMap[i & 1] = w.map;
        }
    }
    if (WorldStepEggs(g, WhoOf(g, i))) return;   // the Day Care Pokemon grow; an Egg may hatch
    if (CheckSight(g, i)) return;
    Profile& P = WP(g, i);
    if (P.repel > 0 && --P.repel == 0) { Say(w, "Repel's effect wore off..."); return; }
    if (m.wild.empty() || w.stepsSinceWild < 3 || P.repel > 0 || (!m.wildIf.empty() && !HasFlag(g, i, m.wildIf))) return;
    int lead = -1;   // a Cleanse Tag on the first Pokemon: a third fewer wild Pokemon
    for (int k = 0; k < (int)P.mons.size() && lead < 0; k++) if (!P.mons[k].boxed) lead = k;
    int tag = lead >= 0 && P.mons[lead].item == "Cleanse Tag" ? 2 : 3;
    if (c == 'g' && m.name == "Route 113" && BagCount(P, "Soot Sack") > 0) {   // volcanic ash in the grass: into the Soot Sack
        std::string key = "ash:" + P.name + "|";
        SetFlagVal(g, key, std::to_string(std::min(9999, std::max(0, FlagNum(g, key)) + 1)));
    }
    {   // the White Flute brings wild Pokemon out, the Black Flute keeps them away
        if (Flags(g).count("flute:white:" + P.name)) tag = tag * 3 / 2;
        else if (Flags(g).count("flute:black:" + P.name)) tag = std::max(1, tag / 2);
    }
    if (InJohto(g, i)) {   // the Pokegear's Pokemon Music: the March brings them out, the Lullaby keeps them away
        std::string st = RadioStation(g, i);
        if (st == "march") tag = tag * 3 / 2; else if (st == "lullaby") tag = std::max(1, tag / 2);
    }
    if ((c == 'g' && SimRand(0, 299) < 12 * tag) || (c == 'c' && SimRand(0, 299) < 8 * tag)) WildEncounter(g, i);   // tall grass; a cave floor
    else if (w.surfing && (c == 'w' || c == 'd') && SimRand(0, 299) < 6 * tag && WildAreaHasMethod(m.wild, "surf")) WildEncounter(g, i, "surf");
}

constexpr float kSteep = 14;   // (3D maps) more of a rise than this between two tiles: a wall, not a slope
bool TooSteep(float rise, char to, char from) {   // ...unless it's a slope or steps (up to three tiles' worth), or a door's mat
    if (to == 'D' || from == 'D') return false;
    return std::abs(rise) > ((to == 's' || from == 's') ? 48 : kSteep);
}
// One step (or a ledge hop, a boulder push, Surf on and off).
// ---- Emerald's rotating gates (pokeemerald's rotating_gate.c): arms on the lines between squares, turning on a
// corner; walk through an arm and it swings round (if the squares it sweeps are clear), or it stops you.
std::map<int, std::vector<int>> gGateOri;   // map -> each gate's orientation now (back as it was when you come in)
std::vector<int>& GateOri(int map) {
    auto& v = gGateOri[map];
    const auto& gs = gMaps[map].rgates;
    if (v.size() != gs.size()) { v.clear(); for (const auto& gt : gs) v.push_back(gt.orient & 3); }
    return v;
}
void ResetGates(int map) { gGateOri.erase(map); }
namespace rg {
constexpr int NONE = 255;
constexpr int R(int dirn, int arm, int lng) { return (dirn << 4) | (arm * 2 + lng); }   // dirn 1 anticlockwise, 2 clockwise
const uint8_t kNorth[16] = {NONE, NONE, NONE, NONE, R(2, 3, 1), R(2, 3, 0), R(1, 1, 0), R(1, 1, 1), NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE};
const uint8_t kSouth[16] = {NONE, NONE, NONE, NONE, NONE, NONE, NONE, NONE, R(1, 3, 1), R(1, 3, 0), R(2, 1, 0), R(2, 1, 1), NONE, NONE, NONE, NONE};
const uint8_t kWest[16] = {NONE, R(1, 0, 1), NONE, NONE, NONE, R(1, 0, 0), NONE, NONE, NONE, R(2, 2, 0), NONE, NONE, NONE, R(2, 2, 1), NONE, NONE};
const uint8_t kEast[16] = {NONE, NONE, R(2, 0, 1), NONE, NONE, NONE, R(2, 0, 0), NONE, NONE, NONE, R(1, 2, 0), NONE, NONE, NONE, R(1, 2, 1), NONE};
const int kCW[8][2] = {{0, -1}, {1, -2}, {0, 0}, {1, 0}, {-1, 0}, {-1, 1}, {-1, -1}, {-2, -1}};
const int kACW[8][2] = {{-1, -1}, {-1, -2}, {0, -1}, {1, -1}, {0, 0}, {0, 1}, {-1, 0}, {-2, 0}};
const uint8_t kArms[8][8] = {{1, 0, 1, 0, 0, 0, 0, 0}, {1, 1, 1, 0, 0, 0, 0, 0}, {1, 0, 1, 1, 0, 0, 0, 0}, {1, 1, 1, 1, 0, 0, 0, 0},
                             {1, 0, 1, 0, 1, 0, 0, 0}, {1, 1, 1, 0, 1, 0, 0, 0}, {1, 0, 1, 1, 1, 0, 0, 0}, {1, 0, 1, 0, 1, 1, 0, 0}};
}
// false: a gate's arm is in the way and can't turn (moving dir onto nx, ny); a gate that can turn, turns
bool GateLetsPass(Game& g, int i, int map, int dir, int nx, int ny) {
    const OwMap& m = gMaps[map];
    if (m.rgates.empty() || gReachMode) return true;
    auto& ori = GateOri(map);
    for (int k = 0; k < (int)m.rgates.size(); k++) {
        const auto& gt = m.rgates[k];
        if (nx < gt.x - 2 || nx > gt.x + 1 || ny < gt.y - 2 || ny > gt.y + 1) continue;
        int cx = nx - gt.x + 2, cy = ny - gt.y + 2;
        const uint8_t* tab = dir == 1 ? rg::kNorth : dir == 0 ? rg::kSouth : dir == 2 ? rg::kWest : rg::kEast;
        int info = tab[cy * 4 + cx];
        if (info == rg::NONE) continue;
        int rot = info >> 4, armInfo = info & 15;
        int arm = armInfo / 2, lng = armInfo % 2;
        int shape = std::max(0, std::min(7, gt.shape));
        if (!rg::kArms[shape][((arm - ori[k] + 4) % 4) * 2 + lng]) continue;   // no arm there
        const int (*pos)[2] = rot == 1 ? rg::kACW : rg::kCW;
        bool can = true;
        for (int a = 0; a < 4 && can; a++)
            for (int j = 0; j < 2 && can; j++)
                if (rg::kArms[shape][a * 2 + j]) {
                    int idx = 2 * ((ori[k] + a) % 4) + j;
                    char c = Cell(g, i, map, gt.x + pos[idx][0], gt.y + pos[idx][1]);
                    if (c == '#') can = false;
                }
        if (!can) return false;
        ori[k] = rot == 1 ? (ori[k] + 3) % 4 : (ori[k] + 1) % 4;
        PlaySfx(Sfx::Door);
        return true;
    }
    return true;
}
void TryStep(Game& g, int i, int dir) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    w.face = dir;
    int nx = w.x + DX[dir], ny = w.y + DY[dir];
    auto bump = [&]() { if (w.bumpT <= 0) { PlaySfx(Sfx::Bump); w.bumpT = 0.35f; } w.spin = -1; };
    for (const auto& b : m.blocks)   // turned back until you've done what it takes
        if (b.x == nx && b.y == ny && !HasFlag(g, i, b.flag) && !gReachMode) { w.spin = -1; Say(w, b.text.empty() ? "You can't go this way yet." : b.text, b.act); return; }
    for (const auto& d : m.doors)   // a door, stairs, or a mat you step off (it can be just past the edge)
        if (d.x == nx && d.y == ny && ((gReachMode && (d.ifF + d.unlessF).find("biking") == std::string::npos) || CondOk(g, i, d.ifF, d.unlessF))) { w.spin = -1; RunAction(g, i, d.act); return; }
    // off the edge: the next map
    if (nx < 0 || ny < 0 || nx >= m.w || ny >= m.h) {
        OwLink L = m.link[dir];
        int tx = 0, ty = 0;
        if (L.map >= 0) {
            const OwMap& n = gMaps[L.map];
            if (dir == 1) { tx = nx + L.off; ty = n.h - 1; }
            else if (dir == 0) { tx = nx + L.off; ty = 0; }
            else if (dir == 2) { tx = n.w - 1; ty = ny + L.off; }
            else { tx = 0; ty = ny + L.off; }
            if (tx < 0 || ty < 0 || tx >= n.w || ty >= n.h) L.map = -1;   // not that one: another map on this side?
        }
        if (L.map < 0)
            for (const auto& nb : m.near) {   // a placed map: whichever one is there
                const OwMap& n = gMaps[nb.map];
                int ax = nx - nb.dx, ay = ny - nb.dy;
                if (ax >= 0 && ay >= 0 && ax < n.w && ay < n.h) { L.map = nb.map; tx = ax; ty = ay; break; }
            }
        if (L.map >= 0) {
            const OwMap& n = gMaps[L.map];
            char c = Cell(g, i, L.map, tx, ty);
            bool ok = w.surfing ? (IsWater(c) || Walkable(c)) : Walkable(c);
            if (!ok || WalkerAt(g, i, L.map, tx, ty)) { bump(); return; }
            if (!w.surfing && !m.hts.empty() && !n.hts.empty() && TooSteep(n.Lift(tx, ty) - m.Lift(w.x, w.y), c, m.At(w.x, w.y))) { bump(); return; }
            if (n.bike && !w.biking) {   // Cycling Road
                if (w.bumpT <= 0) {
                    Say(w, BagCount(WP(g, i), "Bicycle") > 0 ? "No pedestrians allowed on Cycling Road!|Get on your Bicycle (from the Bag) to ride it."
                                                             : "No pedestrians allowed on Cycling Road!|You need a Bicycle to go this way.");
                    w.bumpT = 0.6f;
                }
                return;
            }
            bool someoneThere = false;
            for (int k = 0; k < Driven(g); k++) if (k != i && g.ow.w[k].map == L.map) someoneThere = true;
            if (PartnerOn(g, L.map)) someoneThere = true;
            // you and your Pokemon, seen from the new map
            int dx = tx - DX[dir] - w.x, dy = ty - DY[dir] - w.y;
            w.map = L.map;
            if (!someoneThere) ResetNpcs(L.map);
            w.bannerT = 2.2f;
            w.fromX = w.x + dx; w.fromY = w.y + dy;
            w.fFromX = w.fx + dx; w.fFromY = w.fy + dy;
            w.fx = w.fromX; w.fy = w.fromY;
            w.x = tx; w.y = ty;
            w.stepT = 0;
            w.fFace = dir;
            w.walkAnim ^= 1;
            if (!IsWater(c) && !n.underwater) w.surfing = false;
            if (n.underwater) { w.surfing = true; w.biking = false; }
            if (i == 0) SendPos(g);
            return;
        }
        if (!L.msg.empty() && w.bumpT <= 0) { RunAction(g, i, L.msg); w.bumpT = 0.5f; }
        return;
    }
    char c = Cell(g, i, w.map, nx, ny);
    if ((dir == 0 && c == '^') || (dir == 1 && m.At(w.x, w.y) == '^')) { bump(); return; }   // a wall along the square's top edge
    bool blocked = NpcAt(g, w.map, nx, ny) >= 0 || WalkerAt(g, i, w.map, nx, ny) || ItemAt(g, i, w.map, nx, ny, false) >= 0;
    bool hop = false;
    int hx = nx, hy = ny;
    if (w.surfing) {
        if (c == 'W' || blocked) { bump(); return; }   // a waterfall: use Waterfall (face it, press A)
        if (!IsWater(c) && !Walkable(c)) { bump(); return; }
    } else if ((c == 'v' && dir == 0) || (c == '<' && dir == 2) || (c == '>' && dir == 3)) {   // a ledge: hop down / across
        hop = true;
        hx = nx + DX[dir]; hy = ny + DY[dir];
        char c2 = Cell(g, i, w.map, hx, hy);
        if (!Walkable(c2) || NpcAt(g, w.map, hx, hy) >= 0 || WalkerAt(g, i, w.map, hx, hy)) { bump(); return; }
    } else if (c == 'B' && w.strengthMap == w.map && !blocked) {   // push the boulder along
        int bx = nx + DX[dir], by = ny + DY[dir];
        char c2 = Cell(g, i, w.map, bx, by);
        if (Walkable(c2) && c2 != 'g' && NpcAt(g, w.map, bx, by) < 0 && !WalkerAt(g, i, w.map, bx, by) && ItemAt(g, i, w.map, bx, by, false) < 0) {
            auto& bl = gRt[w.map].boulders;
            for (int& k : bl) if (k == ny * m.w + nx) k = by * m.w + bx;
            ShareMap(g, w.map);
            PlaySfx(Sfx::Thud);
            w.bumpT = 0.3f;
        } else bump();
        return;
    } else if (!Walkable(c) || blocked) {
        bump();
        return;
    } else if (!w.surfing && !IsWater(c) && TooSteep(m.Lift(nx, ny) - m.Lift(w.x, w.y), c, m.At(w.x, w.y))) {   // up a cliff (3D maps)
        bump();
        return;
    }
    if (!hop && !GateLetsPass(g, i, w.map, dir, nx, ny)) { bump(); return; }   // a rotating gate's arm that can't turn
    w.fFromX = w.fx; w.fFromY = w.fy;
    w.fx = w.x; w.fy = w.y;
    w.fFace = w.fx != w.fFromX ? (w.fx > w.fFromX ? 3 : 2) : (w.fy > w.fFromY ? 0 : w.fy < w.fFromY ? 1 : w.fFace);
    w.fromX = w.x; w.fromY = w.y;
    w.x = hx; w.y = hy;
    w.stepT = 0;
    w.hop = hop;
    w.walkAnim ^= 1;
    if (w.surfing && !IsWater(c) && !m.underwater) w.surfing = false;   // back on land (under the sea you keep swimming)
    if (hop) PlaySfx(Sfx::Ledge);
    if (i == 0) SendPos(g);
}

#ifdef ARENA_AUTOTEST
} // namespace
// Autotest: every map of a region you can walk to from (map, x, y) with the engine's own steps - TryStep, doors,
// ledges, the edges between maps, slopes - plus Surf (onto water), Waterfall (up a 'W' column), Dive, Cut / Rock
// Smash / Strength (trees, rocks and boulders are passable), and the boats people offer. Story blocks, gates and
// people are left out (they open as the story goes). Returns "reached N of M" and the maps it never got to.
std::string OwReach(Game& g, const std::string& region, const std::string& startMap, int sx, int sy) {
    OwWalker& w = g.ow.w[0];
    gReachMode = true;
    struct St { int map, x, y; bool surf; };
    auto key = [](const St& s) { return ((long long)s.map << 40) | ((long long)(s.x + 512) << 24) | ((long long)(s.y + 512) << 4) | (s.surf ? 1 : 0); };
    std::set<long long> seen;
    std::vector<St> q;
    auto push = [&](St s) { if (s.map < 0) return; if (seen.insert(key(s)).second) q.push_back(s); };
    int m0 = MapIndex(startMap);
    push({m0, sx, sy, false});
    auto put = [&](const St& s) {
        w.map = s.map; w.x = w.fromX = w.fx = w.fFromX = s.x; w.y = w.fromY = w.fy = w.fFromY = s.y;
        w.surfing = s.surf; w.biking = !s.surf; w.msg.clear(); w.choices.clear(); w.after.clear(); w.stepT = 1; w.hop = false;
        w.strengthMap = s.map;
    };
    auto here = [&]() { return St{w.map, w.x, w.y, w.surfing}; };
    auto choices = [&](const St& from) {   // a boat / a ferry's destinations: each one
        auto ch = w.choices;
        for (const auto& c : ch) {
            if (c.second.empty()) continue;
            put(from);
            RunAction(g, 0, c.second);
            push(here());
        }
    };
    for (size_t qi = 0; qi < q.size(); qi++) {
        St s = q[qi];
        const OwMap& m = gMaps[s.map];
        for (int dir = 0; dir < 4; dir++)
          for (int bike = 0; bike < (s.surf ? 1 : 2); bike++) {   // on foot and on a Bicycle (cracked floors)
            put(s);
            w.biking = bike == 1;
            w.face = dir;
            TryStep(g, 0, dir);
            St n = here();
            if (!w.choices.empty()) choices(s);
            else if (!w.after.empty() && w.after != "surfstep") { std::string a = w.after; put(s); RunAction(g, 0, a); if (w.choices.empty()) push(here()); else choices(s); }
            push(n);
            int nx = s.x + DX[dir], ny = s.y + DY[dir];
            char c = Cell(g, 0, s.map, nx, ny);
            if (!s.surf && IsWater(c) && c != 'W') push({s.map, nx, ny, true});           // Surf
            if ((c == 'T' || c == 'R' || c == 'B')) {                                      // Cut, Rock Smash, Strength
                int bx = nx + DX[dir], by = ny + DY[dir];
                if (c != 'B' || Walkable(Cell(g, 0, s.map, bx, by))) push({s.map, nx, ny, s.surf});
            }
            if (s.surf && c == 'W' && dir == 1) {                                          // Waterfall
                int y = s.y;
                while (Cell(g, 0, s.map, s.x, y - 1) == 'W') y--;
                if (IsWater(Cell(g, 0, s.map, s.x, y - 1))) y--;
                push({s.map, s.x, y, true});
            }
            if (s.surf && c == 'W' && dir == 0) {                                          // down a waterfall: you just go
                int y = ny;
                while (Cell(g, 0, s.map, s.x, y) == 'W') y++;
                if (IsWater(Cell(g, 0, s.map, s.x, y))) push({s.map, s.x, y, true});
            }
        }
        for (const auto& d : m.doors)   // every door next to you, whatever it waits for (Shoal Cave's tides: both ways in)
            if (std::abs(d.x - s.x) + std::abs(d.y - s.y) == 1 && d.act.rfind("warp|", 0) == 0) { put(s); RunAction(g, 0, d.act); push(here()); }
        for (const auto& e : m.emergeAt) if (s.surf && e.x == s.x && e.y == s.y) push({MapIndex(e.map), e.tx, e.ty, true});
        if (s.surf) if (const auto* d = DiveSpotAt(g, 0, m, s.x, s.y)) push({MapIndex(d->map), d->tx, d->ty, true});
        if (s.surf && (!m.diveTo.empty() || !m.emergeTo.empty())) {                      // Dive
            for (int down = 0; down < 2; down++) {
                std::string to = down ? m.diveTo : m.emergeTo;
                if (to.empty()) continue;
                int tx = down ? m.diveX : m.emergeX, ty = down ? m.diveY : m.emergeY;
                if (tx < 0) { tx = s.x; ty = s.y; }
                char c = m.At(s.x, s.y);
                if (down && c != 'd') continue;
                push({MapIndex(to), tx, ty, true});
            }
        }
        // the people who take you somewhere (a boat, a ferry, a gate guard who walks you through)
        for (const auto& np : m.npcs) {
            if (np.kind != "sailto" && np.kind != "boat" && np.kind != "travel" && np.kind != "warp" && np.kind != "ferry") continue;
            if (std::abs(np.x - s.x) + std::abs(np.y - s.y) != 1) continue;
            std::string act = np.kind;
            for (const auto& a : np.args) act += "|" + a;
            put(s);
            RunAction(g, 0, act);
            if (!w.choices.empty()) choices(s);
            else if (!w.after.empty()) { std::string a = w.after; put(s); RunAction(g, 0, a); if (!w.choices.empty()) choices(s); else push(here()); }
            else push(here());
        }
    }
    gReachMode = false;
    if (const char* dump = getenv("ARENA_REACH_DUMP")) {   // every square reached, for tools to look at
        FILE* f = fopen(dump, "a");
        if (f) { for (auto k : seen) { int mi = (int)(k >> 40), x = (int)((k >> 24) & 0xFFFF) - 512, y = (int)((k >> 4) & 0xFFFFF) - 512; fprintf(f, "%s\t%d\t%d\t%d\n", gMaps[mi].name.c_str(), x, y, (int)(k & 1)); } fclose(f); }
    }
    std::set<int> got;
    for (auto k : seen) got.insert((int)(k >> 40));
    int total = 0, reached = 0;
    std::string miss;
    for (int i = 0; i < (int)gMaps.size(); i++) {
        if (RegionInfo(gMaps[i].region).name != region) continue;
        total++;
        if (got.count(i)) reached++;
        else miss += (miss.empty() ? "" : "; ") + gMaps[i].name;
    }
    w.msg.clear(); w.choices.clear(); w.after.clear();
    return TextFormat("reached %d of %d", reached, total) + std::string(miss.empty() ? "" : "  NOT: " + miss);
}
namespace {
#endif

// This walker's buttons: solo/online the usual keys; local co-op each player's own layout.
struct WalkIn { int dir = -1; bool confirm = false, back = false, run = false, up = false, down = false, pause = false; };
WalkIn ReadWalk(const Game& g, int i) {
    WalkIn r;
    PlayerInput in;
#ifdef ARENA_AUTOTEST
    if (gOwTestOn) in = gOwTestIn[i];
    else
#endif
        in = LocalCoop(g) ? ReadInput(i, true) : ReadInput(0, false);
    bool up = in.move.y < -0.5f, down = in.move.y > 0.5f, left = in.move.x < -0.5f, right = in.move.x > 0.5f;
    up |= in.dpad.y < 0; down |= in.dpad.y > 0; left |= in.dpad.x < 0; right |= in.dpad.x > 0;   // the D-pad still walks here
    if (!LocalCoop(g)) {
        up |= IsKeyDown(KEY_UP); down |= IsKeyDown(KEY_DOWN); left |= IsKeyDown(KEY_LEFT); right |= IsKeyDown(KEY_RIGHT);
        r.run = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    }
    r.dir = up ? 1 : down ? 0 : left ? 2 : right ? 3 : -1;
    r.confirm = in.confirm;
    r.back = in.back;
    r.up = in.up;
    r.down = in.down;
    r.pause = in.pause;
    if (LocalCoop(g)) r.run = in.dodge || IsGamepadButtonDown(i, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
    if (!LocalCoop(g)
#ifdef ARENA_AUTOTEST
        && !gOwTestOn
#endif
    ) {   // Enter / Space / Esc as in the menus
        PlayerInput mi = MenuInput();
        r.confirm |= mi.confirm;
        r.back |= mi.back;
        r.up |= mi.up;
        r.down |= mi.down;
        r.pause |= IsKeyPressed(KEY_ESCAPE);
        for (int pad = 0; pad < 4; pad++)   // hold B to run (like FireRed); the dodge button too
            if (IsGamepadAvailable(pad)) r.run |= IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) || IsGamepadButtonDown(pad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        r.run |= in.dodge;
    }
#ifdef ARENA_AUTOTEST
    if (gOwTestOn) { gOwTestIn[i].confirm = gOwTestIn[i].back = gOwTestIn[i].up = gOwTestIn[i].down = gOwTestIn[i].pause = false; }
#endif
    return r;
}

// Pressing A at something on the map: an item, a tree to Cut, a rock to smash, a boulder, the water.
void Ask(OwWalker& w, const std::string& text, std::vector<std::pair<std::string, std::string>> choices) {
    std::string t = text;
    for (size_t at; (at = t.find("{PLAYER}")) != std::string::npos;) t.replace(at, 8, gPlayerName);   // FireRed's lines name you
    w.msg = Split(t, '|');
    w.msgPage = 0;
    w.after.clear();
    w.choices = std::move(choices);
    w.choiceSel = 0;
    PlaySfx(Sfx::MenuMove);
}
// Dive: A on the dark deep water while surfing (down), or under the sea where light filters down (up).
const OwMap::DiveSpot* DiveSpotAt(const Game& g, int i, const OwMap& m, int x, int y) {
    for (const auto& d : m.diveSpots)
        if (x >= d.x0 && x <= d.x1 && y >= d.y0 && y <= d.y1 && (gReachMode || HasFlag(g, i, d.flag))) return &d;
    return nullptr;
}
bool DiveHere(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    char c = m.At(w.x, w.y);
    bool down = !m.underwater && w.surfing && ((c == 'd' && !m.diveTo.empty()) || DiveSpotAt(g, i, m, w.x, w.y));
    bool up = m.underwater && c == 'u' && !m.emergeTo.empty();
    if (!down && !up) return false;
    std::string who;
    std::string what = down ? "The sea is deep here. A Pokemon may be able to go underwater." : "Light is filtering down from above. A Pokemon may be able to surface.";
    if (CanField(g, i, "Dive", &who)) Ask(w, what + " Would you like to use Dive?", {{"Use Dive (" + who + ")", "field|Dive"}, {"No", ""}});
    else Say(w, what + (PartyKnows(WP(g, i), "Dive") ? "|" + FieldNeed("Dive", MapRegionOf(g, i)) : ""));
    return true;
}
bool FacingThing(Game& g, int i, int fx, int fy) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    for (int hidden = 0; hidden < 2; hidden++) {
        int k = ItemAt(g, i, w.map, fx, fy, hidden != 0);
        if (k < 0) continue;
        const auto& it = m.items[k];
        SetFlag(g, "got:" + m.name + ":" + std::to_string(fx) + "," + std::to_string(fy));
        BagAdd(WP(g, i), it.item, it.n);
        if (g.coop) SaveProfile(WP(g, i));
        SaveAll(g);
        PlaySfx(Sfx::ItemGet);
        Say(w, (hidden ? std::string("You found something hidden! ") : std::string("")) + (it.n > 1 ? TextFormat("You found %d x %s!", it.n, it.item.c_str()) : "You found " + it.item + "!") +
               "|" + PocketHint(it.item));
        return true;
    }
    char c = Cell(g, i, w.map, fx, fy);
    std::string who;
    if (c == 'T' || c == 'R') {
        std::string mv = c == 'T' ? "Cut" : "Rock Smash";
        std::string what = c == 'T' ? "This tree looks like it can be cut down!" : "It's a rugged rock, but a Pokemon may be able to smash it.";
        if (CanField(g, i, mv, &who)) Ask(w, what + " Would you like to use " + mv + "?", {{"Use " + mv + " (" + who + ")", "field|" + mv}, {"No", ""}});
        else Say(w, what);
        return true;
    }
    if (c == 'B') {
        if (w.strengthMap == w.map) Say(w, "Push it by walking into it!");
        else if (CanField(g, i, "Strength", &who)) Ask(w, "It's a big boulder, but a Pokemon may be able to push it aside. Would you like to use Strength?", {{"Use Strength (" + who + ")", "field|Strength"}, {"No", ""}});
        else Say(w, "It's a big boulder, but a Pokemon may be able to push it aside.");
        return true;
    }
    if (c == 'W' && w.surfing) {
        if (CanField(g, i, "Waterfall", &who)) Ask(w, "It's a large waterfall. Would you like to use Waterfall?", {{"Use Waterfall (" + who + ")", "field|Waterfall"}, {"No", ""}});
        else Say(w, "It's a large waterfall. A Pokemon may be able to climb it.");
        return true;
    }
    if (IsWater(c)) {
        std::vector<std::pair<std::string, std::string>> ch;
        if (!w.surfing && CanField(g, i, "Surf", &who) && (c == 'w' || c == 'd')) ch.push_back({"Surf (" + who + ")", "field|Surf"});
        for (const char* rod : {"Old Rod", "Good Rod", "Super Rod"})
            if (BagCount(WP(g, i), rod) > 0) ch.push_back({std::string("Fish: ") + rod, std::string("fish|") + rod});
        if (ch.empty()) { if (!w.surfing) Say(w, PartyKnows(WP(g, i), "Surf") ? "The water is dyed a deep blue...|" + FieldNeed("Surf", MapRegionOf(g, i)) : "The water is dyed a deep blue..."); return !w.surfing; }
        ch.push_back({"Cancel", ""});
        Ask(w, "The water is dyed a deep blue...", ch);
        return true;
    }
    if (m.dark && w.flashMap != w.map && CanField(g, i, "Flash", &who)) {
        Ask(w, "It's pitch dark in here. Would you like to use Flash?", {{"Use Flash (" + who + ")", "field|Flash"}, {"No", ""}});
        return true;
    }
    return false;
}

// Blacked out: home to Mom.
bool BlackOut(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const Profile& p = WP(g, i);
    if (!w.msg.empty() || w.stepT < 1 || p.mons.empty() || HealthyCount(p) > 0) return false;
    // the last Pokemon Center you used, like the games; home to Mom before you've used one
    for (const auto& f : Flags(g)) {
        if (f.rfind("center:", 0) != 0) continue;
        auto p = Split(f.substr(7), ':');
        if (p.size() < 3) break;
        int cm = MapIndex(p[0]), cx = std::atoi(p[1].c_str()), cy = std::atoi(p[2].c_str());
        if (cm < 0) break;
        if (w.map == cm && w.x == cx && w.y == cy) return false;
        if (WalkerAt(g, i, cm, cx, cy)) cy++;
        w.surfing = w.biking = false;
        w.face = 1;
        PlaceAt(g, i, cm, cx, cy);
        w.fadeT = 0.35f;
        SavePos(g);
        Say(w, WP(g, i).name + " scurried to a Pokemon Center, protecting the exhausted and fainted Pokemon from further harm...|Nurse: Let's get your Pokemon properly rested!", "heal");
        return true;
    }
    int home = MapIndex("Your House 1F");
    if (home < 0 || w.map == home) return false;
    int x = 9, y = 6;
    if (WalkerAt(g, i, home, x, y)) y = 7;
    w.surfing = w.biking = false;
    PlaceAt(g, i, home, x, y);
    w.face = 2;
    w.fadeT = 0.35f;
    SavePos(g);
    Say(w, "You hurried home to rest your Pokemon...|Mom: You look tired! Let your Pokemon rest a while.", "heal");
    return true;
}

// Between steps: what's waiting to happen to this walker.
//   "winthen|i|action" (a route battle won - screens_story.cpp sets it): the action now (the next Winstrate, Selphy's way home)
//   a map's "auto = condition|action" lines, once the condition holds (they make it false themselves)
//   the Fame Checker: arriving somewhere you hear about someone (World/fame.txt "enter")
bool WorldFollowUps(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    if (g.screen != Screen::World || g.ow.sub[i] || !w.msg.empty() || w.map < 0) return false;
    auto& fl = Flags(g);
    const std::string key = "winthen|" + std::to_string(i) + "|";
    auto it = fl.lower_bound(key);
    if (it != fl.end() && it->rfind(key, 0) == 0) {
        std::string act = it->substr(key.size());
        fl.erase(it);
        RunAction(g, i, act);
        return true;
    }
    const OwMap& m = MapOf(w);
    for (const auto& [cond, act] : m.autos)
        if (HasFlag(g, i, cond)) { RunAction(g, i, act); return true; }
    static int seen[2] = {-1, -1};
    if (seen[i] != w.map) {
        seen[i] = w.map;
        for (const auto& note : FameNotesOnEnter(m.name)) FameNote(g, i, note);
    }
    return false;
}

void UpdateWalker(Game& g, int i, float dt, const WalkIn& in) {
    OwWalker& w = g.ow.w[i];
    gPlayerName = WP(g, i).name;
    gTrendNow = DewfordTrend(g);
    w.bannerT = std::max(0.0f, w.bannerT - dt);
    w.fadeT = std::max(0.0f, w.fadeT - dt);
    w.bumpT = std::max(0.0f, w.bumpT - dt);
    if (w.fishing) { w.fishT += dt; if (w.msg.empty() || g.screen != Screen::World) w.fishing = false; }
    if (BlackOut(g, i)) return;
    if (!w.msg.empty()) {   // a message box
        if (w.msgPage >= (int)w.msg.size() - 1 && !w.choices.empty()) {   // ...that asks: pick one
            int n = (int)w.choices.size();
            if (in.up) { w.choiceSel = (w.choiceSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
            if (in.down) { w.choiceSel = (w.choiceSel + 1) % n; PlaySfx(Sfx::MenuMove); }
            if (in.confirm || in.back) {
                std::string act = in.back ? "" : w.choices[std::min(w.choiceSel, n - 1)].second;
                w.msg.clear();
                w.choices.clear();
                w.after = act;
                PlaySfx(in.back ? Sfx::MenuBack : Sfx::MenuOk);
                AfterMessage(g, i);
            }
            return;
        }
        if (in.confirm || in.back) {
            PlaySfx(Sfx::MenuMove);
            if (in.back && w.after == "teamyes") { w.msg.clear(); w.after.clear(); OnlineSendTeam(3); return; }   // no thanks
            if (++w.msgPage >= (int)w.msg.size()) { w.msg.clear(); AfterMessage(g, i); }
        }
        return;
    }
    if (w.approach >= 0) {   // a trainer walks over
        w.approachT += dt;
        if (w.approachT < 0) return;
        NpcRt& r = gNpc[w.map][w.approach];
        int nx = r.x + DX[r.face], ny = r.y + DY[r.face];
        if (nx == w.x && ny == w.y) {   // next to you: the challenge
            int k = w.approach;
            w.approach = -1;
            w.face = Opp(r.face);
            const OwNpc& n = MapOf(w).npcs[k];
            int line = n.kind == "fight" ? 2 : 1;
            Say(w, (int)n.args.size() > line && !n.args[line].empty() ? n.args[line] : "Let's battle!", "battle:" + std::to_string(k));
            return;
        }
        if (w.approachT >= WALK) { w.approachT = 0; r.x = nx; r.y = ny; }
        return;
    }
    if (w.stepT < 1) {   // walking
        auto durOf = [&]() { return w.hop ? WALK * 1.6f : w.spin >= 0 ? 0.09f : w.biking ? BIKE : w.surfing ? WALK * 0.85f : (w.running ? RUN : WALK); };
        float t = w.stepT + dt / durOf();
        if (t < 1) { w.stepT = t; return; }
        // The step is done. Still holding the way on? Go straight into the next step with the time left
        // over, so walking doesn't hitch for a frame at every tile.
        float spare = (t - 1) * durOf();
        w.stepT = 1;
        w.hop = false;
        int mapBefore = w.map;
        Arrived(g, i);
        if (g.screen != Screen::World || !w.msg.empty() || w.approach >= 0 || w.map != mapBefore || g.ow.menu || g.encounterT > 0) return;
        int d = w.spin >= 0 ? w.spin : in.dir;
        if (d < 0 || spare <= 0) return;
        if (w.spin < 0) w.running = in.run && (BagCount(WP(g, i), "Running Shoes") > 0 || Flags(g).count("shoes")) && !w.surfing && !w.biking;
        TryStep(g, i, d);
        if (w.stepT < 1) w.stepT = std::min(0.95f, spare / durOf());
        else if (w.spin >= 0) w.spin = -1;
        return;
    }
    if (WorldFollowUps(g, i)) return;   // a battle won that leads somewhere, a map's own events, the Fame Checker's notes
    if (in.pause) { g.ow.menu = true; g.ow.menuSel = 0; g.ow.menuWho = i; PlaySfx(Sfx::MenuOk); return; }   // the menu
    if (w.spin >= 0) {   // spin tiles: slide on until a stop tile or a wall
        TryStep(g, i, w.spin);
        if (w.stepT >= 1) w.spin = -1;
        return;
    }
    if (in.confirm) {
        const OwMap& m = MapOf(w);
        int fx = w.x + DX[w.face], fy = w.y + DY[w.face];
        int idx = NpcAt(g, w.map, fx, fy);
        if (idx >= 0) { Interact(g, i, idx); return; }
        if (Online(g) && i == 0 && g.ow.w[1].here && g.ow.w[1].map == w.map && g.ow.w[1].x == fx && g.ow.w[1].y == fy) {   // your partner
            const std::string& pn = g.coopP[1 - g.on.me].name;
            if (PartnerBattling(g)) {   // watch their battle
                g.ow.watching = true;
                g.ow.watchBattle.reset();
                OnlineSendWatch(true);
                PlaySfx(Sfx::MenuOk);
            } else if (g.ow.teamUp) {
                g.ow.teamUp = false;
                OnlineSendTeam(0);
                Say(w, "You split up with " + pn + ".|Wild Pokemon are your own battles again.");
            } else {
                OnlineSendTeam(1);
                Say(w, "You asked " + pn + " to team up...");
            }
            return;
        }
        if (LocalCoop(g)) {   // local: face your partner and press your button to team up / split up
            int o = 1 - i;
            const OwWalker& p = g.ow.w[o];
            if (p.map == w.map && p.x == fx && p.y == fy && !g.ow.sub[o]) {
                g.ow.teamUp = !g.ow.teamUp;
                PlaySfx(Sfx::Buff);
                std::string t = g.ow.teamUp ? "You teamed up with " + WP(g, o).name + "!|Wild Pokemon in the grass are 2v2 battles for you both now."
                                           : "You split up with " + WP(g, o).name + ".|Wild Pokemon are your own battles again.";
                Say(w, t);
                return;
            }
        }
        for (const auto& s : m.spots)
            if (s.x == fx && s.y == fy && CondOk(g, i, s.ifF, s.unlessF)) {
                for (const auto& note : FameNotesAt(m.name, fx, fy)) FameNote(g, i, note);   // (a sign the Fame Checker remembers)
                RunAction(g, i, s.act);
                return;
            }
        if (DiveHere(g, i)) return;
        for (const auto& st : m.stands)   // a puzzle's spot (the Regis' tombs): use the move standing here
            if (st.x == w.x && st.y == w.y && !HasFlag(g, i, st.flag)) {
                std::string who;
                int hm = HmIndex(st.move);
                bool can = hm > 0 ? CanField(g, i, st.move, &who) : PartyKnows(WP(g, i), st.move, &who);
                if (!can) break;
                Ask(w, "Would you like to use " + st.move + " here?", {{"Use " + st.move + " (" + who + ")", "standuse|" + st.flag}, {"No", ""}});
                return;
            }
        if (BerryTreeHere(g, i, fx, fy)) return;
        if (FacingThing(g, i, fx, fy)) return;
        // across a counter: the Nurse or the clerk on the other side
        int idx2 = NpcAt(g, w.map, fx + DX[w.face], fy + DY[w.face]);
        if (idx2 >= 0 && m.At(fx, fy) == '#') { Interact(g, i, idx2); return; }
    }
    w.running = in.run && (BagCount(WP(g, i), "Running Shoes") > 0 || Flags(g).count("shoes")) && !w.surfing && !w.biking;
    if (in.dir >= 0) TryStep(g, i, in.dir);
}

// Online: your partner's figure walks to where their game says they are.
void UpdateGhost(Game& g, float dt) {
    OwWalker& w = g.ow.w[1];
    auto& q = g.ow.ghostQ;
    if (!Online(g) || !w.here) return;
    float spare = dt;
    for (int guard = 0; guard < 3 && spare > 0; guard++) {
        if (w.stepT >= 1) {
            if (q.empty()) return;
            auto [x, y, face] = q.front();
            q.erase(q.begin());
            w.fFromX = w.fx; w.fFromY = w.fy;
            w.fx = w.x; w.fy = w.y;
            w.fFace = w.fx != w.fFromX ? (w.fx > w.fFromX ? 3 : 2) : (w.fy > w.fFromY ? 0 : 1);
            w.fromX = w.x; w.fromY = w.y;
            w.x = x; w.y = y; w.face = face & 3;
            w.running = face & 4; w.biking = face & 8; w.surfing = face & 16;
            w.hop = std::abs(y - w.fromY) == 2 || std::abs(x - w.fromX) == 2;
            w.walkAnim ^= 1;
            w.stepT = 0;
        }
        float dur = w.hop ? WALK * 1.6f : w.biking ? BIKE : w.surfing ? WALK * 0.85f : w.running ? RUN : WALK;
        if (q.size() > 2) dur *= 0.6f;   // behind: catch up
        float t = w.stepT + spare / dur;
        spare = t > 1 ? (t - 1) * dur : 0;
        w.stepT = std::min(1.0f, t);
    }
}

// Online: you were called over to a story battle your partner started.
void TakeIncoming(Game& g) {
    OwState& o = g.ow;
    OwWalker& w = o.w[0];
    if (!o.incoming || !w.msg.empty() || w.stepT < 1 || w.approach >= 0 || o.sub[0]) return;
    o.incoming = false;
    if (o.inMap >= 0 && o.inMap < (int)gMaps.size()) {
        int ox = o.inX, oy = o.inY;
        if (!FreeBeside(g, 0, o.inMap, o.inX, o.inY, w.face, ox, oy)) { ox = o.inX; oy = o.inY + 1; }
        PlaceAt(g, 0, o.inMap, ox, oy);
        w.fadeT = 0.35f;
    }
    if (o.inStage < 0) {   // they ran into a wild Pokemon: straight into the battle beside them
        o.wildArea = o.inMap >= 0 && o.inMap < (int)gMaps.size() ? gMaps[o.inMap].wild : "";
        size_t bar = o.inTrainer.find('|');
        o.wildMethod = bar != std::string::npos ? o.inTrainer.substr(bar + 1) : "walk";
        Toast(g, g.coopP[1 - g.on.me].name + " found a wild Pokemon! You run over to battle it together!");
        OnlineSetReady(g, 2); g.ow.waitFight = true; g.ow.wildBattle = true; g.ow.waitName = "the wild Pokemon";
    } else Say(w, g.coopP[1 - g.on.me].name + " was challenged by " + o.inTrainer + "!|You hurry over to help!", "joinfight");
}

// ---- the menu (like the games: Start)
enum MenuRow { MR_DEX, MR_MON, MR_BAG, MR_MAP, MR_RADIO, MR_QUEST, MR_LOOK, MR_PRACTICE, MR_WAIT, MR_TRADE, MR_HOF, MR_BOARD, MR_NEXT, MR_SAVE, MR_SETTINGS, MR_QUIT };
std::vector<int> MenuRowsOf(const Game& g) {
    std::vector<int> v{MR_DEX, MR_MON, MR_BAG, MR_MAP};
    if (InJohto(g, g.ow.menuWho) && BagCount(WP(g, g.ow.menuWho), "Pokegear") > 0) v.push_back(MR_RADIO);   // (HeartGold's Pokegear)
    for (int r : {MR_QUEST, MR_LOOK, MR_PRACTICE, MR_WAIT}) v.push_back(r);
    if (g.coop) v.push_back(MR_TRADE);
    if (HallOfFameCount(g) > 0) v.push_back(MR_HOF);
    if (g.coop) v.push_back(MR_BOARD);
    if (Flags(g).count("ow:visit") || CanChooseRegion(g)) v.push_back(MR_NEXT);
    v.push_back(MR_SAVE);
    v.push_back(MR_SETTINGS);
    v.push_back(MR_QUIT);
    return v;
}
std::string NextRegionName(const Game& g) {
    int s = std::min(StoryStageNow(g), (int)Story().stages.size() - 1);
    return s >= 0 ? RegionInfo(StageRegion(s)).name : "the Story menu";
}
std::string MenuLabel(const Game& g, int r) {
    switch (r) {
        case MR_DEX: return "POKeDEX";
        case MR_MON: return "POKeMON";
        case MR_BAG: return "BAG";
        case MR_MAP: return "MAP";
        case MR_RADIO: return "RADIO";
        case MR_QUEST: return Cfg().questMarker ? "QUEST: ON" : "QUEST: OFF";
        case MR_LOOK: return "YOUR LOOK";
        case MR_PRACTICE: return "PRACTICE";
        case MR_WAIT: return IsNight(StoryClock(g)) ? "WAIT TILL DAWN" : "WAIT";
        case MR_TRADE: return "TRADE";
        case MR_HOF: return "HALL OF FAME";
        case MR_BOARD: return "LEADERBOARD";
        case MR_NEXT: return Flags(g).count("ow:visit") ? "LEAVE KANTO" : "NEXT REGION";
        case MR_SAVE: return "SAVE";
        case MR_SETTINGS: return "SETTINGS";
        default: return "SAVE & QUIT";
    }
}
std::string MenuTip(const Game& g, int r) {
    switch (r) {
        case MR_DEX: return "Your Pokedex";
        case MR_MON: return "Your party and your Box";
        case MR_BAG: return "Items, Medicine, key items";
        case MR_MAP: return "The Town Map - Fly to towns you've been to";
        case MR_RADIO: return "The Pokegear's radio: Prof. Oak, Pokemon Music, Buena's Password...";
        case MR_QUEST: return "An arrow toward your next story battle (Enter: turn it on or off)";
        case MR_LOOK: return "Change how your trainer looks";
        case MR_PRACTICE: return "The practice arena: try out moves on training dummies";
        case MR_WAIT: return "Rest until morning - only indoors (a house, a Pokemon Center...)";
        case MR_TRADE: return "Trade Pokemon with your partner";
        case MR_HOF: return "Your Hall of Fame";
        case MR_BOARD: return "Your journey's leaderboard: who did what";
        case MR_NEXT: return Flags(g).count("ow:visit") ? "Back to " + NextRegionName(g) + " and its Story menu" : "On to the next region (you can come back from its Story menu)";
        case MR_SAVE: return "Save your progress";
        case MR_SETTINGS: return "Sound, controls and display";
        default: return "Save and go back to the title screen";
    }
}
PlayerInput MenuOf(const Game& g) { return LocalCoop(g) ? ReadInput(g.ow.menuWho, true) : MenuInput(); }

void LeaveWorld(Game& g) {   // off to the Story menu (another region's)
    g.ow.active = false;
    g.ow.menu = false;
    if (Online(g)) OnlineSendWorldPos(false, -1, 0, 0, 0);
}

void UpdatePauseMenu(Game& g) {
    OwState& o = g.ow;
    PlayerInput in = MenuOf(g);
#ifdef ARENA_AUTOTEST
    if (gAutoMenu) { in.up = gAutoMenu & 1; in.down = gAutoMenu & 2; in.confirm = gAutoMenu & 16; in.back = gAutoMenu & 32; gAutoMenu = 0; }
#endif
    if (IsKeyPressed(KEY_ESCAPE) && !LocalCoop(g)) in.back = true;
    auto rows = MenuRowsOf(g);
    const int N = (int)rows.size();
    o.menuSel = std::max(0, std::min(N - 1, o.menuSel));
    if (in.up) { o.menuSel = (o.menuSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { o.menuSel = (o.menuSel + 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.back) { o.menu = false; PlaySfx(Sfx::MenuBack); return; }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    int who = WhoOf(g, o.menuWho);
    OwWalker& w = o.w[o.menuWho];
    SaveAll(g);
    int row = rows[o.menuSel];
    if (row == MR_QUEST) { Cfg().questMarker = !Cfg().questMarker; SaveSettings(); return; }   // (the menu stays open)
    o.menu = false;
    switch (row) {
        case MR_DEX: OpenPokedex(g, who); break;
        case MR_MON:
            if (g.coop) CoopOpenFor(g, who);
            g.myCreSel = 0; g.itemPick = false; g.actMenu = false; g.swapFrom = -1; g.stonePick = false; g.moveOrder = false; g.boxTab = 0;
            g.screen = Screen::MyCreatures;
            break;
        case MR_BAG:
            if (g.coop) CoopOpenFor(g, who);
            OpenBag(g);
            break;
        case MR_MAP: o.townMap = true; o.townSel = 0; break;
        case MR_RADIO: RunAction(g, o.menuWho, "radio"); break;
        case MR_LOOK:
            if (g.coop) CoopOpenFor(g, who);
            OpenCreator(g, true);
            break;
        case MR_PRACTICE: StartPractice(g); break;
        case MR_WAIT:
            if (!MapOf(w).indoor) { Say(w, "You can't rest out here!|Find somewhere indoors - a house or a Pokemon Center."); break; }
            if (!IsNight(StoryClock(g))) { Say(w, "It's already daytime.|(You can rest until morning once night falls.)"); break; }
            if (Online(g) && g.on.me != 0) { Say(w, "Your partner keeps the time - ask them to rest until morning."); break; }
            StoryTimeStep(g, true);
            (g.coop ? g.coopSave.clockSec : g.profile.clockSec) = 0;
            SaveAll(g);
            if (Online(g)) OnlineSendCoop(g);
            w.fadeT = 0.35f;
            break;
        case MR_TRADE: OpenTrade(g); break;
        case MR_HOF: g.hofSel = 0; g.hofBack = Screen::World; g.hofT = 0; g.screen = Screen::HallOfFame; break;
        case MR_BOARD: g.showBoard = true; break;
        case MR_NEXT:
            if (Flags(g).count("ow:visit")) {
                Flags(g).erase("ow:visit");
                if (Online(g)) OnlineSendCoop(g);
                SaveAll(g);
                LeaveWorld(g);
                EnterStoryHub(g);
                break;
            }
            if (Online(g) && g.on.me != 0) { Toast(g, g.coopP[0].name + " picks the next region."); break; }
            LeaveWorld(g);
            BeginRegionChoose(g, g.coop ? 3 : 1);
            break;
        case MR_SAVE: Toast(g, g.coop ? "Your journey together was saved." : WP(g, o.menuWho).name + " saved the game."); PlaySfx(Sfx::Save); break;
        case MR_SETTINGS: g.settingsSel = 0; g.settingsBack = Screen::World; g.screen = Screen::Settings; break;
        default: Ask(w, "Save and quit to the title screen?", {{"Yes", "quitgame"}, {"No", ""}}); break;
    }
}

std::vector<int> Towns(int region = -1) {   // (region: only that region's - the Town Map shows where you are)
    std::vector<int> v;
    for (size_t k = 0; k < gMaps.size(); k++) if (!gMaps[k].town.empty() && (region < 0 || gMaps[k].region == region)) v.push_back((int)k);
    return v;
}

void UpdateTownMap(Game& g) {
    OwState& o = g.ow;
    PlayerInput in = MenuOf(g);
#ifdef ARENA_AUTOTEST
    if (gAutoMenu) { in.up = gAutoMenu & 1; in.down = gAutoMenu & 2; in.confirm = gAutoMenu & 16; in.back = gAutoMenu & 32; gAutoMenu = 0; }
#endif
    if (IsKeyPressed(KEY_ESCAPE) && !LocalCoop(g)) in.back = true;
    auto t = Towns(MapRegionOf(g, o.menuWho));
    int n = (int)t.size();
    if (in.back || n == 0) { o.townMap = false; PlaySfx(Sfx::MenuBack); return; }
    if (in.up || in.left) { o.townSel = (o.townSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.down || in.right) { o.townSel = (o.townSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    int i = o.menuWho;
    OwWalker& w = o.w[i];
    const OwMap& to = gMaps[t[std::min(o.townSel, n - 1)]];
    std::string who;
    if (!Flags(g).count("visit:" + to.town)) { Toast(g, "You haven't been to " + to.town + " yet."); PlaySfx(Sfx::MenuBack); return; }
    if (!CanField(g, i, "Fly", &who)) { Toast(g, FieldNeed("Fly", MapRegionOf(g, i))); PlaySfx(Sfx::MenuBack); return; }
    if (MapOf(w).indoor) { Toast(g, "You can't Fly from indoors!"); PlaySfx(Sfx::MenuBack); return; }
    if (to.flyX < 0) { Toast(g, "There's nowhere to land there."); return; }
    o.townMap = false;
    w.surfing = w.biking = false;
    PlaceAt(g, i, t[std::min(o.townSel, n - 1)], to.flyX, to.flyY);
    w.face = 0;
    w.fadeT = 0.6f;
    SavePos(g);
    PlaySfx(Sfx::Buff);
    Toast(g, who + " used Fly! You flew to " + to.town + ".");
}

} // namespace

// ---------------------------------------------------------------- loading

bool WorldAvailable() { return gLoaded && !gMaps.empty(); }
bool WorldBattleOnWater(const Game& g) {   // a wild battle met surfing, fishing or under the sea (the Dive Ball)
    if (!g.ow.wildBattle) return false;
    const std::string& m = g.ow.wildMethod;
    if (m == "surf" || m.find("rod") != std::string::npos) return true;
    int mi = g.ow.w[0].map;
    return mi >= 0 && mi < (int)gMaps.size() && gMaps[mi].underwater;
}

// A map's pictures, loaded the first time it's on screen; the ones not seen for a while are let go
// (Kanto and the Sevii Islands are a few hundred maps).
void TouchMap(int map) {
    if (map < 0 || map >= (int)gMaps.size()) return;
    OwMap& m = gMaps[map];
    m.used = GetTime();
    if (m.tex.id) return;
    std::string dir = DataDir() + "World/";
    m.tex = LoadTex(dir + (m.altShown ? m.altImage : m.image));
    const std::string& ov = m.altShown ? m.altOver : m.overImage;
    if (!ov.empty()) m.over = LoadTex(dir + ov);
    if (!m.img2.empty()) m.tex2 = LoadTex(dir + m.img2);
    int loaded = 0;
    for (const auto& o : gMaps) loaded += o.tex.id != 0;
    while (loaded > 24) {
        OwMap* old = nullptr;
        for (auto& o : gMaps) if (o.tex.id && (!old || o.used < old->used)) old = &o;
        if (!old || old == &m) break;
        UnloadTexture(old->tex); old->tex = {};
        if (old->over.id) { UnloadTexture(old->over); old->over = {}; }
        if (old->tex2.id) { UnloadTexture(old->tex2); old->tex2 = {}; }
        loaded--;
    }
}

void LoadWorld(std::string& log) {
    for (auto& m : gMaps) { if (m.tex.id) UnloadTexture(m.tex); if (m.over.id) UnloadTexture(m.over); if (m.tex2.id) UnloadTexture(m.tex2); }
    gMaps.clear();
    gSpriteNames.clear();
    gStoryTown.clear();
    gWalkedRegions.clear();
    gSkipStages.clear();
    gFerries.clear();
    gLoaded = false;
    LoadTutors();
    LoadTrades();
    LoadLilycoveLady();
    std::string dir = DataDir() + "World/";
    std::ifstream f0(dir + "world.txt");
    if (!f0) return;
    if (std::ifstream cn(dir + "chars.txt"); cn) {   // the people on chars.png, row by row (FireRed's, HeartGold's, Emerald's)
        std::string nm;
        while (cn >> nm) gSpriteNames.push_back(nm);
    }
    bool namesFromSheet = !gSpriteNames.empty();
    std::stringstream f;   // Kanto and the Sevii Islands (tools/frlg_world.py), then Johto (tools/hgss_world.py)
    f << f0.rdbuf() << "\n";
    if (std::ifstream f1(dir + "johto.txt"); f1) f << f1.rdbuf() << "\n";
    if (std::ifstream f2(dir + "hoenn.txt"); f2) f << f2.rdbuf() << "\n";   // Hoenn (tools/emerald_world.py)
    if (std::ifstream f3(dir + "extras.txt"); f3) f << f3.rdbuf() << "\n";   // hand-made maps (the Goldenrod Tunnel)
    std::string line;
    OwMap* m = nullptr;
    bool grid = false, hgrid = false;
    std::vector<std::pair<int, std::vector<std::string>>> links;   // resolved after every map is read
    std::string* lastIf = nullptr;     // "if = flag" / "unless = flag" apply to the last npc / door / spot / item
    OwNpc* lastNpc = nullptr;
    std::string* lastUnless = nullptr;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) { grid = hgrid = false; continue; }
        if (line[0] == '#' && !grid && !hgrid) continue;
        if (line == "[map]") { gMaps.emplace_back(); m = &gMaps.back(); grid = hgrid = false; lastIf = lastUnless = nullptr; lastNpc = nullptr; continue; }
        if (hgrid && m) { m->hts += line; continue; }
        if (grid && m) {
            if (m->w == 0) m->w = (int)line.size();
            if ((int)line.size() == m->w) { m->cells += line; m->h++; }
            continue;
        }
        size_t eq = line.find(" = ");
        std::string k = Trim(line.substr(0, eq)), v = eq == std::string::npos ? "" : Trim(line.substr(eq + 3));
        if (k == "sprites") { if (!namesFromSheet) { std::istringstream ss(v); std::string s; while (ss >> s) gSpriteNames.push_back(s); } continue; }
        if (k == "walked") { for (const auto& r : Split(v, ',')) for (int i = 0; i < 8; i++) if (RegionInfo(i).name == Trim(r)) gWalkedRegions.insert(i); continue; }
        if (k == "story_skip") { gSkipStages.insert(Trim(v)); continue; }
        if (k == "ferry") {
            auto p = Split(v, '|');
            if (p.size() >= 3) { Ferry f; f.label = Trim(p[0]); f.map = Trim(p[1]); std::sscanf(p[2].c_str(), "%d %d", &f.x, &f.y); f.cond = p.size() > 3 ? Trim(p[3]) : ""; gFerries.push_back(f); }
            continue;
        }
        if (k == "story_town") { auto p = Split(v, '|'); if (p.size() == 2) gStoryTown[Trim(p[0])] = Trim(p[1]); continue; }
        if (!m) continue;
        if (line == "grid:") { grid = true; continue; }
        if (line == "heights:") { hgrid = true; continue; }
        if (k == "name") m->name = v;
        else if (k == "region") { for (int r = 0; r < RegionCount(); r++) if (RegionInfo(r).name == v) m->region = r; }
        else if (k == "imgy") m->imgOffY = std::atoi(v.c_str());
        else if (k == "pos") { m->hasPos = std::sscanf(v.c_str(), "%d %d", &m->gx, &m->gy) == 2; }
        else if (k == "image") m->image = v;
        else if (k == "over") m->overImage = v;
        else if (k == "cycling") m->cycling = v == "yes";
        else if (k == "divespot") {
            OwMap::DiveSpot d{};
            int n = 0;
            std::sscanf(v.c_str(), "%d %d %d %d %n", &d.x0, &d.y0, &d.x1, &d.y1, &n);
            auto p = Split(v.substr(n), '|');
            if (p.size() >= 4) { d.flag = Trim(p[0]); d.map = Trim(p[1]); d.tx = std::atoi(p[2].c_str()); d.ty = std::atoi(p[3].c_str()); m->diveSpots.push_back(d); }
        }
        else if (k == "emergeat") {
            int x = 0, y = 0, n = 0;
            std::sscanf(v.c_str(), "%d %d %n", &x, &y, &n);
            auto p = Split(v.substr(n), '|');
            if (p.size() >= 3) m->emergeAt.push_back({x, y, Trim(p[0]), std::atoi(p[1].c_str()), std::atoi(p[2].c_str())});
        }
        else if (k == "alt") { auto p = Split(v, '|'); if (p.size() >= 2) { m->altFlag = Trim(p[0]); m->altImage = Trim(p[1]); m->altOver = p.size() > 2 ? Trim(p[2]) : ""; } }
        else if (k == "objects") m->objSprites = v == "sprites";
        else if (k == "wild") m->wild = v;
        else if (k == "wildif") m->wildIf = v;
        else if (k == "indoor") m->indoor = v == "yes";
        else if (k == "dark") m->dark = v == "yes";
        else if (k == "weather") m->weather = v;
        else if (k == "stand") {
            OwMap::Stand st; int n = 0;
            if (std::sscanf(v.c_str(), "%d %d %n", &st.x, &st.y, &n) >= 2) {
                auto p = Split(v.substr(n), '|');
                st.move = Trim(p[0]); st.flag = p.size() > 1 ? Trim(p[1]) : ""; st.text = p.size() > 2 ? p[2] : "";
                m->stands.push_back(st);
            }
        }
        else if (k == "lap") {
            auto p = Split(v, '|');
            if (p.size() >= 3) {
                m->lapFlag = Trim(p[0]);
                std::sscanf(p[1].c_str(), "%d %d", &m->lapGX, &m->lapGY);
                std::istringstream ss(p[2]); std::string xy;
                while (ss >> xy) { int x = 0, y = 0; if (std::sscanf(xy.c_str(), "%d,%d", &x, &y) == 2) m->lap.push_back({x, y}); }
            }
        }
        else if (k == "berry") { OwMap::BerryTree bt; char nm[32] = {}; if (std::sscanf(v.c_str(), "%d %d %d %31s", &bt.x, &bt.y, &bt.id, nm) == 4) { bt.berry = nm; m->berries.push_back(bt); lastIf = &m->berries.back().ifF; lastUnless = nullptr; } }
        else if (k == "wxzone") { OwMap::WxZone z; char kind[16] = {}; if (std::sscanf(v.c_str(), "%d %d %d %d %15s", &z.x0, &z.y0, &z.x1, &z.y1, kind) == 5) { z.wx = kind; m->wxZones.push_back(z); } }
        else if (k == "bike") m->bike = v == "yes";
        else if (k == "underwater") m->underwater = v == "yes";
        else if (k == "dive" || k == "emerge") {   // Map, or Map|x|y
            auto p = Split(v, '|');
            (k == "dive" ? m->diveTo : m->emergeTo) = Trim(p[0]);
            if (p.size() >= 3) {
                (k == "dive" ? m->diveX : m->emergeX) = std::atoi(p[1].c_str());
                (k == "dive" ? m->diveY : m->emergeY) = std::atoi(p[2].c_str());
            }
        }
        else if (k == "fightopts" && lastNpc) lastNpc->opts = v;
        else if (k == "rematch" && lastNpc) { for (const auto& t : Split(v, '/')) lastNpc->rematch.push_back(Trim(t)); }
        else if (k == "town") m->town = v;
        else if (k == "fly") std::sscanf(v.c_str(), "%d %d", &m->flyX, &m->flyY);
        else if (k == "if" && lastIf) *lastIf = v;
        else if (k == "won" && lastNpc) lastNpc->winThen = v;
        else if (k == "auto") { size_t bar = v.find('|'); if (bar != std::string::npos) m->autos.push_back({v.substr(0, bar), v.substr(bar + 1)}); }
        else if (k == "need" && lastNpc) { auto p = Split(v, '|'); lastNpc->needItem = Trim(p[0]); lastNpc->needText = p.size() > 1 ? p[1] : ""; lastNpc->usedText = p.size() > 2 ? p[2] : ""; }
        else if (k == "unless" && lastUnless) *lastUnless = v;
        else if (k == "item") {   // x y Name [count] [hidden]
            OwMap::Item it;
            int n = 0;
            std::sscanf(v.c_str(), "%d %d %n", &it.x, &it.y, &n);
            std::string rest = v.substr(n);
            if (rest.size() > 7 && rest.substr(rest.size() - 7) == " hidden") { it.hidden = true; rest = Trim(rest.substr(0, rest.size() - 7)); }
            size_t sp = rest.find_last_of(' ');
            if (sp != std::string::npos && std::isdigit((unsigned char)rest[sp + 1])) { it.n = std::max(1, std::atoi(rest.c_str() + sp + 1)); rest = Trim(rest.substr(0, sp)); }
            it.item = rest;
            m->items.push_back(it);
            lastIf = &m->items.back().ifF; lastUnless = &m->items.back().unlessF;
        } else if (k == "block") {   // x y flag|text
            OwMap::Block b;
            int n = 0;
            std::sscanf(v.c_str(), "%d %d %n", &b.x, &b.y, &n);
            auto p = Split(v.substr(n), '|');
            b.flag = Trim(p[0]);
            size_t q = 1;
            for (; q < p.size() && (p[q].empty() || p[q][0] != '>'); q++) b.text += (q > 1 ? "|" : "") + p[q];
            for (size_t r = q; r < p.size(); r++) b.act += (r > q ? "|" : "") + (r == q ? p[r].substr(1) : p[r]);
            for (char& ch : b.text) if (ch == '/') ch = '|';
            m->blocks.push_back(b);
        } else if (k == "gate") {   // x y flag: solid until the flag is set
            OwMap::Gate gt;
            int n = 0;
            std::sscanf(v.c_str(), "%d %d %n", &gt.x, &gt.y, &n);
            gt.flag = Trim(v.substr(n));
            m->gates.push_back(gt);
        }
        else if (k == "start") std::sscanf(v.c_str(), "%d %d", &m->startX, &m->startY);
        else if (k == "connect") {
            std::istringstream ss(v);
            std::string dirn; ss >> dirn;
            std::string rest; std::getline(ss, rest);
            rest = Trim(rest);
            size_t sp = rest.find_last_of(' ');
            links.push_back({(int)gMaps.size() - 1, {dirn, rest.substr(0, sp), rest.substr(sp + 1)}});
        } else if (k == "north" || k == "south" || k == "west" || k == "east") {
            int d = k == "south" ? 0 : k == "north" ? 1 : k == "west" ? 2 : 3;
            m->link[d].msg = v;
        } else if (k == "img2") m->img2 = v;
        else if (k == "swap") {
            OwMap::Swap sw; int n = 0; std::sscanf(v.c_str(), "%d %d %n", &sw.x, &sw.y, &n);
            auto p = Split(Trim(v.substr(n)), ' ');
            sw.flag = p.empty() ? "" : p[0];
            if (p.size() > 1 && p[1].size() == 1) sw.cell = p[1][0];
            m->swaps.push_back(sw);
        }
        else if (k == "rtile") { int x, y, c, d; if (std::sscanf(v.c_str(), "%d %d %d %d", &x, &y, &c, &d) == 4) m->rtiles[y * 100000 + x] = {c, d}; }
        else if (k == "wildat") { OwMap::WildAt z; int n = 0; if (std::sscanf(v.c_str(), "%d %d %d %d %n", &z.x0, &z.y0, &z.x1, &z.y1, &n) >= 4) { z.area = Trim(v.substr(n)); m->wildAt.push_back(z); } }
        else if (k == "rgate") { OwMap::RGate rg{}; if (std::sscanf(v.c_str(), "%d %d %d %d", &rg.x, &rg.y, &rg.shape, &rg.orient) == 4) m->rgates.push_back(rg); }
        else if (k == "door" || k == "sign" || k == "spot" || k == "step") {
            int x = 0, y = 0, n = 0;
            std::sscanf(v.c_str(), "%d %d %n", &x, &y, &n);
            std::string act = v.substr(n);
            if (k == "sign") act = "msg|" + act;
            auto& list = k == "door" ? m->doors : k == "step" ? m->steps : m->spots;
            list.push_back({x, y, act, "", ""});
            lastIf = &list.back().ifF; lastUnless = &list.back().unlessF;
        } else if (k == "npc") {
            OwNpc p;
            char spr[32] = {}, face[16] = {};
            int n = 0;
            std::sscanf(v.c_str(), "%d %d %31s %15s %d %n", &p.x, &p.y, spr, face, &p.sight, &n);
            p.sprite = SpriteIndex(spr);
            if (std::strncmp(spr, "mon:", 4) == 0) { p.mon = SpeciesIndex(spr + 4); p.sprite = 0; }
            if (std::strncmp(spr, "apricorn:", 9) == 0) { p.mon = -3; p.tree = spr + 9; p.sprite = 0; }
            if (std::string(spr) == "raidboss") { p.mon = -2; p.sprite = 0; }   // the Raid Den: today's boss stands there
            p.face = FaceOf(face);
            p.wander = std::string(face) == "wander";
            auto parts = Split(v.substr(n), '|');
            p.kind = parts[0];
            p.args.assign(parts.begin() + 1, parts.end());
            if (p.kind == "talk" || p.kind == "heal" || p.kind == "shop") {   // the rest is one message
                std::string t;
                for (size_t i = 0; i < p.args.size(); i++) t += (i ? "|" : "") + p.args[i];
                p.args = {t};
            }
            if (p.kind == "fight") {   // name|team|before|after[|flag] -> + the flag a win sets (beat:Map:Name unless it's given)
                std::string own = p.args.size() > 4 ? Trim(p.args[4]) : "";
                while (p.args.size() < 4) p.args.push_back("");
                p.args.resize(4);
                p.args.push_back(own.empty() ? "beat:" + m->name + ":" + p.args[0] : own);
            }
            if (p.kind == "give" || p.kind == "legend" || p.kind == "fight" || p.kind == "encounter" || p.kind == "trainer") for (auto& a : p.args) for (char& ch : a) if (ch == '/') ch = '|';   // (text pages: "/" between them)
            m->npcs.push_back(p);
            lastIf = &m->npcs.back().ifF; lastUnless = &m->npcs.back().unlessF;
            lastNpc = &m->npcs.back();
        }
    }
    for (auto& [mi, a] : links) {
        int d = a[0] == "south" ? 0 : a[0] == "north" ? 1 : a[0] == "west" ? 2 : 3;
        int tm = MapIndex(a[1]), off = std::atoi(a[2].c_str());
        OwLink& L = gMaps[mi].link[d];
        if (L.map < 0) { L.map = tm; L.off = off; }
        else if (tm >= 0) {   // a second map on the same side (Water Path: Green Path, Six Island and Ruin Valley to the west)
            const OwMap& n = gMaps[tm];
            const OwMap& A = gMaps[mi];
            int dx = d == 2 ? -n.w : d == 3 ? A.w : -off, dy = d == 1 ? -n.h : d == 0 ? A.h : -off;
            gMaps[mi].near.push_back({tm, dx, dy});
        }
    }
    for (auto& mm : gMaps)   // two route trainers with one name on a map (Team Rocket's grunts): each their own win
        for (auto& p : mm.npcs) {
            if (p.kind != "fight" || p.args.size() < 5) continue;
            int same = 0;
            for (const auto& q : mm.npcs) same += q.kind == "fight" && !q.args.empty() && q.args[0] == p.args[0];
            if (same > 1) p.args[4] += "@" + std::to_string(p.x) + "," + std::to_string(p.y);
        }
    for (size_t a = 0; a < gMaps.size(); a++) {   // placed maps: whoever touches whom
        OwMap& A = gMaps[a];
        if ((int)A.hts.size() != A.w * A.h) A.hts.clear();
        if (!A.hasPos) continue;
        for (size_t b = 0; b < gMaps.size(); b++) {
            const OwMap& B = gMaps[b];
            if (a == b || !B.hasPos) continue;
            bool xs = B.gx < A.gx + A.w && A.gx < B.gx + B.w, ys = B.gy < A.gy + A.h && A.gy < B.gy + B.h;
            bool tx = B.gx == A.gx + A.w || A.gx == B.gx + B.w, ty = B.gy == A.gy + A.h || A.gy == B.gy + B.h;
            if ((xs && ty) || (ys && tx)) A.near.push_back({(int)b, B.gx - A.gx, B.gy - A.gy});
        }
    }
    gNpc.assign(gMaps.size(), {});
    gRt.assign(gMaps.size(), {});
    for (size_t i = 0; i < gMaps.size(); i++) ResetNpcs((int)i);
    for (auto& mm : gMaps)   // the pictures load when a map is first seen (see TouchMap); just check they're there
        if (!FileExists((dir + mm.image).c_str())) log += "World: missing " + mm.image + "\n";
    gChars = LoadTex(dir + "chars.png");
    gActions = LoadTex(dir + "actions.png");
    gFieldObj = LoadTex(dir + "fieldobj.png");
    gBerryTex = LoadTex(dir + "berrytrees.png");
    gWxRain = LoadTex(dir + "weather_rain.png");   // FireRed's weather
    gWxSnow[0] = LoadTex(dir + "weather_snow0.png");
    gWxSnow[1] = LoadTex(dir + "weather_snow1.png");
    gWxFog = LoadTex(dir + "weather_fog_horizontal.png");
    gWxSand = LoadTex(dir + "weather_sandstorm.png");
    gLoaded = !gMaps.empty();
    log += "World: " + std::to_string(gMaps.size()) + " maps\n";
}

std::string WorldPlaceName(const Game& g) {
    if (!WorldAvailable()) return "";
    std::string saved = g.coop ? g.coopSave.owMap[g.on.active ? g.on.me : 0] : g.profile.owMap;
    int m = MapIndex(saved);
    if (m < 0) {   // not out in the world yet: your room in the region your story's in
        std::string map; int x, y;
        int s = StoryStageNow(g);
        int r = s >= 0 && s < (int)Story().stages.size() ? StageRegion(s) : 0;
        if (WorldStartOf(r, map, x, y) || WorldStartOf(0, map, x, y)) return map;
    }
    return m >= 0 ? gMaps[m].name : "";
}

void EnterWorld(Game& g) {
    if (!WorldAvailable()) { Toast(g, "The overworld files are missing."); return; }
    // your room in the region your story is in (Kanto: Pallet Town; Johto: New Bark Town)
    int region = 0;
    {
        int s = StoryStageNow(g);
        const auto& order = g.coop ? g.coopSave.regionOrder : g.profile.regionOrder;
        if (s >= 0 && s < (int)Story().stages.size()) region = StageRegion(s);
        else if (!order.empty()) region = order.back();
        if (!gWalkedRegions.count(region) && s > 0 && s <= (int)Story().stages.size()) region = StageRegion(s - 1);   // (just past a region's end)
    }
    int start = -1;
    for (size_t i = 0; i < gMaps.size(); i++) if (gMaps[i].startX >= 0 && (start < 0 || gMaps[i].region == region)) start = (int)i;
    if (start < 0) start = 0;
    for (int i = 0; i < Driven(g); i++) g.ow.w[i] = OwWalker();   // (online: your partner's figure stays where their game said)
    g.ow.waitFight = false;
    g.ow.incoming = false;
    for (int i = 0; i < Driven(g); i++) {
        int who = WhoOf(g, i);
        std::string name = g.coop ? g.coopSave.owMap[who] : g.profile.owMap;
        int x = g.coop ? g.coopSave.owX[who] : g.profile.owX, y = g.coop ? g.coopSave.owY[who] : g.profile.owY;
        int face = g.coop ? g.coopSave.owFace[who] : g.profile.owFace;
        int m = MapIndex(name);
        if (name.empty() && StoryStageNow(g) > 0) {   // a save from before the walked world: the Pokemon Center of the town before its next battle
            int s = std::min(StoryStageNow(g), (int)Story().stages.size() - 1);
            auto it = gStoryTown.find(Story().stages[s].name);
            std::string town = it == gStoryTown.end() ? "" : it->second;
            int pc = MapIndex(town + " Pokemon Center"), tm = MapIndex(town);
            if (pc >= 0)
                for (const auto& n : gMaps[pc].npcs) if (n.kind == "heal") { m = pc; x = n.x + who; y = n.y + 2; face = 1; break; }
            if (m < 0 && tm >= 0 && gMaps[tm].flyX >= 0) { m = tm; x = gMaps[tm].flyX + who; y = gMaps[tm].flyY; face = 0; }
        }
        if (m < 0 || gMaps[m].At(x, y) == '#' || WalkerAt(g, i, m, x, y)) {   // your room
            m = start;
            x = gMaps[start].startX + who;   // player 2 beside player 1
            y = gMaps[start].startY;
            face = 0;
        }
        g.ow.w[i].face = face & 3;
        PlaceAt(g, i, m, x, y);
        g.ow.w[i].fadeT = 0.35f;
    }
    g.ow.active = true;
    g.mode = g.coop ? Mode::CoopStory : Mode::Story;
    g.screen = Screen::World;
    g.ow.menu = g.ow.townMap = false;
    // Running Shoes: Mom hands them over as you leave home; a journey that's already under way just has them
    if (StoryStageNow(g) > 0)
        for (int i = 0; i < Driven(g); i++)
            if (BagCount(WP(g, i), "Running Shoes") == 0) { BagAdd(WP(g, i), "Running Shoes"); Flags(g).insert("shoes"); if (g.coop) SaveProfile(WP(g, i)); }
    SendPos(g);
}

// Online: where your partner's game says they are. Steps join a little queue that their figure walks
// through at their own pace (running, cycling), so it never skips or jumps.
void WorldPartnerPos(Game& g, int map, int x, int y, int face) {
    OwWalker& w = g.ow.w[1];
    auto& q = g.ow.ghostQ;
    if (map < 0 || map >= (int)gMaps.size()) { w.here = false; q.clear(); return; }
    int lx = q.empty() ? w.x : q.back()[0], ly = q.empty() ? w.y : q.back()[1];
    bool step = w.here && w.map == map && std::abs(x - lx) + std::abs(y - ly) <= 2 && q.size() < 8;
    bool arrived = !w.here || w.map != map;
    if (step) {
        if (x != lx || y != ly) q.push_back({x, y, face});
        else { w.face = face & 3; w.running = face & 4; w.biking = face & 8; w.surfing = face & 16; }   // just turned (or got on the bike)
    } else {   // a new map, a warp or Fly: they appear there
        q.clear();
        w.fromX = w.fx = w.fFromX = x;
        w.fromY = w.fy = w.fFromY = y;
        w.stepT = 1;
        w.map = map;
        w.x = x; w.y = y; w.face = face & 3;
        w.running = face & 4; w.biking = face & 8; w.surfing = face & 16;
    }
    w.here = true;
    if (arrived && g.ow.w[0].map == map) ShareMap(g, map);   // they walked onto our map: show them how it is now
}

void WorldMapState(Game& g, int map, const std::string& st) {
    if (map < 0 || map >= (int)gRt.size()) return;
    MapRt r;
    size_t semi = st.find(';');
    if (semi == std::string::npos) return;
    auto nums = [](const std::string& s) { std::vector<int> v; size_t p = 0; while (p < s.size()) { size_t c = s.find(',', p); if (c == std::string::npos) c = s.size(); if (c > p) v.push_back(atoi(s.substr(p, c - p).c_str())); p = c + 1; } return v; };
    for (int k : nums(st.substr(0, semi))) r.cleared.insert(k);
    r.boulders = nums(st.substr(semi + 1));
    if (r.boulders.size() != gRt[map].boulders.size() && !gRt[map].boulders.empty()) return;   // not the same map (old data)
    gRt[map] = r;
}

void WorldPartnerFight(Game& g, int stage, int map, int x, int y, const std::string& trainer) {
    OwState& o = g.ow;
    o.incoming = true;
    o.inStage = stage; o.inMap = map; o.inX = x; o.inY = y;
    o.inTrainer = trainer;
    if (g.screen != Screen::World) Toast(g, g.coopP[1 - g.on.me].name + " was challenged by " + trainer + "! Pick Story battle (or walk around) to join.");
}

void WorldXpShare(Game& g, int xp) {
    if (!g.coop) return;
    int me = g.on.me;
    bool inSub = (bool)g.ow.sub[0];
    Profile& target = inSub ? g.ow.sub[0]->profile : (g.coopActive == me ? g.profile : g.coopP[me]);
    std::string note = ShareXp(target, xp);
    if (!inSub) { SaveProfile(target); OnlineSendProfile(g); }
    Toast(g, TextFormat("Exp. All: your party got %d XP from %s's battle.%s", xp, g.coopP[1 - me].name.c_str(), note.c_str()));
}

void WorldTeamMsg(Game& g, int what) {
    const std::string& pn = g.coopP[1 - g.on.me].name;
    OwWalker& w = g.ow.w[0];
    bool here = g.screen == Screen::World && !g.ow.sub[0];
    if (what == 1) {   // they asked
        if (here && w.msg.empty()) Say(w, pn + " wants to team up!|Team up? Wild Pokemon in the grass become 2v2 battles for you both.   (Enter: yes   Esc: no)", "teamyes");
        else OnlineSendTeam(3);
    } else if (what == 2) { g.ow.teamUp = true; if (here) Say(w, pn + " teamed up with you!|Wild Pokemon in the grass are 2v2 battles now."); else Toast(g, pn + " teamed up with you!"); }
    else if (what == 3) { if (here && w.msg.empty()) Say(w, pn + " would rather explore alone for now."); else Toast(g, pn + " didn't team up."); }
    else { g.ow.teamUp = false; Toast(g, pn + " split up with you."); }
}

void WorldWatchRequest(Game& g, bool on) {
    g.ow.watchers = on;
    if (!on) g.ow.watchSent = false;
}
void WorldWatchState(Game& g, const std::string& state) {
    if (!g.ow.watching) return;
    auto b = std::make_shared<Battle>();
    if (!LoadBattle(state, *b)) return;
    bool first = !g.ow.watchBattle;
    g.ow.watchBattle = b;
    if (first && IsWindowReady()) SetBattleArena(b->Setup().arena, b->GetField());
}
void WorldWatchStep(Game& g, float dt, const PlayerInput in[2]) {
    if (!g.ow.watching || !g.ow.watchBattle) return;
    int keep = gLocalInput;
    gLocalInput = 7;   // nobody's keys here
    g.ow.watchBattle->Update(dt, in);
    gLocalInput = keep;
}
void WorldWatchEnd(Game& g) {
    if (!g.ow.watching) return;
    g.ow.watching = false;
    g.ow.watchBattle.reset();
    Toast(g, g.coopP[1 - g.on.me].name + "'s battle is over.");
}

// ---------------------------------------------------------------- update

// People who stroll around: a step now and then, never far from home, never into anyone.
void UpdateWanderers(Game& g, float dt) {
    for (int map = 0; map < (int)gMaps.size(); map++) {
        bool here = false, busy = false;
        for (int i = 0; i < 2; i++) {
            const OwWalker& w = g.ow.w[i];
            if (!Shown(g, i) || w.map != map) continue;
            here = true;
            busy |= !w.msg.empty() || w.approach >= 0;
        }
        if (!here) continue;
        const OwMap& m = gMaps[map];
        for (int k = 0; k < (int)m.npcs.size(); k++) {
            const OwNpc& n = m.npcs[k];
            NpcRt& r = gNpc[map][k];
            if (r.st < 1) { r.st = std::min(1.0f, r.st + dt / (WALK * 1.25f)); continue; }   // (mid-step: a wanderer, or carried along by a rotating tile)
            if (!n.wander) continue;
            if (busy) continue;
            r.wt -= dt;
            if (r.wt > 0) continue;
            r.wt = 1.2f + SimRand(0, 280) / 100.0f;
            int d = SimRand(0, 3);
            r.face = d;
            if (SimRand(0, 2) == 0) continue;   // just looks around
            int nx = r.x + DX[d], ny = r.y + DY[d];
            if (std::abs(nx - n.x) > 2 || std::abs(ny - n.y) > 2) continue;
            char c = Cell(g, 0, map, nx, ny);
            if (!(c == '.' || c == 'g') || NpcAt(g, map, nx, ny) >= 0 || WalkerAt(g, -1, map, nx, ny) || ItemAt(g, 0, map, nx, ny, false) >= 0) continue;
            bool doorway = false;
            for (const auto& dd : m.doors) doorway |= dd.x == nx && dd.y == ny;
            if (doorway) continue;
            r.fromX = r.x; r.fromY = r.y;
            r.x = nx; r.y = ny;
            r.st = 0;
        }
    }
}

// The walked day: 20 minutes of daylight, 20 of night (the story clock; online, the host keeps it).
void WorldClock(Game& g, float dt) {
    OwState& o = g.ow;
    int& sec = g.coop ? g.coopSave.clockSec : g.profile.clockSec;
    o.clockAcc += dt;
    while (o.clockAcc >= 1) {
        o.clockAcc -= 1;
        if (Online(g) && g.on.me != 0) { sec = std::min(sec + 1, HALF_DAY_SEC - 1); continue; }   // (the host's clock turns it over)
        if (++sec < HALF_DAY_SEC) continue;
        sec = 0;
        StoryTimeStep(g, true);
        SaveAll(g);
        if (Online(g)) OnlineSendCoop(g);
    }
}

void UpdateWorld(Game& g, float dt) {
    {   // the region's own sounds (Hoenn: Emerald's), while you walk there
        int mi = g.ow.w[0].map;
        int r = mi >= 0 && mi < (int)gMaps.size() ? gMaps[mi].region : 0;
        SetSfxSet(r >= 0 && r < RegionCount() && RegionInfo(r).name == "Hoenn" ? "Hoenn" : "");
    }
    if (!WorldAvailable() || g.ow.w[0].map < 0) { g.ow.active = false; EnterStoryHub(g); return; }
    OwState& o = g.ow;
    for (int k = 0; k < Driven(g); k++) {   // a map with two pictures (the Mansion's barriers): the one its switch says
        int mi = o.w[k].map;
        if (mi < 0 || mi >= (int)gMaps.size()) continue;
        auto check = [&](OwMap& m) {
            if (m.altFlag.empty()) return;
            bool want = HasFlag(g, k, m.altFlag);
            if (want == m.altShown) return;
            m.altShown = want;
            if (m.tex.id) { UnloadTexture(m.tex); m.tex = {}; }
            if (m.over.id) { UnloadTexture(m.over); m.over = {}; }
            if (m.tex2.id) { UnloadTexture(m.tex2); m.tex2 = {}; }
        };
        check(gMaps[mi]);
        for (const auto& nb : gMaps[mi].near) check(gMaps[nb.map]);
    }
    if (Online(g)) {
        UpdateGhost(g, dt);
        if (!o.w[1].here) o.teamUp = false;
        if (o.watching) {   // watching your partner's battle
            bool stop = IsKeyPressed(KEY_ESCAPE) || MenuInput().back || !o.w[1].here;
#ifdef ARENA_AUTOTEST
            extern int gAutoMenu;
            if (gAutoMenu & 32) { gAutoMenu = 0; stop = true; }
#endif
            if (stop) { o.watching = false; o.watchBattle.reset(); OnlineSendWatch(false); PlaySfx(Sfx::MenuBack); }
            return;
        }
        TakeIncoming(g);
        if (o.waitFight) {   // waiting for the partner to come over
            if (g.on.myReady != 1) o.waitFight = false;
            else if (IsKeyPressed(KEY_ESCAPE) || MenuInput().back) { OnlineSetReady(g, 0); o.waitFight = false; Toast(g, "You stopped waiting."); }
            return;
        }
    }
    if (g.showBoard) {   // the leaderboard (from the menu): any key closes it
        PlayerInput in = MenuInput();
        if (in.confirm || in.back) { g.showBoard = false; PlaySfx(Sfx::MenuBack); }
        return;
    }
    if (gWorldHome && !o.hubOnly && !InWorldMode(g) && !AnySub(g)) { LeaveWorld(g); EnterStoryHub(g); return; }   // the story went on to another region
    WorldClock(g, dt);
    if (g.coop && !g.coopP[0].mons.empty() && !g.coopP[1].mons.empty() && SpeciesIndex(g.coopSave.rival[0]) < 0) EnsureRival(g, 0);   // Blue, once you both have Oak's Pokemon
    if (o.townMap) { UpdateTownMap(g); return; }
    if (o.menu) { UpdatePauseMenu(g); return; }
#ifdef ARENA_AUTOTEST
    extern int gAutoMenu;
    {   // autotests: "back" leaves the world for the Story menu, as before
        bool busy = false;
        for (int i = 0; i < Driven(g); i++) busy |= o.w[i].stepT < 1 || !o.w[i].msg.empty() || o.w[i].approach >= 0;
        busy |= AnySub(g);
        if ((gAutoMenu & 32) && !busy) { gAutoMenu = 0; SaveAll(g); o.active = false; o.hubOnly = true; if (Online(g)) OnlineSendWorldPos(false, -1, 0, 0, 0); EnterStoryHub(g); return; }
    }
#endif
    UpdateWanderers(g, dt);
    UpdateRustle(dt);
    for (int i = 0; i < Driven(g); i++) {
        if (o.sub[i]) { RunSub(g, i, dt); if (o.sub[i]) continue; }
        WalkIn in = ReadWalk(g, i);
        UpdateWalker(g, i, dt, in);
        if (g.screen != Screen::World) return;   // a story battle
    }
}

// ---------------------------------------------------------------- drawing

namespace {
struct View { float x, y; int vx, vw; };   // camera (map pixels at the view's top-left), the view's screen strip

Vector2 Scr(const View& c, float mx, float my) { return {c.vx + (mx - c.x) * Z, (my - c.y) * Z}; }

void DrawMapAt(const OwMap& m, const View& c, float ox, float oy) {
    Vector2 p = Scr(c, ox, oy + m.imgOffY);
    if (!m.tex.id) { p = Scr(c, ox, oy); DrawRectangle((int)p.x, (int)p.y, (int)(m.w * T * Z), (int)(m.h * T * Z), Color{40, 80, 50, 255}); return; }
    DrawTexturePro(m.tex, {0, 0, (float)m.tex.width, (float)m.tex.height}, {p.x, p.y, m.tex.width * Z, m.tex.height * Z}, {0, 0}, 0, WHITE);
}

// A person: feet at the bottom of the tile. frame 0 standing, 1/2 walking.
void DrawChar(const View& c, int sprite, int face, int frame, float mx, float my, float lift = 0, const Texture2D* sheet = nullptr) {
    const Texture2D& tex = sheet ? *sheet : gChars;
    if (!tex.id) return;
    static const int kCol[3] = {1, 0, 2};   // the sheet's columns: step, stand, step (frame 0 = standing still)
    int col = (face == 0 ? 0 : face == 1 ? 3 : 6) + kCol[frame < 0 || frame > 2 ? 0 : frame];
    // (32x32 cells: FireRed's 16-wide people in the middle, HeartGold's people as wide as they are)
    Rectangle src{(float)col * 32, (float)sprite * 32, face == 3 ? -32.0f : 32.0f, 32};
    Vector2 p = Scr(c, mx - 8, my + T - 32 - lift);
    DrawEllipse((int)(p.x + 16 * Z), (int)(Scr(c, mx, my + T - 1).y), 6 * Z, 2 * Z, Fade(BLACK, 0.25f));
    DrawTexturePro(tex, src, {p.x, p.y, 32 * Z, 32 * Z}, {0, 0}, 0, WHITE);
}
// The player on the Bicycle, surfing or fishing: a 32x32 cell of the actions sheet, feet on the tile.
void DrawAction(const View& c, int row, int cell, bool flip, float mx, float my, float lift = 0, const Texture2D* sheet = nullptr) {
    Rectangle src{(float)cell * 32, (float)row * 32, flip ? -32.0f : 32.0f, 32};
    Vector2 p = Scr(c, mx - 8, my + T - 32 - lift);
    DrawEllipse((int)(p.x + 16 * Z), (int)(Scr(c, mx, my + T - 1).y), 7 * Z, 2 * Z, Fade(BLACK, 0.25f));
    DrawTexturePro(sheet ? *sheet : gActions, src, {p.x, p.y, 32 * Z, 32 * Z}, {0, 0}, 0, WHITE);
}

// Tall grass in front of whoever stands in it (the bottom of the tile, over their feet).
void DrawGrassOver(const OwMap& m, const View& c, int tx, int ty) {
    if (m.At(tx, ty) != 'g' || !m.tex.id) return;
    float lift = m.Lift(tx, ty);
    int sx = tx, sy = ty;
    if (!m.hts.empty()) {
        // HeartGold's maps are rendered from 3D models: at a patch's edge the picture under a grass square can be the
        // path or the ground beside it, so the grass over your legs comes from a square in the middle of a patch
        if (m.grassSample == -2) {
            m.grassSample = -1;
            for (int y = 1; y + 1 < m.h && m.grassSample < 0; y++)
                for (int x = 1; x + 1 < m.w && m.grassSample < 0; x++) {
                    bool all = true;
                    for (int dy = -1; dy <= 1 && all; dy++) for (int dx = -1; dx <= 1 && all; dx++) all = m.At(x + dx, y + dy) == 'g' && m.Lift(x + dx, y + dy) == m.Lift(x, y);
                    if (all) m.grassSample = y * m.w + x;
                }
        }
        if (m.grassSample >= 0) { sx = m.grassSample % m.w; sy = m.grassSample / m.w; }
    }
    Rectangle src{(float)sx * T, (float)sy * T + 9 - m.imgOffY - m.Lift(sx, sy), (float)T, 7};
    Vector2 p = Scr(c, (float)tx * T, (float)ty * T + 9 - lift);
    DrawTexturePro(m.tex, src, {p.x, p.y, T * Z, 7 * Z}, {0, 0}, 0, WHITE);
}

void DrawFollower(const Game& g, int i, const View& c, const OwMap& m) {
    const OwWalker& w = g.ow.w[i];
    const Profile& p = WP(g, i);
    const OwnedMon* lead = nullptr;
    for (const auto& mm : p.mons) if (!mm.boxed && mm.hp > 0) { lead = &mm; break; }
    int idx = lead ? MonSpecies(*lead) : -1;
    if (idx < 0) return;
    float u = w.stepT;
    float fxp = (w.fFromX + (w.fx - w.fFromX) * u) * T, fyp = (w.fFromY + (w.fy - w.fFromY) * u) * T;
    fyp -= m.Lift(w.fFromX, w.fFromY) * (1 - u) + m.Lift(w.fx, w.fy) * u;
    int sp = ShinyIdx(idx, lead->shiny);
    int view = w.fFace;
    float h = CreatureHeight(sp, view);
    const CreatureSprites& cs = SpritesFor(sp);
    if (h > 0 && cs.view[view][0].w > 0) {
        float pixH = h / cs.pixel;                       // the sprite's own pixels
        float want = std::min(pixH, 26.0f) * Z;          // about a person's height at most
        float s = want / h;
        Vector2 feet = Scr(c, fxp + T / 2.0f, fyp + T - 2);
        DrawEllipse((int)feet.x, (int)feet.y, 6 * Z, 2 * Z, Fade(BLACK, 0.25f));
        int fr = (u < 1 && u > 0.25f) ? 1 : 0;
        DrawCreature(sp, view, fr, feet.x, feet.y, s, s, WHITE, 0);
    }
    DrawGrassOver(m, c, w.fx, w.fy);
}

// What changed on the map since you came in: cut trees and smashed rocks become ground, boulders sit where
// they were pushed (both drawn from the map's own picture), and the item balls still lying around.
// The living map: light glinting on the water, the waterfall pouring, grass shaking as you walk through.
void DrawMapLife(const Game& g, int map, const View& c) {
    const OwMap& m = gMaps[map];
    int x0 = std::max(0, (int)(c.x / T) - 1), y0 = std::max(0, (int)(c.y / T) - 1);
    int x1 = std::min(m.w - 1, x0 + (int)(c.vw / (T * Z)) + 3), y1 = std::min(m.h - 1, y0 + (int)(VH / (T * Z)) + 3);
    float t = (float)g.t;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            char ch = m.At(x, y);
            if (ch == 'w') {   // a glint drifts across now and then
                unsigned h = (unsigned)(x * 73856093u ^ y * 19349663u);
                float ph = std::fmod(t * 0.45f + (h % 1000) / 1000.0f, 1.0f);
                if (ph > 0.42f) continue;
                float a = std::sin(ph / 0.42f * PI);
                float gx = x * T + 3 + (h >> 4) % 8 + ph * 6, gy = y * T + 4 + (h >> 8) % 8;
                Vector2 p = Scr(c, gx, gy);
                DrawRectangle((int)p.x, (int)p.y, (int)(3 * Z), (int)Z, Fade(WHITE, 0.45f * a));
                DrawRectangle((int)(p.x + Z), (int)(p.y - Z), (int)Z, (int)Z, Fade(WHITE, 0.25f * a));
            } else if (ch == 'W') {   // the waterfall pours down
                for (int k = 0; k < 3; k++) {
                    float fy = std::fmod(t * 40.0f + k * 5.3f + x * 3.1f, (float)T);
                    Vector2 p = Scr(c, (float)x * T + 2 + k * 5, (float)y * T + fy);
                    DrawRectangle((int)p.x, (int)p.y, (int)Z, (int)(3 * Z), Fade(WHITE, 0.5f));
                }
            }
        }
    for (const Rustle& r : gRustle) {   // the blades part and spring back; a few bits fly up
        if (r.map != map) continue;
        float u = r.t / 0.35f;
        for (int k = 0; k < 3; k++) {
            float side = k == 0 ? -1.0f : k == 1 ? 1.0f : 0.2f;
            float bx = r.x * T + 8 + side * (2 + u * 5), by = r.y * T + 9 - std::sin(u * PI) * (4 + k);
            Vector2 p = Scr(c, bx, by);
            DrawRectangle((int)p.x, (int)p.y, (int)(2 * Z), (int)(2 * Z), Fade(Color{70, 160, 60, 255}, 1 - u));
        }
    }
}
void UpdateRustle(float dt) {
    for (auto& r : gRustle) r.t += dt;
    gRustle.erase(std::remove_if(gRustle.begin(), gRustle.end(), [](const Rustle& r) { return r.t > 0.35f; }), gRustle.end());
}

void DrawBerryTrees(const Game& g, int map, const View& c) {
    if (!gBerryTex.id) return;
    for (const auto& t : gMaps[map].berries) {
        if (!HasFlag(g, 0, t.ifF)) continue;
        TreeState s = TreeOf(g, t);
        int st = TreeStage(g, s), bi = BerryIndexOf(s.berry);
        if (st < 0 || bi < 0) continue;
        int anim = ((int)(g.t * 2) + t.id) & 1;
        int col = st == 0 ? 0 : 1 + (st - 1) * 2 + anim;
        Vector2 p = Scr(c, (float)t.x * T, (float)t.y * T - 16);
        DrawTexturePro(gBerryTex, {(float)col * 16, (float)bi * 32, 16, 32}, {p.x, p.y, 16 * Z, 32 * Z}, {0, 0}, 0, WHITE);
    }
}
Texture2D gGateTex[8] = {};   // the rotating gates' pictures (World/rgates/, pokeemerald's graphics/rotating_gates)
void DrawMapChanges(const Game& g, int map, const View& c) {
    const OwMap& m = gMaps[map];
    const MapRt& rt = gRt[map];
    if (!m.tex.id) return;
    if (m.tex2.id)   // squares from the second picture (a button pressed, a hole filled)
        for (const auto& sw : m.swaps)
            if (HasFlag(g, 0, sw.flag)) {
                Vector2 p = Scr(c, (float)sw.x * T, (float)sw.y * T);
                DrawTexturePro(m.tex2, {(float)sw.x * T, (float)sw.y * T - m.imgOffY, (float)T, (float)T}, {p.x, p.y, T * Z, T * Z}, {0, 0}, 0, WHITE);
            }
    if (!m.rgates.empty()) {   // the rotating gates, turned as they are now
        static const char* kGate[8] = {"l1", "l2", "l3", "l4", "t1", "t2", "t3", "t4"};
        auto it = gGateOri.find(map);
        for (int k = 0; k < (int)m.rgates.size(); k++) {
            const auto& gt = m.rgates[k];
            int s = std::max(0, std::min(7, gt.shape));
            if (!gGateTex[s].id) gGateTex[s] = LoadTex(DataDir() + "World/rgates/" + kGate[s] + ".png");
            if (!gGateTex[s].id) continue;
            int o = it != gGateOri.end() && k < (int)it->second.size() ? it->second[k] : (gt.orient & 3);
            Vector2 p = Scr(c, (float)gt.x * T, (float)gt.y * T);   // (they turn on the corner at the top left of their square)
            float wpx = (float)gGateTex[s].width;
            DrawTexturePro(gGateTex[s], {0, 0, wpx, (float)gGateTex[s].height}, {p.x, p.y, wpx * Z, gGateTex[s].height * Z}, {wpx * Z / 2, gGateTex[s].height * Z / 2}, 90.0f * o, WHITE);
        }
    }
    auto tileAt = [&](int k, int kx, int ky) {   // copy picture tile k to tile (kx, ky)
        Rectangle src{(float)(k % m.w) * T, (float)(k / m.w) * T, (float)T, (float)T};
        Vector2 p = Scr(c, (float)kx * T, (float)ky * T);
        DrawTexturePro(m.tex, src, {p.x, p.y, T * Z, T * Z}, {0, 0}, 0, WHITE);
    };
    auto groundNear = [&](int x, int y) {   // a plain ground tile nearby, to paint over what's gone
        for (int r = 1; r <= 3; r++)
            for (int d = 0; d < 4; d++) {
                int nx = x + DX[d] * r, ny = y + DY[d] * r;
                if (m.At(nx, ny) == '.') return ny * m.w + nx;
            }
        return -1;
    };
    if (m.objSprites && gFieldObj.id) {   // the picture is bare ground: draw what's still standing
        auto obj = [&](int kind, int k) {
            Vector2 p = Scr(c, (float)(k % m.w) * T, (float)(k / m.w) * T);
            DrawTexturePro(gFieldObj, {(float)kind * 16, 0, 16, 16}, {p.x, p.y, T * Z, T * Z}, {0, 0}, 0, WHITE);
        };
        for (int k = 0; k < (int)m.cells.size(); k++)
            if ((m.cells[k] == 'T' || m.cells[k] == 'R') && !rt.cleared.count(k)) obj(m.cells[k] == 'T' ? 0 : 1, k);
        for (int k : rt.boulders) obj(2, k);
        for (const auto& it : m.items)
            if (!it.hidden && ItemAt(g, 0, map, it.x, it.y, false) >= 0) obj(3, it.y * m.w + it.x);
        return;
    }
    auto clear = [&](int k) { int gk = groundNear(k % m.w, k / m.w); if (gk >= 0) tileAt(gk, k % m.w, k / m.w); };
    for (int k : rt.cleared) clear(k);
    int firstBoulder = -1;
    for (int k = 0; k < (int)m.cells.size(); k++) {
        if (m.cells[k] != 'B') continue;
        if (firstBoulder < 0) firstBoulder = k;
        if (std::find(rt.boulders.begin(), rt.boulders.end(), k) == rt.boulders.end()) clear(k);
    }
    for (int k : rt.boulders) if (m.cells[k] != 'B' && firstBoulder >= 0) tileAt(firstBoulder, k % m.w, k / m.w);
    for (const auto& it : m.items) {   // item balls
        if (it.hidden || ItemAt(g, 0, map, it.x, it.y, false) < 0) continue;
        Vector2 p = Scr(c, it.x * T + T / 2.0f, it.y * T + T / 2.0f + 2);
        DrawEllipse((int)p.x, (int)p.y + 9, 6, 2, Fade(BLACK, 0.25f));
        DrawPokeBall(p, 5.5f * Z / 2, 0, 0, Color{226, 52, 52, 255});
    }
}

// Your Surf Pokemon under you on the water (the first in the party that knows Surf).
void DrawSurfMon(const Game& g, int i, const View& c, float mx, float my) {
    const Profile& p = WP(g, i);
    int idx = -1;
    bool shiny = false;
    for (const auto& mm : p.mons)
        if (!mm.boxed && std::find(mm.moves.begin(), mm.moves.end(), "Surf") != mm.moves.end()) { idx = MonSpecies(mm); shiny = mm.shiny; break; }
    Vector2 feet = Scr(c, mx + T / 2.0f, my + T + 1);
    DrawEllipse((int)feet.x, (int)feet.y - 2, 9 * Z, 3 * Z, Fade(Color{220, 240, 255, 255}, 0.55f));   // the wake
    if (idx < 0) { DrawEllipse((int)feet.x, (int)feet.y - 6, 8 * Z, 4 * Z, Color{90, 120, 200, 255}); return; }
    int sp = ShinyIdx(idx, shiny);
    const OwWalker& w = g.ow.w[i];
    float h = CreatureHeight(sp, w.face);
    const CreatureSprites& cs = SpritesFor(sp);
    if (h > 0 && cs.view[w.face][0].w > 0) {
        float want = std::min(h / cs.pixel, 22.0f) * Z;
        float s = want / h;
        DrawCreature(sp, w.face, 0, feet.x, feet.y, s, s, WHITE, 0);
    }
}

// A Pokemon standing on the map (a legendary in its lair): its walking sprite, gently breathing.
// An Apricorn tree: a round little tree; its Apricorns hang on it until today's has been picked.
void DrawApricornTree(const Game& g, const View& c, const OwNpc& n, float mx, float my) {
    static const std::pair<const char*, Color> kCol[] = {{"Red", {230, 60, 50, 255}}, {"Yellow", {250, 210, 50, 255}}, {"Blue", {70, 120, 240, 255}},
                                                        {"Green", {90, 200, 80, 255}}, {"Pink", {250, 140, 190, 255}}, {"White", {245, 245, 245, 255}},
                                                        {"Black", {50, 50, 60, 255}}};
    Color fruit = WHITE;
    for (const auto& [k, col] : kCol) if (n.tree == k) fruit = col;
    Vector2 base = Scr(c, mx + T / 2.0f, my + T);
    float s = Z;
    DrawEllipse((int)base.x, (int)(base.y - 1 * s), 6 * s, 2 * s, Fade(BLACK, 0.25f));
    DrawRectangle((int)(base.x - 1.5f * s), (int)(base.y - 9 * s), (int)(3 * s), (int)(8 * s), Color{120, 80, 45, 255});
    DrawCircleV({base.x, base.y - 15 * s}, 9 * s, Color{40, 110, 50, 255});
    DrawCircleV({base.x - 2 * s, base.y - 17 * s}, 7 * s, Color{60, 150, 65, 255});
    DrawCircleV({base.x - 3.5f * s, base.y - 19 * s}, 2.5f * s, Fade(WHITE, 0.25f));
    if (n.args.empty() || !ApriPicked(g, n.args[0]))
        for (int k = 0; k < 3; k++) {
            Vector2 p{base.x + (k - 1) * 5.5f * s, base.y - (11 + (k & 1) * 6) * s};
            DrawCircleV(p, 2.4f * s, ColorBrightness(fruit, -0.35f));
            DrawCircleV(p, 1.8f * s, fruit);
        }
}
void DrawNpcMon(const Game& g, const View& c, int species, int face, float mx, float my) {
    Vector2 feet = Scr(c, mx + T / 2.0f, my + T);
    DrawEllipse((int)feet.x, (int)feet.y - 2, 10 * Z, 3 * Z, Fade(BLACK, 0.25f));
    float h = CreatureHeight(species, face);
    const CreatureSprites& cs = SpritesFor(species);
    if (h <= 0 || cs.view[face][0].w <= 0) return;
    float want = std::min(h / cs.pixel, 34.0f) * Z;
    float s = want / h;
    float breathe = 1.0f + 0.025f * std::sin((float)g.t * 2.2f);
    DrawCreature(species, face, ((int)(g.t * 2.0f)) & 1, feet.x, feet.y, s, s * breathe, WHITE, 0);
}

// The Bicycle: two wheels and a frame at your feet.
void DrawBike(const View& c, int face, float mx, float my, float spin) {
    Vector2 f = Scr(c, mx + T / 2.0f, my + T - 3);
    Color dark{40, 40, 50, 255}, red{220, 60, 60, 255};
    bool side = face >= 2;
    float dx = side ? 5 * Z : 0, dy = side ? 0 : 3 * Z;
    for (int k = -1; k <= 1; k += 2) {
        Vector2 wc{f.x + k * dx, f.y + k * dy};
        DrawCircleLinesV(wc, 3.2f * Z, dark);
        float a = spin + k;
        DrawLineEx({wc.x - std::cos(a) * 3 * Z, wc.y - std::sin(a) * 3 * Z}, {wc.x + std::cos(a) * 3 * Z, wc.y + std::sin(a) * 3 * Z}, 1, dark);
    }
    DrawLineEx({f.x - dx, f.y - dy}, {f.x + dx, f.y + dy}, 2, red);
}

void WrapText(const std::string& s, int w, std::vector<std::string>& out) {
    std::istringstream ss(s);
    std::string word, line;
    while (ss >> word) {
        std::string t = line.empty() ? word : line + " " + word;
        if (TextW(t.c_str(), 10) > w && !line.empty()) { out.push_back(line); line = word; }
        else line = t;
    }
    if (!line.empty()) out.push_back(line);
}

// Braille (the Regis' tombs): six dots a letter, raised or flat.
int BrailleDots(char ch) {   // bit k = dot k+1
    static const char* kDots[26] = {"1", "12", "14", "145", "15", "124", "1245", "125", "24", "245", "13", "123", "134", "1345", "135",
                                    "1234", "12345", "1235", "234", "2345", "136", "1236", "2456", "1346", "13456", "1356"};
    int b = 0;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
    const char* d = ch >= 'A' && ch <= 'Z' ? kDots[ch - 'A'] : ch == '.' ? "256" : ch == ',' ? "2" : "";
    for (const char* c = d; *c; c++) b |= 1 << (*c - '1');
    return b;
}
void DrawBraille(int cx, int y, const std::string& text) {   // the cells, centred on cx
    const int cw = 16, dot = 3;
    int n = (int)text.size(), x0 = cx - n * cw / 2;
    for (int k = 0; k < n; k++) {
        int b = BrailleDots(text[k]);
        for (int d = 0; d < 6; d++) {
            float px = x0 + k * cw + 4 + (d / 3) * 7, py = y + 3 + (d % 3) * 7;
            if (text[k] == ' ') continue;
            if (b >> d & 1) DrawCircle((int)px, (int)py, dot, Color{60, 50, 40, 255});
            else DrawCircle((int)px, (int)py, 1, Fade(Color{60, 50, 40, 255}, 0.35f));
        }
    }
}
void DrawBox(int bx, int by, int bw, int bh, const std::string& text, bool arrow);
void DrawBrailleBox(int bx, int by, int bw, int bh, const std::string& page) {   // "braille:TEXT": the stone, then what it says
    std::string t = page.substr(8);
    std::vector<std::string> lines;
    for (size_t a = 0, b; a < t.size(); a = b + 1) { b = t.find('~', a); if (b == std::string::npos) b = t.size(); lines.push_back(t.substr(a, b - a)); }
    int ph = 14 + 24 * (int)lines.size();
    DrawPanel(bx + bw / 2 - 150, by - ph - 8, 300, ph, Color{215, 200, 170, 245}, Color{120, 100, 70, 255});
    for (int k = 0; k < (int)lines.size(); k++) DrawBraille(bx + bw / 2, by - ph - 8 + 8 + k * 24, lines[k]);
    std::string read;
    for (const auto& l : lines) read += (read.empty() ? "" : " ") + l;
    DrawBox(bx, by, bw, bh, "It's written in braille. It reads: \"" + read + "\"", true);
}
void DrawBox(int bx, int by, int bw, int bh, const std::string& text, bool arrow) {
    DrawPanel(bx, by, bw, bh, Color{250, 250, 250, 245}, Color{60, 90, 140, 255});
    std::vector<std::string> lines;
    WrapText(text, bw - 24, lines);
    for (size_t i = 0; i < lines.size() && i < 4; i++) GDrawText(lines[i].c_str(), bx + 12, by + 10 + (int)i * 13, 10, Color{40, 40, 50, 255});
    if (arrow && std::fmod(GetTime(), 0.8) < 0.5) DrawTriangle({(float)bx + bw - 18, (float)by + bh - 14}, {(float)bx + bw - 10, (float)by + bh - 14}, {(float)bx + bw - 14, (float)by + bh - 9}, Color{220, 60, 60, 255});
}

// One walker's view of the world, in the screen strip [vx, vx + vw).
RenderTexture2D gSubRT[2];
bool gSubRTok[2] = {false, false};

// Morning, day, evening and night, from the clock: a soft colour over the outdoor maps.
float WorldHour(const Game& g) {   // the story clock as an hour: a day half runs 6:00-18:00, a night half 18:00-6:00
    int sec = g.coop ? g.coopSave.clockSec : g.profile.clockSec;
    float u = std::min(1.0f, sec / (float)HALF_DAY_SEC);
    return std::fmod((IsNight(StoryClock(g)) ? 18.0f : 6.0f) + 12 * u, 24.0f);
}
Color DayTint(float h) {
#ifdef ARENA_AUTOTEST
    if (const char* e = getenv("ARENA_HOUR")) h = (float)std::atof(e);
#endif
    struct Key { float h; Color c; };
    static const Key keys[] = {{0, {16, 24, 72, 110}}, {4.5f, {16, 24, 72, 110}}, {6.5f, {255, 170, 120, 40}}, {9, {255, 255, 255, 0}},
                               {17, {255, 255, 255, 0}}, {18.5f, {255, 130, 60, 50}}, {20.5f, {16, 24, 72, 110}}, {24, {16, 24, 72, 110}}};
    for (int k = 0; k + 1 < (int)(sizeof keys / sizeof keys[0]); k++) {
        if (h < keys[k].h || h > keys[k + 1].h) continue;
        float t = (h - keys[k].h) / std::max(0.01f, keys[k + 1].h - keys[k].h);
        const Color &a = keys[k].c, &b = keys[k + 1].c;
        auto mix = [&](unsigned char x, unsigned char y) { return (unsigned char)(x + (y - x) * t); };
        Color c{mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b), mix(a.a, b.a)};
        if (a.a == 0) { c.r = b.r; c.g = b.g; c.b = b.b; }
        if (b.a == 0) { c.r = a.r; c.g = a.g; c.b = a.b; }
        return c;
    }
    return {0, 0, 0, 0};
}
// Rain, snow, fog or a sandstorm drifting over the view.
void DrawMapWeather(const std::string& wx, int vx, int vw, float t) {   // (DrawView clips to the view)
    auto tile = [&](const Texture2D& tx, float sx, float sy, float alpha, float h) {   // a 64-pixel weather layer, scrolling
        float w = 64 * Z, hh = h * Z;
        float ox = std::fmod(sx, w), oy = std::fmod(sy, hh);
        if (ox > 0) ox -= w;
        if (oy > 0) oy -= hh;
        for (float y = oy; y < VH; y += hh)
            for (float x = ox; x < vw; x += w)
                DrawTexturePro(tx, {0, 0, 64, h}, {vx + x, y, w, hh}, {0, 0}, 0, Fade(WHITE, alpha));
    };
    if (wx == "fog" && gWxFog.id) {   // FireRed's fog: two layers drifting at different speeds
        tile(gWxFog, t * 10, 0, 0.40f, 64);
        tile(gWxFog, -t * 6 + 32 * Z, 20 * Z, 0.25f, 64);
        return;
    }
    if (wx == "sand" && gWxSand.id) {
        DrawRectangle(vx, 0, vw, VH, Fade(Color{200, 160, 90, 255}, 0.18f));
        tile(gWxSand, t * 260, std::sin(t * 2) * 6, 0.9f, 64);
        tile(gWxSand, t * 180 + 20 * Z, 32 * Z, 0.6f, 64);
        return;
    }
    if (wx == "rain" && gWxRain.id) {   // streaks falling down-left; each one ends in a little splash
        DrawRectangle(vx, 0, vw, VH, Fade(Color{40, 50, 90, 255}, 0.16f));
        for (int k = 0; k < 70; k++) {
            float sx = (float)((k * 7919) % 1000) / 1000.0f, sy = (float)((k * 104729) % 1000) / 1000.0f;
            float period = 0.8f + sx * 0.4f, ph = std::fmod(t / period + sy, 1.0f);
            float x0 = sx * (vw + 120), y0 = sy * VH * 0.6f;
            if (ph < 0.8f) {   // falling
                float u = ph / 0.8f;
                float x = x0 - u * 120, y = y0 + u * VH * 0.55f - 40;
                DrawTexturePro(gWxRain, {0, (float)(k % 2) * 32, 16, 32}, {vx + x, y, 16 * Z, 32 * Z}, {0, 0}, 0, Fade(WHITE, 0.8f));
            } else {           // the splash
                int f = 2 + std::min(3, (int)((ph - 0.8f) / 0.05f));
                float x = x0 - 120, y = y0 + VH * 0.55f - 40;
                DrawTexturePro(gWxRain, {0, (float)f * 32, 16, 32}, {vx + x, y, 16 * Z, 32 * Z}, {0, 0}, 0, Fade(WHITE, 0.8f));
            }
        }
        return;
    }
    if (wx == "ash") {   // Mt. Chimney's ash on Route 113: grey flakes drifting down slowly, the air hazy
        DrawRectangle(vx, 0, vw, VH, Fade(Color{120, 115, 110, 255}, 0.16f));
        for (int k = 0; k < 90; k++) {
            float sx = (float)((k * 7919) % 1000) / 1000.0f, sy = (float)((k * 104729) % 1000) / 1000.0f;
            float y = std::fmod(sy * VH + t * (22 + (k % 4) * 5), (float)VH + 8) - 8;
            float x = std::fmod(sx * vw + std::sin(t * 0.8f + k) * 10 + t * 12 + vw * 4, (float)vw);
            float r = (1.2f + (k % 3) * 0.6f) * Z * 0.5f;
            DrawRectangle(vx + (int)x, (int)y, (int)std::max(2.0f, r * 2), (int)std::max(2.0f, r * 2), Fade(Color{90, 88, 86, 255}, 0.75f));
        }
        return;
    }
    if (wx == "shade") {   // Petalburg Woods, the Abandoned Ship: the light only gets in a little
        DrawRectangle(vx, 0, vw, VH, Fade(Color{10, 20, 30, 255}, 0.22f));
        return;
    }
    if (wx == "bubbles") {   // under the sea: blue water, bubbles rising, light rippling down
        DrawRectangle(vx, 0, vw, VH, Fade(Color{20, 60, 140, 255}, 0.22f));
        for (int k = 0; k < 4; k++) {
            float x = std::fmod(t * (10 + k * 4) + k * 173.0f, (float)vw + 200) - 100;
            DrawRectangleGradientH(vx + (int)x - 60, 0, 60, VH, Fade(WHITE, 0.0f), Fade(WHITE, 0.05f));
            DrawRectangleGradientH(vx + (int)x, 0, 60, VH, Fade(WHITE, 0.05f), Fade(WHITE, 0.0f));
        }
        for (int k = 0; k < 40; k++) {
            float sx = (float)((k * 7919) % 1000) / 1000.0f, sy = (float)((k * 104729) % 1000) / 1000.0f;
            float y = VH - std::fmod(sy * VH + t * (30 + (k % 5) * 8), (float)VH + 10);
            float x = sx * vw + std::sin(t * 2 + k) * 4;
            DrawCircleLines(vx + (int)x, (int)y, 1.5f * Z + (k % 3), Fade(Color{200, 230, 255, 255}, 0.6f));
        }
        return;
    }
    if (wx == "snow" && gWxSnow[0].id) {   // flakes drifting down, swaying
        for (int k = 0; k < 60; k++) {
            float sx = (float)((k * 7919) % 1000) / 1000.0f, sy = (float)((k * 104729) % 1000) / 1000.0f;
            float y = std::fmod(sy * VH + t * (28 + (k % 5) * 6), (float)VH + 16) - 16;
            float x = std::fmod(sx * vw + std::sin(t * 1.1f + k) * 14 + vw, (float)vw);
            const Texture2D& fl = gWxSnow[k % 2];
            DrawTexturePro(fl, {0, 0, 8, 8}, {vx + x, y, 8 * Z, 8 * Z}, {0, 0}, 0, Fade(WHITE, 0.9f));
        }
        return;
    }
    if (wx == "fog") {
        for (int k = 0; k < 3; k++) {
            float x = std::fmod(t * (8 + k * 5), (float)vw);
            DrawRectangleGradientH(vx + (int)x - vw, 0, vw, VH, Fade(WHITE, 0.0f), Fade(WHITE, 0.18f));
            DrawRectangleGradientH(vx + (int)x, 0, vw, VH, Fade(WHITE, 0.18f), Fade(WHITE, 0.0f));
        }
    } else {
        int n = wx == "rain" ? 140 : wx == "sand" ? 160 : 90;
        for (int k = 0; k < n; k++) {
            float sx = (float)((k * 7919) % 1000) / 1000.0f, sy = (float)((k * 104729) % 1000) / 1000.0f;
            if (wx == "rain") {
                float y = std::fmod(sy * VH + t * 520, (float)VH), x = std::fmod(sx * vw - t * 90 + vw * 4, (float)vw);
                DrawLineEx({vx + x, y}, {vx + x - 4, y + 14}, 1.5f, Fade(Color{190, 210, 255, 255}, 0.55f));
            } else if (wx == "snow") {
                float y = std::fmod(sy * VH + t * 45, (float)VH), x = std::fmod(sx * vw + std::sin(t * 1.3f + k) * 12 + vw, (float)vw);
                DrawCircleV({vx + x, y}, 1.5f + (k % 3) * 0.6f, Fade(WHITE, 0.85f));
            } else {
                float x = std::fmod(sx * vw + t * 300, (float)vw), y = std::fmod(sy * VH + std::sin(t + k) * 8 + VH, (float)VH);
                DrawRectangle(vx + (int)x, (int)y, 3, 1, Fade(Color{220, 190, 130, 255}, 0.7f));
            }
        }
        if (wx == "rain") DrawRectangle(vx, 0, vw, VH, Fade(Color{40, 50, 90, 255}, 0.18f));
        if (wx == "sand") DrawRectangle(vx, 0, vw, VH, Fade(Color{200, 160, 90, 255}, 0.2f));
    }
}

// ---- the quest marker: which way to the next story battle (an arrow by you; a "!" over them once they're in view)
bool QuestTarget(const Game& g, int i, int& tm, int& tx, int& ty) {
    auto at = [&](const char* map, int x, int y) { tm = MapIndex(map); tx = x; ty = y; return tm >= 0; };
    if (WP(g, i).mons.empty()) return at("Oak's Lab", 9, 5);   // Oak's Pokemon on the table
    if (BagCount(WP(g, i), "Oak's Parcel") > 0) return at("Oak's Lab", 6, 3);
    bool lab = StageBeaten(g, 0);
    if (lab && !Flags(g).count("parcel_done") && !Flags(g).count("got_parcel")) return at("Viridian City Poke Mart", 2, 3);   // the Mart has Oak's order
    for (int s = 0; s < (int)Story().stages.size(); s++) {
        if (!StageWalked(s) || StageBeaten(g, s)) continue;
        for (int pass = 0; pass < 2; pass++)   // standing there now; else where they'll be
            for (int k = 0; k < (int)gMaps.size(); k++)
                for (const auto& n : gMaps[k].npcs)
                    if (TrainerStage(n) == s && (pass == 1 || NpcHere(g, n))) { tm = k; tx = n.x; ty = n.y; return true; }
    }
    return false;
}
// The first step toward map `to` from map `from` (BFS over doors and the roads between maps): the tile to head for.
bool QuestStepRaw(int from, int to, int px, int py, int& ex, int& ey);
bool QuestStep(int from, int to, int px, int py, int& ex, int& ey) {   // (remembered while you stand still)
    static int k[4] = {-1, -1, -1, -1}, rx = 0, ry = 0;
    static bool ok = false;
    if (k[0] != from || k[1] != to || k[2] != px || k[3] != py) { k[0] = from; k[1] = to; k[2] = px; k[3] = py; ok = QuestStepRaw(from, to, px, py, rx, ry); }
    ex = rx; ey = ry;
    return ok;
}
bool QuestStepRaw(int from, int to, int px, int py, int& ex, int& ey) {
    if (from == to) return false;
    int n = (int)gMaps.size();
    std::vector<int> prev(n, -2);
    std::vector<std::array<int, 2>> exitOf(n, {0, 0});   // (for the first hop) where you leave `from`
    std::vector<int> q{from};
    prev[from] = -1;
    for (size_t h = 0; h < q.size(); h++) {
        int cur = q[h];
        const OwMap& m = gMaps[cur];
        auto push = [&](int nx, int x, int y) {
            if (nx < 0 || nx >= n || prev[nx] != -2) return;
            prev[nx] = cur;
            exitOf[nx] = cur == from ? std::array<int, 2>{x, y} : exitOf[cur];
            q.push_back(nx);
        };
        for (const auto& d : m.doors) {
            auto a = Split(d.act, '|');
            if (a.size() > 1 && a[0] == "warp") push(MapIndex(a[1]), d.x, d.y);
        }
        for (int d = 0; d < 4; d++) {
            const OwLink& L = m.link[d];
            if (L.map < 0) continue;
            int x = d == 2 ? -1 : d == 3 ? m.w : std::max(0, std::min(m.w - 1, px));
            int y = d == 1 ? -1 : d == 0 ? m.h : std::max(0, std::min(m.h - 1, py));
            push(L.map, x, y);
        }
        for (const auto& nb : m.near) {   // placed maps: out the side toward it
            const OwMap& n = gMaps[nb.map];
            int x = nb.dx >= m.w ? m.w : nb.dx + n.w <= 0 ? -1 : std::max(0, std::min(m.w - 1, std::max(nb.dx, std::min(nb.dx + n.w - 1, px))));
            int y = nb.dy >= m.h ? m.h : nb.dy + n.h <= 0 ? -1 : std::max(0, std::min(m.h - 1, std::max(nb.dy, std::min(nb.dy + n.h - 1, py))));
            push(nb.map, x, y);
        }
        if (prev[to] != -2) { ex = exitOf[to][0]; ey = exitOf[to][1]; return true; }
    }
    return false;
}
} // namespace
std::string WorldQuestInfo(const Game& g) {   // (autotests) where the quest marker points
    int tm, tx, ty, ex = -1, ey = -1;
    if (!QuestTarget(g, 0, tm, tx, ty)) return "none";
    bool step = QuestStep(g.ow.w[0].map, tm, g.ow.w[0].x, g.ow.w[0].y, ex, ey);
    return gMaps[tm].name + " " + std::to_string(tx) + "," + std::to_string(ty) + (step ? " via " + std::to_string(ex) + "," + std::to_string(ey) : "");
}
namespace {
void DrawQuestMarker(const Game& g, int me, const View& c, float px, float py) {
    if (!Cfg().questMarker || !InWorldMode(g)) return;
    const OwWalker& w = g.ow.w[me];
    if (!w.msg.empty() || w.map < 0) return;
    int tm, tx, ty;
    if (!QuestTarget(g, me, tm, tx, ty)) return;
    float bob = std::sin(g.t * 5) * 2;
    if (tm == w.map) {
        Vector2 t = Scr(c, tx * T + T / 2.0f, ty * T - 12 + bob - gMaps[tm].Lift(tx, ty));
        bool onScreen = t.x > c.vx && t.x < c.vx + c.vw && t.y > 0 && t.y < VH;
        if (onScreen && (std::abs(tx - w.x) > 0 || std::abs(ty - w.y) > 1)) {   // a "!" over them
            Color ink{40, 30, 30, 255}, gold{255, 210, 60, 255};
            DrawRectangleRounded({t.x - 5, t.y - 24, 10, 17}, 0.4f, 4, ink);
            DrawRectangleRounded({t.x - 3.5f, t.y - 22.5f, 7, 14}, 0.4f, 4, gold);
            DrawRectangleRounded({t.x - 5, t.y - 5, 10, 9}, 0.4f, 4, ink);
            DrawRectangleRounded({t.x - 3.5f, t.y - 3.5f, 7, 6}, 0.4f, 4, gold);
            return;
        }
        if (onScreen) return;   // right beside them
    } else if (!QuestStep(w.map, tm, w.x, w.y, tx, ty)) return;
    float dx = tx * T + T / 2.0f - (px + T / 2.0f), dy = ty * T - MapOf(w).Lift(tx, ty) + T / 2.0f - (py + T / 2.0f);
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1) return;
    dx /= len; dy /= len;
    Vector2 ctr = Scr(c, px + T / 2.0f, py + T / 2.0f - 6);
    float r = 26 + bob;   // a gold arrow circling you, pointing the way
    auto tri = [&](float len, float wid, float back, Color col) {
        Vector2 tip{ctr.x + dx * (r + len), ctr.y + dy * (r + len)};
        Vector2 l{ctr.x + dx * (r - back) - dy * wid, ctr.y + dy * (r - back) + dx * wid};
        Vector2 rr{ctr.x + dx * (r - back) + dy * wid, ctr.y + dy * (r - back) - dx * wid};
        DrawTriangle(tip, rr, l, col);
        DrawTriangle(tip, l, rr, col);
    };
    tri(14, 10, 2, Fade(Color{40, 30, 30, 255}, 0.85f));
    tri(11, 7, 0, Color{255, 210, 60, 255});
}

void DrawView(const Game& g, int me, int vx, int vw) {
    const OwState& o = g.ow;
    const OwWalker& w = o.w[me];
    if (w.map < 0) return;
    TouchMap(w.map);
    for (int d = 0; d < 4; d++) TouchMap(gMaps[w.map].link[d].map);
    for (const auto& nb : gMaps[w.map].near) TouchMap(nb.map);
    const OwMap& m = MapOf(w);
    BeginScissorMode(vx * CANVAS_ZOOM, 0, vw * CANVAS_ZOOM, VH * CANVAS_ZOOM);
    DrawRectangle(vx, 0, vw, VH, BLACK);
    if (o.sub[me] && gSubRTok[me]) {   // their own battle / Healing Center / Mart, fitted into the strip
        const Texture2D& t = gSubRT[me].texture;
        float h = (float)vw * VH / VW;
        DrawTexturePro(t, {0, 0, (float)t.width, -(float)t.height}, {(float)vx, (VH - h) / 2, (float)vw, h}, {0, 0}, 0, WHITE);
        if (vw < VW) {
            const std::string& nm = WP(g, me).name;
            GDrawText(nm.c_str(), vx + 6, (int)((VH - h) / 2) - 14, 10, WhoOf(g, me) == 0 ? UI_GOLD : Color{255, 150, 230, 255});
        }
        EndScissorMode();
        return;
    }
    float u = w.stepT;
    float px = (w.fromX + (w.x - w.fromX) * u) * T, py = (w.fromY + (w.y - w.fromY) * u) * T;
    auto liftOf = [&](int fx, int fy, int x, int y, float t) { return m.Lift(fx, fy) * (1 - t) + m.Lift(x, y) * t; };
    float myLift = liftOf(w.fromX, w.fromY, w.x, w.y, u);
    float viewW = vw / Z, viewH = VH / Z;
    View c{px + T / 2.0f - viewW / 2, py - myLift + T / 2.0f - viewH / 2, vx, vw};
    // keep the view on the map where nothing joins it (no black past the trees); small rooms
    // sit in the middle
    auto clampAxis = [](float& v, float size, float view, bool lowOpen, bool highOpen) {
        if (size <= view) { v = (size - view) / 2; return; }
        if (!lowOpen) v = std::max(v, 0.0f);
        if (!highOpen) v = std::min(v, size - view);
    };
    bool open = !m.indoor;
    bool side[4] = {m.link[0].map >= 0, m.link[1].map >= 0, m.link[2].map >= 0, m.link[3].map >= 0};
    for (const auto& nb : m.near) {
        const OwMap& n = gMaps[nb.map];
        side[0] |= nb.dy + n.h > m.h; side[1] |= nb.dy < 0; side[2] |= nb.dx < 0; side[3] |= nb.dx + n.w > m.w;
    }
    clampAxis(c.x, (float)m.w * T, viewW, open && side[2], open && side[3]);
    clampAxis(c.y, (float)m.h * T, viewH, open && side[1], open && side[0]);
    // the camera moves in whole screen pixels (the canvas is CANVAS_ZOOM x the layout): smooth, without
    // the map and the walkers drifting a pixel apart
    c.x = std::round(c.x * Z * CANVAS_ZOOM) / (Z * CANVAS_ZOOM);
    c.y = std::round(c.y * Z * CANVAS_ZOOM) / (Z * CANVAS_ZOOM);
    DrawMapAt(m, c, 0, 0);
    DrawMapChanges(g, w.map, c);
    DrawBerryTrees(g, w.map, c);
    DrawMapLife(g, w.map, c);
    if (!m.indoor)   // the maps next door, so the road goes on
        for (int d = 0; d < 4; d++) {
            const OwLink& L = m.link[d];
            if (L.map < 0) continue;
            const OwMap& n = gMaps[L.map];
            float ox, oy;
            if (d == 1) { ox = -L.off * T; oy = -n.h * T; }
            else if (d == 0) { ox = -L.off * T; oy = m.h * T; }
            else if (d == 2) { ox = -n.w * T; oy = -L.off * T; }
            else { ox = m.w * T; oy = -L.off * T; }
            DrawMapAt(n, c, ox, oy);
        }
    if (!m.indoor)
        for (const auto& nb : m.near) DrawMapAt(gMaps[nb.map], c, nb.dx * T, nb.dy * T);

    // everyone on this map, back to front
    struct Who { float y; int kind; int i; };   // kind 0 a person, 1 a walker's Pokemon, 2 a walker
    std::vector<Who> draw;
    for (size_t k = 0; k < m.npcs.size(); k++) if (NpcHere(g, m.npcs[k])) draw.push_back({(float)gNpc[w.map][k].y * T, 0, (int)k});
    for (int k = 0; k < 2; k++) {
        if (!Shown(g, k) || o.w[k].map != w.map) continue;
        const OwWalker& v = o.w[k];
        float uy = (v.fromY + (v.y - v.fromY) * v.stepT) * T;
        float fy = (v.fFromY + (v.fy - v.fFromY) * v.stepT) * T;
        bool follower = !(v.fx == v.x && v.fy == v.y && v.fFromX == v.fromX && v.fFromY == v.fromY) && !v.surfing && !v.biking;
        if (follower) draw.push_back({fy - 0.5f, 1, k});
        draw.push_back({uy, 2, k});
    }
    std::sort(draw.begin(), draw.end(), [](const Who& a, const Who& b) { return a.y < b.y; });
    for (const Who& d : draw) {
        if (d.kind == 0) {
            const OwNpc& n = m.npcs[d.i];
            const NpcRt& r = gNpc[w.map][d.i];
            int frame = 0;
            float nx = (float)r.x * T, ny = (float)r.y * T;
            if (r.st < 1) {   // a wanderer mid-step
                nx = (r.fromX + (r.x - r.fromX) * r.st) * T;
                ny = (r.fromY + (r.y - r.fromY) * r.st) * T;
                frame = r.st > 0.2f && r.st < 0.8f ? 1 + ((r.fromX + r.fromY) & 1) : 0;
                ny -= liftOf(r.fromX, r.fromY, r.x, r.y, r.st);
            } else ny -= m.Lift(r.x, r.y);
            const OwWalker* who = nullptr;
            for (int k = 0; k < Driven(g); k++) if (o.w[k].map == w.map && o.w[k].approach == d.i) who = &o.w[k];
            if (who && who->approachT >= 0) {   // mid-step toward them
                float t = who->approachT / WALK;
                nx += DX[r.face] * T * t;
                ny += DY[r.face] * T * t;
                frame = t > 0.2f && t < 0.8f ? 1 + ((r.x + r.y) & 1) : 0;
            }
            if (n.mon == -2) { RaidInfo ri = RaidOfDay(RaidToday()); if (ri.species >= 0) DrawNpcMon(g, c, ri.species, r.face, nx, ny); }
            else if (n.mon == -3) DrawApricornTree(g, c, n, nx, ny);
            else if (n.mon >= 0) DrawNpcMon(g, c, n.mon, r.face, nx, ny);
            else DrawChar(c, n.sprite, r.face, frame, nx, ny);
            DrawGrassOver(m, c, r.x, r.y);
            if (who && who->approachT < 0) {   // "!"
                Vector2 p = Scr(c, nx + 4, ny - 28);
                DrawPanel((int)p.x, (int)p.y, 16, 18, WHITE, UI_INK);
                DrawTextCentered("!", (int)p.x + 8, (int)p.y + 2, 14, Color{220, 40, 40, 255}, false);
            }
        } else if (d.kind == 1) {
            DrawFollower(g, d.i, c, m);
        } else {
            const OwWalker& v = o.w[d.i];
            float vu = v.stepT;
            float vx2 = (v.fromX + (v.x - v.fromX) * vu) * T, vy2 = (v.fromY + (v.y - v.fromY) * vu) * T;
            vy2 -= liftOf(v.fromX, v.fromY, v.x, v.y, vu);
            float lift = v.hop ? std::sin(vu * PI) * 10 : 0;
            int frame = (vu < 1 && vu > 0.15f && vu < 0.85f) ? 1 + v.walkAnim : 0;
            if (v.hop) { Vector2 sh = Scr(c, vx2 + 8, vy2 + T - 1); DrawEllipse((int)sh.x, (int)sh.y, 6 * Z, 2 * Z, Fade(BLACK, 0.3f)); }
            if (v.surfing) {   // riding your Surf Pokemon: it bobs on the water under you
                float bob = std::sin(g.t * 4.0f) * 1.2f;
                DrawSurfMon(g, d.i, c, vx2, vy2 + bob);
                lift += 6 + bob;
                frame = 0;
            }
            int girl = WhoOf(g, d.i) == 1 ? 1 : 0;
            // your own look (the trainer creator): your walking sheet, and Red's or Leaf's poses in your colours
            TrainerLook look = ParseLook(WP(g, d.i).look);
            Texture2D lookChars{}, lookActs{};
            if (!look.Empty()) { girl = look.type; lookChars = LookChars(look); lookActs = LookActions(look); }
            const Texture2D* cs = lookChars.id ? &lookChars : nullptr;
            const Texture2D* as = lookActs.id ? &lookActs : nullptr;
            int arow = as ? 0 : girl;   // a look's own action sheet has one row each: bike, surf, fish
            static const int kCol[3] = {1, 0, 2};
            int dirCol = v.face == 0 ? 0 : v.face == 1 ? 3 : 6;
            if (gActions.id && v.biking) DrawAction(c, arow, dirCol + kCol[frame < 0 || frame > 2 ? 0 : frame], v.face == 3, vx2, vy2, lift, as);
            else if (gActions.id && v.surfing) DrawAction(c, as ? 1 : 2 + girl, v.face == 0 ? 0 : v.face == 1 ? 1 : 2, v.face == 3, vx2, vy2, lift - 6, as);
            else if (gActions.id && v.fishing) {   // the rod comes out, then stays out
                int k = std::min(3, (int)(v.fishT / 0.07f));
                int base = v.face == 0 ? 8 : v.face == 1 ? 4 : 0;
                DrawAction(c, as ? 2 : 4 + girl, base + k, v.face == 3, vx2, vy2, lift, as);
            } else {
                if (v.biking) DrawBike(c, v.face, vx2, vy2, vu < 1 ? g.t * 18 : 0);
                DrawChar(c, cs ? 0 : WalkerSprite(g, d.i), v.face, frame, vx2, vy2, lift, cs);
            }
            if (vu >= 1) DrawGrassOver(m, c, v.x, v.y);
            if (g.coop && d.i != me) {   // your partner's name over their head (and a sword when they're battling)
                const std::string& nm = WP(g, d.i).name;
                Vector2 p = Scr(c, vx2 + 8, vy2 - 20);
                DrawTextCentered(nm.c_str(), (int)p.x, (int)p.y, 10, d.i == 0 ? UI_GOLD : Color{255, 150, 230, 255});
                bool battling = Online(g) ? PartnerBattling(g) : (bool)o.sub[d.i];
                if (battling) {
                    float b = std::sin(g.t * 5) * 1.5f;
                    DrawPanel((int)p.x - 26, (int)(p.y - 16 + b), 52, 13, Fade(UI_PANEL, 0.92f), Color{255, 150, 230, 255});
                    DrawTextCentered(Online(g) ? "BATTLING" : "BUSY", (int)p.x, (int)(p.y - 15 + b), 10, UI_GOLD, false);
                }
            }
        }
    }

    {   // what stands in front of people: tree tops, roofs, the tops of signs (this map and the ones next door)
        auto over = [&](const OwMap& n, float ox, float oy) {
            if (!n.over.id) return;
            Vector2 p = Scr(c, ox, oy + n.imgOffY);
            DrawTexturePro(n.over, {0, 0, (float)n.over.width, (float)n.over.height}, {p.x, p.y, n.over.width * Z, n.over.height * Z}, {0, 0}, 0, WHITE);
        };
        over(m, 0, 0);
        if (!m.indoor)
            for (int d = 0; d < 4; d++) {
                const OwLink& L = m.link[d];
                if (L.map < 0) continue;
                const OwMap& n = gMaps[L.map];
                if (d == 1) over(n, -L.off * T, -n.h * T);
                else if (d == 0) over(n, -L.off * T, m.h * T);
                else if (d == 2) over(n, -n.w * T, -L.off * T);
                else over(n, m.w * T, -L.off * T);
            }
        if (!m.indoor)
            for (const auto& nb : m.near) over(gMaps[nb.map], nb.dx * T, nb.dy * T);
    }
    if (!m.indoor && !m.dark) {   // outdoors: the season, the time of day (the story clock) and the day's weather
        static const Color kSeason[4] = {{0, 0, 0, 0}, {255, 236, 190, 16}, {230, 140, 60, 26}, {205, 225, 255, 40}};
        Color sc = kSeason[SeasonOf(StoryClock(g)) & 3];
        if (sc.a) DrawRectangle(vx, 0, vw, VH, sc);
        Color tint = DayTint(WorldHour(g));
        if (tint.a) DrawRectangle(vx, 0, vw, VH, tint);
        std::string wx = WorldWeatherAt(g, m, w.x, w.y);
        if (wx == "sun") { if (!IsNight(StoryClock(g))) DrawRectangle(vx, 0, vw, VH, Color{255, 230, 150, 22}); }
        else if (!wx.empty()) DrawMapWeather(wx, vx, vw, g.t);
    }
    if (me == 0 || Driven(g) == 2) DrawQuestMarker(g, me, c, px, py - myLift);
    if (m.dark && w.flashMap != w.map) {   // a dark cave: only a little circle of light around you
        Vector2 ctr = Scr(c, px + T / 2.0f, py - myLift + T / 2.0f);
        float r = 26 * Z, R = (float)VW + VH;
        DrawRing(ctr, r, R, 0, 360, 72, BLACK);
        DrawRing(ctr, r - 6 * Z, r, 0, 360, 72, Fade(BLACK, 0.55f));
    }
    // the place name, the message box
    if (w.bannerT > 0) {
        float a = std::min(1.0f, w.bannerT / 0.4f);
        int bw = std::max(120, TextW(m.name.c_str(), 10) + 24);
        DrawPanel(vx + 8, 8 - (int)((1 - a) * 30), bw, 20, Fade(UI_PANEL, a), Fade(UI_BORDER, a));
        GDrawText(m.name.c_str(), vx + 20, 13 - (int)((1 - a) * 30), 10, Fade(UI_TEXT, a));
    }
    if (!w.msg.empty()) {
        bool asking = w.msgPage >= (int)w.msg.size() - 1 && !w.choices.empty();
        const std::string& page = w.msg[std::min(w.msgPage, (int)w.msg.size() - 1)];
        if (page.rfind("braille:", 0) == 0) DrawBrailleBox(vx + 12, VH - 76, vw - 24, 64, page);
        else DrawBox(vx + 12, VH - 76, vw - 24, 64, page, !asking);
        if (asking) {   // the question's answers, over the right of the box
            int bw = 40;
            for (const auto& ch : w.choices) bw = std::max(bw, TextW(ch.first.c_str(), 10) + 30);
            int bh = 10 + 14 * (int)w.choices.size(), bx = vx + vw - 14 - bw, by = VH - 80 - bh;
            DrawPanel(bx, by, bw, bh, Color{250, 250, 250, 245}, Color{60, 90, 140, 255});
            for (int k = 0; k < (int)w.choices.size(); k++) {
                int y = by + 6 + k * 14;
                if (k == w.choiceSel) DrawTriangle({(float)bx + 8, (float)y + 2}, {(float)bx + 8, (float)y + 10}, {(float)bx + 14, (float)y + 6}, Color{40, 40, 50, 255});
                GDrawText(w.choices[k].first.c_str(), bx + 18, y + 1, 10, Color{40, 40, 50, 255});
            }
        }
    }
    else if (me == 0 && o.waitFight) DrawBox(vx + 12, VH - 76, vw - 24, 64, "Waiting for " + g.coopP[1 - g.on.me].name + " to come and battle " + o.waitName + " with you...   (Esc: stop waiting)", false);
    if (w.fadeT > 0) DrawRectangle(vx, 0, vw, VH, Fade(BLACK, w.fadeT / 0.35f));
    EndScissorMode();
}
} // namespace

namespace {
void DrawTeamTag(int right) {   // "TEAMED UP" in the top corner
    const char* s = "TEAMED UP - 2v2 wild battles";
    int w = TextW(s, 10) + 16;
    DrawPanel(right - w, 6, w, 16, Fade(UI_PANEL, 0.92f), Color{140, 230, 150, 255});
    GDrawText(s, right - w + 8, 9, 10, Color{140, 230, 150, 255});
}
} // namespace

void WorldPreDraw(const Game& g) {
    if (g.screen != Screen::World) return;
    for (int i = 0; i < 2; i++) {
        const std::shared_ptr<Game>& s = g.ow.sub[i];
        if (!s || s->screen == Screen::World) continue;
        if (s->screen == Screen::Battle) s->battle.PrepareHud();
        if (!gSubRTok[i]) {
            gSubRT[i] = LoadRenderTexture(VW * CANVAS_ZOOM, VH * CANVAS_ZOOM);
            SetTextureFilter(gSubRT[i].texture, TEXTURE_FILTER_BILINEAR);
            gSubRTok[i] = true;
        }
        BeginTextureMode(gSubRT[i]);
        ClearBackground(BLACK);
        Camera2D cam{};
        cam.zoom = (float)CANVAS_ZOOM;
        BeginMode2D(cam);
        float keepUi = gUiScreenT;
        gUiScreenT = s->screenT;
        DrawScreenOf(*s);
        DrawToast(*s);
        gUiScreenT = keepUi;
        EndMode2D();
        EndTextureMode();
    }
}

namespace {
void DrawPauseMenu(const Game& g) {
    const OwState& o = g.ow;
    auto rows = MenuRowsOf(g);
    int n = (int)rows.size();
    int bw = 128, bh = n * 16 + 12, bx = VW - bw - 10, by = 10;
    DrawPanel(bx, by, bw, bh, Color{250, 250, 250, 248}, Color{60, 90, 140, 255});
    int sel = std::max(0, std::min(n - 1, o.menuSel));
    for (int k = 0; k < n; k++) {
        int y = by + 8 + k * 16;
        if (k == sel) DrawTriangle({(float)bx + 8, (float)y + 2}, {(float)bx + 8, (float)y + 11}, {(float)bx + 14, (float)y + 6.5f}, Color{40, 40, 50, 255});
        bool dim = rows[k] == MR_WAIT && !MapOf(o.w[o.menuWho]).indoor;
        GDrawText(MenuLabel(g, rows[k]).c_str(), bx + 20, y + 1, 10, dim ? Color{150, 150, 160, 255} : Color{40, 40, 50, 255});
    }
    DrawBox(10, VH - 40, VW - 20 - bw - 10, 30, MenuTip(g, rows[sel]), false);
    if (g.coop) GDrawText(TextFormat("(%s)", WP(g, o.menuWho).name.c_str()), bx + 6, by + bh + 4, 10, UI_DIM);
}

void DrawTownMap(const Game& g) {
    const OwState& o = g.ow;
    DrawRectangle(0, 0, VW, VH, Fade(Color{16, 24, 40, 255}, 0.94f));
    GDrawText("TOWN MAP", 14, 10, 20, UI_GOLD);
    auto t = Towns(MapRegionOf(g, o.menuWho));
    int sel = std::min(o.townSel, std::max(0, (int)t.size() - 1));
    const int mx = 14, my = 36, mw = 400, mh = 300;
    Profile pv = g.coop ? g.coopP[g.on.active ? g.on.me : 0] : g.profile;
    pv.storyStage = StoryStageNow(g);
    DrawStoryMap(pv, mx, my, mw, mh, g.t, false, -1, 0);
    int x = 424;
    std::string who;
    bool canFly = CanField(g, o.menuWho, "Fly", &who);
    GDrawText(canFly ? TextFormat("Fly with %s: pick a town", who.c_str()) : TextFormat("Fly: needs the %s Badge", FieldBadge(2, MapRegionOf(g, o.menuWho)).c_str()), x, 40, 10, canFly ? Color{140, 230, 150, 255} : UI_DIM);
    for (int k = 0; k < (int)t.size(); k++) {
        const OwMap& tm = gMaps[t[k]];
        bool seen = Flags(g).count("visit:" + tm.town) > 0;
        int y = 58 + k * 16;
        if (y > VH - 30) break;
        bool on = k == sel;
        DrawPanel(x, y, VW - x - 12, 14, on ? UI_PANEL_LIGHT : UI_PANEL, on ? UI_GOLD : Color{70, 64, 92, 255});
        GDrawText(seen ? tm.town.c_str() : "???", x + 8, y + 2, 10, on ? UI_GOLD : (seen ? UI_TEXT : UI_DIM));
    }
    bool here = false;
    for (int k = 0; k < (int)t.size(); k++) here |= o.w[o.menuWho].map == t[k] && k == sel;
    if (here) GDrawText("(you are here)", x, VH - 34, 10, UI_DIM);
    GDrawText("Up/Down town   Enter Fly   Esc back", x, VH - 18, 10, UI_DIM);
}
} // namespace

void DrawWorld(const Game& g) {
    DrawRectangle(0, 0, VW, VH, BLACK);
    if (!WorldAvailable() || g.ow.w[0].map < 0) return;
    if (g.ow.townMap) { DrawTownMap(g); return; }
    if (Driven(g) == 2) {   // local co-op: split screen
        DrawView(g, 0, 0, VW / 2 - 1);
        DrawView(g, 1, VW / 2 + 1, VW / 2 - 1);
        DrawRectangle(VW / 2 - 1, 0, 2, VH, UI_INK);
        if (g.ow.teamUp) DrawTeamTag(VW / 2 + 85);
        GDrawText(g.coopP[0].name.c_str(), 6, VH - 13, 10, UI_GOLD);
        GDrawText(g.coopP[1].name.c_str(), VW / 2 + 6, VH - 13, 10, Color{255, 150, 230, 255});
        if (g.ow.menu) DrawPauseMenu(g);
        if (g.showBoard) DrawLeaderboard(g);
        return;
    }
    if (g.ow.watching) {   // your partner's battle
        const std::string& pn = g.coopP[1 - g.on.me].name;
        if (g.ow.watchBattle) {
            int keep = gLocalInput;
            gLocalInput = 7;
            g.ow.watchBattle->Draw();
            gLocalInput = keep;
        } else {
            DrawRectangle(0, 0, VW, VH, Color{18, 16, 30, 255});
            DrawTextCentered((pn + " is getting ready for battle...").c_str(), VW / 2, VH / 2 - 6, 10, UI_TEXT, false);
        }
        std::string s = "Watching " + pn + "'s battle   (Esc: stop watching)";
        DrawPanel(VW / 2 - TextW(s.c_str(), 10) / 2 - 10, 2, TextW(s.c_str(), 10) + 20, 16, Fade(UI_PANEL, 0.9f), Color{255, 150, 230, 255});
        DrawTextCentered(s.c_str(), VW / 2, 5, 10, UI_TEXT, false);
        return;
    }
    DrawView(g, 0, 0, VW);
    if (g.ow.sub[0]) return;   // your own battle / Healing Center / Mart (online) fills the screen
    if (g.ow.menu) { DrawPauseMenu(g); return; }
    if (g.showBoard) { DrawLeaderboard(g); return; }
    if (g.ow.teamUp) DrawTeamTag(VW - 8);
    if (Online(g) && g.ow.w[1].here) {   // facing your partner: watch their battle, or team up / split up
        const OwWalker& w = g.ow.w[0];
        const OwWalker& p = g.ow.w[1];
        if (p.map == w.map && p.x == w.x + DX[w.face] && p.y == w.y + DY[w.face] && w.msg.empty()) {
            const std::string& pn = g.coopP[1 - g.on.me].name;
            std::string s = PartnerBattling(g) ? "Enter: watch " + pn + "'s battle" : g.ow.teamUp ? "Enter: split up with " + pn : "Enter: team up with " + pn;
            DrawPanel(VW / 2 - TextW(s.c_str(), 10) / 2 - 10, VH - 40, TextW(s.c_str(), 10) + 20, 16, Fade(UI_PANEL, 0.92f), UI_GOLD);
            DrawTextCentered(s.c_str(), VW / 2, VH - 37, 10, UI_GOLD, false);
        }
    }
    if (Online(g) && !g.ow.w[1].here) {
        std::string s = g.coopP[1 - g.on.me].name + " is " + (g.on.partnerActivity.empty() ? std::string("busy") : g.on.partnerActivity);
        GDrawText(s.c_str(), VW - 8 - TextW(s.c_str(), 10), VH - 14, 10, Fade(Color{255, 150, 230, 255}, 0.85f));
    }
}

bool StageWalked(int s) {
    return WorldAvailable() && s >= 0 && s < (int)Story().stages.size() && gWalkedRegions.count(StageRegion(s)) > 0;
}
bool StageSkipped(int s) { return StageWalked(s) && gSkipStages.count(Story().stages[s].name) > 0; }
bool WorldRequired(const Game& g) {
    if (!WorldAvailable() || gWalkedRegions.empty()) return false;
    int s = StoryStageNow(g);
    if (s < 0 || s >= (int)Story().stages.size()) return false;
    return gWalkedRegions.count(StageRegion(s)) > 0;
}

// The story lives out in the world: the walked region's story, the time after its Champion until you go on
// to the next region, and a visit back from the Story menu.
#ifdef ARENA_AUTOTEST
bool gWorldHome = false;
#else
bool gWorldHome = true;
#endif
bool InWorldMode(const Game& g) {
    if (!gWorldHome || !WorldAvailable() || gWalkedRegions.empty()) return false;
    if (WorldRequired(g) || Flags(g).count("ow:visit")) return true;
    int s = StoryStageNow(g);
    return s > 0 && s <= (int)Story().stages.size() && CanChooseRegion(g) && gWalkedRegions.count(StageRegion(s - 1)) > 0;
}
bool WorldStartOf(int region, std::string& map, int& x, int& y) {   // your room in that region (New Bark Town for Johto)
    for (const auto& m : gMaps)
        if (m.startX >= 0 && m.region == region) { map = m.name; x = m.startX; y = m.startY; return true; }
    return false;
}
bool WalkedOpening(int region) { return gWorldHome && WorldAvailable() && gWalkedRegions.count(region) > 0; }
bool CanVisitWorld(const Game& g) {
    if (!WorldAvailable() || gWalkedRegions.empty()) return false;
    for (int r : gWalkedRegions) if (RegionDone(g, r)) return true;
    return false;
}
void VisitWorld(Game& g) {
    if (!CanVisitWorld(g)) return;
    SetFlag(g, "ow:visit");
    g.ow.hubOnly = false;
    EnterWorld(g);
}
// The weather of a battle out in the world: the map's own (FireRed's rainy routes) or the day's.
int gBattleWx = -1;   // (set as a world battle starts: a Weather, -1 = not from the world)
Weather WorldBattleWeather(const Game& g, int clock) {
    if (gBattleWx >= 0) return (Weather)gBattleWx;
    (void)g;
    return DayBattleWeather(clock);
}
namespace {
std::string WorldWeatherAt(const Game& g, const OwMap& m, int x, int y) {   // what's falling on this map now ("" clear)
    if (m.indoor) return "";
    for (const auto& z : m.wxZones) if (x >= z.x0 && x <= z.x1 && y >= z.y0 && y <= z.y1) return z.wx;
    if (!m.weather.empty()) return m.weather;
    return DayWeather(StoryClock(g));
}
} // namespace
namespace {
void NoteBattleWeather(const Game& g, int i) {   // a battle starts here: its weather is the map's
    const OwMap& m = MapOf(g.ow.w[i]);
    std::string wx = WorldWeatherAt(g, m, g.ow.w[i].x, g.ow.w[i].y);
    gBattleWx = (int)(wx == "rain" ? Weather::Rain : wx == "snow" ? Weather::Snow : wx == "sand" ? Weather::Sand
                      : wx == "sun" && !IsNight(StoryClock(g)) ? Weather::Sun : Weather::None);
}
} // namespace

std::string WorldMatchCall(Game& g) {   // the Pokenav from the Bag: who's registered, and who wants a rematch
    if (!g.ow.active || !WorldAvailable()) return "Use it out in the world.";
    int i = std::max(0, std::min(1, g.ow.menuWho));
    OwWalker& w = g.ow.w[i];
    if (w.map < 0) return "Use it out in the world.";
    int reg = 0;
    std::string ready;
    for (const auto& m : gMaps)
        for (const auto& n : m.npcs) {
            if (!McRegistered(g, n)) continue;
            reg++;
            if (RematchTier(g, n) > 0) ready += "|" + n.args[0] + " - waiting on " + m.name;
        }
    if (reg == 0) { Say(w, "Phone|No Trainers registered yet. Beat Johto's and Hoenn's Trainers and they'll keep in touch!"); return ""; }
    Say(w, std::string("Phone|") + TextFormat("%d Trainer%s registered.", reg, reg == 1 ? "" : "s") +
           (ready.empty() ? "|Nobody's asked for a rematch just now. They'll call you as you travel." : "|They want a rematch:" + ready));
    return "";
}

std::string WorldVsSeeker(Game& g) {
    if (!g.ow.active || !WorldAvailable()) return "Use it out in the world, near Trainers you've beaten.";
    int i = std::max(0, std::min(1, g.ow.menuWho));
    OwWalker& w = g.ow.w[i];
    if (w.map < 0) return "Use it out in the world.";
    if (w.vsCharge < 100) return "The battery isn't charged enough. Walk " + std::to_string(100 - w.vsCharge) + " more steps to recharge it.";
    const OwMap& m = MapOf(w);
    int badges = (int)(g.coop ? g.coopSave.badges : g.profile.badges).size();
    int ready = 0;
    for (int k = 0; k < (int)m.npcs.size(); k++) {
        const OwNpc& n = m.npcs[k];
        if (n.kind != "fight" || n.args.size() < 5 || !NpcHere(g, n) || !Flags(g).count(n.args[4]) || RematchTier(g, n) > 0) continue;
        const NpcRt& r = gNpc[w.map][k];
        if (std::abs(r.x - w.x) > 7 || std::abs(r.y - w.y) > 5) continue;   // on screen
        int next = RematchesWon(g, n) + 1;
        if (next > 4 || badges < 2 * next) continue;   // the next rematch waits for more badges
        SetFlag(g, "rematch:" + n.args[4] + ":" + std::to_string(next));
        ready++;
    }
    if (!ready) return "The VS Seeker beeped... but no Trainers nearby want a rematch.";
    w.vsCharge = 0;
    Say(w, ready == 1 ? "The VS Seeker beeped!|A Trainer nearby wants to battle again!" : "The VS Seeker beeped!|" + std::to_string(ready) + " Trainers nearby want to battle again!");
    return "";
}

// The Bag's Repels and Escape Rope (a message why not, or "": used).
std::string WorldUseField(Game& g, const std::string& item) {
    if (!g.ow.active || !WorldAvailable()) return "Use it out in the world (Walk around).";
    int i = std::max(0, std::min(1, g.ow.menuWho));
    OwWalker& w = g.ow.w[i];
    if (w.map < 0) return "Use it out in the world.";
    Profile& P = g.profile;   // (from the Bag: in co-op it's the opened player's, copied back after)
    const ItemDef* d = FindItem(item);
    if (!d || (d->fx != ItemFx::Field && item != "Itemfinder") || BagCount(P, item) <= 0) return "You can't use that here.";
    if (item == "White Flute" || item == "Black Flute") {   // Emerald's flutes: more (white) or fewer (black) wild Pokemon while it plays
        std::string me = (item == "White Flute" ? "flute:white:" : "flute:black:") + P.name, other = (item == "White Flute" ? "flute:black:" : "flute:white:") + P.name;
        Flags(g).erase(other);
        bool on = !Flags(g).count(me);
        if (on) Flags(g).insert(me); else Flags(g).erase(me);
        SaveAll(g);
        Toast(g, on ? (item == "White Flute" ? "You played the White Flute. Wild Pokemon will appear more often." : "You played the Black Flute. Wild Pokemon will appear less often.")
                    : "You stopped playing the " + item + ".");
        return "";
    }
    if (item == "Itemfinder") {   // which way the nearest hidden item is
        const OwMap& m = MapOf(w);
        int best = -1, bd = 1 << 30;
        for (int k = 0; k < (int)m.items.size(); k++) {
            const auto& it = m.items[k];
            if (!it.hidden || ItemAt(g, i, w.map, it.x, it.y, true) < 0) continue;
            int dd = std::abs(it.x - w.x) + std::abs(it.y - w.y);
            if (dd < bd) { bd = dd; best = k; }
        }
        if (best < 0) { Toast(g, "...Nope! The Itemfinder isn't responding."); return ""; }
        const auto& it = m.items[best];
        int dx = it.x - w.x, dy = it.y - w.y;
        std::string dir = dx == 0 && dy == 0 ? "right under your feet" :
            std::string(dy < 0 ? "north" : dy > 0 ? "south" : "") + (dx && dy ? "-" : "") + (dx < 0 ? "west" : dx > 0 ? "east" : "");
        Toast(g, bd <= 1 ? "...! The Itemfinder is going wild - it's " + dir + ", right next to you!"
                         : TextFormat("...! The Itemfinder is responding: something is hidden %d steps away, to the %s.", bd, dir.c_str()));
        return "";
    }
    if (item != "Escape Rope") {
        if (P.repel > 0) return "The last Repel is still keeping wild Pokemon away.";
        P.repel = (int)d->value;
        BagTake(P, item);
        SaveOrWarnP(g);
        Toast(g, TextFormat("Used a %s! Wild Pokemon will stay away for %d steps.", item.c_str(), P.repel));
        PlaySfx(Sfx::Repel);
        return "";
    }
    const OwMap& m = MapOf(w);
    if (!m.indoor || m.wild.empty()) return "You can only use it in a cave or a building with wild Pokemon.";
    int to = -1;   // the town of the last Pokemon Center you used (Pallet Town before you've used one)
    for (const auto& f : Flags(g)) {
        if (f.rfind("center:", 0) != 0) continue;
        std::string cm = Split(f.substr(7), ':')[0];
        size_t at = cm.find(" Pokemon Center");
        if (at != std::string::npos) to = MapIndex(cm.substr(0, at));
        break;
    }
    if (to < 0 || gMaps[to].flyX < 0) to = MapIndex("Pallet Town");
    if (to < 0 || gMaps[to].flyX < 0) return "There's nowhere to climb out to.";
    BagTake(P, item);
    w.surfing = w.biking = false;
    PlaceAt(g, i, to, gMaps[to].flyX, gMaps[to].flyY);
    w.face = 0;
    w.fadeT = 0.6f;
    SavePos(g);
    SaveOrWarnP(g);
    Toast(g, "Used the Escape Rope! You climbed back out to " + gMaps[to].name + ".");
    return "";
}

std::string WorldToggleBike(Game& g) {
    if (!g.ow.active || !WorldAvailable()) return "Ride it out in the world (Walk around).";
    int i = std::max(0, std::min(1, g.ow.menuWho));
    OwWalker& w = g.ow.w[i];
    if (w.map < 0) return "Ride it out in the world.";
    const OwMap& m = MapOf(w);
    if (w.biking) {
        if (m.bike) return "You can't get off here - this is Cycling Road!";
        w.biking = false;
        return "";
    }
    if (m.indoor && !m.cycling) return "You can't ride a Bicycle indoors!";
    if (w.surfing) return "You can't ride a Bicycle on the water!";
    w.biking = true;
    return "";
}

void SafariEnded(Game& g) { SafariOver(g); }   // (screens_safari.cpp: out of balls)
// The Ruins of Alph: a puzzle put together - the floor gives way into the Inner Chamber, where the Unown wake up
void AlphSolved(Game& g, const std::string& kind) {
    SetFlag(g, "alph:" + kind);
    SetFlag(g, "alph:any");
    SaveAll(g);
    struct At { const char* kind; int x, y; };
    static const At kAt[] = {{"kabuto", 15, 4}, {"hooh", 3, 16}, {"omanyte", 3, 22}, {"aerodactyl", 15, 25}};
    int x = 10, y = 12;
    for (const auto& k : kAt) if (kind == k.kind) { x = k.x; y = k.y; }
    PlaySfx(Sfx::Thud);
    Say(g.ow.w[0], "*Rumble...*|The floor gave way!", TextFormat("warp|Ruins of Alph Inner Chamber|%d|%d|down", x, y));
}

// (the autotests: a flag the way the map sees it - "hightide", "mirage" - and the square at x,y as you'd walk it)
bool OwHasFlag(const Game& g, const std::string& f) { return HasFlag(g, 0, f); }
char OwCell(const Game& g, int x, int y) { return g.ow.w[0].map >= 0 ? Cell(g, 0, g.ow.w[0].map, x, y) : '#'; }
std::string OwNpcDump(const Game& g) {   // (the autotests: where everyone on this map stands, and which way they face)
    std::string s;
    int map = g.ow.w[0].map;
    if (map < 0) return s;
    for (size_t k = 0; k < gNpc[map].size() && k < gMaps[map].npcs.size(); k++)
        s += TextFormat("%s@%d,%d/%d ", gMaps[map].npcs[k].kind.c_str(), gNpc[map][k].x, gNpc[map][k].y, gNpc[map][k].face);
    return s;
}
void OwRunAction(Game& g, int i, const std::string& act) { RunAction(g, i, act); }   // (the autotests)
// A person off the people sheet, standing and facing you, drawn at screen x, y (top-left) and this scale (the Fame Checker).
void DrawPersonSprite(const std::string& name, float x, float y, float scale) {
    if (!gChars.id) return;
    int s = SpriteIndex(name);
    DrawTexturePro(gChars, {32.0f, (float)s * 32, 32, 32}, {x, y, 32 * scale, 32 * scale}, {0, 0}, 0, WHITE);
}
// (the autotests: a Feebas tile on this map, if it has any)
bool OwFeebasTile(const Game& g, int& x, int& y) {
    int map = g.ow.w[0].map;
    if (map < 0) return false;
    const OwMap& m = gMaps[map];
    for (y = 0; y < m.h; y++) for (x = 0; x < m.w; x++) if (FeebasTile(g, m, x, y)) return true;
    return false;
}
