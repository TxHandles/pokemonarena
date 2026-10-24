// Voltorb Flip (HeartGold / SoulSilver's Goldenrod Game Corner): a 5 x 5 board of face-down cards - 1, 2, 3 or a
// Voltorb. Each row and column says what its cards add up to and how many Voltorb hide in it. Flip cards to multiply
// your coins for the round; flip every 2 and 3 to win them (and go up a level), flip a Voltorb and they're gone (and
// the level drops to how many cards you'd flipped). Quit any time to keep what you have. Coins go to the Game Corner's.
#include <algorithm>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "input.h"
#include "ui.h"

namespace {
struct Board {
    int card[25] = {};      // 0 Voltorb, 1, 2, 3
    bool up[25] = {};
    int cur = 12;
    int level = 1;
    int coins = 0;          // this round's
    int flipped = 0;
    int phase = 0;          // 0 playing, 1 won, 2 lost
    float t = 0;
    std::string note;
} gV;
// HeartGold's boards: for each level, five layouts of (2s, 3s, Voltorb) - the rest are 1s
const int kLayouts[8][5][3] = {
    {{3, 1, 6}, {0, 3, 6}, {5, 0, 6}, {2, 2, 6}, {4, 1, 6}},
    {{1, 3, 7}, {6, 0, 7}, {3, 2, 7}, {0, 4, 7}, {5, 1, 7}},
    {{2, 3, 8}, {7, 0, 8}, {4, 2, 8}, {1, 4, 8}, {6, 1, 8}},
    {{3, 3, 8}, {0, 5, 8}, {8, 0, 10}, {5, 2, 10}, {2, 4, 10}},
    {{7, 1, 10}, {4, 3, 10}, {1, 5, 10}, {9, 0, 10}, {6, 2, 10}},
    {{3, 4, 10}, {0, 6, 10}, {8, 1, 10}, {5, 3, 10}, {2, 5, 10}},
    {{7, 2, 10}, {4, 4, 10}, {1, 6, 13}, {9, 1, 13}, {6, 3, 10}},
    {{0, 7, 10}, {8, 2, 10}, {5, 4, 10}, {2, 6, 10}, {7, 3, 10}},
};
bool RoundWon() {
    for (int k = 0; k < 25; k++) if (gV.card[k] >= 2 && !gV.up[k]) return false;
    return true;
}
void Deal(int level) {
    gV.level = std::max(1, std::min(8, level));
    const int* L = kLayouts[gV.level - 1][GetRandomValue(0, 4)];
    std::vector<int> deck;
    for (int k = 0; k < L[0]; k++) deck.push_back(2);
    for (int k = 0; k < L[1]; k++) deck.push_back(3);
    for (int k = 0; k < L[2]; k++) deck.push_back(0);
    while (deck.size() < 25) deck.push_back(1);
    for (int k = 24; k > 0; k--) std::swap(deck[k], deck[GetRandomValue(0, k)]);
    for (int k = 0; k < 25; k++) { gV.card[k] = deck[k]; gV.up[k] = false; }
    gV.coins = 0; gV.flipped = 0; gV.phase = 0; gV.t = 0;
    gV.note = TextFormat("Level %d. Flip the cards and find all the 2s and 3s!", gV.level);
}
constexpr int kCard = 36, kGap = 5;
int BX() { return 40; }
int BY() { return 46; }
void Bank(Game& g, int coins) {
    if (coins <= 0) return;
    g.profile.casino = std::min(99999, g.profile.casino + coins);
    SaveProfile(g.profile);
}
}  // namespace

void OpenVoltorbFlip(Game& g) {
    Deal(gV.level > 0 ? gV.level : 1);
    g.screen = Screen::VoltorbFlip;
    PlaySfx(Sfx::MenuOk);
}

void UpdateVoltorbFlip(Game& g) {
    PlayerInput in = MenuInput();
    gV.t += GetFrameTime();
    if (gV.phase != 0) {   // a round over: show the board, then deal the next
        if (gV.t > 0.8f && in.confirm) { PlaySfx(Sfx::MenuOk); Deal(gV.phase == 1 ? gV.level + 1 : std::min(gV.level, std::max(1, gV.flipped))); }
        if (gV.t > 0.8f && in.back) { g.screen = Screen::World; PlaySfx(Sfx::MenuBack); }
        return;
    }
    if (in.back) {   // quit: keep this round's coins
        Bank(g, gV.coins);
        if (gV.coins > 0) Toast(g, TextFormat("You kept %d coins.", gV.coins));
        g.screen = Screen::World;
        PlaySfx(Sfx::MenuBack);
        return;
    }
    int cx = gV.cur % 5, cy = gV.cur / 5;
    if (in.left) cx = (cx + 4) % 5;
    if (in.right) cx = (cx + 1) % 5;
    if (in.up) cy = (cy + 4) % 5;
    if (in.down) cy = (cy + 1) % 5;
    if (cy * 5 + cx != gV.cur) { gV.cur = cy * 5 + cx; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm || gV.up[gV.cur]) return;
    gV.up[gV.cur] = true;
    int v = gV.card[gV.cur];
    gV.t = 0;
    if (v == 0) {   // a Voltorb!
        PlaySfx(Sfx::Faint);
        gV.phase = 2;
        for (int k = 0; k < 25; k++) gV.up[k] = true;
        int next = std::min(gV.level, std::max(1, gV.flipped));
        gV.note = TextFormat("Oh no! You get 0 coins!  (Next: level %d)  Enter: play again   Esc: quit", next);
        gV.coins = 0;
        return;
    }
    gV.flipped++;
    gV.coins = gV.coins == 0 ? v : gV.coins * v;
    PlaySfx(v > 1 ? Sfx::ItemGet : Sfx::MenuMove);
    if (RoundWon()) {
        gV.phase = 1;
        Bank(g, gV.coins);
        for (int k = 0; k < 25; k++) gV.up[k] = true;
        gV.note = TextFormat("Game clear! You got %d coins!  (Next: level %d)  Enter: play again   Esc: quit", gV.coins, std::min(8, gV.level + 1));
    }
}

void DrawVoltorbFlip(const Game& g) {
    ClearBackground(Color{24, 98, 74, 255});
    DrawTextCentered("Voltorb Flip", VW / 2, 10, 10, UI_GOLD);
    GDrawText(TextFormat("Level %d", gV.level), 40, 30, 10, UI_TEXT);
    GDrawText(TextFormat("This round: %d", gV.coins), 150, 30, 10, UI_GOLD);
    GDrawText(TextFormat("Your coins: %d", g.profile.casino), 300, 30, 10, UI_TEXT);
    static const Color kRow[5] = {{230, 112, 80, 255}, {100, 180, 70, 255}, {240, 170, 60, 255}, {70, 140, 220, 255}, {190, 100, 210, 255}};
    auto hint = [&](int x, int y, int sum, int volt, Color c) {
        DrawRectangle(x, y, kCard, kCard, c);
        DrawRectangleLines(x, y, kCard, kCard, Color{30, 30, 30, 255});
        GDrawText(TextFormat("%02d", sum), x + kCard - 18, y + 4, 10, Color{20, 20, 20, 255});
        DrawCircle(x + 11, y + kCard - 11, 7, Color{220, 50, 40, 255});
        DrawRectangle(x + 4, y + kCard - 11, 14, 7, RAYWHITE);
        DrawCircleLines(x + 11, y + kCard - 11, 7, Color{20, 20, 20, 255});
        GDrawText(TextFormat("%d", volt), x + kCard - 12, y + kCard - 16, 10, Color{20, 20, 20, 255});
    };
    for (int r = 0; r < 5; r++) {
        int sumR = 0, vR = 0, sumC = 0, vC = 0;
        for (int k = 0; k < 5; k++) {
            sumR += gV.card[r * 5 + k]; vR += gV.card[r * 5 + k] == 0;
            sumC += gV.card[k * 5 + r]; vC += gV.card[k * 5 + r] == 0;
        }
        hint(BX() + 5 * (kCard + kGap), BY() + r * (kCard + kGap), sumR, vR, kRow[r]);
        hint(BX() + r * (kCard + kGap), BY() + 5 * (kCard + kGap), sumC, vC, kRow[r]);
    }
    for (int k = 0; k < 25; k++) {
        int x = BX() + (k % 5) * (kCard + kGap), y = BY() + (k / 5) * (kCard + kGap);
        if (!gV.up[k]) {
            DrawRectangle(x, y, kCard, kCard, Color{40, 150, 110, 255});
            DrawRectangleLines(x + 3, y + 3, kCard - 6, kCard - 6, Color{120, 210, 170, 255});
        } else if (gV.card[k] == 0) {   // a Voltorb
            DrawRectangle(x, y, kCard, kCard, Color{236, 226, 196, 255});
            DrawCircle(x + kCard / 2, y + kCard / 2, 15, Color{220, 50, 40, 255});
            DrawRectangle(x + kCard / 2 - 15, y + kCard / 2, 30, 15, RAYWHITE);
            DrawCircleLines(x + kCard / 2, y + kCard / 2, 15, Color{20, 20, 20, 255});
            DrawRectangle(x + kCard / 2 - 9, y + kCard / 2 - 6, 5, 4, Color{20, 20, 20, 255});
            DrawRectangle(x + kCard / 2 + 4, y + kCard / 2 - 6, 5, 4, Color{20, 20, 20, 255});
        } else {
            DrawRectangle(x, y, kCard, kCard, Color{236, 226, 196, 255});
            DrawTextCentered(TextFormat("%d", gV.card[k]), x + kCard / 2, y + kCard / 2 - 6, 10, Color{40, 40, 40, 255}, false);
        }
        DrawRectangleLines(x, y, kCard, kCard, Color{20, 60, 45, 255});
    }
    if (gV.phase == 0) {
        float a = 0.6f + 0.4f * std::sin((float)GetTime() * 6);
        int x = BX() + (gV.cur % 5) * (kCard + kGap) - 3, y = BY() + (gV.cur / 5) * (kCard + kGap) - 3;
        DrawRectangleLinesEx({(float)x, (float)y, (float)kCard + 6, (float)kCard + 6}, 3, Fade(Color{255, 230, 90, 255}, a));
    }
    int px = BX() + 6 * (kCard + kGap) + 10;
    DrawPanel(px, BY(), VW - px - 20, 150);
    DrawWrapped("Each row and column shows its cards' total and how many Voltorb it hides. Flip every 2 and 3 to clear the level - "
                "the coins multiply with each card. A Voltorb ends the round.", px + 8, BY() + 8, VW - px - 36, 10, UI_TEXT, 13);
    DrawWrapped(gV.note.c_str(), 40, VH - 44, VW - 80, 10, gV.phase == 2 ? Color{255, 140, 120, 255} : UI_GOLD, 13);
    if (gV.phase == 0) GDrawText("Arrows  move    Enter  flip    Esc  quit (keep the coins)", 40, VH - 16, 10, UI_DIM);
}
