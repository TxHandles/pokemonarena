// Practice arena: try your moves on dummies that never fight back and never stay down.
// Nothing is saved: no XP, no HP lost, no items used.
#include "arena.h"
#include "game.h"
#include "ui.h"
#include "input.h"
#include "audio.h"
#include "settings.h"
#include "data.h"
#include "net.h"

#include <algorithm>
#include <cmath>

CombatantSpec FromEntry(const RosterEntry& e);

namespace {
const char* kFirstDummy = "Snorlax";

SideSetup PartySide(const Profile& p, int who, bool coop) {
    SideSetup h;
    for (const RosterEntry& e : ProfileRoster(p, true)) {
        if ((int)h.team.size() >= MAX_TEAM_SIZE) break;
        CombatantSpec c = FromEntry(e);
        c.hpFrac = 1;
        c.item.clear();          // held items stay in the bag: nothing gets used up
        c.owner = who;
        h.team.push_back(c);
    }
    h.label = p.name;
    h.tag = coop ? (who == 0 ? "P1" : "P2") : "";
    h.team_ = 0;
    h.input = who;
    h.level = Cfg().cpuLevel;
    return h;
}

SideSetup DummySide(int level, const char* species) {
    SideSetup d;
    CombatantSpec c;
    c.species = std::max(0, SpeciesIndex(species));
    c.level = std::max(1, std::min(MAX_LEVEL, level));
    c.moves = MovesAtLevel(AllSpecies()[c.species], c.level);
    d.team.push_back(c);
    d.cpu = true;
    d.dummy = true;
    d.label = "Practice Dummy";
    d.tag = "DUMMY";
    d.team_ = 1;
    return d;
}
} // namespace

void StartPractice(Game& g) {
    bool online = g.on.active;
    bool localCoop = g.coop && !online;
    std::vector<SideSetup> v;
    if (localCoop) {
        for (int p = 0; p < 2; p++) if (!ProfileRoster(g.coopP[p], true).empty()) v.push_back(PartySide(g.coopP[p], p, true));
    } else {
        const Profile& me = online ? g.coopP[g.on.me] : (g.coop ? g.coopP[0] : g.profile);
        if (!ProfileRoster(me, true).empty()) v.push_back(PartySide(me, 0, false));
    }
    if (v.empty()) { Toast(g, "No Pokemon in your party to practice with!"); return; }
    int sum = 0, n = 0;
    for (auto& h : v) for (auto& c : h.team) { sum += c.level; n++; }
    int avg = n ? (int)std::lround((float)sum / n) : 5;
    bool doubles = v.size() > 1;
    v.push_back(DummySide(avg, kFirstDummy));
    if (doubles) v.push_back(DummySide(avg, "Geodude"));
    if (!doubles) v[0].input = 0;

    BattleSetup su;
    su.side = v;
    su.twoPlayer = doubles;
    su.autoAim = Cfg().autoAim;
    su.screenShake = Cfg().screenShake;
    su.damageNumbers = true;     // the point of the place
    su.arena = ArenaKind::Stadium;
    su.venue = (uint8_t)Venue::Dojo;
    su.practice = true;
    su.catchable = false;
    g.practiceBack = g.screen;
    g.practicePeak = 0;
    g.battle.Start(su);
    g.paused = false;
    g.pauseSel = 0;
    g.screen = Screen::Practice;
}

namespace {
void LeavePractice(Game& g) {
    gShake = {0, 0};
    g.paused = false;
    EnterStoryHub(g);
}
} // namespace

void UpdatePractice(Game& g, float dt) {
    bool tp = g.battle.Setup().twoPlayer;
    PlayerInput in[2] = {ReadInput(0, tp), ReadInput(1, tp)};
    if (in[0].pause || in[1].pause) {
        g.paused = !g.paused;
        g.pauseSel = 0;
        PlaySfx(g.paused ? Sfx::MenuOk : Sfx::MenuBack);
    }
    if (g.paused) {
        PlayerInput m = MenuInput();
        const int N = 3;
        if (m.up) { g.pauseSel = (g.pauseSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (m.down) { g.pauseSel = (g.pauseSel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (m.back) { g.paused = false; return; }
        if (m.confirm) {
            PlaySfx(Sfx::MenuOk);
            if (g.pauseSel == 0) g.paused = false;
            else if (g.pauseSel == 1) { g.battle.PracticeCycleDummies(); g.paused = false; }
            else LeavePractice(g);
        }
        return;
    }
    g.battle.Update(dt, in);
    float dealt = 0;
    for (int s = 0; s < g.battle.N(); s++)
        if (!g.battle.GetSide(s).dummy)
            for (const auto& f : g.battle.GetSide(s).team) dealt += f.dmgDealt;
    g.practicePeak = dealt;
}

void DrawPractice(const Game& g) {
    g.battle.Draw();
    // A small scoreboard under the HUD: total damage, the dummies' types
    int w = 196, x = VW / 2 - w / 2, y = 37;
    DrawRectangle(x, y, w, 26, Fade(UI_INK, 0.72f));
    const char* head = TextFormat("PRACTICE   total damage %d", (int)g.practicePeak);
    DrawText(head, VW / 2 - TextW(head, 10) / 2, y + 2, 10, UI_GOLD);
    int tw = 0;
    for (int pass = 0; pass < 2; pass++) {   // measure, then draw centred
        int dx = VW / 2 - tw / 2, start = dx;
        for (int s = 0; s < g.battle.N(); s++) {
            const Side& sd = g.battle.GetSide(s);
            if (!sd.dummy) continue;
            const SpeciesDef& sp = *sd.Cur().sp;
            if (pass) DrawText(sp.name.c_str(), dx, y + 14, 10, UI_TEXT);
            dx += TextW(sp.name.c_str(), 10) + 3;
            dx += (pass ? DrawTypeBadge(sp.type1, dx, y + 13) : TextW(TypeName(sp.type1), 10) + 6) + 2;
            if (sp.type2 != Type::None) dx += (pass ? DrawTypeBadge(sp.type2, dx, y + 13) : TextW(TypeName(sp.type2), 10) + 6) + 2;
            dx += 8;
        }
        tw = dx - start - 8;
    }
    if (!g.paused) {
        const char* hint = "Pause: next dummy / leave";
        DrawText(hint, VW / 2 - TextW(hint, 10) / 2, y + 28, 10, Fade(UI_DIM, 0.85f));
        return;
    }
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.7f));
    DrawTextCentered("PRACTICE", VW / 2, 60, 30, UI_GOLD);
    DrawTextCentered("Dummies don't fight back. Knock one down and the next type steps in.", VW / 2, 96, 10, UI_TEXT, false);
    const char* items[3] = {"Resume", "Next dummy type", "Leave practice"};
    for (int i = 0; i < 3; i++) DrawMenuItem(items[i], VW / 2, 122 + i * 26, 20, g.pauseSel == i);
    DrawControlsHelp(60, 210, g.battle.Setup().twoPlayer);
}
