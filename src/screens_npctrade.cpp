// An in-game trade (FireRed, HeartGold, Emerald's traders: World/trades.txt, overworld.cpp's "trade" action): your
// Pokemon goes back into its ball and off to them, theirs comes over to you and pops out. The swap itself is already
// in your save by the time this plays; this is the show.
#include <algorithm>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "input.h"
#include "sprites.h"
#include "ui.h"

namespace {
struct NpcTradeShow {
    int give = -1, get = -1;          // species shown
    bool giveShiny = false;
    std::string giveName, getName, who;
    float t = 0;
    bool sfx[4] = {};
} gT;
constexpr float T_IN = 0.9f, T_FLY = 2.6f, T_OUT = 3.4f, T_END = 3.9f;   // your Pokemon goes in / balls cross / theirs comes out

void Ball(float x, float y, float r, float spin, float open = 0) {
    DrawCircleV({x, y + r * 0.15f}, r + 1, Fade(BLACK, 0.25f));
    DrawCircleSector({x, y - open * r * 0.5f}, r, 180 + spin, 360 + spin, 24, Color{230, 70, 60, 255});
    DrawCircleSector({x, y + open * r * 0.5f}, r, 0 + spin, 180 + spin, 24, RAYWHITE);
    DrawCircleLinesV({x, y}, r, Color{40, 40, 50, 255});
    DrawCircleV({x, y}, r * 0.32f, Color{40, 40, 50, 255});
    DrawCircleV({x, y}, r * 0.2f, RAYWHITE);
}
}  // namespace

void OpenNpcTrade(Game& g, int giveSpecies, bool giveShiny, const std::string& giveName, int getSpecies, const std::string& getName, const std::string& who) {
    gT = NpcTradeShow{};
    gT.give = giveSpecies; gT.giveShiny = giveShiny; gT.giveName = giveName;
    gT.get = getSpecies; gT.getName = getName; gT.who = who;
    g.screen = Screen::NpcTrade;
    PlaySfx(Sfx::MenuOk);
}

void UpdateNpcTrade(Game& g, float dt) {
    gT.t += dt;
    if (gT.t > 0.3f && !gT.sfx[0]) { gT.sfx[0] = true; PlaySfx(Sfx::Status); }      // into the ball
    if (gT.t > T_IN && !gT.sfx[1]) { gT.sfx[1] = true; PlaySfx(Sfx::MenuMove); }    // off it goes
    if (gT.t > T_FLY && !gT.sfx[2]) { gT.sfx[2] = true; PlaySfx(Sfx::Heal); }        // theirs pops out
    if (gT.t > T_END && !gT.sfx[3]) { gT.sfx[3] = true; PlaySfx(Sfx::LevelUp); }
    PlayerInput in = MenuInput();
#ifdef ARENA_AUTOTEST
    if (gT.t > T_END + 0.6f) in.confirm = true;
#endif
    if (gT.t > T_END && (in.confirm || in.back)) { g.screen = Screen::World; PlaySfx(Sfx::MenuOk); }
}

void DrawNpcTrade(const Game& g) {
    (void)g;
    float t = gT.t;
    // the link: two sides, a cable between them
    ClearBackground(Color{40, 60, 110, 255});
    for (int y = 0; y < VH; y += 4) DrawRectangle(0, y, VW, 2, Fade(Color{70, 100, 170, 255}, 0.35f));
    const float LX = VW * 0.22f, RX = VW * 0.78f, Y = VH * 0.52f;
    DrawRectangle((int)LX - 70, (int)Y + 26, 140, 10, Color{30, 40, 70, 255});
    DrawRectangle((int)RX - 70, (int)Y + 26, 140, 10, Color{30, 40, 70, 255});
    for (int k = 0; k < 40; k++) {   // the cable, a pulse running along it while the balls fly
        float u = k / 39.0f;
        float x = LX + (RX - LX) * u, y = Y + 31 + std::sin(u * PI) * 18;
        float pulse = (t > T_IN && t < T_FLY) ? std::max(0.0f, 1 - std::fabs(u - std::fmod((t - T_IN) * 1.6f, 1.0f)) * 6) : 0;
        DrawCircleV({x, y}, 2.2f + pulse * 2, ColorLerp(Color{90, 110, 160, 255}, Color{255, 230, 120, 255}, pulse));
    }
    GDrawText("You", (int)LX - TextW("You", 10) / 2, (int)Y + 44, 10, UI_GOLD);
    GDrawText(gT.who.c_str(), (int)RX - TextW(gT.who.c_str(), 10) / 2, (int)Y + 44, 10, UI_TEXT);

    // your Pokemon: shrinks into its ball (a flash of red light)
    if (t < T_IN && gT.give >= 0) {
        float s = t < 0.3f ? 1.0f : std::max(0.0f, 1 - (t - 0.3f) / (T_IN - 0.3f));
        Color tint = ColorLerp(WHITE, Color{255, 90, 80, 255}, 1 - s);
        if (s > 0.02f) DrawSpeciesFit(ShinyIdx(gT.give, gT.giveShiny), VIEW_FRONT, (int)LX, (int)(Y + 18), (int)(96 * s), tint);
        if (t > 0.3f) Ball(LX, Y + 10, 9, 0);
    }
    // the balls cross: yours over the top to them, theirs underneath to you
    if (t >= T_IN && t < T_FLY) {
        float u = (t - T_IN) / (T_FLY - T_IN), e = u * u * (3 - 2 * u);
        for (int side = 0; side < 2; side++) {
            float sx = side == 0 ? LX : RX, ex = side == 0 ? RX : LX;
            float x = sx + (ex - sx) * e, y = Y + 10 - std::sin(e * PI) * (side == 0 ? 120.0f : 60.0f);
            for (int k = 1; k < 7; k++) {   // a trail
                float uu = std::max(0.0f, e - k * 0.025f);
                DrawCircleV({sx + (ex - sx) * uu, Y + 10 - std::sin(uu * PI) * (side == 0 ? 120.0f : 60.0f)}, 4 - k * 0.5f, Fade(side == 0 ? UI_GOLD : Color{140, 220, 255, 255}, 0.6f - k * 0.08f));
            }
            Ball(x, y, 9, e * 720 * (side == 0 ? 1 : -1));
        }
        if (u > 0.4f && u < 0.6f) DrawCircleV({(LX + RX) / 2, Y - 20}, 18 * (1 - std::fabs(u - 0.5f) * 10), Fade(WHITE, 0.6f));   // they pass each other
    }
    // theirs pops out on your side
    if (t >= T_FLY && gT.get >= 0) {
        float u = std::min(1.0f, (t - T_FLY) / (T_OUT - T_FLY));
        if (u < 1) Ball(LX, Y + 10, 9, 0, u);
        float s = u < 0.35f ? 0 : std::min(1.0f, (u - 0.35f) / 0.65f);
        if (s > 0.02f) DrawSpeciesFit(gT.get, VIEW_FRONT, (int)LX, (int)(Y + 18), (int)(96 * (s < 1 ? s * 1.1f : 1.0f)), ColorLerp(Color{140, 220, 255, 255}, WHITE, s));
        for (int k = 0; k < 8 && u < 1; k++) {   // sparkles
            float a = k * PI / 4 + t * 3, r = 10 + u * 34;
            DrawCircleV({LX + std::cos(a) * r, Y - 10 + std::sin(a) * r}, 2, Fade(WHITE, 1 - u));
        }
    }
    // their side: an empty spot, then yours arriving there
    if (t >= T_FLY && gT.give >= 0) {
        float u = std::min(1.0f, (t - T_FLY) / (T_OUT - T_FLY));
        float s = u < 0.35f ? 0 : std::min(1.0f, (u - 0.35f) / 0.65f);
        if (s > 0.02f) DrawSpeciesFit(ShinyIdx(gT.give, gT.giveShiny), VIEW_FRONT, (int)RX, (int)(Y + 18), (int)(80 * s), Fade(WHITE, 0.85f));
    }
    if (t < T_IN) { DrawSpeciesFit(gT.get, VIEW_FRONT, (int)RX, (int)(Y + 18), 80, WHITE); }

    DrawTextCentered("Trading Pokemon", VW / 2, 14, 10, UI_GOLD);
    std::string line = t < T_IN ? "Off you go, " + gT.giveName + "!" : t < T_FLY ? "Trading..." : gT.getName + " came over to you!";
    DrawTextCentered(line.c_str(), VW / 2, VH - 46, 10, UI_TEXT, false);
    if (t > T_END) {
        std::string done = "You traded " + gT.giveName + " for " + gT.getName + "!";
        DrawTextCentered(done.c_str(), VW / 2, VH - 30, 10, UI_GOLD, false);
        DrawTextCentered("Enter: continue", VW / 2, VH - 14, 10, UI_DIM, false);
    }
}
