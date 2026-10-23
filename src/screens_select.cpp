// Roster choice, team select, battle screen, pause and quick-battle results.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "arena.h"
#include "wild.h"
#include "audio.h"
#include "game.h"
#include "settings.h"
#include "sprites.h"
#include "ui.h"
#include "net.h"

#ifdef ARENA_AUTOTEST
extern bool gAutoPlayP1;
#endif

CombatantSpec FromEntry(const RosterEntry& e) {
    CombatantSpec c;
    c.species = e.species;
    c.level = e.level;
    c.xp = e.xp;
    c.moves = e.moves;
    c.ownedId = e.ownedId;
    c.hpFrac = e.hpFrac;
    c.item = e.item;
    c.abilitySlot = e.ownedId >= 0 ? e.abilitySlot : -1;
    c.nature = e.ownedId >= 0 ? e.nature : -1;
    c.shiny = e.shiny;
    return c;
}

namespace {

constexpr int GRID_COLS = 6;
constexpr int GRID_ROWS_VISIBLE = 3;


// Which players pick with their own input this screen.
int HumanPickers(const Game& g) { return TwoHumans(g) ? 2 : 1; }

const std::vector<RosterEntry>& ViewRoster(const Game& g) {
    int vp = g.shared ? 0 : g.pickPhase;
    return g.roster[vp];
}


// Free battles: a gym arena or a venue (quick_arena: -1 random, 0..8 arenas, then the venues).
ArenaKind QuickArena(uint8_t& venue) {
    int a = Cfg().quickArena;
    int n = ARENA_COUNT + VENUE_COUNT - 1;
    if (a < 0 || a >= n) a = GetRandomValue(0, n - 1);
    venue = 0;
    if (a < ARENA_COUNT) return (ArenaKind)a;
    venue = (uint8_t)(a - ARENA_COUNT + 1);
    return ArenaKind::Stadium;
}

std::vector<CombatantSpec> RandomFreeTeam(int n, int level, bool freePlay) {
    std::vector<CombatantSpec> t;
    std::vector<int> used;
    int count = (int)AllSpecies().size();
    while ((int)t.size() < std::min(n, count)) {
        int idx = GetRandomValue(0, count - 1);
        if (!InSpeciesPool(idx)) continue;
        if (!freePlay) {
            if (!IsBaseForm(idx)) continue;
            idx = EvolvedFormAt(idx, level);
        }
        if (std::find(used.begin(), used.end(), idx) != used.end()) continue;
        used.push_back(idx);
        CombatantSpec c;
        c.species = idx;
        c.level = level;
        c.moves = freePlay ? FreePlayMoves(AllSpecies()[idx]) : MovesAtLevel(AllSpecies()[idx], level);
        t.push_back(c);
    }
    return t;
}

const char* SideName(const Game& g, int s) {
    switch (g.mode) {
        case Mode::Watch: return s == 0 ? "BLUE" : "RED";
        case Mode::TwoPlayer: return s == 0 ? "P1" : "P2";
        case Mode::Doubles: return s == 0 ? "P1" : (g.doublesKind == 0 ? "CPU" : "P2");
        case Mode::Story: case Mode::Training: return s == 0 ? g.profile.name.c_str() : "CPU";
        case Mode::CoopStory: case Mode::CoopTraining: return g.coopP[s & 1].name.c_str();
        default: return s == 0 ? "P1" : "CPU";
    }
}

} // namespace

void GoTitle(Game& g) {
    if (g.coop && g.coopActive >= 0) g.coopP[g.coopActive] = g.profile;
    g.coop = false;
    g.coopActive = -1;
    g.screen = Screen::Title;
    g.paused = false;
    gShake = {0, 0};
}

// ------------------------------------------------------------------ roster choice

void StartQuickFlow(Game& g, Mode m) {
    g.mode = m;
    g.profiles = ListProfiles();
    g.rosterSrc[0] = g.rosterSrc[1] = -1;
    g.rosterReady[0] = false;
    g.rosterReady[1] = !TwoHumans(g);
    if (m == Mode::Watch || g.profiles.empty()) {
        BeginTeamSelect(g);
        return;
    }
    g.screen = Screen::RosterPick;
}

void UpdateRosterPick(Game& g) {
    int n = (int)g.profiles.size();
    for (int p = 0; p < HumanPickers(g); p++) {
        PlayerInput in = TwoHumans(g) ? ReadInput(p, true) : MenuInput();
        if (p == 0 && TwoHumans(g) && IsKeyPressed(KEY_ESCAPE) && !g.rosterReady[0]) { GoTitle(g); return; }
        if (!g.rosterReady[p]) {
            if (in.left) { g.rosterSrc[p] = g.rosterSrc[p] <= -1 ? n - 1 : g.rosterSrc[p] - 1; PlaySfx(Sfx::MenuMove); }
            if (in.right) { g.rosterSrc[p] = g.rosterSrc[p] >= n - 1 ? -1 : g.rosterSrc[p] + 1; PlaySfx(Sfx::MenuMove); }
            if (in.confirm) { g.rosterReady[p] = true; PlaySfx(Sfx::MenuOk); }
            if (in.back && p == 0) { GoTitle(g); return; }
        } else if (in.back) {
            g.rosterReady[p] = false;
            PlaySfx(Sfx::MenuBack);
        }
    }
    if (g.rosterReady[0] && g.rosterReady[1]) {
        for (int p = 0; p < 2; p++) {
            if (g.rosterSrc[p] >= 0) LoadProfile(g.profiles[g.rosterSrc[p]], g.rosterProfile[p]);
            if (g.rosterSrc[p] >= 0 && g.rosterProfile[p].mons.empty()) g.rosterSrc[p] = -1;
        }
        BeginTeamSelect(g);
    }
}

void DrawRosterPick(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    DrawTextCentered("WHOSE POKEMON?", VW / 2, 24, 20, UI_GOLD);
    DrawTextCentered("Free play: every Pokemon at Lv50.  A profile: its Story Mode Pokemon, levels and moves (no XP here).",
                     VW / 2, 52, 10, UI_DIM, false);
    int players = HumanPickers(g);
    for (int p = 0; p < players; p++) {
        int cx = players == 1 ? VW / 2 : (p == 0 ? VW / 4 : VW * 3 / 4);
        int y = 110;
        DrawTextCentered(p == 0 ? "PLAYER 1" : "PLAYER 2", cx, y - 26, 10, TeamColor(p));
        std::string label = g.rosterSrc[p] < 0 ? "Free play (Lv50)" : g.profiles[g.rosterSrc[p]];
        std::string s = "< " + label + " >";
        DrawPanel(cx - 120, y - 6, 240, 26, g.rosterReady[p] ? UI_PANEL_LIGHT : UI_PANEL, g.rosterReady[p] ? UI_GOLD : TeamColor(p));
        DrawTextCentered(s.c_str(), cx, y, 10, g.rosterReady[p] ? UI_GOLD : UI_TEXT, false);
        DrawTextCentered(g.rosterReady[p] ? "READY" : TextFormat("Left/Right choose, %s confirm", ConfirmLabel(p, TwoHumans(g))),
                         cx, y + 30, 10, g.rosterReady[p] ? UI_GOLD : UI_DIM, false);
    }
}

// ------------------------------------------------------------------ team select

void BeginTeamSelect(Game& g) {
    for (int p = 0; p < 2; p++) {
        g.picks[p].clear();
        g.ready[p] = false;
        g.cursor[p] = 0;
        g.scroll[p] = 0;
        g.selBox[p] = -1;
        g.selBoxScroll[p] = 0;
    }
    if (IsCoop(g.mode)) {
        for (int p = 0; p < 2; p++) g.roster[p] = ProfileRoster(g.coopP[p], true);
    } else if (IsStoryMode(g.mode)) {
        g.roster[0] = ProfileRoster(g.profile, true);
        g.roster[1].clear();
    } else {
        for (int p = 0; p < 2; p++)
            g.roster[p] = (g.mode == Mode::Watch || g.rosterSrc[p] < 0) ? FreePlayRoster() : ProfileRoster(g.rosterProfile[p]);
    }
    g.shared = !TwoHumans(g) || (!IsCoop(g.mode) && g.rosterSrc[0] == g.rosterSrc[1]);
    g.pickPhase = g.on.active ? g.on.me : 0;  // online: you only pick your own team
    if (TwoHumans(g) && g.shared) g.cursor[1] = std::min(GRID_COLS - 1, (int)g.roster[0].size() - 1);
    g.teamSize = IsStoryMode(g.mode) ? MAX_TEAM_SIZE : Cfg().teamSize;
    if (g.mode == Mode::Doubles) g.teamSize = std::min(3, g.teamSize);
    if (g.tower)   // the Battle Tower heals everyone: anyone in the party can fight
        for (int p = 0; p < 2; p++) for (auto& e : g.roster[p]) e.hpFrac = 1;
    if (IsCoop(g.mode)) g.teamSize = g.raid ? MAX_TEAM_SIZE : 3;  // each player brings up to 3 of their party (a raid: 6)
    else if (g.tower) g.teamSize = TowerTeamSize(g.towerFloor);
    else if (IsStoryMode(g.mode)) g.teamSize = std::min(g.teamSize, HealthyCount(g.profile));
    if (!IsCoop(g.mode))  // co-op: each player locks in early if they have fewer
        for (int p = 0; p < HumanPickers(g); p++) g.teamSize = std::min(g.teamSize, (int)g.roster[p].size());
    g.teamSize = std::max(1, g.teamSize);
    g.startDelay = -1;
    g.areaPick = false;
    g.screen = Screen::Select;
}

namespace {
bool TrainingSelect(const Game& g) { return (g.mode == Mode::Training || g.mode == Mode::CoopTraining) && !g.tower && g.legend < 0 && !g.raid; }

// Training: the "where to train" menu (X / Tab on the team screen).
void UpdateAreaPick(Game& g) {
    PlayerInput in = MenuInput();
    auto open = OpenWildAreas(TrainStage(g));
    int n = (int)open.size();
    if (n == 0 || in.back || in.sort) { g.areaPick = false; PlaySfx(Sfx::MenuBack); return; }
    if (in.up) { g.areaSel = (g.areaSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.areaSel = (g.areaSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.left) { g.areaSel = std::max(0, g.areaSel - 8); PlaySfx(Sfx::MenuMove); }
    if (in.right) { g.areaSel = std::min(n - 1, g.areaSel + 8); PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    int area = open[std::max(0, std::min(n - 1, g.areaSel))];
    if (area == TrainingAreaIndex(g)) { g.areaPick = false; PlaySfx(Sfx::MenuOk); return; }
    if (g.on.active && g.rareBattle) { Toast(g, "This rare find lives where you are - change the area after this battle."); return; }
    if (!SetTrainArea(g, area)) { PlaySfx(Sfx::MenuBack); return; }
    SetTrainingWild(g);
    if (g.rareBattle) RepickRare(g);
    g.areaPick = false;
    PlaySfx(Sfx::MenuOk);
}

void DrawAreaPick(const Game& g) {
    auto open = OpenWildAreas(TrainStage(g));
    int n = (int)open.size();
    if (n == 0) return;
    int sel = std::max(0, std::min(n - 1, g.areaSel));
    int cur = TrainingAreaIndex(g);
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.75f));
    DrawPanel(14, 14, 612, 328, Color{28, 40, 34, 255}, Color{120, 200, 120, 255});
    DrawRectangle(14, 14, 612, 22, Color{50, 110, 60, 255});
    DrawTextShadow("WHERE DO YOU WANT TO TRAIN?", 24, 19, 10, WHITE);
    if (g.on.active && g.on.me != 0) GDrawText("(the host picks the area)", 400, 19, 10, Color{200, 230, 200, 255});
    // the areas
    int rows = 14, first = std::max(0, std::min(sel - rows / 2, n - rows));
    for (int k = first; k < std::min(n, first + rows); k++) {
        int a = open[k];
        int y = 42 + (k - first) * 21;
        bool on = k == sel, here = a == cur;
        DrawPanel(22, y, 196, 19, on ? Color{60, 120, 70, 255} : Color{36, 54, 42, 255}, on ? UI_GOLD : (here ? Color{140, 230, 150, 255} : Color{60, 90, 66, 255}));
        GDrawText(WildAreas()[a].name.c_str(), 30, y + 5, 10, on ? UI_GOLD : UI_TEXT);
        if (here) GDrawText("here", 212 - TextW("here", 10), y + 5, 10, Color{140, 230, 150, 255});
    }
    if (n > rows) GDrawText(TextFormat("%d / %d", sel + 1, n), 22, 338 - 12, 10, UI_DIM);
    // what lives there: seen ones show, unseen ones are silhouettes
    int area = open[sel];
    auto mons = WildAreaMons(area, TrainStage(g), TrainBadges(g));
    GDrawText(WildAreas()[area].name.c_str(), 232, 44, 20, UI_GOLD);
    GDrawText("Pokemon found here", 232, 68, 10, UI_DIM);
    auto seen = [&](int sp) {
        const std::string& nm = AllSpecies()[sp].name;
        for (int p = 0; p < (g.coop ? 2 : 1); p++)
            if (DexHasSeen(Prof(g, p), nm) || DexHasCaught(Prof(g, p), nm)) return true;
        return false;
    };
    int cols = 4;
    for (int k = 0; k < (int)mons.size() && k < 16; k++) {
        const auto& m = mons[k];
        int x = 232 + (k % cols) * 96, y = 84 + (k / cols) * 58;
        bool s = seen(m.species);
        DrawPanel(x, y, 92, 54, Color{36, 54, 42, 255}, m.rare ? Color{255, 150, 230, 255} : Color{70, 110, 76, 255});
        DrawSpeciesFit(m.species, VIEW_FRONT, x + 46, y + 38, 32, s ? WHITE : Color{12, 16, 14, 255});
        DrawTextCentered(s ? AllSpecies()[m.species].name.c_str() : "???", x + 46, y + 40, 10, s ? UI_TEXT : UI_DIM, false);
        const char* tag = m.rare ? "RARE" : (m.share >= 0.25f ? "Common" : m.share >= 0.10f ? "Uncommon" : "Rare");
        GDrawText(tag, x + 4, y + 3, 10, m.rare ? Color{255, 150, 230, 255} : UI_DIM);
        GDrawText(TextFormat("Lv%d-%d", m.lo, m.hi), x + 88 - TextW(TextFormat("Lv%d-%d", m.lo, m.hi), 10), y + 3, 10, UI_DIM);
    }
    if (mons.size() > 16) GDrawText(TextFormat("+%d more", (int)mons.size() - 16), 232, 316, 10, UI_DIM);
    GDrawText("Unseen Pokemon show as silhouettes - meet them to fill in your Pokedex.", 232, 324, 10, Color{170, 200, 170, 255});
    GDrawText("Up/Down area   Enter train here   X / Esc close", 24, 346 - 12, 10, Color{200, 230, 200, 255});
}
} // namespace

// ------------------------------------------------------------------ the Box (Story team select)
// Swap a Box Pokemon with the party Pokemon under the cursor, right on the team screen, to bring
// the right types for the next battle. STRONG / WEAK marks show the matchup against the stage's foes.
namespace {
Profile& SelProfile(Game& g, int p) { return IsCoop(g.mode) ? g.coopP[p] : g.profile; }
const Profile& SelProfile(const Game& g, int p) { return IsCoop(g.mode) ? g.coopP[p] : g.profile; }

bool SelBoxAllowed(const Game& g, int p) {
    if (g.raid && !IsCoop(g.mode)) return false;   // the solo raid lists the Box already
    return IsStoryMode(g.mode) && !(g.on.active && p != g.on.me);
}

// Box Pokemon (profile indexes), in collection order.
std::vector<int> SelBoxList(const Game& g, int p) {
    std::vector<int> v;
    const Profile& P = SelProfile(g, p);
    for (int i = 0; i < (int)P.mons.size(); i++)
        if (P.mons[i].boxed && MonSpecies(P.mons[i]) >= 0) v.push_back(i);
    return v;
}

// The species the next battle's foes use (named team entries; the legendary), for the matchup marks.
std::vector<int> SelFoes(const Game& g) {
    std::vector<int> v;
    if (g.mode != Mode::Story && g.mode != Mode::CoopStory) {
        if (g.rareBattle && g.rareSpecies >= 0) v.push_back(g.rareSpecies);
        return v;
    }
    const StageDef st = ActiveStage(g);
    if (st.IsLegend()) { int i = SpeciesIndex(st.trainer); if (i >= 0) v.push_back(i); }
    for (std::string e : st.team) {
        size_t sp = e.find_last_of(' ');
        if (sp != std::string::npos && sp + 1 < e.size() && (e[sp + 1] == '+' || e[sp + 1] == '-')) e = e.substr(0, sp);
        if (size_t s2 = e.find(" shiny"); s2 != std::string::npos) e = e.substr(0, s2);
        int i = SpeciesIndex(e);
        if (i >= 0) v.push_back(i);
    }
    return v;
}

// +1: one of its moves hits a foe super effectively; -1 bit: a foe's type hits it super effectively.
int SelMatchup(int species, const std::vector<MoveDef>& moves, const std::vector<int>& foes) {
    if (foes.empty() || species < 0) return 0;
    const SpeciesDef& me = AllSpecies()[species];
    bool strong = false, weak = false;
    for (int f : foes) {
        const SpeciesDef& fs = AllSpecies()[f];
        for (const auto& m : moves) if (IsDamaging(m) && TypeEffectiveness(m.type, fs.type1, fs.type2) > 1) strong = true;
        if (TypeEffectiveness(fs.type1, me.type1, me.type2) > 1 || (fs.type2 != Type::None && TypeEffectiveness(fs.type2, me.type1, me.type2) > 1)) weak = true;
    }
    return (strong ? 1 : 0) | (weak ? 2 : 0);
}

void DrawMatchupMarks(int mu, int x, int y) {   // a green up arrow = strong, a red down arrow = weak
    if (mu & 1) DrawTriangle({(float)x + 4, (float)y}, {(float)x, (float)y + 6}, {(float)x + 8, (float)y + 6}, Color{110, 230, 120, 255});
    if (mu & 2) DrawTriangle({(float)x + 10, (float)y}, {(float)x + 14, (float)y + 6}, {(float)x + 18, (float)y}, Color{255, 110, 100, 255});
}

constexpr int BOX_COLS = 5, BOX_ROWS = 3;

// The Box is open for player p: move, swap, close. Returns after handling the input.
void UpdateSelBox(Game& g, int p, const PlayerInput& in, bool boxKey) {
    auto list = SelBoxList(g, p);
    int n = (int)list.size();
    int& c = g.selBox[p];
    if (in.back || boxKey || n == 0) { c = -1; PlaySfx(Sfx::MenuBack); return; }
    int col = c % BOX_COLS, row = c / BOX_COLS, rows = (n + BOX_COLS - 1) / BOX_COLS;
    if (in.left) col = (col + BOX_COLS - 1) % BOX_COLS;
    if (in.right) col = (col + 1) % BOX_COLS;
    if (in.up) row = (row + rows - 1) % rows;
    if (in.down) row = (row + 1) % rows;
    if (in.left || in.right || in.up || in.down) {
        c = std::max(0, std::min(n - 1, row * BOX_COLS + col));
        int r = c / BOX_COLS;
        if (r < g.selBoxScroll[p]) g.selBoxScroll[p] = r;
        if (r >= g.selBoxScroll[p] + BOX_ROWS) g.selBoxScroll[p] = r - BOX_ROWS + 1;
        PlaySfx(Sfx::MenuMove);
    }
    if (!in.confirm) return;
    auto& ros = g.roster[g.shared ? 0 : p];
    int cur = g.cursor[p];
    if (cur < 0 || cur >= (int)ros.size() || ros[cur].ownedId < 0) return;
    Profile& P = SelProfile(g, p);
    int a = ros[cur].ownedId, b = list[std::max(0, std::min(n - 1, c))];
    std::string inName = P.mons[b].species, outName = P.mons[a].species;
    std::swap(P.mons[a], P.mons[b]);   // the Box Pokemon takes the party slot, so the picks stay put
    P.mons[a].boxed = false;
    P.mons[b].boxed = true;
    ros = ProfileRoster(P, true);
    if (g.tower) for (auto& e : ros) e.hpFrac = 1;
    auto& pk = g.picks[p];
    bool unpicked = false;
    if (IsStoryMode(g.mode) && !g.tower && cur < (int)ros.size() && ros[cur].hpFrac <= 0) {   // a fainted one can't be picked
        auto it = std::find(pk.begin(), pk.end(), cur);
        if (it != pk.end()) { pk.erase(it); unpicked = true; }
    }
    if (IsCoop(g.mode)) { if (OnlineMine(g, p)) SaveProfile(P); if (g.on.active) OnlineSendProfile(g); }
    else if (!SaveProfile(P)) Toast(g, "Could not save the profile!");
    c = -1;
    PlaySfx(Sfx::MenuOk);
    Toast(g, inName + " joined the party. " + outName + " went to the Box." + (unpicked ? " (It has fainted, so it was taken off the team.)" : ""));
}

void DrawSelBox(const Game& g, int p) {
    auto list = SelBoxList(g, p);
    const Profile& P = SelProfile(g, p);
    auto foes = SelFoes(g);
    const auto& ros = g.roster[g.shared ? 0 : p];
    int cur = g.cursor[p];
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.7f));
    int x0 = 10, y0 = 28, w = VW - 20, h = 196;
    DrawPanel(x0, y0, w, h, Color{30, 36, 54, 255}, Color{120, 160, 230, 255});
    DrawRectangle(x0, y0, w, 20, Color{50, 74, 120, 255});
    std::string out = cur >= 0 && cur < (int)ros.size() ? AllSpecies()[ros[cur].species].name : "?";
    DrawTextShadow(TextFormat("BOX - swap with %s", out.c_str()), x0 + 8, y0 + 5, 10, WHITE);
    GDrawText(foes.empty() ? "" : "green up = strong vs this foe   red down = weak to it", x0 + w - 8 - TextW("green up = strong vs this foe   red down = weak to it", 10), y0 + 5, 10, Color{200, 220, 255, 255});
    int sc = g.selBoxScroll[p];
    for (int k = 0; k < (int)list.size(); k++) {
        int r = k / BOX_COLS;
        if (r < sc || r >= sc + BOX_ROWS) continue;
        int i = list[k];
        CombatantSpec cs = MonCombatant(P, i);
        const SpeciesDef& sp = AllSpecies()[cs.species];
        int x = x0 + 6 + (k % BOX_COLS) * 122, y = y0 + 26 + (r - sc) * 54, cw = 118, ch = 50;
        bool on = k == g.selBox[p];
        DrawPanel(x, y, cw, ch, on ? Color{60, 80, 130, 255} : UI_PANEL, on ? UI_GOLD : Color{80, 74, 104, 255});
        DrawRectangle(x + 1, y + 1, cw - 2, 3, TypeColor(sp.type1));
        DrawSpeciesFit(ShinyIdx(cs.species, cs.shiny), VIEW_FRONT, x + 21, y + 46, 36);
        GDrawText(sp.name.c_str(), x + 40, y + 7, 10, UI_TEXT);
        int bx = x + 40 + DrawTypeBadge(sp.type1, x + 40, y + 20) + 2;
        if (sp.type2 != Type::None) DrawTypeBadge(sp.type2, bx, y + 20);
        GDrawText(TextFormat("Lv%d", cs.level), x + 40, y + 34, 10, UI_GOLD);
        DrawMatchupMarks(SelMatchup(cs.species, cs.moves, foes), x + cw - 22, y + 36);
        DrawHpBar(x + 40, y + 46, 52, 2, P.mons[i].hp);
        if (P.mons[i].hp <= 0) GDrawText("FAINTED", x + 40, y + 34, 10, Color{255, 120, 100, 255});
    }
    int rows = ((int)list.size() + BOX_COLS - 1) / BOX_COLS;
    if (sc > 0) DrawTextCentered("^ more ^", VW / 2, y0 + 20, 10, UI_DIM, false);
    if (sc + BOX_ROWS < rows) DrawTextCentered("v more v", VW / 2, y0 + h - 12, 10, UI_DIM, false);
    GDrawText(TextFormat("%d in the Box   Enter: swap in   R / Esc: close", (int)list.size()), x0 + 8, y0 + h - 12, 10, Color{200, 220, 255, 255});
}
} // namespace

void UpdateSelect(Game& g, float dt) {
    if (g.areaPick) { UpdateAreaPick(g); return; }
    if (TrainingSelect(g) && MenuSortPressed()) {   // X / Tab: choose where to train
        auto open = OpenWildAreas(TrainStage(g));
        g.areaSel = 0;
        for (int k = 0; k < (int)open.size(); k++) if (open[k] == TrainingAreaIndex(g)) g.areaSel = k;
        if (!open.empty()) { g.areaPick = true; PlaySfx(Sfx::MenuOk); return; }
    }
    int players = HumanPickers(g);
    bool online = g.on.active;
    for (int p = 0; p < players; p++) {
        if (!g.shared && p != g.pickPhase) continue;
        if (online && p != g.on.me) continue;
        const std::vector<RosterEntry>& ros = g.roster[g.shared ? 0 : p];
        int n = (int)ros.size();
        int rows = (n + GRID_COLS - 1) / GRID_COLS;
        PlayerInput in = (players == 1 || online) ? MenuInput() : ReadInput(p, true);
        if (online && g.on.picksSent) continue;  // locked in and sent: waiting for the battle
        int& c = g.cursor[p];
        bool boxKey = in.filter || ((players != 1 && !online) && (IsKeyPressed(KEY_R) || (IsGamepadAvailable(p) && IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_FACE_UP))));
        if (g.selBox[p] >= 0) { UpdateSelBox(g, p, in, boxKey); continue; }
        if (boxKey && !g.ready[p] && SelBoxAllowed(g, p)) {
            if (SelBoxList(g, p).empty()) Toast(g, "Your Box is empty.");
            else if (c < n && ros[c].ownedId >= 0) { g.selBox[p] = 0; g.selBoxScroll[p] = 0; PlaySfx(Sfx::MenuOk); }
            continue;
        }
        if (!g.ready[p]) {
            int col = c % GRID_COLS, row = c / GRID_COLS;
            bool moved = in.left || in.right || in.up || in.down;
            if (in.left) col = (col + GRID_COLS - 1) % GRID_COLS;
            if (in.right) col = (col + 1) % GRID_COLS;
            if (in.up) row = (row + rows - 1) % rows;
            if (in.down) row = (row + 1) % rows;
            c = std::max(0, std::min(n - 1, row * GRID_COLS + col));
            if (moved) {
                PlaySfx(Sfx::MenuMove);
                int r = c / GRID_COLS;
                int& sc = g.scroll[g.shared ? 0 : p];
                if (r < sc) sc = r;
                if (r >= sc + GRID_ROWS_VISIBLE) sc = r - GRID_ROWS_VISIBLE + 1;
            }
        }
        auto& pk = g.picks[p];
        bool already = std::find(pk.begin(), pk.end(), c) != pk.end();
        bool lockIn = !g.ready[p] && !pk.empty() && (in.swap || (in.confirm && (already || (int)pk.size() >= g.teamSize)));
        if (g.raid && !g.ready[p] && pk.empty() && !g.raidTeam1.empty() && in.swap) lockIn = true;   // Team 2 can be left empty
        if (in.confirm && !lockIn && !g.ready[p] && (int)pk.size() < g.teamSize && !already) {
            if (g.raid && std::find(g.raidTaken.begin(), g.raidTaken.end(), c) != g.raidTaken.end()) {
                Toast(g, AllSpecies()[ros[c].species].name + " is already on Team 1");
                PlaySfx(Sfx::MenuBack);
            } else if (IsStoryMode(g.mode) && c < n && ros[c].hpFrac <= 0) {
                Toast(g, AllSpecies()[ros[c].species].name + " has fainted - heal it at the Healing Center");
                PlaySfx(Sfx::MenuBack);
            } else {
                pk.push_back(c);
                PlaySfx(Sfx::MenuOk);
            }
        }
        if (lockIn) {
            {
                g.ready[p] = true;
                PlaySfx(Sfx::MenuOk);
                if (!g.shared && p == 0 && !online) { g.pickPhase = 1; g.scroll[1] = 0; }
            }
        }
        if (in.back) {
            PlaySfx(Sfx::MenuBack);
            if (g.ready[p]) { g.ready[p] = false; if (!g.shared) g.pickPhase = p; }
            else if (!g.picks[p].empty()) g.picks[p].pop_back();
            else if (online) { OnlineCancelSelect(g); EnterStoryHub(g); return; }
            else if (p == 1 && !g.shared) { g.pickPhase = 0; g.ready[0] = false; }
            else if (p == 0) {
                if (IsStoryMode(g.mode)) EnterStoryHub(g);
                else GoTitle(g);
                return;
            }
        }
    }
    if (online) return;  // the host starts the battle once both teams are in (screens_online.cpp)
    if (players == 1) g.ready[1] = g.ready[0];
    if (g.raid && !IsCoop(g.mode) && g.ready[0] && g.raidTeam1.empty()) {
        // the solo raid: that was Team 1 - now Team 2 (it comes in when all of Team 1 is down)
        for (int i : g.picks[0]) g.raidTeam1.push_back(FromEntry(g.roster[0][i]));
        g.raidTaken = g.picks[0];
        g.picks[0].clear();
        g.ready[0] = g.ready[1] = false;
        int left = 0;
        for (int i = 0; i < (int)g.roster[0].size(); i++)
            if (g.roster[0][i].hpFrac > 0 && std::find(g.raidTaken.begin(), g.raidTaken.end(), i) == g.raidTaken.end()) left++;
        g.teamSize = std::min(MAX_TEAM_SIZE, left);
        if (left == 0) { g.ready[0] = g.ready[1] = true; }   // nobody left for Team 2: straight in
        else Toast(g, "Team 1 is set! Now pick Team 2 - it comes in when all of Team 1 is down.");
        PlaySfx(Sfx::MenuOk);
        if (left > 0) return;
    }

    if (g.ready[0] && g.ready[1]) {
        if (g.startDelay < 0) g.startDelay = 0.7f;
        g.startDelay -= dt;
        if (g.startDelay <= 0) StartBattleFromSelect(g);
    } else {
        g.startDelay = -1;
    }
}

void StartBattleFromSelect(Game& g) {
    BattleSetup su;
    su.twoPlayer = TwoHumans(g);
    su.autoAim = Cfg().autoAim;
    su.screenShake = Cfg().screenShake;
    su.damageNumbers = Cfg().damageNumbers;

    su.arena = IsStoryMode(g.mode) ? ArenaKind::Stadium : QuickArena(su.venue);
    SideSetup& a = su.side[0];
    for (int i : g.picks[0]) a.team.push_back(FromEntry(g.roster[0][i]));
    if (!IsStoryMode(g.mode))
        for (auto& c : a.team) c.hpFrac = 1;
    a.cpu = g.mode == Mode::Watch;
#ifdef ARENA_AUTOTEST
    if (gAutoPlayP1) { a.cpu = true; a.level = AILevel::Hard; }
#endif
    a.level = Cfg().cpuLevel;
    a.earnXp = IsStoryMode(g.mode);
    a.label = SideName(g, 0);
    if (g.ow.inputPlayer >= 0) {   // local co-op: a player's own battle in their half of the screen, on their keys
        a.input = g.ow.inputPlayer;
        su.twoPlayer = true;
    }

    SideSetup& b = su.side[1];
    b.label = SideName(g, 1);
    int avg = 0, top = 0;
    for (const auto& c : a.team) { avg += c.level; top = std::max(top, c.level); }
    avg = a.team.empty() ? 5 : (int)std::lround((float)avg / a.team.size());
    SetPartyTop(top);   // later regions scale to your strongest
    {   // the foes' name plates mark the species you already own (your own Pokedex; local co-op: either of you)
        std::vector<char> owned(AllSpecies().size(), 0);
        for (int p = 0; p < (g.coop ? 2 : 1); p++) {
            if (g.coop && g.on.active && p != g.on.me) continue;
            const Profile& P = g.coop ? g.coopP[p] : g.profile;
            for (size_t i = 0; i < owned.size(); i++) if (!owned[i] && DexHasCaught(P, AllSpecies()[i].name)) owned[i] = 1;
        }
        SetOwnedSpecies(std::move(owned));
    }
    bool storyish = g.mode == Mode::Story || g.mode == Mode::CoopStory;
    SetDifficulty(storyish ? (g.coop ? g.coopP[0].difficulty : g.profile.difficulty) : 1);   // co-op: the host's
    auto diffAI = [&](AILevel base) { return !storyish ? base : Difficulty() == 0 ? AILevel::Easy : Difficulty() == 2 ? AILevel::Hard : base; };

    switch (g.mode) {
        case Mode::TwoPlayer: {
            const auto& ros = g.roster[g.shared ? 0 : 1];
            for (int i : g.picks[1]) b.team.push_back(FromEntry(ros[i]));
            for (auto& c : b.team) c.hpFrac = 1;
        } break;
        case Mode::Story: {
            StageDef st = ActiveStage(g);
            if (g.legend >= 0) {   // a legendary: one boss with a huge HP pool, in its lair
                g.cpuTeam = {LegendBoss(g.legend, avg, (int)a.team.size(), false)};
                b.team = g.cpuTeam;
                b.cpu = true;
                b.level = AILevel::Hard;
                b.label = "Legendary";
                su.trainerBattle = false;
                su.arena = st.arena;
                su.catchable = true;
                break;
            }
            if (st.rival) {   // Blue in Kanto, Silver in Johto
                EnsureRival(g, st.region);
                SetRivalContext(SpeciesIndex(RegionRival(g.profile, st.region)), -1);
            }
            g.cpuTeam = MakeCpuTeam(&st, avg, MAX_TEAM_SIZE);
            b.team = g.cpuTeam;
            b.cpu = true;
            b.level = diffAI(st.cpu);
            b.label = st.trainer;
            su.trainerBattle = true;
            su.arena = st.arena;
            su.catchable = st.Catchable();
            if (st.dbl) {
                // A double battle: the foes come out two at a time, and a teammate fights beside you -
                // the stage's partner trainer (Lance), or else your own Pokemon, run by the CPU.
                SideSetup me = a, foe1 = b, foe2 = b;
                foe1.team.clear(); foe2.team.clear();
                for (int i = 0; i < (int)g.cpuTeam.size(); i++) (i % 2 == 0 ? foe1 : foe2).team.push_back(g.cpuTeam[i]);
                foe2.tag = "CPU 2";
                size_t amp = st.trainer.find(" & ");
                if (amp != std::string::npos) {   // "Twins Eli & Anne": each foe slot shows its own trainer
                    std::string first = st.trainer.substr(0, amp), second = st.trainer.substr(amp + 3);
                    size_t sp = first.find_last_of(' ');
                    if (second.find(' ') == std::string::npos && sp != std::string::npos) second = first.substr(0, sp + 1) + second;
                    foe1.label = first;
                    foe2.label = second;
                }
                SideSetup ally;
                ally.cpu = true;
                ally.level = AILevel::Hard;
                ally.team_ = 0;
                if (!st.partner.empty()) {
                    StageDef ps = st;
                    ps.team = st.partnerTeam.empty() ? std::vector<std::string>{"random", "random", "random"} : st.partnerTeam;
                    ps.rival = false;
                    ally.team = MakeCpuTeam(&ps, avg, MAX_TEAM_SIZE);
                    ally.label = st.partner;
                    ally.tag = "ALLY";
                    ally.earnXp = false;
                } else if (me.team.size() >= 2) {   // your 2nd, 4th and 6th picks fight beside you
                    std::vector<CombatantSpec> mine;
                    for (int i = 0; i < (int)me.team.size(); i++) (i % 2 == 0 ? mine : ally.team).push_back(me.team[i]);
                    me.team = mine;
                    ally.label = "Partner";
                    ally.tag = "CPU";
                    ally.earnXp = true;
                }
                su.side.clear();
                su.side.push_back(me);
                if (!ally.team.empty()) su.side.push_back(ally);
                su.side.push_back(foe1);
                if (!foe2.team.empty()) su.side.push_back(foe2);
            }
        } break;
        case Mode::Training:
            if (g.raid) break;   // the Daily Raid: built below
            su.catchable = true;
            SetTrainingWild(g);   // the Pokemon that live in the chosen training area
            if (g.rareBattle && g.rareSpecies >= 0) {
                // A lone rare creature, a little above your level, in a random arena you've unlocked.
                CombatantSpec c;
                c.level = g.ow.staticLevel > 0 ? g.ow.staticLevel : std::min(MAX_LEVEL, avg + 3);   // met on the map: its own level
                c.species = WildActive() || g.ow.staticLevel > 0 ? g.rareSpecies : EvolvedFormAt(g.rareSpecies, c.level);
                c.moves = MovesAtLevel(AllSpecies()[c.species], c.level);
                c.shiny = RollShiny();   // the rare one can be shiny too
                g.cpuTeam = {c};
                // A regular one tags along - unless you brought just one Pokemon (never more wild
                // Pokemon than you have).
                if (g.picks[0].size() >= 2 && !g.ow.wildBattle) {
                    auto pal = MakeCpuTeam(nullptr, std::max(2, avg - 1), 1);
                    if (!pal.empty()) g.cpuTeam.push_back(pal[0]);
                }
                b.label = "Wild " + AllSpecies()[c.species].name;
                b.level = AILevel::Hard;
                su.arena = ArenaKind::Stadium;   // wild battles happen out in the world (venue below)
            } else {
                // never more wild Pokemon than you brought
                int foes = g.ow.wildBattle ? 1 : std::max(1, std::min((int)g.picks[0].size(), Story().trainingMaxFoes));   // tall grass: one at a time
                g.cpuTeam = MakeCpuTeam(nullptr, avg, foes);
                b.level = Cfg().cpuLevel;
                b.label = "Wild";
            }
            b.team = g.cpuTeam;
            b.cpu = true;
            break;
        case Mode::CoopStory:
        case Mode::CoopTraining: {
            if (g.raid) break;   // the Daily Raid: built below
            // 2v2: each player's slot against the CPU team split across two slots.
            std::vector<SideSetup> v(2);
            for (int p = 0; p < 2; p++) {
                SideSetup& h = v[p];
                for (int i : g.picks[p]) {
                    CombatantSpec c = FromEntry(g.roster[p][i]);
                    c.owner = p;
                    h.team.push_back(c);
                }
                h.earnXp = true;
                h.team_ = 0;
                h.input = p;
                h.tag = p == 0 ? "P1" : "P2";
                h.label = g.coopP[p].name;
                h.level = Cfg().cpuLevel;
#ifdef ARENA_AUTOTEST
                if (gAutoPlayP1) { h.cpu = true; h.level = AILevel::Hard; }
#endif
            }
            int sum = 0, cnt = 0, top2 = 0;
            for (auto& h : v) for (auto& c : h.team) { sum += c.level; cnt++; top2 = std::max(top2, c.level); }
            int avg2 = cnt ? (int)std::lround((float)sum / cnt) : 5;
            SetPartyTop(top2);
            std::vector<CombatantSpec> cpu;
            AILevel lvA = Cfg().cpuLevel, lvB = Cfg().cpuLevel;
            std::string label = "Rival";
            if (g.mode == Mode::CoopStory && g.legend >= 0) {   // 2v1: both players against the legendary
                StageDef st = ActiveStage(g);
                int brought = (int)(v[0].team.size() + v[1].team.size());
                cpu = {LegendBoss(g.legend, avg2, brought, true)};
                lvA = lvB = AILevel::Hard;
                label = "Legendary";
                su.trainerBattle = false;
                su.arena = st.arena;
                su.catchable = true;
            } else if (g.mode == Mode::CoopStory) {
                StageDef st = ActiveStage(g);
                if (st.rival) {
                    // The rival brings both of his starters (two of the ones neither player took).
                    EnsureRival(g, st.region);
                    const std::string* rv = CoopRival(g.coopSave, st.region);
                    SetRivalContext(SpeciesIndex(rv[0]), SpeciesIndex(rv[1]));
                    std::vector<std::string> team = st.team;
                    bool hasPartner = false;
                    for (auto& e : team) hasPartner |= e.rfind("rival_partner", 0) == 0;
                    if (!hasPartner) {   // at the same stage of evolution as the main starter ("rival@20" -> "rival_partner@20")
                        std::string partner = "rival_partner";
                        for (const auto& e : team)
                            if (e.rfind("rival", 0) == 0 && (e.size() == 5 || e[5] == '@' || e[5] == ' ')) {
                                size_t at = e.find('@');
                                if (at != std::string::npos) partner += e.substr(at, e.find(' ', at) == std::string::npos ? std::string::npos : e.find(' ', at) - at);
                            }
                        team.insert(team.end() - (team.empty() ? 0 : 1), partner);
                    }
                    // (no random padding: the rival's team is exactly his own Pokemon)
                    while ((int)team.size() > MAX_TEAM_SIZE) team.erase(team.begin());
                    st.team = team;
                } else {
                // Two players face more creatures: at least 3, two more than solo, never more than 6.
                int written = std::max(1, (int)st.team.size());
                int want = std::min(MAX_TEAM_SIZE, std::max(3, written + 2));
                std::vector<std::string> base = st.team.empty() ? std::vector<std::string>{"random"} : st.team;
                st.team.clear();
                for (int i = 0; i < want; i++) st.team.push_back(base[i % base.size()]);
                }
                cpu = MakeCpuTeam(&st, avg2, MAX_TEAM_SIZE);
                lvA = lvB = diffAI(st.cpu);
                label = st.trainer;
                su.trainerBattle = true;
                su.arena = st.arena;
                su.catchable = st.Catchable();
            } else {
                su.catchable = true;
                SetTrainingWild(g);
                if (g.rareBattle && g.rareSpecies >= 0) {
                    // A tougher rare creature for two players, with a wild partner.
                    CombatantSpec c;
                    c.level = g.ow.staticLevel > 0 ? g.ow.staticLevel : std::min(MAX_LEVEL, avg2 + 6);
                    c.species = WildActive() || g.ow.staticLevel > 0 ? g.rareSpecies : EvolvedFormAt(g.rareSpecies, c.level);
                    c.moves = MovesAtLevel(AllSpecies()[c.species], c.level);
                    c.shiny = RollShiny();   // the rare one can be shiny too
                    cpu = {c};
                    if (v[0].team.size() + v[1].team.size() >= 2) {
                        auto pal = MakeCpuTeam(nullptr, std::min(MAX_LEVEL, avg2 + 1), 1);
                        if (!pal.empty()) cpu.push_back(pal[0]);
                    }
                    lvA = AILevel::Hard;
                    label = "Wild " + AllSpecies()[c.species].name;
                    su.arena = ArenaKind::Stadium;
                } else {
                    int total = (int)(v[0].team.size() + v[1].team.size());
                    int foes = g.ow.wildBattle ? std::min(2, total) : std::min(4, std::max(1, total - 1));   // never more wild Pokemon than the two of you brought (tall grass, teamed up: two)
                    cpu = MakeCpuTeam(nullptr, avg2, foes);
                    label = "Wild";
                }
            }
            g.cpuTeam = cpu;
            for (int k = 0; k < 2; k++) {
                SideSetup c;
                for (int i = k; i < (int)cpu.size(); i += 2) c.team.push_back(cpu[i]);
                if (c.team.empty()) continue;
                c.cpu = true;
                c.level = k == 0 ? lvA : lvB;
                c.label = label;
                if (size_t amp = label.find(" & "); amp != std::string::npos && g.legend < 0) {   // "Twins Eli & Anne": one trainer per slot
                    std::string first = label.substr(0, amp), second = label.substr(amp + 3);
                    size_t sp = first.find_last_of(' ');
                    if (second.find(' ') == std::string::npos && sp != std::string::npos) second = first.substr(0, sp + 1) + second;
                    c.label = k == 0 ? first : second;
                }
                c.tag = g.legend >= 0 ? "LEGEND" : "CPU";
                c.team_ = 1;
                v.push_back(c);
            }
            su.side = v;
        } break;
        case Mode::Doubles: {
            // Four slots: P1 + (CPU or P2) against two CPUs, or P1 + CPU vs P2 + CPU.
            int size = std::max(1, (int)g.picks[0].size());
            bool freePlay = g.rosterSrc[0] < 0;
            int lv = freePlay ? FREE_PLAY_LEVEL : avg;
            a.tag = "P1"; a.label = "P1"; a.team_ = 0; a.input = 0;
            SideSetup p2;
            if (g.doublesKind == 0) {
                p2.team = RandomFreeTeam(size, lv, freePlay);
                p2.cpu = true; p2.level = AILevel::Hard; p2.label = "CPU"; p2.tag = "CPU"; p2.team_ = 0;
            } else {
                const auto& ros = g.roster[g.shared ? 0 : 1];
                for (int i : g.picks[1]) p2.team.push_back(FromEntry(ros[i]));
                for (auto& c : p2.team) c.hpFrac = 1;
                p2.label = "P2"; p2.tag = "P2"; p2.input = 1; p2.team_ = g.doublesKind == 1 ? 0 : 1;
            }
#ifdef ARENA_AUTOTEST
            if (gAutoPlayP1 && p2.input == 1) { p2.cpu = true; p2.level = AILevel::Hard; }
#endif
            SideSetup p1 = a;
            su.side.clear();
            su.side.push_back(p1);
            su.side.push_back(p2);
            if (g.doublesKind == 2) {  // each player gets a CPU partner
                for (int t = 0; t < 2; t++) {
                    SideSetup c;
                    c.team = RandomFreeTeam(size, lv, freePlay);
                    c.cpu = true; c.level = Cfg().cpuLevel; c.label = "CPU"; c.tag = "CPU"; c.team_ = t;
                    su.side.push_back(c);
                }
            } else {
                for (int k = 0; k < 2; k++) {
                    SideSetup c;
                    c.team = RandomFreeTeam(size, lv, freePlay);
                    c.cpu = true; c.level = Cfg().cpuLevel; c.label = "CPU"; c.tag = k == 0 ? "CPU 1" : "CPU 2"; c.team_ = 1;
                    su.side.push_back(c);
                }
            }
        } break;
        default: {  // Quick / Watch
            bool freePlay = g.mode == Mode::Watch || g.rosterSrc[0] < 0;
            b.team = RandomFreeTeam(std::max(1, (int)g.picks[0].size()), freePlay ? FREE_PLAY_LEVEL : avg, freePlay);
            b.cpu = true;
            b.level = Cfg().cpuLevel;
        } break;
    }
    if (g.raid) {
        // The Daily Raid: every Pokemon you brought is on the field at once - you steer one, the CPU the rest
        // (Swap moves you between them). Solo, Team 2 waits and comes in when all of Team 1 is down.
        bool coopRaid = g.mode == Mode::CoopTraining;
        std::vector<CombatantSpec> first[2], second;
        for (int p = 0; p < (coopRaid ? 2 : 1); p++)
            for (int i : g.picks[p]) {
                CombatantSpec c = FromEntry(g.roster[p][i]);
                c.owner = p;
                (coopRaid || g.raidTeam1.empty() ? first[p] : second).push_back(c);
            }
        if (!coopRaid && !g.raidTeam1.empty()) first[0] = g.raidTeam1;
        int top = 1, onField = 0;
        for (int p = 0; p < 2; p++) for (const auto& c : first[p]) { top = std::max(top, c.level); onField++; }
        for (const auto& c : second) top = std::max(top, c.level);
        RaidInfo r = RaidOfDay(RaidToday());
        CombatantSpec boss;
        boss.species = std::max(0, r.species);
        boss.level = AllSpecies()[boss.species].legendary ? 70 : 55;   // raid bosses never scale to you
        (void)top;
        boss.moves = MovesAtLevel(AllSpecies()[boss.species], boss.level);
        boss.shiny = r.shiny;
        boss.boss = 1.0f + (coopRaid ? 4.0f : 4.5f) * std::max(1, onField);   // a long fight: every one of yours is hitting it
        g.cpuTeam = {boss};
        std::vector<SideSetup> v;
        for (int p = 0; p < (coopRaid ? 2 : 1); p++) {
            int n = (int)first[p].size();
            for (int k = 0; k < n; k++) {
                SideSetup sd;
                sd.team = {first[p][k]};
                if (!coopRaid) for (int j = k; j < (int)second.size(); j += n) sd.team.push_back(second[j]);   // Team 2, spread over the slots
                sd.squad = p;
                sd.team_ = 0;
                sd.earnXp = true;
                sd.level = AILevel::Hard;
                sd.cpu = k != 0;
                sd.input = k == 0 ? p : -1;
#ifdef ARENA_AUTOTEST
                if (gAutoPlayP1) { sd.cpu = true; sd.input = -1; }
#endif
                sd.label = k == 0 ? (coopRaid ? g.coopP[p].name : g.profile.name) : "Ally";
                sd.tag = coopRaid ? (p == 0 ? "P1" : "P2") : "YOU";
                v.push_back(sd);
            }
        }
        SideSetup bs;
        bs.team = g.cpuTeam;
        bs.cpu = true;
        bs.level = AILevel::Hard;
        bs.label = "Raid Boss";
        bs.tag = "RAID";
        bs.team_ = 1;
        v.push_back(bs);
        su.side = v;
        su.raid = true;
        if (coopRaid) { g.coopSave.raidDay = RaidToday(); if (!g.on.active || g.on.me == 0) SaveCoop(g.coopSave); }   // one try a day, win or lose
        else { g.profile.raidDay = RaidToday(); SaveProfile(g.profile); }
        su.catchable = true;
        su.trainerBattle = true;
        su.arena = ArenaKind::Stadium;
    }
    if ((g.mode == Mode::Training || g.mode == Mode::CoopTraining) && !g.raid) {
        // Wild Pokemon are never more than 1 level above your strongest Pokemon in the battle.
        int top = 1;
        for (const auto& sd : su.side)
            if (sd.team_ == 0 || &sd == &su.side[0])
                for (const auto& c : sd.team) top = std::max(top, c.level);
        for (auto& sd : su.side) {
            if (sd.team_ == 0 || &sd == &su.side[0]) continue;
            for (auto& c : sd.team)
                if (c.level > top + 1 && c.boss <= 0) {
                    c.level = std::max(2, top + 1);
                    c.moves = MovesAtLevel(AllSpecies()[c.species], c.level);
                }
        }
        for (auto& c : g.cpuTeam)
            if (c.level > top + 1 && c.boss <= 0) { c.level = std::max(2, top + 1); c.moves = MovesAtLevel(AllSpecies()[c.species], c.level); }
    }
    if (g.tower)   // the Battle Tower: everyone fights at full health (and the profile's HP isn't touched)
        for (auto& sd : su.side)
            if (sd.team_ == 0 || &sd == &su.side[0]) for (auto& c : sd.team) c.hpFrac = 1;
    if (IsStoryMode(g.mode)) {   // Story: where it happens, day or night, and the season's weather
        int clock = StoryClock(g);
        StageDef stv;
        bool hasStage = (g.mode == Mode::Story || g.mode == Mode::CoopStory) && (g.tower || g.owFight || g.legend >= 0 || (g.stageIdx >= 0 && g.stageIdx < (int)Story().stages.size()));
        if (hasStage) stv = ActiveStage(g);
        const StageDef* st = hasStage ? &stv : nullptr;
        if (st && (st->IsGym() || st->IsLeague()) && st->towerFloor <= 0) {
            su.leader = st->trainer;   // the leader's picture and cut-ins
            // Gym Leaders, the Elite Four and Champions carry 3 Potions (no other trainer uses items)
            int lv = 0;
            for (const auto& c : g.cpuTeam) lv = std::max(lv, c.level);
            const char* pot = st->IsChampion() ? "Full Restore" : st->IsLeague() || lv >= 40 ? "Hyper Potion" : lv >= 20 ? "Super Potion" : "Potion";
            for (auto& sd : su.side)
                if (sd.cpu && sd.team_ != 0 && &sd != &su.side[0]) { sd.hot[0] = pot; sd.hotN[0] = 3; }
        }
        // your item hotkeys (the Bag's Medicine), each player their own
        for (int i = 0; i < (int)su.side.size(); i++) {
            SideSetup& sd = su.side[i];
            if (sd.cpu && !(i == 0 && g.mode == Mode::Story)) continue;
            const Profile* P = nullptr;
            if (g.mode == Mode::CoopStory || g.mode == Mode::CoopTraining) { if (sd.input == 0 || sd.input == 1) P = &g.coopP[sd.input]; }
            else if (i == 0) P = &g.profile;
            if (!P) continue;
            for (int k = 0; k < 4; k++) {
                const ItemDef* d = FindItem(P->hotItems[k]);
                if (!d || !(IsMedicine(*d) || IsBall(*d))) continue;
                sd.hot[k] = d->name;
                sd.hotN[k] = BagCount(*P, d->name);
            }
            if (su.catchable && !su.trainerBattle && !su.raid) {   // a wild battle with no ball on a hotkey: your best one goes on a free slot
                bool any = false;
                for (int k = 0; k < 4; k++) { const ItemDef* d = FindItem(sd.hot[k]); any |= d && IsBall(*d) && sd.hotN[k] > 0; }
                for (const char* b : {"Ultra Ball", "Great Ball", "Poke Ball"}) {
                    if (any || BagCount(*P, b) <= 0) continue;
                    for (int k = 0; k < 4 && !any; k++) if (sd.hot[k].empty() || sd.hotN[k] <= 0) { sd.hot[k] = b; sd.hotN[k] = BagCount(*P, b); any = true; }
                }
            }
        }
        su.season = SeasonOf(clock);
        if (su.arena == ArenaKind::Stadium) {   // trainers fight where they stand; wild Pokemon where they live
            if (st) su.venue = (uint8_t)(st->venue.empty() ? VenueForPlace(st->name) : VenueByName(st->venue));
            else {
                int area = TrainingAreaIndex(g);
                bool grass = false;
                if (area >= 0) for (const auto& e : WildAreas()[area].mons) grass |= e.method == "walk";
                su.venue = (uint8_t)(area < 0 ? RandomWildVenue(GetRandomValue(0, 999)) : !grass ? Venue::Beach : VenueForPlace(WildAreas()[area].name));
            }
        }
        bool indoors = su.arena != ArenaKind::Stadium ? (st ? st->Indoors() : true) : VenueIndoors((Venue)su.venue);
        if (!indoors) {   // gyms and buildings are lit and dry; outdoors gets the time of day and weather
            su.night = IsNight(clock);
            su.daylight = !su.night;
            bool world = g.ow.active || g.ow.wildBattle || g.owFight;   // out in the walked world: the weather you were walking in
            su.weather = world ? WorldBattleWeather(g, clock) : RollSeasonWeather(SeasonOf(clock));
        }
        if (su.arena == ArenaKind::Ice || su.arena == ArenaKind::GiantChasm) su.weather = Weather::Snow;   // it never stops snowing in Lorelei's chamber
        if (g.legend >= 0) {   // a legendary brings its own weather
            su.night = false;
            su.daylight = true;
            su.weather = su.arena == ArenaKind::Seafoam || su.arena == ArenaKind::GiantChasm ? Weather::Snow
                       : su.arena == ArenaKind::PowerPlant || su.arena == ArenaKind::MarineCave || su.arena == ArenaKind::Dragonspiral ? Weather::Rain
                       : su.arena == ArenaKind::Ember ? Weather::Sun : su.arena == ArenaKind::DesertRuins ? Weather::Sand : Weather::None;
        }
    }
    g.battle.Start(su);
    g.paused = false;
    g.pauseSel = 0;
    g.wheel = g.wheelRelease = false;
    g.swapHoldT = 0;
    g.pendingSwap = -1;
    g.screen = Screen::Battle;
}

static void DrawEntryInfo(const RosterEntry& e, int x, int y) {
    const SpeciesDef& sp = AllSpecies()[e.species];
    DrawRectangle(x, y, 42, 42, UI_PANEL_LIGHT);
    DrawSpeciesFit(ShinyIdx(e.species, e.shiny), VIEW_FRONT, x + 21, y + 40, 38);
    if (e.shiny) DrawShinyStar((float)x + 5, (float)y + 5, 3.5f);
    DrawTextShadow(sp.name.c_str(), x + 48, y + 1, 10, UI_TEXT);
    GDrawText(TextFormat("Lv%d", e.level), x + 52 + TextW(sp.name.c_str(), 10), y + 1, 10, UI_GOLD);
    int bx = x + 48;
    bx += DrawTypeBadge(sp.type1, bx, y + 13) + 2;
    if (sp.type2 != Type::None) DrawTypeBadge(sp.type2, bx, y + 13);
    if (sp.floats) GDrawText("hovers", x + 48, y + 28, 10, UI_DIM);
    if (!e.item.empty()) GDrawText(TextFormat("@ %s", e.item.c_str()), x + 48, y + 38, 10, Color{140, 220, 255, 255});
    int sx = x + 150;
    float mul = StatMul(e.level);
    auto sv = [&](int b) { return (int)std::lround(Scaled(b) * mul); };
    DrawStatBar("HP", sv(sp.hp), sx - 4, y, 24);
    DrawStatBar("ATK", sv(sp.atk), sx - 4, y + 11, 24);
    DrawStatBar("DEF", sv(sp.def), sx - 4, y + 22, 24);
    DrawStatBar("SPD", (int)std::lround(Scaled(sp.spd)), sx - 4, y + 33, 24);
    DrawStatBar("SpA", sv(sp.SpAtk()), sx + 74, y + 11, 24);
    DrawStatBar("SpD", sv(sp.SpDef()), sx + 74, y + 22, 24);
    for (int k = 0; k < 4; k++) {
        int my = y + 48 + k * 11;
        if (k < (int)e.moves.size()) DrawMoveLine(e.moves[k], x, my, 296, false);
        else GDrawText("-", x + 10, my, 10, UI_DIM);
    }
}

void DrawSelect(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    const auto& ros = ViewRoster(g);
    int vp = g.shared ? 0 : g.pickPhase;
    std::string head = TextFormat("CHOOSE YOUR TEAM  (1-%d)", g.teamSize);
    if (g.raid && !IsCoop(g.mode)) {
        RaidInfo ri = RaidOfDay(RaidToday());
        head = g.raidTeam1.empty() ? "DAILY RAID: PICK TEAM 1" : "DAILY RAID: PICK TEAM 2";
        DrawTextCentered(TextFormat("vs %s%s - all of a team fights at once; you steer one (Swap: switch), the CPU the rest", ri.species >= 0 ? AllSpecies()[ri.species].name.c_str() : "?",
                                    ri.shiny ? " (shiny!)" : ""), VW / 2, 24, 10, Color{255, 150, 220, 255}, false);
    }
    if (!g.shared) head = std::string(vp == 0 ? "PLAYER 1" : "PLAYER 2") + ": CHOOSE YOUR TEAM";
    if (IsCoop(g.mode)) {
        head = std::string(vp == 0 ? "PLAYER 1" : "PLAYER 2") + (g.raid ? ": PICK UP TO 6" : ": PICK UP TO 3");
        std::string vs;
        if (g.mode == Mode::CoopStory) {
            const StageDef st = ActiveStage(g);
            int want = std::min(MAX_TEAM_SIZE, std::max(3, (int)st.team.size() + 2));
            if (st.IsLegend()) vs = TextFormat("vs %s - 2 on 1, it has a huge HP pool. Weaken it and throw your balls!", st.trainer.c_str());
            else vs = TextFormat("vs %s - %d Pokemon%s", st.trainer.c_str(), want,
                            st.towerFloor > 0 ? TextFormat(" - Battle Tower floor %d", st.towerFloor)
                            : IsRematch(g) && st.IsBoss() ? " - REMATCH (scaled to your level)"
                            : st.IsGym() ? TextFormat(" - %s Badge", st.badge.c_str()) : st.IsChampion() ? " - CHAMPIONSHIP" : st.IsLeague() ? " - Elite Four" : "");
        } else if (g.raid) {
            RaidInfo ri = RaidOfDay(RaidToday());
            vs = TextFormat("DAILY RAID vs %s%s - all 12 fight at once; you steer one (Swap: switch)", ri.species >= 0 ? AllSpecies()[ri.species].name.c_str() : "?", ri.shiny ? " (shiny!)" : "");
        } else if (g.rareBattle && g.rareSpecies >= 0) {
            vs = TextFormat("RARE in %s: a wild %s - weaken it to catch!   X / Tab: area", TrainAreaName(g).c_str(), AllSpecies()[g.rareSpecies].name.c_str());
        } else {
            vs = TextFormat("Training in %s - up to 4 wild Pokemon - X / Tab: choose where to train", TrainAreaName(g).c_str());
        }
        DrawTextCentered(vs.c_str(), VW / 2, 20, 10, (g.rareBattle && g.mode == Mode::CoopTraining) ? Color{255, 150, 230, 255} : UI_TEXT, false);
    }
    DrawTextCentered(head.c_str(), VW / 2, IsCoop(g.mode) ? 0 : 8, 20, UI_GOLD);
    if (IsCoop(g.mode)) {   // who has locked in their team
        bool r[2] = {g.ready[0], g.ready[1]};
        if (g.on.active) { r[g.on.me] = g.ready[g.on.me] || g.on.picksSent; r[1 - g.on.me] = g.on.partnerPicksIn; }
        DrawReadyCount(VW - 4, 3, r[0], r[1]);
    }

    int n = (int)ros.size();
    int rows = (n + GRID_COLS - 1) / GRID_COLS;
    int scroll = g.scroll[vp];
    auto foes = IsStoryMode(g.mode) ? SelFoes(g) : std::vector<int>{};
    for (int i = 0; i < n; i++) {
        int r = i / GRID_COLS;
        if (r < scroll || r >= scroll + GRID_ROWS_VISIBLE) continue;
        const RosterEntry& e = ros[i];
        const SpeciesDef& sp = AllSpecies()[e.species];
        int x = 10 + (i % GRID_COLS) * 104, y = 32 + (r - scroll) * 54, w = 100, h = 50;
        DrawPanel(x, y, w, h, UI_PANEL, Color{80, 74, 104, 255});
        DrawRectangle(x + 1, y + 1, w - 2, 3, TypeColor(sp.type1));
        DrawEllipse(x + 22, y + 44, 13, 3, Fade(BLACK, 0.3f));
        DrawSpeciesFit(ShinyIdx(e.species, e.shiny), VIEW_FRONT, x + 22, y + 46, 38);
        if (e.shiny) DrawShinyStar((float)x + 6, (float)y + 6, 3.5f);
        GDrawText(sp.name.c_str(), x + 42, y + 8, 10, UI_TEXT);
        DrawTypeBadge(sp.type1, x + 42, y + 21);
        if (e.ownedId >= 0 || e.level != FREE_PLAY_LEVEL) GDrawText(TextFormat("Lv%d", e.level), x + 42, y + 35, 10, UI_GOLD);
        if (IsStoryMode(g.mode)) DrawMatchupMarks(SelMatchup(e.species, e.moves, foes), x + 74, y + 37);
        if (IsStoryMode(g.mode)) {
            DrawHpBar(x + 42, y + 46 - 2, 52, 2, e.hpFrac);
            if (e.hpFrac <= 0) {
                DrawRectangle(x + 1, y + 1, w - 2, h - 2, Fade(UI_INK, 0.6f));
                GDrawText("FAINTED", x + 30, y + 20, 10, Color{255, 120, 100, 255});
            }
        }
        if (g.raid && std::find(g.raidTaken.begin(), g.raidTaken.end(), i) != g.raidTaken.end()) {   // already on Team 1
            DrawRectangle(x + 1, y + 1, w - 2, h - 2, Fade(UI_INK, 0.55f));
            DrawRectangle(x + w - 24, y + h - 14, 22, 11, UI_GOLD);
            GDrawText("T1", x + w - 21, y + h - 13, 10, UI_INK);
        }
        for (int p = 0; p < 2; p++) {
            if (!g.shared && p != vp) continue;
            auto it = std::find(g.picks[p].begin(), g.picks[p].end(), i);
            if (it == g.picks[p].end()) continue;
            int slot = (int)(it - g.picks[p].begin()) + 1;
            int bx = x + w - 13 - p * 13, by = y + h - 14;
            DrawRectangle(bx, by, 11, 11, TeamColor(p));
            GDrawText(TextFormat("%d", slot), bx + 3, by + 1, 10, UI_INK);
        }
    }
    if (rows > GRID_ROWS_VISIBLE) {
        if (scroll > 0) DrawTextCentered("^ more ^", VW / 2, 26, 10, UI_DIM, false);
        if (scroll + GRID_ROWS_VISIBLE < rows) DrawTextCentered("v more v", VW / 2, 186, 10, UI_DIM, false);
    }
    for (int p = 0; p < 2; p++) {
        if (p >= HumanPickers(g) || (!g.shared && p != vp)) continue;
        int i = g.cursor[p];
        int r = i / GRID_COLS;
        if (r < scroll || r >= scroll + GRID_ROWS_VISIBLE || i >= n) continue;
        int x = 10 + (i % GRID_COLS) * 104, y = 32 + (r - scroll) * 54;
        int inset = (p == 1 && g.cursor[0] == g.cursor[1]) ? 3 : 0;
        if (((int)(g.t * 4) % 2) == 0 || g.ready[p])
            DrawRectangleLinesEx({(float)(x - 2 + inset), (float)(y - 2 + inset), (float)(104 - inset * 2), (float)(54 - inset * 2)}, 2, TeamColor(p));
    }

    // Bottom panels
    for (int p = 0; p < 2; p++) {
        int x = p == 0 ? 4 : 324, y = 196, w = 312, h = 160;
        DrawPanel(x, y, w, h);
        DrawRectangle(x, y, w, 2, TeamColor(p));
        bool human = p < HumanPickers(g);
        std::string coopWho = IsCoop(g.mode) ? (p == 0 ? "PLAYER 1: " : "PLAYER 2: ") + g.coopP[p].name : "";
        const char* who = IsCoop(g.mode) ? coopWho.c_str() : p == 0 ? (g.mode == Mode::Watch ? "BLUE TEAM (CPU-controlled)" : (IsStoryMode(g.mode) ? g.profile.name.c_str() : "PLAYER 1"))
                                 : (TwoHumans(g) ? (g.mode == Mode::Doubles && g.doublesKind == 1 ? "PLAYER 2 (your partner)" : "PLAYER 2")
                                                 : (g.mode == Mode::Doubles ? "THE CPUs" : "OPPONENT"));
        DrawTextShadow(who, x + 6, y + 6, 10, TeamColor(p));

        if (p == 1 && IsStoryMode(g.mode) && !IsCoop(g.mode)) {
            if (g.mode == Mode::Story) {
                const StageDef st = ActiveStage(g);
                DrawTextShadow(st.trainer.c_str(), x + 6, y + 24 + (TextW(st.trainer.c_str(), 20) > w - 12 ? 6 : 0), TextW(st.trainer.c_str(), 20) > w - 12 ? 10 : 20, UI_TEXT);
                GDrawText(st.name.c_str(), x + 6, y + 48, 10, UI_DIM);
                GDrawText(st.IsLegend() ? "1 legendary - a boss with a huge HP pool" : TextFormat("%d Pokemon", (int)std::min<size_t>(st.team.size(), MAX_TEAM_SIZE)), x + 6, y + 66, 10, st.IsLegend() ? UI_GOLD : UI_TEXT);
                if (st.IsLegend()) GDrawText("A few levels above your team", x + 6, y + 80, 10, UI_GOLD);
                else if (st.towerFloor > 0) GDrawText(TextFormat("Tower floor %d: levels scale to your team", st.towerFloor), x + 6, y + 80, 10, UI_GOLD);
                else if (IsRematch(g) && st.IsBoss()) GDrawText("REMATCH: levels scale to your team", x + 6, y + 80, 10, UI_GOLD);
                else if (st.baseLevel > 0) GDrawText(TextFormat("Levels around %d", st.baseLevel), x + 6, y + 80, 10, UI_TEXT);
                else GDrawText(TextFormat("Levels around your team %s", st.levelOffset >= 0 ? TextFormat("+%d", st.levelOffset) : TextFormat("%d", st.levelOffset)), x + 6, y + 80, 10, UI_TEXT);
                GDrawText(TextFormat("CPU skill: %s", AILevelName(st.cpu)), x + 6, y + 94, 10, UI_TEXT);
                GDrawText(TextFormat("Arena: %s", st.arena == ArenaKind::Stadium ? VenueName(st.venue.empty() ? VenueForPlace(st.name) : VenueByName(st.venue)) : ArenaName(st.arena)), x + 6, y + 108, 10, st.arena == ArenaKind::Stadium ? UI_TEXT : UI_GOLD);
                if (st.dbl) GDrawText(!st.partner.empty() ? TextFormat("DOUBLE BATTLE: %s fights with you!", st.partner.c_str())
                                                          : "DOUBLE BATTLE: picks 2, 4 and 6 fight beside you", x + 6, y + 136, 10, UI_GOLD);
                GDrawText(st.IsLegend() ? "It can't be knocked out: weaken it and throw balls in battle" : st.towerFloor > 0 ? "Battle Tower: everyone fights at full health" : st.IsChampion() ? "The Championship!" : st.IsLeague() ? "Elite Four: lose and the League starts over"
                         : st.IsGym() ? "Gym battle" : "Trainer battle: catch wild ones in training", x + 6, y + 122, 10, UI_DIM);
            } else {
                if (g.raid) {
                    RaidInfo ri = RaidOfDay(RaidToday());
                    DrawTextShadow("DAILY RAID", x + 6, y + 24, 20, Color{255, 120, 200, 255});
                    if (ri.species >= 0) {
                        DrawRectangle(x + 200, y + 24, 80, 80, UI_PANEL_LIGHT);
                        DrawSpeciesFit(ShinyIdx(ri.species, ri.shiny), VIEW_FRONT, x + 240, y + 102, 72);
                        GDrawText(TextFormat("%s%s - a boss with a huge HP pool", AllSpecies()[ri.species].name.c_str(), ri.shiny ? " (shiny!)" : ""), x + 6, y + 50, 10, UI_TEXT);
                    }
                    GDrawText("Team 1 all fights at once: you steer one,", x + 6, y + 66, 10, UI_TEXT);
                    GDrawText("the CPU the rest. Swap: take over another.", x + 6, y + 78, 10, UI_TEXT);
                    GDrawText("Team 2 comes in when Team 1 is all down.", x + 6, y + 92, 10, UI_TEXT);
                    GDrawText("Win, then try to catch it! One try a day.", x + 6, y + 108, 10, UI_GOLD);
                } else if (g.rareBattle && g.rareSpecies >= 0) {
                    DrawTextShadow("RARE ENCOUNTER!", x + 6, y + 24, 20, Color{255, 150, 230, 255});
                    DrawRectangle(x + 200, y + 24, 80, 80, UI_PANEL_LIGHT);
                    DrawSpeciesFit(g.rareSpecies, VIEW_FRONT, x + 240, y + 102, 72);
                    GDrawText(TextFormat("A wild %s appeared!", AllSpecies()[g.rareSpecies].name.c_str()), x + 6, y + 50, 10, UI_TEXT);
                    GDrawText("It's a few levels above your team.", x + 6, y + 64, 10, UI_DIM);
                    GDrawText("Weaken it, then throw a ball from your", x + 6, y + 82, 10, UI_GOLD);
                    GDrawText("item hotkeys - don't knock it out!", x + 6, y + 94, 10, UI_GOLD);
                    GDrawText(TextFormat("Poke Balls: %d", BagCount(g.profile, "Poke Ball")), x + 6, y + 112, 10, UI_TEXT);
                    GDrawText(TextFormat("In %s   (X / Tab: area)", TrainAreaName(g).c_str()), x + 6, y + 126, 10, Color{140, 230, 150, 255});
                } else {
                    DrawTextShadow("Training battle", x + 6, y + 24, 20, UI_TEXT);
                    GDrawText(TextFormat("In %s", TrainAreaName(g).c_str()), x + 6, y + 50, 10, Color{140, 230, 150, 255});
                    GDrawText(TextFormat("Up to %d wild Pokemon - never more than you bring", std::max(2, Story().trainingMaxFoes)), x + 6, y + 66, 10, UI_TEXT);
                    GDrawText(TextFormat("Training wins: %d", g.profile.trainingWins), x + 6, y + 80, 10, UI_TEXT);
                    GDrawText("Throw Poke Balls from your item hotkeys in battle!", x + 6, y + 96, 10, UI_GOLD);
                    GDrawText("X / Tab: choose where to train", x + 6, y + 112, 10, UI_GOLD);
                }
            }
            continue;
        }
        if (!human) {
            if (g.mode == Mode::Doubles) {
                GDrawText("Your CPU partner and both rival CPUs", x + 6, y + 60, 10, UI_DIM);
                GDrawText("get random teams the same size as yours.", x + 6, y + 72, 10, UI_DIM);
            } else
                GDrawText(g.ready[0] ? "Picking..." : "Waiting for you...", x + 6, y + 60, 10, UI_DIM);
            continue;
        }
        bool partnerView = g.on.active && p != g.on.me;
        const std::vector<int>& shown = partnerView ? (g.on.partnerPicksIn ? g.on.partnerPicks : g.on.partnerPickView) : g.picks[p];
        const auto& pr = g.roster[g.shared ? 0 : p];
        for (int s = 0; s < g.teamSize; s++) {
            int sx = x + w - 6 - (g.teamSize - s) * 23, sy = y + 4;
            DrawRectangle(sx, sy, 22, 22, UI_PANEL_LIGHT);
            RectLines(sx, sy, 22, 22, Color{90, 84, 116, 255});
            if (s < (int)shown.size() && shown[s] >= 0 && shown[s] < (int)pr.size()) DrawSpeciesFit(ShinyIdx(pr[shown[s]].species, pr[shown[s]].shiny), VIEW_FRONT, sx + 11, sy + 21, 20);
        }
        if (partnerView || (IsCoop(g.mode) && !g.shared && p != vp && g.ready[p])) {
            // The partner's team (online: as they pick it), so you can cover each other's weaknesses
            int ly = y + 30;
            for (int k = 0; k < (int)shown.size() && k < 3; k++) {
                if (shown[k] < 0 || shown[k] >= (int)pr.size()) continue;
                const RosterEntry& e = pr[shown[k]];
                const SpeciesDef& sp = AllSpecies()[e.species];
                int ry = ly + k * 26;
                DrawRectangle(x + 6, ry, w - 12, 24, UI_PANEL_LIGHT);
                DrawSpeciesFit(ShinyIdx(e.species, e.shiny), VIEW_FRONT, x + 18, ry + 23, 22);
                GDrawText(sp.name.c_str(), x + 34, ry + 2, 10, UI_TEXT);
                GDrawText(TextFormat("Lv%d", e.level), x + 38 + TextW(sp.name.c_str(), 10), ry + 2, 10, UI_GOLD);
                int bx = x + 34;
                bx += DrawTypeBadge(sp.type1, bx, ry + 12) + 2;
                if (sp.type2 != Type::None) bx += DrawTypeBadge(sp.type2, bx, ry + 12) + 2;
                for (size_t q = 0; q < e.moves.size() && q < 4; q++) {   // move types as little chips
                    DrawRectangle(x + w - 50 + (int)q * 10, ry + 8, 8, 8, TypeColor(e.moves[q].type));
                }
            }
            if (shown.empty()) GDrawText("No picks yet...", x + 6, ly + 4, 10, UI_DIM);
            const char* st;
            if (partnerView) {
                st = g.on.partnerPicksIn ? "Ready! Waiting for you..." : "Picking on their screen...";
                int pc = g.on.partnerSelCursor;
                if (!g.on.partnerPicksIn && pc >= 0 && pc < (int)pr.size())
                    GDrawText(TextFormat("Looking at: %s", AllSpecies()[pr[pc].species].name.c_str()), x + 6, y + h - 28, 10, UI_DIM);
            } else {
                st = "READY!";
            }
            GDrawText(st, x + 6, y + h - 14, 10, (partnerView ? g.on.partnerPicksIn : true) ? UI_GOLD : UI_DIM);
            continue;
        }
        if (!g.shared && p != vp && !g.ready[p]) {
            GDrawText("Waiting for Player 1...", x + 6, y + 60, 10, UI_DIM);
            continue;
        }
        if (g.cursor[p] < (int)pr.size()) DrawEntryInfo(pr[g.cursor[p]], x + 6, y + 30);
        const char* status;
        char buf[96];
        bool tp = TwoHumans(g);
        if (g.ready[p]) status = "READY!";
        else if ((int)g.picks[p].size() >= g.teamSize) status = "Team full - confirm again to lock in";
        else if (!g.picks[p].empty()) {
            std::snprintf(buf, sizeof buf, "Pick: %s   Lock in now: %s   Undo: %s%s", ConfirmLabel(p, tp), SwapKeyLabel(p, tp), BackLabel(p, tp), SelBoxAllowed(g, p) ? "   Box: R" : "");
            status = buf;
        } else {
            std::snprintf(buf, sizeof buf, "Pick: %s   Undo/back: %s%s", ConfirmLabel(p, tp), BackLabel(p, tp), SelBoxAllowed(g, p) ? "   Box: R" : "");
            status = buf;
        }
        GDrawText(status, x + 6, y + h - 14, 10, g.ready[p] ? UI_GOLD : UI_DIM);
    }
    if (g.startDelay >= 0) {
        DrawRectangle(0, 150, VW, 40, Fade(UI_INK, 0.8f));
        DrawTextCentered("BATTLE START!", VW / 2, 160, 20, UI_GOLD);
    }
    if (g.areaPick) DrawAreaPick(g);
    for (int p = 0; p < 2; p++) if (g.selBox[p] >= 0) { DrawSelBox(g, p); break; }
}

// ------------------------------------------------------------------ battle

namespace {
// Solo Story Mode: hold Swap for the Quick Select wheel (the battle pauses), tap it to swap to
// the next Pokemon as before.
constexpr float WHEEL_HOLD = 0.45f;
bool WheelMode(const Game& g) { return (g.mode == Mode::Story || g.mode == Mode::Training) && !g.coop && !g.on.active; }

// The wheel's slots: every Pokemon of your team, in order, around a circle (slot 0 at the top).
int WheelSlotAt(int n, Vector2 v) {
    if (n <= 0) return -1;
    float a = std::atan2(v.x, -v.y);   // 0 = up, clockwise
    if (a < 0) a += 2 * PI;
    return (int)std::lround(a / (2 * PI / n)) % n;
}
bool WheelPickable(const Side& sd, int i) {
    const Fighter& f = sd.team[i];
    return i != sd.active && f.hp > 0 && !f.retired && !f.lent && f.state == FState::Benched;
}

// Returns true while the wheel is open (the battle waits).
bool UpdateWheel(Game& g, PlayerInput& p, float dt) {
    const Side& sd = g.battle.GetSide(0);
    int n = (int)sd.team.size();
    if (g.wheel) {
        Vector2 v = p.stick2;
        if (std::sqrt(v.x * v.x + v.y * v.y) < 0.5f) v = p.move;
        if (std::sqrt(v.x * v.x + v.y * v.y) >= 0.5f) {
            int s = WheelSlotAt(n, v);
            if (s != g.wheelSel) { g.wheelSel = s; PlaySfx(Sfx::MenuMove); }
        }
        if (p.swapTo >= 0 && p.swapTo < n) g.wheelSel = p.swapTo;
        bool go = p.confirm || p.swapTo >= 0;
        if (p.back) { g.wheel = false; g.wheelRelease = true; PlaySfx(Sfx::MenuBack); p = PlayerInput(); return false; }
        if (go && g.wheelSel >= 0 && g.wheelSel < n) {
            if (!WheelPickable(sd, g.wheelSel)) {
                PlaySfx(Sfx::MenuBack);
                return true;
            }
            int to = g.wheelSel;
            g.wheel = false;
            g.wheelRelease = true;
            p = PlayerInput();
            p.swapTo = to;
            PlaySfx(Sfx::MenuOk);
            return false;
        }
        return true;
    }
    if (g.wheelRelease) {   // the wheel just closed: wait until Swap is let go
        if (!p.swapHeld) g.wheelRelease = false;
        p.swap = false;
        g.swapHoldT = 0;
        return false;
    }
    if (p.swapHeld) {
        p.swap = false;   // decided when it's let go (a tap) or held (the wheel)
        g.swapHoldT += dt;
        if (g.swapHoldT >= WHEEL_HOLD && sd.Cur().state == FState::Active && sd.AliveCount() > 1) {
            g.wheel = true;
            g.wheelSel = -1;
            for (int k = 1; k <= n; k++) if (WheelPickable(sd, (sd.active + k) % n)) { g.wheelSel = (sd.active + k) % n; break; }
            g.swapHoldT = 0;
            PlaySfx(Sfx::MenuOk);
            return true;
        }
    } else if (g.swapHoldT > 0) {
        if (g.swapHoldT < WHEEL_HOLD) p.swap = true;   // a tap: the next Pokemon, as always
        g.swapHoldT = 0;
    }
    return false;
}
} // namespace

void (*gBattleStepHook)(const Battle&, float, const PlayerInput*) = nullptr;

void UpdateBattleScreen(Game& g, float dt) {
    if (OnlineBattleUpdate(g, dt)) return;
    bool tp = TwoHumans(g);
    PlayerInput in[2] = {ReadInput(0, tp), ReadInput(1, tp)};
    if (WheelMode(g) && !g.paused) {
        if (UpdateWheel(g, in[0], dt)) return;
        // A chosen swap (wheel or keys 1-6) waits for the cooldown instead of being lost.
        const Side& sd = g.battle.GetSide(0);
        if (in[0].swapTo >= 0 && WheelPickable(sd, in[0].swapTo) && (sd.swapCd > 0 || sd.Cur().state != FState::Active)) {
            g.pendingSwap = in[0].swapTo;
            g.pendingSwapT = 4.0f;
        }
        if (g.pendingSwap >= 0) {
            g.pendingSwapT -= dt;
            if (g.pendingSwapT <= 0 || g.pendingSwap >= (int)sd.team.size() || !WheelPickable(sd, g.pendingSwap)) g.pendingSwap = -1;
            else if (sd.swapCd <= 0 && sd.Cur().state == FState::Active) { in[0].swapTo = g.pendingSwap; g.pendingSwap = -1; }
        }
    }
    if (in[0].pause || in[1].pause) {
        g.paused = !g.paused;
        g.pauseSel = 0;
        PlaySfx(g.paused ? Sfx::MenuOk : Sfx::MenuBack);
    }
    if (g.paused) {
        PlayerInput m = MenuInput();
        if (m.up || m.down) { g.pauseSel = 1 - g.pauseSel; PlaySfx(Sfx::MenuMove); }
        if (m.confirm) {
            if (g.pauseSel == 0) g.paused = false;
            else if (IsStoryMode(g.mode)) { g.paused = false; gShake = {0, 0}; Toast(g, "Left the battle - nothing was saved"); EnterStoryHub(g); }
            else GoTitle(g);
        }
        return;
    }
    float stepDt = g.on.active ? dt : dt * g.battle.AnimTimeScale();
    g.battle.Update(stepDt, in);
    if (gBattleStepHook && !g.on.active) gBattleStepHook(g.battle, stepDt, in);   // the overworld: a partner is watching
    if (g.battle.Finished()) {
        gShake = {0, 0};
        if (IsStoryMode(g.mode)) ProcessStoryBattleEnd(g);
        else {
            g.resultSel = 0;
            g.screen = Screen::Results;
        }
    }
}

void DrawBattleScreen(const Game& g) {
    g.battle.Draw();
    if (g.on.active) {
        GDrawText(TextFormat("%dms", net::PingMs()), VW - 34, VH - 12, 10, Fade(UI_DIM, 0.7f));
        if (g.on.stallT > 0.5f) DrawTextCentered("Waiting for your partner's connection...", VW / 2, 150, 10, UI_GOLD, false);
        if (g.on.leaveAsk) {
            DrawRectangle(0, 130, VW, 50, Fade(UI_INK, 0.85f));
            DrawTextCentered("Leave this battle? Your partner goes back to the hub too, and nothing is saved.", VW / 2, 140, 10, UI_TEXT, false);
            DrawTextCentered("Enter: leave     Esc: keep playing", VW / 2, 158, 10, UI_GOLD, false);
        }
        return;
    }
    if (g.wheel) {   // the Quick Select wheel
        const Side& sd = g.battle.GetSide(0);
        int n = (int)sd.team.size();
        DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.6f));
        Vector2 c{VW / 2.0f, 172};
        float R = 100;
        DrawCircleV(c, R + 34, Fade(Color{20, 22, 40, 255}, 0.85f));
        DrawCircleLines((int)c.x, (int)c.y, R + 34, Fade(UI_GOLD, 0.6f));
        if (g.wheelSel >= 0 && g.wheelSel < n) {   // the pointer, under the slots and the middle
            float a = g.wheelSel * 2 * PI / n;
            DrawLineEx(c, Vector2{c.x + std::sin(a) * R, c.y - std::cos(a) * R}, 3, Fade(UI_GOLD, 0.7f));
        }
        DrawCircleV(c, 40, Color{30, 34, 60, 255});
        DrawCircleLines((int)c.x, (int)c.y, 40, Fade(UI_GOLD, 0.35f));
        for (int i = 0; i < n; i++) {
            float a = i * 2 * PI / n;
            Vector2 p{c.x + std::sin(a) * R, c.y - std::cos(a) * R};
            const Fighter& f = sd.team[i];
            bool sel = g.wheelSel == i, cur = i == sd.active;
            bool ok = WheelPickable(sd, i);
            DrawCircleV(p, 27, sel ? Color{255, 214, 60, 255} : (cur ? Color{90, 150, 230, 255} : Color{60, 64, 96, 255}));
            DrawCircleV(p, 24, ok || cur ? Color{36, 40, 70, 255} : Color{40, 30, 36, 255});
            DrawSpeciesFit(ShinyIdx(f.RealSpecies(), f.shiny), VIEW_FRONT, (int)p.x, (int)p.y + 14, 30, f.hp > 0 ? WHITE : Color{90, 90, 100, 255}, true);
            DrawCircleV(Vector2{p.x - 21, p.y - 19}, 7, sel ? Color{255, 214, 60, 255} : Color{24, 26, 46, 255});
            DrawTextCentered(TextFormat("%d", i + 1), (int)p.x - 21, (int)p.y - 24, 10, sel ? UI_INK : UI_GOLD, false);
            float hp = f.maxHp > 0 ? std::max(0.0f, f.hp) / f.maxHp : 0;
            DrawHpBar((int)p.x - 16, (int)p.y + 17, 32, 3, hp);
            if (cur) DrawTextCentered("IN BATTLE", (int)p.x, (int)p.y - 36, 10, Color{140, 190, 255, 255}, true);
            else if (f.hp <= 0) DrawTextCentered("FAINTED", (int)p.x, (int)p.y - 36, 10, Color{255, 120, 100, 255}, true);
        }
        if (g.wheelSel >= 0 && g.wheelSel < n) {
            const Fighter& f = sd.team[g.wheelSel];
            DrawTextCentered(f.sp->name.c_str(), (int)c.x, (int)c.y - 12, 10, UI_GOLD, false);
            DrawTextCentered(TextFormat("Lv%d", f.level), (int)c.x, (int)c.y + 2, 10, UI_TEXT, false);
        } else {
            DrawTextCentered("QUICK", (int)c.x, (int)c.y - 12, 10, UI_GOLD, false);
            DrawTextCentered("SELECT", (int)c.x, (int)c.y + 2, 10, UI_GOLD, false);
        }
        DrawRectangle(0, 314, VW, sd.swapCd > 0 ? 34 : 20, Fade(UI_INK, 0.92f));
        DrawTextCentered("Right stick: point   A: switch   B: cancel   (keys 1-6 work too)", VW / 2, 319, 10, UI_TEXT, false);
        if (sd.swapCd > 0) DrawTextCentered(TextFormat("Swap ready in %.1fs - it switches as soon as it can", sd.swapCd), VW / 2, 333, 10, UI_DIM, false);
        return;
    }
    if (!g.paused) return;
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.7f));
    DrawTextCentered("PAUSED", VW / 2, 70, 30, UI_GOLD);
    const char* items[2] = {"Resume", IsStoryMode(g.mode) ? "Leave battle (no XP)" : "Quit to title"};
    for (int i = 0; i < 2; i++) DrawMenuItem(items[i], VW / 2, 120 + i * 26, 20, g.pauseSel == i);
    DrawControlsHelp(60, 200, TwoHumans(g));
    GDrawText("Status: BRN burn, PSN/TOX poison, PAR paralysis, FRZ frozen, SLP asleep. Moves show x2 / x0.5 / x0 vs the foe.",
             60, 250, 10, UI_DIM);
    if (WheelMode(g))
        GDrawText("Quick Select: hold Swap (RT), point with the right stick, A to switch.   Keys 1-6: that slot.",
                 60, 264, 10, Color{140, 230, 150, 255});
}

// ------------------------------------------------------------------ quick results

void UpdateResults(Game& g) {
    PlayerInput in = MenuInput();
    if (in.up) { g.resultSel = (g.resultSel + 2) % 3; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.resultSel = (g.resultSel + 1) % 3; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    if (g.resultSel == 0) {
        BattleSetup su = g.battle.Setup();
        if (g.mode == Mode::Quick || g.mode == Mode::Watch) {
            // A fresh random opponent for the rematch.
            bool freePlay = g.mode == Mode::Watch || g.rosterSrc[0] < 0;
            int lv = su.side[0].team.empty() ? FREE_PLAY_LEVEL : su.side[0].team[0].level;
            su.side[1].team = RandomFreeTeam((int)su.side[1].team.size(), freePlay ? FREE_PLAY_LEVEL : lv, freePlay);
        }
        if (g.mode == Mode::Doubles) {  // fresh CPU teams
            bool freePlay = g.rosterSrc[0] < 0;
            int lv = su.side[0].team.empty() ? FREE_PLAY_LEVEL : su.side[0].team[0].level;
            for (auto& sd : su.side)
                if (sd.cpu) sd.team = RandomFreeTeam((int)sd.team.size(), freePlay ? FREE_PLAY_LEVEL : lv, freePlay);
        }
        g.battle.Start(su);
        g.paused = false;
        g.screen = Screen::Battle;
    } else if (g.resultSel == 1) {
        BeginTeamSelect(g);
    } else {
        GoTitle(g);
    }
}

void DrawResults(const Game& g) {
    g.battle.Draw(false);
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.75f));
    int w = g.battle.Winner();
    std::string wl = w == 2 ? "" : g.battle.TeamLabel(w);
    std::string msg = w == 2 ? "DRAW!" : wl + (wl.find(" & ") != std::string::npos ? " WIN!" : " WINS!");
    DrawTextCentered(msg.c_str(), VW / 2, 40, 30, w == 2 ? UI_TEXT : TeamColor(w));
    for (int s = 0; s < 2; s++) {
        auto fs = g.battle.TeamFighters(s);
        int cx = s == 0 ? VW / 4 : VW * 3 / 4;
        DrawTextCentered(g.battle.TeamLabel(s).c_str(), cx, 90, 10, TeamColor(s));
        int n = (int)fs.size();
        for (int i = 0; i < n; i++) {
            const Fighter& f = *fs[i];
            int sp = n > 4 ? 44 : (n > 3 ? 50 : 64);
            int x = cx - (n - 1) * sp / 2 + i * sp;
            int yy = (n > 3 && i % 2) ? 26 : 0;
            Color tint = f.hp > 0 ? WHITE : Color{90, 90, 100, 255};
            DrawSpeciesFit(f.species, VIEW_FRONT, x, 150 + yy, n > 3 ? 40 : 52, tint);
            DrawTextCentered(f.sp->name.c_str(), x, 154 + yy, 10, UI_TEXT);
            DrawTextCentered(f.hp > 0 ? TextFormat("%d HP", (int)std::ceil(f.hp)) : "fainted", x, 166 + yy, 10,
                             f.hp > 0 ? Color{120, 230, 130, 255} : UI_DIM);
        }
    }
    const char* items[3] = {"Rematch", "Change teams", "Title screen"};
    for (int i = 0; i < 3; i++) DrawMenuItem(items[i], VW / 2, 228 + i * 26, 20, g.resultSel == i);
}

// ------------------------------------------------------------------ 2v2 menu

bool TwoHumans(const Game& g) {
    return g.mode == Mode::TwoPlayer || (g.mode == Mode::Doubles && g.doublesKind > 0) ||
           g.mode == Mode::CoopStory || g.mode == Mode::CoopTraining;
}

bool IsCoop(Mode m) { return m == Mode::CoopStory || m == Mode::CoopTraining; }

void UpdateDoublesMenu(Game& g) {
    PlayerInput in = MenuInput();
    const int N = 4;
    if (in.up) { g.doublesSel = (g.doublesSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.doublesSel = (g.doublesSel + 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.back) { PlaySfx(Sfx::MenuBack); GoTitle(g); return; }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    if (g.doublesSel == 3) { GoTitle(g); return; }
    g.doublesKind = g.doublesSel;
    StartQuickFlow(g, Mode::Doubles);
}

void DrawDoublesMenu(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    DrawTextCentered("2v2 BATTLE", VW / 2, 30, 30, UI_GOLD);
    DrawTextCentered("Two Pokemon per team on the field at once. Each player brings 1-3.", VW / 2, 70, 10, UI_TEXT, false);
    const char* items[4] = {"You + CPU partner  vs  2 CPUs", "Player 1 + Player 2  vs  2 CPUs (co-op)",
                            "Player 1 + CPU  vs  Player 2 + CPU", "Back"};
    for (int i = 0; i < 4; i++) DrawMenuItem(items[i], VW / 2, 110 + i * 30, 20, g.doublesSel == i);
    DrawTextCentered("Aim: walk toward the foe you want to hit. Your target has brackets around it.", VW / 2, 250, 10, UI_DIM, false);
    DrawTextCentered("If your Pokemon all faint, press Swap to take over one of your partner's benched ones.", VW / 2, 264, 10, UI_DIM, false);
    DrawTextCentered("Enter choose   Esc back", VW / 2, 330, 10, UI_DIM, false);
}
