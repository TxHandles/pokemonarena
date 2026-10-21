// The overworld (test build). Maps are the game's own pictures (World/*.png) with a hidden layer of
// rules from World/world.txt: which 16-pixel tiles are solid, tall grass or a ledge, where the
// doors, signs and people are, and how the maps join at their edges. Walking up to the story's
// trainers, into tall grass or through a Pokemon Center's door uses the same battles and screens
// as the menu.
#include "overworld.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
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
extern void (*gBattleStepHook)(const Battle&, float, const PlayerInput*);
void DrawScreenOf(const Game& g);

#ifdef ARENA_AUTOTEST
PlayerInput gOwTestIn[2];   // autotest: scripted walking for each walker
bool gOwTestOn = false;
#endif

namespace {

constexpr int T = 16;        // tile size in map pixels
constexpr float Z = 2.0f;    // map pixels -> screen units
constexpr float WALK = 0.24f, RUN = 0.13f;   // seconds per tile

struct OwNpc {
    int x = 0, y = 0, face = 0, sight = 0;
    int sprite = 0;
    std::string kind;                 // talk, trainer, heal, shop
    std::vector<std::string> args;    // talk: text; trainer: stage, before, after
};
struct OwLink { int map = -1; int off = 0; std::string msg; };
struct OwMap {
    std::string name, image, wild;
    bool indoor = false;
    int w = 0, h = 0;
    std::string cells;
    Texture2D tex{};
    OwLink link[4];                   // down (south), up (north), left (west), right (east)
    struct Act { int x, y; std::string act; };
    std::vector<Act> doors, spots;
    std::vector<OwNpc> npcs;
    int startX = -1, startY = -1;
    char At(int x, int y) const { return (x < 0 || y < 0 || x >= w || y >= h) ? '#' : cells[y * w + x]; }
};
struct NpcRt { int x, y, face; };     // where a person is right now (they turn and walk over)

std::vector<OwMap> gMaps;
std::vector<std::vector<NpcRt>> gNpc;
std::vector<std::string> gSpriteNames;
Texture2D gChars{};
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

// ---- the story's trainers
int TrainerStage(const OwNpc& n) { return n.kind == "trainer" && !n.args.empty() ? StageIndexByName(n.args[0]) : -1; }
bool TrainerReady(const Game& g, const OwNpc& n) {   // the story is at this battle now
    int s = TrainerStage(n);
    return s >= 0 && s == StoryStageNow(g) && s < (int)Story().stages.size();
}
bool TrainerBeaten(const Game& g, const OwNpc& n) {
    int s = TrainerStage(n);
    return s >= 0 && StoryStageNow(g) > s;
}
// Your rival only waits where the story has him now.
bool NpcHere(const Game& g, const OwNpc& n) {
    if (n.kind != "trainer") return true;
    if (gSpriteNames.size() > (size_t)n.sprite && gSpriteNames[n.sprite] == "rival") return TrainerReady(g, n);
    return true;
}

void Say(OwWalker& w, const std::string& text, const std::string& after = "") {
    w.msg = Split(text, '|');
    w.msgPage = 0;
    w.after = after;
    PlaySfx(Sfx::MenuMove);
}

void ResetNpcs(int map) {
    if (map < 0) return;
    auto& v = gNpc[map];
    v.clear();
    for (const auto& n : gMaps[map].npcs) v.push_back({n.x, n.y, n.face});
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
    OnlineSendWorldPos(true, w.map, w.x, w.y, w.face);
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
void StartStoryFight(Game& g, int i, int npc) {
    OwWalker& w = g.ow.w[i];
    const OwNpc& n = MapOf(w).npcs[npc];
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
    else if (n.kind == "heal") Say(w, line, "heal");
    else if (n.kind == "shop") Say(w, line, "shop");
    else if (n.kind == "trainer") {
        if (TrainerReady(g, n)) Say(w, n.args.size() > 1 ? n.args[1] : "Let's battle!", "battle:" + std::to_string(npc));
        else if (TrainerBeaten(g, n)) Say(w, n.args.size() > 2 ? n.args[2] : "You're strong!");
        else Say(w, "Let's battle another time!");
    }
}

void Warp(Game& g, int i, const std::vector<std::string>& a) {
    if (a.size() < 4) return;
    int m = MapIndex(a[1]);
    if (m < 0) return;
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
}

void AfterMessage(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    std::string a = w.after;
    w.after.clear();
    if (a.empty()) return;
    if (a.rfind("battle:", 0) == 0) StartStoryFight(g, i, std::atoi(a.c_str() + 7));
    else if (a == "joinfight") { OnlineSetReady(g, 1); g.ow.waitFight = true; }
    else if (a == "joinwild") { OnlineSetReady(g, 2); g.ow.waitFight = true; g.ow.wildBattle = true; g.ow.waitName = "the wild Pokemon"; }
    else if (a == "teamyes") { g.ow.teamUp = true; OnlineSendTeam(2); PlaySfx(Sfx::Buff); }
    else RunAction(g, i, a);
}

// Teamed up: a 2v2 wild battle for both players (your partner comes over). False: they're busy,
// so it's your own battle after all.
bool TeamWild(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    for (int p = 0; p < 2; p++) if (HealthyCount(Prof(g, p)) == 0) return false;
    if (Online(g)) {
        if (PartnerBattling(g) || !g.ow.w[1].here) return false;
        g.ow.wildBattle = true;
        g.ow.wildArea = m.wild;
        OnlineSendWorldFight(-1, w.map, w.x, w.y, "wild");
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
void WildEncounter(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    if (WildAreaIndex(m.wild) < 0 || HealthyCount(WP(g, i)) == 0) return;
    w.stepsSinceWild = 0;
    SaveAll(g);
    if (g.coop && g.ow.teamUp && TeamWild(g, i)) return;
    Game& t = g.coop ? StartSub(g, i) : g;
    if (t.profile.trainArea != m.wild) { t.profile.trainArea = m.wild; t.rareSpecies = -1; }
    t.ow.wildBattle = true;
    t.ow.wildArea = m.wild;
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
    if (i == 0) SendPos(g);
    if (CheckSight(g, i)) return;
    if (m.At(w.x, w.y) == 'g' && !m.wild.empty() && w.stepsSinceWild >= 3 && SimRand(0, 99) < 12) WildEncounter(g, i);
}

void TryStep(Game& g, int i, int dir) {
    OwWalker& w = g.ow.w[i];
    const OwMap& m = MapOf(w);
    w.face = dir;
    int nx = w.x + DX[dir], ny = w.y + DY[dir];
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
            char c = n.At(tx, ty);
            if (c == '#' || c == 'v' || WalkerAt(g, i, L.map, tx, ty)) { w.bumpT = 0.3f; return; }
            bool someoneThere = false;
            for (int k = 0; k < Driven(g); k++) if (k != i && g.ow.w[k].map == L.map) someoneThere = true;
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
            if (i == 0) SendPos(g);
            return;
        }
        if (!L.msg.empty() && w.bumpT <= 0) { RunAction(g, i, L.msg); w.bumpT = 0.5f; }
        return;
    }
    for (const auto& d : m.doors)
        if (d.x == nx && d.y == ny) { RunAction(g, i, d.act); return; }
    char c = m.At(nx, ny);
    bool hop = false;
    if (c == 'v') {
        if (dir != 0) { if (w.bumpT <= 0) { PlaySfx(Sfx::Thud); w.bumpT = 0.35f; } return; }
        hop = true;
        ny++;
        char c2 = m.At(nx, ny);
        if (c2 == '#' || c2 == 'v' || NpcAt(g, w.map, nx, ny) >= 0 || WalkerAt(g, i, w.map, nx, ny)) return;
    } else if (c == '#' || NpcAt(g, w.map, nx, ny) >= 0 || WalkerAt(g, i, w.map, nx, ny)) {
        if (w.bumpT <= 0) { PlaySfx(Sfx::Thud); w.bumpT = 0.35f; }
        return;
    }
    w.fFromX = w.fx; w.fFromY = w.fy;
    w.fx = w.x; w.fy = w.y;
    w.fFace = w.fx != w.fFromX ? (w.fx > w.fFromX ? 3 : 2) : (w.fy > w.fFromY ? 0 : w.fy < w.fFromY ? 1 : w.fFace);
    w.fromX = w.x; w.fromY = w.y;
    w.x = nx; w.y = ny;
    w.stepT = 0;
    w.hop = hop;
    w.walkAnim ^= 1;
    if (hop) PlaySfx(Sfx::MenuMove);
    if (i == 0) SendPos(g);
}

// This walker's buttons: solo/online the usual keys; local co-op each player's own layout.
struct WalkIn { int dir = -1; bool confirm = false, back = false, run = false; };
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
    if (!LocalCoop(g)
#ifdef ARENA_AUTOTEST
        && !gOwTestOn
#endif
    ) {   // Enter / Space / Esc as in the menus
        PlayerInput mi = MenuInput();
        r.confirm |= mi.confirm;
        r.back |= mi.back;
    }
#ifdef ARENA_AUTOTEST
    if (gOwTestOn) { gOwTestIn[i].confirm = gOwTestIn[i].back = false; }
#endif
    return r;
}

// Blacked out: home to Mom.
bool BlackOut(Game& g, int i) {
    OwWalker& w = g.ow.w[i];
    const Profile& p = WP(g, i);
    if (!w.msg.empty() || w.stepT < 1 || p.mons.empty() || HealthyCount(p) > 0) return false;
    int home = MapIndex("Your House 1F");
    if (home < 0 || w.map == home) return false;
    int x = 9, y = 6;
    if (WalkerAt(g, i, home, x, y)) y = 7;
    PlaceAt(g, i, home, x, y);
    w.face = 2;
    w.fadeT = 0.35f;
    SavePos(g);
    Say(w, "You hurried home to rest your Pokemon...|Mom: You look tired! Let your Pokemon rest a while.", "heal");
    return true;
}

void UpdateWalker(Game& g, int i, float dt, const WalkIn& in) {
    OwWalker& w = g.ow.w[i];
    w.bannerT = std::max(0.0f, w.bannerT - dt);
    w.fadeT = std::max(0.0f, w.fadeT - dt);
    w.bumpT = std::max(0.0f, w.bumpT - dt);
    if (BlackOut(g, i)) return;
    if (!w.msg.empty()) {   // a message box
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
            Say(w, n.args.size() > 1 ? n.args[1] : "Let's battle!", "battle:" + std::to_string(k));
            return;
        }
        if (w.approachT >= WALK) { w.approachT = 0; r.x = nx; r.y = ny; }
        return;
    }
    if (w.stepT < 1) {   // walking
        float dur = w.hop ? WALK * 1.6f : (w.running ? RUN : WALK);
        w.stepT = std::min(1.0f, w.stepT + dt / dur);
        if (w.stepT >= 1) { w.hop = false; Arrived(g, i); }
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
            if (s.x == fx && s.y == fy) { RunAction(g, i, s.act); return; }
        // across a counter: the Nurse or the clerk on the other side
        int idx2 = NpcAt(g, w.map, fx + DX[w.face], fy + DY[w.face]);
        if (idx2 >= 0 && m.At(fx, fy) == '#') { Interact(g, i, idx2); return; }
    }
    w.running = in.run;
    if (in.dir >= 0) TryStep(g, i, in.dir);
}

// Online: your partner's figure walks to where their game says they are.
void UpdateGhost(Game& g, float dt) {
    OwWalker& w = g.ow.w[1];
    if (!Online(g) || !w.here || w.stepT >= 1) return;
    w.stepT = std::min(1.0f, w.stepT + dt / WALK);
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
    if (o.inStage < 0) {   // teamed up: they ran into a wild Pokemon
        o.wildArea = o.inMap >= 0 && o.inMap < (int)gMaps.size() ? gMaps[o.inMap].wild : "";
        Say(w, g.coopP[1 - g.on.me].name + " found a wild Pokemon!|You run over to battle it together!", "joinwild");
    } else Say(w, g.coopP[1 - g.on.me].name + " was challenged by " + o.inTrainer + "!|You hurry over to help!", "joinfight");
}

const char* const kHints = "Arrows walk   Shift run   Enter talk/read   Esc menu";

} // namespace

// ---------------------------------------------------------------- loading

bool WorldAvailable() { return gLoaded && !gMaps.empty(); }

void LoadWorld(std::string& log) {
    for (auto& m : gMaps) if (m.tex.id) UnloadTexture(m.tex);
    gMaps.clear();
    gSpriteNames.clear();
    gLoaded = false;
    std::string dir = DataDir() + "World/";
    std::ifstream f(dir + "world.txt");
    if (!f) return;
    std::string line;
    OwMap* m = nullptr;
    bool grid = false;
    std::vector<std::pair<int, std::vector<std::string>>> links;   // resolved after every map is read
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) { grid = false; continue; }
        if (line[0] == '#' && !grid) continue;
        if (line == "[map]") { gMaps.emplace_back(); m = &gMaps.back(); grid = false; continue; }
        if (grid && m) {
            if (m->w == 0) m->w = (int)line.size();
            if ((int)line.size() == m->w) { m->cells += line; m->h++; }
            continue;
        }
        size_t eq = line.find(" = ");
        std::string k = Trim(line.substr(0, eq)), v = eq == std::string::npos ? "" : Trim(line.substr(eq + 3));
        if (k == "sprites") { std::istringstream ss(v); std::string s; while (ss >> s) gSpriteNames.push_back(s); continue; }
        if (!m) continue;
        if (line == "grid:") { grid = true; continue; }
        if (k == "name") m->name = v;
        else if (k == "image") m->image = v;
        else if (k == "wild") m->wild = v;
        else if (k == "indoor") m->indoor = v == "yes";
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
            (k == "door" ? m->doors : m->spots).push_back({x, y, act});
        } else if (k == "npc") {
            OwNpc p;
            char spr[32] = {}, face[16] = {};
            int n = 0;
            std::sscanf(v.c_str(), "%d %d %31s %15s %d %n", &p.x, &p.y, spr, face, &p.sight, &n);
            p.sprite = SpriteIndex(spr);
            p.face = FaceOf(face);
            auto parts = Split(v.substr(n), '|');
            p.kind = parts[0];
            p.args.assign(parts.begin() + 1, parts.end());
            if (p.kind == "talk" || p.kind == "heal" || p.kind == "shop") {   // the rest is one message
                std::string t;
                for (size_t i = 0; i < p.args.size(); i++) t += (i ? "|" : "") + p.args[i];
                p.args = {t};
            }
            m->npcs.push_back(p);
        }
    }
    for (auto& [mi, a] : links) {
        int d = a[0] == "south" ? 0 : a[0] == "north" ? 1 : a[0] == "west" ? 2 : 3;
        gMaps[mi].link[d].map = MapIndex(a[1]);
        gMaps[mi].link[d].off = std::atoi(a[2].c_str());
    }
    gNpc.assign(gMaps.size(), {});
    for (size_t i = 0; i < gMaps.size(); i++) ResetNpcs((int)i);
    for (auto& mm : gMaps) {
        mm.tex = LoadTex(dir + mm.image);
        if (!mm.tex.id) log += "World: missing " + mm.image + "\n";
    }
    gChars = LoadTex(dir + "chars.png");
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
    SendPos(g);
}

void WorldPartnerPos(Game& g, int map, int x, int y, int face) {
    OwWalker& w = g.ow.w[1];
    if (map < 0 || map >= (int)gMaps.size()) { w.here = false; return; }
    bool step = w.here && w.map == map && std::abs(x - w.x) + std::abs(y - w.y) <= 2 && (x != w.x || y != w.y);
    if (step) {
        w.fFromX = w.fx; w.fFromY = w.fy;
        w.fx = w.x; w.fy = w.y;
        w.fFace = w.fx != w.fFromX ? (w.fx > w.fFromX ? 3 : 2) : (w.fy > w.fFromY ? 0 : 1);
        w.fromX = w.x; w.fromY = w.y;
        w.stepT = 0;
        w.hop = std::abs(y - w.y) == 2;
        w.walkAnim ^= 1;
    } else {
        w.fromX = w.fx = w.fFromX = x;
        w.fromY = w.fy = w.fFromY = y;
        w.stepT = 1;
    }
    w.map = map;
    w.x = x; w.y = y; w.face = face & 3;
    w.here = true;
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
    bool menu = IsKeyPressed(KEY_ESCAPE);
#ifdef ARENA_AUTOTEST
    extern int gAutoMenu;
    if (gAutoMenu & 32) { gAutoMenu = 0; menu = true; }
#endif
    bool busy = false;
    for (int i = 0; i < Driven(g); i++) busy |= o.w[i].stepT < 1 || !o.w[i].msg.empty() || o.w[i].approach >= 0;
    busy |= AnySub(g);
    if (menu && !busy) { PlaySfx(Sfx::MenuBack); SaveAll(g); o.active = false; if (Online(g)) OnlineSendWorldPos(false, -1, 0, 0, 0); EnterStoryHub(g); return; }
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
void DrawChar(const View& c, int sprite, int face, int frame, float mx, float my, float lift = 0) {
    if (!gChars.id) return;
    static const int kCol[3] = {1, 0, 2};   // the sheet's columns: step, stand, step (frame 0 = standing still)
    int col = (face == 0 ? 0 : face == 1 ? 3 : 6) + kCol[frame < 0 || frame > 2 ? 0 : frame];
    Rectangle src{(float)col * 16, (float)sprite * 24, face == 3 ? -16.0f : 16.0f, 24};
    Vector2 p = Scr(c, mx, my + T - 24 - lift);
    DrawEllipse((int)(p.x + 8 * Z), (int)(Scr(c, mx, my + T - 1).y), 6 * Z, 2 * Z, Fade(BLACK, 0.25f));
    DrawTexturePro(gChars, src, {p.x, p.y, 16 * Z, 24 * Z}, {0, 0}, 0, WHITE);
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

void DrawView(const Game& g, int me, int vx, int vw) {
    const OwState& o = g.ow;
    const OwWalker& w = o.w[me];
    if (w.map < 0) return;
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
    c.x = std::round(c.x * Z) / Z;
    c.y = std::round(c.y * Z) / Z;
    DrawMapAt(m, c, 0, 0);
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
        bool follower = !(v.fx == v.x && v.fy == v.y && v.fFromX == v.fromX && v.fFromY == v.fromY);
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
            const OwWalker* who = nullptr;
            for (int k = 0; k < Driven(g); k++) if (o.w[k].map == w.map && o.w[k].approach == d.i) who = &o.w[k];
            if (who && who->approachT >= 0) {   // mid-step toward them
                float t = who->approachT / WALK;
                nx += DX[r.face] * T * t;
                ny += DY[r.face] * T * t;
                frame = t > 0.2f && t < 0.8f ? 1 + ((r.x + r.y) & 1) : 0;
            }
            DrawChar(c, n.sprite, r.face, frame, nx, ny);
            DrawGrassOver(m, c, r.x, r.y);
            if (who && who->approachT < 0) {   // "!"
                Vector2 p = Scr(c, nx + 4, ny - 20);
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
            DrawChar(c, WalkerSprite(g, d.i), v.face, frame, vx2, vy2, lift);
            if (vu >= 1) DrawGrassOver(m, c, v.x, v.y);
            if (g.coop && d.i != me) {   // your partner's name over their head (and a sword when they're battling)
                const std::string& nm = WP(g, d.i).name;
                Vector2 p = Scr(c, vx2 + 8, vy2 - 12);
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

    // the place name, the message box
    if (w.bannerT > 0) {
        float a = std::min(1.0f, w.bannerT / 0.4f);
        int bw = std::max(120, TextW(m.name.c_str(), 10) + 24);
        DrawPanel(vx + 8, 8 - (int)((1 - a) * 30), bw, 20, Fade(UI_PANEL, a), Fade(UI_BORDER, a));
        GDrawText(m.name.c_str(), vx + 20, 13 - (int)((1 - a) * 30), 10, Fade(UI_TEXT, a));
    }
    if (!w.msg.empty()) DrawBox(vx + 12, VH - 76, vw - 24, 64, w.msg[std::min(w.msgPage, (int)w.msg.size() - 1)], true);
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

void DrawWorld(const Game& g) {
    DrawRectangle(0, 0, VW, VH, BLACK);
    if (!WorldAvailable() || g.ow.w[0].map < 0) return;
    if (Driven(g) == 2) {   // local co-op: split screen
        DrawView(g, 0, 0, VW / 2 - 1);
        DrawView(g, 1, VW / 2 + 1, VW / 2 - 1);
        DrawRectangle(VW / 2 - 1, 0, 2, VH, UI_INK);
        if (g.ow.teamUp) DrawTeamTag(VW / 2 + 85);
        GDrawText(g.coopP[0].name.c_str(), 6, VH - 13, 10, UI_GOLD);
        GDrawText(g.coopP[1].name.c_str(), VW / 2 + 6, VH - 13, 10, Color{255, 150, 230, 255});
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
