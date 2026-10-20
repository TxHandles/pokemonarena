// Starting a co-op story together: new trainers type their names, then both players pick their
// starters on one screen (local: each with their own controls; online: each on their own computer).
#include <algorithm>
#include <cctype>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "sprites.h"
#include "story.h"
#include "ui.h"

namespace {
const Color kP2{120, 230, 255, 255};
Color PCol(int p) { return p == 0 ? UI_GOLD : kP2; }

const Game* gPoolGame = nullptr;   // whose starter screen is up (its region's Pokemon)
std::vector<int> Pool() { return gPoolGame ? StarterPool(*gPoolGame) : RegionStarters(0); }
int StarterCount() { return (int)Pool().size(); }
int StarterSpecies(int i) { auto p = Pool(); return i >= 0 && i < (int)p.size() ? p[i] : -1; }

void GoStarters(Game& g) {
    gPoolGame = &g;
    g.coopReady[0] = g.coopReady[1] = false;
    g.coopStarterSel[0] = 0;
    g.coopStarterSel[1] = std::min(1, std::max(0, StarterCount() - 1));
    g.screen = Screen::CoopStarter;
}

// The species a player already has as their starter (-1 = none yet).
int TakenStarter(const Game& g, int who) {
    const Profile& P = g.coopP[who];
    if (P.mons.empty()) return -1;
    int s = SpeciesIndex(RegionStarter(P, g.journeyRegion));
    if (s < 0 && g.journeyRegion == 0) s = SpeciesIndex(P.starter);
    return s >= 0 ? s : MonSpecies(P.mons[0]);
}

void GiveStarter(Game& g, int who, int sp) {
    Profile& P = g.coopP[who];
    P.mons.clear();
    P.mons.push_back(NewMon(sp, Story().startLevel));
    RegionStarter(P, g.journeyRegion) = AllSpecies()[sp].name;   // a journey can start in any region
    if (P.bag["Poke Ball"] < START_BALLS) P.bag["Poke Ball"] = START_BALLS;
    SaveProfile(P);
}
} // namespace

// ------------------------------------------------------------------ new trainers' names (local co-op)

#ifdef ARENA_AUTOTEST
bool gAutoEnter = false;   // autotest: press Enter on the name screen
#endif

void UpdateCoopSetup(Game& g) {
    int p = g.coopNameWho;
    std::string& buf = g.coopName[p];
    int c;
    while ((c = GetCharPressed()) != 0) {
        if (buf.size() < 14 && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '_'))
            buf += (char)c;
    }
    if ((IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) && !buf.empty()) buf.pop_back();
    if (IsKeyPressed(KEY_ESCAPE)) { PlaySfx(Sfx::MenuBack); StartCoopFlow(g); return; }
    bool ok = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER);
#ifdef ARENA_AUTOTEST
    if (gAutoEnter) { ok = true; gAutoEnter = false; }
#endif
    if (IsGamepadAvailable(p) && IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {   // gamepad: A takes a default name
        if (buf.empty()) {
            for (int k = 1; k < 100; k++) {
                std::string tryName = TextFormat("Trainer %d", k);
                if (!ProfileExists(tryName) && tryName != g.coopName[1 - p]) { buf = tryName; break; }
            }
        }
        ok = true;
    }
    if (!ok) return;
    while (!buf.empty() && buf.back() == ' ') buf.pop_back();
    if (buf.empty()) { Toast(g, "Type a name first"); return; }
    if (ProfileExists(buf) || (g.coopNew[1 - p] && p == 1 && buf == g.coopName[0]) || (!g.coopNew[1 - p] && buf == g.coopP[1 - p].name)) {
        Toast(g, "That name is taken");
        return;
    }
    Profile np;
    np.name = buf;
    np.bag["Poke Ball"] = START_BALLS;
    g.coopP[p] = np;
    SaveProfile(np);
    PlaySfx(Sfx::MenuOk);
    if (p == 0 && g.coopNew[1]) {   // now player 2's name
        g.coopNameWho = 1;
        while (GetCharPressed()) {}
        return;
    }
    g.profiles = ListProfiles();
    GoStarters(g);
}

void DrawCoopSetup(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    int p = g.coopNameWho;
    DrawTextCentered("A NEW CO-OP JOURNEY", VW / 2, 40, 30, UI_GOLD);
    DrawTextCentered(TextFormat("PLAYER %d - name your trainer", p + 1), VW / 2, 96, 20, PCol(p));
    DrawPanel(VW / 2 - 120, 130, 240, 30, UI_PANEL_LIGHT, PCol(p));
    std::string shown = g.coopName[p] + ((((int)(g.t * 2)) % 2) ? "_" : " ");
    DrawTextCentered(shown.c_str(), VW / 2, 138, 20, UI_TEXT, false);
    DrawTextCentered("Type a name, Enter to confirm   Esc: back   (gamepad: A uses a default name)", VW / 2, 176, 10, UI_DIM, false);
    if (p == 1 && g.coopNew[0]) DrawTextCentered(TextFormat("Player 1 is %s", g.coopP[0].name.c_str()), VW / 2, 210, 10, PCol(0), false);
    DrawTextCentered("Next, you'll both pick your starters together.", VW / 2, 240, 10, UI_DIM, false);
    DrawToast(g);
}

// ------------------------------------------------------------------ Johto: Professor Elm's starters
// One at a time: the host (player 1) picks, then player 2, then Silver takes two of the ones left.

namespace {
int JohtoTaken(const Game& g, int who) { return SpeciesIndex(RegionStarter(g.coopP[who], std::max(0, PickedRegion(g)))); }
void GiveJohtoStarter(Game& g, int who, int sp) {
    Profile& P = g.coopP[who];
    int r = std::max(0, PickedRegion(g));
    AddMon(P, NewMon(sp, RegionGiftLevel(g, r, who)));
    RegionStarter(P, r) = AllSpecies()[sp].name;
    DexRegisterOwned(P);
    SaveProfile(P);
}
} // namespace

static void UpdateJohtoCoopStarter(Game& g) {
    int n = StarterCount();
    bool online = g.on.active;
    int a = JohtoTaken(g, 0), b = JohtoTaken(g, 1);
    int region = std::max(0, PickedRegion(g));
    if (a >= 0 && b >= 0) {   // both chosen: the rival takes theirs, then on to the new region
        EnsureRival(g, region);
        if (SpeciesIndex(CoopRival(g.coopSave, region)[0]) < 0) return;   // online guest: waiting for the host's pick to arrive
        if (g.regionRevealT < 0) { g.regionRevealT = 0; PlaySfx(Sfx::Status); }
        g.regionRevealT += GetFrameTime();
        PlayerInput in = MenuInput();
        if (g.regionRevealT > 2.5f && (in.confirm || in.back)) {
            g.regionPick = 0;
            g.regionRevealT = -1;
            if (!online) { SaveCoop(g.coopSave); SaveProfile(g.coopP[0]); SaveProfile(g.coopP[1]); }
            Toast(g, "Welcome to " + RegionInfo(region).name + "!");
            EnterStoryHub(g);
        }
        return;
    }
    int turn = a < 0 ? 0 : 1;   // the host first, then player 2
    PlayerInput in = online ? MenuInput() : ReadInput(turn, true);
#ifdef ARENA_AUTOTEST
    { extern int gAutoMenu; if (!online && gAutoMenu) in = MenuInput(); }
#endif
    if (online && turn != g.on.me) {   // the other player's turn: watch
        if (MenuInput().back) { g.regionPick = 0; EnterStoryHub(g); }
        return;
    }
    if (!online && ReadInput(0, true).back && turn == 0) { g.regionPick = 0; EnterStoryHub(g); return; }
    int& sel = g.coopStarterSel[turn];
    static int lastSent = -1;
    if (in.left || in.up) { sel = (sel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.right || in.down) { sel = (sel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (online && sel != lastSent) { lastSent = sel; OnlineSendCursor(sel); }
    if (!in.confirm) return;
    int sp = StarterSpecies(sel);
    if (sp < 0) return;
    if (turn == 1 && sp == a) { Toast(g, g.coopP[0].name + " took " + AllSpecies()[sp].name + " - pick another!"); PlaySfx(Sfx::MenuBack); return; }
    GiveJohtoStarter(g, turn, sp);
    if (online) OnlineSendProfile(g);
    PlaySfx(Sfx::LevelUp);
    Toast(g, g.coopP[turn].name + " chose " + AllSpecies()[sp].name + "!");
}

static void DrawJohtoCoopStarter(const Game& g) {
    int region = std::max(0, PickedRegion(g));
    int lvA = RegionGiftLevel(g, region, 0), lvB = RegionGiftLevel(g, region, 1);
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    std::string title = "WELCOME TO " + RegionInfo(region).name;
    for (char& c : title) c = (char)std::toupper((unsigned char)c);
    DrawTextCentered(title.c_str(), VW / 2, 12, 20, UI_GOLD);
    std::string prof = region == 0 ? "Professor Oak" : region >= 2 ? "Professor Birch" : "Professor Elm";
    DrawTextCentered((lvA == lvB ? TextFormat("%s has a Pokemon for each of you (level %d). %s picks first, then %s - and then...", prof.c_str(), lvA,
                                              g.coopP[0].name.c_str(), g.coopP[1].name.c_str())
                                 : TextFormat("%s has a Pokemon for each of you (levels %d and %d). %s picks first, then %s - and then...", prof.c_str(), lvA, lvB,
                                              g.coopP[0].name.c_str(), g.coopP[1].name.c_str())), VW / 2, 38, 10, UI_TEXT, false);
    int a = JohtoTaken(g, 0), b = JohtoTaken(g, 1);
    const std::string* rv = CoopRival(g.coopSave, region);
    if (a >= 0 && b >= 0 && SpeciesIndex(rv[0]) >= 0) {
        DrawRivalReveal(g, {RegionStarter(g.coopP[0], region), RegionStarter(g.coopP[1], region)}, {rv[0], rv[1]});
        return;
    }
    bool online = g.on.active;
    int turn = a < 0 ? 0 : 1;
    int n = StarterCount();
    int gap = n > 3 ? 8 : 20;
    int cw = std::min(150, (620 - (n - 1) * gap) / std::max(1, n));
    int x0 = VW / 2 - (n * cw + (n - 1) * gap) / 2;
    int sel = g.coopStarterSel[turn];
    if (online && turn != g.on.me) sel = g.on.partnerCursor >= 0 && g.on.partnerCursor < n ? g.on.partnerCursor : 0;
    for (int i = 0; i < n; i++) {
        int idx = StarterSpecies(i);
        if (idx < 0) continue;
        int x = x0 + i * (cw + gap);
        bool takenA = idx == a;
        DrawCreatureCard(idx, RegionGiftLevel(g, region, turn), x, 70, cw, 130, sel == i && !takenA);
        if (takenA) {
            DrawRectangle(x, 70, cw, 130, Fade(UI_INK, 0.55f));
            DrawTextCentered(TextFormat("%s's!", g.coopP[0].name.c_str()), x + cw / 2, 130, 10, PCol(0), false);
        } else if (sel == i) {
            int mx = x + cw / 2;
            DrawTriangle({(float)mx - 6, 206}, {(float)mx + 6, 206}, {(float)mx, 214}, PCol(turn));
            DrawTextCentered(TextFormat("P%d", turn + 1), mx, 218, 10, PCol(turn), false);
        }
    }
    for (int p = 0; p < 2; p++) {
        int x = p == 0 ? 20 : 330;
        int t = p == 0 ? a : b;
        bool mine = t >= 0;
        DrawPanel(x, 250, 290, 60, UI_PANEL, mine ? PCol(p) : (turn == p ? UI_GOLD : UI_BORDER));
        GDrawText(TextFormat("%s%s", g.coopP[p].name.c_str(), online && p == g.on.me ? " (you)" : ""), x + 8, 256, 10, PCol(p));
        const char* st = mine ? TextFormat("Partner: %s", AllSpecies()[t].name.c_str())
                       : turn == p ? (online && p != g.on.me ? "choosing..." : TextFormat("Left/Right choose   %s take it", online ? "Enter" : ConfirmLabel(p, true)))
                                   : "waits for player 1";
        GDrawText(st, x + 8, 274, 10, mine ? PCol(p) : UI_DIM);
    }
    DrawTextCentered(RegionWatchLine(region, true, std::max(0, n - 2)).c_str(), VW / 2, 320, 10, Color{255, 150, 150, 255}, false);
    DrawTextCentered(online ? "Esc: back to the hub (until you take one)" : "Player 1 Back: back to the hub", VW / 2, 340, 10, UI_DIM, false);
    DrawToast(g);
}

// ------------------------------------------------------------------ choosing starters together

void UpdateCoopStarter(Game& g) {
    gPoolGame = &g;
    if (g.regionPick) { UpdateJohtoCoopStarter(g); return; }
    int n = StarterCount();
    if (n == 0) {   // no starters defined: just go
        if (g.on.active) EnterStoryHub(g); else CoopBeginJourney(g);
        return;
    }
    bool online = g.on.active;
    if (online && g.coopSave.regionOrder.empty()) return;   // the host is still choosing where the journey starts
    if (online) {
        int me = g.on.me, other = 1 - me;
        int mine = TakenStarter(g, me), theirs = TakenStarter(g, other);
        if (mine >= 0 && theirs >= 0) {
            if (mine == theirs && me == 1) {   // both took the same one at once: the host keeps it
                g.coopP[me].mons.clear();
                RegionStarter(g.coopP[me], g.journeyRegion).clear();
                SaveProfile(g.coopP[me]);
                OnlineSendProfile(g);
                Toast(g, g.coopP[other].name + " took " + AllSpecies()[theirs].name + " first - pick another!");
                return;
            }
            if (mine != theirs) { Toast(g, "Your journey begins!"); EnterStoryHub(g); }
            return;
        }
        if (mine >= 0) return;   // waiting for the partner
        PlayerInput in = MenuInput();
        int& sel = g.coopStarterSel[me];
        static int lastSent = -1;
        if (in.left) { sel = (sel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
        if (in.right) { sel = (sel + 1) % n; PlaySfx(Sfx::MenuMove); }
        if (sel != lastSent) { lastSent = sel; OnlineSendCursor(sel); }
        if (!in.confirm) return;
        int sp = StarterSpecies(sel);
        if (sp < 0) return;
        if (sp == theirs) { Toast(g, g.coopP[other].name + " already took " + AllSpecies()[sp].name + "."); PlaySfx(Sfx::MenuBack); return; }
        GiveStarter(g, me, sp);
        OnlineSendProfile(g);
        PlaySfx(Sfx::LevelUp);
        return;
    }
    // Local: each player with their own controls.
    for (int p = 0; p < 2; p++) {
        PlayerInput in = ReadInput(p, true);
        bool needs = g.coopP[p].mons.empty();
        if (!needs) continue;
        if (!g.coopReady[p]) {
            if (in.left || in.up) { g.coopStarterSel[p] = (g.coopStarterSel[p] + n - 1) % n; PlaySfx(Sfx::MenuMove); }
            if (in.right || in.down) { g.coopStarterSel[p] = (g.coopStarterSel[p] + 1) % n; PlaySfx(Sfx::MenuMove); }
            if (in.confirm) {
                int sp = StarterSpecies(g.coopStarterSel[p]);
                int otherSp = g.coopP[1 - p].mons.empty() ? (g.coopReady[1 - p] ? StarterSpecies(g.coopStarterSel[1 - p]) : -1) : TakenStarter(g, 1 - p);
                if (sp < 0) continue;
                if (sp == otherSp) { Toast(g, "Your partner took that one - pick another!"); PlaySfx(Sfx::MenuBack); }
                else { g.coopReady[p] = true; PlaySfx(Sfx::MenuOk); }
            }
            if (in.back && p == 0) { PlaySfx(Sfx::MenuBack); StartCoopFlow(g); return; }
        } else if (in.back) {
            g.coopReady[p] = false;
            PlaySfx(Sfx::MenuBack);
        }
    }
    bool done = true;
    for (int p = 0; p < 2; p++) done &= !g.coopP[p].mons.empty() || g.coopReady[p];
    if (!done) return;
    for (int p = 0; p < 2; p++)
        if (g.coopP[p].mons.empty()) GiveStarter(g, p, StarterSpecies(g.coopStarterSel[p]));
    g.coopReady[0] = g.coopReady[1] = false;
    g.profiles = ListProfiles();
    PlaySfx(Sfx::LevelUp);
    CoopBeginJourney(g);
}

void DrawCoopStarter(const Game& g) {
    gPoolGame = &g;
    if (g.regionPick) { DrawJohtoCoopStarter(g); return; }
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    bool online = g.on.active;
    if (online && g.coopSave.regionOrder.empty()) {
        DrawTextCentered("A NEW JOURNEY", VW / 2, 120, 20, UI_GOLD);
        DrawTextCentered((g.coopP[0].name + " is choosing the region your journey starts in...").c_str(), VW / 2, 160, 10, UI_TEXT, false);
        DrawToast(g);
        return;
    }
    DrawTextCentered(("CHOOSE YOUR STARTERS - " + RegionInfo(g.journeyRegion).name).c_str(), VW / 2, 14, 20, UI_GOLD);
    DrawTextCentered(online ? "Each of you picks one - you can't both take the same Pokemon. Your rival takes two of the others!"
                            : "Each player picks one with their own controls - not the same one. Your rival takes two of the others!",
                     VW / 2, 40, 10, UI_TEXT, false);
    int n = StarterCount();
    int gap = n > 3 ? 8 : 20;
    int cw = std::min(150, (620 - (n - 1) * gap) / std::max(1, n));
    int x0 = VW / 2 - (n * cw + (n - 1) * gap) / 2;
    int sel[2], taken[2];
    bool locked[2];
    for (int p = 0; p < 2; p++) {
        taken[p] = TakenStarter(g, p);
        locked[p] = taken[p] >= 0 || (!online && g.coopReady[p]);
        sel[p] = g.coopStarterSel[p];
        if (online && p != g.on.me) sel[p] = g.on.partnerCursor >= 0 && g.on.partnerCursor < n ? g.on.partnerCursor : 0;
        if (taken[p] >= 0)
            for (int k = 0; k < n; k++) if (StarterSpecies(k) == taken[p]) sel[p] = k;
    }
    for (int i = 0; i < n; i++) {
        int idx = StarterSpecies(i);
        if (idx < 0) continue;
        int x = x0 + i * (cw + gap);
        bool any = sel[0] == i || sel[1] == i;
        DrawCreatureCard(idx, Story().startLevel, x, 70, cw, 130, any);   // a new journey: level 5 wherever it starts
        for (int p = 0; p < 2; p++) {
            if (sel[p] != i) continue;
            int mx = x + cw / 2 + (sel[0] == sel[1] ? (p == 0 ? -22 : 22) : 0);
            DrawTriangle({(float)mx - 6, 206}, {(float)mx + 6, 206}, {(float)mx, 214}, PCol(p));   // pointing up at the card
            const char* tag = locked[p] ? TextFormat("P%d took it!", p + 1) : TextFormat("P%d", p + 1);
            DrawTextCentered(tag, mx, 218, 10, PCol(p), false);
        }
    }
    for (int p = 0; p < 2; p++) {
        int x = p == 0 ? 20 : 330;
        DrawPanel(x, 250, 290, 70, UI_PANEL, locked[p] ? PCol(p) : UI_BORDER);
        GDrawText(TextFormat("%s%s", g.coopP[p].name.c_str(), online && p == g.on.me ? " (you)" : ""), x + 8, 256, 10, PCol(p));
        const char* st;
        if (taken[p] >= 0) st = TextFormat("Partner: %s", AllSpecies()[taken[p]].name.c_str());
        else if (!online && g.coopReady[p]) st = "Ready! (Back to change)";
        else if (online && p != g.on.me) st = "choosing...";
        else st = TextFormat("Left/Right choose   %s take it", online ? "Enter" : ConfirmLabel(p, true));
        GDrawText(st, x + 8, 274, 10, locked[p] ? PCol(p) : UI_DIM);
        if (taken[p] < 0 && sel[p] >= 0 && sel[p] < n) {
            int idx = StarterSpecies(sel[p]);
            if (idx >= 0) {
                std::string hint = EvolutionHint(idx);
                GDrawText(hint.c_str(), x + 8, 292, 10, UI_DIM);
            }
        }
    }
    DrawTextCentered(online ? "Esc / Back: you can change your mind until you take one" : "Player 1 Back (before choosing): back to trainer select",
                     VW / 2, 340, 10, UI_DIM, false);
    DrawToast(g);
}
