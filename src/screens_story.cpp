// Story Mode: profiles, starter, hub, creature list, post-battle growth, evolution, moves, rewards.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "wild.h"
#include "settings.h"
#include "sprites.h"
#include "ui.h"
#include "rlgl.h"
#include "vec.h"
#include "storymap.h"
#include "leaders.h"
#include "net.h"
#include "legend.h"

namespace {

const MoveDef* LookupMove(const std::string& name, int species) {
    const MoveDef* d = FindMove(name);
    if (d || species < 0) return d;
    const SpeciesDef& sp = AllSpecies()[species];
    for (const auto& e : sp.learnset) if (e.move.name == name) return &e.move;
    for (const auto& m : sp.moves) if (m.name == name) return &m;
    return nullptr;
}

int NewMonLevel(const Profile& p) {
    int start = Story().startLevel;
    if (p.mons.empty()) return start;
    int sum = 0;
    for (const auto& m : p.mons) sum += m.level;
    return std::max(start, (int)std::lround((float)sum / p.mons.size()) - 2);
}

void DrawHeart(int x, int y, bool full) {
    Color c = full ? Color{255, 110, 150, 255} : Color{70, 60, 84, 255};
    DrawCircle(x + 2, y + 2, 2.2f, c);
    DrawCircle(x + 6, y + 2, 2.2f, c);
    DrawTriangle({(float)x - 0.2f, (float)y + 2.6f}, {(float)x + 4, (float)y + 7.5f}, {(float)x + 8.2f, (float)y + 2.6f}, c);
}

const char* FriendshipWord(int f) {
    if (f >= 220) return "Best friends";
    if (f >= 160) return "Very friendly";
    if (f >= 100) return "Friendly";
    if (f >= 50) return "Getting along";
    return "Wary of you";
}

void DrawFriendship(int f, int x, int y) {
    int hearts = (f + 25) / 51;  // 0-5
    for (int i = 0; i < 5; i++) DrawHeart(x + i * 11, y + 1, i < hearts);
    GDrawText(FriendshipWord(f), x + 58, y, 10, f >= 160 ? Color{255, 140, 180, 255} : UI_DIM);
}

void DrawXpBar(const OwnedMon& m, int x, int y, int w) {
    int lo = XpForLevel(m.level), hi = XpForLevel(m.level + 1);
    float f = m.level >= MAX_LEVEL ? 1.0f : (float)(m.xp - lo) / std::max(1, hi - lo);
    DrawRectangle(x, y, w, 3, Color{40, 40, 60, 255});
    DrawRectangle(x, y, (int)(w * std::max(0.0f, std::min(1.0f, f))), 3, Color{100, 180, 255, 255});
}

void SaveOrWarn(Game& g) {
    if (!SaveProfile(g.profile)) Toast(g, "Could not save the profile!");
}

const Color kCoin{255, 200, 60, 255};

// Season, time of day and steps until the next season (on the hub map).
void DrawTimePanel(int clock, int right, int y) {   // right-aligned at 'right'
    bool night = IsNight(clock);
    int s = SeasonOf(clock);
    Color sc = s == 0 ? Color{150, 230, 140, 255} : s == 1 ? Color{255, 200, 80, 255} : s == 2 ? Color{240, 140, 70, 255} : Color{190, 220, 255, 255};
    int left = StepsToNextSeason(clock);
    std::string l1 = TextFormat("%s %s", SeasonName(s), night ? "night" : "day");
    std::string l2 = TextFormat("%s in %d battle%s", SeasonName(s + 1), left, left == 1 ? "" : "s");
    int w = std::max(TextW(l1.c_str(), 10), TextW(l2.c_str(), 10)) + 36;
    int x = right - w;
    DrawPanel(x, y, w, 30, Fade(UI_INK, 0.85f), sc);
    if (night) { DrawCircle(x + 14, y + 15, 7, Color{235, 235, 210, 255}); DrawCircle(x + 17, y + 13, 6, UI_INK); }
    else {
        DrawCircle(x + 14, y + 15, 6, Color{255, 210, 80, 255});
        for (int k = 0; k < 8; k++) {
            float a = k * 0.785f;
            DrawLineEx({x + 14 + std::cos(a) * 8.0f, y + 15 + std::sin(a) * 8.0f}, {x + 14 + std::cos(a) * 10.0f, y + 15 + std::sin(a) * 10.0f}, 1, Color{255, 210, 80, 255});
        }
    }
    GDrawText(l1.c_str(), x + 28, y + 5, 10, sc);
    GDrawText(l2.c_str(), x + 28, y + 17, 10, UI_DIM);
}

void DrawCoins(int coins, int x, int y) {
    DrawCircle(x + 4, y + 5, 4.5f, Color{170, 120, 20, 255});
    DrawCircle(x + 4, y + 5, 3.5f, kCoin);
    DrawRectangle(x + 3, y + 3, 1, 3, Color{255, 240, 170, 255});
    GDrawText(TextFormat("%d", coins), x + 12, y + 1, 10, kCoin);
}

void DrawPokeBall(float x, float y, float r, float rot, Color top = Color{230, 60, 60, 255}) { ::DrawPokeBall(Vector2{x, y}, r, rot, 0, top); }

} // namespace

int HealthyCount(const Profile& p) {
    int n = 0;
    for (const auto& m : p.mons)
        if (!m.boxed && m.hp > 0 && MonSpecies(m) >= 0) n++;
    return n;
}

bool IsStoryMode(Mode m) { return m == Mode::Story || m == Mode::Training || m == Mode::CoopStory || m == Mode::CoopTraining; }

Profile& Prof(Game& g, int who) {
    if (!g.coop) return g.profile;
    who = who & 1;
    return g.coopActive == who ? g.profile : g.coopP[who];
}
const Profile& Prof(const Game& g, int who) {
    if (!g.coop) return g.profile;
    who = who & 1;
    return g.coopActive == who ? g.profile : g.coopP[who];
}

// ---- Training areas (wild.txt): any area the story has reached
int TrainStage(const Game& g) { return ProgressStageFor(g.coop ? g.coopSave.storyStage : g.profile.storyStage); }
int TrainBadges(const Game& g) { return (int)(g.coop ? g.coopSave.badges.size() : g.profile.badges.size()); }
int TrainArea(const Game& g) { return ResolveWildArea(g.coop ? g.coopSave.trainArea : g.profile.trainArea, TrainStage(g)); }
std::string TrainAreaName(const Game& g) {
    int a = TrainArea(g);
    return a >= 0 ? WildAreas()[a].name : std::string("the wild");
}
void CycleTrainArea(Game& g, int dir) {
    auto open = OpenWildAreas(TrainStage(g));
    if (open.empty()) return;
    int cur = TrainArea(g), pos = (int)open.size() - 1;
    for (int i = 0; i < (int)open.size(); i++) if (open[i] == cur) pos = i;
    pos = (pos + dir + (int)open.size()) % (int)open.size();
    std::string& slot = g.coop ? g.coopSave.trainArea : g.profile.trainArea;
    slot = WildAreas()[open[pos]].name;
    g.rareSpecies = -1;   // a rare find belongs to the area
    PlaySfx(Sfx::MenuMove);
}
bool SetTrainArea(Game& g, int area) {
    auto open = OpenWildAreas(TrainStage(g));
    if (std::find(open.begin(), open.end(), area) == open.end()) { Toast(g, "You can't train there yet."); return false; }
    if (g.on.active && g.on.me != 0) { Toast(g, "The host picks the training area."); return false; }
    std::string& slot = g.coop ? g.coopSave.trainArea : g.profile.trainArea;
    if (TrainArea(g) != area) g.rareSpecies = -1;   // a rare find belongs to the area
    slot = WildAreas()[area].name;
    if (g.on.active) OnlineSendCoop(g);
    else if (g.coop) SaveCoop(g.coopSave);
    else SaveOrWarn(g);
    Toast(g, "Training battles now happen in " + slot + ".");
    return true;
}
void SetTrainingWild(Game& g) {
    int area = g.ow.wildBattle && WildAreaIndex(g.ow.wildArea) >= 0 ? WildAreaIndex(g.ow.wildArea) : TrainArea(g);   // tall grass: that route's own
    SetWildContext(area, TrainStage(g), TrainBadges(g));
}
// A new rare find for the current training area (after the area changed on the team screen).
void RepickRare(Game& g) {
    SetTrainingWild(g);
    std::vector<std::string> owned;
    int sum = 0, n = 0;
    for (int p = 0; p < (g.coop ? 2 : 1); p++)
        for (const auto& m : Prof(g, p).mons) {
            owned.push_back(m.species);
            if (!m.boxed) { sum += m.level; n++; }
        }
    g.rareSpecies = PickRareSpecies(owned, n ? sum / n : 5);
}
int TrainingAreaIndex(const Game& g) { return TrainArea(g); }

void SaveWho(Game& g, int who) {
    if (!OnlineMine(g, who)) return;  // online: the partner's profile lives on their computer
    if (!SaveProfile(Prof(g, who))) Toast(g, "Could not save the profile!");
}

Profile ProgressView(const Game& g) {
    if (!g.coop) return g.profile;
    Profile v;
    v.name = g.coopSave.p[0] + " & " + g.coopSave.p[1];
    v.storyStage = g.coopSave.storyStage;
    v.badges = g.coopSave.badges;
    v.wins = g.coopSave.wins;
    v.losses = g.coopSave.losses;
    v.trainingWins = g.coopSave.trainingWins;
    v.clock = g.coopSave.clock;   // the map darkens at night
    for (const auto& m : Prof(g, 0).mons) if (!m.boxed) v.mons.push_back(m);
    return v;
}

int& StoryClock(Game& g) { return g.coop ? g.coopSave.clock : g.profile.clock; }
int StoryClock(const Game& g) { return g.coop ? g.coopSave.clock : g.profile.clock; }

// The rival picks their starter(s) once and keeps them for every battle (saved with the progress).
// Kanto: Blue, against your first starter. Johto: Silver, against Professor Elm's gift. Hoenn: May, against Birch's.
void EnsureRival(Game& g, int region) {
    auto starterOf = [&](const Profile& p) {
        int s = SpeciesIndex(RegionStarter(p, region));
        return s >= 0 || region != 0 ? s : ProfileStarter(p);   // old Kanto saves: the first Pokemon
    };
    bool gift = region >= 1;
    if (!g.coop) {
        std::string& rv = RegionRival(g.profile, region);
        if (SpeciesIndex(rv) >= 0) return;
        int mine = starterOf(g.profile);
        if (gift && mine < 0) return;
        int r = PickRivalSolo(mine, region);
        if (r < 0) return;
        rv = AllSpecies()[r].name;
        SaveWho(g, 0);
        return;
    }
    std::string* rv = CoopRival(g.coopSave, region);
    if (SpeciesIndex(rv[0]) >= 0) return;
    if (g.on.active && g.on.me != 0) return;   // online: the host picks (it reaches the guest with the co-op save)
    int a = starterOf(g.coopP[0]);
    int b = starterOf(g.coopP[1]);
    if (gift && (a < 0 || b < 0)) return;
    int out[2];
    PickRivalCoop(a, b, out, region);
    if (out[0] < 0) return;
    rv[0] = AllSpecies()[out[0]].name;
    rv[1] = out[1] >= 0 ? AllSpecies()[out[1]].name : std::string();
    if (g.on.active) OnlineSendCoop(g);
    else SaveCoop(g.coopSave);
}

int ProfileStarter(const Profile& p) {
    int i = SpeciesIndex(p.starter);
    if (i >= 0) return i;
    for (const auto& m : p.mons) {   // older saves: the first creature from a starter line
        int b = BaseFormOf(MonSpecies(m));
        for (const auto& s : Story().starters)
            if (b >= 0 && SpeciesIndex(s) == b) return b;
    }
    return -1;
}

void StoryTimeStep(Game& g, bool announce) {
    int& c = StoryClock(g);
    int before = SeasonOf(c);
    c++;
    if (!announce) return;
    if (SeasonOf(c) != before) Toast(g, std::string("The season changed - it's ") + SeasonName(SeasonOf(c)) + "!");
    else Toast(g, IsNight(c) ? "Night fell." : "The sun came up.");
}

int BadgeCount(const Game& g) { return (int)(g.coop ? g.coopSave.badges.size() : g.profile.badges.size()); }

bool StoryDone(const Game& g) {
    int st = g.coop ? g.coopSave.storyStage : g.profile.storyStage;
    return !Story().stages.empty() && st >= (int)Story().stages.size();
}
int CurStage(const Game& g) { return g.coop ? g.coopSave.storyStage : g.profile.storyStage; }
bool RegionDone(const Game& g, int r) {
    if (r >= RegionCount()) return false;
    const auto& R = RegionInfo(r);
    return R.end > R.first && CurStage(g) >= R.end;
}
bool KantoDone(const Game& g) { return RegionCount() > 1 ? RegionDone(g, RegionOrder().empty() ? 0 : RegionOrder()[0]) : StoryDone(g); }
int RegionBadgeCount(const Game& g, int region) {
    const auto& have = g.coop ? g.coopSave.badges : g.profile.badges;
    int n = 0;
    for (const auto& s : Story().stages)
        if (s.region == region && s.IsGym() && std::find(have.begin(), have.end(), s.badge) != have.end()) n++;
    return n;
}
int NeedsRegionStarter(const Game& g) {
    int cur = CurStage(g);
    if (cur >= (int)Story().stages.size() || BetweenRegions(cur)) return -1;
    int r = ActiveRegion(cur);
    if (!RegionChosen(r)) return -1;
    auto missing = [&](const Profile& p) {
        if (!RegionStarter(p, r).empty()) return false;
        if (r == 0 && ProfileStarter(p) >= 0 && RegionSlot(0) == 0) return false;   // an old Kanto save: its first Pokemon
        return true;
    };
    if (g.coop) return (missing(g.coopP[0]) || missing(g.coopP[1])) ? r : -1;
    return missing(g.profile) ? r : -1;
}
std::vector<int> StarterPool(const Game& g) {
    if (g.regionPick) return RegionStarters(PickedRegion(g));
    if (g.coop || g.screen == Screen::CoopStarter) return RegionStarters(g.journeyRegion);
    return RegionStarters(0);
}
std::vector<int> RegionsLeft(const Game&) {
    std::vector<int> v;
    for (int r = 0; r < RegionCount(); r++)
        if (!RegionChosen(r) && RegionInfo(r).end > RegionInfo(r).first) v.push_back(r);
    return v;
}
bool CanChooseRegion(const Game& g) { return BetweenRegions(CurStage(g)) && !RegionsLeft(g).empty(); }
void ApplySaveOrder(Game& g) {
    std::vector<int>& order = g.coop ? g.coopSave.regionOrder : g.profile.regionOrder;
    int stage = g.coop ? g.coopSave.storyStage : g.profile.storyStage;
    bool started = g.coop ? (stage > 0 || g.coopSave.wins + g.coopSave.losses + g.coopSave.trainingWins > 0) : (!g.profile.mons.empty() || stage > 0);
    if (order.empty() && started) {   // a save from before region order (or made without one): the usual order, as far as it got
        ApplyRegionOrder({});
        if (g.coop) order = DeriveRegionOrder(stage, {true, !g.coopSave.rival2[0].empty(), !g.coopSave.rival3[0].empty()});
        else order = DeriveRegionOrder(stage, {true, !g.profile.starter2.empty(), !g.profile.starter3.empty()});
    }
    ApplyRegionOrder(order);
}
int RegionGiftLevel(const Game& g, int region, int who) {
    const auto& R = RegionInfo(region);
    if (R.startAvg == 0) return R.startLevel;
    const Profile& p = g.coop ? g.coopP[who & 1] : g.profile;
    int sum = 0, n = 0;
    for (const auto& m : p.mons) if (!m.boxed) { sum += m.level; n++; }
    int avg = n ? sum / n : R.startLevel;
    return std::max(R.startLevel, std::min(MAX_LEVEL, avg + R.startAvg));
}
void BeginRegionStarter(Game& g) {
    g.regionPick = std::max(0, NeedsRegionStarter(g)) + 1;
    g.regionRevealT = -1;
    if (g.coop) {
        g.coopReady[0] = g.coopReady[1] = false;
        g.coopStarterSel[0] = 0;
        g.coopStarterSel[1] = std::min(1, std::max(0, (int)StarterPool(g).size() - 1));
        g.screen = Screen::CoopStarter;
    } else {
        g.starterSel = 0;
        g.screen = Screen::Starter;
    }
}

bool PostgameBeaten(const Game& g, int stage) {
    const auto& v = Story().stages;
    if (stage < 0 || stage >= (int)v.size() || !v[stage].postgame) return false;
    const auto& done = g.coop ? g.coopSave.postDone : g.profile.postDone;
    return done.count(v[stage].name) > 0;
}

uint32_t TowerSeedFor(const Game& g) {
    std::string key = g.coop ? g.coopSave.p[0] + "&" + g.coopSave.p[1] : g.profile.name;
    int runs = g.coop ? g.coopSave.towerRuns : g.profile.towerRuns;
    uint32_t h = 2166136261u;
    for (char c : key) { h ^= (uint8_t)c; h *= 16777619u; }
    return h ^ ((uint32_t)runs * 0x9E3779B9u);
}

void BeginTowerFloor(Game& g, int floor, uint32_t seed) {
    g.tower = true;
    g.towerFloor = std::max(1, floor);
    g.towerSeed = seed;
    g.stageIdx = -1;
    g.mode = g.coop ? Mode::CoopStory : Mode::Story;
    g.rareBattle = false;
    g.screen = Screen::StageIntro;
}

StageDef ActiveStage(const Game& g) {
    if (g.legend >= 0) return LegendStage(g.legend);
    if (g.tower) return TowerStage(g.towerFloor, g.towerSeed);
    const auto& stages = Story().stages;
    if (stages.empty()) return StageDef();
    StageDef st = stages[std::max(0, std::min((int)stages.size() - 1, g.stageIdx))];
    if (IsRematch(g) && st.IsBoss()) st = RematchStage(st);
    return st;
}

namespace {
// Where the League run stands: how many of the five are beaten in this run (-1: not in the League yet).
int LeagueProgress(int storyStage) {
    int start = LeagueStartStage(StageRegion(storyStage));
    if (start < 0 || storyStage < start) return -1;
    return storyStage - start;
}
std::string NextLine(const Game& g, int storyStage) {
    const auto& stages = Story().stages;
    if (storyStage >= (int)stages.size()) {
        int best = g.coop ? g.coopSave.towerBest : g.profile.towerBest;
        return best > 0 ? TextFormat("Champion! Battle Tower record: floor %d", best) : "Champion! The Battle Tower is open - how high can you climb?";
    }
    if (CanChooseRegion(g))
        return TextFormat("%s Champion! Choose where your journey goes next from the menu.", RegionInfo(ActiveRegion(storyStage)).name.c_str());
    if (int r = NeedsRegionStarter(g); r >= 0)
        return TextFormat("A new journey awaits in %s - travel there from the menu.", RegionInfo(r).name.c_str());
    const StageDef& st = stages[storyStage];
    std::string l = "Next: " + st.trainer + "  -  " + st.name;
    int lp = LeagueProgress(storyStage);
    if (lp > 0) l += TextFormat("   (League run: %d of 5 beaten)", lp);
    return l;
}
} // namespace

void OpenHealingCenter(Game& g) {
    g.healSel = 0;
    g.healReturn = g.screen == Screen::Heal ? Screen::StoryHub : g.screen;
    g.screen = Screen::Heal;
}

void EnterStoryHub(Game& g) {
    ApplySaveOrder(g);   // the story laid out in this save's region order
    g.tower = false;
    g.legend = -1;
    if (g.coop) {
        if (g.coopActive >= 0) {  // done with one player's Shop / Healing Center / Party & Box
            g.coopP[g.coopActive] = g.profile;
            g.coopActive = -1;
            OnlineSendProfile(g);
        }
        g.mode = Mode::CoopStory;
        g.ow.wildBattle = false;
        if (g.ow.active) { g.screen = Screen::World; return; }   // walking around: back on the map
        g.hubSel = 0;
        g.screen = Screen::CoopHub;
        return;
    }
    g.mode = Mode::Story;
    g.ow.wildBattle = false;
    if (g.ow.active) { g.screen = Screen::World; g.ow.w[0].fadeT = 0.35f; return; }   // walking around: back on the map
    g.hubSel = 0;
    g.screen = Screen::StoryHub;
}

// ------------------------------------------------------------------ profile select / new profile

void UpdateProfileSelect(Game& g) {
    PlayerInput in = MenuInput();
    int n = (int)g.profiles.size() + 1;  // + "new profile"
    if (in.up) { g.profSel = (g.profSel + n - 1) % n; g.confirmDelete = false; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.profSel = (g.profSel + 1) % n; g.confirmDelete = false; PlaySfx(Sfx::MenuMove); }
    if (in.back) { PlaySfx(Sfx::MenuBack); GoTitle(g); return; }
    bool del = IsKeyPressed(KEY_DELETE) || in.swap;
    if (del && g.profSel < (int)g.profiles.size()) {
        if (!g.confirmDelete) { g.confirmDelete = true; PlaySfx(Sfx::MenuMove); }
        else {
            DeleteProfile(g.profiles[g.profSel]);
            Toast(g, "Deleted " + g.profiles[g.profSel]);
            g.profiles = ListProfiles();
            g.profSel = 0;
            g.confirmDelete = false;
            PlaySfx(Sfx::MenuBack);
        }
        return;
    }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    if (g.profSel == (int)g.profiles.size()) {
        g.nameBuf.clear();
        while (GetCharPressed()) {}
        g.screen = Screen::NameEntry;
        return;
    }
    std::string problems;
    if (!LoadProfile(g.profiles[g.profSel], g.profile, &problems)) { Toast(g, "Could not load that profile"); return; }
    if (!problems.empty()) Toast(g, "Some Pokemon in this save are missing from the Pokemon folder");
    if (ProfileRoster(g.profile).empty()) {
        BeginRegionChoose(g, 0);   // a story not started yet: where it begins, then the first partner
    } else {
        EnterStoryHub(g);
    }
}

void DrawProfileSelect(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    DrawTextCentered("STORY MODE", VW / 2, 16, 30, UI_GOLD);
    DrawTextCentered("Choose a profile. Each one keeps its own Pokemon, levels and story progress.", VW / 2, 52, 10, UI_DIM, false);
    int n = (int)g.profiles.size();
    for (int i = 0; i <= n; i++) {
        int y = 76 + i * 26;
        if (y > 300) break;
        bool sel = g.profSel == i;
        DrawPanel(VW / 2 - 150, y, 300, 22, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
        if (i < n) {
            Profile p;
            LoadProfile(g.profiles[i], p);
            GDrawText(p.name.c_str(), VW / 2 - 140, y + 6, 10, sel ? UI_GOLD : UI_TEXT);
            const char* info = TextFormat("%d Pokemon   stage %d/%d   %dW %dL", (int)p.mons.size(),
                                          std::min(p.storyStage, (int)Story().stages.size()), (int)Story().stages.size(), p.wins, p.losses);
            GDrawText(info, VW / 2 + 140 - TextW(info, 10), y + 6, 10, UI_DIM);
        } else {
            GDrawText("+ New profile", VW / 2 - 140, y + 6, 10, sel ? UI_GOLD : Color{140, 220, 150, 255});
        }
    }
    if (g.confirmDelete) DrawTextCentered("Press Delete / Swap again to delete this profile for good", VW / 2, 316, 10, Color{255, 120, 100, 255}, false);
    GDrawText("Enter choose   Esc back   Delete (or your Swap key) deletes a profile", 20, 340, 10, UI_DIM);
    DrawToast(g);
}

void UpdateNameEntry(Game& g) {
    int c;
    while ((c = GetCharPressed()) != 0) {
        if (g.nameBuf.size() < 14 && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_'))
            g.nameBuf += (char)c;
    }
    if (IsKeyPressed(KEY_BACKSPACE) && !g.nameBuf.empty()) g.nameBuf.pop_back();
    if (IsKeyPressed(KEY_ESCAPE)) { g.screen = g.netNewTrainer ? Screen::NetMenu : Screen::ProfileSelect; g.netNewTrainer = false; return; }
    bool ok = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
    // Gamepad users: A on an empty name picks a default.
    if (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
        if (g.nameBuf.empty()) g.nameBuf = TextFormat("Player %d", (int)g.profiles.size() + 1);
        ok = true;
    }
    if (!ok) return;
    while (!g.nameBuf.empty() && g.nameBuf.back() == ' ') g.nameBuf.pop_back();
    if (g.nameBuf.empty()) { Toast(g, "Type a name first"); return; }
    if (ProfileExists(g.nameBuf)) { Toast(g, "That name is taken"); return; }
    g.profile = Profile();
    g.profile.name = g.nameBuf;
    g.profile.bag["Poke Ball"] = START_BALLS;
    SaveOrWarn(g);
    PlaySfx(Sfx::MenuOk);
    if (g.netNewTrainer) {   // online co-op: back to the online menu with the new trainer chosen
        g.netNewTrainer = false;
        g.profiles = ListProfiles();
        for (int i = 0; i < (int)g.profiles.size(); i++) if (g.profiles[i] == g.profile.name) g.on.profSel = i;
        g.screen = Screen::NetMenu;
        Toast(g, "You'll pick your starter together once you're connected.");
        return;
    }
    BeginRegionChoose(g, 0);
}

void DrawNameEntry(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    DrawTextCentered("NEW PROFILE", VW / 2, 60, 30, UI_GOLD);
    DrawTextCentered("Type your trainer name", VW / 2, 110, 10, UI_TEXT, false);
    DrawPanel(VW / 2 - 120, 130, 240, 30, UI_PANEL_LIGHT, UI_GOLD);
    std::string shown = g.nameBuf + ((((int)(g.t * 2)) % 2) ? "_" : " ");
    DrawTextCentered(shown.c_str(), VW / 2, 138, 20, UI_TEXT, false);
    DrawTextCentered("Enter to confirm   Esc to cancel   (gamepad: A uses a default name)", VW / 2, 176, 10, UI_DIM, false);
    DrawToast(g);
}

// ------------------------------------------------------------------ starter

void UpdateStarter(Game& g) {
    PlayerInput in = MenuInput();
    std::vector<int> pool = StarterPool(g);
    int n = (int)pool.size();
    if (n == 0) { EnterStoryHub(g); return; }
    g.starterSel = std::max(0, std::min(n - 1, g.starterSel));
    if (in.left) { g.starterSel = (g.starterSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.right) { g.starterSel = (g.starterSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (g.regionPick) {   // the professor's gift on arriving in a region (Elm in Johto, Birch in Hoenn) - or a new story's first partner
        int r = PickedRegion(g);
        if (g.regionRevealT >= 0) {   // the rival has taken theirs: on to the region
            g.regionRevealT += GetFrameTime();
            if (g.regionRevealT > 2.5f && (in.confirm || in.back)) { g.regionPick = 0; g.regionRevealT = -1; g.newJourney = false; EnterStoryHub(g); }
            return;
        }
        if (in.back) {
            g.regionPick = 0;
            if (g.newJourney) { BeginRegionChoose(g, 0); return; }
            EnterStoryHub(g);
            return;
        }
        if (!in.confirm) return;
        int idx = pool[g.starterSel];
        bool boxed = AddMon(g.profile, NewMon(idx, RegionGiftLevel(g, r)));
        RegionStarter(g.profile, r) = AllSpecies()[idx].name;
        RegionRival(g.profile, r).clear();
        DexRegisterOwned(g.profile);
        EnsureRival(g, r);   // the rival takes one of the starters that beat yours
        SaveOrWarn(g);
        PlaySfx(Sfx::LevelUp);
        Toast(g, AllSpecies()[idx].name + (boxed ? " was sent to your Box." : " joined your team!"));
        g.regionRevealT = 0;
        return;
    }
    if (in.back) { BeginRegionChoose(g, 0); return; }
    if (!in.confirm) return;
    int idx = pool[g.starterSel];
    g.profile.mons.push_back(NewMon(idx, Story().startLevel));
    g.profile.starter = AllSpecies()[idx].name;
    g.profile.rival.clear();
    if (g.profile.regionOrder.empty()) g.profile.regionOrder = {0};
    ApplySaveOrder(g);
    EnsureRival(g);   // the rival picks right away, against your choice
    SaveOrWarn(g);
    PlaySfx(Sfx::LevelUp);
    Toast(g, AllSpecies()[idx].name + " joined your team!");
    EnterStoryHub(g);
}

namespace {
std::string Fill(std::string s, const std::string& key, const std::string& val) {
    for (size_t p = s.find(key); p != std::string::npos; p = s.find(key, p + val.size())) s.replace(p, key.size(), val);
    return s;
}
std::string Upper(std::string s) { for (char& c : s) c = (char)std::toupper((unsigned char)c); return s; }
Color RivalColor(int region) { return region >= 2 ? Color{240, 130, 70, 255} : Color{230, 70, 80, 255}; }
} // namespace

std::string RegionRivalName(int r) {
    const auto& R = RegionInfo(r);
    return !R.rivalName.empty() ? R.rivalName : r == 1 ? "Silver" : "Your rival";
}
std::string RegionWatchLine(int r, bool coop, int left) {
    const auto& R = RegionInfo(r);
    static const char* kWords[] = {"none", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten"};
    std::string n = left >= 0 && left <= 10 ? kWords[left] : std::to_string(left);
    if (coop) {
        std::string w = !R.watchCoop.empty() ? R.watchCoop : "A red-haired boy lurks by the lab window... he'll grab what you two leave behind - and one more.";
        return Fill(w, "{left}", n);
    }
    return !R.watch.empty() ? R.watch : "A red-haired boy is watching the lab window... he'll take one that beats yours.";
}
std::string RegionIntroLine(int r, const std::string& who, int level) {
    const auto& R = RegionInfo(r);
    std::string w = !R.intro.empty() ? R.intro : "Professor Elm in New Bark Town has a Pokemon for you, {name}! It starts at level {level}.";
    return Fill(Fill(w, "{name}", who), "{level}", std::to_string(level));
}

void DrawRivalReveal(const Game& g, const std::vector<std::string>& yours, const std::vector<std::string>& his) {
    int region = std::max(0, PickedRegion(g));
    const auto& R = RegionInfo(region);
    Color rc = RivalColor(region);
    float u = std::max(0.0f, g.regionRevealT);
    DrawRectangle(0, 66, VW, 270, Fade(UI_INK, 0.75f));
    // yours, under a calm light
    int n = 0;
    for (const auto& s : yours) n += SpeciesIndex(s) >= 0;
    int k = 0;
    for (const auto& s : yours) {
        int idx = SpeciesIndex(s);
        if (idx < 0) continue;
        int cx = 150 + (n > 1 ? (k == 0 ? -50 : 50) : 0);
        DrawEllipse(cx, 220, 40, 9, Fade(BLACK, 0.4f));
        DrawSpeciesFit(idx, VIEW_FRONT, cx, 218, 76);
        DrawTextCentered(AllSpecies()[idx].name.c_str(), cx, 228, 10, UI_TEXT, false);
        k++;
    }
    DrawTextCentered(n > 1 ? "Your partners" : "Your partner", 150, 90, 10, UI_GOLD, false);
    // theirs, slammed in with the rival's colour
    float in = std::min(1.0f, u / 0.5f);
    float e = 1 - (1 - in) * (1 - in) * (1 - in);
    int m = 0;
    for (const auto& s : his) m += SpeciesIndex(s) >= 0;
    k = 0;
    for (const auto& s : his) {
        int idx = SpeciesIndex(s);
        if (idx < 0) continue;
        int cx = 490 + (m > 1 ? (k == 0 ? -50 : 50) : 0) + (int)((1 - e) * 220);
        float pulse = 0.5f + 0.5f * std::sin(g.t * 5 + k);
        DrawCircleGradient(cx, 186, 52, Fade(rc, (0.3f + 0.15f * pulse) * e), Fade(rc, 0));
        DrawEllipse(cx, 220, 40, 9, Fade(BLACK, 0.4f));
        DrawSpeciesFit(idx, VIEW_FRONT, cx, 218, 76);
        DrawTextCentered(AllSpecies()[idx].name.c_str(), cx, 228, 10, ColorBrightness(rc, 0.45f), false);
        k++;
    }
    DrawTextCentered(Upper(RegionRivalName(region)).c_str(), 490 + (int)((1 - e) * 220), 84, 20, rc);
    DrawTextCentered("VS", VW / 2, 170, 30, Fade(UI_GOLD, e));
    std::string names;
    for (size_t i = 0; i < his.size(); i++) if (!his[i].empty()) names += (names.empty() ? "" : " and ") + his[i];
    if (u > 0.6f) {
        std::string what = !R.reveal.empty() ? Fill(R.reveal, "{mons}", names) : "A boy with red hair broke into Professor Elm's lab and stole " + names + "!";
        std::string quote = !R.revealQuote.empty() ? R.revealQuote : "...What are you staring at? This one's strong - it beats yours. Remember the name: Silver.";
        DrawPanel(30, 252, 580, 50, UI_PANEL, rc);
        DrawTextCentered(what.c_str(), VW / 2, 260, 10, UI_TEXT, false);
        DrawTextCentered(("\"" + quote + "\"").c_str(), VW / 2, 278, 10, ColorBrightness(rc, 0.55f), false);
    }
    if (u > 2.5f) DrawTextCentered(("Enter: on to " + R.name + "!").c_str(), VW / 2, 318, 10, UI_GOLD, false);
    DrawToast(g);
}

void DrawStarter(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    int lvl = g.regionPick ? RegionGiftLevel(g, PickedRegion(g)) : Story().startLevel;
    std::vector<int> pool = StarterPool(g);
    if (g.regionPick) {
        int r = PickedRegion(g);
        DrawTextCentered(("WELCOME TO " + Upper(RegionInfo(r).name)).c_str(), VW / 2, 14, 20, UI_GOLD);
        DrawTextCentered(RegionIntroLine(r, g.profile.name, lvl).c_str(), VW / 2, 40, 10, UI_TEXT, false);
        DrawTextCentered(RegionWatchLine(r, false, 0).c_str(), VW / 2, 54, 10, UI_DIM, false);
        if (g.regionRevealT >= 0) { DrawRivalReveal(g, {RegionStarter(g.profile, r)}, {RegionRival(g.profile, r)}); return; }
    } else {
        DrawTextCentered("CHOOSE YOUR FIRST PARTNER", VW / 2, 20, 20, UI_GOLD);
        DrawTextCentered(TextFormat("Welcome, %s! Your partner starts at level %d.", g.profile.name.c_str(), Story().startLevel), VW / 2, 46, 10, UI_TEXT, false);
    }
    int n = (int)pool.size();
    int gap = n > 3 ? 8 : 20;
    int cw = std::min(150, (620 - (n - 1) * gap) / std::max(1, n));
    int x0 = VW / 2 - (n * cw + (n - 1) * gap) / 2;
    for (int i = 0; i < n; i++) {
        int idx = pool[i];
        int x = x0 + i * (cw + gap);
        DrawCreatureCard(idx, lvl, x, 70, cw, 130, g.starterSel == i);
    }
    int idx = g.starterSel >= 0 && g.starterSel < n ? pool[g.starterSel] : -1;
    if (idx >= 0) {
        const SpeciesDef& sp = AllSpecies()[idx];
        DrawPanel(120, 212, 400, 120);
        auto moves = MovesAtLevel(sp, lvl);
        GDrawText("Starts with:", 130, 220, 10, UI_DIM);
        for (size_t k = 0; k < moves.size(); k++) DrawMoveLine(moves[k], 130, 234 + (int)k * 22, 380, true);
        std::string hint = EvolutionHint(idx);
        if (!hint.empty()) GDrawText(hint.c_str(), 130, 318, 10, UI_GOLD);
    }
    DrawTextCentered(g.regionPick && !g.newJourney ? "Left/Right choose   Enter confirm   Esc: not yet" : "Left/Right choose   Enter confirm   Esc: back", VW / 2, 342, 10, UI_DIM, false);
    DrawToast(g);
}

// ------------------------------------------------------------------ hub

namespace {
bool NextTrainingIsRare(int trainingWins) {
    int every = std::max(1, Story().rareEvery);
    return (trainingWins + 1) % every == 0;
}
bool NextTrainingIsRare(const Profile& p) { return NextTrainingIsRare(p.trainingWins); }
} // namespace

// The hub's top-left icons: the Settings gear (and, in co-op, the Leaderboard graph). They sit
// above the menu: Up from the first item reaches them, Left/Right moves between them.
bool HubNav(Game& g, const PlayerInput& in, int n, bool graph) {
    if (g.hubSel >= HUB_GEAR) {
        if (graph && (in.left || in.right)) { g.hubSel = g.hubSel == HUB_GEAR ? HUB_GRAPH : HUB_GEAR; PlaySfx(Sfx::MenuMove); return true; }
        if (in.down) { g.hubSel = 0; PlaySfx(Sfx::MenuMove); return true; }
        if (in.up) { g.hubSel = n - 1; PlaySfx(Sfx::MenuMove); return true; }
        return false;
    }
    if (in.up) { g.hubSel = g.hubSel == 0 ? HUB_GEAR : g.hubSel - 1; PlaySfx(Sfx::MenuMove); return true; }
    if (in.down) { g.hubSel = g.hubSel >= n - 1 ? HUB_GEAR : g.hubSel + 1; PlaySfx(Sfx::MenuMove); return true; }
    return false;
}

void DrawHubIcon(int kind, int x, int y, bool sel, float t) {
    Color bg = sel ? UI_PANEL_LIGHT : UI_PANEL, edge = sel ? UI_GOLD : Color{70, 64, 92, 255}, ic = sel ? UI_GOLD : UI_TEXT;
    DrawPanel(x, y, 18, 16, bg, edge);
    float cx = x + 9.0f, cy = y + 8.0f;
    if (kind == 0) {   // a gear
        float rot = sel ? t * 90 : 0;
        for (int k = 0; k < 8; k++) {
            float a = (k * 45 + rot) * DEG2RAD;
            Vector2 c{cx + std::cos(a) * 5.0f, cy + std::sin(a) * 5.0f};
            DrawRectanglePro({c.x, c.y, 3, 3}, {1.5f, 1.5f}, k * 45 + rot, ic);
        }
        DrawCircleV({cx, cy}, 4.6f, ic);
        DrawCircleV({cx, cy}, 2.0f, bg);
    } else {           // a bar graph
        DrawRectangle(x + 4, y + 9, 3, 4, ic);
        DrawRectangle(x + 8, y + 6, 3, 7, ic);
        DrawRectangle(x + 12, y + 3, 3, 10, ic);
        DrawRectangle(x + 3, y + 13, 13, 1, ic);
    }
    if (sel) {
        const char* tip = kind == 0 ? "Settings" : "Leaderboard";
        int tw = TextW(tip, 10);
        DrawRectangleRounded({(float)x - 3, (float)y + 17, (float)tw + 6, 12}, 0.4f, 4, Fade(UI_INK, 0.92f));
        DrawRectangleRoundedLinesEx({(float)x - 3, (float)y + 17, (float)tw + 6, 12}, 0.4f, 4, 1, UI_GOLD);
        GDrawText(tip, x, y + 18, 10, UI_GOLD);
    }
}

// ------------------------------------------------------------------ story cards

namespace { double gCardShown = 0; }
void ShowStoryCard(Game& g, const std::string& tt, const std::string& mon) {
    size_t bar = tt.find('|');
    auto trim = [](std::string v) {
        size_t a = v.find_first_not_of(" \t"), b = v.find_last_not_of(" \t");
        return a == std::string::npos ? std::string() : v.substr(a, b - a + 1);
    };
    g.cardTitle = bar == std::string::npos ? std::string("Meanwhile...") : trim(tt.substr(0, bar));
    g.cardText = trim(bar == std::string::npos ? tt : tt.substr(bar + 1));
    g.cardMon = mon;
    gCardShown = GetTime();
}
bool StoryCardUp(const Game& g) { return !g.cardTitle.empty(); }
bool UpdateStoryCard(Game& g) {
    if (!StoryCardUp(g)) return false;
    PlayerInput in = MenuInput();
    if (GetTime() - gCardShown > 0.5 && (in.confirm || in.back)) {
        PlaySfx(Sfx::MenuOk);
        g.cardTitle.clear();
        g.cardText.clear();
        g.cardMon.clear();
    }
    return true;
}
void DrawStoryCard(const Game& g) {
    if (!StoryCardUp(g)) return;
    float u = std::min(1.0f, (float)(GetTime() - gCardShown) / 0.35f);
    DrawRectangle(0, 0, VW, VH, Fade(BLACK, 0.72f * u));
    int sp = SpeciesIndex(g.cardMon);
    int w = 540, h = 250, x = VW / 2 - w / 2, y = VH / 2 - h / 2 + (int)((1 - u) * 20);
    DrawPanel(x, y, w, h, UI_PANEL, UI_GOLD);
    DrawRectangle(x + 10, y + 34, w - 20, 1, Fade(UI_GOLD, 0.6f));
    DrawTextCentered(g.cardTitle.c_str(), VW / 2, y + 12, 20, UI_GOLD);
    int tw = sp >= 0 ? w - 170 : w - 40;
    if (sp >= 0) {   // the Pokemon the page is about, glowing on the right
        int cx = x + w - 80, cy = y + 170;
        float p = 0.5f + 0.5f * std::sin(g.t * 2.2f);
        DrawCircleGradient(cx, cy - 50, 70, Fade(UI_GOLD, 0.18f + 0.08f * p), Fade(UI_GOLD, 0));
        DrawEllipse(cx, cy + 2, 46, 9, Fade(BLACK, 0.45f));
        DrawSpeciesFit(sp, VIEW_FRONT, cx, cy, 120, Fade(WHITE, u));
        DrawTextCentered(AllSpecies()[sp].name.c_str(), cx, cy + 12, 10, UI_TEXT, false);
    }
    DrawWrapped(g.cardText.c_str(), x + 20, y + 46, tw, 10, UI_TEXT);
    if (GetTime() - gCardShown > 0.5) DrawTextCentered("Enter: continue", VW / 2, y + h - 18, 10, UI_DIM, false);
}

void UpdateStoryHub(Game& g) {
    if (UpdateStoryCard(g)) return;
    PlayerInput in = MenuInput();
    const int N = 12;
    if (HubNav(g, in, N, false)) return;
    if (g.hubSel == 1 && (in.left || in.right)) { CycleTrainArea(g, in.left ? -1 : 1); SaveOrWarn(g); return; }
    if (g.hubSel == 0 && (in.left || in.right) && KantoDone(g) && !StoryDone(g)) { g.hubTower = !g.hubTower; PlaySfx(Sfx::MenuMove); return; }
    if (in.back) { SaveOrWarn(g); GoTitle(g); return; }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    int stages = (int)Story().stages.size();
    bool tower = StoryDone(g) || (KantoDone(g) && g.hubTower);
    if (g.hubSel == 0 && !tower && CanChooseRegion(g)) { BeginRegionChoose(g, 1); return; }
    if (g.hubSel == 0 && !tower && NeedsRegionStarter(g) >= 0) { BeginRegionStarter(g); return; }
    if (((g.hubSel == 0 && !tower) || g.hubSel == 1) && HealthyCount(g.profile) == 0) {
        Toast(g, "Your whole team has fainted! Visit the Healing Center first.");
        OpenHealingCenter(g);
        return;
    }
    switch (g.hubSel) {
        case 0:
            if (tower) { BeginTowerFloor(g, g.profile.towerFloor + 1, TowerSeedFor(g)); break; }
            g.stageIdx = std::min(g.profile.storyStage, stages - 1);
            g.mode = Mode::Story;
            g.rareBattle = false;
            g.screen = Screen::StageIntro;
            break;
        case 1:
            StartTrainingBattle(g);
            break;
        case 2:
            g.mapSel = std::min(g.profile.storyStage, stages - 1);
            g.screen = Screen::Map;
            break;
        case 3:
            g.shopSel = 0;
            g.screen = Screen::Shop;
            break;
        case 4:
            OpenHealingCenter(g);
            break;
        case 5:
            g.myCreSel = 0;
            g.itemPick = false;
            g.actMenu = false;
            g.swapFrom = -1;
            g.stonePick = false;
            g.moveOrder = false;
            g.boxTab = 0;
            g.screen = Screen::MyCreatures;
            break;
        case 6:
            OpenBag(g);
            break;
        case 7:
            OpenPokedex(g, 0);
            break;
        case 8:
            StoryTimeStep(g, true);
            SaveOrWarn(g);
            break;
        case 9:
            StartPractice(g);
            break;
        case 10:
            EnterWorld(g);
            break;
        case HUB_GEAR:
            g.settingsSel = 0;
            g.settingsBack = Screen::StoryHub;
            g.screen = Screen::Settings;
            break;
        default:
            SaveOrWarn(g);
            GoTitle(g);
            break;
    }
}

namespace {
// The hub's first item: the next story battle, the League, the trip to Johto, or the Battle Tower
// (once Kanto is done, Left/Right swaps between the story and the Tower).
std::string CoopFirstLabel(const Game& g, bool tower, int storyStage) {
    bool swap = KantoDone(g) && !StoryDone(g);
    std::string s;
    if (tower) s = TextFormat("Battle Tower 2v2: floor %d  (best %d)", g.coopSave.towerFloor + 1, g.coopSave.towerBest);
    else if (CanChooseRegion(g)) s = g.on.active && g.on.me != 0 ? "The host picks the next region..." : "Choose the next region!";
    else if (int r = NeedsRegionStarter(g); r >= 0) s = "Travel to " + RegionInfo(r).name + " together!";
    else s = LeagueProgress(storyStage) >= 0 ? (StageRegion(storyStage) >= 1 ? RegionInfo(StageRegion(storyStage)).name + " League (2v2)" : "Pokemon League (2v2)") : "Story battle (2v2)";
    return swap ? "< " + s + " >" : s;
}
std::string HubFirstLabel(const Game& g, bool tower, int storyStage, int towerFloor, int towerBest) {
    bool swap = KantoDone(g) && !StoryDone(g);
    std::string s;
    if (tower) s = TextFormat("Battle Tower: floor %d  (best %d)", towerFloor + 1, towerBest);
    else if (CanChooseRegion(g)) s = "Choose your next region!";
    else if (int r = NeedsRegionStarter(g); r >= 0) s = "Travel to " + RegionInfo(r).name + "!";
    else s = LeagueProgress(storyStage) >= 0 ? (StageRegion(storyStage) >= 1 ? RegionInfo(StageRegion(storyStage)).name + " League" : "Pokemon League") : "Story battle";
    return swap ? "< " + s + " >" : s;
}
// The hub's Map item: legendary encounters live in a tab on the Map, so it says when some are waiting.
std::string MapHubLabel(const Game& g) {
    if (BadgeCount(g) < 8) return "Map";
    int open = 0;
    for (int i = 0; i < LEGEND_COUNT; i++) open += LegendOpen(g, i) ? 1 : 0;
    return open ? TextFormat("Map  (%d legendar%s waiting)", open, open == 1 ? "y" : "ies") : "Map";
}
} // namespace

namespace {
// The hub's sky: the time of day and the season, with drifting petals, leaves, snow or fireflies.
void DrawHubSky(int clock, float t) {
    bool night = IsNight(clock);
    int season = SeasonOf(clock);
    static const Color dayTop[4] = {{120, 170, 230, 255}, {80, 160, 240, 255}, {200, 150, 110, 255}, {170, 190, 215, 255}};
    static const Color dayBot[4] = {{250, 200, 220, 255}, {250, 230, 160, 255}, {240, 190, 120, 255}, {230, 238, 248, 255}};
    Color top = night ? Color{14, 16, 44, 255} : dayTop[season];
    Color bot = night ? Color{50, 36, 78, 255} : dayBot[season];
    ClearBackground(top);
    DrawRectangleGradientV(0, 0, VW, VH, top, bot);
    DrawRectangle(0, 0, VW, VH, Fade(Color{20, 18, 32, 255}, night ? 0.25f : 0.55f));
    DrawRectangleGradientV(0, VH - 50, VW, 50, Fade(Color{20, 18, 32, 255}, 0), Fade(Color{20, 18, 32, 255}, 0.7f));   // keep the panels readable
    if (night) {
        for (int i = 0; i < 60; i++) {
            float x = (float)((i * 173) % VW), y = (float)((i * 97) % 200);
            float tw = 0.4f + 0.6f * (0.5f + 0.5f * std::sin(t * 2.0f + i));
            DrawCircleV({x, y}, (i % 7 == 0) ? 1.1f : 0.6f, Fade(WHITE, tw * 0.8f));
        }
        DrawCircleV({600, 14}, 9, Fade(Color{250, 245, 220, 255}, 0.9f));
        DrawCircleV({604, 12}, 8, top);
    } else {
        DrawCircleV({612, 10}, 20, Fade(Color{255, 240, 180, 255}, 0.18f));
        DrawCircleV({612, 10}, 11, Fade(Color{255, 244, 200, 255}, 0.7f));
    }
    if (Cfg().fxLevel >= 2) return;
    for (int i = 0; i < 34; i++) {   // seasonal drift
        float sp = 10 + (i % 5) * 4.0f;
        float x = std::fmod(i * 57.0f + t * sp * (season == 3 ? 0.3f : 1.0f) + std::sin(t + i) * 8, (float)VW + 20) - 10;
        float y = std::fmod(i * 41.0f + t * sp * 0.8f, (float)VH + 20) - 10;
        if (night && season != 3) {   // fireflies
            float a = 0.5f + 0.5f * std::sin(t * 3 + i * 1.7f);
            DrawCircleV({x, y}, 2.4f, Fade(Color{200, 255, 120, 255}, 0.12f * a));
            DrawCircleV({x, y}, 0.9f, Fade(Color{230, 255, 160, 255}, 0.8f * a));
            continue;
        }
        float rot = t * 90 + i * 37;
        switch (season) {
            case 0: DrawEllipse((int)x, (int)y, 2.2f, 1.2f, Fade(Color{255, 190, 215, 255}, 0.75f)); break;   // petals
            case 1: DrawCircleV({x, y}, 0.8f, Fade(Color{255, 250, 200, 255}, 0.5f)); break;                    // summer motes
            case 2: {
                Vector2 c{x, y};
                float ca = std::cos(rot * DEG2RAD) * 2.4f, sa = std::sin(rot * DEG2RAD) * 2.4f;
                DrawLineEx({c.x - ca, c.y - sa}, {c.x + ca, c.y + sa}, 1.8f, Fade(i % 2 ? Color{220, 110, 40, 255} : Color{240, 170, 50, 255}, 0.8f));
                break;
            }
            default: DrawCircleV({x, y}, (i % 3) ? 1.0f : 1.6f, Fade(WHITE, 0.85f)); break;   // snow
        }
    }
}

// One hub menu entry as a tile with an icon.
void DrawHubTile(int kind, const char* label, int x, int y, int w, bool sel, Color tc, float e, float t) {
    static const Color acc[12] = {{230, 80, 80, 255}, {90, 190, 90, 255}, {90, 150, 230, 255}, {240, 190, 60, 255}, {250, 120, 160, 255}, {226, 52, 52, 255},
                                  {170, 120, 70, 255}, {220, 60, 60, 255}, {120, 110, 210, 255}, {230, 140, 60, 255}, {255, 200, 80, 255}, {110, 170, 200, 255}};
    x += (int)std::lround((1 - e) * 70) - (sel ? 4 : 0);
    Color a = acc[kind % 12];
    Rectangle rc{(float)x, (float)y, (float)w + (sel ? 4 : 0), 14};
    DrawRectangleRounded({rc.x + 1, rc.y + 1.5f, rc.width, rc.height}, 0.4f, 6, Fade(BLACK, 0.3f * e));
    DrawRectangleRounded(rc, 0.4f, 6, Fade(sel ? UI_PANEL_LIGHT : UI_PANEL, 0.95f * e));
    DrawRectangleGradientH(x + 15, y + 1, 50, 12, Fade(a, (sel ? 0.35f : 0.18f) * e), Fade(a, 0));
    DrawRectangleRoundedLinesEx(rc, 0.4f, 6, 1.0f, Fade(sel ? UI_GOLD : Color{70, 64, 92, 255}, e));
    if (sel) {   // a light sweep across the selected tile
        float sx = std::fmod(t * 140.0f, (float)w + 120) - 40;
        if (sx > 0 && sx < w) DrawRectangleGradientH(x + (int)sx, y + 1, 18, 12, Fade(WHITE, 0), Fade(WHITE, 0.14f));
    }
    // icon chip
    DrawRectangleRounded({(float)x + 1.5f, (float)y + 1.5f, 12, 11}, 0.35f, 6, Fade(a, 0.95f * e));
    float cx = x + 7.5f, cy = y + 7.0f;
    Color ic = Fade(WHITE, e), ink = Fade(UI_INK, e);
    switch (kind) {
        case 0: DrawLineEx({cx - 3.5f, cy + 3.5f}, {cx + 3.5f, cy - 3.5f}, 1.5f, ic); DrawLineEx({cx - 3.5f, cy - 3.5f}, {cx + 3.5f, cy + 3.5f}, 1.5f, ic); break;   // swords
        case 1: for (int k = -1; k <= 1; k++) DrawTriangle({cx + k * 2.5f, cy - 3.5f + std::abs(k)}, {cx + k * 2.5f - 1.4f, cy + 3.5f}, {cx + k * 2.5f + 1.4f, cy + 3.5f}, ic); break;   // grass
        case 2: DrawRectangle((int)cx - 4, (int)cy - 3, 8, 6, ic); DrawLineEx({cx - 1.3f, cy - 3}, {cx - 1.3f, cy + 3}, 0.8f, ink); DrawLineEx({cx + 1.3f, cy - 3}, {cx + 1.3f, cy + 3}, 0.8f, ink); break;   // map
        case 3: DrawCircleV({cx, cy}, 3.6f, Fade(Color{255, 230, 120, 255}, e)); DrawRing({cx, cy}, 2.2f, 2.9f, 0, 360, 16, Fade(Color{200, 150, 40, 255}, e)); break;   // coin
        case 4: DrawRectangle((int)cx - 1, (int)cy - 4, 3, 8, ic); DrawRectangle((int)cx - 4, (int)cy - 1, 8, 3, ic); break;   // cross
        case 5: DrawPokeBall({cx, cy}, 3.8f, 0, 0, WHITE, (unsigned char)(255 * e)); break;
        case 6: DrawRectangle((int)cx - 3, (int)cy - 2, 7, 6, ic); DrawRing({cx + 0.5f, cy - 2}, 1.2f, 2.0f, 180, 360, 10, ic); break;   // bag
        case 7: DrawRectangle((int)cx - 3, (int)cy - 4, 7, 8, ic); DrawCircleV({cx + 0.5f, cy - 1}, 1.4f, Fade(Color{120, 200, 255, 255}, e)); break;   // dex
        case 8: DrawCircleV({cx, cy}, 3.6f, ic); DrawCircleV({cx + 1.6f, cy - 1.2f}, 3.0f, Fade(a, e)); break;   // moon
        case 9: DrawRing({cx, cy}, 2.6f, 3.8f, 0, 360, 16, ic); DrawCircleV({cx, cy}, 1.2f, ic); break;   // target
        case 10: DrawShinyStar(cx, cy, 3.6f); break;
        default: DrawRectangle((int)cx - 3, (int)cy - 3, 7, 7, ic); DrawRectangle((int)cx - 2, (int)cy - 3, 4, 2, ink); break;   // save
    }
    GDrawText(label, x + 18, y + 2, 10, Fade(tc, e));
}
} // namespace

void DrawStoryHub(const Game& g) {
    const Profile& p = g.profile;
    DrawHubSky(p.clock, (float)GetTime());
    int stages = (int)Story().stages.size();
    bool done = p.storyStage >= stages;
    bool tower = done || (KantoDone(g) && g.hubTower);

    // Map on the left
    DrawStoryMap(p, 8, 26, 400, 300, g.t, false);
    DrawHubIcon(0, 8, 5, g.hubSel == HUB_GEAR, g.t);
    DrawTextShadow(p.name.c_str(), 32, 6, 20, UI_GOLD);
    std::string next = NextLine(g, p.storyStage);
    GDrawText(next.c_str(), 10, 330, 10, done ? UI_GOLD : UI_TEXT);

    // Coins and badges
    int rx = 416, rw = 216;
    DrawCoins(p.coins, rx, 6);
    GDrawText(TextFormat("%dW %dL", p.wins, p.losses), rx + 60, 7, 10, UI_DIM);
    std::vector<std::string> badges;   // this region's
    int hubRegion = ActiveRegion(p.storyStage);
    for (const auto& st : Story().stages)
        if (st.IsGym() && st.region == hubRegion) badges.push_back(st.badge);
    for (size_t i = 0; i < badges.size(); i++) DrawBadge(badges[i], rx + rw - 6 - (int)(badges.size() - 1 - i) * 14, 12, HasBadge(p, badges[i]));

    // Menu
    bool rare = NextTrainingIsRare(p);
    int every = std::max(1, Story().rareEvery);
    int left = every - (p.trainingWins % every) - 1;
    int hurt = 0;
    for (const auto& m : p.mons) hurt += (!m.boxed && m.hp < 0.999f) ? 1 : 0;
    int dexCaught = 0;
    for (const auto& sp : AllSpecies()) if (sp.dex > 0 && sp.custom && DexHasCaught(p, sp.name)) dexCaught++;
    std::string items[12] = {
        HubFirstLabel(g, tower, p.storyStage, p.towerFloor, p.towerBest),
        (rare ? "RARE in " : "Training: ") + TrainAreaName(g) + (rare ? "!" : TextFormat("  (%d to a rare)", left)),
        MapHubLabel(g),
        GameCornerOpen(g) ? "Shop & Game Corner" : "Shop",
        hurt ? TextFormat("Healing Center (%d hurt)", hurt) : "Healing Center",
        "Party & Box",
        "Bag",
        TextFormat("Pokedex (%d caught)", dexCaught),
        IsNight(p.clock) ? "Wait until morning" : "Wait until night",
        "Practice arena",
        "Walk around (test): " + WorldPlaceName(g),
        "Save and quit",
    };
    for (int i = 0; i < 12; i++) {
        int y = 26 + i * 15;
        bool sel = g.hubSel == i;
        Color c = sel ? UI_GOLD : UI_TEXT;
        if (i == 1 && rare) c = Color{255, 150, 230, 255};
        if (i == 2 && items[2].size() > 3) c = sel ? UI_GOLD : Color{255, 200, 120, 255};   // legendaries waiting
        DrawHubTile(i == 11 ? 11 : i == 10 ? 2 : i, items[i].c_str(), rx, y, rw, sel, c, UiEnterT(i, 0.03f), (float)GetTime());
    }
    DrawTimePanel(p.clock, 402, 30);

    // Team
    int ty = 26 + 12 * 15 + 8;
    std::vector<const OwnedMon*> party;
    for (const auto& m : p.mons) if (!m.boxed) party.push_back(&m);
    GDrawText(TextFormat("Party (%d/%d)", (int)party.size(), PARTY_SIZE), rx, ty, 10, UI_DIM);
    for (int i = 0; i < (int)party.size(); i++) {
        const OwnedMon& m = *party[i];
        int idx = MonSpecies(m);
        float e = UiEnterT(12 + i, 0.03f);
        int y = ty + 12 + i * 18;
        int x = rx + (int)std::lround((1 - e) * 60);
        Color tc = idx >= 0 ? TypeColor(AllSpecies()[idx].type1) : UI_PANEL;
        DrawRectangleRounded({(float)x, (float)y, (float)rw, 17}, 0.35f, 6, Fade(UI_PANEL, 0.92f * e));
        DrawRectangleGradientH(x + 1, y + 1, 40, 15, Fade(tc, 0.45f * e), Fade(tc, 0));
        DrawRectangleRoundedLinesEx({(float)x, (float)y, (float)rw, 17}, 0.35f, 6, 1.0f, Fade(i == 0 ? UI_GOLD : Color{70, 64, 92, 255}, e));
        float hop = 0;
        if (i == 0 && m.hp > 0) {   // the lead hops now and then
            float ph = std::fmod((float)GetTime(), 2.4f);
            if (ph < 0.36f) hop = std::sin(ph / 0.36f * PI) * 4.0f;
        }
        if (idx >= 0) {
            DrawEllipse(x + 11, y + 15, 6, 1.5f, Fade(BLACK, 0.3f * e));
            DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, x + 11, y + 16 - (int)hop, 15, m.hp > 0 ? WHITE : Color{90, 90, 100, 255});
        }
        if (m.shiny) DrawShinyStar((float)x + 4, (float)y + 4, 3);
        GDrawText(m.species.c_str(), x + 24, y + 4, 10, Fade(m.hp > 0 ? UI_TEXT : UI_DIM, e));
        if (i == 0) GDrawText("LEAD", x + 24 + TextW(m.species.c_str(), 10) + 4, y + 4, 10, Fade(UI_GOLD, 0.8f * e));
        GDrawText(TextFormat("Lv%d", m.level), x + 112 + 14, y + 4, 10, Fade(UI_GOLD, e));
        DrawHpBar(x + 156, y + 7, 40, 3, m.hp);
        if (!m.item.empty()) DrawRectangle(x + rw - 12, y + 6, 5, 5, Color{140, 220, 255, 255});
    }
    if (p.mons.size() > party.size()) GDrawText(TextFormat("+%d in the Box", (int)(p.mons.size() - party.size())), rx + 120, ty, 10, UI_DIM);
    GDrawText("Enter choose   Esc save and leave", rx, 346, 10, UI_DIM);
    DrawToast(g);
    DrawStoryCard(g);
}

// ------------------------------------------------------------------ map

bool IsRematch(const Game& g) {
    if (g.mode != Mode::Story && g.mode != Mode::CoopStory) return false;
    int done = g.coop ? g.coopSave.storyStage : g.profile.storyStage;
    return g.stageIdx >= 0 && g.stageIdx < (int)Story().stages.size() && g.stageIdx < done;
}

namespace {
bool CanRematch(const Game& g, int stage) {
    const auto& stages = Story().stages;
    return stage >= 0 && stage < (int)stages.size() && (stages[stage].IsBoss() || stages[stage].postgame) && stage < ProgressView(g).storyStage;
}
} // namespace

bool StartGymRematch(Game& g, int stage) {
    if (!CanRematch(g, stage)) { Toast(g, Story().stages[std::max(0, std::min((int)Story().stages.size() - 1, stage))].postgame ? "Beat the Champion first." : "Beat them in the story first."); return false; }
    const StageDef& st = Story().stages[stage];
    if (g.coop && g.on.active) {   // online: both pick the same rematch, like any battle
        if (OnlineWaiting(g)) return false;
        if (HealthyCount(Prof(g, g.on.me)) == 0) { Toast(g, "Your party has all fainted! Visit the Healing Center first."); return false; }
        OnlineSetReady(g, 3 + stage);
        EnterStoryHub(g);
        g.hubSel = 2;
        const std::string& pn = g.coopP[1 - g.on.me].name;
        Toast(g, g.on.partnerReady == 3 + stage ? "Rematch with " + st.trainer + "!"
                                                 : "Waiting for " + pn + " to pick the " + st.trainer + " rematch on their Map.");
        return true;
    }
    if (g.coop) {
        for (int p = 0; p < 2; p++)
            if (HealthyCount(g.coopP[p]) == 0) { Toast(g, g.coopP[p].name + "'s party has all fainted! Visit the Healing Center first."); return false; }
    } else if (HealthyCount(g.profile) == 0) {
        Toast(g, "Your whole team has fainted! Visit the Healing Center first.");
        return false;
    }
    g.stageIdx = stage;
    g.mode = g.coop ? Mode::CoopStory : Mode::Story;
    g.rareBattle = false;
    g.screen = Screen::StageIntro;
    return true;
}

// The Map's two tabs: the route and the Legendary Pokemon. Tab (X) switches.
bool MapTabSwitch(Game& g, bool toLegends) {
    if (toLegends && BadgeCount(g) < 8) {
        PlaySfx(Sfx::MenuBack);
        Toast(g, "Legends tell of rare Pokemon that only show themselves to trainers with all 8 badges.");
        return false;
    }
    PlaySfx(Sfx::MenuMove);
    if (toLegends) {   // the legendaries of the region the map is showing
        int r = StageRegion(g.mapSel);
        if (Legend(g.legendSel).region != r)
            for (int i = 0; i < LEGEND_COUNT; i++) if (Legend(i).region == r) { g.legendSel = i; break; }
        OpenLegends(g);
    } else {
        int r = Legend(g.legendSel).region;   // back on the map: the same region
        if (StageRegion(g.mapSel) != r && r < RegionCount()) g.mapSel = std::min(RegionInfo(r).first, (int)Story().stages.size() - 1);
        g.screen = Screen::Map;
    }
    return true;
}

void DrawMapTabs(const Game& g, bool legends) {
    static float slide = 0;
    float target = legends ? 1.0f : 0.0f;
    slide += (target - slide) * std::min(1.0f, GetFrameTime() * 14);
    if (std::fabs(target - slide) > 1 || Cfg().fxLevel >= 2) slide = target;
    const char* names[2] = {"Route Map", "Legendary Pokemon"};
    int x0 = 8, y = 5, h = 15, w[2] = {72, 112};
    int xs[2] = {x0, x0 + w[0] + 4};
    bool locked = BadgeCount(g) < 8;
    for (int i = 0; i < 2; i++) {
        Rectangle rc{(float)xs[i], (float)y, (float)w[i], (float)h};
        DrawRectangleRounded({rc.x + 1, rc.y + 1.5f, rc.width, rc.height}, 0.4f, 6, Fade(BLACK, 0.35f));
        DrawRectangleRounded(rc, 0.4f, 6, Fade(UI_PANEL, 0.95f));
        DrawRectangleRoundedLinesEx(rc, 0.4f, 6, 1.0f, Color{70, 64, 92, 255});
    }
    float hx = xs[0] + (xs[1] - xs[0]) * slide, hw = w[0] + (w[1] - w[0]) * slide;   // the highlight slides between tabs
    DrawRectangleRounded({hx, (float)y, hw, (float)h}, 0.4f, 6, UI_PANEL_LIGHT);
    DrawRectangleRoundedLinesEx({hx, (float)y, hw, (float)h}, 0.4f, 6, 1.0f, UI_GOLD);
    DrawRectangle((int)hx + 8, y + h - 2, (int)hw - 16, 2, UI_GOLD);
    for (int i = 0; i < 2; i++) {
        bool on = (i == 1) == legends;
        Color c = on ? UI_GOLD : (i == 1 && locked ? Fade(UI_DIM, 0.6f) : UI_TEXT);
        GDrawText(names[i], xs[i] + w[i] / 2 - TextW(names[i], 10) / 2, y + 3, 10, c);
    }
    const char* hint = locked ? "Tab (X): 8 badges needed" : "Tab (X): switch";
    DrawTextOutlined(hint, xs[1] + w[1] + 8, y + 3, 10, UI_DIM);
}

void UpdateMap(Game& g) {
    PlayerInput in = MenuInput();
    const auto& stages = Story().stages;
    int n = (int)stages.size();
    if (n == 0 || in.back) { PlaySfx(Sfx::MenuBack); EnterStoryHub(g); g.hubSel = 2; return; }
    if (in.sort) { MapTabSwitch(g, true); return; }
    if (in.confirm) {
        if (CanRematch(g, g.mapSel)) { PlaySfx(Sfx::MenuOk); StartGymRematch(g, g.mapSel); return; }
        PlaySfx(Sfx::MenuBack);
        EnterStoryHub(g);
        g.hubSel = 2;
        return;
    }
    // Browse the regions reached so far (Johto's map shows once you've got there).
    int maxSel = std::min(n - 1, RegionInfo(ActiveRegion(CurStage(g))).end - 1);
    if (maxSel < 0) maxSel = n - 1;
    g.mapSel = std::min(g.mapSel, maxSel);
    if (in.up) { g.mapSel = std::max(0, g.mapSel - 1); PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.mapSel = std::min(maxSel, g.mapSel + 1); PlaySfx(Sfx::MenuMove); }
    if (in.left || in.right) {   // jump to the previous / next gym
        int d = in.left ? -1 : 1;
        for (int i = g.mapSel + d; i >= 0 && i <= maxSel; i += d)
            if (stages[i].IsBoss() || stages[i].postgame) { g.mapSel = i; PlaySfx(Sfx::MenuMove); break; }
    }
}

void DrawMap(const Game& g) {
    DrawMenuBackdrop(Color{20, 18, 32, 255}, (float)GetTime());
    Profile pv = ProgressView(g);
    DrawStoryMap(pv, 0, 0, 480, 360, g.t, true, g.mapSel);
    DrawMapTabs(g, false);
    int x = 484, w = 152;
    DrawPanel(x, 4, w, 352);
    GDrawText("ROUTE", x + 8, 10, 10, UI_GOLD);
    const auto& stages = Story().stages;
    int n = (int)stages.size();
    if (n == 0) return;
    const StageDef& st = stages[g.mapSel];
    bool beaten = g.mapSel < pv.storyStage;
    bool isNext = g.mapSel == pv.storyStage;
    const auto& RG = RegionInfo(st.region);
    GDrawText(TextFormat("%s  -  stage %d of %d", RG.name.c_str(), g.mapSel - RG.first + 1, RG.end - RG.first), x + 8, 26, 10, UI_DIM);
    DrawTextShadow(st.name.c_str(), x + 8, 40, 10, UI_TEXT);
    GDrawText(st.trainer.c_str(), x + 8, 54, 10, st.IsGym() ? UI_GOLD : UI_TEXT);
    if (st.postgame) {   // off the main road: fought from here once the Champion falls
        bool won = PostgameBeaten(g, g.mapSel), open = g.mapSel < pv.storyStage;
        GDrawText(won ? "Beaten" : open ? "Post-game challenge" : "After the Champion", x + 8, 70, 10,
                  won ? Color{120, 230, 130, 255} : open ? UI_GOLD : UI_DIM);
        if (open) {
            float pulse = 0.75f + 0.25f * std::sin(g.t * 5);
            DrawPanel(x + 6, 140, w - 12, 14, UI_PANEL_LIGHT, Fade(UI_GOLD, pulse));
            GDrawText(won ? "Enter: BATTLE AGAIN" : "Enter: CHALLENGE", x + 12, 142, 10, UI_GOLD);
        }
    } else
    GDrawText(beaten ? "Beaten" : (isNext ? "Up next" : "Locked"), x + 8, 70, 10,
             beaten ? Color{120, 230, 130, 255} : (isNext ? WHITE : UI_DIM));
    GDrawText(TextFormat("%d Pokemon", (int)st.team.size()), x + 8, 86, 10, UI_TEXT);
    GDrawText(TextFormat("Arena: %s", ArenaName(st.arena)), x + 8, 100, 10, UI_TEXT);
    if (st.IsBoss()) {
        if (st.IsGym()) {
            DrawBadge(st.badge, x + 20, 126, HasBadge(pv, st.badge), 11);
            GDrawText(TextFormat("%s Badge", st.badge.c_str()), x + 36, 121, 10, BadgeColor(st.badge));
        } else {
            DrawLeagueMark(st, x + 20, 126, beaten, 9);
            GDrawText(st.IsChampion() ? "The Championship" : TextFormat("Elite Four - %d of 4", st.league), x + 36, 121, 10, GymColor(st.arena));
        }
        if (beaten) {
            float pulse = 0.75f + 0.25f * std::sin(g.t * 5);
            DrawPanel(x + 6, 140, w - 12, 14, UI_PANEL_LIGHT, Fade(UI_GOLD, pulse));
            GDrawText("Enter: REMATCH", x + 12, 142, 10, UI_GOLD);
        }
    }
    // Gyms and the League
    int y = 158;
    GDrawText("Gyms & League", x + 8, y, 10, UI_DIM);
    y += 13;
    int partnerPick = g.on.active && IsRematchKind(g.on.partnerReady) ? g.on.partnerReady - 3 : -1;
    int rows = 0;
    for (int i = RG.first; i < RG.end; i++) rows += stages[i].IsBoss() || stages[i].postgame;
    int step = rows > 13 ? 10 : 11;   // Hoenn and Johto: 8 gyms, the League and a post-game challenge
    for (int i = RG.first; i < RG.end; i++) {   // this region's
        if (!stages[i].IsBoss() && !stages[i].postgame) continue;
        bool have = stages[i].IsGym() ? HasBadge(pv, stages[i].badge) : stages[i].postgame ? PostgameBeaten(g, i) : i < pv.storyStage;
        if (i == g.mapSel) DrawRectangle(x + 4, y - 1, w - 8, step, UI_PANEL_LIGHT);
        if (stages[i].IsGym()) DrawBadge(stages[i].badge, x + 14, y + 4, have, 4.5f);
        else if (stages[i].postgame) DrawPoly({(float)x + 14, (float)y + 4}, 5, 4.5f, g.t * 40, have ? UI_GOLD : Fade(UI_DIM, 0.7f));   // a star-ish mark
        else DrawLeagueMark(stages[i], x + 14, y + 4, have, 4.5f);
        std::string nm = stages[i].trainer;
        GDrawText(nm.c_str(), x + 26, y, 10, i == g.mapSel ? UI_GOLD : have ? UI_TEXT : UI_DIM);
        if (i == partnerPick) GDrawText("<", x + w - 14, y, 10, Color{140, 230, 150, 255});
        y += step;
    }
    if (partnerPick >= 0) {
        DrawWrapped(TextFormat("%s wants a rematch with %s - pick it too!", g.coopP[1 - g.on.me].name.c_str(), stages[partnerPick].trainer.c_str()),
                    x + 8, 306, w - 16, 10, Color{140, 230, 150, 255});
        return;
    }
    GDrawText("Up/Down: browse", x + 8, 318, 10, UI_DIM);
    GDrawText("Left/Right: gyms & League", x + 8, 330, 10, UI_DIM);
    GDrawText("Esc: back", x + 8, 342, 10, UI_DIM);
    DrawToast(g);
}

void UpdateStageIntro(Game& g) {
    if (g.legend < 0 && !g.tower && !IsRematch(g) && g.stageIdx >= 0 && g.stageIdx < (int)Story().stages.size() && g.cardFor != g.stageIdx) {
        g.cardFor = g.stageIdx;   // a story card before this battle, the first time it comes up
        const StageDef& st = Story().stages[g.stageIdx];
        if (!st.card.empty()) ShowStoryCard(g, st.card, st.cardMon);
    }
    if (UpdateStoryCard(g)) return;
    PlayerInput in = MenuInput();
    if (g.tower && in.sort) {   // a look at the Hall of Fame
        bool any = !g.profile.hof.empty() || (g.coop && (!g.coopP[0].hof.empty() || !g.coopP[1].hof.empty()));
        if (any) { g.hofSel = 0; g.hofBack = Screen::StageIntro; g.screen = Screen::HallOfFame; PlaySfx(Sfx::MenuOk); }
        return;
    }
    if (in.back) { PlaySfx(Sfx::MenuBack); if (g.on.active) OnlineCancelSelect(g); EnterStoryHub(g); return; }
    if (in.confirm) { PlaySfx(Sfx::MenuOk); g.mode = g.coop ? Mode::CoopStory : Mode::Story; BeginTeamSelect(g); }
}

namespace {
// Gym battles are an event: a packed stadium, the leader's name in lights, fireworks.
void DrawGymIntro(const Game& g, const StageDef& st) {
    float t = g.t;
    Color team = GymColor(st.arena);
    DrawGymStadium(t, st.arena, 0.5f + 0.5f * std::sin(t * 1.3f), 0, 236);
    // the arena floor far below, with the gym's colours
    DrawRectangle(0, 236, VW, VH - 236, Color{20, 16, 34, 255});
    DrawTriangle({40, VH}, {VW - 40.0f, VH}, {VW / 2.0f, 236}, Fade(team, 0.0f));
    for (int y = 236; y < VH; y++) {
        float u = (y - 236) / (float)(VH - 236);
        int half = (int)(150 + u * 170);
        DrawRectangle(VW / 2 - half, y, half * 2, 1, Fade(ColorBrightness(team, -0.55f + u * 0.2f), 1));
    }
    DrawEllipseLines(VW / 2, 300, 120, 40, Fade(WHITE, 0.5f));
    DrawLine(VW / 2, 236, VW / 2, VH, Fade(WHITE, 0.4f));
    // fireworks (the Championship gets the full show)
    for (int k = 0; k < (st.IsChampion() ? 7 : 3); k++) {
        float ph = std::fmod(t * 0.55f + k * 0.37f, 1.0f);
        int cyc = (int)(t * 0.55f + k * 0.37f);
        float fx = 80 + std::fmod(cyc * 197.0f + k * 151.0f, (float)(VW - 160)), fy = 30 + std::fmod(cyc * 61.0f + k * 37.0f, 90.0f);
        if (ph < 0.25f) { DrawLineEx({fx, 236 - (236 - fy) * (ph / 0.25f)}, {fx, 236 - (236 - fy) * (ph / 0.25f) + 6}, 1, Fade(WHITE, 0.8f)); continue; }
        float u = (ph - 0.25f) / 0.75f;
        Color fc = k == 0 ? team : k == 1 ? Color{255, 230, 120, 255} : Color{140, 220, 255, 255};
        for (int r = 0; r < 18; r++) {
            float a = r * 6.2831853f / 18;
            Vector2 p{fx + std::cos(a) * u * 34, fy + std::sin(a) * u * 34 + u * u * 10};
            DrawCircleV(p, 1.5f, Fade(fc, 1 - u));
        }
    }
    // the big sign
    DrawRectangle(0, 54, VW, 74, Fade(UI_INK, 0.72f));
    DrawRectangle(0, 54, VW, 2, team);
    DrawRectangle(0, 126, VW, 2, team);
    bool again = IsRematch(g);
    std::string head, sub;
    if (st.towerFloor > 0) {
        head = TextFormat("BATTLE TOWER  -  FLOOR %d  -  TOWER BOSS", st.towerFloor);
        sub = TextFormat("%s   -   a story boss at full strength, scaled to your team", ArenaName(st.arena));
    } else if (st.postgame) {
        head = TextFormat(again && PostgameBeaten(g, g.stageIdx) ? "POST-GAME REMATCH  -  %s" : "POST-GAME CHALLENGE  -  %s", st.name.c_str());
        sub = TextFormat("%s   -   the strongest trainer in %s", ArenaName(st.arena), RegionInfo(st.region).name.c_str());
    } else if (st.IsChampion()) {
        const char* where = st.region >= 2 ? "EVER GRANDE CITY" : "INDIGO PLATEAU";
        head = TextFormat(again ? "CHAMPIONSHIP REMATCH  -  %s" : "CHAMPIONSHIP BATTLE  -  %s", where);
        sub = again ? std::string(ArenaName(st.arena)) + "   -   rematch: the team is scaled to your level"
                    : std::string(ArenaName(st.arena)) + "   -   win and enter the Hall of Fame!";
    } else if (st.IsLeague()) {
        head = TextFormat(again ? "ELITE FOUR REMATCH  -  %s" : "THE ELITE FOUR  -  %s", st.name.c_str());
        sub = again ? TextFormat("%s   -   rematch: the team is scaled to your level", ArenaName(st.arena))
                    : TextFormat("%s   -   battle %d of 5: lose and the League starts over", ArenaName(st.arena), st.league);
    } else {
        head = TextFormat(again ? "GYM REMATCH  -  %s" : "GYM BATTLE  -  %s", st.name.c_str());
        sub = again ? TextFormat("%s   -   rematch: the team is scaled to your level", ArenaName(st.arena))
                    : TextFormat("%s   -   win the %s Badge!", ArenaName(st.arena), st.badge.c_str());
    }
    DrawTextCentered(head.c_str(), VW / 2, 60, 10, Fade(WHITE, 0.9f), false);
    float pulse = 1.0f + 0.04f * std::sin(t * 4);
    if (TextW(st.trainer.c_str(), 30) > VW - 140) DrawTextCentered(st.trainer.c_str(), VW / 2, (int)(82 - 4 * (pulse - 1)), 20, UI_GOLD);   // "Leader Tate & Liza"
    else DrawTextCentered(st.trainer.c_str(), VW / 2, (int)(76 - 4 * (pulse - 1)), 30, UI_GOLD);
    DrawTextCentered(sub.c_str(), VW / 2, 110, 10, st.IsGym() ? BadgeColor(st.badge) : ColorBrightness(team, 0.3f), false);
    int half = std::max(150, TextW(st.trainer.c_str(), TextW(st.trainer.c_str(), 30) > VW - 140 ? 20 : 30) / 2 + 22);   // clear of the name
    if (st.IsGym()) {
        DrawBadge(st.badge, VW / 2 - half, 92, true, 11);
        DrawBadge(st.badge, VW / 2 + half, 92, true, 11);
    } else if (st.IsLeague()) {
        DrawLeagueMark(st, VW / 2 - std::max(160, half), 92, true, 11);
        DrawLeagueMark(st, VW / 2 + std::max(160, half), 92, true, 11);
    }
}

// The gym leader walks out onto the arena floor under a spotlight (Leaders/<Name>.png).
void DrawLeaderOnFloor(const Game& g, const StageDef& st, const Texture2D& pic, float shift = 0, const char* name = nullptr) {
    static std::string lastKey;
    static double start = 0, lastSeen = -10;
    double now = GetTime();
    std::string key = st.trainer + "|" + st.name;
    if (key != lastKey || now - lastSeen > 0.4) { lastKey = key; start = now; }
    lastSeen = now;
    float u = (float)(now - start);
    Color team = GymColor(st.arena);
    float walk = std::min(1.0f, u / 0.85f);
    float e = 1 - (1 - walk) * (1 - walk) * (1 - walk);
    float cx = 548 + shift + (1 - e) * 140, foot = 338;
    float scale = std::min(2.0f, 128.0f / pic.height);   // stays below the info panel
    float bob = walk < 1 ? std::fabs(std::sin(u * 14)) * 3 : 0;
    // the spotlight: a soft cone from above, landing as a bright pool on the floor
    float lit = std::min(1.0f, std::max(0.0f, (u - 0.15f) / 0.4f));
    for (int k = 0; k < 6; k++) {
        float spread = 20 + k * 9;
        DrawTriangle({cx - 6.0f, 0}, {cx - spread * 1.6f, foot}, {cx + spread * 1.6f, foot}, Fade(ColorBrightness(team, 0.6f), 0.035f * lit));
        DrawTriangle({cx + 6.0f, 0}, {cx - spread * 1.6f, foot}, {cx + spread * 1.6f, foot}, Fade(WHITE, 0.02f * lit));
    }
    DrawEllipse((int)cx, (int)foot, (int)(70 * lit) + 1, 12, Fade(ColorBrightness(team, 0.5f), 0.35f * lit));
    DrawEllipse((int)cx, (int)foot, 36, 7, Fade(BLACK, 0.45f));
    // a coloured glow behind them once they've arrived, pulsing with the crowd
    if (walk >= 1) {
        float p = 0.5f + 0.5f * std::sin(g.t * 3);
        DrawCircleGradient((int)cx, (int)(foot - pic.height * scale * 0.55f), 70 + p * 6, Fade(team, 0.22f), Fade(team, 0));
    }
    // a dark silhouette for a moment, then the lights come up on them
    float reveal = std::min(1.0f, std::max(0.0f, (u - 0.55f) / 0.35f));
    unsigned char v = (unsigned char)(40 + 215 * reveal);
    DrawLeaderPicture(pic, cx, foot - bob, scale, Color{v, v, v, 255});
    if (reveal > 0 && reveal < 1) {   // a flash as the spotlight hits
        float f = 1 - std::fabs(reveal * 2 - 1);
        DrawCircleGradient((int)cx, (int)(foot - pic.height * scale * 0.5f), 60, Fade(WHITE, 0.35f * f), Fade(WHITE, 0));
    }
    // their name plate at their feet
    std::string nm = name ? std::string(name) : LeaderShortName(st.trainer);
    int tw = TextW(nm.c_str(), 10);
    DrawRectangle((int)cx - tw / 2 - 6, (int)foot + 2, tw + 12, 13, Fade(UI_INK, 0.85f * reveal));
    DrawRectangle((int)cx - tw / 2 - 6, (int)foot + 2, tw + 12, 1, Fade(team, reveal));
    DrawTextCentered(nm.c_str(), (int)cx, (int)foot + 4, 10, Fade(UI_GOLD, reveal), false);
}

// The Battle Tower: floor number in lights, the trainer, your record.
void DrawTowerIntro(const Game& g, const StageDef& st) {
    float t = g.t;
    DrawGymStadium(t, st.arena, 0.35f + 0.25f * std::sin(t * 1.1f), 0, 236);
    DrawRectangle(0, 236, VW, VH - 236, Color{20, 16, 34, 255});
    for (int y = 236; y < VH; y++) {   // the tower's floor, far below
        float u = (y - 236) / (float)(VH - 236);
        int half = (int)(150 + u * 170);
        DrawRectangle(VW / 2 - half, y, half * 2, 1, ColorBrightness(Color{150, 120, 60, 255}, -0.6f + u * 0.25f));
    }
    DrawRectangle(0, 40, VW, 84, Fade(UI_INK, 0.75f));
    DrawRectangle(0, 40, VW, 2, UI_GOLD);
    DrawRectangle(0, 122, VW, 2, UI_GOLD);
    DrawTextCentered("BATTLE TOWER", VW / 2, 46, 10, Fade(WHITE, 0.9f), false);
    DrawTextCentered(TextFormat("FLOOR %d", st.towerFloor), VW / 2, 58, 30, UI_GOLD);
    DrawTextCentered(st.trainer.c_str(), VW / 2, 96, 20, UI_TEXT);
    std::string say = "\"" + st.say + "\"";
    int lines = 1 + TextW(say.c_str(), 10) / 500;
    DrawPanel(60, 132, 520, 16 + lines * 12, UI_PANEL, UI_GOLD);
    DrawWrapped(say.c_str(), 70, 139, 500, 10, UI_TEXT);
    int written = (int)std::min<size_t>(st.team.size(), MAX_TEAM_SIZE);
    int foes = g.coop ? std::min(MAX_TEAM_SIZE, std::max(3, written + 2)) : written;
    int mine = g.coop ? 3 : TowerTeamSize(st.towerFloor);
    DrawRectangle(40, 168, VW - 80, 48, Fade(UI_INK, 0.85f));
    DrawTextCentered(TextFormat("%s%d vs %d   CPU skill %s   levels: your team %s", g.coop ? "2v2 co-op: up to 3 each   " : "", g.coop ? 6 : mine, foes,
                                AILevelName(st.cpu), st.levelOffset >= 0 ? TextFormat("+%d", st.levelOffset) : TextFormat("%d", st.levelOffset)),
                     VW / 2, 173, 10, UI_TEXT, false);
    DrawTextCentered(st.arena == ArenaKind::Stadium ? "The Tower's stadium. No tricks - just battling." : TextFormat("Tonight's arena: %s - %s", ArenaName(st.arena), ArenaTwist(st.arena)),
                     VW / 2, 187, 10, Color{150, 200, 240, 255}, false);
    int best = g.coop ? g.coopSave.towerBest : g.profile.towerBest;
    DrawTextCentered(TextFormat("This run: %d floor%s cleared     Record: floor %d     Every 10th floor: a Tower Boss", st.towerFloor - 1, st.towerFloor == 2 ? "" : "s", best),
                     VW / 2, 201, 10, UI_GOLD, false);
    DrawTextCentered("Everyone fights at full health (it won't change their HP outside). XP and coins as usual. Lose once and the run ends.", VW / 2, 300, 10, UI_DIM, false);
    bool hof = !g.profile.hof.empty() || (g.coop && (!g.coopP[0].hof.empty() || !g.coopP[1].hof.empty()));
    DrawRectangle(VW / 2 - 190, 330, 380, 16, Fade(UI_INK, 0.85f));
    DrawTextCentered(hof ? "Enter: choose your team     Tab / X: Hall of Fame     Esc: back" : "Enter: choose your team     Esc: back", VW / 2, 333, 10, UI_DIM, false);
}
} // namespace

namespace { void DrawStageIntroBase(const Game& g); }
void DrawStageIntro(const Game& g) {
    DrawStageIntroBase(g);
    DrawStoryCard(g);
}
namespace {
void DrawStageIntroBase(const Game& g) {
    if (g.legend >= 0) { DrawLegendIntro(g); return; }
    const StageDef st = ActiveStage(g);
    if (st.towerFloor > 0 && !st.BigIntro()) { DrawTowerIntro(g, st); return; }
    if (st.BigIntro()) {
        DrawGymIntro(g, st);
        const Texture2D* leader = (st.IsGym() || st.IsLeague()) && st.towerFloor <= 0 ? LeaderPicture(st.trainer) : nullptr;
        const Texture2D* leader2 = nullptr;   // a pair of leaders (Tate & Liza): both walk out
        std::string firstName;
        if (leader && st.trainer.find(" & ") != std::string::npos) {
            std::string first = st.trainer.substr(0, st.trainer.find(" & "));
            leader2 = LeaderPicture(first);
            if (leader2 == leader) leader2 = nullptr;
            firstName = LeaderShortName(first);
        }
        std::string say = "\"" + (st.say.empty() ? std::string("Let's battle!") : st.say) + "\"";
        // With the leader on the floor at the right, the speech and the info sit to their left.
        int px = leader ? 40 : 60, pw = leader ? 420 : 520;
        int lines = 1 + (TextW(say.c_str(), 10) + 90) / (pw - 20);
        if (leader2) {
            DrawLeaderOnFloor(g, st, *leader2, -30, firstName.c_str());
            DrawLeaderOnFloor(g, st, *leader, 34);
        } else if (leader) DrawLeaderOnFloor(g, st, *leader);
        DrawPanel(px, 136, pw, 14 + lines * 12, UI_PANEL, GymColor(st.arena));
        DrawWrapped(say.c_str(), px + 10, 142, pw - 20, 10, UI_TEXT);
        if (leader) {   // the speech bubble's tail, pointing at the leader
            float tx = (float)(px + pw);
            DrawTriangle({tx - 1, 142}, {tx - 1, 154}, {tx + 12, 150}, GymColor(st.arena));
        }
        int foes = g.coop ? std::min(MAX_TEAM_SIZE, std::max(3, (int)st.team.size() + 2)) : (int)std::min<size_t>(st.team.size(), MAX_TEAM_SIZE);
        int iy = std::max(170, 136 + 14 + lines * 12 + 4);
        int ix = leader ? 24 : 40, iw = leader ? 476 : VW - 80, icx = ix + iw / 2;   // the leader stands to the right
        // the arena's twist, wrapped to fit the panel
        std::vector<std::string> tl;
        {
            std::string tw = ArenaTwist(st.arena), cur, word;
            for (size_t i = 0; i <= tw.size(); i++) {
                if (i == tw.size() || tw[i] == ' ') {
                    std::string tryLine = cur.empty() ? word : cur + " " + word;
                    if (TextW(tryLine.c_str(), 10) > iw - 20 && !cur.empty()) { tl.push_back(cur); cur = word; }
                    else cur = tryLine;
                    word.clear();
                } else word += tw[i];
            }
            if (!cur.empty()) tl.push_back(cur);
        }
        DrawRectangle(ix, iy, iw, 20 + 12 * (int)tl.size(), Fade(UI_INK, 0.85f));
        DrawTextCentered(TextFormat("%d Pokemon   CPU skill %s%s", foes, AILevelName(st.cpu), g.coop ? "   2v2 co-op battle" : ""), icx, iy + 5, 10, UI_TEXT, false);
        for (size_t k = 0; k < tl.size(); k++) DrawTextCentered(tl[k].c_str(), icx, iy + 19 + 12 * (int)k, 10, Color{150, 200, 240, 255}, false);
        if (st.towerFloor > 0) DrawTextCentered("Battle Tower: everyone fights at full health. Lose and the run ends.", VW / 2, 318, 10, UI_DIM, false);
        DrawRectangle(icx - 130, 336, 260, 16, Fade(UI_INK, 0.85f));
        DrawTextCentered("Enter: choose your team     Esc: back", icx, 339, 10, UI_DIM, false);
        return;
    }
    DrawArena(g.t);
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.6f));
    {
        int reg = StageRegion(g.stageIdx);
        int num = g.stageIdx - RegionInfo(reg).first + 1;
        DrawTextCentered(reg > 0 ? TextFormat("%s %d: %s", RegionInfo(reg).name.c_str(), num, st.name.c_str()) : TextFormat("Stage %d: %s", num, st.name.c_str()), VW / 2, 60, 20, UI_DIM);
    }
    if (TextW(st.trainer.c_str(), 30) > VW - 40) DrawTextCentered(st.trainer.c_str(), VW / 2, 102, 20, UI_GOLD);   // a long pair of names
    else DrawTextCentered(st.trainer.c_str(), VW / 2, 96, 30, UI_GOLD);
    // Word-wrap the trainer's line into the speech panel.
    std::string say = "\"" + (st.say.empty() ? std::string("Let's battle!") : st.say) + "\"";
    std::vector<std::string> lines;
    std::string cur, word;
    for (size_t i = 0; i <= say.size(); i++) {
        if (i == say.size() || say[i] == ' ') {
            std::string tryLine = cur.empty() ? word : cur + " " + word;
            if (TextW(tryLine.c_str(), 10) > 500 && !cur.empty()) { lines.push_back(cur); cur = word; }
            else cur = tryLine;
            word.clear();
        } else {
            word += say[i];
        }
    }
    if (!cur.empty()) lines.push_back(cur);
    int ph = 20 + (int)lines.size() * 12;
    int py = 150;
    if (!st.story.empty() && !IsRematch(g)) {   // the story so far, in the narrator's voice
        int nl = 1 + TextW(st.story.c_str(), 10) / 500;
        DrawWrapped(st.story.c_str(), 70, 132, 500, 10, Color{170, 205, 240, 255});
        py = 136 + nl * 12 + 4;
    }
    DrawPanel(60, py, 520, ph, UI_PANEL, UI_BORDER);
    for (size_t i = 0; i < lines.size(); i++) DrawTextCentered(lines[i].c_str(), VW / 2, py + 10 + (int)i * 12, 10, UI_TEXT, false);
    int dy = std::max(0, py + ph + 8 - 214);   // the narration and a long speech push the rest down
    int foes = g.coop ? std::min(MAX_TEAM_SIZE, std::max(3, (int)st.team.size() + 2)) : (int)std::min<size_t>(st.team.size(), MAX_TEAM_SIZE);
    DrawTextCentered(TextFormat("%d Pokemon   CPU skill %s%s", foes, AILevelName(st.cpu), g.coop ? "   2v2 co-op battle" : ""),
                     VW / 2, 214 + dy, 10, UI_TEXT, false);
    if (st.arena != ArenaKind::Stadium) {
        DrawTextCentered(ArenaName(st.arena), VW / 2, 234 + dy, 20, UI_GOLD);
        DrawWrapped(ArenaTwist(st.arena), 50, 258 + dy, VW - 100, 10, Color{150, 200, 240, 255});
        dy += 12 * (TextW(ArenaTwist(st.arena), 10) / (VW - 100));
    }
    if (st.IsGym()) DrawTextCentered(TextFormat("Win the %s Badge!", st.badge.c_str()), VW / 2, 276 + dy, 10, BadgeColor(st.badge), false);
    else if (st.Catchable()) DrawTextCentered("Weaken their Pokemon (don't knock them out) for a chance to catch one.", VW / 2, 252 + dy, 10, UI_DIM, false);
    else if (!st.snorlax) DrawTextCentered("A trainer's Pokemon can't be caught - find new ones in training battles.", VW / 2, 252 + dy, 10, UI_DIM, false);
    DrawTextCentered("Enter: choose your team     Esc: back", VW / 2, std::max(300, 288 + dy), 10, UI_DIM, false);
}
} // namespace

// ------------------------------------------------------------------ party & box

namespace {
std::vector<std::string> HeldItemsInBag(const Profile& p) {
    std::vector<std::string> v;
    for (const auto& d : ItemList())
        if (IsHoldable(d) && BagCount(p, d.name) > 0) v.push_back(d.name);
    return v;
}

// Stones in the bag that would evolve this creature.
std::vector<std::string> UsableStones(const Profile& p, const OwnedMon& m) {
    std::vector<std::string> v;
    for (const auto& s : EvolutionStonesFor(MonSpecies(m)))
        if (BagCount(p, s) > 0) v.push_back(s);
    return v;
}

// ---- TMs and the Move Relearner
constexpr int RELEARN_PRICE = 100;
bool KnowsMove(const OwnedMon& m, const std::string& mv) { return std::find(m.moves.begin(), m.moves.end(), mv) != m.moves.end(); }
bool CanLearnTM(const OwnedMon& m, const std::string& move) {
    int si = MonSpecies(m);
    if (si < 0) return false;
    const auto& v = AllSpecies()[si].tmMoves;
    return std::find(v.begin(), v.end(), move) != v.end();
}
// TMs in the bag this creature can learn (and doesn't know yet), as item names.
std::vector<std::string> UsableTMs(const Profile& p, const OwnedMon& m) {
    std::vector<std::string> v;
    for (const auto& d : ItemList())
        if (IsTM(d) && BagCount(p, d.name) > 0 && CanLearnTM(m, d.move) && !KnowsMove(m, d.move)) v.push_back(d.name);
    return v;
}
// Moves the Move Relearner can teach: its level-up moves up to its level (and evolution moves),
// plus anything it forgot (like old TM moves). {level, move}; level -1 = forgotten TM/other move.
std::vector<std::pair<int, std::string>> RelearnMoves(const OwnedMon& m) {
    std::vector<std::pair<int, std::string>> v;
    int si = MonSpecies(m);
    auto add = [&](int lv, const std::string& mv) {
        if (mv.empty() || KnowsMove(m, mv) || !FindMove(mv)) return;
        for (const auto& e : v) if (e.second == mv) return;
        v.push_back({lv, mv});
    };
    if (si >= 0)
        for (const auto& e : AllSpecies()[si].learnset)
            if (e.level <= m.level) add(e.level, e.move.name);
    for (const auto& f : m.forgotten) add(-1, f);
    std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return (a.first < 0 ? 999 : a.first) < (b.first < 0 ? 999 : b.first); });
    return v;
}
void PayForLearn(Profile& p, int src, const std::string& item) {
    if (src == 1) BagTake(p, item);
    else if (src == 2) p.coins -= RELEARN_PRICE;
}
void Forgot(OwnedMon& m, const std::string& mv) {
    if (std::find(m.forgotten.begin(), m.forgotten.end(), mv) == m.forgotten.end()) m.forgotten.push_back(mv);
}
// Teach `move` now (free slot) or open the "forget a move?" screen. src 1 = TM `item`, 2 = Move Relearner.
void TeachMove(Game& g, int mon, const std::string& move, int src, const std::string& item, bool fromShop) {
    Profile& p = g.profile;
    if (mon < 0 || mon >= (int)p.mons.size()) return;
    OwnedMon& m = p.mons[mon];
    if (m.moves.size() < 4) {
        m.moves.push_back(move);
        m.forgotten.erase(std::remove(m.forgotten.begin(), m.forgotten.end(), move), m.forgotten.end());
        PayForLearn(p, src, item);
        SaveOrWarn(g);
        PlaySfx(Sfx::LevelUp);
        Toast(g, m.species + " learned " + move + "!");
        return;
    }
    g.evoQueue.clear();
    g.learnQueue.clear();
    g.rewardsPending = 0;
    g.rewardWho.clear();
    g.catchPending = false;
    g.learnQueue.push_back({mon, move, g.coop ? g.coopActive : 0, src, item});
    g.returnToShop = fromShop;
    g.returnToParty = !fromShop;
    g.learnSel = 0;
    g.screen = Screen::LearnMove;
    PlaySfx(Sfx::MenuOk);
}

// Indexes into p.mons for the party (tab 0) or the Box (tab 1).
const char* kBoxSorts[6] = {"Caught", "Level", "Dex no.", "Name", "Type", "Newest"};

bool CanStillEvolve(int idx) { return idx >= 0 && (EvolutionIndex(idx) >= 0 || !AllSpecies()[idx].evoAlts.empty()); }

bool BoxFilterMatch(const OwnedMon& m, const std::string& f) {
    if (f.empty()) return true;
    int idx = MonSpecies(m);
    if (f == "evolve") return CanStillEvolve(idx);
    if (f == "item") return !m.item.empty();
    if (f == "hurt") return m.hp < 0.999f;
    if (f.rfind("type:", 0) == 0) {
        if (idx < 0) return false;
        std::string t = f.substr(5);
        const SpeciesDef& sp = AllSpecies()[idx];
        return TypeName(sp.type1) == t || (sp.type2 != Type::None && TypeName(sp.type2) == t);
    }
    return true;
}

// The filters worth offering: all, the special ones, then each type somebody in the Box has.
std::vector<std::string> BoxFilters(const Profile& p) {
    std::vector<std::string> f = {"", "evolve", "item", "hurt"};
    bool has[TYPE_COUNT] = {};
    for (const auto& m : p.mons) {
        int idx = MonSpecies(m);
        if (!m.boxed || idx < 0) continue;
        has[(int)AllSpecies()[idx].type1] = true;
        if (AllSpecies()[idx].type2 != Type::None) has[(int)AllSpecies()[idx].type2] = true;
    }
    for (int t = 0; t < TYPE_COUNT; t++) if (has[t]) f.push_back(std::string("type:") + TypeName((Type)t));
    return f;
}

std::string BoxFilterName(const std::string& f) {
    if (f.empty()) return "All";
    if (f == "evolve") return "Can evolve";
    if (f == "item") return "Has item";
    if (f == "hurt") return "Hurt";
    if (f.rfind("type:", 0) == 0) return f.substr(5);
    return f;
}

std::vector<int> TabList(const Profile& p, int tab) {
    std::vector<int> v;
    for (int i = 0; i < (int)p.mons.size(); i++)
        if (p.mons[i].boxed == (tab == 1) && (tab == 0 || BoxFilterMatch(p.mons[i], p.boxFilter))) v.push_back(i);
    if (tab != 1) return v;   // the party keeps its order (it's the order you send them out)
    auto dex = [&](int i) { int s = MonSpecies(p.mons[i]); return s >= 0 ? AllSpecies()[s].dex : 9999; };
    auto type = [&](int i) { int s = MonSpecies(p.mons[i]); return s >= 0 ? (int)AllSpecies()[s].type1 : 99; };
    switch (p.boxSort) {
        case 1: std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return p.mons[a].level > p.mons[b].level; }); break;
        case 2: std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return dex(a) < dex(b); }); break;
        case 3: std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return p.mons[a].species < p.mons[b].species; }); break;
        case 4: std::stable_sort(v.begin(), v.end(), [&](int a, int b) { return type(a) != type(b) ? type(a) < type(b) : dex(a) < dex(b); }); break;
        case 5: std::reverse(v.begin(), v.end()); break;
        default: break;
    }
    return v;
}

enum Act { ACT_ITEM, ACT_UP, ACT_DEPOSIT, ACT_WITHDRAW, ACT_SWAP, ACT_STONE, ACT_TM, ACT_MOVES, ACT_RELEASE, ACT_CANCEL };

std::vector<std::pair<int, std::string>> ActOptions(const Game& g) {
    std::vector<std::pair<int, std::string>> o;
    const Profile& p = g.profile;
    auto list = TabList(p, g.boxTab);
    if (g.myCreSel >= (int)list.size()) return o;
    const OwnedMon& m = p.mons[list[g.myCreSel]];
    if (g.boxTab == 0) {
        o.push_back({ACT_ITEM, m.item.empty() ? "Give a held item" : "Change held item"});
        if (g.myCreSel > 0) o.push_back({ACT_UP, "Move up the party"});
        if (PartyCount(p) > 1) o.push_back({ACT_DEPOSIT, "Send to the Box"});
    } else {
        if (PartyCount(p) < PARTY_SIZE) o.push_back({ACT_WITHDRAW, "Add to the party"});
        else o.push_back({ACT_SWAP, "Swap with a party member"});
        o.push_back({ACT_ITEM, m.item.empty() ? "Give a held item" : "Change held item"});
    }
    if (m.moves.size() > 1) o.push_back({ACT_MOVES, "Rearrange moves (buttons)"});
    if (!UsableStones(p, m).empty()) o.push_back({ACT_STONE, "Use an evolution stone"});
    { int n = (int)UsableTMs(p, m).size(); if (n > 0) o.push_back({ACT_TM, TextFormat("Use a TM (%d it can learn)", n)}); }
    if (g.boxTab == 1) o.push_back({ACT_RELEASE, "Release into the wild"});
    o.push_back({ACT_CANCEL, "Cancel"});
    return o;
}
} // namespace

void UpdateMyCreatures(Game& g) {
    PlayerInput in = MenuInput();
    Profile& p = g.profile;
    if (p.mons.empty()) { EnterStoryHub(g); return; }
    auto list = TabList(p, g.boxTab);
    int n = (int)list.size();
    g.myCreSel = std::max(0, std::min(g.myCreSel, n - 1));

    if (g.releaseAsk >= 0) {   // "Release X? It's goodbye for good."
        if (in.up || in.down || in.left || in.right) { g.releaseSel = 1 - g.releaseSel; PlaySfx(Sfx::MenuMove); }
        if (in.back || (in.confirm && g.releaseSel == 0)) { g.releaseAsk = -1; PlaySfx(Sfx::MenuBack); return; }
        if (!in.confirm) return;
        int idx = g.releaseAsk;
        g.releaseAsk = -1;
        if (idx < 0 || idx >= (int)p.mons.size() || !p.mons[idx].boxed) return;
        OwnedMon m = p.mons[idx];
        if (!m.item.empty()) BagAdd(p, m.item);   // its held item comes back to the Bag
        p.mons.erase(p.mons.begin() + idx);
        Toast(g, m.item.empty() ? "Bye-bye, " + m.species + "!" : "Bye-bye, " + m.species + "! (Its " + m.item + " went back to your Bag.)");
        SaveOrWarn(g);
        PlaySfx(Sfx::MenuBack);
        if (TabList(p, g.boxTab).empty()) { g.boxTab = 0; g.myCreSel = 0; }
        return;
    }

    if (g.itemPick && n > 0) {
        auto items = HeldItemsInBag(p);
        int N = (int)items.size() + 1;  // + take off / cancel
        if (in.up) { g.itemSel = (g.itemSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.down) { g.itemSel = (g.itemSel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.back) { g.itemPick = false; PlaySfx(Sfx::MenuBack); return; }
        if (!in.confirm) return;
        OwnedMon& m = p.mons[list[g.myCreSel]];
        if (g.itemSel == 0) {
            if (!m.item.empty()) {
                BagAdd(p, m.item);
                Toast(g, "Took the " + m.item + " from " + m.species + ".");
                m.item.clear();
            }
        } else {
            std::string it = items[g.itemSel - 1];
            BagTake(p, it);
            if (!m.item.empty()) BagAdd(p, m.item);
            m.item = it;
            Toast(g, m.species + " is now holding the " + it + ".");
        }
        g.itemPick = false;
        SaveOrWarn(g);
        PlaySfx(Sfx::MenuOk);
        return;
    }

    if (g.stonePick && n > 0) {
        OwnedMon& m = p.mons[list[g.myCreSel]];
        auto stones = UsableStones(p, m);
        int N = (int)stones.size() + 1;  // + cancel
        if (in.up) { g.itemSel = (g.itemSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.down) { g.itemSel = (g.itemSel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.back || (in.confirm && g.itemSel == N - 1)) { g.stonePick = false; PlaySfx(Sfx::MenuBack); return; }
        if (!in.confirm) return;
        g.stonePick = false;
        g.moveOrder = false;
        int to = StoneEvolution(MonSpecies(m), stones[g.itemSel]);
        if (to < 0) return;
        BagTake(p, stones[g.itemSel]);
        SaveOrWarn(g);
        g.evoQueue.clear();
        g.learnQueue.clear();
        g.rewardsPending = 0;
        g.rewardWho.clear();
        g.catchPending = false;
        g.evoQueue.push_back({list[g.myCreSel], to, g.coop ? g.coopActive : 0});
        g.returnToParty = true;
        g.evoT = 0;
        g.screen = Screen::Evolve;
        PlaySfx(Sfx::Evolve);
        return;
    }

    if (g.tmPick && n > 0) {
        int mi = list[g.myCreSel];
        auto tms = UsableTMs(p, p.mons[mi]);
        int N = (int)tms.size() + 1;  // + cancel
        if (in.up) { g.itemSel = (g.itemSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.down) { g.itemSel = (g.itemSel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.back || (in.confirm && g.itemSel == N - 1)) { g.tmPick = false; PlaySfx(Sfx::MenuBack); return; }
        if (!in.confirm) return;
        g.tmPick = false;
        const ItemDef* d = FindItem(tms[g.itemSel]);
        if (d) TeachMove(g, mi, d->move, 1, d->name, false);
        return;
    }

    if (g.moveOrder && n > 0) {   // pick a move, then the slot (button) to swap it with
        OwnedMon& m = p.mons[list[g.myCreSel]];
        int N = (int)m.moves.size();
        if (N == 0) { g.moveOrder = false; return; }
        g.moveOrdSel = std::max(0, std::min(g.moveOrdSel, N - 1));
        if (in.up) { g.moveOrdSel = (g.moveOrdSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.down) { g.moveOrdSel = (g.moveOrdSel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.back) {
            if (g.moveGrab >= 0) g.moveGrab = -1;
            else g.moveOrder = false;
            PlaySfx(Sfx::MenuBack);
            return;
        }
        if (!in.confirm) return;
        if (g.moveGrab < 0) { g.moveGrab = g.moveOrdSel; PlaySfx(Sfx::MenuOk); return; }
        if (g.moveGrab != g.moveOrdSel) {
            std::swap(m.moves[g.moveGrab], m.moves[g.moveOrdSel]);
            SaveOrWarn(g);
        }
        g.moveGrab = -1;
        PlaySfx(Sfx::MenuOk);
        return;
    }

    if (g.actMenu && n > 0) {
        auto opts = ActOptions(g);
        int N = (int)opts.size();
        if (in.up) { g.actSel = (g.actSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.down) { g.actSel = (g.actSel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.back) { g.actMenu = false; PlaySfx(Sfx::MenuBack); return; }
        if (!in.confirm) return;
        g.actMenu = false;
        int idx = list[g.myCreSel];
        switch (opts[g.actSel].first) {
            case ACT_ITEM: g.itemPick = true; g.itemSel = 0; break;
            case ACT_STONE: g.stonePick = true; g.itemSel = 0; break;
            case ACT_TM: g.tmPick = true; g.itemSel = 0; break;
            case ACT_MOVES: g.moveOrder = true; g.moveOrdSel = 0; g.moveGrab = -1; break;
            case ACT_UP: std::swap(p.mons[idx], p.mons[list[g.myCreSel - 1]]); g.myCreSel--; break;
            case ACT_DEPOSIT:
                p.mons[idx].boxed = true;
                Toast(g, p.mons[idx].species + " was sent to the Box.");
                break;
            case ACT_WITHDRAW: {
                OwnedMon m = p.mons[idx];
                m.boxed = false;
                p.mons.erase(p.mons.begin() + idx);
                int last = -1;
                for (int i = 0; i < (int)p.mons.size(); i++) if (!p.mons[i].boxed) last = i;
                p.mons.insert(p.mons.begin() + (last + 1), m);
                Toast(g, m.species + " joined the party.");
            } break;
            case ACT_RELEASE:
                g.releaseAsk = idx;
                g.releaseSel = 0;
                return;
            case ACT_SWAP:
                g.swapFrom = idx;
                g.boxTab = 0;
                g.myCreSel = 0;
                break;
            default: break;
        }
        SaveOrWarn(g);
        PlaySfx(Sfx::MenuOk);
        return;
    }

    if (g.swapFrom >= 0) {  // choosing who goes to the Box in exchange
        if (in.back) { g.swapFrom = -1; g.boxTab = 1; g.myCreSel = 0; PlaySfx(Sfx::MenuBack); return; }
        if (in.up) { g.myCreSel = (g.myCreSel + n - 1) % std::max(1, n); PlaySfx(Sfx::MenuMove); }
        if (in.down) { g.myCreSel = (g.myCreSel + 1) % std::max(1, n); PlaySfx(Sfx::MenuMove); }
        if (in.confirm && n > 0) {
            int a = list[g.myCreSel], b = g.swapFrom;
            std::string inName = p.mons[b].species, outName = p.mons[a].species;
            std::swap(p.mons[a], p.mons[b]);
            p.mons[a].boxed = false;
            p.mons[b].boxed = true;
            g.swapFrom = -1;
            Toast(g, inName + " joined the party. " + outName + " went to the Box.");
            SaveOrWarn(g);
            PlaySfx(Sfx::MenuOk);
        }
        return;
    }

    if (in.back) { PlaySfx(Sfx::MenuBack); EnterStoryHub(g); g.hubSel = 5; return; }
    if (in.left || in.right) { g.boxTab = 1 - g.boxTab; g.myCreSel = 0; PlaySfx(Sfx::MenuMove); return; }
    if ((in.sort || in.filter) && g.boxTab == 1) {
        if (in.sort) p.boxSort = (p.boxSort + 1) % 6;
        if (in.filter) {
            auto f = BoxFilters(p);
            int at = 0;
            for (int i = 0; i < (int)f.size(); i++) if (f[i] == p.boxFilter) at = i;
            p.boxFilter = f[(at + 1) % f.size()];
        }
        g.myCreSel = 0;
        SaveOrWarn(g);
        PlaySfx(Sfx::MenuMove);
        return;
    }
    if (n == 0) return;
    if (in.up) { g.myCreSel = (g.myCreSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.myCreSel = (g.myCreSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.confirm) { g.actMenu = true; g.actSel = 0; PlaySfx(Sfx::MenuOk); }
}

void DrawMyCreatures(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    const Profile& p = g.profile;
    DrawTextCentered(g.coop ? TextFormat("%s: PARTY & BOX", p.name.c_str()) : "PARTY & BOX", VW / 2, 6, 20, UI_GOLD);
    const auto& mons = p.mons;
    auto list = TabList(p, g.boxTab);
    int n = (int)list.size();
    // Tabs
    const char* tabs[2] = {"Party", "Box"};
    static float tabX = 10;
    tabX += (10 + g.boxTab * 92 - tabX) * std::min(1.0f, GetFrameTime() * 14);
    if (Cfg().fxLevel >= 2) tabX = 10.0f + g.boxTab * 92;
    for (int i = 0; i < 2; i++) {
        int x = 10 + i * 92;
        DrawPanel(x, 30, 88, 16, UI_PANEL, Color{70, 64, 92, 255});
    }
    DrawRectangleRounded({tabX, 30, 88, 16}, 0.35f, 6, UI_PANEL_LIGHT);
    DrawRectangleRoundedLinesEx({tabX, 30, 88, 16}, 0.35f, 6, 1.0f, UI_GOLD);
    DrawRectangle((int)tabX + 10, 44, 68, 2, UI_GOLD);
    for (int i = 0; i < 2; i++) {
        int x = 10 + i * 92;
        bool sel = g.boxTab == i;
        const char* t = i == 0 ? TextFormat("%s %d/%d", tabs[i], PartyCount(p), PARTY_SIZE)
                               : TextFormat("%s (%d)", tabs[i], (int)mons.size() - PartyCount(p));
        DrawTextCentered(t, x + 44, 33, 10, sel ? UI_GOLD : UI_DIM, false);
    }
    bool boxTools = g.boxTab == 1 && (int)mons.size() > PartyCount(p);
    int listY = boxTools ? 66 : 50, listRows = boxTools ? 11 : 12;
    if (boxTools) {   // sort and filter
        GDrawText(TextFormat("Sort: %s", kBoxSorts[std::max(0, std::min(5, p.boxSort))]), 12, 50, 10, p.boxSort ? UI_GOLD : UI_DIM);
        std::string f = "Show: " + BoxFilterName(p.boxFilter);
        if (!p.boxFilter.empty()) f += TextFormat(" (%d)", n);
        while (TextW(f.c_str(), 10) > 92 && f.size() > 4) f = f.substr(0, f.size() - 4) + "...";
        GDrawText(f.c_str(), 98, 50, 10, p.boxFilter.empty() ? UI_DIM : Color{140, 220, 255, 255});
    }
    int sel = std::max(0, std::min(g.myCreSel, n - 1));
    int first = std::max(0, std::min(sel - 5, n - listRows));
    if (n > listRows) {   // scroll position
        float f0 = (float)first / n, f1 = (float)(first + listRows) / n;
        DrawRectangle(192, listY + (int)(f0 * listRows * 24), 2, std::max(4, (int)((f1 - f0) * listRows * 24)), UI_DIM);
    }
    if (g.boxTab == 1) {   // Box wallpaper: a checker of soft tiles in the season's colour
        static const Color wall[4] = {{120, 170, 120, 255}, {90, 150, 200, 255}, {190, 120, 70, 255}, {140, 160, 200, 255}};
        Color wc = wall[SeasonOf(p.clock)];
        int wy = listY - 2, wh = listRows * 24 + 2;
        DrawRectangleRounded({6, (float)wy, 188, (float)wh}, 0.04f, 6, Fade(wc, 0.35f));
        BeginScissorMode(6, wy, 188, wh);
        float off = std::fmod((float)GetTime() * 6.0f, 24.0f);
        for (int yy = -1; yy < wh / 12 + 2; yy++)
            for (int xx = -1; xx < 188 / 12 + 2; xx++)
                if (((xx + yy) & 1) == 0) DrawRectangle(6 + xx * 12 + (int)off / 2, wy + yy * 12 + (int)off / 2, 12, 12, Fade(wc, 0.18f));
        EndScissorMode();
        DrawRectangleRoundedLinesEx({6, (float)wy, 188, (float)wh}, 0.04f, 6, 1.0f, Fade(wc, 0.8f));
    }
    // a hop when a different Pokemon is picked (visual only)
    static int hopKey = -1;
    static double hopAt = 0;
    int key = n > 0 ? list[sel] * 4 + g.boxTab : -1;
    if (key != hopKey) { hopKey = key; hopAt = GetTime(); }
    float hu = (float)std::min(1.0, (GetTime() - hopAt) / 0.38);
    float hop = Cfg().fxLevel >= 2 ? 0 : std::sin(hu * PI) * 7.0f * (1 - hu * 0.3f);
    for (int k = first; k < std::min(n, first + listRows); k++) {
        int i = list[k];
        float e = UiEnterT(k - first, 0.03f);
        int y = listY + (k - first) * 24;
        int x = 10 - (int)std::lround((1 - e) * 60);
        bool on = sel == k;
        int idx = MonSpecies(mons[i]);
        Color tc = idx >= 0 ? TypeColor(AllSpecies()[idx].type1) : UI_PANEL;
        Rectangle rc{(float)x + (on ? 3 : 0), (float)y, 180, 22};
        DrawRectangleRounded({rc.x + 1, rc.y + 1.5f, rc.width, rc.height}, 0.3f, 6, Fade(BLACK, 0.3f * e));
        DrawRectangleRounded(rc, 0.3f, 6, Fade(on ? UI_PANEL_LIGHT : UI_PANEL, 0.95f * e));
        DrawRectangleGradientH((int)rc.x + 1, y + 1, 60, 20, Fade(tc, (on ? 0.5f : 0.28f) * e), Fade(tc, 0));
        DrawRectangleRoundedLinesEx(rc, 0.3f, 6, 1.0f, Fade(on ? UI_GOLD : Color{70, 64, 92, 255}, e));
        int sx = (int)rc.x + 14;
        if (idx >= 0) {
            DrawEllipse(sx, y + 20, 7, 1.6f, Fade(BLACK, 0.3f * e));
            DrawSpeciesFit(ShinyIdx(idx, mons[i].shiny), VIEW_FRONT, sx, y + 21 - (on ? (int)hop / 2 : 0), 18, mons[i].hp > 0 ? WHITE : Color{90, 90, 100, 255});
        }
        if (mons[i].shiny) DrawShinyStar(rc.x + 4, (float)y + 5, 3);
        GDrawText(mons[i].species.c_str(), (int)rc.x + 30, y + 6, 10, Fade(on ? UI_GOLD : UI_TEXT, e));
        GDrawText(TextFormat("Lv%d", mons[i].level), (int)rc.x + 140, y + 3, 10, Fade(UI_GOLD, e));
        DrawHpBar((int)rc.x + 140, y + 15, 34, 2, mons[i].hp);
        if (!mons[i].item.empty()) DrawRectangle((int)rc.x + 130, y + 4, 5, 5, Color{140, 220, 255, 255});
    }
    if (n == 0 && boxTools) {
        GDrawText("Nothing in the Box matches this filter.", 16, 74, 10, UI_DIM);
        GDrawText("R / Y: change the filter", 16, 88, 10, UI_DIM);
        GDrawText("Left/Right switch tab   Tab sort   R filter   Esc back", 212, 340, 10, UI_DIM);
        DrawToast(g);
        return;
    }
    if (n == 0) {
        GDrawText(g.boxTab == 1 ? "Your Box is empty." : "No Pokemon in the party.", 16, 58, 10, UI_DIM);
        if (g.boxTab == 1) {
            GDrawText("Pokemon you catch or receive", 16, 74, 10, UI_DIM);
            GDrawText("go here when your party is full.", 16, 86, 10, UI_DIM);
        }
        GDrawText("Left/Right switch tab   Esc back", 212, 340, 10, UI_DIM);
        DrawToast(g);
        return;
    }
    const OwnedMon& m = mons[list[sel]];
    int idx = MonSpecies(m);
    DrawPanel(200, 30, 430, 304);
    if (idx < 0) {
        GDrawText(TextFormat("%s is missing from the Pokemon folder.", m.species.c_str()), 212, 50, 10, Color{255, 120, 100, 255});
        DrawToast(g);
        return;
    }
    const SpeciesDef& sp = AllSpecies()[idx];
    {   // the portrait on a backdrop in its type's colours, with rays and a hop when picked
        Color c1 = TypeColor(sp.type1), c2 = sp.type2 != Type::None ? TypeColor(sp.type2) : c1;
        Rectangle pr{212, 42, 80, 80};
        DrawRectangleRounded(pr, 0.12f, 6, UI_PANEL_LIGHT);
        DrawRectangleGradientV(213, 43, 78, 78, Fade(c1, 0.55f), Fade(c2, 0.25f));
        BeginScissorMode(212, 42, 80, 80);
        float rt = (float)GetTime() * 12.0f;
        for (int k = 0; k < 12; k++) {
            float a0 = (rt + k * 30) * DEG2RAD, a1 = a0 + 12 * DEG2RAD;
            Vector2 c{252, 84};
            DrawTriangle(c, {c.x + std::cos(a1) * 80, c.y + std::sin(a1) * 80}, {c.x + std::cos(a0) * 80, c.y + std::sin(a0) * 80}, Fade(WHITE, 0.07f));
        }
        DrawCircleGradient(252, 84, 36, Fade(WHITE, 0.22f), Fade(WHITE, 0));
        EndScissorMode();
        DrawEllipse(252, 116, 22, 4, Fade(BLACK, 0.3f));
        float breathe = 1.0f + std::sin((float)GetTime() * 2.2f) * 0.015f;
        DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, 252, 118 - (int)hop, (int)(72 * breathe));
        DrawRectangleRoundedLinesEx(pr, 0.12f, 6, 1.0f, Fade(c1, 0.9f));
    }
    if (m.shiny) { DrawShinyStar(220, 50, 5); GDrawText("Shiny!", 228, 46, 10, Color{255, 214, 80, 255}); }
    DrawTextShadow(sp.name.c_str(), 302, 44, 20, UI_TEXT);
    int bx = 302;
    bx += DrawTypeBadge(sp.type1, bx, 68) + 2;
    if (sp.type2 != Type::None) DrawTypeBadge(sp.type2, bx, 68);
    GDrawText(TextFormat("Level %d", m.level), 302, 86, 10, UI_GOLD);
    if (m.level < MAX_LEVEL) GDrawText(TextFormat("%d XP to next level", XpForLevel(m.level + 1) - m.xp), 370, 86, 10, UI_DIM);
    DrawXpBar(m, 302, 100, 200);
    std::string hint = EvolutionHint(idx);
    if (!hint.empty()) {
        if (TextW(hint.c_str(), 10) > 320) hint = hint.substr(0, 60) + "...";
        GDrawText(hint.c_str(), 302, 110, 10, Color{140, 220, 255, 255});
    }
    {
        std::string abn = MonAbility(m);
        Ab ab = AbilityByName(abn);
        if (ab != Ab::None) {
            std::string line = abn + (m.abilitySlot == 2 ? " (hidden)" : "") + ": " + AbilityInfo(ab).desc;
            while (TextW(line.c_str(), 10) > 320 && line.size() > 4) line = line.substr(0, line.size() - 4) + "...";
            GDrawText(line.c_str(), 302, 120, 10, Color{255, 225, 130, 255});
        }
    }
    GDrawText(TextFormat("Battles %d   Wins %d   KOs %d", m.battles, m.wins, m.kos), 470, 44, 10, UI_DIM);
    DrawFriendship(m.friendship, 470, 82);
    GDrawText(m.hp <= 0 ? "Fainted" : TextFormat("HP %d%%", (int)std::round(m.hp * 100)), 470, 58, 10, m.hp <= 0 ? Color{255, 120, 100, 255} : UI_TEXT);
    DrawHpBar(530, 60, 90, 4, m.hp);
    if (!m.ot.empty()) GDrawText(("Traded from " + m.ot).c_str(), 470, 70, 10, Color{255, 150, 230, 255});
    else if (m.caught) GDrawText("Caught in the wild", 470, 70, 10, Color{140, 220, 150, 255});
    if (m.boxed) GDrawText("In the Box", 576, 70, 10, UI_DIM);

    float mul = StatMul(m.level);
    {   // nature: the raised stat's label is red, the lowered one blue
        static const char* kShort[5] = {"ATK", "DEF", "SPD", "SpA", "SpD"};
        int nu = NatureUp(m.nature), nd = NatureDown(m.nature);
        std::string nt = std::string(NatureName(m.nature)) + " nature";
        GDrawText(nt.c_str(), 212, 132, 10, Color{255, 200, 150, 255});
        if (nu >= 0) {
            int x = 212 + TextW(nt.c_str(), 10) + 6;
            GDrawText(TextFormat("+%s", kShort[nu]), x, 132, 10, Color{255, 110, 100, 255});
            GDrawText(TextFormat("-%s", kShort[nd]), x + 30, 132, 10, Color{110, 160, 255, 255});
        } else GDrawText("(no change)", 212 + TextW(nt.c_str(), 10) + 6, 132, 10, UI_DIM);
    }
    auto mark = [&](int st) { return NatureUp(m.nature) == st ? 1 : NatureDown(m.nature) == st ? -1 : 0; };
    auto sv = [&](int b, int st) { return (int)std::lround(Scaled(b) * mul * NatureMul(m.nature, st)); };
    int stHp = (int)std::lround(Scaled(sp.hp) * mul), stAtk = sv(sp.atk, NS_ATK), stDef = sv(sp.def, NS_DEF);
    int stSpe = (int)std::lround(Scaled(sp.spd) * NatureMul(m.nature, NS_SPE)), stSpa = sv(sp.SpAtk(), NS_SPA), stSpd = sv(sp.SpDef(), NS_SPD);
    DrawStatBar("HP", stHp, 212, 146, 0);
    DrawStatBar("ATK", stAtk, 212, 158, 0, mark(NS_ATK));
    DrawStatBar("DEF", stDef, 212, 170, 0, mark(NS_DEF));
    DrawStatBar("SPD", stSpe, 212, 182, 0, mark(NS_SPE));
    DrawStatBar("SpA", stSpa, 268, 158, 0, mark(NS_SPA));
    DrawStatBar("SpD", stSpd, 268, 170, 0, mark(NS_SPD));
    {
        int hv[6] = {stHp, stAtk, stDef, stSpe, stSpd, stSpa};
        float grow = 0.25f + 0.75f * (1 - (1 - hu) * (1 - hu));
        if (Cfg().fxLevel >= 2) grow = 1;
        DrawStatHex(364, 177, 21, hv, TypeColor(sp.type1), grow);
        static const char* hl[6] = {"HP", "ATK", "DEF", "SPD", "SpD", "SpA"};
        for (int k = 0; k < 6; k++) {
            float an = (-90 + k * 60) * DEG2RAD;
            int lx = 364 + (int)(std::cos(an) * 34), ly = 177 + (int)(std::sin(an) * 28) - 4;
            GDrawText(hl[k], lx - TextW(hl[k], 10) / 2, ly, 10, Fade(UI_DIM, 0.85f));
        }
    }

    GDrawText("Moves", 212, 200, 10, UI_DIM);
    auto mv = MonMoves(m);
    for (size_t k = 0; k < 4; k++) {
        if (k < mv.size()) DrawMoveLine(mv[k], 212, 214 + (int)k * 22, 400, true);
        else GDrawText("-", 222, 214 + (int)k * 22, 10, UI_DIM);
    }
    for (const auto& e : sp.learnset) {
        if (e.level > m.level) {
            GDrawText(TextFormat("Next: learns %s at level %d", e.move.name.c_str(), e.level), 212, 306, 10, Color{140, 220, 255, 255});
            break;
        }
    }
    GDrawText("Held item", 420, 132, 10, UI_DIM);
    const ItemDef* held = FindItem(m.item);
    GDrawText(held ? held->name.c_str() : "(none)", 420, 146, 10, held ? Color{140, 220, 255, 255} : UI_DIM);
    if (held) {
        std::string desc = held->desc, line, word;
        int ly = 160;
        for (size_t i = 0; i <= desc.size(); i++) {
            if (i == desc.size() || desc[i] == ' ') {
                std::string t2 = line.empty() ? word : line + " " + word;
                if (TextW(t2.c_str(), 10) > 196) { GDrawText(line.c_str(), 420, ly, 10, UI_DIM); ly += 11; line = word; }
                else line = t2;
                word.clear();
            } else word += desc[i];
        }
        if (!line.empty()) GDrawText(line.c_str(), 420, ly, 10, UI_DIM);
    }
    if (g.swapFrom >= 0 && g.swapFrom < (int)mons.size()) {
        DrawRectangle(0, 336, VW, 24, Fade(UI_INK, 0.9f));
        GDrawText(TextFormat("Who goes to the Box so %s can join?   Enter swap   Esc cancel", mons[g.swapFrom].species.c_str()), 12, 342, 10, UI_GOLD);
    } else {
        GDrawText(g.boxTab == 1 ? "Left/Right Party / Box   Up/Down browse   Enter options   Tab (X) sort   R (Y) filter   Esc back"
                               : "Left/Right Party / Box   Up/Down browse   Enter options   Esc back", 12, 342, 10, UI_DIM);
    }

    if (g.actMenu) {
        auto opts = ActOptions(g);
        int N = (int)opts.size();
        int h = 28 + N * 16;
        int py = 180 - h / 2;
        DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.5f));
        DrawPanel(200, py, 240, h, UI_PANEL, UI_GOLD);
        GDrawText(m.species.c_str(), 210, py + 8, 10, UI_GOLD);
        for (int i = 0; i < N; i++) {
            int y = py + 24 + i * 16;
            bool on = g.actSel == i;
            if (on) DrawRectangle(204, y - 2, 232, 14, UI_PANEL_LIGHT);
            GDrawText(opts[i].second.c_str(), 212, y, 10, on ? UI_GOLD : UI_TEXT);
        }
    }
    if (g.releaseAsk >= 0 && g.releaseAsk < (int)mons.size()) {
        const OwnedMon& rm = mons[g.releaseAsk];
        DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.55f));
        DrawPanel(160, 120, 320, 110, UI_PANEL, Color{255, 120, 100, 255});
        int si = MonSpecies(rm);
        if (si >= 0) DrawSpeciesFit(ShinyIdx(si, rm.shiny), VIEW_FRONT, 196, 176, 44);
        GDrawText(TextFormat("Release %s (Lv%d)?", rm.species.c_str(), rm.level), 232, 132, 10, UI_GOLD);
        DrawWrapped(rm.shiny ? "It's shiny! Once it's released, it's gone for good." : "Once it's released into the wild, it's gone for good.", 232, 148, 236, 10, UI_TEXT);
        if (!rm.item.empty()) GDrawText(TextFormat("Its %s goes back to your Bag.", rm.item.c_str()), 232, 174, 10, UI_DIM);
        const char* opt[2] = {"No, keep it", "Yes, release it"};
        for (int k = 0; k < 2; k++) {
            int bx = 232 + k * 120, by = 196;
            bool on = g.releaseSel == k;
            DrawPanel(bx, by, 110, 20, on ? UI_PANEL_LIGHT : UI_PANEL, on ? (k ? Color{255, 120, 100, 255} : UI_GOLD) : Color{70, 64, 92, 255});
            DrawTextCentered(opt[k], bx + 55, by + 5, 10, on ? (k ? Color{255, 140, 120, 255} : UI_GOLD) : UI_TEXT, false);
        }
    }
    if (g.moveOrder) {
        int N = (int)m.moves.size();
        int h = 60 + N * 22;
        int py = 180 - h / 2;
        DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.5f));
        DrawPanel(150, py, 340, h, UI_PANEL, UI_GOLD);
        GDrawText(TextFormat("%s: which button uses which move", m.species.c_str()), 160, py + 8, 10, UI_GOLD);
        bool online = g.on.active;
        int who = g.coop && !online ? g.coopWho : 0;
        bool tp = g.coop && !online;
        for (int i = 0; i < N; i++) {
            int y = py + 26 + i * 22;
            bool on = g.moveOrdSel == i, grabbed = g.moveGrab == i;
            if (on) DrawRectangle(154, y - 3, 332, 20, UI_PANEL_LIGHT);
            if (grabbed) DrawRectangleLines(154, y - 3, 332, 20, UI_GOLD);
            const char* key = AttackKeyLabel(who, i, tp);
            DrawPanel(160, y - 1, 34, 16, UI_INK, grabbed ? UI_GOLD : Color{90, 84, 120, 255});
            GDrawText(key, 177 - TextW(key, 10) / 2, y + 2, 10, UI_GOLD);
            const MoveDef* md = FindMove(m.moves[i]);
            if (md) DrawMoveLine(*md, 200, y + 2, 280, false);
            else GDrawText(m.moves[i].c_str(), 204, y + 2, 10, UI_TEXT);
        }
        GDrawText(g.moveGrab < 0 ? "Enter pick a move   Esc done" : "Enter swap it here   Esc cancel", 160, py + h - 16, 10, UI_DIM);
    }
    if (g.stonePick) {
        auto stones = UsableStones(p, m);
        int N = (int)stones.size() + 1;
        int h = 30 + N * 16;
        int py = 180 - h / 2;
        DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.5f));
        DrawPanel(190, py, 260, h, UI_PANEL, UI_GOLD);
        GDrawText(TextFormat("Use a stone on %s", m.species.c_str()), 200, py + 8, 10, UI_GOLD);
        for (int i = 0; i < N; i++) {
            int y = py + 24 + i * 16;
            bool on = g.itemSel == i;
            if (on) DrawRectangle(194, y - 2, 252, 14, UI_PANEL_LIGHT);
            if (i == N - 1) { GDrawText("Cancel", 202, y, 10, on ? UI_GOLD : UI_TEXT); continue; }
            int to = StoneEvolution(idx, stones[i]);
            GDrawText(TextFormat("%s (x%d)  ->  %s", stones[i].c_str(), BagCount(p, stones[i]), to >= 0 ? AllSpecies()[to].name.c_str() : "?"),
                     202, y, 10, on ? UI_GOLD : UI_TEXT);
        }
    }
    if (g.tmPick) {
        auto tms = UsableTMs(p, m);
        int N = (int)tms.size() + 1;
        int rows = std::min(N, 14);
        int first = std::max(0, std::min(g.itemSel - rows / 2, N - rows));
        int h = 44 + rows * 16;
        int py = std::max(8, 180 - h / 2);
        DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.5f));
        DrawPanel(150, py, 340, h, UI_PANEL, UI_GOLD);
        GDrawText(TextFormat("Teach %s a move with a TM", m.species.c_str()), 160, py + 8, 10, UI_GOLD);
        GDrawText(m.moves.size() < 4 ? "It has a free move slot." : "It knows 4 moves - you'll pick one to forget.", 160, py + 20, 10, UI_DIM);
        for (int r = 0; r < rows; r++) {
            int i = first + r;
            int y = py + 36 + r * 16;
            bool on = g.itemSel == i;
            if (on) DrawRectangle(154, y - 2, 332, 14, UI_PANEL_LIGHT);
            if (i == N - 1) { GDrawText("Cancel", 162, y, 10, on ? UI_GOLD : UI_TEXT); continue; }
            const ItemDef* d = FindItem(tms[i]);
            const MoveDef* mv = d ? FindMove(d->move) : nullptr;
            if (mv) DrawRectangle(162, y + 1, 8, 8, TypeColor(mv->type));
            GDrawText(TextFormat("%s  (x%d)", tms[i].c_str(), BagCount(p, tms[i])), 176, y, 10, on ? UI_GOLD : UI_TEXT);
            if (mv && mv->power > 0) GDrawText(TextFormat("pow %d", mv->power), 440, y, 10, UI_DIM);
        }
    }
    if (g.itemPick) {
        auto items = HeldItemsInBag(p);
        int N = (int)items.size() + 1;
        int h = 30 + N * 16;
        int py = std::max(20, 180 - h / 2);
        DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.5f));
        DrawPanel(170, py, 300, h, UI_PANEL, UI_GOLD);
        GDrawText(TextFormat("Give %s an item", m.species.c_str()), 180, py + 8, 10, UI_GOLD);
        for (int i = 0; i < N; i++) {
            int y = py + 24 + i * 16;
            bool on = g.itemSel == i;
            if (on) DrawRectangle(174, y - 2, 292, 14, UI_PANEL_LIGHT);
            if (i == 0) GDrawText(m.item.empty() ? "(cancel)" : TextFormat("Take off %s", m.item.c_str()), 182, y, 10, on ? UI_GOLD : UI_TEXT);
            else {
                const ItemDef* d = FindItem(items[i - 1]);
                GDrawText(TextFormat("%s  x%d", items[i - 1].c_str(), BagCount(p, items[i - 1])), 182, y, 10, on ? UI_GOLD : UI_TEXT);
                if (d && on) GDrawText(d->desc.c_str(), 182, py + h + 4, 10, UI_TEXT);
            }
        }
        if (N == 1) GDrawText("No held items in your bag - buy some at the Shop.", 182, py + h + 4, 10, UI_DIM);
    }
    DrawToast(g);
}

// ------------------------------------------------------------------ after a story battle

namespace { void SnorlaxReward(Game& g, int encounter, float dmg0, float dmg1, int level); void JoinReward(Game& g, const StageDef& st, float dmg0, float dmg1, int level);
void StoryGifts(Game& g, const StageDef& st);
// Where a region's Champion moment is: its finale if it has one (Ghetsis after N), else its Champion.
bool ChampionMoment(const StageDef& s) {
    if (s.finale) return true;
    if (!s.IsChampion() || s.postgame) return false;
    const auto& R = RegionInfo(s.region);
    for (int i = R.first; i < R.end; i++) if (Story().stages[i].finale) return false;
    return true;
}
}
void ProcessStoryBattleEnd(Game& g) {
    const Battle& b = g.battle;
    const bool coop = g.coop;
    g.won = b.Winner() == 0;
    g.resultRows.clear();
    g.resultLines.clear();
    g.learnQueue.clear();
    g.evoQueue.clear();
    g.rewardsPending = 0;
    g.rewardWho.clear();
    g.catchCands.clear();
    g.catchPending = false;
    g.catchOrder.clear();
    g.giftQueue.clear();

    // Shared progress lives in the co-op save, or in the profile when playing alone.
    int& stage = coop ? g.coopSave.storyStage : g.profile.storyStage;
    std::vector<std::string>& badges = coop ? g.coopSave.badges : g.profile.badges;
    int& wins = coop ? g.coopSave.wins : g.profile.wins;
    int& losses = coop ? g.coopSave.losses : g.profile.losses;
    int& trainingWins = coop ? g.coopSave.trainingWins : g.profile.trainingWins;

    StoryTimeStep(g, true);
    // The after-battle stats screen: everyone who was sent out.
    g.statRows.clear();
    for (int s = 0; s < b.NumSlots(); s++) {
        const Side& sd = b.GetSide(s);
        for (const Fighter& f : sd.team) {
            if (!f.sentOut || f.lent) continue;
            Game::StatRow r;
            r.team = sd.teamId;
            r.who = sd.teamId == 0 && coop ? (f.owner & 1) : 0;
            r.species = f.RealSpecies();
            r.level = f.level;
            r.dealt = (int)std::lround(f.dmgDealt);
            r.taken = (int)std::lround(f.dmgTaken);
            r.kos = f.kos;
            r.fainted = f.hp <= 0 && !f.retired;
            r.tag = sd.teamId == 0 ? (coop ? Prof(g, r.who).name : g.profile.name) : sd.label;
            g.statRows.push_back(r);
        }
    }
    auto foes = b.TeamFighters(1);
    for (int w = 0; w < (coop ? 2 : 1); w++)
        if (OnlineMine(g, w))
            for (const Fighter* f : foes)
                if (f->sentOut) DexSee(Prof(g, w), AllSpecies()[f->RealSpecies()].name);
    // Exp. Share: after a win every Pokemon you brought gets a share of the foes' XP - more for
    // the ones that fought, less for the ones that sat on the bench.
    int bonus = 0;
    if (g.won) for (const Fighter* f : foes) bonus += KoXp(*f->sp, f->level, b.Setup().trainerBattle);
    int players = coop ? 2 : 1;
    bool amulet[2] = {false, false};
    float damage[2] = {0, 0};
    int myAvg = 0, myN = 0;
    struct Took { int who, mon; bool fought, fainted; };
    std::vector<Took> took;
    int brought[2] = {0, 0}, standing[2] = {0, 0};

    for (int s = 0; s < b.NumSlots(); s++) {
        const Side& sd = b.GetSide(s);
        if (sd.teamId != 0 || !sd.earnXp) continue;
        int slotOwner = coop && !sd.team.empty() ? (sd.team[0].owner & 1) : 0;
        damage[slotOwner] += sd.damageDealt;
        for (const Fighter& f : sd.team) {
            if (f.lent) continue;  // its copy (with the real result) is in the partner's slot
            int who = coop ? (f.owner & 1) : 0;
            Profile& P = Prof(g, who);
            if (f.ownedId < 0 || f.ownedId >= (int)P.mons.size()) continue;
            OwnedMon& m = P.mons[f.ownedId];
            ResultRow row;
            row.who = who;
            row.mon = f.ownedId;
            row.levelFrom = f.startLevel;
            m.level = f.level;
            m.xp = f.xp;
            m.species = AllSpecies()[f.RealSpecies()].name;  // a transformed Ditto changes back
            m.moves.clear();
            if (f.transformFrom >= 0) for (int i = 0; i < f.origN; i++) m.moves.push_back(f.origMoves[i].name);
            else for (int i = 0; i < f.nMoves; i++) m.moves.push_back(f.moves[i].name);
            if (!g.tower) m.hp = f.hp > 0 ? std::max(0.01f, std::min(1.0f, f.hp / (float)f.maxHp)) : 0.0f;   // the Tower doesn't touch HP
            row.xp = f.xpGained;
            took.push_back({who, f.ownedId, f.sentOut, f.hp <= 0});
            brought[who]++;
            if (f.hp > 0) standing[who]++;
            if (f.sentOut) {
                m.battles++;
                if (g.won) m.wins++;
                amulet[who] |= f.Holds(ItemFx::AmuletCoin);
                myAvg += f.level;
                myN++;
            }
            m.kos += f.kos;
            if (coop) {   // the co-op leaderboard
                g.coopSave.run[who].kos += f.kos;
                if (f.sentOut && f.hp <= 0) g.coopSave.run[who].faints++;
            }
            for (const auto& e : f.evolvedNames) g.resultLines.push_back(e + "!");
            for (const auto& mv : f.learnedNow) g.resultLines.push_back(m.species + " learned " + mv + "!");
            bool mine = OnlineMine(g, who);
            if (mine) for (const auto& mv : f.pendingLearn) g.learnQueue.push_back({f.ownedId, mv, who});

            if (g.won && bonus > 0) {
                Growth gr;
                int share = (int)std::lround(bonus * (f.sentOut ? 0.35f : 0.2f));
                int b2 = f.Holds(ItemFx::LuckyEgg) ? (int)(share * f.item->value) : share;
                AddXp(m, b2, gr);
                row.xp += b2;
                for (const auto& mv : gr.learned) g.resultLines.push_back(m.species + " learned " + mv + "!");
                if (mine) for (const auto& mv : gr.wantsToLearn) g.learnQueue.push_back({f.ownedId, mv, who});
                if (mine && gr.evolveTo >= 0) g.evoQueue.push_back({f.ownedId, gr.evolveTo, who});
            }
            // Catch any evolution that's due (e.g. an evolution added later in a pokemon.txt).
            int idx = MonSpecies(m);
            int evo = idx >= 0 ? EvolutionIndexFor(idx, m.nature) : -1;
            bool queued = false;
            for (const auto& e : g.evoQueue) queued |= e.mon == f.ownedId && e.who == who;
            if (mine && evo >= 0 && !queued && m.level >= AllSpecies()[idx].evolveLevel) g.evoQueue.push_back({f.ownedId, evo, who});

            row.levelTo = m.level;
            row.name = m.species;
            if (f.startSpecies != f.RealSpecies()) row.note = "evolved!";
            else if (!f.sentOut) row.note = "stayed on the bench";
            else if (f.hp <= 0) row.note = "fainted";
            if (f.sentOut || row.xp > 0) g.resultRows.push_back(row);
        }
    }

    if (coop) {   // leaderboard: damage, battles, and the MVP (who dealt the most)
        for (int w = 0; w < 2; w++) {
            g.coopSave.run[w].damage += (int)damage[w];
            if (brought[w] > 0) g.coopSave.run[w].battles++;
        }
        if (std::fabs(damage[0] - damage[1]) > 0.5f) g.coopSave.run[damage[1] > damage[0] ? 1 : 0].mvp++;
    }
    // Friendship (party creatures only): wins raise it, fainting lowers it, a wipe-out lowers it a lot.
    for (int w = 0; w < players; w++) {
        Profile& P = Prof(g, w);
        bool wiped = brought[w] > 0 && standing[w] == 0;
        for (int i = 0; i < (int)P.mons.size(); i++) {
            OwnedMon& m = P.mons[i];
            if (m.boxed) continue;
            const Took* t = nullptr;
            for (const auto& k : took) if (k.who == w && k.mon == i) t = &k;
            int d = 0;
            if (t && t->fainted) d = wiped ? -15 : -5;
            else if (wiped) d = -8;
            else if (g.won) d = (t && t->fought) ? 5 : 2;
            m.friendship = std::max(0, std::min(MAX_FRIENDSHIP, m.friendship + d));
            // Friendship evolutions happen after a win.
            int idx = MonSpecies(m);
            int to = g.won ? FriendshipEvolution(idx, m.friendship, IsNight(StoryClock(g)) ? 1 : 0) : -1;
            bool queued = false;
            for (const auto& e : g.evoQueue) queued |= e.mon == i && e.who == w;
            if (to >= 0 && !queued && OnlineMine(g, w)) g.evoQueue.push_back({i, to, w});
        }
        if (wiped) g.resultLines.push_back(coop ? P.name + "'s team was wiped out - their Pokemon trust them a little less."
                                                : "Your team was wiped out - your Pokemon trust you a little less.");
        // The first time one of your Pokemon loves you completely, a Pokemon fan gives you an Eevee.
        if (OnlineMine(g, w) && !P.eeveeGift) {
            const OwnedMon* best = nullptr;
            for (const auto& m : P.mons) if (m.friendship >= MAX_FRIENDSHIP) { best = &m; break; }
            int ev = SpeciesIndex("Eevee");
            if (best && ev >= 0) {
                std::string who = best->species;
                P.eeveeGift = true;
                bool boxed = AddMon(P, NewMon(ev, NewMonLevel(P)));
                SaveWho(g, w);
                g.giftQueue.push_back({w, ev, (coop ? P.name + ": a" : std::string("A")) + " Pokemon fan saw how much your " + who +
                                                  " loves you and gave you an Eevee! It " + (boxed ? "was sent to the Box." : "joined your team!")});
            }
        }
    }

    int foeLv = 0;
    for (const Fighter* f : foes) foeLv += f->level;
    foeLv = foes.empty() ? 5 : foeLv / (int)foes.size();
    bool storyBattle = (g.mode == Mode::Story || g.mode == Mode::CoopStory) && g.stageIdx >= 0 && !g.tower;
    bool training = g.mode == Mode::Training || g.mode == Mode::CoopTraining;
    StageDef stv;
    bool legend = g.legend >= 0;
    if (storyBattle || g.tower || legend) stv = ActiveStage(g);
    const StageDef* st = storyBattle || g.tower || legend ? &stv : nullptr;
    if (g.won) {
        // Coins: every player gets their own full share.
        int coins = WinCoins(st, foeLv);
        if (training && g.rareBattle) coins *= 2;
        if (b.payDayCoins > 0) g.resultLines.push_back(TextFormat("Picked up %d coins from Pay Day!", b.payDayCoins));
        for (int w = 0; w < players; w++) {
            int c = amulet[w] ? coins * 3 / 2 : coins;
            c += b.payDayCoins;
            Profile& P = Prof(g, w);
            P.coins += c;
            if (coop) g.resultLines.push_back(TextFormat("%s got %d coins! (%d total)", P.name.c_str(), c, P.coins));
            else g.resultLines.push_back(TextFormat("You got %d coins! (%d total)", c, P.coins));
        }
        if (storyBattle && st->IsBoss() && g.stageIdx < stage) g.resultLines.push_back("You won the rematch against " + st->trainer + "!");
        if (storyBattle && st->IsGym() && std::find(badges.begin(), badges.end(), st->badge) == badges.end()) {
            badges.push_back(st->badge);
            g.resultLines.push_back(std::string(coop ? "You both earned the " : "You earned the ") + st->badge + " Badge!");
        }
        if (legend) {
            LegendBattleOver(g, b, damage);
        } else if (b.Setup().catchable) {
            myAvg = myN ? myAvg / myN : foeLv;
            for (const Fighter* fp : foes) {
                const Fighter& f = *fp;
                if (f.hp <= 0) continue;  // knocked out: chance lost
                Game::CatchCand c;
                c.species = f.RealSpecies();
                c.level = f.level;
                if (f.transformFrom >= 0) for (int i = 0; i < f.origN; i++) c.moves.push_back(f.origMoves[i].name);
                else for (int i = 0; i < f.nMoves; i++) c.moves.push_back(f.moves[i].name);
                c.hpFrac = std::max(0.01f, f.hp / (float)f.maxHp);
                c.abSlot = f.abSlot;
                c.nature = f.nature;
                c.shiny = f.shiny;
                float ch = 0.35f + 0.55f * (1.0f - std::min(1.0f, c.hpFrac / 0.25f));
                ch *= std::max(0.6f, std::min(1.3f, 1.0f + (myAvg - f.level) * 0.03f));
                if (!IsBaseForm(c.species)) ch *= 0.8f;
                c.chance = std::max(0.05f, std::min(0.95f, ch));
                g.catchCands.push_back(c);
            }
            g.catchPending = !g.catchCands.empty();
            if (coop) {
                // Catch draft: whoever dealt more damage picks first.
                int first = damage[1] > damage[0] ? 1 : 0;
                g.catchOrder = {first, 1 - first};
                if (g.catchPending)
                    g.resultLines.push_back(TextFormat("%s dealt the most damage and picks first! (%d vs %d)", Prof(g, first).name.c_str(),
                                                       (int)damage[first], (int)damage[1 - first]));
            } else {
                g.catchOrder = {0};
            }
            if (!g.catchPending) g.resultLines.push_back("Every foe was knocked out - nothing left to catch.");
        }
    }
    if (g.won) {
        wins++;
        if (storyBattle && g.stageIdx == stage && g.stageIdx < (int)Story().stages.size()) {
            // No free Pokemon for story wins: you catch them in the wild (training battles).
            const StageDef& beat = Story().stages[g.stageIdx];
            if (beat.snorlax) SnorlaxReward(g, beat.snorlax, damage[0], damage[1], foeLv);
            if (beat.join) JoinReward(g, beat, damage[0], damage[1], foeLv);
            if (coop && beat.name.find("Dojo") != std::string::npos) g.coopSave.dojoFirst = damage[1] > damage[0] ? 1 : 0;
            stage = NextStoryStage(stage);
            StoryGifts(g, beat);
            if (beat.IsLeague() && !beat.IsChampion()) g.resultLines.push_back(beat.trainer + " is beaten! On to the next room...");
            if (ChampionMoment(beat)) {
                if (beat.region == 0) g.resultLines.push_back(coop ? "You're the new Pokemon League Champions!" : "You're the new Pokemon League Champion!");
                else g.resultLines.push_back(TextFormat(coop ? "You're the %s Champions!" : "You're the %s Champion!", RegionInfo(beat.region).name.c_str()));
                if (RegionSlot(beat.region) == 0) g.resultLines.push_back("The Battle Tower is now open - see how high you can climb!");
                for (int i = RegionInfo(beat.region).first; i < RegionInfo(beat.region).end; i++)   // the post-game challenge (Red, Steven)
                    if (Story().stages[i].postgame && i > g.stageIdx)
                        g.resultLines.push_back("A challenge waits on the Map: " + Story().stages[i].trainer + " at " + Story().stages[i].name + "!");
                if (!RegionsLeft(g).empty())
                    g.resultLines.push_back("A new region awaits: choose where to go next from the menu!");
                else if (stage >= (int)Story().stages.size() && RegionCount() > 1)
                    g.resultLines.push_back("You've done it all - every region's League is yours!");
            }
        } else if (storyBattle && g.stageIdx < (int)Story().stages.size() && Story().stages[g.stageIdx].postgame &&
                   (coop ? g.coopSave.postDone : g.profile.postDone).insert(Story().stages[g.stageIdx].name).second) {
            const StageDef& beat = Story().stages[g.stageIdx];   // an optional post-game battle, won for the first time
            g.resultLines.push_back("You beat " + beat.trainer + "! A true legend.");
            StoryGifts(g, beat);
        }
        if (storyBattle && ChampionMoment(*st)) {   // every Championship win goes in the Hall of Fame
            for (int w = 0; w < players; w++) {
                Profile& P = Prof(g, w);
                std::string e = coop ? Prof(g, 1 - w).name : std::string();
                e += "|";
                bool first = true;
                for (const auto& m : P.mons) {
                    if (m.boxed) continue;
                    e += (first ? "" : ",") + m.species + ":" + std::to_string(m.level);
                    first = false;
                }
                if (st->region > 0) e += "|" + RegionInfo(st->region).name;   // which League
                P.hof.push_back(e);
                if (P.hof.size() > 30) P.hof.erase(P.hof.begin());
            }
            g.hofPending = true;
        }
        if (training) {
            trainingWins++;
            if (g.rareBattle) g.rareSpecies = -1;  // a new rare creature next time
        }
    } else {
        losses++;
        if (legend) g.resultLines.push_back(std::string(Legend(g.legend).species) + " was too strong... Heal up, train and come back to " + Legend(g.legend).place + ".");
        if (storyBattle && g.stageIdx == stage && st->IsLeague() && st->league > 1) {   // the League is one run
            int start = LeagueStartStage(st->region);
            if (start >= 0 && start < stage) {
                stage = start;
                g.resultLines.push_back("The League challenge starts over from " + Story().stages[start].trainer + ".");
            }
        }
    }
    if (g.tower) {   // the Battle Tower: a win climbs a floor, a loss ends the run
        int& floor = coop ? g.coopSave.towerFloor : g.profile.towerFloor;
        int& best = coop ? g.coopSave.towerBest : g.profile.towerBest;
        int& runs = coop ? g.coopSave.towerRuns : g.profile.towerRuns;
        if (g.won) {
            floor = std::max(floor, g.towerFloor);
            if (st->IsTowerBoss()) g.resultLines.push_back("You beat the Tower Boss! Triple coins!");
            if (floor > best) {
                best = floor;
                g.resultLines.push_back(TextFormat("New Battle Tower record: floor %d!", best));
            } else {
                g.resultLines.push_back(TextFormat("Floor %d cleared! Next up: floor %d.", floor, floor + 1));
            }
        } else {
            g.resultLines.push_back(TextFormat("The run ends on floor %d (%d cleared). Record: floor %d.", g.towerFloor, floor, best));
            floor = 0;
            runs++;
        }
    }
    if (coop) {
        SaveWho(g, 0);
        SaveWho(g, 1);
        if (!SaveCoop(g.coopSave)) Toast(g, "Could not save the co-op progress!");
    } else {
        SaveOrWarn(g);
    }
    g.resultSel = 0;
    g.screen = g.statRows.empty() ? Screen::StoryResults : Screen::BattleStats;
    PlaySfx(g.won ? Sfx::Win : Sfx::Faint);
}

// ------------------------------------------------------------------ after-battle stats

void UpdateBattleStats(Game& g) {
    PlayerInput in = MenuInput();
    if (!in.confirm && !in.back) return;
    PlaySfx(Sfx::MenuOk);
    g.screen = Screen::StoryResults;
}

void DrawBattleStats(const Game& g) {
    DrawMenuBackdrop(Color{20, 18, 32, 255}, (float)GetTime());
    DrawTextCentered(g.won ? "VICTORY - BATTLE STATS" : "DEFEAT - BATTLE STATS", VW / 2, 8, 20, g.won ? UI_GOLD : Color{255, 120, 100, 255});
    int best = -1, bestDealt = -1;   // MVP: most damage dealt on your side
    for (int i = 0; i < (int)g.statRows.size(); i++)
        if (g.statRows[i].team == 0 && g.statRows[i].dealt > bestDealt) { bestDealt = g.statRows[i].dealt; best = i; }
    int maxDealt = 1;
    for (const auto& r : g.statRows) maxDealt = std::max(maxDealt, std::max(r.dealt, r.taken));
    for (int team = 0; team < 2; team++) {
        int x = team == 0 ? 8 : 326, w = 306, y = 34;
        DrawPanel(x, y, w, 300, UI_PANEL, team == 0 ? Color{90, 140, 220, 255} : Color{220, 90, 90, 255});
        GDrawText(team == 0 ? "YOUR TEAM" : "OPPONENTS", x + 8, y + 6, 10, team == 0 ? Color{140, 190, 255, 255} : Color{255, 140, 130, 255});
        GDrawText("dealt", x + 150, y + 6, 10, UI_DIM);
        GDrawText("taken", x + 216, y + 6, 10, UI_DIM);
        GDrawText("KOs", x + 278, y + 6, 10, UI_DIM);
        int tDealt = 0, tTaken = 0, tKos = 0, row = 0;
        for (int i = 0; i < (int)g.statRows.size(); i++) {
            const auto& r = g.statRows[i];
            if (r.team != team) continue;
            tDealt += r.dealt; tTaken += r.taken; tKos += r.kos;
            if (row >= 11) continue;
            int ry = y + 22 + row * 22;
            row++;
            bool mvp = i == best && r.dealt > 0;
            DrawRectangle(x + 4, ry, w - 8, 20, mvp ? Fade(UI_GOLD, 0.15f) : Fade(BLACK, 0.15f));
            if (r.species >= 0) DrawSpeciesFit(r.species, VIEW_FRONT, x + 16, ry + 19, 18, r.fainted ? Color{110, 110, 120, 255} : WHITE);
            Color nc = team == 0 && g.coop ? (r.who == 0 ? UI_GOLD : Color{120, 230, 255, 255}) : UI_TEXT;
            const char* nm = r.species >= 0 ? AllSpecies()[r.species].name.c_str() : "?";
            GDrawText(nm, x + 30, ry + 2, 10, nc);
            GDrawText(TextFormat("Lv%d%s", r.level, r.fainted ? "  fainted" : ""), x + 30, ry + 11, 10, r.fainted ? Color{255, 120, 100, 255} : UI_DIM);
            if (mvp) GDrawText("MVP", x + 112, ry + 6, 10, UI_GOLD);
            // bars
            DrawRectangle(x + 150, ry + 13, (int)(56.0f * r.dealt / maxDealt), 3, Color{255, 170, 80, 255});
            DrawRectangle(x + 216, ry + 13, (int)(56.0f * r.taken / maxDealt), 3, Color{120, 170, 255, 255});
            GDrawText(TextFormat("%d", r.dealt), x + 150, ry + 2, 10, UI_TEXT);
            GDrawText(TextFormat("%d", r.taken), x + 216, ry + 2, 10, UI_TEXT);
            GDrawText(TextFormat("%d", r.kos), x + 282, ry + 5, 10, r.kos ? UI_GOLD : UI_DIM);
        }
        DrawRectangle(x + 4, y + 272, w - 8, 1, UI_BORDER);
        GDrawText("Total", x + 30, y + 280, 10, UI_TEXT);
        GDrawText(TextFormat("%d", tDealt), x + 150, y + 280, 10, Color{255, 190, 110, 255});
        GDrawText(TextFormat("%d", tTaken), x + 216, y + 280, 10, Color{150, 190, 255, 255});
        GDrawText(TextFormat("%d", tKos), x + 282, y + 280, 10, UI_GOLD);
    }
    if (g.coop) {   // per-player totals
        int d[2] = {0, 0}, k[2] = {0, 0};
        for (const auto& r : g.statRows) if (r.team == 0) { d[r.who] += r.dealt; k[r.who] += r.kos; }
        GDrawText(TextFormat("%s: %d dealt, %d KO%s", g.coopP[0].name.c_str(), d[0], k[0], k[0] == 1 ? "" : "s"), 12, 338, 10, UI_GOLD);
        const char* t2 = TextFormat("%s: %d dealt, %d KO%s", g.coopP[1].name.c_str(), d[1], k[1], k[1] == 1 ? "" : "s");
        GDrawText(t2, 314 - TextW(t2, 10), 338, 10, Color{120, 230, 255, 255});
    }
    GDrawText("Enter: continue", VW - 8 - TextW("Enter: continue", 10), 338, 10, UI_DIM);
}

// ------------------------------------------------------------------ fossils, the Dojo and gifts

namespace {
const char* kFossils[2] = {"Helix Fossil", "Dome Fossil"};
const char* kDojo[2] = {"Hitmonlee", "Hitmonchan"};
constexpr int REVIVE_LEVEL = 5;   // revived fossils start at level 5
int StageNamed(const char* key) {   // first story stage whose name contains key (lower case)
    for (int i = 0; i < (int)Story().stages.size(); i++) {
        std::string n = Story().stages[i].name;
        for (char& c : n) c = (char)std::tolower((unsigned char)c);
        if (n.find(key) != std::string::npos) return i;
    }
    return -1;
}
int FossilSpecies(const std::string& f) {
    return SpeciesIndex(f == "Dome Fossil" ? "Kabuto" : f == "Old Amber" ? "Aerodactyl" : "Omanyte");
}
int ProgressStage(const Game& g) { return ProgressStageFor(g.coop ? g.coopSave.storyStage : g.profile.storyStage); }
const char* const* PickOptions(int kind) { return kind == 1 ? kDojo : kFossils; }
std::string& PickField(Profile& P, int kind) { return kind == 1 ? P.dojo : P.fossil; }
const std::string& PickField(const Profile& P, int kind) { return kind == 1 ? P.dojo : P.fossil; }
// Past the stage (Mt. Moon / the Dojo) and someone hasn't taken theirs yet (older saves get the choice too).
bool PickDue(const Game& g, int kind) {
    int st = StageNamed(kind == 1 ? "dojo" : "mt. moon");
    if (st < 0 || ProgressStage(g) <= st) return false;
    for (int w = 0; w < (g.coop ? 2 : 1); w++)
        if (PickField(Prof(g, w), kind).empty()) return true;
    return false;
}
bool FossilChoiceDue(const Game& g) { return PickDue(g, 0); }
// Co-op: fossils go to the player with the most KOs so far; the Dojo to that battle's MVP.
int PickFirst(const Game& g, int kind) {
    if (!g.coop) return 0;
    if (kind == 1 && (g.coopSave.dojoFirst == 0 || g.coopSave.dojoFirst == 1)) return g.coopSave.dojoFirst;
    return g.coopSave.run[1].kos > g.coopSave.run[0].kos ? 1 : 0;
}
void GivePick(Game& g, int kind, int who, const std::string& f) {
    Profile& P = Prof(g, who);
    PickField(P, kind) = f;
    if (kind == 0) {
        P.fossilRevived = false;
        BagAdd(P, f);
    } else if (OnlineMine(g, who)) {   // the Dojo prize joins right away
        int sp = SpeciesIndex(f);
        if (sp >= 0) {
            bool boxed = AddMon(P, NewMon(sp, NewMonLevel(P)));
            g.giftQueue.push_back({who, sp, (g.coop ? P.name + ": the" : std::string("The")) + " Karate Master hands over the Dojo's prize - " + f +
                                                (boxed ? "! It was sent to the Box." : " joined your team!"), ""});
        }
    }
    SaveWho(g, who);
}
void ApplyPick(Game& g, int kind, int first, const std::string& choice) {
    const char* const* opt = PickOptions(kind);
    GivePick(g, kind, first, choice);
    std::string other = choice == opt[0] ? opt[1] : opt[0];
    if (g.coop) {
        GivePick(g, kind, 1 - first, other);
        Toast(g, g.coopP[first].name + " took " + (kind ? "" : "the ") + choice + ", " + g.coopP[1 - first].name + " " + (kind ? "" : "the ") + other +
                     (kind ? "!" : ". The Cinnabar Lab can revive them!"));
    } else {
        Toast(g, kind ? "You took " + choice + "!" : "You took the " + choice + "! The Cinnabar Lab can revive it.");
    }
    PlaySfx(Sfx::LevelUp);
}
// After beating Brock, the Pewter Museum gives each player a piece of Old Amber.
void QueueAmber(Game& g) {
    int pg = StageNamed("pewter gym");
    if (pg < 0 || ProgressStage(g) <= pg) return;
    for (int w = 0; w < (g.coop ? 2 : 1); w++) {
        Profile& P = Prof(g, w);
        if (P.amber || !OnlineMine(g, w)) continue;
        P.amber = true;
        P.amberRevived = false;
        BagAdd(P, "Old Amber");
        SaveWho(g, w);
        g.giftQueue.push_back({w, -1, (g.coop ? P.name + ": a" : std::string("A")) + " scientist at the Pewter Museum gives you a piece of Old Amber! "
                                       "Something ancient is sealed inside... the Cinnabar Lab could revive it.", "Old Amber"});
    }
}
// What the Cinnabar Lab could revive for player w right now.
std::vector<std::string> RevivalsFor(const Profile& P) {
    std::vector<std::string> v;
    if (!P.fossil.empty() && !P.fossilRevived) v.push_back(P.fossil);
    if (P.amber && !P.amberRevived) v.push_back("Old Amber");
    return v;
}
bool AtCinnabar(const Game& g) {
    int cin = StageNamed("cinnabar");
    return cin >= 0 && ProgressStage(g) >= cin;
}
bool RevivalDue(const Game& g) {
    if (!AtCinnabar(g)) return false;
    for (int w = 0; w < (g.coop ? 2 : 1); w++)
        if (OnlineMine(g, w) && !RevivalsFor(Prof(g, w)).empty()) return true;
    return false;
}
// The lab brings every waiting fossil back to life at level 5.
void DoRevivals(Game& g) {
    for (int w = 0; w < (g.coop ? 2 : 1); w++) {
        if (!OnlineMine(g, w)) continue;
        Profile& P = Prof(g, w);
        for (const std::string& f : RevivalsFor(P)) {
            int sp = FossilSpecies(f);
            if (sp < 0) continue;
            if (f == "Old Amber") P.amberRevived = true;
            else P.fossilRevived = true;
            BagTake(P, f);
            bool boxed = AddMon(P, NewMon(sp, REVIVE_LEVEL));
            g.giftQueue.push_back({w, sp, (g.coop ? P.name + ": the" : std::string("The")) + " Cinnabar Lab revived your " + f + "! A level " +
                                              std::to_string(REVIVE_LEVEL) + " " + AllSpecies()[sp].name + (boxed ? " was sent to the Box." : " joined your team!"), ""});
        }
        SaveWho(g, w);
    }
}
// After the first win: everyone gets the stage's gift Pokemon (Castform, Beldum), and its story card comes up.
void StoryGifts(Game& g, const StageDef& st) {
    if (!st.cardAfter.empty()) ShowStoryCard(g, st.cardAfter, st.cardMon);
    int sp = SpeciesIndex(st.gift);
    std::string gl = st.gift;
    for (char& ch : gl) ch = (char)std::tolower((unsigned char)ch);
    bool monkey = gl == "monkey", fossil = gl.find(" or ") != std::string::npos;
    if (sp < 0 && !monkey && !fossil) return;
    for (int w = 0; w < (g.coop ? 2 : 1); w++) {
        Profile& P = Prof(g, w);
        if (!OnlineMine(g, w)) continue;
        if (monkey) {   // Unova's Dreamyard: the monkey of the type your starter beats (it beats the Striaton brother you'd face)
            int mine = SpeciesIndex(RegionStarter(P, st.region));
            Type t = mine >= 0 ? AllSpecies()[mine].type1 : Type::Grass;
            sp = SpeciesIndex(t == Type::Grass ? "Panpour" : t == Type::Fire ? "Pansage" : "Pansear");
            if (sp < 0) continue;
        } else if (fossil) {   // "Tirtouga or Archen": one of them, at random
            size_t o = gl.find(" or ");
            std::string a = st.gift.substr(0, o), b = st.gift.substr(o + 4);
            sp = SpeciesIndex(GetRandomValue(0, 1) ? b : a);
            if (sp < 0) sp = SpeciesIndex(a);
            if (sp < 0) continue;
        }
        int sum = 0, n = 0;
        for (const auto& m : P.mons) if (!m.boxed) { sum += m.level; n++; }
        int lv = std::max(RegionInfo(st.region).startLevel, std::min(MAX_LEVEL, (n ? sum / n : 5) - 3));
        bool boxed = AddMon(P, NewMon(sp, lv));
        DexSee(P, AllSpecies()[sp].name);
        SaveWho(g, w);
        g.giftQueue.push_back({w, sp, (g.coop ? P.name + " was given " : std::string("You were given ")) + AllSpecies()[sp].name + "! It " +
                                          (boxed ? "was sent to the Box." : "joined your team!"), ""});
    }
}
// A wild boss that joins when beaten (Sudowoodo, the Red Gyarados): alone it's yours; in co-op the MVP's.
void JoinReward(Game& g, const StageDef& st, float dmg0, float dmg1, int level) {
    if (st.team.empty()) return;
    std::string e = st.team.back();
    bool shiny = false;
    {
        std::string lo = e;
        for (char& ch : lo) ch = (char)std::tolower((unsigned char)ch);
        size_t sh = lo.find(" shiny");
        if (sh != std::string::npos) { shiny = true; e = e.substr(0, sh) + e.substr(sh + 6); }
        size_t sp = e.find_last_of(' ');
        if (sp != std::string::npos && sp + 1 < e.size() && (e[sp + 1] == '+' || e[sp + 1] == '-')) e = e.substr(0, sp);
    }
    while (!e.empty() && e.back() == ' ') e.pop_back();
    int sp = SpeciesIndex(e);
    if (sp < 0) return;
    int who = g.coop && dmg1 > dmg0 ? 1 : 0;
    Profile& P = Prof(g, who);
    if (!OnlineMine(g, who)) {
        g.resultLines.push_back(P.name + " befriended the " + AllSpecies()[sp].name + "!");
        return;
    }
    OwnedMon m = NewMon(sp, level);
    if (shiny) m.shiny = true;
    bool boxed = AddMon(P, m);
    DexSee(P, AllSpecies()[sp].name);
    std::string why = !g.coop ? "The " + AllSpecies()[sp].name + " calmed down and decided to come with you!"
                              : "The " + AllSpecies()[sp].name + " took a liking to " + P.name + ", the battle's MVP!";
    g.giftQueue.push_back({who, sp, why + (boxed ? " It was sent to the Box." : " It joined your team!"), ""});
}
// Beating a sleeping Snorlax: it joins (solo every time; co-op one each - the MVP gets the first).
void SnorlaxReward(Game& g, int encounter, float dmg0, float dmg1, int level) {
    int sp = SpeciesIndex("Snorlax");
    if (sp < 0 || encounter < 1 || encounter > 2) return;
    int who = 0;
    if (g.coop) {
        int other = g.coopSave.snorlaxWho[2 - encounter];   // who got the other encounter's Snorlax
        if (other == 0 || other == 1) who = 1 - other;
        else who = dmg1 > dmg0 ? 1 : 0;
        g.coopSave.snorlaxWho[encounter - 1] = who;
    }
    Profile& P = Prof(g, who);
    P.snorlax |= 1 << (encounter - 1);
    if (!OnlineMine(g, who)) {
        g.resultLines.push_back(P.name + " befriended the Snorlax!");
        return;
    }
    bool boxed = AddMon(P, NewMon(sp, level));
    std::string why = !g.coop ? "The Snorlax calmed down and decided to come with you!"
                    : g.coopSave.snorlaxWho[2 - encounter] >= 0 ? "This Snorlax goes to " + P.name + " - " + Prof(g, 1 - who).name + " already has one!"
                                                                 : "The Snorlax took a liking to " + P.name + ", the battle's MVP!";
    g.giftQueue.push_back({who, sp, why + (boxed ? " It was sent to the Box." : " It joined your team!"), ""});
}
void DrawFossilArt(int cx, int cy, int kind, bool sel, float s = 1.0f) {
    if (s < 0.99f) {   // a small icon
        Color c = kind == 2 ? Color{230, 160, 55, 255} : Color{170, 150, 122, 255};
        DrawEllipse(cx, cy, (int)(30 * s), (int)(24 * s), ColorBrightness(c, -0.35f));
        DrawEllipse(cx, cy - 1, (int)(28 * s), (int)(22 * s), c);
        if (kind == 2) DrawEllipse(cx + 1, cy + 1, (int)(10 * s), (int)(7 * s), Color{110, 60, 20, 255});
        else if (kind == 0) DrawCircleLines(cx, cy, 14 * s, ColorBrightness(c, -0.35f));
        else DrawLine(cx - (int)(22 * s), cy + 2, cx + (int)(22 * s), cy + 2, ColorBrightness(c, -0.35f));
        return;
    }
    if (kind == 2) {   // Old Amber: a golden drop with a shadow inside
        Color am = sel ? Color{240, 170, 60, 255} : Color{210, 140, 50, 255};
        DrawEllipse(cx, cy + 22, 26, 7, Fade(BLACK, 0.4f));
        DrawEllipse(cx, cy + 2, 24, 22, ColorBrightness(am, -0.35f));
        DrawEllipse(cx, cy, 22, 20, am);
        DrawEllipse(cx + 2, cy + 3, 9, 6, Fade(Color{90, 50, 20, 255}, 0.7f));
        DrawTriangle({(float)cx - 4, (float)cy + 2}, {(float)cx + 10, (float)cy - 4}, {(float)cx + 6, (float)cy + 6}, Fade(Color{90, 50, 20, 255}, 0.6f));
        DrawEllipse(cx - 9, cy - 9, 5, 3, Fade(WHITE, 0.6f));
        return;
    }
    Color rock = sel ? Color{176, 156, 128, 255} : Color{140, 124, 104, 255}, dk = ColorBrightness(rock, -0.35f), lt = ColorBrightness(rock, 0.25f);
    DrawEllipse(cx, cy + 22, 34, 8, Fade(BLACK, 0.4f));
    DrawEllipse(cx, cy, 34, 26, dk);
    DrawEllipse(cx - 1, cy - 1, 32, 24, rock);
    if (kind == 0) {   // a spiral shell
        for (int k = 0; k < 40; k++) {
            float a = k * 0.32f, r = 2 + k * 0.5f;
            DrawCircle(cx + (int)(std::cos(a) * r), cy + (int)(std::sin(a) * r * 0.85f), 2.0f, dk);
        }
        DrawCircle(cx - 6, cy - 8, 3, lt);
    } else {   // a domed shell
        DrawEllipse(cx, cy + 2, 22, 16, dk);
        DrawEllipse(cx, cy, 20, 14, ColorBrightness(rock, 0.1f));
        DrawLine(cx - 18, cy + 4, cx + 18, cy + 4, dk);
        DrawCircle(cx - 8, cy - 5, 3, lt);
    }
}
int FossilArtKind(const std::string& f) { return f == "Old Amber" ? 2 : f == "Dome Fossil" ? 1 : 0; }
} // namespace

void UpdateFossil(Game& g) {
    int kind = g.pickKind;
    if (!PickDue(g, kind)) { AdvanceStoryQueues(g); return; }
    const char* const* opt = PickOptions(kind);
    if (g.coop) {   // only one player still needs one: they get the one the other didn't take
        bool h0 = !PickField(Prof(g, 0), kind).empty(), h1 = !PickField(Prof(g, 1), kind).empty();
        if (h0 != h1) {
            int w = h0 ? 1 : 0;
            GivePick(g, kind, w, PickField(Prof(g, 1 - w), kind) == opt[0] ? opt[1] : opt[0]);
            AdvanceStoryQueues(g);
            return;
        }
    }
    int first = PickFirst(g, kind);
    if (g.coop && g.on.active && !OnlineMine(g, first)) {   // the partner picks on their computer
        if (!g.on.partnerFossil.empty()) {
            std::string c = g.on.partnerFossil;
            g.on.partnerFossil.clear();
            if (c == opt[0] || c == opt[1]) {
                ApplyPick(g, kind, first, c);
                AdvanceStoryQueues(g);
            }
        }
        return;
    }
    PlayerInput in = MenuInput();
    if (in.left || in.right || in.up || in.down) { g.fossilSel = 1 - g.fossilSel; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    std::string choice = opt[g.fossilSel & 1];
    if (g.coop && g.on.active) OnlineSendFossil(choice);
    ApplyPick(g, kind, first, choice);
    AdvanceStoryQueues(g);
}

void DrawFossil(const Game& g) {
    int kind = g.pickKind;
    const char* const* opt = PickOptions(kind);
    ClearBackground(kind ? Color{38, 26, 22, 255} : Color{30, 26, 34, 255});
    if (kind) {   // dojo floorboards
        for (int y = 0; y < VH; y += 18) DrawRectangle(0, y, VW, 2, Color{52, 36, 28, 255});
        for (int x = 0; x < VW; x += 64) DrawRectangle(x + ((x / 64) % 2) * 20, 0, 2, VH, Color{46, 32, 24, 255});
    } else {
        for (int y = 0; y < VH; y += 6) DrawRectangle(0, y, VW, 3, Color{36, 31, 40, 255});
    }
    DrawTextCentered(kind ? "THE FIGHTING DOJO'S PRIZE" : "FOSSILS IN MT. MOON", VW / 2, 18, 20, UI_GOLD);
    int first = PickFirst(g, kind);
    if (g.coop) {
        if (kind) DrawTextCentered(TextFormat("%s was the MVP of the Dojo battle and picks first.", g.coopP[first].name.c_str()),
                                   VW / 2, 46, 10, first == 0 ? UI_GOLD : Color{120, 230, 255, 255}, false);
        else DrawTextCentered(TextFormat("%s has the most KOs (%d to %d) and picks first.", g.coopP[first].name.c_str(), g.coopSave.run[first].kos, g.coopSave.run[1 - first].kos),
                              VW / 2, 46, 10, first == 0 ? UI_GOLD : Color{120, 230, 255, 255}, false);
        DrawTextCentered(TextFormat("%s gets the other one.", g.coopP[1 - first].name.c_str()), VW / 2, 60, 10, UI_DIM, false);
    } else {
        DrawTextCentered(kind ? "\"You beat the Dojo! Take one of my two fighters - only one!\"" : "Two fossils lie in the cave. You can only take one!",
                         VW / 2, 50, 10, UI_TEXT, false);
    }
    for (int k = 0; k < 2; k++) {
        int cx = VW / 2 + (k ? 120 : -120), cy = 150;
        bool sel = g.fossilSel == k;
        DrawPanel(cx - 100, 84, 200, 190, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
        int sp = kind ? SpeciesIndex(opt[k]) : FossilSpecies(opt[k]);
        if (kind) {
            if (sp >= 0) DrawSpeciesFit(sp, VIEW_FRONT, cx, 186, 92, sel ? WHITE : Color{200, 200, 210, 255});
            DrawTextCentered(opt[k], cx, 192, 20, sel ? UI_GOLD : UI_TEXT);
            DrawTextCentered(k ? "the punching Pokemon" : "the kicking Pokemon", cx, 218, 10, UI_DIM, false);
        } else {
            DrawFossilArt(cx, cy, k, sel);
            DrawTextCentered(opt[k], cx, 192, 20, sel ? UI_GOLD : UI_TEXT);
            if (sp >= 0) {
                DrawSpeciesFit(sp, VIEW_FRONT, cx, 262, 40, Color{40, 36, 50, 255});
                DrawTextCentered(TextFormat("an ancient %s Pokemon", k ? "domed" : "spiral-shelled"), cx, 218, 10, UI_DIM, false);
            }
        }
    }
    bool waiting = g.coop && g.on.active && !OnlineMine(g, first);
    DrawTextCentered(waiting ? TextFormat("Waiting for %s to choose...", g.coopP[first].name.c_str()) : "Left/Right choose   Enter take it",
                     VW / 2, 300, 10, waiting ? UI_GOLD : UI_DIM, false);
    if (!kind) DrawTextCentered("The Cinnabar Lab can bring it back to life later on.", VW / 2, 318, 10, Color{140, 220, 255, 255}, false);
    DrawToast(g);
}

// ------------------------------------------------------------------ Cinnabar Lab: fossil revival

void UpdateRevive(Game& g) {
    if (g.reviveT < 0) {
        if (!RevivalDue(g)) { AdvanceStoryQueues(g); return; }
        PlayerInput in = MenuInput();
        if (!in.confirm) return;
        g.reviveT = 0;
        PlaySfx(Sfx::MenuOk);
        return;
    }
    float before = g.reviveT;
    g.reviveT += GetFrameTime();
    if (before < 1.2f && g.reviveT >= 1.2f) PlaySfx(Sfx::Evolve);
    if (g.reviveT < 3.0f) return;
    g.reviveT = -1;
    DoRevivals(g);
    AdvanceStoryQueues(g);
}

void DrawRevive(const Game& g) {
    ClearBackground(Color{18, 30, 40, 255});
    // the lab: tiled floor, a back wall of monitors
    for (int y = 200; y < VH; y += 20)
        for (int x = (y / 20) % 2 * 20; x < VW; x += 40) DrawRectangle(x, y, 20, 20, Color{26, 42, 54, 255});
    DrawRectangle(0, 0, VW, 200, Color{30, 46, 60, 255});
    DrawRectangle(0, 196, VW, 4, Color{60, 90, 110, 255});
    for (int k = 0; k < 6; k++) {
        int x = 24 + k * 104;
        DrawRectangle(x, 52, 80, 50, Color{14, 22, 30, 255});
        for (int l = 0; l < 4; l++) {
            float w = 30 + 30 * std::fabs(std::sin(g.t * (1.2f + k * 0.3f) + l));
            DrawRectangle(x + 6, 60 + l * 10, (int)w, 4, Fade(Color{80, 230, 160, 255}, 0.7f));
        }
    }
    DrawTextCentered("CINNABAR LAB - FOSSIL REVIVAL", VW / 2, 14, 20, Color{140, 220, 255, 255});
    DrawTextCentered("\"Welcome to Cinnabar Island! Our machine can bring ancient Pokemon back to life.\"", VW / 2, 38, 10, UI_TEXT, false);
    // the machine: a glass tube on a base
    int mx = VW / 2, my = 210;
    bool running = g.reviveT >= 0;
    float glow = running ? std::min(1.0f, g.reviveT / 1.2f) : 0.25f + 0.1f * std::sin(g.t * 2);
    DrawRectangle(mx - 50, my + 20, 100, 30, Color{80, 96, 110, 255});
    DrawRectangle(mx - 54, my + 16, 108, 8, Color{120, 140, 156, 255});
    DrawRectangle(mx - 34, my - 100, 68, 118, Fade(Color{120, 220, 255, 255}, 0.15f + 0.35f * glow));
    DrawRectangleLines(mx - 34, my - 100, 68, 118, Color{160, 220, 240, 255});
    DrawRectangle(mx - 40, my - 108, 80, 10, Color{120, 140, 156, 255});
    // what goes in
    std::vector<std::pair<int, std::string>> list;   // (player, fossil)
    for (int w = 0; w < (g.coop ? 2 : 1); w++)
        for (const auto& f : RevivalsFor(Prof(g, w))) list.push_back({w, f});
    if (running) {
        for (int k = 0; k < 30; k++) {   // rising sparks
            float ph = std::fmod(g.reviveT * 1.4f + k * 0.137f, 1.0f);
            DrawCircle(mx - 28 + (k * 37) % 56, my + 14 - (int)(ph * 112), 1.5f, Fade(Color{200, 255, 240, 255}, 1 - ph));
        }
        if (g.reviveT > 2.2f) DrawCircle(mx, my - 40, (g.reviveT - 2.2f) * 260, Fade(WHITE, std::max(0.0f, 1 - (g.reviveT - 2.2f) * 1.25f)));
    } else if (!list.empty()) {
        DrawFossilArt(mx, my - 30, FossilArtKind(list[0].second), true);
    }
    // the list of fossils, per player
    int y = 92;
    DrawPanel(14, y - 6, 170, 20 + (int)list.size() * 26, UI_PANEL, Color{70, 110, 130, 255});
    GDrawText("Waiting to be revived:", 22, y, 10, UI_DIM);
    for (int i = 0; i < (int)list.size(); i++) {
        int ly = y + 16 + i * 26;
        DrawFossilArt(36, ly + 11, FossilArtKind(list[i].second), false, 0.4f);
        int sp = FossilSpecies(list[i].second);
        GDrawText(list[i].second.c_str(), 62, ly + 1, 10, UI_TEXT);
        GDrawText(TextFormat("%s%s", g.coop ? (Prof(g, list[i].first).name + " - ").c_str() : "", sp >= 0 ? AllSpecies()[sp].name.c_str() : "?"),
                 62, ly + 13, 10, g.coop && list[i].first == 1 ? Color{120, 230, 255, 255} : UI_GOLD);
    }
    DrawPanel(VW - 200, 92, 186, 46, UI_PANEL, Color{70, 110, 130, 255});
    DrawWrapped(TextFormat("Revived Pokemon start at level %d and join your party (or the Box if it's full).", REVIVE_LEVEL), VW - 192, 100, 172, 10,
                Color{150, 210, 230, 255}, 13);
    if (g.coop && g.on.active) GDrawText("Each player revives on their own computer.", 200, 324, 10, UI_DIM);
    DrawTextCentered(running ? "Reviving..." : "Enter: revive your fossils", VW / 2, 300, 10, running ? UI_GOLD : UI_TEXT, false);
    DrawToast(g);
}

void UpdateGift(Game& g) {
    if (g.giftQueue.empty()) { AdvanceStoryQueues(g); return; }
    PlayerInput in = MenuInput();
    if (!in.confirm && !in.back) return;
    PlaySfx(Sfx::MenuOk);
    g.giftQueue.erase(g.giftQueue.begin());
    if (g.giftQueue.empty()) AdvanceStoryQueues(g);
}

void DrawGift(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    if (g.giftQueue.empty()) return;
    const auto& gf = g.giftQueue.front();
    for (int k = 0; k < 24; k++) {   // sparkles
        float a = g.t * 0.8f + k * 0.26f, r = 70 + 30 * std::sin(g.t * 1.3f + k);
        DrawCircle((int)(VW / 2 + std::cos(a) * r * 1.6f), (int)(140 + std::sin(a) * r), 1.5f, Fade(UI_GOLD, 0.5f + 0.5f * std::sin(g.t * 3 + k)));
    }
    DrawTextCentered(gf.species >= 0 ? "A NEW POKEMON!" : "YOU GOT AN ITEM!", VW / 2, 22, 20, UI_GOLD);
    if (gf.species >= 0) DrawSpeciesFit(gf.species, VIEW_FRONT, VW / 2, 210, 120);
    else if (!gf.item.empty()) {
        DrawFossilArt(VW / 2, 140, FossilArtKind(gf.item), true);
        DrawTextCentered(gf.item.c_str(), VW / 2, 186, 20, UI_TEXT);
    }
    DrawPanel(60, 236, VW - 120, 60, UI_PANEL, UI_GOLD);
    DrawWrapped(gf.text.c_str(), 72, 246, VW - 144, 10, UI_TEXT, 13);
    DrawTextCentered("Enter: continue", VW / 2, 316, 10, UI_DIM, false);
}

void AdvanceStoryQueues(Game& g) {
    if (g.legendCatchPending) {   // a weakened legendary: its catch comes first
        g.legendCatchPending = false;
        g.screen = Screen::LegendCatch;
        return;
    }
    if (g.catchPending) {
        g.catchPending = false;
        g.catchPool = g.catchCands;
        g.catchTurn = 0;
        g.catchWho = g.catchOrder.empty() ? 0 : g.catchOrder[0];
        g.catchPoolPick = -1;
        g.catchSel = 0;
        g.catchPhase = 0;
        g.catchT = 0;
        g.screen = Screen::Catch;
        return;
    }
    QueueAmber(g);
    if (!g.giftQueue.empty()) { g.screen = Screen::Gift; PlaySfx(Sfx::LevelUp); return; }
    for (int kind = 0; kind < 2; kind++)
        if (PickDue(g, kind)) { g.pickKind = kind; g.fossilSel = 0; g.screen = Screen::Fossil; return; }
    if (RevivalDue(g)) { g.reviveT = -1; g.screen = Screen::Revive; PlaySfx(Sfx::MenuOk); return; }
    if (!g.evoQueue.empty()) {
        g.evoT = 0;
        g.screen = Screen::Evolve;
        PlaySfx(Sfx::Evolve);
        return;
    }
    while (!g.learnQueue.empty()) {
        auto& l = g.learnQueue.front();
        Profile& P = Prof(g, l.who);
        if (l.mon < 0 || l.mon >= (int)P.mons.size()) { g.learnQueue.erase(g.learnQueue.begin()); continue; }
        OwnedMon& m = P.mons[l.mon];
        if (std::find(m.moves.begin(), m.moves.end(), l.move) != m.moves.end()) { g.learnQueue.erase(g.learnQueue.begin()); continue; }
        if (m.moves.size() < 4) {
            m.moves.push_back(l.move);
            Toast(g, m.species + " learned " + l.move + "!");
            g.learnQueue.erase(g.learnQueue.begin());
            continue;
        }
        g.learnSel = 0;
        g.screen = Screen::LearnMove;
        PlaySfx(Sfx::LevelUp);
        return;
    }
    while (g.rewardsPending > 0) {
        int who = g.rewardWho.empty() ? 0 : g.rewardWho.front();
        std::vector<std::string> owned;
        for (const auto& m : Prof(g, who).mons) owned.push_back(m.species);
        g.rewardChoices = RewardChoices(owned, 3, ProgressStage(g), (int)(g.coop ? g.coopSave.badges.size() : g.profile.badges.size()));
        if (g.rewardChoices.empty()) {
            g.rewardsPending--;
            if (!g.rewardWho.empty()) g.rewardWho.erase(g.rewardWho.begin());
            Toast(g, "You've collected every Pokemon!");
            continue;
        }
        g.rewardSel = 0;
        g.screen = Screen::Reward;
        return;
    }
    if (g.coop) {
        SaveWho(g, 0);
        SaveWho(g, 1);
        SaveCoop(g.coopSave);
    } else {
        SaveOrWarn(g);
    }
    OnlineSendProfile(g);
    if (g.hofPending) {   // a new Champion: the Hall of Fame
        g.hofPending = false;
        g.hofSel = 0;
        g.hofBack = Screen::StoryHub;
        g.hofT = 0;
        g.screen = Screen::HallOfFame;
        PlaySfx(Sfx::Win);
        return;
    }
    if (g.returnToTrade) {  // a trade evolution
        g.returnToTrade = false;
        g.screen = Screen::Trade;
        g.tradePhase = 0;
        for (int p = 0; p < 2; p++) { g.tradeReady[p] = false; g.tradeOffer[p] = -1; }
        OnlineSendTrade(g);
        return;
    }
    if (g.returnToParty) {  // a stone evolution started from Party & Box
        g.returnToParty = false;
        g.screen = Screen::MyCreatures;
        return;
    }
    EnterStoryHub(g);
}

void UpdateStoryResults(Game& g) {
    PlayerInput in = MenuInput();
    if (in.confirm || in.back) {
        PlaySfx(Sfx::MenuOk);
        AdvanceStoryQueues(g);
    }
}

void DrawStoryResults(const Game& g) {
    g.battle.Draw(false);
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.8f));
    float st0 = g.screenT;
    {   // the title on a ribbon that sweeps in, with rays behind a win
        float in = std::min(1.0f, st0 / 0.35f);
        float e = 1 - (1 - in) * (1 - in) * (1 - in);
        Color rc = g.won ? Color{210, 160, 40, 255} : Color{150, 50, 50, 255};
        if (g.won) {
            BeginBlendMode(BLEND_ADDITIVE);
            for (int i = 0; i < 16; i++) {
                float a = i * 6.2832f / 16 + g.t * 0.3f;
                Vector2 c{VW / 2.0f, 34};
                DrawTriangle(c, {c.x + std::cos(a + 0.06f) * 300, c.y + std::sin(a + 0.06f) * 300}, {c.x + std::cos(a) * 300, c.y + std::sin(a) * 300}, Fade(Color{255, 220, 120, 255}, 0.05f * e));
            }
            EndBlendMode();
        }
        float bw = 300 * e;
        DrawRectangleGradientV((int)(VW / 2 - bw / 2), 16, (int)bw, 36, ColorBrightness(rc, 0.1f), ColorBrightness(rc, -0.45f));
        DrawTriangle({VW / 2 - bw / 2 - 14, 34}, {VW / 2 - bw / 2, 52}, {VW / 2 - bw / 2, 16}, ColorBrightness(rc, -0.55f));
        DrawTriangle({VW / 2 + bw / 2 + 14, 34}, {VW / 2 + bw / 2, 16}, {VW / 2 + bw / 2, 52}, ColorBrightness(rc, -0.55f));
        DrawRectangle((int)(VW / 2 - bw / 2), 16, (int)bw, 2, ColorBrightness(rc, 0.5f));
        if (e > 0.8f) DrawTextCentered(g.won ? "VICTORY!" : "DEFEATED...", VW / 2, 19, 30, g.won ? Color{255, 240, 190, 255} : Color{255, 200, 190, 255});
    }
    // the MVP: whoever dealt the most damage
    int mvpRow = -1;
    {
        float best = 0;
        for (int ri = 0; ri < (int)g.resultRows.size(); ri++)
            for (int s = 0; s < g.battle.NumSlots(); s++)
                for (const Fighter& f : g.battle.GetSide(s).team)
                    if (f.ownedId == g.resultRows[ri].mon && (f.owner == g.resultRows[ri].who || !g.coop) && g.battle.GetSide(s).teamId == 0 && f.dmgDealt > best) {
                        best = f.dmgDealt;
                        mvpRow = ri;
                    }
    }
    if ((g.mode == Mode::Story || g.mode == Mode::CoopStory) && (g.stageIdx >= 0 || g.tower || g.legend >= 0)) {
        StageDef st = ActiveStage(g);
        DrawTextCentered(g.tower ? TextFormat("Battle Tower floor %d  -  vs %s", g.towerFloor, st.trainer.c_str()) : TextFormat("vs %s", st.trainer.c_str()), VW / 2, 56, 10, UI_TEXT, false);
    }
    int y = 70;
    bool compact = g.resultRows.size() > 3;
    int rh = g.resultRows.size() > 4 ? 24 : (compact ? 30 : 40);
    int rowI = -1;
    for (const auto& r : g.resultRows) {
        rowI++;
        const Profile& P = Prof(g, r.who);
        if (r.mon < 0 || r.mon >= (int)P.mons.size()) continue;
        const OwnedMon& m = P.mons[r.mon];
        int idx = MonSpecies(m);
        float slide = std::min(1.0f, std::max(0.0f, (st0 - 0.2f - rowI * 0.08f) / 0.25f));   // rows slide in one after another
        if (slide <= 0) { y += rh + (rh < 30 ? 2 : 4); continue; }
        float se = 1 - (1 - slide) * (1 - slide);
        rlPushMatrix();
        rlTranslatef((1 - se) * -60, 0, 0);
        bool mvp = rowI == mvpRow;
        DrawPanel(120, y, 400, rh, UI_PANEL, mvp ? UI_GOLD : UI_BORDER);
        if (mvp) {   // a gold MVP tag
            DrawRectangleRounded({476, (float)y - 4, 34, 11}, 0.5f, 6, UI_GOLD);
            GDrawText("MVP", 482, y - 3, 10, UI_INK);
        }
        if (g.coop) {
            Color tc = r.who == 0 ? UI_GOLD : Color{120, 230, 255, 255};
            DrawRectangle(120, y, 3, rh, tc);
            GDrawText(r.who == 0 ? "P1" : "P2", 102, y + 4, 10, tc);
        }
        if (idx >= 0) DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, 145, y + rh - 2, rh - 6, m.hp > 0 ? WHITE : Color{90, 90, 100, 255});
        if (m.shiny) DrawShinyStar(134, (float)y + 5, 3);
        GDrawText(r.name.c_str(), 172, y + 4, 10, UI_TEXT);
        std::string lv = r.levelTo > r.levelFrom ? TextFormat("Lv%d -> Lv%d", r.levelFrom, r.levelTo) : TextFormat("Lv%d", r.levelTo);
        GDrawText(lv.c_str(), 290, y + 4, 10, r.levelTo > r.levelFrom ? UI_GOLD : UI_TEXT);
        GDrawText(TextFormat("+%d XP", r.xp), 420, y + 4, 10, Color{120, 190, 255, 255});
        if (!r.note.empty()) GDrawText(r.note.c_str(), 172, y + rh - 12, 10, UI_DIM);
        DrawHpBar(290, y + rh - 11, 90, 3, m.hp);
        {   // the XP bar fills up; a level-up flashes
            float fill = std::min(1.0f, std::max(0.0f, (st0 - 0.5f - rowI * 0.08f) / 0.8f));
            int lo = XpForLevel(m.level), hi = XpForLevel(m.level + 1);
            float f = m.level >= MAX_LEVEL ? 1.0f : (float)(m.xp - lo) / std::max(1, hi - lo);
            bool lvl = r.levelTo > r.levelFrom;
            float shown = lvl ? (fill < 0.5f ? fill * 2 : (fill - 0.5f) * 2 * f) : f * (0.6f + 0.4f * fill);
            DrawRectangleRounded({289.0f, (float)(y + rh - 7), 202, 5}, 0.6f, 4, UI_INK);
            DrawRectangle(290, y + rh - 6, 200, 3, Color{40, 40, 60, 255});
            DrawRectangle(290, y + rh - 6, (int)(200 * std::max(0.0f, std::min(1.0f, shown))), 3, Color{100, 180, 255, 255});
            DrawRectangle(290, y + rh - 6, (int)(200 * std::max(0.0f, std::min(1.0f, shown))), 1, Color{190, 225, 255, 255});
            if (lvl && fill >= 0.5f) {
                float p = std::min(1.0f, (fill - 0.5f) * 4);
                const char* lu = "LEVEL UP!";
                Color lc = ((int)(g.t * 6) % 2) ? UI_GOLD : WHITE;
                DrawTextOutlined(lu, 420 - (int)((1 - p) * 10), y + rh - 19, 10, Fade(lc, p));
            }
        }
        rlPopMatrix();
        y += rh + (rh < 30 ? 2 : 4);
    }
    for (const auto& l : g.resultLines) {
        if (y > 300) break;
        std::string shown = l;
        size_t cp = l.find(" coins!");
        if (cp != std::string::npos) {   // coins count up
            size_t ns = l.rfind(' ', cp - 1);
            int n = std::atoi(l.c_str() + ns + 1);
            float u = std::min(1.0f, std::max(0.0f, (st0 - 0.6f) / 0.9f));
            shown = l.substr(0, ns + 1) + std::to_string((int)(n * u)) + l.substr(cp);
            int cx0 = VW / 2 - TextW(shown.c_str(), 10) / 2 - 12;
            DrawCircle(cx0 + 4, y + 6, 4.5f, Color{170, 120, 20, 255});
            DrawCircle(cx0 + 4, y + 6, 3.5f, kCoin);
            DrawRectangle(cx0 + 3, y + 4, 1, 3, Color{255, 240, 170, 255});
        }
        DrawTextCentered(shown.c_str(), VW / 2, y + 2, 10, cp != std::string::npos ? Color{255, 214, 110, 255} : Color{140, 230, 255, 255}, false);
        y += 12;
    }
    {   // a badge just won: the badge case opens and the new one drops into its slot
        std::string won;
        for (const auto& l : g.resultLines) {
            size_t p = l.find(" Badge!");
            if (p == std::string::npos) continue;
            size_t q = l.rfind("the ", p);
            if (q != std::string::npos) won = l.substr(q + 4, p - q - 4);
        }
        if (!won.empty()) {
            std::vector<std::string> all;
            for (const auto& stg : Story().stages) if (stg.IsGym() && std::find(all.begin(), all.end(), stg.badge) == all.end()) all.push_back(stg.badge);
            float open = std::min(1.0f, std::max(0.0f, (st0 - 0.8f) / 0.35f));
            int cw = 104, ch = 78, cx = VW - cw - 6, cy = 64;
            float e = 1 - (1 - open) * (1 - open);
            DrawRectangleRounded({(float)cx, (float)cy + (1 - e) * 40, (float)cw, (float)ch}, 0.15f, 6, Fade(Color{120, 40, 40, 255}, e));
            DrawRectangleRounded({(float)cx + 4, (float)cy + 14 + (1 - e) * 40, (float)cw - 8, (float)ch - 18}, 0.12f, 6, Fade(Color{40, 26, 30, 255}, e));
            GDrawText("BADGES", cx + 8, cy + 3 + (int)((1 - e) * 40), 10, Fade(UI_GOLD, e));
            const Profile& P0 = Prof(g, 0);
            for (int i = 0; i < (int)all.size() && i < 8; i++) {
                int bx = cx + 16 + (i % 4) * 24, by = cy + 30 + (i / 4) * 26 + (int)((1 - e) * 40);
                bool have = std::find(P0.badges.begin(), P0.badges.end(), all[i]) != P0.badges.end();
                if (all[i] == won) {
                    float drop = std::min(1.0f, std::max(0.0f, (st0 - 1.2f) / 0.4f));
                    if (drop <= 0) { DrawCircleLines(bx, by, 7, Fade(UI_DIM, e)); continue; }
                    float bounce = drop < 1 ? (1 - drop) * -30 : 0;
                    BeginBlendMode(BLEND_ADDITIVE);
                    DrawCircleV({(float)bx, (float)by + bounce}, 12 + std::sin(g.t * 6) * 2, Fade(BadgeColor(won), 0.35f));
                    EndBlendMode();
                    DrawBadge(won, bx, (int)(by + bounce), true, 8);
                    if (drop >= 1)
                        for (int k = 0; k < 4; k++) {
                            float a = k * 1.57f + g.t * 2;
                            DrawShinyStar(bx + std::cos(a) * 12, by + std::sin(a) * 10, 2 + std::sin(g.t * 8 + k));
                        }
                } else DrawBadge(all[i], bx, by, have, 7);
            }
        }
    }
    if (g.legendCatchPending) { DrawTextCentered(g.coop ? "The legendary is weakened - time to take turns throwing balls!" : "The legendary is weakened - you have 3 balls to catch it!", VW / 2, y + 6, 10, UI_GOLD, false); y += 12; }
    if (g.catchPending) { DrawTextCentered(g.coop ? "Weakened foes are still here - time for the catch draft!" : "A weakened foe is still here - try to catch it!", VW / 2, y + 6, 10, UI_GOLD, false); y += 12; }
    if (!g.evoQueue.empty()) { DrawTextCentered("Something is happening...", VW / 2, y + 8, 10, UI_GOLD, false); y += 14; }
    if (!g.learnQueue.empty()) { DrawTextCentered("A Pokemon wants to learn a new move!", VW / 2, y + 8, 10, UI_GOLD, false); y += 14; }
    if (g.rewardsPending > 0) DrawTextCentered(g.coop ? "You can each choose a new Pokemon!" : "You can choose a new Pokemon!", VW / 2, y + 8, 10, UI_GOLD, false);
    if (!g.won) DrawTextCentered(g.tower ? "XP from knockouts is kept. Train up and start a new run!" : "XP from knockouts is kept. Heal up, train and try again!", VW / 2, 322, 10, UI_DIM, false);
    DrawTextCentered("Enter to continue", VW / 2, 340, 10, UI_DIM, false);
}

// ------------------------------------------------------------------ evolution

void UpdateEvolve(Game& g, float dt) {
    if (g.evoQueue.empty()) { AdvanceStoryQueues(g); return; }
    float before = g.evoT;
    g.evoT += dt;
    auto& e = g.evoQueue.front();
    Profile& P = Prof(g, e.who);
    if (e.mon < 0 || e.mon >= (int)P.mons.size()) { g.evoQueue.erase(g.evoQueue.begin()); AdvanceStoryQueues(g); return; }
    OwnedMon& m = P.mons[e.mon];
    if (before < 3.0f && g.evoT >= 3.0f) {
        Growth gr;
        Evolve(m, e.to, gr);
        for (const auto& mv : gr.learned) Toast(g, m.species + " learned " + mv + "!");
        for (const auto& mv : gr.wantsToLearn) g.learnQueue.push_back({e.mon, mv, e.who});
        SaveWho(g, e.who);
        PlaySfx(Sfx::Win);
    }
    if (g.evoT > 3.4f) {
        PlayerInput in = MenuInput();
        if (in.confirm || in.back) {
            g.evoQueue.erase(g.evoQueue.begin());
            AdvanceStoryQueues(g);
        }
    }
}

void DrawEvolve(const Game& g) {
    ClearBackground(Color{12, 10, 24, 255});
    if (g.evoQueue.empty()) return;
    const auto& e = g.evoQueue.front();
    const Profile& P = Prof(g, e.who);
    if (e.mon < 0 || e.mon >= (int)P.mons.size()) return;
    const OwnedMon& m = P.mons[e.mon];
    float t = g.evoT;
    if (g.coop) DrawTextCentered(TextFormat("%s's Pokemon", P.name.c_str()), VW / 2, 20, 10, e.who == 0 ? UI_GOLD : Color{120, 230, 255, 255}, false);
    // A swirling backdrop: rays turning faster as it nears, rising motes, rings pulsing outward
    Vector2 cc{VW / 2.0f, 150};
    float build = std::min(1.0f, t / 3.0f);
    DrawRectangleGradientV(0, 0, VW, VH, Color{16, 14, 40, 255}, Color{8, 6, 18, 255});
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < 28; i++) {
        float a = i * 6.2831853f / 28 + t * (0.3f + build * 1.4f);
        Vector2 p{cc.x + std::cos(a) * 420, cc.y + std::sin(a) * 420};
        Vector2 p2{cc.x + std::cos(a + 0.05f) * 420, cc.y + std::sin(a + 0.05f) * 420};
        DrawTriangle(cc, p2, p, Fade(Color{120, 170, 255, 255}, 0.05f + 0.05f * build + 0.03f * std::sin(t * 3 + i)));
    }
    for (int k = 0; k < 4; k++) {
        float u = std::fmod(t * 0.7f + k * 0.25f, 1.0f);
        DrawRing(cc, 30 + u * 220, 32 + u * 220, 0, 360, 64, Fade(Color{170, 210, 255, 255}, 0.25f * (1 - u)));
    }
    for (int i = 0; i < 40; i++) {
        float ph = std::fmod(t * (0.3f + (i % 7) * 0.05f) + i * 0.137f, 1.0f);
        float x = std::fmod(i * 97.3f, (float)VW);
        DrawCircleV({x + std::sin(t + i) * 6, VH - ph * (VH + 20)}, 1.2f + (i % 3) * 0.5f, Fade(Color{200, 230, 255, 255}, 0.6f * std::sin(ph * 3.14159f)));
    }
    DrawCircleV(cc, 60 + build * 40 + std::sin(t * 6) * 4, Fade(Color{150, 190, 255, 255}, 0.12f + build * 0.12f));
    if (t >= 3.0f && t < 3.6f) {   // the burst as it evolves
        float u = (t - 3.0f) / 0.6f;
        DrawRectangle(0, 0, VW, VH, Fade(WHITE, 0.6f * (1 - u)));
        DrawRing(cc, u * 300, u * 300 + 6, 0, 360, 72, Fade(WHITE, 1 - u));
        for (int i = 0; i < 16; i++) {
            float a = i * 6.2832f / 16;
            DrawLineEx({cc.x + std::cos(a) * u * 80, cc.y + std::sin(a) * u * 80}, {cc.x + std::cos(a) * (u * 80 + 40), cc.y + std::sin(a) * (u * 80 + 40)}, 2, Fade(UI_GOLD, 1 - u));
        }
    }
    EndBlendMode();
    if (t > 3.0f)   // sparkles around the new form
        for (int i = 0; i < 6; i++) {
            float a = i * 1.047f + t * 1.5f;
            DrawShinyStar(cc.x + std::cos(a) * 70, cc.y + 20 + std::sin(a) * 40, 2.5f + 1.5f * std::sin(t * 5 + i));
        }
    int from = SpeciesIndex(m.species);
    int show = from;
    float white = 0;
    if (t < 3.0f) {
        float rate = 2.0f + t * 5.0f;
        if (t > 0.8f && ((int)(t * rate) % 2)) show = e.to;
        white = std::min(1.0f, t / 1.2f) * (0.6f + 0.4f * std::sin(t * 10));
    } else {
        show = SpeciesIndex(m.species);
        white = std::max(0.0f, 1.0f - (t - 3.0f) * 3);
    }
    if (show >= 0) {
        DrawSpeciesFit(ShinyIdx(show, m.shiny), VIEW_FRONT, VW / 2, 200, 110);
        if (white > 0) {
            const CreatureSprites& cs = SpritesFor(show);
            const SpriteFrame& fr = cs.hasPortrait ? cs.portrait : cs.view[VIEW_FRONT][0];
            float s = 110.0f / std::max(fr.w, fr.h);
            s = std::floor(s * CANVAS_ZOOM) / CANVAS_ZOOM;
            Rectangle src{0, 0, (float)fr.w, (float)fr.h};
            Rectangle dst{VW / 2 - fr.w * s / 2, 200 - fr.h * s, fr.w * s, fr.h * s};
            DrawTexturePro(fr.white, src, dst, {0, 0}, 0, Fade(WHITE, white));
        }
    }
    if (t < 3.0f) {
        DrawTextCentered(TextFormat("What? %s is evolving!", m.species.c_str()), VW / 2, 250, 20, UI_TEXT);
    } else {
        DrawTextCentered(TextFormat("Congratulations! It evolved into %s!", m.species.c_str()), VW / 2, 250, 20, UI_GOLD);
        if (t > 3.4f) DrawTextCentered("Enter to continue", VW / 2, 300, 10, UI_DIM, false);
    }
    DrawToast(g);
}

// ------------------------------------------------------------------ learn move

void UpdateLearnMove(Game& g) {
    if (g.learnQueue.empty()) { AdvanceStoryQueues(g); return; }
    PlayerInput in = MenuInput();
    const int N = 5;
    if (in.up) { g.learnSel = (g.learnSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.learnSel = (g.learnSel + 1) % N; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    auto l = g.learnQueue.front();
    Profile& LP = Prof(g, l.who);
    OwnedMon& m = LP.mons[l.mon];
    if (g.learnSel < 4 && g.learnSel < (int)m.moves.size()) {
        std::string old = m.moves[g.learnSel];
        m.moves[g.learnSel] = l.move;
        Forgot(m, old);
        m.forgotten.erase(std::remove(m.forgotten.begin(), m.forgotten.end(), l.move), m.forgotten.end());
        PayForLearn(LP, l.src, l.item);
        Toast(g, "1, 2 and... Poof! " + m.species + " forgot " + old + " and learned " + l.move + "!");
        PlaySfx(Sfx::LevelUp);
    } else {
        Toast(g, m.species + " did not learn " + l.move + ".");
        PlaySfx(Sfx::MenuBack);
    }
    g.learnQueue.erase(g.learnQueue.begin());
    SaveWho(g, l.who);
    if (g.returnToShop && g.learnQueue.empty()) {   // the Move Relearner
        g.returnToShop = false;
        g.screen = Screen::Shop;
        return;
    }
    AdvanceStoryQueues(g);
}

void DrawLearnMove(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    if (g.learnQueue.empty()) return;
    const auto& l = g.learnQueue.front();
    const Profile& P = Prof(g, l.who);
    if (l.mon < 0 || l.mon >= (int)P.mons.size()) return;
    const OwnedMon& m = P.mons[l.mon];
    if (g.coop) GDrawText(TextFormat("%s's Pokemon", P.name.c_str()), 40, 14, 10, l.who == 0 ? UI_GOLD : Color{120, 230, 255, 255});
    int idx = MonSpecies(m);
    DrawRectangle(40, 30, 90, 90, UI_PANEL_LIGHT);
    if (idx >= 0) DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, 85, 116, 80);
    DrawTextShadow(TextFormat("%s wants to learn", m.species.c_str()), 146, 34, 20, UI_TEXT);
    DrawTextShadow(l.move.c_str(), 146, 58, 20, UI_GOLD);
    GDrawText("But it already knows four moves. Forget one to make room?", 146, 86, 10, UI_DIM);
    if (l.src == 1) GDrawText(TextFormat("(The %s is only used up if it learns the move.)", l.item.c_str()), 146, 136, 10, Color{140, 220, 255, 255});
    if (l.src == 2) GDrawText(TextFormat("(The Move Relearner charges %d coins only if it learns the move.)", RELEARN_PRICE), 146, 136, 10, Color{140, 220, 255, 255});
    const MoveDef* nm = LookupMove(l.move, idx);
    if (nm) {
        DrawPanel(146, 100, 450, 30, UI_PANEL, UI_GOLD);
        DrawMoveLine(*nm, 154, 104, 434, true);
    }
    for (int i = 0; i < 5; i++) {
        int y = 150 + i * 34;
        bool sel = g.learnSel == i;
        DrawPanel(146, y, 450, 30, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
        if (i < 4 && i < (int)m.moves.size()) {
            const MoveDef* d = LookupMove(m.moves[i], idx);
            if (d) DrawMoveLine(*d, 154, y + 4, 434, true);
            else GDrawText(m.moves[i].c_str(), 164, y + 9, 10, UI_TEXT);
            if (sel) GDrawText("forget", 106, y + 9, 10, Color{255, 120, 100, 255});
        } else if (i == 4) {
            GDrawText(TextFormat("Don't learn %s", l.move.c_str()), 164, y + 9, 10, sel ? UI_GOLD : UI_TEXT);
        }
    }
    GDrawText("Up/Down choose   Enter confirm", 146, 340, 10, UI_DIM);
}

// ------------------------------------------------------------------ reward

void UpdateReward(Game& g) {
    PlayerInput in = MenuInput();
    int n = (int)g.rewardChoices.size();
    if (n == 0) { g.rewardsPending = 0; AdvanceStoryQueues(g); return; }
    if (in.left) { g.rewardSel = (g.rewardSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.right) { g.rewardSel = (g.rewardSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    int idx = g.rewardChoices[g.rewardSel];
    int who = g.rewardWho.empty() ? 0 : g.rewardWho.front();
    Profile& P = Prof(g, who);
    bool boxed = AddMon(P, NewMon(idx, NewMonLevel(P)));
    g.rewardsPending--;
    if (!g.rewardWho.empty()) g.rewardWho.erase(g.rewardWho.begin());
    SaveWho(g, who);
    PlaySfx(Sfx::LevelUp);
    std::string who2 = g.coop ? P.name + ": " : "";
    Toast(g, who2 + AllSpecies()[idx].name + (boxed ? " was sent to the Box (your party is full)." : " joined your team!"));
    AdvanceStoryQueues(g);
}

void DrawReward(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    int who = g.rewardWho.empty() ? 0 : g.rewardWho.front();
    const Profile& P = Prof(g, who);
    DrawTextCentered(g.coop ? TextFormat("%s: CHOOSE A NEW POKEMON", P.name.c_str()) : "CHOOSE A NEW POKEMON", VW / 2, 20, 20,
                     g.coop && who == 1 ? Color{120, 230, 255, 255} : UI_GOLD);
    DrawTextCentered(TextFormat("It joins at level %d.", NewMonLevel(P)), VW / 2, 46, 10, UI_TEXT, false);
    int n = (int)g.rewardChoices.size();
    int cw = 150, gap = 20;
    int x0 = VW / 2 - (n * cw + (n - 1) * gap) / 2;
    for (int i = 0; i < n; i++) DrawCreatureCard(g.rewardChoices[i], NewMonLevel(P), x0 + i * (cw + gap), 70, cw, 130, g.rewardSel == i);
    if (n > 0) {
        int idx = g.rewardChoices[g.rewardSel];
        const SpeciesDef& sp = AllSpecies()[idx];
        DrawPanel(120, 212, 400, 120);
        auto moves = MovesAtLevel(sp, NewMonLevel(P));
        GDrawText("Knows:", 130, 220, 10, UI_DIM);
        for (size_t k = 0; k < moves.size() && k < 4; k++) DrawMoveLine(moves[k], 130, 234 + (int)k * 22, 380, true);
    }
    DrawTextCentered("Left/Right choose   Enter confirm", VW / 2, 342, 10, UI_DIM, false);
    DrawToast(g);
}

// ------------------------------------------------------------------ catching

namespace {
Color BallColor(const std::string& n) {
    if (n == "Master Ball") return {150, 80, 200, 255};
    if (n == "Ultra Ball") return {50, 50, 56, 255};
    if (n == "Great Ball") return {70, 120, 230, 255};
    return {230, 60, 60, 255};
}
// Ball types the player has, in shop order.
std::vector<const ItemDef*> OwnedBalls(const Profile& p) {
    std::vector<const ItemDef*> v;
    for (const auto& d : ItemList())
        if (IsBall(d) && BagCount(p, d.name) > 0) v.push_back(&d);
    return v;
}
float BallChance(float base, const ItemDef* ball) {
    if (!ball) return base;
    if (ball->value <= 0) return 1.0f;  // Master Ball
    return std::min(0.98f, 1.0f - std::pow(1.0f - base, ball->value));
}
} // namespace

// This player's turn is over: what they threw at leaves the pool, then the next player picks.
void EndCatchTurn(Game& g) {
    if (g.on.active && g.catchWho == g.on.me) OnlineSendCatch(g.catchPoolPick);
    if (g.catchPoolPick >= 0 && g.catchPoolPick < (int)g.catchPool.size()) g.catchPool.erase(g.catchPool.begin() + g.catchPoolPick);
    g.catchPoolPick = -1;
    g.catchTurn++;
    if (g.catchTurn < (int)g.catchOrder.size() && !g.catchPool.empty()) {
        g.catchWho = g.catchOrder[g.catchTurn];
        g.catchCands = g.catchPool;
        g.catchSel = 0;
        g.catchPhase = 0;
        g.catchT = 0;
        g.ballSel = 0;
        return;
    }
    g.catchCands.clear();
    g.catchPool.clear();
    AdvanceStoryQueues(g);
}

void UpdateCatch(Game& g, float dt) {
    int n = (int)g.catchCands.size();
    if (g.on.active && g.catchWho != g.on.me) {  // the partner's turn: watch their throws, then take what's left
        float resultAt = 1.3f + g.catchShakes * 0.6f;
        bool idle = g.catchPhase == 0 || (g.catchPhase == 2 && g.catchT > resultAt + 1.4f);
        if (g.catchPhase > 0) {
            float before = g.catchT;
            g.catchT += dt;
            for (int k = 0; k < g.catchShakes; k++) {
                float at = 1.3f + k * 0.6f;
                if (before < at && g.catchT >= at) PlaySfx(Sfx::Thud);
            }
            if (g.catchPhase == 1 && g.catchT >= resultAt) { g.catchPhase = 2; PlaySfx(g.catchOk ? Sfx::LevelUp : Sfx::Faint); }
        }
        if (idle && !g.on.partnerThrows.empty()) {
            OnlineSession::Throw t = g.on.partnerThrows.front();
            g.on.partnerThrows.pop_front();
            Game::CatchCand c{t.species, t.level, {}, 1.0f, 0.0f};
            for (const auto& pc : g.catchPool) if (pc.species == t.species && pc.level == t.level) { c = pc; break; }
            g.catchCands = {c};
            g.catchSel = 0;
            g.catchOk = t.ok;
            g.catchShakes = t.shakes;
            g.catchFled = t.fled;
            g.catchBoxed = false;
            g.catchPhase = 1;
            g.catchT = 0;
            PlaySfx(Sfx::Shoot);
            return;
        }
        if (idle && g.on.partnerThrows.empty() && g.on.partnerCatch.empty() && g.on.reconnecting) {
            g.catchPoolPick = -1;   // the partner dropped out: they pass on this one, the draft goes on
            EndCatchTurn(g);
            return;
        }
        if (idle && g.on.partnerThrows.empty() && !g.on.partnerCatch.empty()) {
            g.catchPoolPick = g.on.partnerCatch.front();
            g.on.partnerCatch.pop_front();
            if (g.catchPoolPick >= (int)g.catchPool.size()) g.catchPoolPick = -1;
            EndCatchTurn(g);
        }
        return;
    }
    if (n == 0) { EndCatchTurn(g); return; }
    PlayerInput in = MenuInput();
    Profile& P = Prof(g, g.catchWho);
    auto balls = OwnedBalls(P);
    if (g.catchPhase == 0) {
        int N = n + 1;  // + "leave it"
        if (in.left) { g.catchSel = (g.catchSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.right) { g.catchSel = (g.catchSel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (!balls.empty()) {
            g.ballSel = std::min(g.ballSel, (int)balls.size() - 1);
            if (in.up) { g.ballSel = (g.ballSel + (int)balls.size() - 1) % (int)balls.size(); PlaySfx(Sfx::MenuMove); }
            if (in.down) { g.ballSel = (g.ballSel + 1) % (int)balls.size(); PlaySfx(Sfx::MenuMove); }
        }
        if (in.back) { PlaySfx(Sfx::MenuBack); EndCatchTurn(g); return; }
        if (!in.confirm) return;
        if (g.catchSel == n) { PlaySfx(Sfx::MenuBack); EndCatchTurn(g); return; }
        if (balls.empty()) { Toast(g, "No Poke Balls! Buy some at the Shop."); PlaySfx(Sfx::MenuBack); return; }
        const ItemDef* ball = balls[g.ballSel];
        BagTake(P, ball->name);
        SaveWho(g, g.catchWho);
        if (g.catchPoolPick < 0) g.catchPoolPick = g.catchSel;  // first throw: this one is taken
        // Once you throw at one, the others leave.
        Game::CatchCand c = g.catchCands[g.catchSel];
        g.catchCands = {c};
        g.catchSel = 0;
        float ch = BallChance(c.chance, ball);
        g.catchOk = GetRandomValue(0, 9999) < (int)(ch * 10000);
        g.catchShakes = g.catchOk ? 3 : GetRandomValue(0, 2);
        g.catchFled = !g.catchOk && GetRandomValue(0, 99) < 35;
        if (g.on.active) OnlineSendThrow(c.species, c.level, g.catchOk, g.catchShakes, g.catchFled);
        g.catchPhase = 1;
        g.catchT = 0;
        PlaySfx(Sfx::Shoot);
        return;
    }
    float before = g.catchT;
    g.catchT += dt;
    // 0-0.7 throw, 0.7-1.1 absorb, then one shake per 0.6 s, then the result
    for (int k = 0; k < g.catchShakes; k++) {
        float at = 1.3f + k * 0.6f;
        if (before < at && g.catchT >= at) PlaySfx(Sfx::Thud);
    }
    float resultAt = 1.3f + g.catchShakes * 0.6f;
    if (g.catchPhase == 1 && g.catchT >= resultAt) {
        g.catchPhase = 2;
        const auto& c = g.catchCands[0];
        if (g.catchOk) {
            OwnedMon m = NewMon(c.species, c.level);
            if (!c.moves.empty()) m.moves = c.moves;
            m.abilitySlot = c.abSlot;
            m.nature = c.nature;
            m.shiny = c.shiny;
            m.hp = c.hpFrac;
            m.caught = true;
            g.catchBoxed = AddMon(P, m);
            SaveWho(g, g.catchWho);
            if (g.coop) {
                g.coopSave.run[g.catchWho & 1].caught++;
                if (g.on.active) OnlineSendCoop(g);
                else SaveCoop(g.coopSave);
            }
            PlaySfx(Sfx::LevelUp);
        } else {
            PlaySfx(Sfx::Faint);
        }
    }
    if (g.catchPhase == 2 && g.catchT > resultAt + 0.5f && (in.confirm || in.back)) {
        PlaySfx(Sfx::MenuOk);
        if (!g.catchOk && !g.catchFled && !OwnedBalls(P).empty() && in.confirm) {
            g.catchPhase = 0;  // still here: throw again
            g.catchT = 0;
            return;
        }
        EndCatchTurn(g);
    }
}

void DrawCatch(const Game& g) {
    DrawArena(g.t);
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.6f));
    int n = (int)g.catchCands.size();
    if (n == 0) return;
    const Profile& P = Prof(g, g.catchWho);
    auto balls = OwnedBalls(P);
    bool watching = g.on.active && g.catchWho != g.on.me;
    if (watching && g.catchPhase == 0) {
        Color pc = (g.catchWho == 0 ? UI_GOLD : Color{120, 230, 255, 255});
        DrawTextCentered("CATCH DRAFT", VW / 2, 14, 20, UI_GOLD);
        DrawTextCentered(TextFormat("%s is choosing which Pokemon to catch...", P.name.c_str()), VW / 2, 40, 10, pc, false);
        int cw = std::min(150, 560 / std::max(1, n)), gap = 10;
        int x0 = VW / 2 - (n * cw + (n - 1) * gap) / 2;
        int ps = g.on.partnerCatchSel;
        for (int i = 0; i < n; i++) {
            int x = x0 + i * (cw + gap);
            DrawCreatureCard(ShinyIdx(g.catchCands[i].species, g.catchCands[i].shiny), g.catchCands[i].level, x, 70, cw, 124, false);
            DrawHpBar(x + 10, 200, cw - 20, 4, g.catchCands[i].hpFrac);
            if (ps == i) {   // the partner's cursor
                DrawRectangleLinesEx({(float)x - 3, 67, (float)cw + 6, 130}, 2, pc);
                float bob = std::sin(g.t * 6) * 2;
                DrawTriangle({x + cw / 2.0f - 6, 58 + bob}, {x + cw / 2.0f, 66 + bob}, {x + cw / 2.0f + 6, 58 + bob}, pc);
            }
        }
        DrawMenuItem("Leave it", VW / 2, 214, 10, ps == n);
        DrawTextCentered("You'll pick from what's left next.", VW / 2, 236, 10, UI_DIM, false);
        return;
    }
    if (watching) {
        Color pc = (g.catchWho == 0 ? UI_GOLD : Color{120, 230, 255, 255});
        DrawPanel(4, 4, 190, 30, UI_PANEL, pc);
        GDrawText(TextFormat("Watching %s", P.name.c_str()), 10, 8, 10, pc);
        GDrawText("Your pick comes next", 10, 20, 10, UI_DIM);
    } else
    if (g.coop) {
        Color pc = g.catchWho == 0 ? UI_GOLD : Color{120, 230, 255, 255};
        DrawPanel(4, 4, 150, 30, UI_PANEL, pc);
        GDrawText(TextFormat("%s's turn", P.name.c_str()), 10, 8, 10, pc);
        GDrawText(TextFormat("Pick %d of %d - one each", g.catchTurn + 1, (int)g.catchOrder.size()), 10, 20, 10, UI_DIM);
    }
    const ItemDef* ball = balls.empty() ? nullptr : balls[std::min(g.ballSel, (int)balls.size() - 1)];
    if (g.catchPhase == 0) {
        DrawTextCentered("CATCH A POKEMON", VW / 2, 14, 20, UI_GOLD);
        DrawTextCentered(g.coop ? "Pick one Pokemon (your partner can't take the same one) and one of your balls."
                         : (n > 1 ? "Pick one weakened foe and a ball. Lower HP = easier catch." : "Throw a ball! Lower HP = easier catch."),
                         VW / 2, 40, 10, UI_TEXT, false);
        int cw = std::min(150, 560 / std::max(1, n)), gap = 10;
        int x0 = VW / 2 - (n * cw + (n - 1) * gap) / 2;
        for (int i = 0; i < n; i++) {
            const auto& c = g.catchCands[i];
            int x = x0 + i * (cw + gap);
            DrawCreatureCard(ShinyIdx(c.species, c.shiny), c.level, x, 56, cw, 124, g.catchSel == i);
            DrawHpBar(x + 10, 186, cw - 20, 4, c.hpFrac);
            float ch = BallChance(c.chance, ball);
            DrawTextCentered(TextFormat("%d%% chance", (int)std::round(ch * 100)), x + cw / 2, 194, 10,
                             ch >= 0.6f ? Color{120, 230, 130, 255} : (ch >= 0.35f ? UI_GOLD : Color{255, 130, 100, 255}), false);
        }
        DrawMenuItem("Leave it", VW / 2, 212, 10, g.catchSel == n);
        // Ball picker
        DrawPanel(150, 230, 340, 70);
        GDrawText("Ball (Up/Down):", 160, 236, 10, UI_DIM);
        if (balls.empty()) {
            GDrawText("No Poke Balls left! Buy more at the Shop.", 160, 256, 10, Color{255, 130, 100, 255});
        } else {
            for (int i = 0; i < (int)balls.size(); i++) {
                int bx = 170 + i * 80, by = 266;
                bool sel = ball == balls[i];
                if (sel) RectLines(bx - 12, by - 14, 76, 30, UI_GOLD);
                DrawPokeBall((float)bx, (float)by, 6, 0, BallColor(balls[i]->name));
                GDrawText(TextFormat("x%d", BagCount(P, balls[i]->name)), bx + 10, by - 10, 10, UI_TEXT);
                std::string nm = balls[i]->name;
                GDrawText(nm.substr(0, nm.find(' ')).c_str(), bx + 10, by + 1, 10, sel ? UI_GOLD : UI_DIM);
            }
        }
        DrawTextCentered("Left/Right Pokemon   Up/Down ball   Enter throw   Esc leave", VW / 2, 340, 10, UI_DIM, false);
        return;
    }
    const auto& c = g.catchCands[0];
    float t = g.catchT;
    Vector2 mon{VW / 2.0f, 190};
    Vector2 from{120, 320};
    float resultAt = 1.3f + g.catchShakes * 0.6f;
    bool inBall = t >= 0.9f && !(g.catchPhase == 2 && !g.catchOk);
    {   // a spotlight on the ground
        BeginBlendMode(BLEND_ADDITIVE);
        DrawEllipse((int)mon.x, (int)mon.y + 20, 70, 16, Fade(Color{255, 240, 200, 255}, 0.10f));
        DrawEllipse((int)mon.x, (int)mon.y + 20, 44, 10, Fade(Color{255, 240, 200, 255}, 0.10f));
        EndBlendMode();
    }
    if (!inBall && !(g.catchPhase == 2 && g.catchFled)) {
        float white = (t > 0.7f && t < 0.9f) ? (t - 0.7f) / 0.2f : 0.0f;
        float shrink = (t > 0.7f && t < 0.9f && g.catchPhase != 2) ? 1 - (t - 0.7f) / 0.2f : 1.0f;
        int box = std::max(4, (int)(90 * shrink));
        DrawSpeciesFit(ShinyIdx(c.species, c.shiny), VIEW_FRONT, (int)mon.x, (int)mon.y + 20 - (int)((1 - shrink) * 20), box, WHITE);
        if (white > 0) {   // pulled into the ball as red light
            BeginBlendMode(BLEND_ADDITIVE);
            for (int k = -2; k <= 2; k++) DrawLineEx({mon.x + k * 4 * shrink, mon.y - 10 * shrink}, {mon.x, mon.y + 18}, 2, Fade(Color{255, 90, 80, 255}, white * 0.8f));
            DrawCircleV({mon.x, mon.y - 5}, 30 * shrink + 6, Fade(Color{255, 120, 100, 255}, white * 0.35f));
            EndBlendMode();
        }
    }
    if (t < 0.7f) {  // the throw
        float u = t / 0.7f;
        Vector2 p = LerpV(from, mon, u);
        p.y -= std::sin(u * PI) * 90;
        DrawPokeBall(p.x, p.y, 6, u * 720);
    } else {
        Vector2 ground{mon.x, mon.y + 18};
        float rot = 0, squash = 0;
        if (g.catchPhase == 1 && t > 1.1f) {
            float k = std::fmod(t - 1.1f, 0.6f);
            if (k < 0.3f) { rot = std::sin(k / 0.3f * PI * 2) * 30; squash = std::sin(k / 0.3f * PI) * 0.12f; }
            // the button blinks red while it decides
            if (k < 0.12f) {
                BeginBlendMode(BLEND_ADDITIVE);
                DrawCircleV({ground.x, ground.y - 9}, 6, Fade(Color{255, 80, 70, 255}, 0.6f));
                EndBlendMode();
            }
        }
        if (t < 1.1f && t >= 0.7f) {   // dropping to the ground with a bounce
            float u = (t - 0.7f) / 0.4f;
            ground.y -= std::fabs(std::cos(u * PI * 1.5f)) * (1 - u) * 26;
        }
        DrawEllipse((int)mon.x, (int)mon.y + 28, 12, 3, Fade(BLACK, 0.4f));
        (void)squash;
        if (g.catchPhase == 2 && !g.catchOk) {
            float u = std::min(1.0f, (t - resultAt) / 0.3f);
            DrawPokeBall(ground.x - 20 * u, ground.y + 10 * u, 6, 0);
        } else {
            ::DrawPokeBall({ground.x, ground.y}, 9, rot, 0, Color{230, 60, 60, 255});
            if (g.catchPhase == 2 && g.catchOk) {   // the click: a star burst and sparkles
                float u = std::min(1.0f, (t - resultAt) / 0.6f);
                BeginBlendMode(BLEND_ADDITIVE);
                for (int i = 0; i < 8; i++) {
                    float a = i * PI / 4 + 0.3f;
                    float r0 = 10 + u * 34, r1 = r0 + 8 * (1 - u);
                    DrawLineEx({ground.x + std::cos(a) * r0, ground.y + std::sin(a) * r0 * 0.8f}, {ground.x + std::cos(a) * r1, ground.y + std::sin(a) * r1 * 0.8f}, 2, Fade(UI_GOLD, 1 - u));
                }
                DrawCircleV(ground, 26 * (1 - u) + 4, Fade(Color{255, 240, 180, 255}, 0.4f * (1 - u)));
                EndBlendMode();
                for (int i = 0; i < 3; i++) {
                    float ph = std::fmod(t * 1.5f + i * 0.33f, 1.0f);
                    DrawShinyStar(ground.x - 16 + i * 16, ground.y - 18 - ph * 16, 3.5f * (1 - ph) + 1);
                }
            }
        }
    }
    const SpeciesDef& sp = AllSpecies()[c.species];
    if (g.catchPhase == 1) {
        int shown = t > 1.1f ? std::min(g.catchShakes, (int)((t - 1.1f) / 0.6f) + 1) : 0;
        std::string dots(shown, '.');
        DrawTextCentered(watching ? TextFormat("%s threw a ball at %s%s", P.name.c_str(), sp.name.c_str(), dots.c_str())
                                  : TextFormat("You threw a ball at %s%s", sp.name.c_str(), dots.c_str()), VW / 2, 262, 20, UI_TEXT);
    } else if (g.catchOk) {
        DrawTextCentered(TextFormat("Gotcha! %s was caught!", sp.name.c_str()), VW / 2, 252, 20, UI_GOLD);
        if (watching) DrawTextCentered(TextFormat("It joins %s's team at level %d.", P.name.c_str(), c.level), VW / 2, 280, 10, UI_TEXT, false);
        else DrawTextCentered(g.catchBoxed ? TextFormat("Your party is full, so it went to the Box (level %d).", c.level)
                                      : TextFormat("It joins %s party at level %d (with its HP as it was).", g.coop ? (P.name + "'s").c_str() : "your", c.level),
                         VW / 2, 280, 10, UI_TEXT, false);
    } else {
        const char* lines[3] = {"Oh no! It broke free!", "Aww! It appeared to be caught!", "Argh! Almost had it!"};
        DrawTextCentered(lines[std::min(2, g.catchShakes)], VW / 2, 252, 20, Color{255, 130, 100, 255});
        if (g.catchFled) DrawTextCentered(TextFormat("%s fled!", sp.name.c_str()), VW / 2, 280, 10, UI_TEXT, false);
        else if (watching) DrawTextCentered(TextFormat("%s is still here...", sp.name.c_str()), VW / 2, 280, 10, UI_GOLD, false);
        else if (!OwnedBalls(P).empty()) DrawTextCentered(TextFormat("%s is still here! Enter: throw again   Esc: let it go", sp.name.c_str()), VW / 2, 280, 10, UI_GOLD, false);
        else DrawTextCentered("You're out of Poke Balls...", VW / 2, 280, 10, UI_TEXT, false);
    }
    bool again = g.catchPhase == 2 && !g.catchOk && !g.catchFled && !OwnedBalls(P).empty();
    if (g.catchPhase == 2 && t > resultAt + 0.5f && !again && !watching) DrawTextCentered("Enter to continue", VW / 2, 330, 10, UI_DIM, false);
}

// ------------------------------------------------------------------ healing center

namespace {
// Only the party is healed here (Pokemon in the Box wait there until they rejoin the party).
std::vector<int> HealParty(const Profile& p) {
    std::vector<int> v;
    for (int i = 0; i < (int)p.mons.size(); i++) if (!p.mons[i].boxed) v.push_back(i);
    return v;
}
int HealAllCost(const Profile& p) {
    int c = 0;
    for (int i : HealParty(p)) c += HealCost(p.mons[i]);
    return c;
}
bool EmergencyHeal(const Profile& p) {
    auto party = HealParty(p);
    if (party.empty()) return false;
    for (int i : party) if (p.mons[i].hp > 0) return false;
    int cheapest = 1 << 30;
    for (int i : party) cheapest = std::min(cheapest, HealCost(p.mons[i]));
    return p.coins < cheapest;
}
float gHealAnim = 0;     // the healing machine's jingle (seconds left)
int gHealOnly = -1;      // which party slot lights up (-1: all)
} // namespace

void UpdateHeal(Game& g) {
    PlayerInput in = MenuInput();
    gHealAnim = std::max(0.0f, gHealAnim - GetFrameTime());
    Profile& p = g.profile;
    auto party = HealParty(p);
    int n = (int)party.size();
    int N = n + 2;  // heal all, each party Pokemon, back
    g.healSel = std::min(g.healSel, N - 1);
    if (in.up) { g.healSel = (g.healSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.healSel = (g.healSel + 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.back || (in.confirm && g.healSel == N - 1)) {
        PlaySfx(Sfx::MenuBack);
        SaveOrWarn(g);
        EnterStoryHub(g);
        return;
    }
    if (!in.confirm || gHealAnim > 0.4f) return;
    auto healed = [&](int only) { gHealAnim = 1.6f; gHealOnly = only; PlaySfx(Sfx::Heal); SaveOrWarn(g); };
    if (g.healSel == 0) {
        if (EmergencyHeal(p)) {
            for (int i : party) p.mons[i].hp = 1;
            Toast(g, "The nurse patched up your team for free. Take care out there!");
            healed(-1);
            return;
        }
        int cost = HealAllCost(p);
        if (cost == 0) { Toast(g, "Your party is already at full health."); return; }
        if (cost > p.coins) {
            // Heal as many as the coins allow, cheapest first.
            std::vector<int> order;
            for (int i : party) if (HealCost(p.mons[i]) > 0) order.push_back(i);
            std::sort(order.begin(), order.end(), [&](int a, int b) { return HealCost(p.mons[a]) < HealCost(p.mons[b]); });
            int count = 0;
            for (int i : order) {
                int c = HealCost(p.mons[i]);
                if (c > p.coins) continue;
                p.coins -= c;
                p.mons[i].hp = 1;
                count++;
            }
            if (!count) { Toast(g, "Not enough coins!"); PlaySfx(Sfx::MenuBack); return; }
            Toast(g, TextFormat("Not enough coins for everyone - healed %d.", count));
        } else {
            p.coins -= cost;
            for (int i : party) p.mons[i].hp = 1;
            Toast(g, "We've restored your Pokemon to full health!");
        }
        healed(-1);
        return;
    }
    int slot = g.healSel - 1;
    OwnedMon& m = p.mons[party[slot]];
    int cost = HealCost(m);
    if (cost == 0) { Toast(g, m.species + " is already healthy."); return; }
    if (cost > p.coins) { Toast(g, "Not enough coins!"); PlaySfx(Sfx::MenuBack); return; }
    p.coins -= cost;
    bool revived = m.hp <= 0;
    m.hp = 1;
    Toast(g, revived ? m.species + " was revived!" : m.species + " is fully healed!");
    healed(slot);
}

void DrawHeal(const Game& g) {
    const Color pink{248, 214, 226, 255}, pink2{252, 232, 238, 255}, red{220, 56, 72, 255}, deep{150, 30, 50, 255};
    // a Pokemon Center: pink-and-white walls, a tiled floor, the red roof sign
    ClearBackground(pink2);
    for (int x = 0; x < VW; x += 32) DrawRectangle(x, 30, 16, 200, pink);
    DrawRectangle(0, 230, VW, VH - 230, Color{236, 226, 214, 255});
    for (int x = 0; x < VW; x += 24)
        for (int y = 230; y < VH; y += 24) DrawRectangleLines(x, y, 24, 24, Color{220, 208, 194, 255});
    DrawRectangle(0, 0, VW, 30, red);
    DrawRectangle(0, 28, VW, 3, deep);
    // the Pokemon Center emblem: a white Poke Ball with a red cross
    DrawCircle(20, 15, 10, WHITE);
    DrawRectangle(17, 8, 6, 14, red);
    DrawRectangle(13, 12, 14, 6, red);
    DrawTextShadow("POKEMON CENTER", 38, 6, 20, WHITE);
    const Profile& p = g.profile;
    if (g.coop) GDrawText(p.name.c_str(), 330, 11, 10, WHITE);
    {   // coins
        const char* c = TextFormat("%d", p.coins);
        int x = VW - 18 - TextW(c, 10);
        DrawCircle(x - 7, 15, 5, Color{170, 120, 20, 255});
        DrawCircle(x - 7, 15, 4, kCoin);
        GDrawText(c, x, 11, 10, WHITE);
    }
    auto party = HealParty(p);
    int n = (int)party.size();
    int N = n + 2;
    bool emergency = EmergencyHeal(p);
    int allCost = HealAllCost(p);
    float t = g.t;

    // left: the counter, the nurse's words and the healing machine
    DrawRectangleRounded({16, 44, 214, 120}, 0.12f, 8, WHITE);
    DrawRectangleRoundedLines({16, 44, 214, 120}, 0.12f, 8, Color{230, 150, 170, 255});
    GDrawText("Welcome to the Pokemon Center!", 26, 52, 10, deep);
    DrawWrapped("We restore your tired Pokemon to full health. Only your party is treated here - Pokemon in the Box rest until they rejoin your team.",
                26, 68, 196, 10, Color{90, 70, 80, 255});
    GDrawText("Heal: about 3 coins per level", 26, 132, 10, Color{120, 100, 110, 255});
    GDrawText("Revive: 6 per level + 10", 26, 144, 10, Color{120, 100, 110, 255});
    // the healing machine with a socket per party Pokemon
    DrawRectangleRounded({30, 176, 186, 80}, 0.2f, 8, Color{90, 96, 110, 255});
    DrawRectangleRounded({36, 182, 174, 68}, 0.2f, 8, Color{60, 64, 76, 255});
    DrawRectangle(98, 168, 50, 10, Color{120, 126, 140, 255});
    DrawCircle(123, 172, 4, gHealAnim > 0 ? Color{120, 255, 160, 255} : Color{70, 200, 110, 255});
    for (int k = 0; k < 6; k++) {
        float cx = 62 + (k % 3) * 47.0f, cy = 199 + (k / 3) * 30.0f;
        DrawCircle((int)cx, (int)cy, 11, Color{40, 42, 52, 255});
        if (k >= n) continue;
        const OwnedMon& m = p.mons[party[k]];
        bool lit = gHealAnim > 0 && (gHealOnly < 0 || gHealOnly == k) && ((int)((1.6f - gHealAnim) * 8) + k) % 2 == 0;
        Color top = m.hp <= 0 ? Color{120, 60, 60, 255} : Color{230, 60, 60, 255};
        if (lit) {
            DrawCircle((int)cx, (int)cy, 13, Fade(Color{255, 250, 180, 255}, 0.6f));
            top = Color{255, 140, 140, 255};
        }
        DrawCircle((int)cx, (int)cy, 8, UI_INK);
        DrawCircleSector({cx, cy}, 7, 180, 360, 16, top);
        DrawCircleSector({cx, cy}, 7, 0, 180, 16, WHITE);
        DrawRectangle((int)cx - 7, (int)cy, 14, 1, UI_INK);
        DrawCircle((int)cx, (int)cy, 2, WHITE);
    }
    if (gHealAnim > 0) DrawTextCentered("* ding ding ding... *", 123, 262, 10, deep, false);

    // right: the party
    int x = 244, w = 380;
    for (int i = 0; i < N; i++) {
        bool sel = g.healSel == i;
        int y = i == 0 ? 42 : (i == N - 1 ? 42 + 26 + n * 36 + 4 : 42 + 26 + (i - 1) * 36);
        int h = (i == 0 || i == N - 1) ? 22 : 34;
        Color bg = sel ? Color{255, 246, 210, 255} : WHITE;
        Color edge = sel ? Color{240, 170, 40, 255} : Color{230, 180, 196, 255};
        DrawRectangleRounded({(float)x, (float)y, (float)w, (float)h}, 0.25f, 8, bg);
        DrawRectangleRoundedLines({(float)x, (float)y, (float)w, (float)h}, 0.25f, 8, edge);
        if (sel) DrawTriangle({(float)x - 9, (float)y + h / 2.0f - 5}, {(float)x - 9, (float)y + h / 2.0f + 5}, {(float)x - 3, (float)y + h / 2.0f}, red);
        Color ink{60, 50, 60, 255};
        if (i == 0) {
            const char* tl = emergency ? "Emergency care (free - your whole party is down)" : (allCost ? "Heal your party" : "Your party is healthy");
            DrawCircle(x + 14, y + 11, 6, red);
            DrawRectangle(x + 13, y + 7, 2, 8, WHITE);
            DrawRectangle(x + 10, y + 10, 8, 2, WHITE);
            GDrawText(tl, x + 26, y + 6, 10, emergency ? Color{40, 150, 70, 255} : ink);
            if (!emergency && allCost) {
                const char* c = TextFormat("%d coins", allCost);
                GDrawText(c, x + w - 10 - TextW(c, 10), y + 6, 10, allCost <= p.coins ? Color{190, 130, 20, 255} : red);
            }
        } else if (i == N - 1) {
            GDrawText("Back", x + 26, y + 6, 10, ink);
        } else {
            const OwnedMon& m = p.mons[party[i - 1]];
            int idx = MonSpecies(m);
            DrawCircle(x + 20, y + 17, 15, m.hp <= 0 ? Color{230, 220, 224, 255} : Color{232, 244, 236, 255});
            if (idx >= 0) DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, x + 20, y + 32, 28, m.hp > 0 ? WHITE : Color{110, 110, 120, 255});
            if (m.shiny) DrawShinyStar((float)x + 6, (float)y + 6, 3);
            GDrawText(m.species.c_str(), x + 42, y + 5, 10, ink);
            GDrawText(TextFormat("Lv%d", m.level), x + 150, y + 5, 10, Color{190, 130, 20, 255});
            DrawRectangle(x + 42, y + 21, 154, 6, Color{80, 80, 90, 255});
            GDrawText("HP", x + 42, y + 19, 10, Color{230, 160, 40, 255});
            DrawHpBar(x + 60, y + 22, 134, 4, m.hp);
            const char* st = m.hp <= 0 ? "Fainted" : (m.hp >= 0.999f ? "Healthy" : TextFormat("%d%%", (int)std::round(m.hp * 100)));
            GDrawText(st, x + 206, y + 19, 10, m.hp <= 0 ? red : (m.hp >= 0.999f ? Color{40, 150, 70, 255} : ink));
            int c = HealCost(m);
            const char* ct = c ? TextFormat("%d coins", c) : "-";
            GDrawText(ct, x + w - 10 - TextW(ct, 10), y + 12, 10, c == 0 ? Color{160, 160, 170, 255} : (c <= p.coins ? Color{190, 130, 20, 255} : red));
        }
    }
    GDrawText("Win battles to earn coins: trainers pay well, gym leaders pay a lot.", 16, 326, 10, Color{120, 100, 110, 255});
    GDrawText("Up/Down choose   Enter heal   Esc / B back", 16, 340, 10, Color{120, 100, 110, 255});
    DrawToast(g);
}

// ------------------------------------------------------------------ shop

int StoryNewMonLevel(const Profile& p) { return NewMonLevel(p); }
void StorySaveProfile(Game& g) { SaveOrWarn(g); }

namespace {
std::vector<const ItemDef*> ShopList(int tab) {
    std::vector<const ItemDef*> v;
    for (const auto& d : ItemList()) {
        if (d.badges >= 99) continue;   // story and Game Corner items
        if ((tab == 0 && IsBall(d)) || (tab == 1 && (IsHoldable(d) || d.fx == ItemFx::ExpAll)) || (tab == 2 && IsStone(d)) || (tab == 3 && IsTM(d))) v.push_back(&d);
    }
    return v;
}
const char* BadgeNeedText(int badges) {
    static const char* names[9] = {"", "Boulder", "Cascade", "Thunder", "Rainbow", "Soul", "Marsh", "Volcano", "Earth"};
    return badges >= 1 && badges <= 8 ? TextFormat("Needs the %s Badge", names[badges]) : "Locked";
}
} // namespace

void UpdateShop(Game& g) {
    PlayerInput in = MenuInput();
    if (in.back && g.shopTab == 4 && g.relearnMon >= 0) {
        int back = g.relearnMon;
        g.relearnMon = -1;
        auto party = TabList(g.profile, 0);
        for (int i = 0; i < (int)party.size(); i++) if (party[i] == back) g.shopSel = i;
        PlaySfx(Sfx::MenuBack);
        return;
    }
    if (in.back) { PlaySfx(Sfx::MenuBack); SaveOrWarn(g); EnterStoryHub(g); g.hubSel = 3; return; }
    const int TABS = 6;
    if (in.left) { g.shopTab = (g.shopTab + TABS - 1) % TABS; g.shopSel = 0; g.relearnMon = -1; PlaySfx(Sfx::MenuMove); }
    if (in.right) { g.shopTab = (g.shopTab + 1) % TABS; g.shopSel = 0; g.relearnMon = -1; PlaySfx(Sfx::MenuMove); }
    if (g.shopTab == 5) { UpdateGameCorner(g, in); return; }
    if (g.shopTab == 4) {   // Move Relearner: pick a creature, then a move it can remember
        Profile& p = g.profile;
        auto party = TabList(p, 0);
        if (g.relearnMon < 0) {
            int n = (int)party.size();
            if (n == 0) return;
            if (in.up) { g.shopSel = (g.shopSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
            if (in.down) { g.shopSel = (g.shopSel + 1) % n; PlaySfx(Sfx::MenuMove); }
            g.shopSel = std::min(g.shopSel, n - 1);
            if (!in.confirm) return;
            if (RelearnMoves(p.mons[party[g.shopSel]]).empty()) { Toast(g, "It has no moves to remember right now."); PlaySfx(Sfx::MenuBack); return; }
            g.relearnMon = party[g.shopSel];
            g.shopSel = 0;
            PlaySfx(Sfx::MenuOk);
            return;
        }
        if (g.relearnMon >= (int)p.mons.size()) { g.relearnMon = -1; return; }
        auto mv = RelearnMoves(p.mons[g.relearnMon]);
        int n = (int)mv.size();
        if (n == 0) { g.relearnMon = -1; return; }
        g.shopSel = std::min(g.shopSel, n - 1);
        if (in.up) { g.shopSel = (g.shopSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
        if (in.down) { g.shopSel = (g.shopSel + 1) % n; PlaySfx(Sfx::MenuMove); }
        if (!in.confirm) return;
        if (p.coins < RELEARN_PRICE) { Toast(g, "Not enough coins!"); PlaySfx(Sfx::MenuBack); return; }
        TeachMove(g, g.relearnMon, mv[g.shopSel].second, 2, "", true);
        return;
    }
    auto list = ShopList(g.shopTab);
    int n = (int)list.size();
    if (n == 0) return;
    if (in.up) { g.shopSel = (g.shopSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.shopSel = (g.shopSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    const ItemDef& d = *list[g.shopSel];
    Profile& p = g.profile;
    if (BadgeCount(g) < d.badges) { Toast(g, std::string(BadgeNeedText(d.badges)) + " to buy this."); PlaySfx(Sfx::MenuBack); return; }
    if (d.fx == ItemFx::ExpAll && BagCount(p, d.name) > 0) { Toast(g, "You already have the Exp. All."); PlaySfx(Sfx::MenuBack); return; }
    if (p.coins < d.price) { Toast(g, "Not enough coins!"); PlaySfx(Sfx::MenuBack); return; }
    p.coins -= d.price;
    BagAdd(p, d.name);
    if (d.fx == ItemFx::ExpAll) {
        SaveOrWarn(g);
        PlaySfx(Sfx::Heal);
        Toast(g, "Bought the Exp. All! In co-op, your partner shares the XP of your solo wild battles.");
        return;
    }
    SaveOrWarn(g);
    PlaySfx(Sfx::Heal);
    Toast(g, IsBall(d) ? "Bought a " + d.name + "!"
             : IsStone(d) ? "Bought a " + d.name + "! Use it on a Pokemon in Party & Box."
             : IsTM(d) ? "Bought " + d.name + "! Teach it in Party & Box (Use a TM)."
                          : "Bought " + d.name + "! Give it to a Pokemon in Party & Box.");
}

namespace {
// The shop's Move Relearner tab.
void DrawRelearner(const Game& g) {
    const Profile& p = g.profile;
    auto party = TabList(p, 0);
    DrawPanel(20, 56, 330, 276);
    GDrawText("MOVE RELEARNER", 30, 62, 10, Color{120, 190, 255, 255});
    GDrawText(TextFormat("Remembers a forgotten move: %d coins each.", RELEARN_PRICE), 30, 76, 10, UI_DIM);
    if (g.relearnMon < 0) {
        for (int i = 0; i < (int)party.size(); i++) {
            const OwnedMon& m = p.mons[party[i]];
            int y = 94 + i * 38;
            bool sel = g.shopSel == i;
            DrawPanel(28, y, 314, 34, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
            int si = MonSpecies(m);
            if (si >= 0) DrawSpeciesFit(si, VIEW_FRONT, 46, y + 32, 28);
            GDrawText(m.species.c_str(), 66, y + 6, 10, sel ? UI_GOLD : UI_TEXT);
            GDrawText(TextFormat("Lv%d", m.level), 300, y + 6, 10, UI_GOLD);
            int k = (int)RelearnMoves(m).size();
            GDrawText(k ? TextFormat("%d move%s to remember", k, k == 1 ? "" : "s") : "nothing to remember", 66, y + 19, 10, k ? Color{140, 230, 150, 255} : UI_DIM);
        }
        DrawPanel(362, 56, 258, 140);
        GDrawText("Pick a party Pokemon to see the", 372, 66, 10, UI_TEXT);
        GDrawText("moves it can remember:", 372, 80, 10, UI_TEXT);
        GDrawText("- level-up moves up to its level", 372, 98, 10, UI_DIM);
        GDrawText("- moves learned when it evolved", 372, 112, 10, UI_DIM);
        GDrawText("- moves it forgot, like old TM moves", 372, 126, 10, UI_DIM);
        GDrawText("You only pay if it learns the move.", 372, 150, 10, Color{140, 220, 255, 255});
        GDrawText("Left/Right tab   Up/Down choose   Enter pick   Esc back", 20, 340, 10, UI_DIM);
        return;
    }
    if (g.relearnMon >= (int)p.mons.size()) return;
    const OwnedMon& m = p.mons[g.relearnMon];
    int si = MonSpecies(m);
    auto mv = RelearnMoves(m);
    int rows = 11, n = (int)mv.size();
    int first = std::max(0, std::min(g.shopSel - rows / 2, n - rows));
    GDrawText(TextFormat("%s can remember:", m.species.c_str()), 30, 90, 10, UI_GOLD);
    for (int i = first; i < std::min(n, first + rows); i++) {
        int y = 106 + (i - first) * 20;
        bool sel = g.shopSel == i;
        DrawPanel(28, y, 314, 18, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
        const MoveDef* d = LookupMove(mv[i].second, si);
        if (d) DrawRectangle(34, y + 5, 8, 8, TypeColor(d->type));
        GDrawText(mv[i].second.c_str(), 48, y + 4, 10, sel ? UI_GOLD : UI_TEXT);
        const char* tag = mv[i].first < 0 ? "forgotten" : mv[i].first == 0 ? "on evolving" : TextFormat("Lv%d", mv[i].first);
        GDrawText(tag, 334 - TextW(tag, 10), y + 4, 10, UI_DIM);
    }
    DrawPanel(362, 56, 258, 140);
    if (si >= 0) DrawSpeciesFit(si, VIEW_FRONT, 400, 120, 56);
    GDrawText(m.species.c_str(), 440, 66, 10, UI_GOLD);
    GDrawText("Knows:", 440, 80, 10, UI_DIM);
    for (int k = 0; k < (int)m.moves.size(); k++) GDrawText(m.moves[k].c_str(), 440, 94 + k * 12, 10, UI_TEXT);
    if (n > 0) {
        const MoveDef* d = LookupMove(mv[g.shopSel].second, si);
        if (d) {
            DrawPanel(362, 204, 258, 46);
            DrawMoveLine(*d, 368, 210, 246, false);
            DrawWrapped(DescribeMove(*d).c_str(), 378, 222, 236, 10, Color{150, 190, 220, 255});
        }
    }
    GDrawText(TextFormat("Price: %d coins", RELEARN_PRICE), 372, 260, 10, p.coins >= RELEARN_PRICE ? Color{255, 200, 60, 255} : Color{255, 120, 100, 255});
    GDrawText("Up/Down choose   Enter teach   Esc pick another Pokemon", 20, 340, 10, UI_DIM);
}
} // namespace

void DrawShop(const Game& g) {
    ClearBackground(Color{26, 30, 44, 255});
    for (int y = 0; y < VH; y += 24) DrawRectangle(0, y, VW, 12, Color{30, 35, 50, 255});
    DrawTextCentered("POKE MART", VW / 2, 8, 20, Color{120, 190, 255, 255});
    const Profile& p = g.profile;
    DrawCoins(p.coins, 16, 12);
    GDrawText(TextFormat("Badges: %d", BadgeCount(g)), VW - 80, 13, 10, UI_DIM);
    if (g.coop) GDrawText(TextFormat("Shopping: %s", p.name.c_str()), 240, 13, 10, g.coopActive == 1 ? Color{120, 230, 255, 255} : UI_GOLD);
    const char* tabs[6] = {"Poke Balls", "Held items", "Evo stones", "TMs", "Move Relearner", "Game Corner"};
    for (int i = 0; i < 6; i++) {
        int x = 20 + i * 100;
        bool sel = g.shopTab == i;
        Color dim = i == 5 ? (GameCornerOpen(g) ? Color{255, 150, 220, 255} : Color{110, 90, 110, 255}) : UI_DIM;
        DrawPanel(x, 32, 96, 18, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
        DrawTextCentered(tabs[i], x + 48, 36, 10, sel ? UI_GOLD : dim, false);
    }
    if (g.shopTab == 4) { DrawRelearner(g); DrawToast(g); return; }
    if (g.shopTab == 5) { DrawGameCorner(g); DrawToast(g); return; }
    auto list = ShopList(g.shopTab);
    int n = (int)list.size();
    int rows = 13;
    int first = std::max(0, std::min(g.shopSel - rows / 2, n - rows));
    for (int i = first; i < std::min(n, first + rows); i++) {
        const ItemDef& d = *list[i];
        int y = 56 + (i - first) * 21;
        bool sel = g.shopSel == i;
        bool locked = BadgeCount(g) < d.badges;
        DrawPanel(20, y, 330, 19, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
        if (IsBall(d)) {
            DrawCircle(32, y + 9, 5, UI_INK);
            Color top = d.name == "Master Ball" ? Color{150, 80, 200, 255} : (d.name == "Ultra Ball" ? Color{60, 60, 60, 255} : (d.name == "Great Ball" ? Color{70, 120, 230, 255} : Color{230, 60, 60, 255}));
            DrawCircleSector({32, (float)y + 9}, 4, 180, 360, 12, top);
            DrawCircleSector({32, (float)y + 9}, 4, 0, 180, 12, WHITE);
        } else if (IsStone(d)) {
            Color c = TypeColor(d.type);
            DrawPoly({32, (float)y + 9}, 6, 5.5f, 0, ColorBrightness(c, -0.4f));
            DrawPoly({32, (float)y + 9}, 6, 4.2f, 0, c);
            DrawRectangle(30, y + 6, 2, 2, Fade(WHITE, 0.8f));
        } else if (IsTM(d)) {   // a disc in the move's type colour
            DrawCircle(32, y + 9, 6, ColorBrightness(TypeColor(d.type), -0.35f));
            DrawCircle(32, y + 9, 4.5f, TypeColor(d.type));
            DrawCircle(32, y + 9, 1.5f, UI_INK);
        } else if (d.fx == ItemFx::TypeBoost) {
            DrawRectangle(28, y + 5, 9, 9, TypeColor(d.type));
        } else {
            DrawRectangle(28, y + 5, 9, 9, Color{140, 220, 255, 255});
        }
        GDrawText(d.name.c_str(), 44, y + 5, 10, locked ? UI_DIM : (sel ? UI_GOLD : UI_TEXT));
        if (locked) GDrawText("locked", 270, y + 5, 10, Color{150, 110, 110, 255});
        else {
            const char* pr = TextFormat("%d", d.price);
            GDrawText(pr, 340 - TextW(pr, 10), y + 5, 10, d.price <= p.coins ? Color{255, 200, 60, 255} : Color{255, 120, 100, 255});
        }
        int have = BagCount(p, d.name);
        if (have > 0) GDrawText(TextFormat("x%d", have), 220, y + 5, 10, UI_DIM);
    }
    if (n > 0) {
        const ItemDef& d = *list[g.shopSel];
        DrawPanel(362, 56, 258, 140);
        DrawTextShadow(d.name.c_str(), 372, 64, 20, UI_TEXT);
        const MoveDef* tmMove = IsTM(d) ? FindMove(d.move) : nullptr;
        if (tmMove) {
            DrawMoveLine(*tmMove, 370, 112, 244, false);
            DrawWrapped(DescribeMove(*tmMove).c_str(), 380, 124, 232, 10, Color{150, 190, 220, 255});
        }
        // wrap the description
        std::string desc = tmMove ? std::string("Used up when taught.") : d.desc, line, word;
        int ly = 92;
        for (size_t i = 0; i <= desc.size(); i++) {
            if (i == desc.size() || desc[i] == ' ') {
                std::string t2 = line.empty() ? word : line + " " + word;
                if (TextW(t2.c_str(), 10) > 236) { GDrawText(line.c_str(), 372, ly, 10, UI_TEXT); ly += 12; line = word; }
                else line = t2;
                word.clear();
            } else word += desc[i];
        }
        if (!line.empty()) GDrawText(line.c_str(), 372, ly, 10, UI_TEXT);
        GDrawText(TextFormat("Price: %d coins", d.price), 372, 150, 10, Color{255, 200, 60, 255});
        GDrawText(TextFormat("In your bag: %d", BagCount(p, d.name)), 372, 164, 10, UI_DIM);
        if (BadgeCount(g) < d.badges) GDrawText(BadgeNeedText(d.badges), 372, 178, 10, Color{255, 130, 100, 255});
        int held = 0;
        for (const auto& m : p.mons) held += m.item == d.name;
        if (held) GDrawText(TextFormat("Held by %d of your Pokemon", held), 372, 178, 10, Color{140, 220, 255, 255});
    }
    if (n > 0 && IsTM(*list[g.shopSel])) {   // who in the party can learn it
        const ItemDef& d = *list[g.shopSel];
        DrawPanel(362, 204, 258, 128);
        GDrawText("Your party:", 372, 210, 10, UI_DIM);
        auto party = TabList(p, 0);
        for (int k = 0; k < (int)party.size() && k < 6; k++) {
            const OwnedMon& m = p.mons[party[k]];
            int x = 372 + (k % 2) * 124, y = 226 + (k / 2) * 34;
            int si = MonSpecies(m);
            bool knows = KnowsMove(m, d.move), able = CanLearnTM(m, d.move);
            if (si >= 0) DrawSpeciesFit(si, VIEW_FRONT, x + 12, y + 26, 24, able ? WHITE : Color{90, 90, 100, 255});
            GDrawText(m.species.c_str(), x + 28, y + 4, 10, able ? UI_TEXT : UI_DIM);
            GDrawText(knows ? "LEARNED" : able ? "ABLE" : "NOT ABLE", x + 28, y + 16, 10,
                     knows ? Color{140, 220, 255, 255} : able ? Color{140, 230, 150, 255} : Color{150, 110, 110, 255});
        }
        GDrawText("Left/Right tab   Up/Down choose   Enter buy   Esc back", 20, 340, 10, UI_DIM);
        DrawToast(g);
        return;
    }
    DrawPanel(362, 204, 258, 72);
    GDrawText("Each Pokemon can hold one item.", 372, 212, 10, UI_DIM);
    GDrawText("Equip items and use stones in Party & Box.", 372, 226, 10, UI_DIM);
    GDrawText("More items unlock with each badge.", 372, 240, 10, UI_DIM);
    GDrawText("Balls are used up when thrown.", 372, 254, 10, UI_DIM);
    GDrawText("Left/Right tab   Up/Down choose   Enter buy   Esc back", 20, 340, 10, UI_DIM);
    DrawToast(g);
}

// ------------------------------------------------------------------ co-op story: choosing profiles

namespace {
const Color kP2Col{120, 230, 255, 255};
Color PlayerCol(int p) { return p == 0 ? UI_GOLD : kP2Col; }

void OpenFor(Game& g, int who) {  // load one player's profile for Shop / Healing / Party & Box
    g.coopActive = who;
    g.profile = g.coopP[who];
}
} // namespace

void CoopOpenFor(Game& g, int who) { OpenFor(g, who); }

void StartCoopFlow(Game& g) {
    g.profiles = ListProfiles();
    int n = (int)g.profiles.size();
    g.coopPick[0] = 0;
    g.coopPick[1] = std::min(1, std::max(0, n - 1));
    g.coopReady[0] = g.coopReady[1] = false;
    g.coop = false;
    g.coopActive = -1;
    g.screen = Screen::CoopSelect;
}

void UpdateCoopSelect(Game& g) {
    int n = (int)g.profiles.size();
    int N = n + 1;   // the profiles, then "+ New trainer"
    if (IsKeyPressed(KEY_ESCAPE) && !g.coopReady[0] && !g.coopReady[1]) { PlaySfx(Sfx::MenuBack); GoTitle(g); return; }
    for (int p = 0; p < 2; p++) {
        PlayerInput in = ReadInput(p, true);
        g.coopPick[p] = std::max(0, std::min(g.coopPick[p], n));
        if (!g.coopReady[p]) {
            if (in.left || in.up) { g.coopPick[p] = (g.coopPick[p] + N - 1) % N; PlaySfx(Sfx::MenuMove); }
            if (in.right || in.down) { g.coopPick[p] = (g.coopPick[p] + 1) % N; PlaySfx(Sfx::MenuMove); }
            if (in.confirm) {
                bool isNew = g.coopPick[p] >= n;
                if (!isNew && g.coopReady[1 - p] && g.coopPick[1 - p] == g.coopPick[p]) { Toast(g, "Each player needs a different trainer."); PlaySfx(Sfx::MenuBack); }
                else { g.coopReady[p] = true; PlaySfx(Sfx::MenuOk); }
            }
            if (in.back && p == 0) { PlaySfx(Sfx::MenuBack); GoTitle(g); return; }
        } else if (in.back) {
            g.coopReady[p] = false;
            PlaySfx(Sfx::MenuBack);
        }
    }
    if (g.coopReady[0] && g.coopReady[1]) {
        bool anyNew = false;
        for (int p = 0; p < 2; p++) {
            g.coopNew[p] = g.coopPick[p] >= n;
            anyNew |= g.coopNew[p];
            g.coopName[p].clear();
            if (!g.coopNew[p]) LoadProfile(g.profiles[g.coopPick[p]], g.coopP[p]);
        }
        g.coopReady[0] = g.coopReady[1] = false;
        CoopSave had;
        if (anyNew || !LoadCoop(g.coopP[0].name, g.coopP[1].name, had)) { BeginRegionChoose(g, 2); return; }   // a new journey: where it starts
        g.journeyRegion = had.regionOrder.empty() ? 0 : had.regionOrder[0];
        CoopContinueSetup(g);
    }
}

// After the region is chosen (or for a journey that already has one): names, starters, then the hub.
void CoopContinueSetup(Game& g) {
    if (g.coopNew[0] || g.coopNew[1]) {   // type the new trainers' names first
        g.coopNameWho = g.coopNew[0] ? 0 : 1;
        while (GetCharPressed()) {}
        g.screen = Screen::CoopSetup;
        return;
    }
    if (g.coopP[0].mons.empty() || g.coopP[1].mons.empty()) {
        g.coopStarterSel[0] = 0;
        g.coopStarterSel[1] = std::min(1, (int)RegionStarters(g.journeyRegion).size() - 1);
        g.screen = Screen::CoopStarter;
        return;
    }
    CoopBeginJourney(g);
}

void DrawCoopSelect(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    DrawTextCentered("CO-OP STORY", VW / 2, 12, 30, UI_GOLD);
    DrawTextCentered("Two trainers, one journey. Badges and story progress are shared;", VW / 2, 48, 10, UI_TEXT, false);
    DrawTextCentered("XP, coins, items and Pokemon stay with each trainer.", VW / 2, 60, 10, UI_TEXT, false);
    int n = (int)g.profiles.size();
    for (int p = 0; p < 2; p++) {
        int x = p == 0 ? 20 : 330, w = 290, y = 80;
        Color pc = PlayerCol(p);
        DrawPanel(x, y, w, 200, UI_PANEL, g.coopReady[p] ? pc : UI_BORDER);
        DrawRectangle(x, y, w, 2, pc);
        GDrawText(p == 0 ? "PLAYER 1" : "PLAYER 2", x + 8, y + 8, 10, pc);
        if (g.coopPick[p] >= n) {
            DrawTextShadow("< + New trainer >", x + 8, y + 24, 20, Color{140, 230, 150, 255});
            GDrawText("Start fresh: type a name, then pick", x + 8, y + 56, 10, UI_TEXT);
            GDrawText("your starter together with your partner.", x + 8, y + 70, 10, UI_TEXT);
        } else {
            Profile pr;
            LoadProfile(g.profiles[g.coopPick[p]], pr);
            DrawTextShadow(TextFormat("< %s >", pr.name.c_str()), x + 8, y + 24, 20, UI_TEXT);
            GDrawText(TextFormat("%d Pokemon   %d coins   solo stage %d", (int)pr.mons.size(), pr.coins, pr.storyStage), x + 8, y + 50, 10, UI_DIM);
            int k = 0;
            for (const auto& m : pr.mons) {
                if (m.boxed) continue;
                int idx = MonSpecies(m);
                int cx = x + 26 + k * 44;
                if (idx >= 0) DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, cx, y + 104, 36);
                DrawTextCentered(TextFormat("Lv%d", m.level), cx, y + 108, 10, UI_GOLD, false);
                if (++k >= 6) break;
            }
            if (pr.mons.empty()) GDrawText("No Pokemon yet - picks a starter first", x + 8, y + 80, 10, Color{140, 230, 150, 255});
        }
        const char* st = g.coopReady[p] ? "READY!" : TextFormat("Left/Right choose   %s confirm", ConfirmLabel(p, true));
        GDrawText(st, x + 8, y + 184, 10, g.coopReady[p] ? pc : UI_DIM);
    }
    CoopSave cs;
    bool bothOld = g.coopPick[0] < n && g.coopPick[1] < n;
    if (bothOld && g.coopPick[0] != g.coopPick[1] && LoadCoop(g.profiles[g.coopPick[0]], g.profiles[g.coopPick[1]], cs))
        DrawTextCentered(TextFormat("These two have a co-op save: stage %d, %d badge%s", cs.storyStage + 1, (int)cs.badges.size(), cs.badges.size() == 1 ? "" : "s"),
                         VW / 2, 292, 10, Color{140, 230, 150, 255}, false);
    else if (!bothOld || g.coopPick[0] != g.coopPick[1])
        DrawTextCentered("A new co-op journey for these two (starts at stage 1).", VW / 2, 292, 10, UI_DIM, false);
    else
        DrawTextCentered("Pick two different trainers.", VW / 2, 292, 10, Color{255, 130, 100, 255}, false);
    DrawTextCentered("Each player uses their own controls.   Esc: back", VW / 2, 340, 10, UI_DIM, false);
    DrawToast(g);
}

void CoopBeginJourney(Game& g) {
    bool existed = LoadCoop(g.coopP[0].name, g.coopP[1].name, g.coopSave);
    g.coopSave.p[0] = g.coopP[0].name;
    g.coopSave.p[1] = g.coopP[1].name;
    if (g.coopSave.regionOrder.empty()) g.coopSave.regionOrder = {g.journeyRegion};   // a new journey starts where they chose
    if (!existed) SaveCoop(g.coopSave);
    g.coop = true;
    g.coopActive = -1;
    g.coopWho = 0;
    g.rareSpecies = -1;
    Toast(g, existed ? "Welcome back! Your co-op journey continues." : "A new co-op journey begins!");
    EnterStoryHub(g);
}

bool CoopNeedsStarter(const Game& g) { return g.coopP[0].mons.empty() || g.coopP[1].mons.empty(); }

// ------------------------------------------------------------------ co-op story: hub

void UpdateCoopHub(Game& g) {
    if (UpdateStoryCard(g)) return;
    PlayerInput in = MenuInput();
    if (g.showBoard) {   // the leaderboard: any key closes it
        if (in.confirm || in.back) { g.showBoard = false; PlaySfx(Sfx::MenuBack); }
        return;
    }
    const int N = 13;
    if (HubNav(g, in, N, true)) return;
    bool online = g.on.active;
    if (online) g.coopWho = g.on.me;  // online: your own Shop / Healing Center / Party & Box / Bag / Pokedex
    else if ((in.left || in.right) && g.hubSel >= 3 && g.hubSel <= 7) { g.coopWho = 1 - g.coopWho; PlaySfx(Sfx::MenuMove); }
    if (g.hubSel == 0 && (in.left || in.right) && KantoDone(g) && !StoryDone(g)) { g.hubTower = !g.hubTower; PlaySfx(Sfx::MenuMove); return; }
    if (g.hubSel == 1 && (in.left || in.right)) {   // where to train (online: the host picks)
        if (online && g.on.me != 0) { Toast(g, "The host picks the training area."); return; }
        CycleTrainArea(g, in.left ? -1 : 1);
        if (online) OnlineSendCoop(g);
        else SaveCoop(g.coopSave);
        return;
    }
    auto leave = [&]() {
        if (online) { OnlineLeave(g, "Left the online game. Your progress is saved."); return; }
        SaveWho(g, 0);
        SaveWho(g, 1);
        SaveCoop(g.coopSave);
        GoTitle(g);
    };
    if (in.back) { leave(); return; }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    int stages = (int)Story().stages.size();
    if (online && (g.hubSel == 0 || g.hubSel == 1 || g.hubSel == 8) && OnlineWaiting(g)) return;
    bool tower = g.hubSel == 0 && (StoryDone(g) || (KantoDone(g) && g.hubTower));
    if (g.hubSel == 0 && !tower && CanChooseRegion(g)) {   // a region is done: player 1 (online: the host) picks the next
        if (online && g.on.me != 0) { Toast(g, g.coopP[0].name + " picks the next region."); return; }
        BeginRegionChoose(g, 3);
        return;
    }
    if (g.hubSel == 0 && !tower && NeedsRegionStarter(g) >= 0) {   // off to a new region: the professor's starters (host first, then player 2)
        if (online) OnlineSetReady(g, 0);
        BeginRegionStarter(g);
        return;
    }
    if (online && (g.hubSel == 0 || g.hubSel == 1)) {
        // Online: say what you want to do; it starts when your partner picks the same.
        int kind = g.hubSel == 0 ? (tower ? KIND_TOWER : 1) : 2;
        if (!tower && HealthyCount(Prof(g, g.on.me)) == 0) {
            Toast(g, "Your party has all fainted! Visit the Healing Center first.");
            OpenFor(g, g.on.me);
            OpenHealingCenter(g);
            return;
        }
        OnlineSetReady(g, g.on.myReady == kind ? 0 : kind);
        return;
    }
    if ((g.hubSel == 0 && !tower) || g.hubSel == 1) {
        for (int p = 0; p < 2; p++) {
            if (HealthyCount(g.coopP[p]) == 0) {
                Toast(g, g.coopP[p].name + "'s party has all fainted! Visit the Healing Center first.");
                g.coopWho = p;
                OpenFor(g, p);
                OpenHealingCenter(g);
                return;
            }
        }
    }
    switch (g.hubSel) {
        case 0:
            if (tower) { BeginTowerFloor(g, g.coopSave.towerFloor + 1, TowerSeedFor(g)); break; }
            g.stageIdx = std::min(g.coopSave.storyStage, stages - 1);
            g.mode = Mode::CoopStory;
            g.rareBattle = false;
            g.screen = Screen::StageIntro;
            break;
        case 1:
            StartCoopTrainingBattle(g);
            break;
        case 2:
            g.mapSel = std::min(g.coopSave.storyStage, stages - 1);
            if (online && IsRematchKind(g.on.partnerReady) && g.on.partnerReady - 3 < stages) g.mapSel = g.on.partnerReady - 3;   // join their rematch
            g.screen = Screen::Map;
            if (online && IsLegendKind(g.on.partnerReady) && g.on.partnerReady - KIND_LEGEND < LEGEND_COUNT && BadgeCount(g) >= 8) {   // join their hunt
                g.legendSel = g.on.partnerReady - KIND_LEGEND;
                OpenLegends(g);
            }
            break;
        case 3:
            OpenFor(g, g.coopWho);
            g.shopSel = 0;
            g.screen = Screen::Shop;
            break;
        case 4:
            OpenFor(g, g.coopWho);
            OpenHealingCenter(g);
            break;
        case 5:
            OpenFor(g, g.coopWho);
            g.myCreSel = 0;
            g.itemPick = false;
            g.actMenu = false;
            g.swapFrom = -1;
            g.stonePick = false;
            g.moveOrder = false;
            g.boxTab = 0;
            g.screen = Screen::MyCreatures;
            break;
        case 6:
            OpenFor(g, g.coopWho);
            OpenBag(g);
            break;
        case 7:
            OpenPokedex(g, g.coopWho);
            break;
        case 8:
            OpenTrade(g);
            break;
        case 9:
            StoryTimeStep(g, true);
            if (online) OnlineSendCoop(g);
            else SaveCoop(g.coopSave);
            break;
        case HUB_GRAPH:
            g.showBoard = true;
            break;
        case 10:
            StartPractice(g);
            break;
        case 11:
            EnterWorld(g);
            break;
        case HUB_GEAR:
            g.settingsSel = 0;
            g.settingsBack = Screen::CoopHub;
            g.screen = Screen::Settings;
            break;
        default:
            leave();
            break;
    }
}

namespace {
void DrawCoopHubBase(const Game& g) {
    DrawMenuBackdrop(Color{20, 18, 32, 255}, (float)GetTime());
    Profile pv = ProgressView(g);
    int stages = (int)Story().stages.size();
    bool done = pv.storyStage >= stages;

    DrawStoryMap(pv, 8, 26, 300, 226, g.t, false);
    DrawHubIcon(0, 8, 4, g.hubSel == HUB_GEAR, g.t);
    DrawHubIcon(1, 29, 4, g.hubSel == HUB_GRAPH, g.t);
    int nx = 54;
    DrawTextShadow(g.coopP[0].name.c_str(), nx, 6, 10, UI_GOLD);
    GDrawText("&", nx + 4 + TextW(g.coopP[0].name.c_str(), 10), 6, 10, UI_DIM);
    DrawTextShadow(g.coopP[1].name.c_str(), nx + 14 + TextW(g.coopP[0].name.c_str(), 10), 6, 10, kP2Col);
    if (g.on.active) GDrawText(TextFormat("ONLINE %dms", net::PingMs()), 226, 6, 10, Color{140, 230, 150, 255});
    else GDrawText("CO-OP", 248, 6, 10, Color{255, 150, 230, 255});
    std::string next = NextLine(g, pv.storyStage);
    GDrawText(next.c_str(), 10, 256, 10, done ? UI_GOLD : UI_TEXT);
    GDrawText(TextFormat("Together: %dW %dL   training wins %d", pv.wins, pv.losses, pv.trainingWins), 10, 270, 10, UI_DIM);
    std::vector<std::string> badges;   // this region's
    int hubRegion = ActiveRegion(pv.storyStage);
    for (const auto& st : Story().stages)
        if (st.IsGym() && st.region == hubRegion) badges.push_back(st.badge);
    for (size_t i = 0; i < badges.size(); i++) DrawBadge(badges[i], 16 + (int)i * 16, 294, HasBadge(pv, badges[i]));
    if (g.on.active) {
        const OnlineSession& o = g.on;
        const std::string& pn = g.coopP[1 - o.me].name;
        auto kind = [](int k) -> std::string {
            if (k == 1) return "a Story battle";
            if (k == KIND_TOWER) return "the Battle Tower";
            if (k == 2) return "Training";
            if (IsLegendKind(k) && k - KIND_LEGEND < LEGEND_COUNT) return std::string("to find ") + Legend(k - KIND_LEGEND).species;
            int st = k - 3;
            return st >= 0 && st < (int)Story().stages.size() ? "a rematch with " + Story().stages[st].trainer : std::string("a battle");
        };
        std::string mine = o.myReady ? "You want " + kind(o.myReady) + " - waiting for " + pn : "Pick a battle - it starts when you both pick it.";
        std::string theirs = o.partnerReady ? pn + " wants " + kind(o.partnerReady) + (IsLegendKind(o.partnerReady) ? " (open the Map's Legendary tab)" : IsRematchKind(o.partnerReady) ? " (open the Map)" : "!")
                                            : pn + " is " + (o.partnerActivity.empty() ? "in the hub" : o.partnerActivity) + ".";
        if (o.reconnecting) {
            mine = "Waiting for " + pn + " to come back (Esc leaves the online game).";
            theirs = o.me == 0 ? "They rejoin with your same code. Restarted? Tab: type their new code." : "Host restarted? Press Tab and type their new code.";
        }
        GDrawText(mine.c_str(), 10, 310, 10, o.myReady ? UI_GOLD : UI_DIM);
        GDrawText(theirs.c_str(), 10, 322, 10, o.partnerReady ? Color{140, 230, 150, 255} : UI_DIM);
    } else {
        GDrawText("Badges and story progress are shared.", 10, 310, 10, UI_DIM);
        GDrawText("XP, coins and Pokemon go to each player.", 10, 322, 10, UI_DIM);
    }

    // Menu
    int rx = 318, rw = 314;
    bool rare = NextTrainingIsRare(g.coopSave.trainingWins);
    int every = std::max(1, Story().rareEvery);
    int left = every - (g.coopSave.trainingWins % every) - 1;
    const std::string& wn = g.coopP[g.coopWho].name;
    bool partnerTrading = g.on.active && g.on.partnerInTrade;
    std::string items[13] = {
        CoopFirstLabel(g, done || (KantoDone(g) && g.hubTower), pv.storyStage),
        (rare ? "RARE 2v2 in " : "Training 2v2: ") + TrainAreaName(g) + (rare ? "!" : TextFormat("  (%d to a rare)", left)),
        MapHubLabel(g),
        GameCornerOpen(g) ? "Shop & Game Corner" : "Shop",
        "Healing Center",
        "Party & Box",
        "Bag",
        "Pokedex",
        partnerTrading ? "Trade (your partner is waiting!)" : "Trade Pokemon",
        IsNight(g.coopSave.clock) ? "Wait until morning" : "Wait until night",
        "Practice arena",
        "Walk around (test): " + WorldPlaceName(g),
        "Save and quit",
    };
    DrawTimePanel(g.coopSave.clock, 304, 30);
    bool partnerHere = g.on.active && g.on.partnerCursor >= 0 && (g.on.partnerActivity.empty() || g.on.partnerActivity == "in the hub");
    for (int i = 0; i < 13; i++) {
        int y = 6 + i * 12;
        bool sel = g.hubSel == i;
        DrawPanel(rx, y, rw, 11, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
        Color c = sel ? UI_GOLD : UI_TEXT;
        if (i == 1 && rare) c = Color{255, 150, 230, 255};
        if (i == 8 && partnerTrading) c = Color{140, 230, 150, 255};
        if (i == 2 && items[2].size() > 3) c = sel ? UI_GOLD : Color{255, 200, 120, 255};   // legendaries waiting
        GDrawText(items[i].c_str(), rx + 8, y + 1, 10, c);
        if (g.on.active && (i == 0 || i == 1 || i == 2)) {   // ready count: the battle starts at 2/2 (Map: a legendary hunt)
            int kind = i == 0 ? (done ? KIND_TOWER : 1) : 2;
            bool r[2];
            if (i == 2) {
                auto lg = [](int k) { return IsLegendKind(k); };
                r[g.on.me] = lg(g.on.myReady);
                r[1 - g.on.me] = lg(g.on.partnerReady);
            } else {
                r[g.on.me] = g.on.myReady == kind;
                r[1 - g.on.me] = g.on.partnerReady == kind;
            }
            if (r[0] || r[1]) DrawReadyCount(rx + rw - 2, y, r[0], r[1]);
        }
        if (partnerHere && g.on.partnerCursor == i) {   // where your partner's cursor is
            Color pc = PlayerCol(1 - g.on.me);
            DrawRectangleLines(rx + 1, y + 1, rw - 2, 10, Fade(pc, 0.9f));
            DrawRectangle(rx + 1, y + 1, 3, 10, pc);
            float bob = std::sin(g.t * 6) * 1.5f;
            DrawTriangle({rx - 8 + bob, (float)y + 1}, {rx - 8 + bob, (float)y + 10}, {rx - 2 + bob, (float)y + 5.5f}, pc);
        }
        if (i >= 3 && i <= 7) {
            std::string tag = (sel && !g.on.active) ? "< " + wn + " >" : wn;
            GDrawText(tag.c_str(), rx + rw - 8 - TextW(tag.c_str(), 10), y + 1, 10, PlayerCol(g.coopWho));
        }
    }

    // Both parties
    for (int p = 0; p < 2; p++) {
        const Profile& P = g.coopP[p];
        int x = rx + p * (rw / 2 + 1), w = rw / 2 - 1, y = 178;
        DrawPanel(x, y, w, 164, UI_PANEL, PlayerCol(p));
        GDrawText(P.name.c_str(), x + 6, y + 5, 10, PlayerCol(p));
        DrawCoins(P.coins, x + w - 44, y + 4);
        int k = 0;
        for (const auto& m : P.mons) {
            if (m.boxed) continue;
            int idx = MonSpecies(m);
            int yy = y + 20 + k * 24;
            if (idx >= 0) DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, x + 14, yy + 21, 20, m.hp > 0 ? WHITE : Color{90, 90, 100, 255});
            if (m.shiny) DrawShinyStar((float)x + 6, (float)yy + 5, 3);
            GDrawText(m.species.c_str(), x + 28, yy + 3, 10, m.hp > 0 ? UI_TEXT : UI_DIM);
            GDrawText(TextFormat("Lv%d", m.level), x + w - 30, yy + 3, 10, UI_GOLD);
            DrawHpBar(x + 28, yy + 15, w - 36, 3, m.hp);
            if (++k >= PARTY_SIZE) break;
        }
        int boxed = (int)P.mons.size() - PartyCount(P);
        if (boxed > 0) GDrawText(TextFormat("+%d", boxed), x + 10 + TextW(P.name.c_str(), 10), y + 5, 10, UI_DIM);   // in the Box
    }
    GDrawText(g.on.active ? "Enter choose   Esc leave the online game" : "Enter choose   Left/Right whose   Esc save and leave", 10, 346, 10, UI_DIM);
    DrawToast(g);
}
} // namespace

// The co-op leaderboard: who's carrying the run?
void DrawLeaderboard(const Game& g) {
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.75f));
    int x = 110, y = 40, w = 420, h = 280;
    DrawPanel(x, y, w, h, UI_PANEL, UI_GOLD);
    DrawTextCentered("CO-OP LEADERBOARD", VW / 2, y + 8, 20, UI_GOLD);
    DrawTextCentered("The whole story run so far", VW / 2, y + 32, 10, UI_DIM, false);
    const CoopSave& c = g.coopSave;
    int cx[2] = {x + 230, x + 340};
    for (int p = 0; p < 2; p++) DrawTextCentered(g.coopP[p].name.c_str(), cx[p], y + 52, 10, PlayerCol(p), false);
    struct Row { const char* label; int v[2]; bool lowWins; };
    Row rows[] = {
        {"Knockouts", {c.run[0].kos, c.run[1].kos}, false},
        {"Damage dealt", {c.run[0].damage, c.run[1].damage}, false},
        {"Pokemon caught", {c.run[0].caught, c.run[1].caught}, false},
        {"MVP battles (most damage)", {c.run[0].mvp, c.run[1].mvp}, false},
        {"Battles fought", {c.run[0].battles, c.run[1].battles}, false},
        {"Pokemon fainted (fewer wins)", {c.run[0].faints, c.run[1].faints}, true},
    };
    int score[2] = {0, 0};
    for (int r = 0; r < 6; r++) {
        int ry = y + 72 + r * 26;
        DrawRectangle(x + 10, ry - 4, w - 20, 22, r % 2 ? Fade(UI_PANEL_LIGHT, 0.5f) : Fade(UI_INK, 0.3f));
        GDrawText(rows[r].label, x + 20, ry + 2, 10, UI_TEXT);
        int lead = rows[r].v[0] == rows[r].v[1] ? -1 : ((rows[r].v[0] > rows[r].v[1]) != rows[r].lowWins ? 0 : 1);
        if (lead >= 0) score[lead]++;
        for (int p = 0; p < 2; p++) {
            const char* t = TextFormat("%d", rows[r].v[p]);
            DrawTextCentered(t, cx[p], ry + 2, 10, lead == p ? UI_GOLD : UI_TEXT, false);
            if (lead == p) {   // a little crown for whoever leads this row
                int kx = cx[p] + TextW(t, 10) / 2 + 6;
                DrawTriangle({(float)kx, (float)ry + 9}, {(float)kx + 6, (float)ry + 9}, {(float)kx + 3, (float)ry + 2}, UI_GOLD);
                DrawTriangle({(float)kx + 3, (float)ry + 9}, {(float)kx + 8, (float)ry + 9}, {(float)kx + 5.5f, (float)ry + 1}, UI_GOLD);
                DrawTriangle({(float)kx + 5, (float)ry + 9}, {(float)kx + 11, (float)ry + 9}, {(float)kx + 8, (float)ry + 2}, UI_GOLD);
                DrawRectangle(kx, ry + 9, 11, 2, UI_GOLD);
            }
        }
    }
    const char* verdict = score[0] == score[1] ? "Neck and neck!"
                        : TextFormat("%s leads %d - %d", g.coopP[score[0] > score[1] ? 0 : 1].name.c_str(), std::max(score[0], score[1]), std::min(score[0], score[1]));
    DrawTextCentered(verdict, VW / 2, y + h - 40, 20, score[0] == score[1] ? UI_TEXT : PlayerCol(score[0] > score[1] ? 0 : 1));
    DrawTextCentered("Enter / Esc: close", VW / 2, y + h - 14, 10, UI_DIM, false);
}

void DrawCoopHub(const Game& g) {
    DrawCoopHubBase(g);
    if (g.showBoard) DrawLeaderboard(g);
    DrawStoryCard(g);
}

// ------------------------------------------------------------------ Hall of Fame

namespace {
struct HofEntry { std::string partner, region; std::vector<std::pair<std::string, int>> mons; };
HofEntry ParseHof(const std::string& e) {
    HofEntry h;
    size_t bar = e.find('|');
    h.partner = bar == std::string::npos ? std::string() : e.substr(0, bar);
    std::string rest = bar == std::string::npos ? e : e.substr(bar + 1);
    size_t bar2 = rest.find('|');   // "|Johto" at the end: that region's League
    if (bar2 != std::string::npos) { h.region = rest.substr(bar2 + 1); rest = rest.substr(0, bar2); }
    size_t pos = 0;
    while (pos < rest.size()) {
        size_t c = rest.find(',', pos);
        std::string part = rest.substr(pos, c == std::string::npos ? std::string::npos : c - pos);
        size_t colon = part.rfind(':');
        if (colon != std::string::npos) h.mons.push_back({part.substr(0, colon), std::atoi(part.c_str() + colon + 1)});
        if (c == std::string::npos) break;
        pos = c + 1;
    }
    return h;
}
int HofCount(const Game& g) {
    int n = (int)g.profile.hof.size();
    if (g.coop) n = (int)std::max(g.coopP[0].hof.size(), g.coopP[1].hof.size());
    return n;
}
} // namespace

void UpdateHallOfFame(Game& g) {
    PlayerInput in = MenuInput();
    g.hofT += GetFrameTime();
    int n = HofCount(g);
    if (n > 1 && (in.left || in.right)) {
        g.hofSel = std::max(0, std::min(n - 1, g.hofSel + (in.left ? 1 : -1)));   // left = older
        g.hofT = 0.6f;
        PlaySfx(Sfx::MenuMove);
    }
    if (in.confirm || in.back) {
        PlaySfx(Sfx::MenuOk);
        if (g.hofBack == Screen::StageIntro) g.screen = Screen::StageIntro;
        else EnterStoryHub(g);
    }
}

void DrawHallOfFame(const Game& g) {
    float t = g.t;
    for (int y = 0; y < VH; y++) {   // night sky fading to gold at the bottom
        float u = y / (float)VH;
        DrawRectangle(0, y, VW, 1, Color{(unsigned char)(16 + u * 70), (unsigned char)(12 + u * 46), (unsigned char)(40 - u * 10), 255});
    }
    BeginBlendMode(BLEND_ADDITIVE);
    for (int k = 0; k < 9; k++) {   // slow light rays
        float a = -1.2f + k * 0.3f + std::sin(t * 0.3f + k) * 0.05f;
        Vector2 src{VW / 2.0f, VH + 40.0f};
        Vector2 p1{src.x + std::sin(a - 0.05f) * 700, src.y - std::cos(a - 0.05f) * 700}, p2{src.x + std::sin(a + 0.05f) * 700, src.y - std::cos(a + 0.05f) * 700};
        DrawTriangle(src, p2, p1, Fade(Color{255, 220, 140, 255}, 0.05f));
        DrawTriangle(src, p1, p2, Fade(Color{255, 220, 140, 255}, 0.05f));
    }
    EndBlendMode();
    for (int i = 0; i < 90; i++) {   // confetti
        float x = std::fmod(i * 73.3f + std::sin(t + i) * 12, (float)VW);
        float y = std::fmod(i * 41.7f + t * (30 + (i % 7) * 6), (float)VH);
        Color c = i % 4 == 0 ? UI_GOLD : i % 4 == 1 ? Color{240, 90, 90, 255} : i % 4 == 2 ? Color{110, 180, 255, 255} : WHITE;
        DrawRectangle((int)x, (int)y, 2 + (i % 2), 2, Fade(c, 0.8f));
    }
    float pulse = 1.0f + 0.03f * std::sin(t * 3);
    std::string hofRegion;
    {
        const Profile& P0 = Prof(g, 0);
        if (!P0.hof.empty()) hofRegion = ParseHof(P0.hof[(int)P0.hof.size() - 1 - std::min(g.hofSel, (int)P0.hof.size() - 1)]).region;
    }
    DrawTextCentered(hofRegion.empty() ? "HALL OF FAME" : TextFormat("%s HALL OF FAME", hofRegion.c_str()), VW / 2, (int)(12 - 3 * (pulse - 1)), 30, UI_GOLD);
    int n = HofCount(g);
    int players = g.coop ? 2 : 1;
    DrawTextCentered(g.hofSel == 0 ? "Welcome to the Hall of Fame! These Pokemon became Champions together."
                                   : TextFormat("Entry %d of %d", n - g.hofSel, n), VW / 2, 46, 10, UI_TEXT, false);
    for (int w = 0; w < players; w++) {
        const Profile& P = Prof(g, w);
        if (P.hof.empty()) continue;
        int idx = (int)P.hof.size() - 1 - std::min(g.hofSel, (int)P.hof.size() - 1);
        HofEntry e = ParseHof(P.hof[idx]);
        int rowY = players == 1 ? 120 : 92 + w * 124;
        int cell = players == 1 ? 98 : 84, size = players == 1 ? 72 : 58;
        int count = (int)e.mons.size();
        int x0 = VW / 2 - count * cell / 2;
        std::string who = P.name + (e.partner.empty() ? "" : " (with " + e.partner + ")") + TextFormat("  -  entry %d", idx + 1);
        DrawTextCentered(who.c_str(), VW / 2, rowY - 22, 10, g.coop ? PlayerCol(w) : UI_GOLD, false);
        for (int i = 0; i < count; i++) {
            float appear = std::min(1.0f, std::max(0.0f, g.hofT * 2.5f - i * 0.5f));
            if (appear <= 0) continue;
            int cx = x0 + i * cell + cell / 2;
            int base = rowY + size + 6;
            DrawEllipse(cx, base + 2, size * 0.45f, 6, Color{60, 46, 30, 255});   // a gold pedestal
            DrawEllipse(cx, base, size * 0.45f, 6, Color{220, 180, 80, 255});
            BeginBlendMode(BLEND_ADDITIVE);
            DrawCircle(cx, base - size / 2, size * 0.6f * appear, Fade(Color{255, 220, 140, 255}, 0.12f));
            if (appear < 1) {   // a camera flash as each one takes its place
                DrawCircle(cx, base - size / 2, size * (1.2f - appear * 0.5f), Fade(WHITE, 0.5f * (1 - appear)));
                DrawRing({(float)cx, (float)(base - size / 2)}, size * appear, size * appear + 2, 0, 360, 40, Fade(UI_GOLD, 1 - appear));
            }
            EndBlendMode();
            int sp = SpeciesIndex(e.mons[i].first);
            float bob = std::sin(t * 2 + i) * 1.5f;
            if (sp >= 0) DrawSpeciesFit(sp, VIEW_FRONT, cx, (int)(base - 2 + bob - (1 - appear) * 20), size, Fade(WHITE, appear));
            DrawTextCentered(e.mons[i].first.c_str(), cx, base + 10, 10, Fade(UI_TEXT, appear), false);
            DrawTextCentered(TextFormat("Lv%d", e.mons[i].second), cx, base + 22, 10, Fade(UI_GOLD, appear), false);
        }
    }
    DrawTextCentered(n > 1 ? "Left/Right: older / newer entries     Enter: continue" : "Enter: continue", VW / 2, 340, 10, UI_DIM, false);
}

// A training battle in the current training area: a rare one every few wins.
void StartTrainingBattle(Game& g) {
    g.stageIdx = -1;
    g.mode = Mode::Training;
    g.rareBattle = NextTrainingIsRare(g.profile);
    SetTrainingWild(g);
    if (g.rareBattle && g.rareSpecies < 0) {
        std::vector<std::string> owned;
        int sum = 0, n = 0;
        for (const auto& m : g.profile.mons) {
            owned.push_back(m.species);
            if (!m.boxed) { sum += m.level; n++; }
        }
        g.rareSpecies = PickRareSpecies(owned, n ? sum / n : 5);
    }
    BeginTeamSelect(g);
}

// A 2v2 training battle for both co-op players (the hub, or teaming up in tall grass).
void StartCoopTrainingBattle(Game& g) {
    g.stageIdx = -1;
    g.mode = Mode::CoopTraining;
    g.rareBattle = NextTrainingIsRare(g.coopSave.trainingWins);
    SetTrainingWild(g);
    if (g.rareBattle && g.rareSpecies < 0) {
        std::vector<std::string> owned;
        int sum = 0, n = 0;
        for (int p = 0; p < 2; p++)
            for (const auto& m : g.coopP[p].mons) {
                owned.push_back(m.species);
                if (!m.boxed) { sum += m.level; n++; }
            }
        g.rareSpecies = PickRareSpecies(owned, n ? sum / n : 5);
    }
    BeginTeamSelect(g);
}
