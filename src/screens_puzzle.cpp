// The Ruins of Alph's stone panels (Pokemon Crystal / HeartGold): a picture of Kabuto, Omanyte, Aerodactyl or Ho-Oh
// cut into sixteen stones and jumbled up. Pick a stone up, put it down on another to swap them; the picture
// whole again, the floor gives way into the Inner Chamber (AlphSolved, overworld.cpp).
// The pictures are Crystal's (World/puzzles/alph_<kind>.png, made by tools/hgss_alph.py).
#include <algorithm>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "embedded.h"
#include "game.h"
#include "input.h"
#include "ui.h"

namespace {
constexpr int kPiece = 12;          // a stone in the picture (it's 48 x 48)
constexpr int kScale = 5;           // ...on the screen
constexpr int kCell = kPiece * kScale + 4;
int BoardX() { return VW / 2 - kCell * 2; }
int BoardY() { return 56; }
bool Solved(const Game::AlphPuzzle& p) {
    for (int k = 0; k < 16; k++) if (p.board[k] != k) return false;
    return true;
}
void Close(Game& g) {
    auto& p = g.alph;
    if (p.tex.id) UnloadTexture(p.tex);
    p.tex = Texture2D{};
    g.screen = Screen::World;
}
}  // namespace

void OpenUnownPuzzle(Game& g, const std::string& kind) {
    auto& p = g.alph;
    if (p.tex.id) UnloadTexture(p.tex);
    p = Game::AlphPuzzle();
    p.kind = kind;
    p.tex = LoadTexture((DataDir() + "World/puzzles/alph_" + kind + ".png").c_str());
    for (int k = 0; k < 16; k++) p.board[k] = k;
    do {   // jumbled (never already whole)
        for (int k = 15; k > 0; k--) std::swap(p.board[k], p.board[GetRandomValue(0, k)]);
    } while (Solved(p));
    p.cur = 5;
    g.screen = Screen::UnownPuzzle;
    PlaySfx(Sfx::MenuOk);
}

void UpdateUnownPuzzle(Game& g) {
    auto& p = g.alph;
    PlayerInput in = MenuInput();
    p.t += GetFrameTime();
    if (p.solved) {
        if (p.t > 1.2f && (in.confirm || in.back)) {
            std::string kind = p.kind;
            Close(g);
            AlphSolved(g, kind);
        }
        return;
    }
    if (in.back) {
        if (p.held >= 0) { p.held = -1; PlaySfx(Sfx::MenuBack); return; }
        Close(g);
        PlaySfx(Sfx::MenuBack);
        return;
    }
    int cx = p.cur % 4, cy = p.cur / 4;
    if (in.left) cx = (cx + 3) % 4;
    if (in.right) cx = (cx + 1) % 4;
    if (in.up) cy = (cy + 3) % 4;
    if (in.down) cy = (cy + 1) % 4;
    if (cy * 4 + cx != p.cur) { p.cur = cy * 4 + cx; PlaySfx(Sfx::MenuMove); }
    if (!in.confirm) return;
    if (p.held < 0) { p.held = p.cur; PlaySfx(Sfx::MenuOk); return; }
    std::swap(p.board[p.held], p.board[p.cur]);
    p.held = -1;
    PlaySfx(Sfx::Thud);
    if (Solved(p)) { p.solved = true; p.t = 0; PlaySfx(Sfx::ItemGet); }
}

void DrawUnownPuzzle(const Game& g) {
    const auto& p = g.alph;
    ClearBackground(Color{40, 26, 14, 255});
    for (int y = 0; y < VH; y += 16)   // the chamber's sandstone wall
        for (int x = (y / 16) % 2 * 16; x < VW; x += 32) DrawRectangle(x, y, 30, 14, Color{58, 38, 20, 255});
    DrawTextCentered("Ruins of Alph", VW / 2, 14, 10, UI_GOLD);
    DrawTextCentered(p.solved ? "The picture is complete!" : "Put the stones back in order to complete the picture.", VW / 2, 32, 10, UI_TEXT);
    int bx = BoardX(), by = BoardY();
    DrawRectangle(bx - 8, by - 8, kCell * 4 + 12, kCell * 4 + 12, Color{24, 14, 6, 255});
    for (int k = 0; k < 16; k++) {
        int x = bx + (k % 4) * kCell, y = by + (k / 4) * kCell;
        int piece = p.board[k];
        Rectangle src{(float)(piece % 4 * kPiece), (float)(piece / 4 * kPiece), (float)kPiece, (float)kPiece};
        Rectangle dst{(float)x, (float)y, (float)(kPiece * kScale), (float)(kPiece * kScale)};
        bool lifted = k == p.held;
        if (lifted) { dst.x -= 3; dst.y -= 5; }
        if (p.tex.id) DrawTexturePro(p.tex, src, dst, {0, 0}, 0, WHITE);
        else DrawRectangleRec(dst, Color{216, 160, 72, 255});
        if (lifted) DrawRectangleLinesEx(dst, 3, Color{255, 230, 120, 255});
    }
    if (!p.solved) {
        float pulse = 0.6f + 0.4f * std::sin(g.t * 6);
        int x = bx + (p.cur % 4) * kCell - 3, y = by + (p.cur / 4) * kCell - 3;
        DrawRectangleLinesEx({(float)x, (float)y, (float)(kPiece * kScale + 6), (float)(kPiece * kScale + 6)}, 3, Fade(Color{120, 220, 255, 255}, pulse));
    } else {
        float a = std::min(1.0f, p.t * 2);
        DrawRectangleLinesEx({(float)bx - 8, (float)by - 8, (float)(kCell * 4 + 12), (float)(kCell * 4 + 12)}, 4, Fade(Color{255, 230, 120, 255}, a));
    }
    DrawTextCentered(p.solved ? "Enter  continue" : p.held >= 0 ? "Enter  put it here (swap)    Esc  put it back"
                                                                 : "Arrows  move    Enter  pick up a stone    Esc  leave",
                     VW / 2, VH - 22, 10, UI_DIM, false);
}
