// Emerald's roulette (Mauville Game Corner, pret/pokeemerald's src/roulette.c): a wheel of twelve slots - Wynaut, Azurill,
// Skitty and Makuhita, each in orange, green and purple. Bet the table's coins on one slot (x12), one Pokemon (its
// three colours: x4, more once some have been hit) or one colour (its four Pokemon: x3, more once some have been hit).
// Six balls a round; a slot a ball has landed in stays taken until the round is over.
#include <algorithm>
#include <cmath>
#include <string>
#include "arena.h"
#include "audio.h"
#include "data.h"
#include "game.h"
#include "input.h"
#include "sprites.h"
#include "ui.h"

namespace {
const char* kMon[4] = {"Wynaut", "Azurill", "Skitty", "Makuhita"};
const char* kColName[3] = {"Orange", "Green", "Purple"};
const Color kCol[3] = {{236, 140, 50, 255}, {80, 180, 90, 255}, {150, 90, 200, 255}};
constexpr int BALLS = 6;
const int kMult[5] = {0, 3, 4, 6, 12};   // (roulette.c's multipliers: a colour starts at 3, a Pokemon at 4)
struct RouletteState {
    int table = 0, minBet = 1;
    int curR = 1, curC = 1;          // the betting grid: row 0 the Pokemon, column 0 the colours, the rest one slot each
    int ballsLeft = BALLS;
    bool hit[3][4] = {};
    int phase = 0;                   // 0 betting, 1 spinning, 2 the result
    float t = 0, wheel = 0, ball = 0, landAt = 0;
    int landR = -1, landC = -1, betR = 1, betC = 1, won = 0;
    std::string msg;
} gR;
// a slot on the wheel: slot k = colour k % 3, Pokemon k / 3 (the wheel's order, going round)
int SlotOf(int r, int c) { return c * 3 + r; }
int Multiplier(int r, int c) {
    if (r == 0 && c == 0) return 0;
    if (r == 0) { int n = 0; for (int k = 0; k < 3; k++) n += gR.hit[k][c - 1]; return n >= 3 ? 0 : kMult[n + 2]; }   // a Pokemon
    if (c == 0) { int n = 0; for (int k = 0; k < 4; k++) n += gR.hit[r - 1][k]; return n >= 4 ? 0 : kMult[n + 1]; }   // a colour
    return gR.hit[r - 1][c - 1] ? 0 : kMult[4];
}
bool Wins(int r, int c, int lr, int lc) {
    if (r == 0) return lc == c - 1;
    if (c == 0) return lr == r - 1;
    return lr == r - 1 && lc == c - 1;
}
void ClearBoard() { for (auto& row : gR.hit) for (bool& b : row) b = false; gR.ballsLeft = BALLS; }
}  // namespace

void OpenRoulette(Game& g, int table) {
    gR = RouletteState{};
    gR.table = table;
    gR.minBet = table == 1 ? 3 : 1;
    g.screen = Screen::Roulette;
    gR.msg = TextFormat("This table's bet: %d coin%s a ball.", gR.minBet, gR.minBet > 1 ? "s" : "");
    PlaySfx(Sfx::MenuOk);
}

void UpdateRoulette(Game& g, float dt) {
    PlayerInput in = MenuInput();
    Profile& p = g.profile;
    gR.t += dt;
    gR.wheel = std::fmod(gR.wheel + dt * (gR.phase == 1 ? 40.0f : 12.0f), 360.0f);
    if (gR.phase == 1) {   // the ball runs round the rim, slowing, and drops into its slot
        float u = std::min(1.0f, gR.t / 2.6f);
        gR.ball = gR.landAt + (1 - u) * (1 - u) * 1440.0f;
        if (u >= 1) {
            gR.phase = 2;
            gR.hit[gR.landR][gR.landC] = true;
            gR.ballsLeft--;
            bool win = Wins(gR.betR, gR.betC, gR.landR, gR.landC);
            std::string where = std::string(kColName[gR.landR]) + " " + kMon[gR.landC];
            if (win) {
                p.casino += gR.won;
                PlaySfx(Sfx::LevelUp);
                gR.msg = TextFormat("%s! It's a hit! You won %d coins!", where.c_str(), gR.won);
            } else {
                PlaySfx(Sfx::MenuBack);
                gR.msg = where + "... Too bad!";
            }
            if (gR.ballsLeft <= 0) gR.msg += "  (That was the last ball - the board is cleared.)";
            StorySaveProfile(g);
        }
        return;
    }
    if (gR.phase == 2) {
        if (in.confirm || in.back) { gR.phase = 0; if (gR.ballsLeft <= 0) ClearBoard(); PlaySfx(Sfx::MenuMove); }
#ifdef ARENA_AUTOTEST
        if (gR.t > 6.0f) { gR.phase = 0; if (gR.ballsLeft <= 0) ClearBoard(); }
#endif
        return;
    }
    if (in.back) { PlaySfx(Sfx::MenuBack); StorySaveProfile(g); g.screen = Screen::World; return; }
    if (in.left) { gR.curC = (gR.curC + 4) % 5; if (gR.curR == 0 && gR.curC == 0) gR.curC = 4; PlaySfx(Sfx::MenuMove); }
    if (in.right) { gR.curC = (gR.curC + 1) % 5; if (gR.curR == 0 && gR.curC == 0) gR.curC = 1; PlaySfx(Sfx::MenuMove); }
    if (in.up) { gR.curR = (gR.curR + 3) % 4; if (gR.curR == 0 && gR.curC == 0) gR.curR = 3; PlaySfx(Sfx::MenuMove); }
    if (in.down) { gR.curR = (gR.curR + 1) % 4; if (gR.curR == 0 && gR.curC == 0) gR.curR = 1; PlaySfx(Sfx::MenuMove); }
#ifdef ARENA_AUTOTEST
    if (gR.t > 0.5f && gR.t < 0.6f) in.confirm = true;   // (one ball on a slot, then out)
    if (gR.t > 1.0f && gR.ballsLeft < BALLS) in.back = true;
    if (in.back) { StorySaveProfile(g); g.screen = Screen::World; return; }
#endif
    if (!in.confirm) return;
    int mult = Multiplier(gR.curR, gR.curC);
    if (mult == 0) { gR.msg = "That bet can't win any more this round - pick another."; PlaySfx(Sfx::MenuBack); return; }
    if (p.casino < gR.minBet) { gR.msg = "You don't have enough coins. Buy some at the counter!"; PlaySfx(Sfx::MenuBack); return; }
    p.casino -= gR.minBet;
    gR.betR = gR.curR; gR.betC = gR.curC;
    gR.won = gR.minBet * mult;
    std::vector<std::pair<int, int>> open;
    for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) if (!gR.hit[r][c]) open.push_back({r, c});
    auto pick = open[GetRandomValue(0, (int)open.size() - 1)];
    gR.landR = pick.first; gR.landC = pick.second;
    gR.landAt = SlotOf(gR.landR, gR.landC) * 30.0f + 15.0f;
    gR.phase = 1;
    gR.t = 0;
    gR.msg = "...!";
    PlaySfx(Sfx::Status);
}

void DrawRoulette(const Game& g) {
    ClearBackground(Color{30, 24, 40, 255});
    for (int x = 0; x < VW; x += 16) DrawCircle(x + 8, 22, 3, ((x / 16 + (int)(g.t * 6)) % 3) ? Color{255, 200, 90, 255} : Color{110, 80, 40, 255});
    DrawTextCentered("MAUVILLE GAME CORNER - ROULETTE", VW / 2, 6, 10, UI_GOLD);
    // the wheel: twelve slots, the ball on the rim
    const float cx = 150, cy = 190, R = 110;
    DrawCircleV({cx, cy}, R + 8, Color{120, 80, 40, 255});
    for (int k = 0; k < 12; k++) {
        float a0 = gR.wheel + k * 30.0f, a1 = a0 + 30.0f;
        int r = k % 3, c = k / 3;
        Color col = kCol[r];
        if (gR.hit[r][c]) col = ColorLerp(col, BLACK, 0.55f);
        DrawCircleSector({cx, cy}, R, a0, a1, 6, col);
        DrawCircleSectorLines({cx, cy}, R, a0, a1, 6, Color{40, 30, 30, 255});
        float am = (a0 + 15.0f) * DEG2RAD;
        int si = SpeciesIndex(kMon[c]);
        if (si >= 0) DrawSpeciesFit(si, VIEW_FRONT, (int)(cx + std::cos(am) * R * 0.68f), (int)(cy + std::sin(am) * R * 0.68f + 12), 26, gR.hit[r][c] ? Fade(WHITE, 0.4f) : WHITE);
    }
    DrawCircleV({cx, cy}, R * 0.3f, Color{90, 60, 30, 255});
    DrawCircleV({cx, cy}, R * 0.22f, Color{200, 160, 60, 255});
    if (gR.phase >= 1) {   // the ball
        float ab = (gR.phase == 1 ? gR.wheel + gR.ball : gR.wheel + gR.landAt) * DEG2RAD;
        float rr = gR.phase == 1 ? R * (0.95f - 0.25f * std::min(1.0f, gR.t / 2.6f)) : R * 0.86f;
        DrawCircleV({cx + std::cos(ab) * rr, cy + std::sin(ab) * rr}, 5, RAYWHITE);
        DrawCircleLinesV({cx + std::cos(ab) * rr, cy + std::sin(ab) * rr}, 5, DARKGRAY);
    }
    // the board: the Pokemon across the top, the colours down the side
    const int bx = 300, by = 60, cw = 64, ch = 46;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 5; c++) {
            if (r == 0 && c == 0) continue;
            int x = bx + c * cw, y = by + r * ch;
            Color fill = r == 0 ? Color{60, 50, 80, 255} : c == 0 ? ColorLerp(kCol[r - 1], BLACK, 0.3f) : ColorLerp(kCol[r - 1], BLACK, gR.hit[r - 1][c - 1] ? 0.7f : 0.1f);
            DrawRectangle(x, y, cw - 4, ch - 4, fill);
            bool sel = gR.curR == r && gR.curC == c && gR.phase == 0;
            bool bet = gR.phase >= 1 && gR.betR == r && gR.betC == c;
            if (sel || bet) RectLines(x - 2, y - 2, cw, ch, bet ? UI_GOLD : RAYWHITE);
            if (r == 0) { int si = SpeciesIndex(kMon[c - 1]); if (si >= 0) DrawSpeciesFit(si, VIEW_FRONT, x + cw / 2 - 2, y + ch - 10, 30); GDrawText(kMon[c - 1], x + 2, y + ch - 14, 10, UI_TEXT); }
            else if (c == 0) GDrawText(kColName[r - 1], x + 4, y + 14, 10, UI_TEXT);
            else if (gR.hit[r - 1][c - 1]) DrawCircleV({(float)x + cw / 2 - 2, (float)y + ch / 2 - 2}, 6, RAYWHITE);
            int m = Multiplier(r, c);
            if (r > 0 && c > 0 && !gR.hit[r - 1][c - 1]) GDrawText(TextFormat("x%d", m), x + cw - 24, y + ch - 16, 10, UI_TEXT);
            if ((r == 0 || c == 0) && m > 0) GDrawText(TextFormat("x%d", m), x + cw - 24, y + 2, 10, UI_GOLD);
        }
    DrawPanel(300, 252, 320, 70);
    GDrawText(TextFormat("Coins: %d     Bet: %d a ball     Balls left: %d", g.profile.casino, gR.minBet, gR.ballsLeft), 310, 260, 10, UI_GOLD);
    DrawWrapped(gR.msg.c_str(), 310, 276, 300, 10, UI_TEXT);
    GDrawText(gR.phase == 0 ? "Arrows: pick a bet   Enter: place it   Esc: leave" : gR.phase == 2 ? "Enter: next ball" : "", 10, VH - 14, 10, UI_DIM);
}
