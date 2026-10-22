// The overworld (test build). Maps are the game's own pictures (World/*.png) with a hidden layer of
// rules from World/world.txt: which 16-pixel tiles are solid, tall grass or a ledge, where the
// doors, signs and people are, and how the maps join at their edges. Walking up to the story's
// trainers, into tall grass or through a Pokemon Center's door uses the same battles and screens
// as the menu.
#include "overworld.h"
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
    int mon = -1;                     // sprite "mon:Name": a Pokemon standing there (a legendary in its lair)
    std::string needItem, needText, usedText;   // "need = Item|text without it|text using it": Snorlax and the Poke Flute
};
struct OwLink { int map = -1; int off = 0; std::string msg; };
struct OwMap {
    std::string name, image, wild;
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
    std::vector<Act> doors, spots;
    std::vector<OwNpc> npcs;
    struct Item { int x, y; std::string item; int n = 1; bool hidden = false; std::string ifF, unlessF; };
    std::vector<Item> items;          // item balls (and hidden items) to pick up, once each
    struct Gate { int x, y; std::string flag; };
    struct Block { int x, y; std::string flag, text; };
    std::vector<Block> blocks;        // "block = x y flag|text": you're turned back there until the flag (FireRed's guards, the old man)
    std::vector<Gate> gates;          // solid until the flag is set (a guard's gone, a switch is pressed)
    bool dark = false;                // a cave you can barely see in until you use Flash
    std::string weather;              // rain, snow, fog, sand: falls on this map (outdoors)
    bool bike = false;                // Cycling Road: Bicycles only
    std::string town;                 // a town you can Fly to (its name on the Town Map)
    int flyX = -1, flyY = -1;         // ...where you land (outside its Pokemon Center)
    int startX = -1, startY = -1;
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
Texture2D gFieldObj{};  // Cut tree, smashable rock, boulder, item ball
bool gLoaded = false;

const int DX[4] = {0, 0, -1, 1}, DY[4] = {1, -1, 0, 0};
int Opp(int f) { return f == 0 ? 1 : f == 1 ? 0 : f == 2 ? 3 : 2; }

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
bool StageBeaten(const Game& g, int s);
bool HasBadge(const Game& g, const std::string& b) {
    const auto& v = g.coop ? g.coopSave.badges : g.profile.badges;
    return std::find(v.begin(), v.end(), b) != v.end();
}
// A condition: a flag that's been set, or "badge:Cascade", "stage:Cerulean Gym" (the story is past it),
// "item:Bicycle" (this walker carries it); "!" in front turns it around.
bool HasFlag(const Game& g, int i, const std::string& f0) {
    if (f0.empty()) return true;
    if (size_t amp = f0.find('&'); amp != std::string::npos) return HasFlag(g, i, f0.substr(0, amp)) && HasFlag(g, i, f0.substr(amp + 1));   // all of them
    if (f0[0] == '!') return !HasFlag(g, i, f0.substr(1));
    if (f0.rfind("badge:", 0) == 0) return HasBadge(g, f0.substr(6));
    if (f0.rfind("badges:", 0) == 0) return (int)(g.coop ? g.coopSave.badges : g.profile.badges).size() >= std::atoi(f0.c_str() + 7);
    if (f0.rfind("stage:", 0) == 0) { int s = StageIndexByName(f0.substr(6)); return s >= 0 && StageBeaten(g, s); }
    if (f0.rfind("item:", 0) == 0) return BagCount(WP(g, i), f0.substr(5)) > 0;
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

// ---- the field moves: someone in your party knows it, and you have the badge
bool PartyKnows(const Profile& p, const std::string& mv, std::string* who = nullptr) {
    for (const auto& m : p.mons)
        if (!m.boxed && std::find(m.moves.begin(), m.moves.end(), mv) != m.moves.end()) { if (who) *who = m.species; return true; }
    return false;
}
int HmIndex(const std::string& mv) { for (int n = 1; n <= 7; n++) if (mv == HmMove(n)) return n; return 0; }
bool CanField(const Game& g, int i, const std::string& mv, std::string* who = nullptr) {
    int n = HmIndex(mv);
    if (n <= 0 || !PartyKnows(WP(g, i), mv, who)) return false;
    return HasBadge(g, kKantoBadges[std::min(8, HmBadges(n))]);
}
std::string FieldNeed(const std::string& mv) {   // "You need the Cascade Badge and a Pokemon that knows Cut."
    int n = HmIndex(mv);
    return std::string("You need the ") + kKantoBadges[std::min(8, HmBadges(n))] + " Badge and a Pokemon that knows " + mv + ".";
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

std::string gPlayerName = "you";   // {PLAYER} in what people say: the one walking
void Say(OwWalker& w, const std::string& text, const std::string& after = "") {
    std::string t = text;
    for (size_t at; (at = t.find("{PLAYER}")) != std::string::npos;) t.replace(at, 8, gPlayerName);   // FireRed's lines name you
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
char Cell(const Game& g, int i, int map, int x, int y) {
    const OwMap& m = gMaps[map];
    char c = m.At(x, y);
    if (c == '#' && (x < 0 || y < 0 || x >= m.w || y >= m.h)) return c;
    int k = y * m.w + x;
    const MapRt& r = gRt[map];
    if ((c == 'T' || c == 'R') && r.cleared.count(k)) c = '.';
    if (c == 'B') c = '.';
    if (std::find(r.boulders.begin(), r.boulders.end(), k) != r.boulders.end()) return 'B';
    for (const auto& gt : m.gates) if (gt.x == x && gt.y == y && !HasFlag(g, i, gt.flag)) return '#';
    return c;
}
bool Walkable(char c) { return c == '.' || c == 'g' || c == 'c' || c == 'D' || c == '0' || (c >= '1' && c <= '4'); }
bool IsWater(char c) { return c == 'w' || c == 'W'; }
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

void PlaceAt(Game& g, int i, int map, int x, int y) {
    OwWalker& w = g.ow.w[i];
    bool newMap = map != w.map;
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
        if (n.sight <= 0 || !NpcHere(g, n) || !TrainerReady(g, n)) continue;
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
    for (const auto& e0 : Split(n.args.size() > 1 ? n.args[1] : "", ',')) {
        std::string e = Trim(e0);
        size_t sp = e.find_last_of(' ');
        if (e.empty() || sp == std::string::npos) continue;
        int lv = std::atoi(e.c_str() + sp + 1) + 7 * tier;   // a rematch: stronger each time, and evolved to match
        std::string name = Trim(e.substr(0, sp));
        if (tier > 0) { int si = SpeciesIndex(name); if (si >= 0) name = AllSpecies()[EvolvedFormAt(si, lv)].name; }
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
void StartRouteFight(Game& g, int i, int npc) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    const OwNpc& n = m.npcs[npc];
    if (HealthyCount(WP(g, i)) == 0) { Say(w, "Your Pokemon are all too tired to battle!|Rest them at a Pokemon Center first."); return; }
    SaveAll(g);
    Game& t = g.coop ? StartSub(g, i) : g;   // co-op: your own battle, in your half of the screen
    t.owFight = true;
    int tier = RematchTier(g, n);
    t.owStage = FightStage(m, n, tier);
    t.owFightFlag = n.args.size() > 4 ? n.args[4] + (tier > 0 ? "#" + std::to_string(tier) : "") : "";
    t.stageIdx = -1;
    t.legend = -1;
    t.tower = false;
    t.raid = false;
    t.mode = Mode::Story;
    t.rareBattle = false;
    t.ow.wildBattle = false;
    t.encounterBlank = g.coop;
    QuickBattle(t);
}

void StartStoryFight(Game& g, int i, int npc) {
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

void Interact(Game& g, int i, int npc) {
    OwWalker& w = g.ow.w[i];
    const OwNpc& n = MapOf(w).npcs[npc];
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
    g.ow.w[i].surfing = false;
    PlaceAt(g, i, m, std::atoi(a[2].c_str()), std::atoi(a[3].c_str()));
    g.ow.w[i].fadeT = 0.35f;
    PlaySfx(Sfx::MenuOk);
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
    if (!CanField(g, i, mv, &who)) { Say(w, FieldNeed(mv)); return; }
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
    } else if (mv == "Waterfall") {
        int y = w.y;
        while (Cell(g, i, w.map, w.x, y - 1) == 'W') y--;
        if (IsWater(Cell(g, i, w.map, w.x, y - 1))) y--;
        if (y != w.y) { w.fromX = w.x; w.fromY = y; w.y = y; w.fx = w.x; w.fy = y + 1; w.fFromX = w.fx; w.fFromY = w.fy; w.stepT = 1; w.fadeT = 0.3f; }
        Say(w, who + " used Waterfall! You climbed the waterfall.");
    }
}

// Fishing with a rod, facing the water.
void Fish(Game& g, int i, const std::string& rod) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    std::string method = rod == "Old Rod" ? "old-rod" : rod == "Good Rod" ? "good-rod" : "super-rod";
    PlaySfx(Sfx::MenuOk);
    w.fishing = true;
    w.fishT = 0;
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
        else { t.shopSel = 0; t.screen = Screen::Shop; }
    }
    else if (k == "shopmsg" || k == "healmsg") Say(w, act.substr(k.size() + 1), k == "shopmsg" ? "shop" : "heal");
    else if (k == "flag" && a.size() > 1) SetFlag(g, a[1]);
    else if (k == "give" && a.size() >= 4) {   // give|item|n|flag
        if (HasFlag(g, i, a[3])) return;
        int n = std::max(1, std::atoi(a[2].c_str()));
        BagAdd(WP(g, i), a[1], n);
        SetFlag(g, a[3]);
        SaveAll(g);
        if (g.coop) SaveProfile(WP(g, i));
        PlaySfx(Sfx::LevelUp);
        Say(w, (n > 1 ? TextFormat("You got %d x %s!", n, a[1].c_str()) : "You got the " + a[1] + "!") + std::string("|") + PocketHint(a[1]));
    }
    else if (k == "field" && a.size() > 1) UseField(g, i, a[1]);
    else if (k == "fish" && a.size() > 1) Fish(g, i, a[1]);
    else if (k == "fly") { g.ow.townMap = true; g.ow.townSel = 0; g.ow.menuWho = i; }
    else if (k == "trashcan" && a.size() > 1) TrashCan(g, i, a[1]);
    else if (k == "raid" || k == "tower" || k == "legend") WorldChallenge(g, i, a);
    else if (k == "encounter" && a.size() >= 4) {   // encounter|Species|level|flag|text|item needed|text without it
        if (HasFlag(g, i, a[3])) return;
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
            PlaySfx(Sfx::LevelUp);
        }
        SaveAll(g);
        if (g.coop) SaveProfile(WP(g, i));
        Say(w, t);
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
        PlaySfx(Sfx::LevelUp);
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
    else RunAction(g, i, a);
}

// Teamed up: a 2v2 wild battle for both players (your partner comes over). False: they're busy,
// so it's your own battle after all.
bool TeamWild(Game& g, int i, const std::string& method = "walk") {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    for (int p = 0; p < 2; p++) if (HealthyCount(Prof(g, p)) == 0) return false;
    if (Online(g)) {
        if (PartnerBattling(g) || !g.ow.w[1].here) return false;
        g.ow.wildBattle = true;
        g.ow.wildArea = m.wild;
        g.ow.wildMethod = method;
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
    g.ow.wildArea = m.wild;
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

void WildEncounter(Game& g, int i, const std::string& method) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    if (Online(g) && (g.ow.incoming || g.ow.waitFight)) return;   // already on your way into a battle together
    if (WildAreaIndex(m.wild) < 0 || HealthyCount(WP(g, i)) == 0) return;
    w.stepsSinceWild = 0;
    SaveAll(g);
    g.ow.wildMethod = method;
    // co-op: teamed up (always, online: you both go in at once, so neither of you walks into another one meanwhile)
    if (g.coop && (Online(g) || (g.ow.teamUp && method == "walk")) && TeamWild(g, i, method)) return;
    Game& t = g.coop ? StartSub(g, i) : g;
    if (t.profile.trainArea != m.wild) { t.profile.trainArea = m.wild; t.rareSpecies = -1; }
    t.ow.wildBattle = true;
    t.ow.wildArea = m.wild;
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

// A step finished: grass, trainers.
void Arrived(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    w.stepsSinceWild++;
    w.vsCharge = std::min(100, w.vsCharge + 1);
    if (m.At(w.x, w.y) == 'g') gRustle.push_back({w.map, w.x, w.y, 0});
    if (i == 0) SendPos(g);
    if (!m.town.empty() && !Flags(g).count("visit:" + m.town)) SetFlag(g, "visit:" + m.town);   // somewhere to Fly to
    if (m.indoor) w.biking = false;
    char c = Cell(g, i, w.map, w.x, w.y);
    if (c >= '1' && c <= '4') w.spin = c == '1' ? 1 : c == '2' ? 0 : c == '3' ? 2 : 3;   // a spin tile: off you go
    else if (c == '0') w.spin = -1;
    if (w.spin >= 0) return;
    if (CheckSight(g, i)) return;
    Profile& P = WP(g, i);
    if (P.repel > 0 && --P.repel == 0) { Say(w, "Repel's effect wore off..."); return; }
    if (m.wild.empty() || w.stepsSinceWild < 3 || P.repel > 0) return;
    int lead = -1;   // a Cleanse Tag on the first Pokemon: a third fewer wild Pokemon
    for (int k = 0; k < (int)P.mons.size() && lead < 0; k++) if (!P.mons[k].boxed) lead = k;
    int tag = lead >= 0 && P.mons[lead].item == "Cleanse Tag" ? 2 : 3;
    if ((c == 'g' && SimRand(0, 299) < 12 * tag) || (c == 'c' && SimRand(0, 299) < 8 * tag)) WildEncounter(g, i);   // tall grass; a cave floor
    else if (w.surfing && c == 'w' && SimRand(0, 299) < 6 * tag && WildAreaHasMethod(m.wild, "surf")) WildEncounter(g, i, "surf");
}

// One step (or a ledge hop, a boulder push, Surf on and off).
void TryStep(Game& g, int i, int dir) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    w.face = dir;
    int nx = w.x + DX[dir], ny = w.y + DY[dir];
    auto bump = [&]() { if (w.bumpT <= 0) { PlaySfx(Sfx::Thud); w.bumpT = 0.35f; } w.spin = -1; };
    for (const auto& b : m.blocks)   // turned back until you've done what it takes
        if (b.x == nx && b.y == ny && !HasFlag(g, i, b.flag)) { w.spin = -1; Say(w, b.text.empty() ? "You can't go this way yet." : b.text); return; }
    for (const auto& d : m.doors)   // a door, stairs, or a mat you step off (it can be just past the edge)
        if (d.x == nx && d.y == ny && CondOk(g, i, d.ifF, d.unlessF)) { w.spin = -1; RunAction(g, i, d.act); return; }
    // off the edge: the next map
    if (nx < 0 || ny < 0 || nx >= m.w || ny >= m.h) {
        const OwLink& L = m.link[dir];
        if (L.map >= 0) {
            const OwMap& n = gMaps[L.map];
            int tx, ty;
            if (dir == 1) { tx = nx + L.off; ty = n.h - 1; }
            else if (dir == 0) { tx = nx + L.off; ty = 0; }
            else if (dir == 2) { tx = n.w - 1; ty = ny + L.off; }
            else { tx = 0; ty = ny + L.off; }
            char c = Cell(g, i, L.map, tx, ty);
            bool ok = w.surfing ? (IsWater(c) || Walkable(c)) : Walkable(c);
            if (!ok || WalkerAt(g, i, L.map, tx, ty)) { bump(); return; }
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
            if (!IsWater(c)) w.surfing = false;
            if (i == 0) SendPos(g);
            return;
        }
        if (!L.msg.empty() && w.bumpT <= 0) { RunAction(g, i, L.msg); w.bumpT = 0.5f; }
        return;
    }
    char c = Cell(g, i, w.map, nx, ny);
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
    }
    w.fFromX = w.fx; w.fFromY = w.fy;
    w.fx = w.x; w.fy = w.y;
    w.fFace = w.fx != w.fFromX ? (w.fx > w.fFromX ? 3 : 2) : (w.fy > w.fFromY ? 0 : w.fy < w.fFromY ? 1 : w.fFace);
    w.fromX = w.x; w.fromY = w.y;
    w.x = hx; w.y = hy;
    w.stepT = 0;
    w.hop = hop;
    w.walkAnim ^= 1;
    if (w.surfing && !IsWater(c)) w.surfing = false;   // back on land
    if (hop) PlaySfx(Sfx::MenuMove);
    if (i == 0) SendPos(g);
}

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
        PlaySfx(Sfx::LevelUp);
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
        if (!w.surfing && CanField(g, i, "Surf", &who) && c == 'w') ch.push_back({"Surf (" + who + ")", "field|Surf"});
        for (const char* rod : {"Old Rod", "Good Rod", "Super Rod"})
            if (BagCount(WP(g, i), rod) > 0) ch.push_back({std::string("Fish: ") + rod, std::string("fish|") + rod});
        if (ch.empty()) { if (!w.surfing) Say(w, PartyKnows(WP(g, i), "Surf") ? "The water is dyed a deep blue...|" + FieldNeed("Surf") : "The water is dyed a deep blue..."); return !w.surfing; }
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

void UpdateWalker(Game& g, int i, float dt, const WalkIn& in) {
    OwWalker& w = g.ow.w[i];
    gPlayerName = WP(g, i).name;
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
            if (s.x == fx && s.y == fy && CondOk(g, i, s.ifF, s.unlessF)) { RunAction(g, i, s.act); return; }
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

const char* const kHints = "Arrows walk   Shift / hold B run   Enter talk/read   Esc menu";

// ---- the menu (like the games: Start) and the Town Map
const char* kMenuRows[7] = {"POKeDEX", "POKeMON", "BAG", "MAP", "SAVE", "SETTINGS", "STORY MENU"};
PlayerInput MenuOf(const Game& g) { return LocalCoop(g) ? ReadInput(g.ow.menuWho, true) : MenuInput(); }

void UpdatePauseMenu(Game& g) {
    OwState& o = g.ow;
    PlayerInput in = MenuOf(g);
#ifdef ARENA_AUTOTEST
    if (gAutoMenu) { in.up = gAutoMenu & 1; in.down = gAutoMenu & 2; in.confirm = gAutoMenu & 16; in.back = gAutoMenu & 32; gAutoMenu = 0; }
#endif
    if (IsKeyPressed(KEY_ESCAPE) && !LocalCoop(g)) in.back = true;
    const int N = 7;
    if (in.up) { o.menuSel = (o.menuSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { o.menuSel = (o.menuSel + 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.back) { o.menu = false; PlaySfx(Sfx::MenuBack); return; }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    int who = WhoOf(g, o.menuWho);
    SaveAll(g);
    o.menu = false;
    switch (o.menuSel) {
        case 0: OpenPokedex(g, who); break;
        case 1:
            if (g.coop) CoopOpenFor(g, who);
            g.myCreSel = 0; g.itemPick = false; g.actMenu = false; g.swapFrom = -1; g.stonePick = false; g.moveOrder = false; g.boxTab = 0;
            g.screen = Screen::MyCreatures;
            break;
        case 2:
            if (g.coop) CoopOpenFor(g, who);
            OpenBag(g);
            break;
        case 3: o.townMap = true; o.townSel = 0; break;
        case 4: Toast(g, g.coop ? "Your journey together was saved." : WP(g, o.menuWho).name + " saved the game."); break;
        case 5: g.settingsSel = 0; g.settingsBack = Screen::World; g.screen = Screen::Settings; break;
        default:   // back to the Story menu (the overworld is still being built)
            o.active = false;
            if (Online(g)) OnlineSendWorldPos(false, -1, 0, 0, 0);
            EnterStoryHub(g);
            break;
    }
}

std::vector<int> Towns() {
    std::vector<int> v;
    for (size_t k = 0; k < gMaps.size(); k++) if (!gMaps[k].town.empty()) v.push_back((int)k);
    return v;
}

void UpdateTownMap(Game& g) {
    OwState& o = g.ow;
    PlayerInput in = MenuOf(g);
#ifdef ARENA_AUTOTEST
    if (gAutoMenu) { in.up = gAutoMenu & 1; in.down = gAutoMenu & 2; in.confirm = gAutoMenu & 16; in.back = gAutoMenu & 32; gAutoMenu = 0; }
#endif
    if (IsKeyPressed(KEY_ESCAPE) && !LocalCoop(g)) in.back = true;
    auto t = Towns();
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
    if (!CanField(g, i, "Fly", &who)) { Toast(g, FieldNeed("Fly")); PlaySfx(Sfx::MenuBack); return; }
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

// A map's pictures, loaded the first time it's on screen; the ones not seen for a while are let go
// (Kanto and the Sevii Islands are a few hundred maps).
void TouchMap(int map) {
    if (map < 0 || map >= (int)gMaps.size()) return;
    OwMap& m = gMaps[map];
    m.used = GetTime();
    if (m.tex.id) return;
    std::string dir = DataDir() + "World/";
    m.tex = LoadTex(dir + m.image);
    if (!m.overImage.empty()) m.over = LoadTex(dir + m.overImage);
    int loaded = 0;
    for (const auto& o : gMaps) loaded += o.tex.id != 0;
    while (loaded > 24) {
        OwMap* old = nullptr;
        for (auto& o : gMaps) if (o.tex.id && (!old || o.used < old->used)) old = &o;
        if (!old || old == &m) break;
        UnloadTexture(old->tex); old->tex = {};
        if (old->over.id) { UnloadTexture(old->over); old->over = {}; }
        loaded--;
    }
}

void LoadWorld(std::string& log) {
    for (auto& m : gMaps) { if (m.tex.id) UnloadTexture(m.tex); if (m.over.id) UnloadTexture(m.over); }
    gMaps.clear();
    gSpriteNames.clear();
    gStoryTown.clear();
    gWalkedRegions.clear();
    gSkipStages.clear();
    gFerries.clear();
    gLoaded = false;
    std::string dir = DataDir() + "World/";
    std::ifstream f(dir + "world.txt");
    if (!f) return;
    std::string line;
    OwMap* m = nullptr;
    bool grid = false;
    std::vector<std::pair<int, std::vector<std::string>>> links;   // resolved after every map is read
    std::string* lastIf = nullptr;     // "if = flag" / "unless = flag" apply to the last npc / door / spot / item
    OwNpc* lastNpc = nullptr;
    std::string* lastUnless = nullptr;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) { grid = false; continue; }
        if (line[0] == '#' && !grid) continue;
        if (line == "[map]") { gMaps.emplace_back(); m = &gMaps.back(); grid = false; lastIf = lastUnless = nullptr; lastNpc = nullptr; continue; }
        if (grid && m) {
            if (m->w == 0) m->w = (int)line.size();
            if ((int)line.size() == m->w) { m->cells += line; m->h++; }
            continue;
        }
        size_t eq = line.find(" = ");
        std::string k = Trim(line.substr(0, eq)), v = eq == std::string::npos ? "" : Trim(line.substr(eq + 3));
        if (k == "sprites") { std::istringstream ss(v); std::string s; while (ss >> s) gSpriteNames.push_back(s); continue; }
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
        if (k == "name") m->name = v;
        else if (k == "image") m->image = v;
        else if (k == "over") m->overImage = v;
        else if (k == "objects") m->objSprites = v == "sprites";
        else if (k == "wild") m->wild = v;
        else if (k == "indoor") m->indoor = v == "yes";
        else if (k == "dark") m->dark = v == "yes";
        else if (k == "weather") m->weather = v;
        else if (k == "bike") m->bike = v == "yes";
        else if (k == "town") m->town = v;
        else if (k == "fly") std::sscanf(v.c_str(), "%d %d", &m->flyX, &m->flyY);
        else if (k == "if" && lastIf) *lastIf = v;
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
            for (size_t q = 1; q < p.size(); q++) b.text += (q > 1 ? "|" : "") + p[q];
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
        } else if (k == "door" || k == "sign" || k == "spot") {
            int x = 0, y = 0, n = 0;
            std::sscanf(v.c_str(), "%d %d %n", &x, &y, &n);
            std::string act = v.substr(n);
            if (k == "sign") act = "msg|" + act;
            auto& list = k == "door" ? m->doors : m->spots;
            list.push_back({x, y, act, "", ""});
            lastIf = &list.back().ifF; lastUnless = &list.back().unlessF;
        } else if (k == "npc") {
            OwNpc p;
            char spr[32] = {}, face[16] = {};
            int n = 0;
            std::sscanf(v.c_str(), "%d %d %31s %15s %d %n", &p.x, &p.y, spr, face, &p.sight, &n);
            p.sprite = SpriteIndex(spr);
            if (std::strncmp(spr, "mon:", 4) == 0) { p.mon = SpeciesIndex(spr + 4); p.sprite = 0; }
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
            if (p.kind == "fight") {   // name|team|before|after -> + the flag a win sets
                while (p.args.size() < 4) p.args.push_back("");
                p.args.resize(4);
                p.args.push_back("beat:" + m->name + ":" + p.args[0]);
            }
            if (p.kind == "give" || p.kind == "legend" || p.kind == "fight" || p.kind == "encounter" || p.kind == "trainer") for (auto& a : p.args) for (char& ch : a) if (ch == '/') ch = '|';   // (text pages: "/" between them)
            m->npcs.push_back(p);
            lastIf = &m->npcs.back().ifF; lastUnless = &m->npcs.back().unlessF;
            lastNpc = &m->npcs.back();
        }
    }
    for (auto& [mi, a] : links) {
        int d = a[0] == "south" ? 0 : a[0] == "north" ? 1 : a[0] == "west" ? 2 : 3;
        gMaps[mi].link[d].map = MapIndex(a[1]);
        gMaps[mi].link[d].off = std::atoi(a[2].c_str());
    }
    gNpc.assign(gMaps.size(), {});
    gRt.assign(gMaps.size(), {});
    for (size_t i = 0; i < gMaps.size(); i++) ResetNpcs((int)i);
    for (auto& mm : gMaps)   // the pictures load when a map is first seen (see TouchMap); just check they're there
        if (!FileExists((dir + mm.image).c_str())) log += "World: missing " + mm.image + "\n";
    gChars = LoadTex(dir + "chars.png");
    gActions = LoadTex(dir + "actions.png");
    gFieldObj = LoadTex(dir + "fieldobj.png");
    gLoaded = !gMaps.empty();
    log += "World: " + std::to_string(gMaps.size()) + " maps\n";
}

std::string WorldPlaceName(const Game& g) {
    if (!WorldAvailable()) return "";
    std::string saved = g.coop ? g.coopSave.owMap[g.on.active ? g.on.me : 0] : g.profile.owMap;
    int m = MapIndex(saved);
    if (m < 0) for (size_t i = 0; i < gMaps.size(); i++) if (gMaps[i].startX >= 0) m = (int)i;
    return m >= 0 ? gMaps[m].name : "";
}

void EnterWorld(Game& g) {
    if (!WorldAvailable()) { Toast(g, "The overworld files are missing."); return; }
    int start = 0;
    for (size_t i = 0; i < gMaps.size(); i++) if (gMaps[i].startX >= 0) start = (int)i;
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
            if (!n.wander) continue;
            NpcRt& r = gNpc[map][k];
            if (r.st < 1) { r.st = std::min(1.0f, r.st + dt / (WALK * 1.25f)); continue; }
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

void UpdateWorld(Game& g, float dt) {
    if (!WorldAvailable() || g.ow.w[0].map < 0) { g.ow.active = false; EnterStoryHub(g); return; }
    OwState& o = g.ow;
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
    if (o.townMap) { UpdateTownMap(g); return; }
    if (o.menu) { UpdatePauseMenu(g); return; }
#ifdef ARENA_AUTOTEST
    extern int gAutoMenu;
    {   // autotests: "back" leaves the world for the Story menu, as before
        bool busy = false;
        for (int i = 0; i < Driven(g); i++) busy |= o.w[i].stepT < 1 || !o.w[i].msg.empty() || o.w[i].approach >= 0;
        busy |= AnySub(g);
        if ((gAutoMenu & 32) && !busy) { gAutoMenu = 0; SaveAll(g); o.active = false; if (Online(g)) OnlineSendWorldPos(false, -1, 0, 0, 0); EnterStoryHub(g); return; }
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
    Vector2 p = Scr(c, ox, oy);
    if (!m.tex.id) { DrawRectangle((int)p.x, (int)p.y, (int)(m.w * T * Z), (int)(m.h * T * Z), Color{40, 80, 50, 255}); return; }
    DrawTexturePro(m.tex, {0, 0, (float)m.tex.width, (float)m.tex.height}, {p.x, p.y, m.tex.width * Z, m.tex.height * Z}, {0, 0}, 0, WHITE);
}

// A person: 16x24, feet at the bottom of the tile. frame 0 standing, 1/2 walking.
void DrawChar(const View& c, int sprite, int face, int frame, float mx, float my, float lift = 0, const Texture2D* sheet = nullptr) {
    const Texture2D& tex = sheet ? *sheet : gChars;
    if (!tex.id) return;
    static const int kCol[3] = {1, 0, 2};   // the sheet's columns: step, stand, step (frame 0 = standing still)
    int col = (face == 0 ? 0 : face == 1 ? 3 : 6) + kCol[frame < 0 || frame > 2 ? 0 : frame];
    Rectangle src{(float)col * 16, (float)sprite * 32, face == 3 ? -16.0f : 16.0f, 32};
    Vector2 p = Scr(c, mx, my + T - 32 - lift);
    DrawEllipse((int)(p.x + 8 * Z), (int)(Scr(c, mx, my + T - 1).y), 6 * Z, 2 * Z, Fade(BLACK, 0.25f));
    DrawTexturePro(tex, src, {p.x, p.y, 16 * Z, 32 * Z}, {0, 0}, 0, WHITE);
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
    Rectangle src{(float)tx * T, (float)ty * T + 9, (float)T, 7};
    Vector2 p = Scr(c, (float)tx * T, (float)ty * T + 9);
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

void DrawMapChanges(const Game& g, int map, const View& c) {
    const OwMap& m = gMaps[map];
    const MapRt& rt = gRt[map];
    if (!m.tex.id) return;
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
Color DayTint() {
    std::time_t now = std::time(nullptr);
    std::tm* lt = std::localtime(&now);
    float h = lt ? lt->tm_hour + lt->tm_min / 60.0f : 12.0f;
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

void DrawView(const Game& g, int me, int vx, int vw) {
    const OwState& o = g.ow;
    const OwWalker& w = o.w[me];
    if (w.map < 0) return;
    TouchMap(w.map);
    for (int d = 0; d < 4; d++) TouchMap(gMaps[w.map].link[d].map);
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
    float viewW = vw / Z, viewH = VH / Z;
    View c{px + T / 2.0f - viewW / 2, py + T / 2.0f - viewH / 2, vx, vw};
    // keep the view on the map where nothing joins it (no black past the trees); small rooms
    // sit in the middle
    auto clampAxis = [](float& v, float size, float view, bool lowOpen, bool highOpen) {
        if (size <= view) { v = (size - view) / 2; return; }
        if (!lowOpen) v = std::max(v, 0.0f);
        if (!highOpen) v = std::min(v, size - view);
    };
    bool open = !m.indoor;
    clampAxis(c.x, (float)m.w * T, viewW, open && m.link[2].map >= 0, open && m.link[3].map >= 0);
    clampAxis(c.y, (float)m.h * T, viewH, open && m.link[1].map >= 0, open && m.link[0].map >= 0);
    // the camera moves in whole screen pixels (the canvas is CANVAS_ZOOM x the layout): smooth, without
    // the map and the walkers drifting a pixel apart
    c.x = std::round(c.x * Z * CANVAS_ZOOM) / (Z * CANVAS_ZOOM);
    c.y = std::round(c.y * Z * CANVAS_ZOOM) / (Z * CANVAS_ZOOM);
    DrawMapAt(m, c, 0, 0);
    DrawMapChanges(g, w.map, c);
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
            }
            const OwWalker* who = nullptr;
            for (int k = 0; k < Driven(g); k++) if (o.w[k].map == w.map && o.w[k].approach == d.i) who = &o.w[k];
            if (who && who->approachT >= 0) {   // mid-step toward them
                float t = who->approachT / WALK;
                nx += DX[r.face] * T * t;
                ny += DY[r.face] * T * t;
                frame = t > 0.2f && t < 0.8f ? 1 + ((r.x + r.y) & 1) : 0;
            }
            if (n.mon == -2) { RaidInfo ri = RaidOfDay(RaidToday()); if (ri.species >= 0) DrawNpcMon(g, c, ri.species, r.face, nx, ny); }
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
            Vector2 p = Scr(c, ox, oy);
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
    }
    if (!m.indoor && !m.dark) {   // outdoors: the time of day (your computer's clock) tints the world
        Color tint = DayTint();
        if (tint.a) DrawRectangle(vx, 0, vw, VH, tint);
        if (!m.weather.empty()) DrawMapWeather(m.weather, vx, vw, g.t);
    }
    if (m.dark && w.flashMap != w.map) {   // a dark cave: only a little circle of light around you
        Vector2 ctr = Scr(c, px + T / 2.0f, py + T / 2.0f);
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
        DrawBox(vx + 12, VH - 76, vw - 24, 64, w.msg[std::min(w.msgPage, (int)w.msg.size() - 1)], !asking);
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
    int bw = 118, bh = 7 * 18 + 12, bx = VW - bw - 10, by = 10;
    DrawPanel(bx, by, bw, bh, Color{250, 250, 250, 248}, Color{60, 90, 140, 255});
    for (int k = 0; k < 7; k++) {
        int y = by + 8 + k * 18;
        const char* label = k == 6 && g.coop ? "CO-OP MENU" : kMenuRows[k];
        if (k == o.menuSel) DrawTriangle({(float)bx + 8, (float)y + 2}, {(float)bx + 8, (float)y + 11}, {(float)bx + 14, (float)y + 6.5f}, Color{40, 40, 50, 255});
        GDrawText(label, bx + 20, y + 1, 10, Color{40, 40, 50, 255});
    }
    const char* tips[7] = {"Your Pokedex", "Your party and your Box", "Items, Medicine, key items", "The Town Map - Fly to towns you've been to",
                           "Save your progress", "Sound, controls and display", "Back to the Story menu"};
    DrawBox(10, VH - 40, VW - 20 - bw - 10, 30, tips[std::min(6, o.menuSel)], false);
    if (g.coop) GDrawText(TextFormat("(%s)", WP(g, o.menuWho).name.c_str()), bx + 6, by + bh + 4, 10, UI_DIM);
}

void DrawTownMap(const Game& g) {
    const OwState& o = g.ow;
    DrawRectangle(0, 0, VW, VH, Fade(Color{16, 24, 40, 255}, 0.94f));
    GDrawText("TOWN MAP", 14, 10, 20, UI_GOLD);
    auto t = Towns();
    int sel = std::min(o.townSel, std::max(0, (int)t.size() - 1));
    const int mx = 14, my = 36, mw = 400, mh = 300;
    Profile pv = g.coop ? g.coopP[g.on.active ? g.on.me : 0] : g.profile;
    pv.storyStage = StoryStageNow(g);
    DrawStoryMap(pv, mx, my, mw, mh, g.t, false, -1, 0);
    int x = 424;
    std::string who;
    bool canFly = CanField(g, o.menuWho, "Fly", &who);
    GDrawText(canFly ? TextFormat("Fly with %s: pick a town", who.c_str()) : "Fly: needs the Thunder Badge", x, 40, 10, canFly ? Color{140, 230, 150, 255} : UI_DIM);
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
    if (g.ow.teamUp) DrawTeamTag(VW - 8);
    if (g.ow.w[0].msg.empty() && !g.ow.waitFight) GDrawText(kHints, 8, VH - 14, 10, Fade(UI_TEXT, 0.75f));
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
    if (m.indoor) return "You can't ride a Bicycle indoors!";
    if (w.surfing) return "You can't ride a Bicycle on the water!";
    w.biking = true;
    return "";
}
