// Emerald's Easy Chat: build a phrase from fixed words (World/easychat.txt, tools/emerald_easychat.py) - the Bard's
// song, Dewford's trendy saying, the Lilycove Lady's quiz answer. The slots across the top, then a group, then a word.
// Done: back to the walked world, and the action that asked for it runs with the words ("then|word+word+...").
#include <algorithm>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "input.h"
#include "raylib.h"
#include "ui.h"
#include "embedded.h"

void OwRunAction(Game& g, int i, const std::string& act);

namespace {
struct EcGroup { std::string name; std::vector<std::string> words; };
std::vector<EcGroup> gGroups;
struct EcState {
    std::vector<std::string> slots;
    std::string title, then;
    int who = 0;
    int mode = 0;          // 0 the slots, 1 a group, 2 a word
    int slot = 0, group = 0, word = 0, scroll = 0;
} gE;
constexpr int COLS = 3, ROWS = 9;

void LoadGroups() {
    if (!gGroups.empty()) return;
    char* txt = LoadFileText((DataDir() + "World/easychat.txt").c_str());
    if (!txt) return;
    std::string s = txt;
    UnloadFileText(txt);
    EcGroup* cur = nullptr;
    size_t at = 0;
    while (at <= s.size()) {
        size_t nl = s.find('\n', at);
        std::string line = s.substr(at, nl == std::string::npos ? std::string::npos : nl - at);
        at = nl == std::string::npos ? s.size() + 1 : nl + 1;
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') { gGroups.push_back({line.substr(1, line.find(']') - 1), {}}); cur = &gGroups.back(); continue; }
        if (!cur) continue;
        size_t p = 0;
        while (p < line.size()) {
            size_t c = line.find(", ", p);
            cur->words.push_back(line.substr(p, c == std::string::npos ? std::string::npos : c - p));
            p = c == std::string::npos ? line.size() : c + 2;
        }
    }
}
std::string Phrase() {
    std::string s;
    for (const auto& w : gE.slots) { if (w.empty()) continue; if (!s.empty()) s += " "; s += w; }
    return s;
}
}  // namespace

void OpenEasyChat(Game& g, int who, int slots, const std::string& title, const std::string& then, const std::vector<std::string>& start) {
    LoadGroups();
    gE = EcState{};
    gE.slots.assign(std::max(1, std::min(6, slots)), "");
    for (size_t k = 0; k < start.size() && k < gE.slots.size(); k++) gE.slots[k] = start[k];
    gE.title = title; gE.then = then; gE.who = who;
    g.screen = Screen::EasyChat;
    PlaySfx(Sfx::MenuOk);
}

void UpdateEasyChat(Game& g) {
    PlayerInput in = MenuInput();
    int N = (int)gE.slots.size();
    auto finish = [&](bool ok) {
        g.screen = Screen::World;
        std::string words;
        for (const auto& w : gE.slots) words += (words.empty() ? "" : "+") + w;
        PlaySfx(ok ? Sfx::MenuOk : Sfx::MenuBack);
        OwRunAction(g, gE.who, gE.then + "|" + (ok ? words : std::string("")) + "|" + (ok ? "ok" : "cancel"));
    };
    if (gE.mode == 0) {   // the slots and OK
        if (in.left) { gE.slot = (gE.slot + N) % (N + 1); PlaySfx(Sfx::MenuMove); }
        if (in.right) { gE.slot = (gE.slot + 1) % (N + 1); PlaySfx(Sfx::MenuMove); }
        if (in.back) { finish(false); return; }
        if (in.confirm) {
            if (gE.slot == N) { if (Phrase().empty()) { PlaySfx(Sfx::MenuBack); return; } finish(true); return; }
            gE.mode = 1; PlaySfx(Sfx::MenuOk);
        }
        return;
    }
    if (gGroups.empty()) { gE.mode = 0; return; }
    if (gE.mode == 1) {   // a group
        int G = (int)gGroups.size();
        if (in.up) { gE.group = (gE.group + G - 1) % G; PlaySfx(Sfx::MenuMove); }
        if (in.down) { gE.group = (gE.group + 1) % G; PlaySfx(Sfx::MenuMove); }
        if (in.back) { gE.mode = 0; PlaySfx(Sfx::MenuBack); return; }
        if (in.left) { gE.slots[gE.slot].clear(); gE.mode = 0; PlaySfx(Sfx::MenuBack); return; }   // (left: clear the slot)
        if (in.confirm || in.right) { gE.mode = 2; gE.word = 0; gE.scroll = 0; PlaySfx(Sfx::MenuOk); }
        return;
    }
    const auto& ws = gGroups[gE.group].words;   // a word
    int W = (int)ws.size();
    if (in.left) { if (gE.word % COLS == 0) { gE.mode = 1; PlaySfx(Sfx::MenuBack); return; } gE.word--; PlaySfx(Sfx::MenuMove); }
    if (in.right && gE.word + 1 < W) { gE.word++; PlaySfx(Sfx::MenuMove); }
    if (in.up && gE.word >= COLS) { gE.word -= COLS; PlaySfx(Sfx::MenuMove); }
    if (in.down && gE.word + COLS < W) { gE.word += COLS; PlaySfx(Sfx::MenuMove); }
    int row = gE.word / COLS;
    if (row < gE.scroll) gE.scroll = row;
    if (row >= gE.scroll + ROWS) gE.scroll = row - ROWS + 1;
    if (in.back) { gE.mode = 1; PlaySfx(Sfx::MenuBack); return; }
    if (in.confirm && W > 0) {
        gE.slots[gE.slot] = ws[gE.word];
        gE.mode = 0;
        gE.slot = std::min((int)gE.slots.size(), gE.slot + 1);
        PlaySfx(Sfx::MenuOk);
    }
}

void DrawEasyChat(const Game& g) {
    (void)g;
    ClearBackground(Color{36, 70, 60, 255});
    DrawTextCentered(gE.title.c_str(), VW / 2, 10, 10, UI_GOLD);
    int N = (int)gE.slots.size();
    int sw = std::min(100, (VW - 120) / std::max(1, N));
    int x0 = (VW - (N * (sw + 6) + 50)) / 2;
    for (int k = 0; k <= N; k++) {   // the slots, then OK
        int x = x0 + k * (sw + 6), w = k == N ? 44 : sw;
        bool sel = gE.mode == 0 && gE.slot == k, edit = gE.mode > 0 && gE.slot == k;
        DrawPanel(x, 30, w, 24, sel ? Color{90, 130, 90, 255} : edit ? Color{70, 90, 140, 255} : Color{30, 40, 36, 245});
        std::string t = k == N ? "OK" : (gE.slots[k].empty() ? "- - -" : gE.slots[k]);
        DrawTextCentered(t.c_str(), x + w / 2, 37, 10, k == N ? UI_GOLD : (gE.slots[k].empty() ? UI_DIM : UI_TEXT), false);
    }
    DrawTextCentered(Phrase().empty() ? "(pick a word for each slot)" : ("\"" + Phrase() + "\"").c_str(), VW / 2, 62, 10, UI_TEXT, false);
    // the groups
    DrawPanel(10, 80, 130, VH - 110, Color{24, 34, 30, 245});
    int G = (int)gGroups.size();
    for (int k = 0; k < G; k++) {
        int y = 86 + k * 12;
        if (y > VH - 40) break;
        bool sel = gE.group == k && gE.mode >= 1;
        if (sel) DrawRectangle(12, y - 1, 126, 12, gE.mode == 1 ? Color{90, 130, 90, 255} : Color{50, 70, 60, 255});
        GDrawText(gGroups[k].name.c_str(), 18, y, 10, sel ? UI_GOLD : UI_TEXT);
    }
    // the words
    DrawPanel(146, 80, VW - 156, VH - 110, Color{24, 34, 30, 245});
    if (gE.mode >= 1 && gE.group < G) {
        const auto& ws = gGroups[gE.group].words;
        int cw = (VW - 170) / COLS;
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++) {
                int i = (gE.scroll + r) * COLS + c;
                if (i >= (int)ws.size()) break;
                int x = 152 + c * cw, y = 88 + r * 20;
                bool sel = gE.mode == 2 && gE.word == i;
                if (sel) DrawRectangle(x - 3, y - 3, cw - 4, 16, Color{90, 130, 90, 255});
                GDrawText(ws[i].c_str(), x, y, 10, sel ? UI_GOLD : UI_TEXT);
            }
        int rowsAll = ((int)ws.size() + COLS - 1) / COLS;
        if (rowsAll > ROWS) {
            float f0 = (float)gE.scroll / rowsAll, f1 = (float)(gE.scroll + ROWS) / rowsAll;
            DrawRectangle(VW - 14, 84 + (int)(f0 * (VH - 120)), 2, std::max(4, (int)((f1 - f0) * (VH - 120))), UI_DIM);
        }
    }
    GDrawText(gE.mode == 0 ? "Left/Right: a slot   Enter: choose its word (or OK)   Esc: cancel"
              : gE.mode == 1 ? "Up/Down: a group   Enter: its words   Left: clear the slot   Esc: back"
                             : "Arrows: a word   Enter: use it   Esc: back to the groups", 10, VH - 22, 10, UI_DIM);
}

#ifdef ARENA_AUTOTEST
void EasyChatAutoFill(Game& g, const std::vector<std::string>& words) {   // (the autotests: these words, then OK)
    for (size_t k = 0; k < words.size() && k < gE.slots.size(); k++) gE.slots[k] = words[k];
    g.screen = Screen::World;
    std::string w;
    for (const auto& s : gE.slots) w += (w.empty() ? "" : "+") + s;
    OwRunAction(g, gE.who, gE.then + "|" + w + "|ok");
}
#endif

// A word from an Easy Chat group, picked by seed (the Hipster's word of the day, Giddy's chatter, Dewford's starting trend).
std::string EasyChatWordOf(const std::string& group, uint32_t seed) {
    LoadGroups();
    for (const auto& gr : gGroups)
        if (gr.name == group && !gr.words.empty()) return gr.words[(seed * 2654435761u >> 9) % gr.words.size()];
    return "Pokemon";
}
