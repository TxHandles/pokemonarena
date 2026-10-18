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
    ClearBackground(Color{24, 20, 38, 255});
    DrawTextCentered("WHOSE CREATURES?", VW / 2, 24, 20, UI_GOLD);
    DrawTextCentered("Free play: every creature at Lv50.  A profile: its Story Mode creatures, levels and moves (no XP here).",
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
    if (IsCoop(g.mode)) g.teamSize = 3;  // each player brings up to 3 of their party
    else if (g.tower) g.teamSize = TowerTeamSize(g.towerFloor);
    else if (IsStoryMode(g.mode)) g.teamSize = std::min(g.teamSize, HealthyCount(g.profile));
    if (!IsCoop(g.mode))  // co-op: each player locks in early if they have fewer
        for (int p = 0; p < HumanPickers(g); p++) g.teamSize = std::min(g.teamSize, (int)g.roster[p].size());
    g.teamSize = std::max(1, g.teamSize);
    g.startDelay = -1;
    g.screen = Screen::Select;
}

void UpdateSelect(Game& g, float dt) {
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
        if (in.confirm && !lockIn && !g.ready[p] && (int)pk.size() < g.teamSize && !already) {
            if (IsStoryMode(g.mode) && c < n && ros[c].hpFrac <= 0) {
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

    SideSetup& b = su.side[1];
    b.label = SideName(g, 1);
    int avg = 0;
    for (const auto& c : a.team) avg += c.level;
    avg = a.team.empty() ? 5 : (int)std::lround((float)avg / a.team.size());

    switch (g.mode) {
        case Mode::TwoPlayer: {
            const auto& ros = g.roster[g.shared ? 0 : 1];
            for (int i : g.picks[1]) b.team.push_back(FromEntry(ros[i]));
            for (auto& c : b.team) c.hpFrac = 1;
        } break;
        case Mode::Story: {
            StageDef st = ActiveStage(g);
            if (st.rival) SetRivalContext(RivalStarterFor({ProfileStarter(g.profile)}), false);
            g.cpuTeam = MakeCpuTeam(&st, avg, MAX_TEAM_SIZE);
            b.team = g.cpuTeam;
            b.cpu = true;
            b.level = st.cpu;
            b.label = st.trainer;
            su.trainerBattle = true;
            su.arena = st.arena;
            su.catchable = st.Catchable();
        } break;
        case Mode::Training:
            su.catchable = true;
            SetTrainingWild(g);   // the Pokemon that live in the chosen training area
            if (g.rareBattle && g.rareSpecies >= 0) {
                // A lone rare creature, a little above your level, in a random arena you've unlocked.
                CombatantSpec c;
                c.level = std::min(MAX_LEVEL, avg + 3);
                c.species = WildActive() ? g.rareSpecies : EvolvedFormAt(g.rareSpecies, c.level);
                c.moves = MovesAtLevel(AllSpecies()[c.species], c.level);
                g.cpuTeam = {c};
                // Wild creatures never come alone: a regular one tags along.
                auto pal = MakeCpuTeam(nullptr, std::max(2, avg - 1), 1);
                g.cpuTeam.push_back(pal[0]);
                b.label = "Wild " + AllSpecies()[c.species].name;
                b.level = AILevel::Hard;
                su.arena = ArenaKind::Stadium;   // wild battles happen out in the world (venue below)
            } else {
                int foes = std::max(2, std::min((int)g.picks[0].size(), Story().trainingMaxFoes));  // never fewer than 2
                g.cpuTeam = MakeCpuTeam(nullptr, avg, foes);
                b.level = Cfg().cpuLevel;
                b.label = "Wild";
            }
            b.team = g.cpuTeam;
            b.cpu = true;
            break;
        case Mode::CoopStory:
        case Mode::CoopTraining: {
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
            int sum = 0, cnt = 0;
            for (auto& h : v) for (auto& c : h.team) { sum += c.level; cnt++; }
            int avg2 = cnt ? (int)std::lround((float)sum / cnt) : 5;
            std::vector<CombatantSpec> cpu;
            AILevel lvA = Cfg().cpuLevel, lvB = Cfg().cpuLevel;
            std::string label = "Rival";
            if (g.mode == Mode::CoopStory) {
                StageDef st = ActiveStage(g);
                if (st.rival) {
                    // The rival takes the starter neither player has, and brings his Pikachu too.
                    SetRivalContext(RivalStarterFor({ProfileStarter(g.coopP[0]), ProfileStarter(g.coopP[1])}), true);
                    std::vector<std::string> team = st.team;
                    bool hasPartner = false;
                    for (auto& e : team) hasPartner |= e.rfind("rival_partner", 0) == 0;
                    if (!hasPartner) team.insert(team.end() - (team.empty() ? 0 : 1), "rival_partner");
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
                lvA = lvB = st.cpu;
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
                    c.level = std::min(MAX_LEVEL, avg2 + 6);
                    c.species = WildActive() ? g.rareSpecies : EvolvedFormAt(g.rareSpecies, c.level);
                    c.moves = MovesAtLevel(AllSpecies()[c.species], c.level);
                    cpu = {c};
                    auto pal = MakeCpuTeam(nullptr, std::min(MAX_LEVEL, avg2 + 1), 1);
                    cpu.push_back(pal[0]);
                    lvA = AILevel::Hard;
                    label = "Wild " + AllSpecies()[c.species].name;
                    su.arena = ArenaKind::Stadium;
                } else {
                    int total = (int)(v[0].team.size() + v[1].team.size());
                    int foes = std::min(4, std::max(2, total - 1));
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
                c.tag = "CPU";
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
    if (g.tower)   // the Battle Tower: everyone fights at full health (and the profile's HP isn't touched)
        for (auto& sd : su.side)
            if (sd.team_ == 0 || &sd == &su.side[0]) for (auto& c : sd.team) c.hpFrac = 1;
    if (IsStoryMode(g.mode)) {   // Story: where it happens, day or night, and the season's weather
        int clock = StoryClock(g);
        StageDef stv;
        bool hasStage = (g.mode == Mode::Story || g.mode == Mode::CoopStory) && (g.tower || (g.stageIdx >= 0 && g.stageIdx < (int)Story().stages.size()));
        if (hasStage) stv = ActiveStage(g);
        const StageDef* st = hasStage ? &stv : nullptr;
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
            su.weather = RollSeasonWeather(SeasonOf(clock));
        }
        if (su.arena == ArenaKind::Ice) su.weather = Weather::Snow;   // it never stops snowing in Lorelei's chamber
    }
    g.battle.Start(su);
    g.paused = false;
    g.pauseSel = 0;
    g.screen = Screen::Battle;
}

static void DrawEntryInfo(const RosterEntry& e, int x, int y) {
    const SpeciesDef& sp = AllSpecies()[e.species];
    DrawRectangle(x, y, 42, 42, UI_PANEL_LIGHT);
    DrawSpeciesFit(e.species, VIEW_FRONT, x + 21, y + 40, 38);
    DrawTextShadow(sp.name.c_str(), x + 48, y + 1, 10, UI_TEXT);
    DrawText(TextFormat("Lv%d", e.level), x + 52 + TextW(sp.name.c_str(), 10), y + 1, 10, UI_GOLD);
    int bx = x + 48;
    bx += DrawTypeBadge(sp.type1, bx, y + 13) + 2;
    if (sp.type2 != Type::None) DrawTypeBadge(sp.type2, bx, y + 13);
    if (sp.floats) DrawText("hovers", x + 48, y + 28, 10, UI_DIM);
    if (!e.item.empty()) DrawText(TextFormat("@ %s", e.item.c_str()), x + 48, y + 38, 10, Color{140, 220, 255, 255});
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
        else DrawText("-", x + 10, my, 10, UI_DIM);
    }
}

void DrawSelect(const Game& g) {
    ClearBackground(Color{24, 20, 38, 255});
    const auto& ros = ViewRoster(g);
    int vp = g.shared ? 0 : g.pickPhase;
    std::string head = TextFormat("CHOOSE YOUR TEAM  (1-%d)", g.teamSize);
    if (!g.shared) head = std::string(vp == 0 ? "PLAYER 1" : "PLAYER 2") + ": CHOOSE YOUR TEAM";
    if (IsCoop(g.mode)) {
        head = std::string(vp == 0 ? "PLAYER 1" : "PLAYER 2") + ": PICK UP TO 3";
        std::string vs;
        if (g.mode == Mode::CoopStory) {
            const StageDef st = ActiveStage(g);
            int want = std::min(MAX_TEAM_SIZE, std::max(3, (int)st.team.size() + 2));
            vs = TextFormat("vs %s - %d creatures%s", st.trainer.c_str(), want,
                            st.towerFloor > 0 ? TextFormat(" - Battle Tower floor %d", st.towerFloor)
                            : IsRematch(g) && st.IsBoss() ? " - REMATCH (scaled to your level)"
                            : st.IsGym() ? TextFormat(" - %s Badge", st.badge.c_str()) : st.IsChampion() ? " - CHAMPIONSHIP" : st.IsLeague() ? " - Elite Four" : "");
        } else if (g.rareBattle && g.rareSpecies >= 0) {
            vs = TextFormat("RARE: a wild %s and a partner - weaken them to catch!", AllSpecies()[g.rareSpecies].name.c_str());
        } else {
            vs = "Training: 2 to 4 wild creatures - weaken them to catch!";
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
    for (int i = 0; i < n; i++) {
        int r = i / GRID_COLS;
        if (r < scroll || r >= scroll + GRID_ROWS_VISIBLE) continue;
        const RosterEntry& e = ros[i];
        const SpeciesDef& sp = AllSpecies()[e.species];
        int x = 10 + (i % GRID_COLS) * 104, y = 32 + (r - scroll) * 54, w = 100, h = 50;
        DrawPanel(x, y, w, h, UI_PANEL, Color{80, 74, 104, 255});
        DrawRectangle(x + 1, y + 1, w - 2, 3, TypeColor(sp.type1));
        DrawEllipse(x + 22, y + 44, 13, 3, Fade(BLACK, 0.3f));
        DrawSpeciesFit(e.species, VIEW_FRONT, x + 22, y + 46, 38);
        DrawText(sp.name.c_str(), x + 42, y + 8, 10, UI_TEXT);
        DrawTypeBadge(sp.type1, x + 42, y + 21);
        if (e.ownedId >= 0 || e.level != FREE_PLAY_LEVEL) DrawText(TextFormat("Lv%d", e.level), x + 42, y + 35, 10, UI_GOLD);
        if (IsStoryMode(g.mode)) {
            DrawHpBar(x + 42, y + 46 - 2, 52, 2, e.hpFrac);
            if (e.hpFrac <= 0) {
                DrawRectangle(x + 1, y + 1, w - 2, h - 2, Fade(UI_INK, 0.6f));
                DrawText("FAINTED", x + 30, y + 20, 10, Color{255, 120, 100, 255});
            }
        }
        for (int p = 0; p < 2; p++) {
            if (!g.shared && p != vp) continue;
            auto it = std::find(g.picks[p].begin(), g.picks[p].end(), i);
            if (it == g.picks[p].end()) continue;
            int slot = (int)(it - g.picks[p].begin()) + 1;
            int bx = x + w - 13 - p * 13, by = y + h - 14;
            DrawRectangle(bx, by, 11, 11, TeamColor(p));
            DrawText(TextFormat("%d", slot), bx + 3, by + 1, 10, UI_INK);
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
                DrawTextShadow(st.trainer.c_str(), x + 6, y + 24, 20, UI_TEXT);
                DrawText(st.name.c_str(), x + 6, y + 48, 10, UI_DIM);
                DrawText(TextFormat("%d creature%s", (int)std::min<size_t>(st.team.size(), MAX_TEAM_SIZE), st.team.size() > 1 ? "s" : ""), x + 6, y + 66, 10, UI_TEXT);
                if (st.towerFloor > 0) DrawText(TextFormat("Tower floor %d: levels scale to your team", st.towerFloor), x + 6, y + 80, 10, UI_GOLD);
                else if (IsRematch(g) && st.IsBoss()) DrawText("REMATCH: levels scale to your team", x + 6, y + 80, 10, UI_GOLD);
                else if (st.baseLevel > 0) DrawText(TextFormat("Levels around %d", st.baseLevel), x + 6, y + 80, 10, UI_TEXT);
                else DrawText(TextFormat("Levels around your team %s", st.levelOffset >= 0 ? TextFormat("+%d", st.levelOffset) : TextFormat("%d", st.levelOffset)), x + 6, y + 80, 10, UI_TEXT);
                DrawText(TextFormat("CPU skill: %s", AILevelName(st.cpu)), x + 6, y + 94, 10, UI_TEXT);
                DrawText(TextFormat("Arena: %s", st.arena == ArenaKind::Stadium ? VenueName(st.venue.empty() ? VenueForPlace(st.name) : VenueByName(st.venue)) : ArenaName(st.arena)), x + 6, y + 108, 10, st.arena == ArenaKind::Stadium ? UI_TEXT : UI_GOLD);
                DrawText(st.towerFloor > 0 ? "Battle Tower: everyone fights at full health" : st.IsChampion() ? "The Championship!" : st.IsLeague() ? "Elite Four: lose and the League starts over"
                         : st.IsGym() ? "Gym battle" : "Trainer battle: catch wild ones in training", x + 6, y + 122, 10, UI_DIM);
            } else {
                if (g.rareBattle && g.rareSpecies >= 0) {
                    DrawTextShadow("RARE ENCOUNTER!", x + 6, y + 24, 20, Color{255, 150, 230, 255});
                    DrawRectangle(x + 200, y + 24, 80, 80, UI_PANEL_LIGHT);
                    DrawSpeciesFit(g.rareSpecies, VIEW_FRONT, x + 240, y + 102, 72);
                    DrawText(TextFormat("A wild %s appeared!", AllSpecies()[g.rareSpecies].name.c_str()), x + 6, y + 50, 10, UI_TEXT);
                    DrawText("It's a few levels above your team.", x + 6, y + 64, 10, UI_DIM);
                    DrawText("Weaken it, then catch it with", x + 6, y + 82, 10, UI_GOLD);
                    DrawText("a ball - don't knock it out!", x + 6, y + 94, 10, UI_GOLD);
                    DrawText(TextFormat("Poke Balls: %d", BagCount(g.profile, "Poke Ball")), x + 6, y + 112, 10, UI_TEXT);
                } else {
                    DrawTextShadow("Training battle", x + 6, y + 24, 20, UI_TEXT);
                    DrawText("Wild Pokemon near your team's level.", x + 6, y + 50, 10, UI_DIM);
                    DrawText(TextFormat("2 to %d wild Pokemon - bring up to 6!", std::max(2, Story().trainingMaxFoes)), x + 6, y + 66, 10, UI_TEXT);
                    int every = std::max(1, Story().rareEvery);
                    DrawText(TextFormat("Training wins: %d  (every %dth battle: a rare creature)", g.profile.trainingWins, every), x + 6, y + 80, 10, UI_TEXT);
                    DrawText("Weaken a foe to try catching it after the win!", x + 6, y + 96, 10, UI_GOLD);
                }
            }
            continue;
        }
        if (!human) {
            if (g.mode == Mode::Doubles) {
                DrawText("Your CPU partner and both rival CPUs", x + 6, y + 60, 10, UI_DIM);
                DrawText("get random teams the same size as yours.", x + 6, y + 72, 10, UI_DIM);
            } else
                DrawText(g.ready[0] ? "Picking..." : "Waiting for you...", x + 6, y + 60, 10, UI_DIM);
            continue;
        }
        bool partnerView = g.on.active && p != g.on.me;
        const std::vector<int>& shown = partnerView ? (g.on.partnerPicksIn ? g.on.partnerPicks : g.on.partnerPickView) : g.picks[p];
        const auto& pr = g.roster[g.shared ? 0 : p];
        for (int s = 0; s < g.teamSize; s++) {
            int sx = x + w - 6 - (g.teamSize - s) * 23, sy = y + 4;
            DrawRectangle(sx, sy, 22, 22, UI_PANEL_LIGHT);
            RectLines(sx, sy, 22, 22, Color{90, 84, 116, 255});
            if (s < (int)shown.size() && shown[s] >= 0 && shown[s] < (int)pr.size()) DrawSpeciesFit(pr[shown[s]].species, VIEW_FRONT, sx + 11, sy + 21, 20);
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
                DrawSpeciesFit(e.species, VIEW_FRONT, x + 18, ry + 23, 22);
                DrawText(sp.name.c_str(), x + 34, ry + 2, 10, UI_TEXT);
                DrawText(TextFormat("Lv%d", e.level), x + 38 + TextW(sp.name.c_str(), 10), ry + 2, 10, UI_GOLD);
                int bx = x + 34;
                bx += DrawTypeBadge(sp.type1, bx, ry + 12) + 2;
                if (sp.type2 != Type::None) bx += DrawTypeBadge(sp.type2, bx, ry + 12) + 2;
                for (size_t q = 0; q < e.moves.size() && q < 4; q++) {   // move types as little chips
                    DrawRectangle(x + w - 50 + (int)q * 10, ry + 8, 8, 8, TypeColor(e.moves[q].type));
                }
            }
            if (shown.empty()) DrawText("No picks yet...", x + 6, ly + 4, 10, UI_DIM);
            const char* st;
            if (partnerView) {
                st = g.on.partnerPicksIn ? "Ready! Waiting for you..." : "Picking on their screen...";
                int pc = g.on.partnerSelCursor;
                if (!g.on.partnerPicksIn && pc >= 0 && pc < (int)pr.size())
                    DrawText(TextFormat("Looking at: %s", AllSpecies()[pr[pc].species].name.c_str()), x + 6, y + h - 28, 10, UI_DIM);
            } else {
                st = "READY!";
            }
            DrawText(st, x + 6, y + h - 14, 10, (partnerView ? g.on.partnerPicksIn : true) ? UI_GOLD : UI_DIM);
            continue;
        }
        if (!g.shared && p != vp && !g.ready[p]) {
            DrawText("Waiting for Player 1...", x + 6, y + 60, 10, UI_DIM);
            continue;
        }
        if (g.cursor[p] < (int)pr.size()) DrawEntryInfo(pr[g.cursor[p]], x + 6, y + 30);
        const char* status;
        char buf[96];
        bool tp = TwoHumans(g);
        if (g.ready[p]) status = "READY!";
        else if ((int)g.picks[p].size() >= g.teamSize) status = "Team full - confirm again to lock in";
        else if (!g.picks[p].empty()) {
            std::snprintf(buf, sizeof buf, "Pick: %s   Lock in now: %s   Undo: %s", ConfirmLabel(p, tp), SwapKeyLabel(p, tp), BackLabel(p, tp));
            status = buf;
        } else {
            std::snprintf(buf, sizeof buf, "Pick: %s   Undo/back: %s", ConfirmLabel(p, tp), BackLabel(p, tp));
            status = buf;
        }
        DrawText(status, x + 6, y + h - 14, 10, g.ready[p] ? UI_GOLD : UI_DIM);
    }
    if (g.startDelay >= 0) {
        DrawRectangle(0, 150, VW, 40, Fade(UI_INK, 0.8f));
        DrawTextCentered("BATTLE START!", VW / 2, 160, 20, UI_GOLD);
    }
}

// ------------------------------------------------------------------ battle

void UpdateBattleScreen(Game& g, float dt) {
    if (OnlineBattleUpdate(g, dt)) return;
    bool tp = TwoHumans(g);
    PlayerInput in[2] = {ReadInput(0, tp), ReadInput(1, tp)};
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
    g.battle.Update(dt, in);
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
        DrawText(TextFormat("%dms", net::PingMs()), VW - 34, VH - 12, 10, Fade(UI_DIM, 0.7f));
        if (g.on.stallT > 0.5f) DrawTextCentered("Waiting for your partner's connection...", VW / 2, 150, 10, UI_GOLD, false);
        if (g.on.leaveAsk) {
            DrawRectangle(0, 130, VW, 50, Fade(UI_INK, 0.85f));
            DrawTextCentered("Leave this battle? Your partner goes back to the hub too, and nothing is saved.", VW / 2, 140, 10, UI_TEXT, false);
            DrawTextCentered("Enter: leave     Esc: keep playing", VW / 2, 158, 10, UI_GOLD, false);
        }
        return;
    }
    if (!g.paused) return;
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.7f));
    DrawTextCentered("PAUSED", VW / 2, 70, 30, UI_GOLD);
    const char* items[2] = {"Resume", IsStoryMode(g.mode) ? "Leave battle (no XP)" : "Quit to title"};
    for (int i = 0; i < 2; i++) DrawMenuItem(items[i], VW / 2, 120 + i * 26, 20, g.pauseSel == i);
    DrawControlsHelp(60, 200, TwoHumans(g));
    DrawText("Status: BRN burn, PSN/TOX poison, PAR paralysis, FRZ frozen, SLP asleep. Moves show x2 / x0.5 / x0 vs the foe.",
             60, 250, 10, UI_DIM);
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
    ClearBackground(Color{24, 20, 38, 255});
    DrawTextCentered("2v2 BATTLE", VW / 2, 30, 30, UI_GOLD);
    DrawTextCentered("Two creatures per team on the field at once. Each player brings 1-3.", VW / 2, 70, 10, UI_TEXT, false);
    const char* items[4] = {"You + CPU partner  vs  2 CPUs", "Player 1 + Player 2  vs  2 CPUs (co-op)",
                            "Player 1 + CPU  vs  Player 2 + CPU", "Back"};
    for (int i = 0; i < 4; i++) DrawMenuItem(items[i], VW / 2, 110 + i * 30, 20, g.doublesSel == i);
    DrawTextCentered("Aim: walk toward the foe you want to hit. Your target has brackets around it.", VW / 2, 250, 10, UI_DIM, false);
    DrawTextCentered("If your creatures all faint, press Swap to take over one of your partner's benched ones.", VW / 2, 264, 10, UI_DIM, false);
    DrawTextCentered("Enter choose   Esc back", VW / 2, 330, 10, UI_DIM, false);
}
