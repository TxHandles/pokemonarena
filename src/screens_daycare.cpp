// Nicknames, Eggs hatching and the Day Care - the screens (the rules are in breeding.cpp).
#include <algorithm>
#include <cmath>
#include <string>
#include "arena.h"
#include "audio.h"
#include "breeding.h"
#include "game.h"
#include "input.h"
#include "profile.h"
#include "sprites.h"
#include "ui.h"

#ifdef ARENA_AUTOTEST
extern int gAutoMenu;
#endif

namespace {
void SaveFor(Game& g, int who) {
    if (g.coop) SaveWho(g, who);
    else StorySaveProfile(g);
}

const char* kGrid = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -'.!?";
constexpr int GRID_COLS = 12;
int GridCells() { return (int)std::string(kGrid).size() + 2; }   // + DEL and OK

bool NickChar(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == ' ' || c == '-' || c == '\'' || c == '.' || c == '!' || c == '?';
}

void NickDone(Game& g, bool keep) {
    Profile& P = Prof(g, g.nickWho);
    if (keep && g.nickMon >= 0 && g.nickMon < (int)P.mons.size()) {
        std::string n = g.nickBuf;
        while (!n.empty() && n.back() == ' ') n.pop_back();
        while (!n.empty() && n.front() == ' ') n.erase(n.begin());
        OwnedMon& m = P.mons[g.nickMon];
        m.nick = n == m.species ? "" : n.substr(0, NICK_MAX);
        if (!m.nick.empty()) Toast(g, m.species + " is now called " + m.nick + "!");
        SaveFor(g, g.nickWho);
    }
    PlaySfx(Sfx::MenuOk);
    if (g.nickReturn == 1) g.screen = Screen::MyCreatures;
    else if (g.nickReturn == 2) g.screen = Screen::World;
    else AdvanceStoryQueues(g);
}
}  // namespace

// ------------------------------------------------------------------ nicknames

void OpenNickname(Game& g, int who, int mon, int returnTo, bool ask) {
    g.nickWho = who;
    g.nickMon = mon;
    g.nickReturn = returnTo;
    g.nickPhase = ask ? 0 : 1;
    g.nickSel = 0;
    g.nickGrid = 0;
    const Profile& P = Prof(g, who);
    g.nickBuf = mon >= 0 && mon < (int)P.mons.size() ? P.mons[mon].nick : "";
    while (GetCharPressed()) {}
    g.screen = Screen::Nickname;
}

bool NextNickOrHatch(Game& g) {
    if (!g.hatchQueue.empty()) {
        g.hatchT = 0;
        g.hatchFromWorld = false;
        g.screen = Screen::Hatch;
        PlaySfx(Sfx::Status);
        return true;
    }
    while (!g.nickQueue.empty()) {
        Game::MonJob j = g.nickQueue.front();
        g.nickQueue.erase(g.nickQueue.begin());
        if (!OnlineMine(g, j.who) || j.mon < 0 || j.mon >= (int)Prof(g, j.who).mons.size()) continue;
        OpenNickname(g, j.who, j.mon, 0, true);
        return true;
    }
    return false;
}

void UpdateNickname(Game& g) {
    Profile& P = Prof(g, g.nickWho);
    if (g.nickMon < 0 || g.nickMon >= (int)P.mons.size()) { NickDone(g, false); return; }
    if (g.nickPhase == 0) {   // "Give a nickname to the caught Pidgey?"  Yes / No
        PlayerInput in = MenuInput();
        if (in.up || in.down || in.left || in.right) { g.nickSel = 1 - g.nickSel; PlaySfx(Sfx::MenuMove); }
        if (in.back) { NickDone(g, false); return; }
        if (!in.confirm) return;
        if (g.nickSel == 1) { NickDone(g, false); return; }
        g.nickPhase = 1;
        g.nickBuf = P.mons[g.nickMon].nick;
        while (GetCharPressed()) {}
        PlaySfx(Sfx::MenuOk);
        return;
    }
#ifdef ARENA_AUTOTEST
    if (gAutoMenu) {   // a scripted Enter keeps what's typed; a scripted Esc gives up
        int a = gAutoMenu;
        gAutoMenu = 0;
        if (a & 16) NickDone(g, true);
        else if (a & 32) NickDone(g, false);
        return;
    }
#endif
    // typing: the keyboard, or the letter grid with a gamepad (D-pad, A adds, B deletes, Start is done)
    int c;
    while ((c = GetCharPressed()) != 0)
        if ((int)g.nickBuf.size() < NICK_MAX && NickChar(c)) g.nickBuf += (char)c;
    if (IsKeyPressed(KEY_BACKSPACE) && !g.nickBuf.empty()) g.nickBuf.pop_back();
    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) { NickDone(g, true); return; }
    if (IsKeyPressed(KEY_ESCAPE)) { NickDone(g, false); return; }
    int cells = GridCells();
    for (int pad = 0; pad < 2; pad++) {
        if (!IsGamepadAvailable(pad)) continue;
        auto pr = [&](int b) { return IsGamepadButtonPressed(pad, b); };
        if (pr(GAMEPAD_BUTTON_LEFT_FACE_LEFT)) g.nickGrid = (g.nickGrid + cells - 1) % cells;
        if (pr(GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) g.nickGrid = (g.nickGrid + 1) % cells;
        if (pr(GAMEPAD_BUTTON_LEFT_FACE_UP)) g.nickGrid = (g.nickGrid + cells - GRID_COLS) % cells;
        if (pr(GAMEPAD_BUTTON_LEFT_FACE_DOWN)) g.nickGrid = std::min(cells - 1, g.nickGrid + GRID_COLS);
        if (pr(GAMEPAD_BUTTON_RIGHT_FACE_RIGHT) && !g.nickBuf.empty()) g.nickBuf.pop_back();
        if (pr(GAMEPAD_BUTTON_MIDDLE_RIGHT)) { NickDone(g, true); return; }
        if (pr(GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
            int n = (int)std::string(kGrid).size();
            if (g.nickGrid == n) { if (!g.nickBuf.empty()) g.nickBuf.pop_back(); }
            else if (g.nickGrid == n + 1) { NickDone(g, true); return; }
            else if ((int)g.nickBuf.size() < NICK_MAX) g.nickBuf += kGrid[g.nickGrid];
            PlaySfx(Sfx::MenuMove);
        }
    }
}

void DrawNickname(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    const Profile& P = Prof(g, g.nickWho);
    if (g.nickMon < 0 || g.nickMon >= (int)P.mons.size()) return;
    const OwnedMon& m = P.mons[g.nickMon];
    int si = MonSpecies(m);
    DrawPanel(VW / 2 - 200, 26, 400, 108, UI_PANEL, UI_GOLD);
    if (si >= 0) DrawSpeciesFit(ShinyIdx(si, m.shiny), VIEW_FRONT, VW / 2 - 150, 122, 80);
    DrawGender(m.gender, VW / 2 - 108, 40, 9);
    if (g.nickPhase == 0) {
        DrawTextCentered("NICKNAME", VW / 2 + 40, 36, 20, UI_GOLD);
        DrawWrapped(TextFormat("Give a nickname to %s?", m.species.c_str()), VW / 2 - 80, 64, 270, 10, UI_TEXT);
        const char* opt[2] = {"Yes", "No"};
        for (int k = 0; k < 2; k++) {
            int x = VW / 2 - 60 + k * 110, y = 96;
            bool on = g.nickSel == k;
            DrawPanel(x, y, 90, 22, on ? UI_PANEL_LIGHT : UI_PANEL, on ? UI_GOLD : UI_BORDER);
            DrawTextCentered(opt[k], x + 45, y + 6, 10, on ? UI_GOLD : UI_TEXT, false);
        }
        GDrawText("Enter choose   Esc no", 12, 342, 10, UI_DIM);
        return;
    }
    DrawTextCentered(TextFormat("%s's nickname?", m.species.c_str()), VW / 2 + 40, 36, 10, UI_TEXT, false);
    // the name so far, with a blinking cursor and blanks for the letters left
    int bx = VW / 2 - 80;
    for (int k = 0; k < NICK_MAX; k++) {
        int x = bx + k * 18;
        DrawRectangle(x, 86, 14, 2, k < (int)g.nickBuf.size() ? UI_GOLD : UI_DIM);
        if (k < (int)g.nickBuf.size()) {
            char ch[2] = {g.nickBuf[k], 0};
            DrawTextCentered(ch, x + 7, 66, 20, UI_TEXT);
        } else if (k == (int)g.nickBuf.size() && std::fmod(GetTime(), 1.0) < 0.6) DrawRectangle(x + 2, 68, 10, 14, Fade(UI_GOLD, 0.6f));
    }
    if (g.nickBuf.empty()) GDrawText(TextFormat("(empty: it stays %s)", m.species.c_str()), bx, 100, 10, UI_DIM);
    // the letter grid (for a gamepad)
    std::string grid = kGrid;
    int n = (int)grid.size();
    int gx = VW / 2 - GRID_COLS * 24 / 2, gy = 150;
    for (int k = 0; k < n + 2; k++) {
        int x = gx + (k % GRID_COLS) * 24, y = gy + (k / GRID_COLS) * 24;
        bool on = g.nickGrid == k;
        if (k < n) {
            DrawPanel(x, y, 22, 22, on ? UI_PANEL_LIGHT : UI_PANEL, on ? UI_GOLD : Color{70, 64, 92, 255});
            char ch[2] = {grid[k], 0};
            DrawTextCentered(grid[k] == ' ' ? "_" : ch, x + 11, y + 6, 10, on ? UI_GOLD : UI_TEXT, false);
        } else {
            int w = 70;
            x = gx + (k - n) * 80 + (GRID_COLS * 24) / 2 - 75;
            y = gy + ((n + GRID_COLS - 1) / GRID_COLS) * 24 + 4;
            DrawPanel(x, y, w, 22, on ? UI_PANEL_LIGHT : UI_PANEL, on ? UI_GOLD : Color{70, 64, 92, 255});
            DrawTextCentered(k == n ? "DEL" : "OK", x + w / 2, y + 6, 10, on ? UI_GOLD : UI_TEXT, false);
        }
    }
    GDrawText("Type a name - Enter done   Backspace delete   Esc keep its name   (gamepad: D-pad, A add, B delete, Start done)", 8, 342, 10, UI_DIM);
}

// ------------------------------------------------------------------ an Egg hatching

void UpdateHatch(Game& g, float dt) {
    g.hatchT += dt;
    if (g.hatchQueue.empty()) { if (g.hatchFromWorld) g.screen = Screen::World; else AdvanceStoryQueues(g); return; }
    float t = g.hatchT;
    static int lastBeat = -1;
    int beat = t < 2.7f ? (int)(t / 0.5f) : 99;
    if (beat != lastBeat && t > 0.6f && t < 2.7f) PlaySfx(Sfx::BallShake);
    if (t >= 2.7f && lastBeat < 99) PlaySfx(Sfx::EggHatch);
    lastBeat = beat;
    PlayerInput in = MenuInput();
    if (t < 4.0f || !(in.confirm || in.back)) return;
    Game::MonJob j = g.hatchQueue.front();
    g.hatchQueue.erase(g.hatchQueue.begin());
    lastBeat = -1;
    OpenNickname(g, j.who, j.mon, g.hatchFromWorld ? 2 : 0, true);
}

void DrawHatch(const Game& g) {
    DrawMenuBackdrop(Color{30, 40, 34, 255}, (float)GetTime());
    if (g.hatchQueue.empty()) return;
    const Game::MonJob& j = g.hatchQueue.front();
    const Profile& P = Prof(g, j.who);
    if (j.mon < 0 || j.mon >= (int)P.mons.size()) return;
    const OwnedMon& m = P.mons[j.mon];
    float t = g.hatchT;
    float cx = VW / 2.0f, by = 230;
    DrawEllipse((int)cx, (int)by, 46, 8, Fade(BLACK, 0.3f));
    if (t < 3.0f) {
        float k = std::max(0.0f, t - 0.6f) / 2.4f;   // the wobbles grow stronger
        float wob = std::sin(t * (6 + 10 * k)) * 0.35f * k;
        DrawEgg(cx, by, 110, wob);
        if (t > 1.6f) {   // cracks
            float c = std::min(1.0f, (t - 1.6f) / 1.2f);
            Color ink{90, 80, 70, 255};
            DrawLineEx({cx - 18, by - 70}, {cx - 6 * c, by - 62 - 4 * c}, 2, ink);
            DrawLineEx({cx - 6, by - 62}, {cx + 10 * c, by - 74}, 2, ink);
            if (c > 0.5f) DrawLineEx({cx + 10, by - 74}, {cx + 22, by - 64}, 2, ink);
        }
        DrawPanel(60, 288, VW - 120, 40, UI_PANEL, UI_GOLD);
        GDrawText(t < 1.2f ? "Oh?" : "Oh? The Egg is hatching!", 76, 302, 10, UI_TEXT);
        if (t > 2.7f) DrawRectangle(0, 0, VW, VH, Fade(WHITE, (t - 2.7f) / 0.3f));
        return;
    }
    int si = MonSpecies(m);
    float pop = std::min(1.0f, (t - 3.0f) / 0.35f);
    for (int k = 0; k < 16; k++) {   // sparkles
        float a = t * 0.8f + k * 0.39f, r = 60 + 30 * std::sin(t * 1.3f + k);
        DrawCircle((int)(cx + std::cos(a) * r * 1.5f), (int)(by - 60 + std::sin(a) * r), 1.5f, Fade(UI_GOLD, 0.5f + 0.5f * std::sin(t * 3 + k)));
    }
    if (si >= 0) DrawSpeciesFit(ShinyIdx(si, m.shiny), VIEW_FRONT, (int)cx, (int)by, (int)(120 * (0.6f + 0.4f * pop)));
    if (m.shiny) DrawShinyStar(cx + 50, by - 110, 6);
    if (t < 3.3f) DrawRectangle(0, 0, VW, VH, Fade(WHITE, 1 - (t - 3.0f) / 0.3f));
    DrawPanel(60, 288, VW - 120, 40, UI_PANEL, UI_GOLD);
    GDrawText(TextFormat("%s hatched from the Egg!", m.species.c_str()), 76, 296, 10, UI_TEXT);
    DrawGender(m.gender, 76 + TextW(TextFormat("%s hatched from the Egg!", m.species.c_str()), 10) + 6, 296, 9);
    GDrawText(IvVerdict(m).c_str(), 76, 310, 10, UI_DIM);
    if (t >= 4.0f) DrawTextCentered("Enter: continue", VW / 2, 340, 10, UI_DIM, false);
}

// ------------------------------------------------------------------ steps: the Day Care and Eggs

namespace {
void Hatched(Game& g, int who, Profile& P, std::vector<int> ready) {
    std::sort(ready.rbegin(), ready.rend());   // (from the back, so the indexes stay good)
    std::vector<Game::MonJob> jobs;
    for (int e : ready) {
        int mi = -1;
        HatchEgg(P, e, &mi);
        if (mi >= 0) jobs.push_back({who, mi});
    }
    std::reverse(jobs.begin(), jobs.end());
    for (auto& j : jobs) g.hatchQueue.push_back(j);
}
}  // namespace

void HubDaycareSteps(Game& g) {
    if (g.ow.active || g.ow.wildBattle || g.owFight || g.tower) return;   // (out in the world, steps are counted as you walk)
    for (int w = 0; w < (g.coop ? 2 : 1); w++) {
        if (!OnlineMine(g, w)) continue;
        Profile& P = Prof(g, w);
        bool had = P.daycareEgg;
        auto ready = DaycareSteps(P, BATTLE_STEPS);
        if (!had && P.daycareEgg) g.resultLines.push_back((g.coop ? P.name + ": the" : std::string("The")) + " Day Care man found an Egg! Come and get it at the Day Care.");
        if (!ready.empty()) Hatched(g, w, P, ready);
    }
}

bool WorldStepEggs(Game& g, int who) {
    if (!OnlineMine(g, who)) return false;
    Profile& P = Prof(g, who);
    if (P.daycare.empty() && P.eggs.empty()) return false;
    bool had = P.daycareEgg;
    auto ready = DaycareSteps(P, 1);
    if (!had && P.daycareEgg) Toast(g, "The Day Care man has an Egg for you! Pick it up at the Day Care.");
    if (ready.empty()) return false;
    Hatched(g, who, P, ready);
    SaveFor(g, who);
    g.hatchT = 0;
    g.hatchFromWorld = true;
    g.screen = Screen::Hatch;
    PlaySfx(Sfx::Status);
    return true;
}

// ------------------------------------------------------------------ the Day Care

namespace {
int Fee(const OwnedMon& m) { return DAYCARE_FEE + DAYCARE_FEE * std::max(0, m.level - std::max(1, m.dcLevel)); }

struct DcOpt { int kind, arg; std::string label; };   // kind: 0 take back, 1 leave one, 2 the Egg, 3 back
std::vector<DcOpt> DcOptions(const Game& g) {
    const Profile& P = Prof(g, g.dcWho);
    std::vector<DcOpt> v;
    for (int k = 0; k < (int)P.daycare.size(); k++)
        v.push_back({0, k, TextFormat("Take back %s (%d coins)", MonName(P.daycare[k]).c_str(), Fee(P.daycare[k]))});
    if (P.daycare.size() < 2) v.push_back({1, 0, "Leave a Pokemon here"});
    if (P.daycareEgg) v.push_back({2, 0, "Take the Egg"});
    v.push_back({3, 0, "Back"});
    return v;
}
std::vector<int> PartyIdx(const Profile& P) {
    std::vector<int> v;
    for (int i = 0; i < (int)P.mons.size(); i++) if (!P.mons[i].boxed) v.push_back(i);
    return v;
}
}  // namespace

void OpenDaycare(Game& g, int who, Screen back) {
    g.dcWho = who;
    g.dcBack = back;
    g.dcSel = 0;
    g.dcMode = 0;
    g.dcPickSel = 0;
    g.screen = Screen::Daycare;
}

void UpdateDaycare(Game& g) {
    PlayerInput in = MenuInput();
    Profile& P = Prof(g, g.dcWho);
    if (g.dcMode == 1) {   // which Pokemon to leave
        auto party = PartyIdx(P);
        int n = (int)party.size();
        if (in.back) { g.dcMode = 0; PlaySfx(Sfx::MenuBack); return; }
        if (n == 0) { g.dcMode = 0; return; }
        if (in.up) { g.dcPickSel = (g.dcPickSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
        if (in.down) { g.dcPickSel = (g.dcPickSel + 1) % n; PlaySfx(Sfx::MenuMove); }
        if (!in.confirm) return;
        if (n <= 1) { Toast(g, "That's your last Pokemon! You need it with you."); PlaySfx(Sfx::MenuBack); return; }
        int idx = party[std::min(g.dcPickSel, n - 1)];
        int healthy = 0;
        for (int i : party) healthy += P.mons[i].hp > 0 && i != idx;
        if (healthy == 0) { Toast(g, "If you leave that one, you won't have a Pokemon that can battle!"); PlaySfx(Sfx::MenuBack); return; }
        OwnedMon m = P.mons[idx];
        m.dcLevel = m.level;
        m.boxed = false;
        m.hp = 1;   // they're looked after
        P.mons.erase(P.mons.begin() + idx);
        P.daycare.push_back(m);
        P.daycareSteps = 0;
        FixParty(P);
        g.dcMode = 0;
        Toast(g, "Fine, we'll raise your " + MonName(m) + " for a while. Come back for it later.");
        if (P.daycare.size() == 2) Toast(g, DaycareLine(DaycareCompat(P.daycare[0], P.daycare[1])));
        SaveFor(g, g.dcWho);
        PlaySfx(Sfx::MenuOk);
        return;
    }
    auto opts = DcOptions(g);
    int N = (int)opts.size();
    g.dcSel = std::max(0, std::min(g.dcSel, N - 1));
    if (in.up) { g.dcSel = (g.dcSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.dcSel = (g.dcSel + 1) % N; PlaySfx(Sfx::MenuMove); }
    if (in.back) { PlaySfx(Sfx::MenuBack); if (g.dcBack == Screen::StoryHub) EnterStoryHub(g); else g.screen = g.dcBack; return; }
    if (!in.confirm) return;
    const DcOpt& o = opts[g.dcSel];
    switch (o.kind) {
        case 0: {   // take one back
            OwnedMon m = P.daycare[o.arg];
            int fee = Fee(m);
            if (P.coins < fee) { Toast(g, TextFormat("That'll be %d coins - you don't have enough.", fee)); PlaySfx(Sfx::MenuBack); return; }
            P.coins -= fee;
            int grew = m.level - std::max(1, m.dcLevel);
            P.daycare.erase(P.daycare.begin() + o.arg);
            P.daycareSteps = 0;
            m.dcLevel = 0;
            bool boxed = AddMon(P, m);
            Toast(g, grew > 0 ? TextFormat("Your %s has grown by %d level%s! (%d coins)%s", MonName(m).c_str(), grew, grew == 1 ? "" : "s", fee, boxed ? " It went to the Box." : "")
                              : TextFormat("Here's your %s back. (%d coins)%s", MonName(m).c_str(), fee, boxed ? " It went to the Box." : ""));
            PlaySfx(Sfx::Heal);
        } break;
        case 1:
            if (PartyIdx(P).size() <= 1) { Toast(g, "You'd have no Pokemon left with you!"); PlaySfx(Sfx::MenuBack); return; }
            g.dcMode = 1;
            g.dcPickSel = 0;
            PlaySfx(Sfx::MenuOk);
            return;
        case 2: {
            if ((int)P.eggs.size() >= MAX_EGGS) { Toast(g, "You have no room for it! (6 Eggs at most)"); PlaySfx(Sfx::MenuBack); return; }
            OwnedMon egg;
            if (P.daycare.size() < 2 || !MakeEgg(P.daycare[0], P.daycare[1], egg, P.name)) { P.daycareEgg = false; return; }
            P.eggs.push_back(egg);
            P.daycareEgg = false;
            Toast(g, "You received the Egg from the Day Care man! Walk (or battle) and it'll hatch.");
            PlaySfx(Sfx::LevelUp);
        } break;
        default:
            PlaySfx(Sfx::MenuBack);
            if (g.dcBack == Screen::StoryHub) EnterStoryHub(g);
            else g.screen = g.dcBack;
            return;
    }
    SaveFor(g, g.dcWho);
}

void DrawDaycare(const Game& g) {
    DrawMenuBackdrop(Color{30, 34, 24, 255}, (float)GetTime());
    const Profile& P = Prof(g, g.dcWho);
    DrawTextCentered("POKEMON DAY CARE", VW / 2, 8, 20, UI_GOLD);
    DrawTextCentered("Leave two Pokemon that get along and an Egg may turn up. They grow as you walk (or battle).", VW / 2, 32, 10, UI_DIM, false);
    // the two left here
    for (int k = 0; k < 2; k++) {
        int x = 16 + k * 152, y = 50;
        DrawPanel(x, y, 144, 118);
        if (k >= (int)P.daycare.size()) {
            DrawTextCentered("(empty)", x + 72, y + 52, 10, UI_DIM, false);
            continue;
        }
        const OwnedMon& m = P.daycare[k];
        int si = MonSpecies(m);
        if (si >= 0) DrawSpeciesFit(ShinyIdx(si, m.shiny), VIEW_FRONT, x + 72, y + 74, 60);
        GDrawText(MonName(m).c_str(), x + 6, y + 80, 10, UI_TEXT);
        DrawGender(m.gender, x + 10 + TextW(MonName(m).c_str(), 10), y + 81, 8);
        int grew = m.level - std::max(1, m.dcLevel);
        GDrawText(TextFormat("Lv%d%s", m.level, grew > 0 ? TextFormat("  (+%d)", grew) : ""), x + 6, y + 92, 10, UI_GOLD);
        auto eg = EggGroups(m.species);
        std::string gs;
        for (const auto& e : eg) gs += (gs.empty() ? "" : " / ") + e;
        GDrawText(gs.c_str(), x + 6, y + 104, 10, UI_DIM);
        if (!m.item.empty()) GDrawText(m.item.c_str(), x + 6, y + 6, 10, Color{140, 220, 255, 255});
    }
    int ly = 174;
    if (P.daycare.size() == 2) {
        int c = DaycareCompat(P.daycare[0], P.daycare[1]);
        GDrawText(("Day Care man: \"" + DaycareLine(c) + "\"").c_str(), 16, ly, 10, c > 0 ? UI_TEXT : Color{255, 150, 130, 255});
        ly += 14;
    }
    if (P.daycareEgg) {
        DrawEgg(30, ly + 22, 22, std::sin((float)GetTime() * 4) * 0.12f);
        GDrawText("He found an Egg! Take it while you're here.", 46, ly + 8, 10, UI_GOLD);
        ly += 30;
    }
    // the menu
    if (g.dcMode == 0) {
        auto opts = DcOptions(g);
        for (int k = 0; k < (int)opts.size(); k++) {
            int y = 236 + k * 18;
            bool on = g.dcSel == k;
            DrawPanel(16, y, 288, 16, on ? UI_PANEL_LIGHT : UI_PANEL, on ? UI_GOLD : Color{70, 64, 92, 255});
            GDrawText(opts[k].label.c_str(), 24, y + 3, 10, on ? UI_GOLD : UI_TEXT);
        }
    } else {
        auto party = PartyIdx(P);
        GDrawText("Which Pokemon should we raise?", 16, 220, 10, UI_GOLD);
        for (int k = 0; k < (int)party.size(); k++) {
            const OwnedMon& m = P.mons[party[k]];
            int y = 236 + k * 16;
            bool on = g.dcPickSel == k;
            DrawPanel(16, y, 288, 15, on ? UI_PANEL_LIGHT : UI_PANEL, on ? UI_GOLD : Color{70, 64, 92, 255});
            GDrawText(TextFormat("%s  Lv%d", MonName(m).c_str(), m.level), 24, y + 3, 10, on ? UI_GOLD : UI_TEXT);
            DrawGender(m.gender, 220, y + 4, 8);
        }
    }
    // the Eggs you carry
    DrawPanel(320, 50, 304, 284);
    GDrawText(TextFormat("Eggs you carry (%d/%d)", (int)P.eggs.size(), MAX_EGGS), 330, 58, 10, UI_GOLD);
    if (P.eggs.empty()) GDrawText("None yet.", 330, 76, 10, UI_DIM);
    for (int k = 0; k < (int)P.eggs.size(); k++) {
        int y = 74 + k * 42;
        DrawEgg(346, y + 34, 30, std::sin((float)GetTime() * 3 + k) * 0.06f);
        DrawWrapped(EggStatusLine(P.eggs[k]).c_str(), 366, y + 6, 250, 10, UI_TEXT);
    }
    GDrawText(TextFormat("Coins: %d", P.coins), 16, 342, 10, UI_GOLD);
    GDrawText("Up/Down choose   Enter pick   Esc back", 360, 342, 10, UI_DIM);
    DrawToast(g);
}

// ------------------------------------------------------------------ the Judge

void DrawJudge(const OwnedMon& m) {
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.6f));
    DrawPanel(150, 56, 340, 236, UI_PANEL, UI_GOLD);
    GDrawText(TextFormat("The Judge: %s", MonName(m).c_str()), 162, 64, 10, UI_GOLD);
    GDrawText(IvVerdict(m).c_str(), 162, 80, 10, UI_TEXT);
    static const char* kStat[6] = {"HP", "Attack", "Defense", "Sp. Atk", "Sp. Def", "Speed"};
    GDrawText("IV (0-31)", 260, 100, 10, UI_DIM);
    GDrawText("EVs", 400, 100, 10, UI_DIM);
    for (int k = 0; k < 6; k++) {
        int y = 116 + k * 22;
        GDrawText(kStat[k], 162, y, 10, UI_TEXT);
        int iv = std::max(0, m.iv[k]);
        Color jc = iv >= 31 ? UI_GOLD : iv >= 26 ? Color{140, 220, 150, 255} : iv >= 16 ? UI_TEXT : UI_DIM;
        GDrawText(TextFormat("%2d  %s", iv, IvJudge(iv).c_str()), 230, y, 10, jc);
        DrawRectangle(360, y + 2, 100, 7, UI_INK);
        DrawRectangle(360, y + 2, (int)(100.0f * m.ev[k] / EV_STAT_MAX), 7, Color{120, 200, 255, 255});
        GDrawText(TextFormat("%d", m.ev[k]), 464, y, 10, UI_TEXT);
    }
    GDrawText(TextFormat("EVs in all: %d / %d", EvTotal(m), EV_TOTAL_MAX), 162, 252, 10, UI_DIM);
    GDrawText("EVs come from the Pokemon it beats (vitamins: +10)", 162, 266, 10, UI_DIM);
    GDrawText("Enter / Esc: close", 380, 278, 10, UI_DIM);
}
