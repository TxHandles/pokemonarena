// Title, Settings and Controls screens.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "settings.h"
#include "sprites.h"
#include "ui.h"

namespace {

const char* kTitleItems[] = {"Story Mode", "Co-op Story", "Online Co-op", "Quick Battle", "2 Players", "2v2 Battle", "Watch CPU vs CPU", "Settings", "Quit"};
constexpr int TITLE_N = 9;

constexpr int SET_N = 12;

const char* kTabs[] = {"Keyboard - 1 player", "Keyboard - 2P Player 1", "Keyboard - 2P Player 2", "Gamepad 1", "Gamepad 2"};
constexpr int TAB_N = 5;
constexpr int CTRL_ROWS = 1 + ACT_COUNT + 2;  // tab row, actions, reset, back

void ApplyFullscreen() {
    bool on = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    if (on != Cfg().fullscreen) ToggleBorderlessWindowed();
}

} // namespace

// ------------------------------------------------------------------ title

void UpdateTitle(Game& g) {
    PlayerInput in = MenuInput();
    if (in.up) { g.titleSel = (g.titleSel + TITLE_N - 1) % TITLE_N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.titleSel = (g.titleSel + 1) % TITLE_N; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    switch (g.titleSel) {
        case 0:
            g.profiles = ListProfiles();
            g.profSel = 0;
            g.confirmDelete = false;
            g.screen = Screen::ProfileSelect;
            break;
        case 1: StartCoopFlow(g); break;
        case 2: StartOnlineFlow(g); break;
        case 3: StartQuickFlow(g, Mode::Quick); break;
        case 4: StartQuickFlow(g, Mode::TwoPlayer); break;
        case 5: g.doublesSel = 0; g.screen = Screen::DoublesMenu; break;
        case 6: StartQuickFlow(g, Mode::Watch); break;
        case 7: g.settingsSel = 0; g.screen = Screen::Settings; break;
        case 8: g.quit = true; break;
    }
}

void DrawTitle(const Game& g) {
    DrawArena(g.t);
    DrawRectangle(0, 0, VW, VH, Fade(UI_INK, 0.55f));
    DrawTextCentered("ARENA CLASH", VW / 2 + 2, 28, 40, Color{60, 40, 90, 255}, false);
    DrawTextCentered("ARENA CLASH", VW / 2, 26, 40, UI_GOLD);
    DrawTextCentered("18 types. Real-time battles. Level up, evolve, and become champion.", VW / 2, 70, 10, UI_TEXT);

    for (int i = 0; i < TITLE_N; i++) DrawMenuItem(kTitleItems[i], VW / 2, 82 + i * 18, 20, g.titleSel == i);

    static std::vector<int> parade;
    if (parade.empty() || g.t < 0.05f) {
        parade.clear();
        for (int i = 0; i < (int)AllSpecies().size(); i++) if (InSpeciesPool(i)) parade.push_back(i);
    }
    int n = (int)parade.size();
    const int spacing = 35;
    float total = (float)n * spacing;
    float scroll = n > 18 ? std::fmod(g.t * 20.0f, total) : 0.0f;
    for (int i = 0; i < n; i++) {
        float fx = 22 + i * spacing - scroll;
        if (n > 18 && fx < -20) fx += total;
        if (fx < -20 || fx > VW + 20) continue;
        int x = (int)fx;
        int hop = ((int)(g.t * 4 + i * 0.7f) % 4 == 0) ? 3 : 0;
        DrawEllipse(x, 274, 10, 3, Fade(BLACK, 0.35f));
        DrawSpeciesFit(parade[i], VIEW_FRONT, x, 276 - hop, 32);
    }
    DrawControlsHelp(20, 300, false);
    DrawText("Up/Down choose   Enter / Space / J confirm   Esc back", 20, 340, 10, UI_DIM);
    if (g.customCount > 0) {
        const char* msg = TextFormat("%d from the Pokemon folder", g.customCount);
        DrawText(msg, VW - 8 - TextW(msg, 10), 348, 10, Color{140, 220, 150, 255});
    }
}

// ------------------------------------------------------------------ settings

static void SettingText(int i, char* label, char* value) {
    Settings& c = Cfg();
    auto onoff = [](bool b) { return b ? "On" : "Off"; };
    switch (i) {
        case 0: std::sprintf(label, "Fullscreen"); std::sprintf(value, "%s", onoff(c.fullscreen)); break;
        case 1: std::sprintf(label, "Show FPS"); std::sprintf(value, "%s", onoff(c.showFps)); break;
        case 2: std::sprintf(label, "Master volume"); std::sprintf(value, "%d%%", c.masterVolume); break;
        case 3: std::sprintf(label, "Effects volume"); std::sprintf(value, "%d%%", c.sfxVolume); break;
        case 4: std::sprintf(label, "Auto-aim"); std::sprintf(value, "%s", onoff(c.autoAim)); break;
        case 5: std::sprintf(label, "Screen shake"); std::sprintf(value, "%s", onoff(c.screenShake)); break;
        case 6: std::sprintf(label, "Damage numbers"); std::sprintf(value, "%s", onoff(c.damageNumbers)); break;
        case 7: std::sprintf(label, "Team size"); std::sprintf(value, "%d", c.teamSize); break;
        case 8: std::sprintf(label, "CPU skill (Quick Battle / Training)"); std::sprintf(value, "%s", AILevelName(c.cpuLevel)); break;
        case 9: std::sprintf(label, "Arena (Quick Battle / 2 Players)"); std::sprintf(value, "%s", c.quickArena < 0 ? "Random" : c.quickArena < ARENA_COUNT ? ArenaName((ArenaKind)c.quickArena) : VenueName((Venue)(c.quickArena - ARENA_COUNT + 1))); break;
        case 10: std::sprintf(label, "Controls..."); value[0] = 0; break;
        default: std::sprintf(label, "Back"); value[0] = 0; break;
    }
}

void UpdateSettings(Game& g) {
    PlayerInput in = MenuInput();
    Settings& c = Cfg();
    if (in.up) { g.settingsSel = (g.settingsSel + SET_N - 1) % SET_N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.settingsSel = (g.settingsSel + 1) % SET_N; PlaySfx(Sfx::MenuMove); }
    if (in.back) { SaveSettings(); PlaySfx(Sfx::MenuBack); g.screen = Screen::Title; return; }
    int step = in.left ? -1 : ((in.right || in.confirm) ? 1 : 0);
    if (!step) return;
    bool changed = true;
    switch (g.settingsSel) {
        case 0: c.fullscreen = !c.fullscreen; ApplyFullscreen(); break;
        case 1: c.showFps = !c.showFps; break;
        case 2: c.masterVolume = std::max(0, std::min(100, c.masterVolume + step * 10)); break;
        case 3: c.sfxVolume = std::max(0, std::min(100, c.sfxVolume + step * 10)); break;
        case 4: c.autoAim = !c.autoAim; break;
        case 5: c.screenShake = !c.screenShake; break;
        case 6: c.damageNumbers = !c.damageNumbers; break;
        case 7: c.teamSize = ((c.teamSize - 1 + step + MAX_TEAM_SIZE) % MAX_TEAM_SIZE) + 1; break;
        case 8: c.cpuLevel = (AILevel)(((int)c.cpuLevel + step + 3) % 3); break;
        case 9: { const int n = ARENA_COUNT + VENUE_COUNT - 1 + 1; c.quickArena = ((c.quickArena + 1 + step + n) % n) - 1; } break;
        case 10:
            changed = false;
            if (in.confirm) { g.ctrlTab = 0; g.ctrlRow = 1; g.ctrlCol = 0; g.capturing = false; g.screen = Screen::Controls; }
            break;
        default:
            changed = false;
            if (in.confirm) { SaveSettings(); g.screen = Screen::Title; }
            break;
    }
    if (changed) {
        ApplyAudioSettings();
        SaveSettings();
        PlaySfx(Sfx::MenuOk);
    }
}

void DrawSettings(const Game& g) {
    ClearBackground(Color{24, 20, 38, 255});
    DrawTextCentered("SETTINGS", VW / 2, 14, 20, UI_GOLD);
    int x = 150, w = 340;
    const char* groups[] = {"Display", "", "Sound", "", "Gameplay", "", "", "", "", "", "Controls", ""};
    for (int i = 0; i < SET_N; i++) {
        char label[64], value[32];
        SettingText(i, label, value);
        int y = 42 + i * 23;
        bool sel = g.settingsSel == i;
        if (groups[i][0]) DrawText(groups[i], 40, y + 4, 10, UI_DIM);
        DrawPanel(x, y, w, 20, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : Color{70, 64, 92, 255});
        DrawText(label, x + 10, y + 5, 10, sel ? UI_GOLD : UI_TEXT);
        if (value[0]) {
            std::string v = std::string("< ") + value + " >";
            DrawText(v.c_str(), x + w - 10 - TextW(v.c_str(), 10), y + 5, 10, sel ? UI_GOLD : UI_TEXT);
        }
        if (i == 2 || i == 3) {
            int vol = i == 2 ? Cfg().masterVolume : Cfg().sfxVolume;
            DrawRectangle(x + 150, y + 8, 100, 4, Color{60, 56, 72, 255});
            DrawRectangle(x + 150, y + 8, vol, 4, UI_GOLD);
        }
    }
    DrawText("Left/Right change   Esc back   (saved automatically)", 150, 336, 10, UI_DIM);
}

// ------------------------------------------------------------------ controls

void UpdateControls(Game& g) {
    Settings& c = Cfg();
    bool padTab = g.ctrlTab >= 3;
    int pad = g.ctrlTab - 3;

    if (g.capturing) {
        g.captureT -= GetFrameTime();
        Act a = (Act)(g.ctrlRow - 1);
        if (g.captureT > 7.8f) {  // ignore the key that started capturing
            while (GetKeyPressed()) {}
            return;
        }
        if (IsKeyPressed(KEY_ESCAPE) || g.captureT <= 0) { g.capturing = false; PlaySfx(Sfx::MenuBack); return; }
        if (!padTab) {
            int k = GetKeyPressed();
            if (k == 0 || k == KEY_F11) return;
            if (k == KEY_BACKSPACE || k == KEY_DELETE) k = 0;
            c.keys[g.ctrlTab][(int)a][g.ctrlCol] = k;
        } else {
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressed(KEY_DELETE)) {
                c.pad[pad][(int)a] = 0;
            } else {
                int found = 0;
                if (IsGamepadAvailable(pad))
                    for (int b = 1; b <= GAMEPAD_BUTTON_RIGHT_THUMB && !found; b++)
                        if (IsGamepadButtonPressed(pad, b)) found = b;
                if (!found) return;
                c.pad[pad][(int)a] = found;
            }
        }
        g.capturing = false;
        SaveSettings();
        PlaySfx(Sfx::MenuOk);
        return;
    }

    PlayerInput in = MenuInput();
    if (in.back) { SaveSettings(); PlaySfx(Sfx::MenuBack); g.screen = Screen::Settings; return; }
    if (in.up) { g.ctrlRow = (g.ctrlRow + CTRL_ROWS - 1) % CTRL_ROWS; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.ctrlRow = (g.ctrlRow + 1) % CTRL_ROWS; PlaySfx(Sfx::MenuMove); }
    if (g.ctrlRow == 0) {
        if (in.left) g.ctrlTab = (g.ctrlTab + TAB_N - 1) % TAB_N;
        if (in.right || in.confirm) g.ctrlTab = (g.ctrlTab + 1) % TAB_N;
        g.ctrlCol = 0;
        return;
    }
    if (g.ctrlRow <= ACT_COUNT) {
        if (!padTab && (in.left || in.right)) g.ctrlCol = 1 - g.ctrlCol;
        if (in.confirm) { g.capturing = true; g.captureT = 8.0f; PlaySfx(Sfx::MenuOk); }
        return;
    }
    if (g.ctrlRow == ACT_COUNT + 1 && in.confirm) {
        if (padTab) ResetPad(pad);
        else ResetKeys(g.ctrlTab);
        SaveSettings();
        Toast(g, "Reset to defaults");
        PlaySfx(Sfx::MenuOk);
    }
    if (g.ctrlRow == ACT_COUNT + 2 && in.confirm) { SaveSettings(); g.screen = Screen::Settings; }
}

void DrawControls(const Game& g) {
    ClearBackground(Color{24, 20, 38, 255});
    DrawTextCentered("CONTROLS", VW / 2, 10, 20, UI_GOLD);
    bool padTab = g.ctrlTab >= 3;
    int pad = g.ctrlTab - 3;

    // Tab row
    std::string tab = std::string("< ") + kTabs[g.ctrlTab] + " >";
    DrawMenuItem(tab.c_str(), VW / 2, 38, 10, g.ctrlRow == 0);
    if (padTab) {
        const char* st = IsGamepadAvailable(pad) ? GetGamepadName(pad) : "not connected";
        DrawTextCentered(TextFormat("Gamepad %d: %s   (left stick always moves)", pad + 1, st), VW / 2, 52, 10, UI_DIM, false);
    }

    int x = 150, w = 340, y0 = 66;
    DrawText(padTab ? "Button" : "Key", x + 150, y0 - 2, 10, UI_DIM);
    if (!padTab) DrawText("Alternate", x + 250, y0 - 2, 10, UI_DIM);
    for (int a = 0; a < ACT_COUNT; a++) {
        int y = y0 + 10 + a * 18;
        bool rowSel = g.ctrlRow == a + 1;
        DrawPanel(x, y, w, 16, rowSel ? UI_PANEL_LIGHT : UI_PANEL, rowSel ? UI_GOLD : Color{70, 64, 92, 255});
        DrawText(ActName((Act)a), x + 10, y + 3, 10, rowSel ? UI_GOLD : UI_TEXT);
        int cols = padTab ? 1 : 2;
        for (int col = 0; col < cols; col++) {
            int cx = x + 150 + col * 100;
            bool cellSel = rowSel && g.ctrlCol == col;
            const char* label;
            bool conflict = false;
            if (padTab) {
                int b = Cfg().pad[pad][a];
                label = PadButtonName(b);
                for (int o = 0; o < ACT_COUNT; o++) conflict |= o != a && b && Cfg().pad[pad][o] == b;
            } else {
                int k = Cfg().keys[g.ctrlTab][a][col];
                label = KeyName(k);
                for (int o = 0; o < ACT_COUNT; o++)
                    for (int oc = 0; oc < 2; oc++) conflict |= (o != a || oc != col) && k && Cfg().keys[g.ctrlTab][o][oc] == k;
            }
            if (cellSel) DrawRectangle(cx - 4, y + 1, 80, 14, Fade(UI_GOLD, 0.25f));
            if (cellSel && g.capturing) {
                bool blink = ((int)(g.t * 4) % 2) == 0;
                DrawText(padTab ? "press a button" : "press a key", cx, y + 3, 10, blink ? UI_GOLD : UI_TEXT);
            } else {
                DrawText(label, cx, y + 3, 10, conflict ? Color{255, 110, 100, 255} : UI_TEXT);
            }
        }
    }
    int ry = y0 + 10 + ACT_COUNT * 18 + 4;
    DrawMenuItem("Reset this page to defaults", VW / 2, ry, 10, g.ctrlRow == ACT_COUNT + 1);
    DrawMenuItem("Back", VW / 2, ry + 16, 10, g.ctrlRow == ACT_COUNT + 2);
    DrawText(g.capturing ? "Esc cancels   Backspace clears the binding"
                         : "Up/Down choose   Left/Right column or page   Enter rebind   Esc back   Red = used twice",
             40, 344, 10, UI_DIM);
    DrawToast(g);
}
