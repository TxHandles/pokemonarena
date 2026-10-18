// Celadon Game Corner: the slot machines and the prize counter (a tab of the shop).
#include <algorithm>
#include <cmath>
#include <cctype>
#include "arena.h"
#include "audio.h"
#include "story.h"
#include "game.h"
#include "items.h"
#include "sprites.h"
#include "ui.h"

namespace {

constexpr int CASINO_MAX = 99999;
constexpr int REEL = 21;
// Symbols: 0 "7", 1 Rocket "R", 2 Pikachu, 3 Psyduck, 4 Magnemite, 5 Shellder, 6 Cherry
const int kStrip[3][REEL] = {
    {6, 4, 5, 2, 0, 5, 3, 4, 6, 5, 1, 3, 4, 5, 2, 6, 4, 3, 5, 1, 2},
    {5, 2, 4, 6, 3, 5, 0, 4, 2, 5, 6, 1, 3, 4, 5, 3, 2, 6, 4, 1, 5},
    {3, 5, 6, 4, 1, 2, 5, 3, 4, 0, 6, 5, 2, 4, 3, 5, 6, 1, 4, 2, 5},
};
const int kPay[6] = {300, 100, 15, 15, 6, 4};   // three in a line; a cherry on the left reel pays 2
const char* kSymMon[6] = {nullptr, nullptr, "Pikachu", "Psyduck", "Magnemite", "Shellder"};
const int kLine[5][3] = {{0, 0, 0}, {-1, -1, -1}, {1, 1, 1}, {-1, 0, 1}, {1, 0, -1}};
int LinesFor(int bet) { return bet >= 3 ? 5 : bet == 2 ? 3 : 1; }
constexpr float SPIN_SPEED = 14.0f;   // symbols per second

// The prize counter: the FireRed / LeafGreen prizes (both versions' Pokemon), plus a few rarer extras.
struct Prize { const char* name; int price; bool mon; const char* tmMove; };
const Prize kPrizes[] = {
    {"Abra", 180, true, nullptr},          {"Clefairy", 500, true, nullptr},       {"Pinsir", 2500, true, nullptr},
    {"Dratini", 2800, true, nullptr},      {"Scyther", 5500, true, nullptr},       {"Porygon", 9999, true, nullptr},
    {"Smoke Ball", 800, false, nullptr},   {"Miracle Seed", 1000, false, nullptr}, {"Charcoal", 1000, false, nullptr},
    {"Mystic Water", 1000, false, nullptr}, {"Yellow Flute", 1600, false, nullptr},
    {"TM Ice Beam", 4000, false, "Ice Beam"}, {"TM Thunderbolt", 4000, false, "Thunderbolt"},
    {"TM Flamethrower", 4000, false, "Flamethrower"}, {"TM Shadow Ball", 4500, false, "Shadow Ball"},
    {"Leftovers", 3000, false, nullptr},   {"Focus Band", 4000, false, nullptr},   {"Choice Scarf", 5000, false, nullptr},
    {"Life Orb", 6000, false, nullptr},    {"Master Ball", 20000, false, nullptr},
};
constexpr int NPRIZES = (int)(sizeof kPrizes / sizeof kPrizes[0]);
constexpr int TOP_ROWS = 3;   // play, buy 50, buy 500
const int kBuyCoins[2] = {50, 500}, kBuyCost[2] = {25, 250};

const ItemDef* PrizeItem(const Prize& p) {
    if (p.mon) return nullptr;
    if (!p.tmMove) return FindItem(p.name);
    for (const auto& d : ItemList())
        if (d.fx == ItemFx::TM && d.move == p.tmMove) return &d;
    return nullptr;
}

int CornerStage() {
    for (int i = 0; i < (int)Story().stages.size(); i++) {
        std::string n = Story().stages[i].name;
        for (char& c : n) c = (char)std::tolower((unsigned char)c);
        if (n.find("celadon") != std::string::npos) return i;
    }
    return 0;
}

int ReelAt(int r, float pos, int row) {   // the symbol 'row' rows below the top of a reel at 'pos'
    int i = ((int)std::floor(pos) + row) % REEL;
    return kStrip[r][(i + REEL) % REEL];
}

void Payout(Game& g) {
    Profile& p = g.profile;
    g.slotLines.clear();
    int win = 0;
    for (int l = 0; l < LinesFor(g.slotBet); l++) {
        int sy[3];
        for (int r = 0; r < 3; r++) sy[r] = ReelAt(r, (float)g.slotStop[r], 1 + kLine[l][r]);
        int w = 0;
        if (sy[0] == sy[1] && sy[1] == sy[2] && sy[0] < 6) w = kPay[sy[0]];
        else if (sy[0] == 6) w = 2;
        if (w) { win += w; g.slotLines.push_back(l); }
    }
    g.slotWin = win;
    g.slotT = 0;
    p.casino = std::min(CASINO_MAX, p.casino + win);
    if (win >= 100) PlaySfx(Sfx::Win);
    else if (win > 0) PlaySfx(Sfx::Heal);
    else PlaySfx(Sfx::MenuBack);
    StorySaveProfile(g);
}

// Reel symbols, drawn as little pictures (s = scale; 1 fills a 30 px reel cell).
void DrawSymbol(int sym, int cx, int cy, float alpha, float s = 1.0f) {
    auto C = [&](Color c) { return Fade(c, alpha); };
    float x = (float)cx, y = (float)cy;
    switch (sym) {
        case 0:   // a red 7
            DrawTextCentered("7", cx, cy - (int)(12 * s), s < 0.7f ? 20 : 30, C(Color{230, 40, 40, 255}));
            break;
        case 1:   // the Rocket R
            DrawRectangle(cx - (int)(13 * s), cy - (int)(13 * s), (int)(26 * s), (int)(26 * s), C(UI_INK));
            DrawTextCentered("R", cx, cy - (int)(10 * s), s < 0.7f ? 10 : 20, C(Color{230, 40, 40, 255}));
            break;
        case 2: {   // Pikachu: a lightning bolt
            Color yl = C(Color{250, 210, 40, 255}), dk = C(Color{150, 100, 20, 255});
            Vector2 p[6] = {{x + 4 * s, y - 14 * s}, {x - 8 * s, y + 2 * s}, {x - 1 * s, y + 2 * s}, {x - 5 * s, y + 14 * s}, {x + 9 * s, y - 3 * s}, {x + 2 * s, y - 3 * s}};
            DrawTriangle(p[0], p[1], p[2], yl);
            DrawTriangle(p[0], p[2], p[5], yl);
            DrawTriangle(p[5], p[2], p[4], yl);
            DrawTriangle(p[2], p[3], p[4], yl);
            for (int k = 0; k < 6; k++) DrawLineV(p[k], p[(k + 1) % 6], dk);
        } break;
        case 3: {   // Psyduck: a yellow duck head with a beak
            DrawCircleV({x, y}, 11 * s, C(Color{120, 90, 30, 255}));
            DrawCircleV({x, y}, 10 * s, C(Color{250, 210, 90, 255}));
            DrawEllipse(cx, cy + (int)(5 * s), (int)(8 * s), (int)(4 * s), C(Color{250, 230, 190, 255}));
            DrawCircleV({x - 4 * s, y - 3 * s}, 2 * s, C(WHITE));
            DrawCircleV({x + 4 * s, y - 3 * s}, 2 * s, C(WHITE));
            DrawCircleV({x - 4 * s, y - 3 * s}, 0.8f * s, C(UI_INK));
            DrawCircleV({x + 4 * s, y - 3 * s}, 0.8f * s, C(UI_INK));
            DrawLineEx({x, y - 10 * s}, {x - 2 * s, y - 15 * s}, 1.2f, C(UI_INK));
            DrawLineEx({x, y - 10 * s}, {x + 2 * s, y - 15 * s}, 1.2f, C(UI_INK));
        } break;
        case 4: {   // Magnemite: a steel ball with an eye and two magnets
            for (int sd = -1; sd <= 1; sd += 2) {
                DrawRectangle(cx + sd * (int)(11 * s) - (int)(3 * s), cy - (int)(4 * s), (int)(6 * s), (int)(8 * s), C(Color{150, 150, 160, 255}));
                DrawRectangle(cx + sd * (int)(11 * s) - (int)(3 * s), cy - (int)(4 * s), (int)(6 * s), (int)(2 * s), C(Color{220, 60, 60, 255}));
                DrawRectangle(cx + sd * (int)(11 * s) - (int)(3 * s), cy + (int)(2 * s), (int)(6 * s), (int)(2 * s), C(Color{60, 90, 220, 255}));
            }
            DrawCircleV({x, y}, 8 * s, C(Color{90, 96, 110, 255}));
            DrawCircleV({x, y}, 7 * s, C(Color{180, 186, 200, 255}));
            DrawCircleV({x, y}, 3.5f * s, C(WHITE));
            DrawCircleV({x, y}, 1.6f * s, C(UI_INK));
            DrawLineEx({x, y - 8 * s}, {x, y - 13 * s}, 1.5f, C(Color{90, 96, 110, 255}));
        } break;
        case 5: {   // Shellder: a purple clam with its tongue out
            DrawCircleSector({x, y + 2 * s}, 12 * s, 180, 360, 12, C(Color{80, 50, 120, 255}));
            DrawCircleSector({x, y + 2 * s}, 11 * s, 180, 360, 12, C(Color{150, 110, 200, 255}));
            for (int k = -2; k <= 2; k++) DrawLineEx({x + k * 4 * s, y + 2 * s}, {x + k * 5 * s, y - 8 * s}, 1, C(Color{110, 80, 160, 255}));
            DrawRectangle(cx - (int)(10 * s), cy + (int)(2 * s), (int)(20 * s), (int)(5 * s), C(Color{60, 40, 80, 255}));
            DrawCircleV({x - 3 * s, y + 4 * s}, 1.6f * s, C(WHITE));
            DrawCircleV({x + 3 * s, y + 4 * s}, 1.6f * s, C(WHITE));
            DrawEllipse(cx + (int)(5 * s), cy + (int)(9 * s), (int)(5 * s), (int)(3 * s), C(Color{240, 120, 150, 255}));
        } break;
        default: {   // cherries
            DrawLineEx({x - 6 * s, y + 4 * s}, {x + 2 * s, y - 12 * s}, 2, C(Color{60, 140, 50, 255}));
            DrawLineEx({x + 7 * s, y + 2 * s}, {x + 2 * s, y - 12 * s}, 2, C(Color{60, 140, 50, 255}));
            DrawCircleV({x - 6 * s, y + 6 * s}, 7 * s, C(Color{210, 30, 50, 255}));
            DrawCircleV({x + 7 * s, y + 4 * s}, 7 * s, C(Color{230, 40, 60, 255}));
            DrawCircleV({x - 8 * s, y + 3 * s}, 2 * s, C(Fade(WHITE, 0.7f)));
        } break;
    }
}

} // namespace

bool GameCornerOpen(const Game& g) { return (g.coop ? g.coopSave.storyStage : g.profile.storyStage) >= CornerStage(); }

// ------------------------------------------------------------------ the prize counter (shop tab)

void UpdateGameCorner(Game& g, const PlayerInput& in) {
    if (!GameCornerOpen(g)) return;
    int n = TOP_ROWS + NPRIZES;
    if (in.up) { g.shopSel = (g.shopSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.shopSel = (g.shopSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    g.shopSel = std::max(0, std::min(g.shopSel, n - 1));
    if (!in.confirm) return;
    Profile& p = g.profile;
    if (g.shopSel == 0) {
        g.slotPhase = 0;
        g.slotWin = 0;
        g.slotLines.clear();
        g.slotBet = std::max(1, std::min(3, g.slotBet));
        g.screen = Screen::Slots;
        PlaySfx(Sfx::MenuOk);
        return;
    }
    if (g.shopSel < TOP_ROWS) {
        int k = g.shopSel - 1;
        if (p.coins < kBuyCost[k]) { Toast(g, "Not enough coins!"); PlaySfx(Sfx::MenuBack); return; }
        if (p.casino + kBuyCoins[k] > CASINO_MAX) { Toast(g, "Your Coin Case is full!"); PlaySfx(Sfx::MenuBack); return; }
        p.coins -= kBuyCost[k];
        p.casino += kBuyCoins[k];
        StorySaveProfile(g);
        PlaySfx(Sfx::Heal);
        Toast(g, TextFormat("Got %d Game Corner coins!", kBuyCoins[k]));
        return;
    }
    const Prize& pr = kPrizes[g.shopSel - TOP_ROWS];
    if (p.casino < pr.price) { Toast(g, "Not enough Game Corner coins!"); PlaySfx(Sfx::MenuBack); return; }
    if (pr.mon) {
        int sp = SpeciesIndex(pr.name);
        if (sp < 0) { Toast(g, std::string(pr.name) + " isn't in this game's Pokemon list."); return; }
        p.casino -= pr.price;
        bool boxed = AddMon(p, NewMon(sp, StoryNewMonLevel(p)));
        StorySaveProfile(g);
        PlaySfx(Sfx::LevelUp);
        Toast(g, std::string(pr.name) + (boxed ? " was sent to the Box!" : " joined your team!"));
        return;
    }
    const ItemDef* d = PrizeItem(pr);
    if (!d) { Toast(g, "That prize is out of stock."); return; }
    p.casino -= pr.price;
    BagAdd(p, d->name);
    StorySaveProfile(g);
    PlaySfx(Sfx::Heal);
    Toast(g, "Got " + d->name + "!");
}

void DrawGameCorner(const Game& g) {
    const Profile& p = g.profile;
    if (!GameCornerOpen(g)) {
        DrawPanel(120, 120, 400, 80);
        DrawTextCentered("CELADON GAME CORNER", VW / 2, 134, 20, Color{255, 120, 200, 255});
        DrawTextCentered("Reach Celadon City to visit the Game Corner.", VW / 2, 170, 10, UI_DIM, false);
        return;
    }
    int n = TOP_ROWS + NPRIZES, rows = 13;
    int first = std::max(0, std::min(g.shopSel - rows / 2, n - rows));
    for (int i = first; i < std::min(n, first + rows); i++) {
        int y = 56 + (i - first) * 21;
        bool sel = g.shopSel == i;
        DrawPanel(20, y, 330, 19, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : (i < TOP_ROWS ? Color{150, 80, 130, 255} : Color{70, 64, 92, 255}));
        Color tc = sel ? UI_GOLD : UI_TEXT;
        if (i == 0) {
            DrawSymbol(0, 32, y + 10, 1, 0.5f);
            DrawText("Play the slot machines", 46, y + 5, 10, sel ? UI_GOLD : Color{255, 150, 220, 255});
        } else if (i < TOP_ROWS) {
            DrawCircle(32, y + 9, 5, Color{200, 200, 220, 255});
            DrawText(TextFormat("Buy %d Game Corner coins", kBuyCoins[i - 1]), 46, y + 5, 10, tc);
            const char* pr = TextFormat("%d coins", kBuyCost[i - 1]);
            DrawText(pr, 340 - TextW(pr, 10), y + 5, 10, kBuyCost[i - 1] <= p.coins ? Color{255, 200, 60, 255} : Color{255, 120, 100, 255});
        } else {
            const Prize& pz = kPrizes[i - TOP_ROWS];
            if (pz.mon) {
                int sp = SpeciesIndex(pz.name);
                if (sp >= 0) DrawSpeciesFit(sp, VIEW_FRONT, 32, y + 18, 16);
            } else {
                const ItemDef* d = PrizeItem(pz);
                if (d && d->fx == ItemFx::TM) {
                    DrawCircle(32, y + 9, 6, ColorBrightness(TypeColor(d->type), -0.35f));
                    DrawCircle(32, y + 9, 4.5f, TypeColor(d->type));
                } else if (d && d->fx == ItemFx::Ball) {
                    DrawCircle(32, y + 9, 5, UI_INK);
                    DrawCircleSector({32, (float)y + 9}, 4, 180, 360, 12, Color{150, 80, 200, 255});
                    DrawCircleSector({32, (float)y + 9}, 4, 0, 180, 12, WHITE);
                } else {
                    DrawRectangle(28, y + 5, 9, 9, d && d->fx == ItemFx::TypeBoost ? TypeColor(d->type) : Color{140, 220, 255, 255});
                }
                int have = d ? BagCount(p, d->name) : 0;
                if (have > 0) DrawText(TextFormat("x%d", have), 230, y + 5, 10, UI_DIM);
            }
            DrawText(pz.name, 46, y + 5, 10, tc);
            const char* pr = TextFormat("%d", pz.price);
            DrawText(pr, 340 - TextW(pr, 10), y + 5, 10, pz.price <= p.casino ? Color{255, 150, 220, 255} : Color{255, 120, 100, 255});
        }
    }
    // the details panel
    DrawPanel(362, 56, 258, 150);
    DrawText("CELADON GAME CORNER", 372, 64, 10, Color{255, 120, 200, 255});
    DrawText(TextFormat("Game Corner coins: %d", p.casino), 372, 80, 10, Color{255, 150, 220, 255});
    std::string desc;
    if (g.shopSel == 0) desc = "Three reels: line up 7s (300), Rocket R's (100), Pikachu or Psyduck (15), Magnemite (6) or Shellder (4). "
                               "A cherry on the left reel pays 2. Bet 3 coins to play all 5 lines!";
    else if (g.shopSel < TOP_ROWS) desc = "Trade your coins for Game Corner coins to play the slots or spend at the prize counter.";
    else {
        const Prize& pz = kPrizes[g.shopSel - TOP_ROWS];
        if (pz.mon) desc = std::string("A prize Pokemon. ") + pz.name + " joins at about your team's level." +
                           (std::string(pz.name) == "Porygon" ? " You can't find it anywhere else!" : "");
        else if (const ItemDef* d = PrizeItem(pz)) desc = d->fx == ItemFx::TM ? "Teaches " + d->move + ". Used up when taught." : d->desc;
    }
    DrawWrapped(desc.c_str(), 372, 98, 238, 10, UI_TEXT, 12);
    DrawText("Prizes cost Game Corner coins.", 372, 190, 10, UI_DIM);
    DrawText("Left/Right tab   Up/Down choose   Enter play / buy   Esc back", 20, 340, 10, UI_DIM);
}

// ------------------------------------------------------------------ the slot machine

void UpdateSlots(Game& g) {
    PlayerInput in = MenuInput();
    Profile& p = g.profile;
    float dt = GetFrameTime();
    g.slotT += dt;
    // spin the reels that are still going; stopping reels ease onto their stop
    for (int r = 0; r < 3; r++) {
        bool spinning = g.slotPhase >= 1 && g.slotPhase <= 3 && r >= g.slotPhase - 1;
        if (g.slotStopping[r]) {
            float target = (float)g.slotStop[r];
            while (target < g.slotPos[r]) target += REEL;
            g.slotPos[r] = std::min(target, g.slotPos[r] + SPIN_SPEED * 0.6f * dt);
            if (g.slotPos[r] >= target - 0.001f) {
                g.slotPos[r] = (float)(g.slotStop[r] % REEL);
                g.slotStopping[r] = false;
                PlaySfx(Sfx::MenuMove);
            }
        } else if (spinning) {
            g.slotPos[r] = std::fmod(g.slotPos[r] + SPIN_SPEED * dt, (float)REEL);
        }
    }
    bool settling = g.slotStopping[0] || g.slotStopping[1] || g.slotStopping[2];
    if (g.slotPhase == 4 && !settling && g.slotWin < 0) Payout(g);
    if (g.slotPhase == 0 || (g.slotPhase == 4 && !settling)) {
        if (in.back) { PlaySfx(Sfx::MenuBack); g.screen = Screen::Shop; g.shopTab = 5; g.shopSel = 0; return; }
        if (in.up || in.right) { g.slotBet = std::min(3, g.slotBet + 1); PlaySfx(Sfx::MenuMove); }
        if (in.down || in.left) { g.slotBet = std::max(1, g.slotBet - 1); PlaySfx(Sfx::MenuMove); }
        if (in.confirm) {
            if (p.casino < g.slotBet) {
                Toast(g, p.casino > 0 ? "Not enough coins for that bet - try a smaller one." : "Out of Game Corner coins! Buy more at the counter.");
                PlaySfx(Sfx::MenuBack);
                return;
            }
            p.casino -= g.slotBet;
            g.slotPhase = 1;
            g.slotWin = 0;
            g.slotLines.clear();
            PlaySfx(Sfx::MenuOk);
        }
        return;
    }
    if (settling || g.slotPhase > 3) return;
    if (in.confirm) {   // stop the next reel; it slips up to two symbols
        int r = g.slotPhase - 1;
        g.slotStop[r] = ((int)std::ceil(g.slotPos[r]) + GetRandomValue(0, 2)) % REEL;
        g.slotStopping[r] = true;
        g.slotPhase++;
        if (g.slotPhase > 3) { g.slotPhase = 4; g.slotWin = -1; }   // pay once the last reel settles
    }
}

void DrawSlots(const Game& g) {
    ClearBackground(Color{40, 14, 34, 255});
    // the parlour: rows of blinking machines in the back
    for (int k = 0; k < 10; k++) {
        int x = 8 + k * 64;
        DrawRectangle(x, 40, 56, 70, Color{70, 26, 58, 255});
        DrawRectangle(x + 6, 50, 44, 22, Color{20, 10, 20, 255});
        bool on = ((int)(g.t * 3) + k) % 4 != 0;
        DrawCircle(x + 28, 44, 3, on ? Color{255, 220, 80, 255} : Color{120, 90, 40, 255});
    }
    for (int x = 0; x < VW; x += 16) DrawCircle(x + 8, 30, 3, ((x / 16 + (int)(g.t * 6)) % 3) ? Color{255, 120, 200, 255} : Color{120, 40, 90, 255});
    DrawTextCentered("CELADON GAME CORNER", VW / 2, 6, 20, Color{255, 150, 220, 255});
    const Profile& p = g.profile;
    // the machine
    int mx = VW / 2 - 150, my = 100, mw = 300, mh = 200;
    DrawRectangleRounded({(float)mx - 6, (float)my - 6, (float)mw + 12, (float)mh + 12}, 0.08f, 8, Color{200, 160, 40, 255});
    DrawRectangleRounded({(float)mx, (float)my, (float)mw, (float)mh}, 0.08f, 8, Color{150, 30, 60, 255});
    int rx0 = mx + 30, ry = my + 22, rw = 72, rh = 120, gap = 12;
    for (int r = 0; r < 3; r++) {
        int x = rx0 + r * (rw + gap);
        DrawRectangle(x, ry, rw, rh, Color{250, 246, 236, 255});
        float pos = g.slotPos[r];
        float frac = pos - std::floor(pos);
        bool moving = (g.slotPhase >= 1 && g.slotPhase <= 3 && r >= g.slotPhase - 1) || g.slotStopping[r];
        for (int row = -1; row < 4; row++) {   // symbols near the window's edges shrink away (no clipping on the render target)
            int sym = ReelAt(r, pos, row);
            float cyf = ry + (row - frac) * 40 + 20;
            float edge = std::min(cyf - ry, ry + rh - cyf);   // distance to the nearer edge
            if (edge < 4) continue;
            float sc = std::min(1.0f, edge / 18.0f);
            DrawSymbol(sym, x + rw / 2, (int)cyf, moving ? 0.8f : 1.0f, sc);
            if (moving) DrawSymbol(sym, x + rw / 2, (int)cyf - 8, 0.25f, sc);   // motion blur
        }
        DrawRectangleGradientV(x, ry, rw, 14, Fade(BLACK, 0.35f), Fade(BLACK, 0));
        DrawRectangleGradientV(x, ry + rh - 14, rw, 14, Fade(BLACK, 0), Fade(BLACK, 0.35f));
        DrawRectangleLines(x, ry, rw, rh, Color{90, 20, 40, 255});
        if (g.slotPhase >= 1 && g.slotPhase <= 3 && r == g.slotPhase - 1)
            DrawTriangle({(float)x + rw / 2 - 6, (float)ry + rh + 4}, {(float)x + rw / 2 + 6, (float)ry + rh + 4}, {(float)x + rw / 2, (float)ry + rh - 4}, UI_GOLD);
    }
    // the bet lines
    int nl = LinesFor(g.slotBet);
    for (int l = 0; l < 5; l++) {
        bool active = l < nl, won = std::find(g.slotLines.begin(), g.slotLines.end(), l) != g.slotLines.end();
        if (!active) continue;
        Color lc = won && std::fmod(g.slotT, 0.4f) < 0.25f ? Color{255, 240, 80, 255} : Fade(Color{255, 120, 160, 255}, won ? 0.9f : 0.35f);
        Vector2 a{(float)rx0 - 8, (float)ry + 60 + kLine[l][0] * 40}, b{(float)rx0 + 3 * rw + 2 * gap + 8, (float)ry + 60 + kLine[l][2] * 40};
        DrawLineEx(a, b, won ? 3 : 1.5f, lc);
    }
    for (int l = 0; l < 3; l++) {
        int ly = ry + 60 + kLine[l][0] * 40;
        Color c = l < nl ? Color{255, 220, 80, 255} : Color{110, 60, 70, 255};
        DrawCircle(mx + 14, ly, 5, c);
        DrawCircle(mx + mw - 14, ly, 5, c);
    }
    // the coin display
    DrawRectangle(mx + 30, my + mh - 46, 240, 34, Color{20, 10, 20, 255});
    DrawText(TextFormat("COINS %05d", p.casino), mx + 40, my + mh - 40, 10, Color{255, 120, 100, 255});
    DrawText(TextFormat("BET %d", g.slotBet), mx + 40, my + mh - 26, 10, Color{255, 220, 80, 255});
    if (g.slotWin > 0) {
        Color wc = std::fmod(g.slotT, 0.3f) < 0.18f ? Color{255, 240, 80, 255} : Color{255, 150, 220, 255};
        const char* t = g.slotWin >= 300 ? TextFormat("JACKPOT! +%d", g.slotWin) : TextFormat("WIN +%d", g.slotWin);
        DrawText(t, mx + 270 - TextW(t, 10) - 10, my + mh - 33, 10, wc);
    }
    if (g.slotWin >= 100 && g.slotT < 3) {   // coins shower
        for (int k = 0; k < 40; k++) {
            float ph = std::fmod(g.slotT * 0.9f + k * 0.071f, 1.0f);
            DrawCircle((k * 53) % VW, (int)(ph * VH), 3, Fade(Color{255, 210, 60, 255}, 1 - ph * 0.5f));
        }
    }
    // the pay table
    DrawPanel(8, 112, 120, 158);
    DrawText("PAYS (per line)", 14, 118, 10, UI_DIM);
    const char* nm[7] = {"7 7 7", "R R R", "Pikachu", "Psyduck", "Magnemite", "Shellder", "Cherry"};
    for (int i = 0; i < 7; i++) {
        int y = 140 + i * 18;
        DrawSymbol(i, 22, y, 1, 0.55f);
        DrawText(nm[i], 36, y - 4, 10, UI_TEXT);
        const char* v = i < 6 ? TextFormat("%d", kPay[i]) : "2";
        DrawText(v, 120 - TextW(v, 10), y - 4, 10, Color{255, 220, 80, 255});
    }
    DrawText("cherry: left reel", 14, 258, 10, UI_DIM);
    DrawPanel(VW - 128, 120, 120, 110);
    DrawText("BET", VW - 120, 126, 10, UI_DIM);
    DrawText("1 coin: middle", VW - 120, 144, 10, g.slotBet == 1 ? UI_GOLD : UI_TEXT);
    DrawText("2 coins: 3 rows", VW - 120, 160, 10, g.slotBet == 2 ? UI_GOLD : UI_TEXT);
    DrawText("3 coins: 3 rows", VW - 120, 176, 10, g.slotBet == 3 ? UI_GOLD : UI_TEXT);
    DrawText("  + diagonals", VW - 120, 190, 10, g.slotBet == 3 ? UI_GOLD : UI_TEXT);
    bool idle = g.slotPhase == 0 || (g.slotPhase == 4 && !g.slotStopping[0] && !g.slotStopping[1] && !g.slotStopping[2]);
    DrawTextCentered(idle ? "Up/Down bet   Enter spin   Esc back to the counter" : "Enter: stop the reel", VW / 2, 322, 10, idle ? UI_DIM : UI_GOLD, false);
    if (g.coop) DrawText(TextFormat("Playing: %s", p.name.c_str()), 10, 340, 10, g.coopActive == 1 ? Color{120, 230, 255, 255} : UI_GOLD);
    DrawToast(g);
}
