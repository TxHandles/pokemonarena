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

constexpr int SET_N = 14;

const char* kTabs[] = {"Keyboard - 1 player", "Keyboard - 2P Player 1", "Keyboard - 2P Player 2", "Gamepad 1", "Gamepad 2"};
constexpr int TAB_N = 5;
constexpr int CTRL_ROWS = 1 + ACT_COUNT + 2;  // tab row, actions, reset, back

void ApplyFullscreen() {
    bool on = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
    if (on != Cfg().fullscreen) ToggleBorderlessWindowed();
}

} // namespace

// ------------------------------------------------------------------ title

namespace {
struct TitleItem { const char* label; const char* hint; };
const TitleItem kMain[] = {
    {"Story Mode", "Your journey: gyms, the League, legendaries"},
    {"Co-op", "Story Mode for two - same screen or online"},
    {"Quick Battle", "One-off battles: vs the CPU, 2 players, 2v2, or watch"},
    {"Settings", "Display, sound, gameplay and controls"},
    {"Quit", "See you next time, Trainer!"},
};
const TitleItem kCoop[] = {
    {"Local Co-op", "Two players on one computer"},
    {"Online Co-op", "Play with a friend over the internet"},
    {"Back", ""},
};
const TitleItem kQuick[] = {
    {"1 vs CPU", "Pick a team and battle the computer"},
    {"2 Players", "Battle a friend on the same computer"},
    {"2v2 Battle", "Doubles: team up with a partner or the CPU"},
    {"Watch CPU vs CPU", "Sit back and watch two CPU teams battle"},
    {"Back", ""},
};
const TitleItem* TitleItems(int menu, int& n) {
    if (menu == 1) { n = 3; return kCoop; }
    if (menu == 2) { n = 5; return kQuick; }
    n = 5;
    return kMain;
}

// A Poke Ball: red top, white bottom, black band and button.
void TitleBall(float x, float y, float r, float rot, unsigned char alpha = 255) {
    Vector2 c{x, y};
    DrawCircleV(c, r + std::max(1.0f, r * 0.08f), Color{20, 18, 30, alpha});
    DrawCircleSector(c, r, -180 + rot, 0 + rot, 32, Color{226, 52, 52, alpha});
    DrawCircleSector(c, r, 0 + rot, 180 + rot, 32, Color{244, 244, 248, alpha});
    Vector2 d{std::cos(rot * DEG2RAD), std::sin(rot * DEG2RAD)};
    DrawLineEx({c.x - d.x * r, c.y - d.y * r}, {c.x + d.x * r, c.y + d.y * r}, std::max(1.0f, r * 0.16f), Color{20, 18, 30, alpha});
    DrawCircleV(c, r * 0.32f, Color{20, 18, 30, alpha});
    DrawCircleV(c, r * 0.2f, Color{250, 250, 250, alpha});
}

// The logo: "POKeMON" in arched yellow letters with a thick blue outline (the classic look),
// then "BATTLE MASTERS" on a red banner with a Poke Ball in the middle.
void DrawLogo(int cx, int top, float t) {
    const char* word = "POKEMON";
    const int size = 40;
    const Color blue{34, 74, 160, 255}, deep{18, 34, 86, 255}, yellow{255, 206, 22, 255}, light{255, 236, 120, 255};
    int widths[7], total = 0;
    for (int i = 0; i < 7; i++) {
        char ch[2] = {word[i], 0};
        widths[i] = TextW(ch, size) + 7;
        total += widths[i];
    }
    int x = cx - total / 2;
    for (int pass = 0; pass < 4; pass++) {
        int xx = x;
        for (int i = 0; i < 7; i++) {
            char ch[2] = {word[i], 0};
            float u = (i - 3) / 3.0f;
            int y = top + (int)std::lround(u * u * 5);   // arched, like the real logo
            float wob = std::sin(t * 2.0f + i * 0.6f) * 0.8f;
            y += (int)wob;
            if (pass == 0) {   // deep shadow
                for (int dx = -4; dx <= 4; dx++)
                    for (int dy = -4; dy <= 6; dy++)
                        if (dx * dx + dy * dy <= 22) DrawText(ch, xx + dx, y + dy + 2, size, deep);
            } else if (pass == 1) {   // blue outline
                for (int dx = -3; dx <= 3; dx++)
                    for (int dy = -3; dy <= 3; dy++)
                        if (dx * dx + dy * dy <= 10) DrawText(ch, xx + dx, y + dy, size, blue);
            } else if (pass == 2) {
                DrawText(ch, xx, y, size, yellow);
            } else {   // the lighter top half of each letter
                BeginScissorMode((xx - 2) * CANVAS_ZOOM, y * CANVAS_ZOOM, (widths[i] + 4) * CANVAS_ZOOM, (size / 2 - 3) * CANVAS_ZOOM);
                DrawText(ch, xx, y, size, light);
                EndScissorMode();
                if (i == 3) {   // the accent on the E
                    float ax = xx + widths[i] * 0.38f, ay = (float)y - 1;
                    DrawLineEx({ax, ay}, {ax + 7, ay - 6}, 5.0f, blue);
                    DrawLineEx({ax + 0.5f, ay - 0.5f}, {ax + 6.5f, ay - 5.5f}, 2.5f, yellow);
                }
            }
            xx += widths[i];
        }
    }
    // the banner
    int by = top + 48;
    const char* l = "BATTLE", *r = "MASTERS";
    int lw = TextW(l, 20), rw = TextW(r, 20), gap = 30;
    int bw = lw + rw + gap + 40;
    DrawRectangle(cx - bw / 2 + 3, by + 3, bw, 24, Fade(BLACK, 0.4f));
    DrawRectangle(cx - bw / 2, by, bw, 24, Color{200, 36, 44, 255});
    DrawRectangle(cx - bw / 2, by, bw, 3, Color{246, 96, 96, 255});
    DrawRectangle(cx - bw / 2, by + 21, bw, 3, Color{130, 18, 30, 255});
    DrawTriangle({(float)cx - bw / 2, (float)by}, {(float)cx - bw / 2 - 12, (float)by + 12}, {(float)cx - bw / 2, (float)by + 24}, Color{200, 36, 44, 255});
    DrawTriangle({(float)cx + bw / 2, (float)by}, {(float)cx + bw / 2, (float)by + 24}, {(float)cx + bw / 2 + 12, (float)by + 12}, Color{200, 36, 44, 255});
    DrawTextOutlined(l, cx - gap / 2 - lw, by + 3, 20, WHITE, Color{110, 14, 24, 255});
    DrawTextOutlined(r, cx + gap / 2, by + 3, 20, WHITE, Color{110, 14, 24, 255});
    TitleBall((float)cx, (float)by + 12, 10, std::sin(t * 1.5f) * 20);
}
} // namespace

void UpdateTitle(Game& g) {
    PlayerInput in = MenuInput();
    int n;
    const TitleItem* items = TitleItems(g.titleMenu, n);
    (void)items;
    if (in.up) { g.titleSel = (g.titleSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.titleSel = (g.titleSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.back && g.titleMenu != 0) { PlaySfx(Sfx::MenuBack); g.titleSel = g.titleMenu; g.titleMenu = 0; return; }
    if (!in.confirm) return;
    PlaySfx(Sfx::MenuOk);
    if (g.titleMenu == 1) {
        switch (g.titleSel) {
            case 0: StartCoopFlow(g); break;
            case 1: StartOnlineFlow(g); break;
            default: g.titleMenu = 0; g.titleSel = 1; break;
        }
        return;
    }
    if (g.titleMenu == 2) {
        switch (g.titleSel) {
            case 0: StartQuickFlow(g, Mode::Quick); break;
            case 1: StartQuickFlow(g, Mode::TwoPlayer); break;
            case 2: g.doublesSel = 0; g.screen = Screen::DoublesMenu; break;
            case 3: StartQuickFlow(g, Mode::Watch); break;
            default: g.titleMenu = 0; g.titleSel = 2; break;
        }
        return;
    }
    switch (g.titleSel) {
        case 0:
            g.profiles = ListProfiles();
            g.profSel = 0;
            g.confirmDelete = false;
            g.screen = Screen::ProfileSelect;
            break;
        case 1: g.titleMenu = 1; g.titleSel = 0; break;
        case 2: g.titleMenu = 2; g.titleSel = 0; break;
        case 3: g.settingsSel = 0; g.settingsBack = Screen::Title; g.screen = Screen::Settings; break;
        default: g.quit = true; break;
    }
}

void DrawTitle(const Game& g) {
    float t = g.t;
    // Background: a deep sky with light rays, a giant Poke Ball and a grassy field.
    for (int y = 0; y < VH; y++) {
        float u = y / (float)VH;
        Color c{(unsigned char)(18 + 30 * u), (unsigned char)(26 + 40 * u), (unsigned char)(70 + 40 * u), 255};
        DrawRectangle(0, y, VW, 1, c);
    }
    BeginBlendMode(BLEND_ADDITIVE);
    Vector2 sun{VW / 2.0f, 70};
    for (int k = 0; k < 14; k++) {
        float a = k * 0.4488f + t * 0.06f;
        Vector2 p1{sun.x + std::cos(a - 0.05f) * 700, sun.y + std::sin(a - 0.05f) * 700};
        Vector2 p2{sun.x + std::cos(a + 0.05f) * 700, sun.y + std::sin(a + 0.05f) * 700};
        DrawTriangle(sun, p2, p1, Fade(Color{120, 160, 255, 255}, 0.05f));
        DrawTriangle(sun, p1, p2, Fade(Color{120, 160, 255, 255}, 0.05f));
    }
    EndBlendMode();
    TitleBall(VW - 60.0f, 210, 120, t * 8, 34);   // faint, slowly turning
    TitleBall(54.0f, 120, 70, -t * 10, 22);
    for (int i = 0; i < 40; i++) {   // drifting sparkles
        float x = std::fmod(i * 97.3f + t * (6 + i % 5 * 3), (float)VW);
        float y = std::fmod(i * 41.7f + std::sin(t + i) * 6, 250.0f);
        float a = 0.3f + 0.3f * std::sin(t * 3 + i);
        DrawRectangle((int)x, (int)y, 1, 1, Fade(WHITE, a));
    }
    // the field the parade walks on
    DrawRectangle(0, 262, VW, VH - 262, Color{46, 112, 62, 255});
    DrawRectangle(0, 262, VW, 3, Color{96, 176, 90, 255});
    for (int i = 0; i < 64; i++) {
        int x = (int)std::fmod(i * 23.0f - t * 20.0f + 1000.0f * VW, (float)VW);
        DrawRectangle(x, 268 + (i * 7) % 12, 2, 2, Color{70, 146, 78, 255});
    }

    DrawLogo(VW / 2, 10, t);
    DrawTextCentered("Catch them. Train them. Battle in real time.", VW / 2, 92, 10, Color{200, 220, 255, 255}, false);

    // Menu
    int n;
    const TitleItem* items = TitleItems(g.titleMenu, n);
    int y0 = g.titleMenu == 0 ? 112 : 108;
    if (g.titleMenu != 0) DrawTextCentered(g.titleMenu == 1 ? "CO-OP" : "QUICK BATTLE", VW / 2, y0 - 2, 10, UI_GOLD, false);
    if (g.titleMenu != 0) y0 += 12;
    int rowH = n > 4 ? 26 : 28;
    for (int i = 0; i < n; i++) {
        bool sel = g.titleSel == i;
        int w = 220, h = 22, x = VW / 2 - w / 2, y = y0 + i * rowH;
        bool back = std::string(items[i].label) == "Back";
        Color fill = sel ? Color{255, 214, 60, 255} : Color{24, 28, 52, 230};
        Color edge = sel ? Color{255, 250, 210, 255} : Color{86, 104, 170, 255};
        DrawRectangle(x + 2, y + 3, w, h, Fade(BLACK, 0.35f));
        DrawRectangleRounded({(float)x, (float)y, (float)w, (float)h}, 0.5f, 8, fill);
        DrawRectangleRoundedLines({(float)x, (float)y, (float)w, (float)h}, 0.5f, 8, edge);
        Color tc = sel ? Color{40, 30, 20, 255} : (back ? UI_DIM : UI_TEXT);
        DrawTextCentered(items[i].label, VW / 2, y + 6, 10, tc, false);
        bool sub = g.titleMenu == 0 && (i == 1 || i == 2);
        if (sub) DrawText(">", x + w - 16, y + 6, 10, tc);
        if (sel) {
            float bob = std::sin(t * 6) * 1.5f;
            TitleBall((float)x + 12 + bob, (float)y + h / 2.0f, 6, std::sin(t * 4) * 25);
        }
    }
    int sel = std::max(0, std::min(n - 1, g.titleSel));
    if (items[sel].hint[0]) {
        int y = y0 + n * rowH + 2;
        if (y < 252) DrawTextCentered(items[sel].hint, VW / 2, y, 10, Color{170, 190, 230, 255}, false);
    }

    // The parade of Pokemon across the field
    static std::vector<int> parade;
    if (parade.empty() || g.t < 0.05f) {
        parade.clear();
        for (int i = 0; i < (int)AllSpecies().size(); i++) if (InSpeciesPool(i)) parade.push_back(i);
    }
    int pn = (int)parade.size();
    const int spacing = 35;
    float total = (float)pn * spacing;
    float scroll = pn > 18 ? std::fmod(g.t * 20.0f, total) : 0.0f;
    for (int i = 0; i < pn; i++) {
        float fx = 22 + i * spacing - scroll;
        if (pn > 18 && fx < -20) fx += total;
        if (fx < -20 || fx > VW + 20) continue;
        int x = (int)fx;
        int hop = ((int)(g.t * 4 + i * 0.7f) % 4 == 0) ? 3 : 0;
        DrawEllipse(x, 296, 10, 3, Fade(BLACK, 0.3f));
        DrawSpeciesFit(parade[i], VIEW_FRONT, x, 298 - hop, 32);
    }
    DrawRectangle(0, 318, VW, VH - 318, Fade(Color{10, 14, 30, 255}, 0.85f));
    DrawText(g.titleMenu ? "Up/Down choose   Enter confirm   Esc / B back" : "Up/Down choose   Enter / A confirm", 12, 330, 10, UI_DIM);
    if (g.customCount > 0) {
        const char* msg = TextFormat("%d Pokemon", g.customCount);
        DrawText(msg, VW - 12 - TextW(msg, 10), 330, 10, Color{140, 220, 150, 255});
    }
    DrawText("v2.0", VW - 12 - TextW("v2.0", 10), 344, 10, Fade(UI_DIM, 0.7f));
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
        case 7: std::sprintf(label, "Effects intensity"); std::sprintf(value, "%s", c.fxLevel == 0 ? "Full" : c.fxLevel == 1 ? "Reduced" : "Minimal"); break;
        case 8: std::sprintf(label, "Screen filter"); std::sprintf(value, "%s", c.screenFilter == 0 ? "Off" : c.screenFilter == 1 ? "Soft glow" : "Retro CRT"); break;
        case 9: std::sprintf(label, "Team size"); std::sprintf(value, "%d", c.teamSize); break;
        case 10: std::sprintf(label, "CPU skill (Quick Battle / Training)"); std::sprintf(value, "%s", AILevelName(c.cpuLevel)); break;
        case 11: std::sprintf(label, "Arena (Quick Battle / 2 Players)"); std::sprintf(value, "%s", c.quickArena < 0 ? "Random" : c.quickArena < ARENA_COUNT ? ArenaName((ArenaKind)c.quickArena) : VenueName((Venue)(c.quickArena - ARENA_COUNT + 1))); break;
        case 12: std::sprintf(label, "Controls..."); value[0] = 0; break;
        default: std::sprintf(label, "Back"); value[0] = 0; break;
    }
}

void UpdateSettings(Game& g) {
    PlayerInput in = MenuInput();
    Settings& c = Cfg();
    if (in.up) { g.settingsSel = (g.settingsSel + SET_N - 1) % SET_N; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.settingsSel = (g.settingsSel + 1) % SET_N; PlaySfx(Sfx::MenuMove); }
    if (in.back) { SaveSettings(); PlaySfx(Sfx::MenuBack); g.screen = g.settingsBack; g.settingsBack = Screen::Title; return; }
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
        case 7: c.fxLevel = (c.fxLevel + step + 3) % 3; break;
        case 8: c.screenFilter = (c.screenFilter + step + 3) % 3; break;
        case 9: c.teamSize = ((c.teamSize - 1 + step + MAX_TEAM_SIZE) % MAX_TEAM_SIZE) + 1; break;
        case 10: c.cpuLevel = (AILevel)(((int)c.cpuLevel + step + 3) % 3); break;
        case 11: { const int n = ARENA_COUNT + VENUE_COUNT - 1 + 1; c.quickArena = ((c.quickArena + 1 + step + n) % n) - 1; } break;
        case 12:
            changed = false;
            if (in.confirm) { g.ctrlTab = 0; g.ctrlRow = 1; g.ctrlCol = 0; g.capturing = false; g.screen = Screen::Controls; }
            break;
        default:
            changed = false;
            if (in.confirm) { SaveSettings(); g.screen = g.settingsBack; g.settingsBack = Screen::Title; }
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
    const char* groups[] = {"Display", "", "Sound", "", "Gameplay", "", "", "Visuals", "", "Battles", "", "", "Controls", ""};
    for (int i = 0; i < SET_N; i++) {
        char label[64], value[32];
        SettingText(i, label, value);
        int y = 38 + i * 21;
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
        int y = y0 + 10 + a * 16;
        bool rowSel = g.ctrlRow == a + 1;
        DrawPanel(x, y, w, 15, rowSel ? UI_PANEL_LIGHT : UI_PANEL, rowSel ? UI_GOLD : Color{70, 64, 92, 255});
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
    int ry = y0 + 10 + ACT_COUNT * 16 + 4;
    DrawMenuItem("Reset this page to defaults", VW / 2, ry, 10, g.ctrlRow == ACT_COUNT + 1);
    DrawMenuItem("Back", VW / 2, ry + 16, 10, g.ctrlRow == ACT_COUNT + 2);
    DrawText(g.capturing ? "Esc cancels   Backspace clears the binding"
                         : "Up/Down choose   Left/Right column or page   Enter rebind   Esc back   Red = used twice",
             40, 344, 10, UI_DIM);
    DrawToast(g);
}
