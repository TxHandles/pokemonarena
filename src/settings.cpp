#include "settings.h"
#include "embedded.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include "audio.h"
#include "raylib.h"

namespace {

Settings gCfg;

const char* kActKeys[ACT_COUNT] = {"up", "down", "left", "right", "move1", "move2", "move3", "move4", "swap", "pause", "dodge", "jump", "swap_back"};
const char* kLayoutKeys[LAYOUT_COUNT] = {"solo", "p1", "p2"};

std::string Path() { return DataDir() + "settings.txt"; }

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

} // namespace

Settings& Cfg() { return gCfg; }

void ResetKeys(int layout) {
    auto set = [&](Act a, int k1, int k2 = 0) {
        gCfg.keys[layout][(int)a][0] = k1;
        gCfg.keys[layout][(int)a][1] = k2;
    };
    if (layout == LAYOUT_SOLO) {
        set(Act::Up, KEY_W, KEY_UP);
        set(Act::Down, KEY_S, KEY_DOWN);
        set(Act::Left, KEY_A, KEY_LEFT);
        set(Act::Right, KEY_D, KEY_RIGHT);
        set(Act::Move1, KEY_J, KEY_F);
        set(Act::Move2, KEY_K, KEY_G);
        set(Act::Move3, KEY_L, KEY_H);
        set(Act::Move4, KEY_I, KEY_T);
        set(Act::Swap, KEY_U, KEY_Q);
        set(Act::Pause, KEY_ESCAPE, KEY_P);
        set(Act::Dodge, KEY_SPACE, KEY_LEFT_SHIFT);
        set(Act::Jump, KEY_E, KEY_O);
        set(Act::SwapPrev, KEY_Y, KEY_Z);
    } else if (layout == LAYOUT_P1) {
        set(Act::Up, KEY_W);
        set(Act::Down, KEY_S);
        set(Act::Left, KEY_A);
        set(Act::Right, KEY_D);
        set(Act::Move1, KEY_F);
        set(Act::Move2, KEY_G);
        set(Act::Move3, KEY_H);
        set(Act::Move4, KEY_T);
        set(Act::Swap, KEY_Q);
        set(Act::Pause, KEY_ESCAPE, KEY_P);
        set(Act::Dodge, KEY_E, KEY_LEFT_SHIFT);
        set(Act::Jump, KEY_R);
        set(Act::SwapPrev, KEY_Z);
    } else {
        set(Act::Up, KEY_UP);
        set(Act::Down, KEY_DOWN);
        set(Act::Left, KEY_LEFT);
        set(Act::Right, KEY_RIGHT);
        set(Act::Move1, KEY_KP_1, KEY_COMMA);
        set(Act::Move2, KEY_KP_2, KEY_PERIOD);
        set(Act::Move3, KEY_KP_3, KEY_SLASH);
        set(Act::Move4, KEY_KP_4, KEY_SEMICOLON);
        set(Act::Swap, KEY_KP_0, KEY_RIGHT_SHIFT);
        set(Act::Pause, KEY_KP_ENTER);
        set(Act::Dodge, KEY_KP_5, KEY_L);
        set(Act::Jump, KEY_KP_6, KEY_APOSTROPHE);
        set(Act::SwapPrev, KEY_KP_DECIMAL, KEY_RIGHT_BRACKET);
    }
}

void ResetPad(int p) {
    int* b = gCfg.pad[p];
    b[(int)Act::Up] = GAMEPAD_BUTTON_LEFT_FACE_UP;
    b[(int)Act::Down] = GAMEPAD_BUTTON_LEFT_FACE_DOWN;
    b[(int)Act::Left] = GAMEPAD_BUTTON_LEFT_FACE_LEFT;
    b[(int)Act::Right] = GAMEPAD_BUTTON_LEFT_FACE_RIGHT;
    b[(int)Act::Move1] = GAMEPAD_BUTTON_RIGHT_FACE_DOWN;
    b[(int)Act::Move2] = GAMEPAD_BUTTON_RIGHT_FACE_RIGHT;
    b[(int)Act::Move3] = GAMEPAD_BUTTON_RIGHT_FACE_LEFT;
    b[(int)Act::Move4] = GAMEPAD_BUTTON_RIGHT_FACE_UP;
    b[(int)Act::Swap] = GAMEPAD_BUTTON_RIGHT_TRIGGER_2;   // RT
    b[(int)Act::Pause] = GAMEPAD_BUTTON_MIDDLE_RIGHT;
    b[(int)Act::Dodge] = GAMEPAD_BUTTON_LEFT_TRIGGER_1;   // LB / L1
    b[(int)Act::Jump] = GAMEPAD_BUTTON_RIGHT_TRIGGER_1;   // RB / R1
    b[(int)Act::SwapPrev] = GAMEPAD_BUTTON_LEFT_TRIGGER_2; // LT: swap back through the party (RT swaps forward)
}

void LoadSettings() {
    gCfg = Settings();
    for (int l = 0; l < LAYOUT_COUNT; l++) ResetKeys(l);
    ResetPad(0);
    ResetPad(1);

    char* text = LoadFileText(Path().c_str());
    if (!text) return;
    std::istringstream ss(text);
    UnloadFileText(text);
    std::string line;
    int version = 1;
    while (std::getline(ss, line)) {
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        int iv = std::atoi(v.c_str());
        bool bv = (v == "1" || v == "yes" || v == "on" || v == "true");
        if (k == "fullscreen") gCfg.fullscreen = bv;
        else if (k == "show_fps") gCfg.showFps = bv;
        else if (k == "master_volume") gCfg.masterVolume = iv < 0 ? 0 : (iv > 100 ? 100 : iv);
        else if (k == "effects_volume") gCfg.sfxVolume = iv < 0 ? 0 : (iv > 100 ? 100 : iv);
        else if (k == "auto_aim") gCfg.autoAim = bv;
        else if (k == "screen_shake") gCfg.screenShake = bv;
        else if (k == "damage_numbers") gCfg.damageNumbers = bv;
        else if (k == "settings_version") version = iv;
        else if (k == "team_size") gCfg.teamSize = iv < 1 ? 1 : (iv > MAX_TEAM_SIZE ? MAX_TEAM_SIZE : iv);
        else if (k == "quick_arena") gCfg.quickArena = iv < -1 ? -1 : (iv >= ARENA_COUNT + VENUE_COUNT - 1 ? ARENA_COUNT - 1 : iv);
        else if (k == "cpu_level") gCfg.cpuLevel = (AILevel)(iv < 0 ? 0 : (iv > 2 ? 2 : iv));
        else if (k.rfind("key.", 0) == 0 || k.rfind("pad", 0) == 0) {
            // key.<layout>.<action> = a,b      pad1.<action> = button
            size_t dot = k.rfind('.');
            std::string act = k.substr(dot + 1);
            int ai = -1;
            for (int i = 0; i < ACT_COUNT; i++)
                if (act == kActKeys[i]) ai = i;
            if (ai < 0) continue;
            if (k[0] == 'k') {
                std::string lay = k.substr(4, dot - 4);
                for (int l = 0; l < LAYOUT_COUNT; l++)
                    if (lay == kLayoutKeys[l]) {
                        size_t c = v.find(',');
                        gCfg.keys[l][ai][0] = std::atoi(v.c_str());
                        gCfg.keys[l][ai][1] = c == std::string::npos ? 0 : std::atoi(v.c_str() + c + 1);
                    }
            } else {
                int p = k[3] == '2' ? 1 : 0;
                gCfg.pad[p][ai] = iv;
            }
        }
    }
    if (version < 2) gCfg.teamSize = MAX_TEAM_SIZE;  // teams opened up to 6
    if (version < 3) {   // Dodge (LB) and Jump (RB) arrived: Swap moves from RB to RT
        for (int p = 0; p < 2; p++) {
            int* b = gCfg.pad[p];
            if (b[(int)Act::Swap] == GAMEPAD_BUTTON_RIGHT_TRIGGER_1) b[(int)Act::Swap] = GAMEPAD_BUTTON_RIGHT_TRIGGER_2;
            for (int a = 0; a < (int)Act::Dodge; a++) {   // an old binding already on LB / RB keeps it
                if (b[a] == b[(int)Act::Dodge]) b[(int)Act::Dodge] = 0;
                if (b[a] == b[(int)Act::Jump]) b[(int)Act::Jump] = 0;
            }
        }
        for (int l = 0; l < LAYOUT_COUNT; l++)
            for (int a = 0; a < (int)Act::Dodge; a++)
                for (int c = 0; c < 2; c++)
                    for (int n = (int)Act::Dodge; n <= (int)Act::Jump; n++)
                        for (int nc = 0; nc < 2; nc++)
                            if (gCfg.keys[l][a][c] && gCfg.keys[l][a][c] == gCfg.keys[l][n][nc]) gCfg.keys[l][n][nc] = 0;
    }
    if (version < 4) {   // Swap (back) arrived: an older binding on the same button / key keeps it
        int n = (int)Act::SwapPrev;
        for (int p = 0; p < 2; p++)
            for (int a = 0; a < n; a++)
                if (gCfg.pad[p][a] == gCfg.pad[p][n]) gCfg.pad[p][n] = 0;
        for (int l = 0; l < LAYOUT_COUNT; l++)
            for (int a = 0; a < n; a++)
                for (int c = 0; c < 2; c++)
                    for (int nc = 0; nc < 2; nc++)
                        if (gCfg.keys[l][a][c] && gCfg.keys[l][a][c] == gCfg.keys[l][n][nc]) gCfg.keys[l][n][nc] = 0;
    }
}

void SaveSettings() {
    std::string o = "# Arena Clash settings (written by the game; edit in the Settings menu)\nsettings_version = 4\n";
    char buf[128];
    std::snprintf(buf, sizeof buf, "fullscreen = %d\nshow_fps = %d\n", gCfg.fullscreen, gCfg.showFps); o += buf;
    std::snprintf(buf, sizeof buf, "master_volume = %d\neffects_volume = %d\n", gCfg.masterVolume, gCfg.sfxVolume); o += buf;
    std::snprintf(buf, sizeof buf, "auto_aim = %d\nscreen_shake = %d\ndamage_numbers = %d\n", gCfg.autoAim, gCfg.screenShake, gCfg.damageNumbers); o += buf;
    std::snprintf(buf, sizeof buf, "team_size = %d\ncpu_level = %d\nquick_arena = %d\n", gCfg.teamSize, (int)gCfg.cpuLevel, gCfg.quickArena); o += buf;
    for (int l = 0; l < LAYOUT_COUNT; l++)
        for (int a = 0; a < ACT_COUNT; a++) {
            std::snprintf(buf, sizeof buf, "key.%s.%s = %d,%d\n", kLayoutKeys[l], kActKeys[a], gCfg.keys[l][a][0], gCfg.keys[l][a][1]);
            o += buf;
        }
    for (int p = 0; p < 2; p++)
        for (int a = 0; a < ACT_COUNT; a++) {
            std::snprintf(buf, sizeof buf, "pad%d.%s = %d\n", p + 1, kActKeys[a], gCfg.pad[p][a]);
            o += buf;
        }
    SaveFileText(Path().c_str(), (char*)o.c_str());
}

void ApplyAudioSettings() { SetSfxVolume(gCfg.masterVolume / 100.0f, gCfg.sfxVolume / 100.0f); }

const char* ActName(Act a) {
    const char* n[ACT_COUNT] = {"Up", "Down", "Left", "Right", "Move 1", "Move 2", "Move 3", "Move 4", "Swap (next)", "Pause", "Dodge", "Jump", "Swap (back)"};
    return n[(int)a];
}

const char* KeyName(int k) {
    if (k == 0) return "-";
    if (k >= KEY_A && k <= KEY_Z) return TextFormat("%c", 'A' + (k - KEY_A));
    if (k >= KEY_ZERO && k <= KEY_NINE) return TextFormat("%c", '0' + (k - KEY_ZERO));
    if (k >= KEY_KP_0 && k <= KEY_KP_9) return TextFormat("Num%d", k - KEY_KP_0);
    if (k >= KEY_F1 && k <= KEY_F12) return TextFormat("F%d", k - KEY_F1 + 1);
    switch (k) {
        case KEY_SPACE: return "Space";
        case KEY_ESCAPE: return "Esc";
        case KEY_ENTER: return "Enter";
        case KEY_TAB: return "Tab";
        case KEY_BACKSPACE: return "Bksp";
        case KEY_UP: return "Up";
        case KEY_DOWN: return "Down";
        case KEY_LEFT: return "Left";
        case KEY_RIGHT: return "Right";
        case KEY_LEFT_SHIFT: return "LShift";
        case KEY_RIGHT_SHIFT: return "RShift";
        case KEY_LEFT_CONTROL: return "LCtrl";
        case KEY_RIGHT_CONTROL: return "RCtrl";
        case KEY_LEFT_ALT: return "LAlt";
        case KEY_RIGHT_ALT: return "RAlt";
        case KEY_KP_ENTER: return "NumEnt";
        case KEY_KP_ADD: return "Num+";
        case KEY_KP_SUBTRACT: return "Num-";
        case KEY_KP_MULTIPLY: return "Num*";
        case KEY_KP_DIVIDE: return "Num/";
        case KEY_KP_DECIMAL: return "Num.";
        case KEY_COMMA: return ",";
        case KEY_PERIOD: return ".";
        case KEY_SLASH: return "/";
        case KEY_SEMICOLON: return ";";
        case KEY_APOSTROPHE: return "'";
        case KEY_LEFT_BRACKET: return "[";
        case KEY_RIGHT_BRACKET: return "]";
        case KEY_MINUS: return "-";
        case KEY_EQUAL: return "=";
        case KEY_BACKSLASH: return "\\";
        case KEY_GRAVE: return "`";
        case KEY_INSERT: return "Ins";
        case KEY_DELETE: return "Del";
        case KEY_HOME: return "Home";
        case KEY_END: return "End";
        case KEY_PAGE_UP: return "PgUp";
        case KEY_PAGE_DOWN: return "PgDn";
        default: return TextFormat("Key%d", k);
    }
}

const char* PadButtonName(int b) {
    switch (b) {
        case 0: return "-";
        case GAMEPAD_BUTTON_LEFT_FACE_UP: return "D-Up";
        case GAMEPAD_BUTTON_LEFT_FACE_RIGHT: return "D-Right";
        case GAMEPAD_BUTTON_LEFT_FACE_DOWN: return "D-Down";
        case GAMEPAD_BUTTON_LEFT_FACE_LEFT: return "D-Left";
        case GAMEPAD_BUTTON_RIGHT_FACE_UP: return "Y";
        case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return "B";
        case GAMEPAD_BUTTON_RIGHT_FACE_DOWN: return "A";
        case GAMEPAD_BUTTON_RIGHT_FACE_LEFT: return "X";
        case GAMEPAD_BUTTON_LEFT_TRIGGER_1: return "LB";
        case GAMEPAD_BUTTON_LEFT_TRIGGER_2: return "LT";
        case GAMEPAD_BUTTON_RIGHT_TRIGGER_1: return "RB";
        case GAMEPAD_BUTTON_RIGHT_TRIGGER_2: return "RT";
        case GAMEPAD_BUTTON_MIDDLE_LEFT: return "Back";
        case GAMEPAD_BUTTON_MIDDLE: return "Home";
        case GAMEPAD_BUTTON_MIDDLE_RIGHT: return "Start";
        case GAMEPAD_BUTTON_LEFT_THUMB: return "LS";
        case GAMEPAD_BUTTON_RIGHT_THUMB: return "RS";
        default: return TextFormat("Btn%d", b);
    }
}
