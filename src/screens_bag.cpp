// The Bag: what you're carrying, in pockets like the games (Poke Balls, held items, evolution
// stones, TMs, key items). Read-only: items are used from the Shop, Party & Box and battles.
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "items.h"
#include "legend.h"
#include "moves.h"
#include "sprites.h"
#include "ui.h"

namespace {
const char* kPockets[5] = {"Poke Balls", "Held Items", "Evolution Stones", "TMs", "Key Items"};
const Color kPocketCol[5] = {{226, 70, 70, 255}, {80, 170, 230, 255}, {120, 200, 110, 255}, {170, 120, 220, 255}, {236, 180, 60, 255}};

struct BagRow {
    const ItemDef* item = nullptr;   // nullptr: a special key item (name / desc below)
    std::string name, desc;
    int count = 0;
};

int Pocket(const ItemDef& d) {
    if (IsBall(d)) return 0;
    if (IsStone(d)) return 2;
    if (IsTM(d)) return 3;
    if (d.fx == ItemFx::KeyItem || d.fx == ItemFx::ExpAll) return 4;
    return 1;
}

std::vector<BagRow> Rows(const Game& g, int pocket) {
    const Profile& p = g.profile;
    std::vector<BagRow> v;
    for (const auto& d : ItemList()) {
        if (Pocket(d) != pocket) continue;
        int n = BagCount(p, d.name);
        if (n <= 0) continue;
        BagRow r;
        r.item = &d;
        r.name = d.name;
        r.desc = d.desc;
        r.count = n;
        v.push_back(r);
    }
    if (pocket == 3)
        std::sort(v.begin(), v.end(), [](const BagRow& a, const BagRow& b) { return a.item->tmNo < b.item->tmNo; });
    if (pocket == 4) {   // the story's key items
        BagRow b;
        b.name = "Badge Case";
        b.desc = TextFormat("Holds your Gym Badges: %d of 8 collected.", BadgeCount(g));
        b.count = -1;
        v.insert(v.begin(), b);
        BagRow c;
        c.name = "Coin Case";
        c.desc = TextFormat("Holds your coins: %d.", p.coins);
        c.count = -1;
        v.insert(v.begin() + 1, c);
        if (LegendCaughtBy(g, 3) >= 0) {
            BagRow m;
            m.name = "Old Sea Map";
            m.desc = "A faded map given by a sailor in Vermilion. It shows the way to a distant island - Faraway Island.";
            m.count = -1;
            v.push_back(m);
        }
    }
    return v;
}

void Backpack(int x, int y, int pocket, float t) {
    // a yellow adventurer's backpack with a flap in the pocket's colour
    Color body{236, 190, 70, 255}, dark{170, 120, 40, 255}, flap = kPocketCol[pocket];
    float bob = std::sin(t * 2) * 1.0f;
    y += (int)bob;
    DrawRectangleRounded({(float)x + 6, (float)y - 10, 14, 14}, 0.6f, 6, dark);         // strap loop
    DrawRectangleRounded({(float)x + 9, (float)y - 7, 8, 9}, 0.6f, 6, Color{40, 30, 30, 255});
    DrawRectangleRounded({(float)x - 2, (float)y + 2, 30, 34}, 0.35f, 8, dark);
    DrawRectangleRounded({(float)x, (float)y, 30, 34}, 0.35f, 8, body);
    DrawRectangleRounded({(float)x + 2, (float)y + 2, 26, 14}, 0.4f, 8, flap);          // flap
    DrawRectangleRounded({(float)x + 2, (float)y + 12, 26, 4}, 0.4f, 8, ColorBrightness(flap, -0.3f));
    DrawRectangle(x + 13, y + 13, 4, 6, Color{250, 230, 160, 255});                       // buckle
    DrawRectangleRounded({(float)x + 6, (float)y + 22, 18, 10}, 0.4f, 6, ColorBrightness(body, -0.12f));   // front pocket
}
} // namespace

void DrawItemIcon(const ItemDef& d, float cx, float cy, float s) {
    if (IsBall(d)) {
        Color top = d.name == "Master Ball" ? Color{150, 80, 200, 255} : (d.name == "Ultra Ball" ? Color{60, 60, 60, 255} : (d.name == "Great Ball" ? Color{70, 120, 230, 255} : Color{230, 60, 60, 255}));
        DrawCircleV({cx, cy}, 5 * s, UI_INK);
        DrawCircleSector({cx, cy}, 4 * s, 180, 360, 12, top);
        DrawCircleSector({cx, cy}, 4 * s, 0, 180, 12, WHITE);
        DrawRectangle((int)(cx - 4 * s), (int)cy, (int)(8 * s), 1, UI_INK);
        DrawCircleV({cx, cy}, 1.2f * s, WHITE);
    } else if (IsStone(d)) {
        Color c = TypeColor(d.type);
        DrawPoly({cx, cy}, 6, 5.5f * s, 0, ColorBrightness(c, -0.4f));
        DrawPoly({cx, cy}, 6, 4.2f * s, 0, c);
        DrawRectangle((int)(cx - 2 * s), (int)(cy - 3 * s), (int)(2 * s), (int)(2 * s), Fade(WHITE, 0.8f));
    } else if (IsTM(d)) {
        DrawCircleV({cx, cy}, 6 * s, ColorBrightness(TypeColor(d.type), -0.35f));
        DrawCircleV({cx, cy}, 4.5f * s, TypeColor(d.type));
        DrawCircleV({cx, cy}, 1.5f * s, UI_INK);
    } else if (d.fx == ItemFx::KeyItem || d.fx == ItemFx::ExpAll) {
        DrawPoly({cx, cy}, 5, 5.5f * s, 0, Color{120, 100, 80, 255});
        DrawPoly({cx, cy}, 5, 4.0f * s, 0, Color{200, 180, 140, 255});
    } else if (d.fx == ItemFx::TypeBoost) {
        DrawRectangle((int)(cx - 4.5f * s), (int)(cy - 4.5f * s), (int)(9 * s), (int)(9 * s), TypeColor(d.type));
    } else {
        DrawRectangle((int)(cx - 4.5f * s), (int)(cy - 4.5f * s), (int)(9 * s), (int)(9 * s), Color{140, 220, 255, 255});
    }
}

// The Key Items that aren't real items: a badge case, a coin case, a rolled-up map.
static void DrawKeyIcon(const std::string& name, float cx, float cy, float s) {
    if (name == "Badge Case") {
        DrawRectangleRounded({cx - 6 * s, cy - 4.5f * s, 12 * s, 9 * s}, 0.3f, 4, Color{150, 40, 50, 255});
        DrawRectangleRounded({cx - 5 * s, cy - 3.5f * s, 10 * s, 7 * s}, 0.3f, 4, Color{210, 70, 80, 255});
        for (int i = 0; i < 4; i++) DrawCircleV({cx - 3.3f * s + i * 2.2f * s, cy}, 0.8f * s, Color{255, 214, 60, 255});
    } else if (name == "Coin Case") {
        DrawCircleV({cx, cy}, 5.5f * s, Color{150, 110, 30, 255});
        DrawCircleV({cx, cy}, 4.3f * s, Color{255, 200, 60, 255});
        DrawRectangle((int)(cx - 0.6f * s), (int)(cy - 2.5f * s), (int)std::max(1.0f, 1.2f * s), (int)(5 * s), Color{180, 130, 30, 255});
    } else {   // a map
        DrawRectangle((int)(cx - 5 * s), (int)(cy - 4 * s), (int)(10 * s), (int)(8 * s), Color{200, 170, 110, 255});
        DrawRectangle((int)(cx - 5 * s), (int)(cy - 4 * s), (int)(2 * s), (int)(8 * s), Color{150, 120, 70, 255});
        DrawCircleV({cx + 1.5f * s, cy}, 1.3f * s, Color{200, 50, 50, 255});
    }
}

void OpenBag(Game& g) {
    g.bagTab = std::max(0, std::min(4, g.bagTab));
    g.bagSel = 0;
    g.screen = Screen::Bag;
}

void UpdateBag(Game& g) {
    PlayerInput in = MenuInput();
    if (in.back) { PlaySfx(Sfx::MenuBack); EnterStoryHub(g); g.hubSel = 6; return; }
    if (in.left || in.right) {
        g.bagTab = (g.bagTab + (in.left ? 4 : 1)) % 5;
        g.bagSel = 0;
        PlaySfx(Sfx::MenuMove);
    }
    int n = (int)Rows(g, g.bagTab).size();
    if (n > 0 && in.up) { g.bagSel = (g.bagSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (n > 0 && in.down) { g.bagSel = (g.bagSel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.confirm && n > 0) {
        const auto rows = Rows(g, g.bagTab);
        const BagRow& r = rows[std::min(g.bagSel, n - 1)];
        if (r.item && (IsStone(*r.item) || IsTM(*r.item) || IsHoldable(*r.item)))
            Toast(g, IsHoldable(*r.item) ? "Give it to a Pokemon from Party & Box." : "Use it on a Pokemon from Party & Box.");
        else if (r.item && IsBall(*r.item)) Toast(g, "Throw it at a weakened wild Pokemon after a battle.");
    }
}

void DrawBag(const Game& g) {
    // HGSS-style: teal stripes, the backpack on the left, the pocket's items on the right
    DrawMenuBackdrop(Color{40, 120, 130, 255}, (float)GetTime());
    for (int y = 0; y < VH; y += 16) DrawRectangle(0, y, VW, 8, Color{50, 136, 146, 255});
    const Profile& p = g.profile;
    Color pc = kPocketCol[g.bagTab];
    DrawRectangle(0, 0, VW, 26, Color{24, 60, 70, 255});
    DrawRectangle(0, 26, VW, 2, pc);
    DrawTextShadow("BAG", 12, 4, 20, WHITE);
    if (g.coop) GDrawText(TextFormat("%s's Bag", p.name.c_str()), 64, 9, 10, g.coopActive == 1 ? Color{120, 230, 255, 255} : UI_GOLD);
    DrawCircle(VW - 66, 13, 4.5f, Color{170, 120, 20, 255});
    DrawCircle(VW - 66, 13, 3.5f, Color{255, 210, 80, 255});
    GDrawText(TextFormat("%d", p.coins), VW - 58, 9, 10, Color{255, 210, 80, 255});

    // left: the backpack and pocket tabs
    DrawPanel(10, 36, 150, 290, Color{24, 60, 70, 235}, pc);
    Backpack(55, 62, g.bagTab, g.t);
    for (int i = 0; i < 5; i++) {
        int y = 116 + i * 22;
        bool sel = g.bagTab == i;
        DrawPanel(18, y, 134, 18, sel ? kPocketCol[i] : Color{30, 70, 80, 255}, sel ? WHITE : ColorBrightness(kPocketCol[i], -0.3f));
        GDrawText(kPockets[i], 26, y + 4, 10, sel ? UI_INK : UI_TEXT);
        int count = 0;
        for (const auto& r : Rows(g, i)) count += r.count > 0 ? r.count : 0;
        if (count > 0) {
            const char* c = TextFormat("%d", count);
            GDrawText(c, 146 - TextW(c, 10), y + 4, 10, sel ? UI_INK : UI_DIM);
        }
    }
    GDrawText("< Left / Right >", 40, 232, 10, UI_DIM);
    GDrawText("switch pockets", 44, 244, 10, UI_DIM);

    // right: the items
    auto rows = Rows(g, g.bagTab);
    int n = (int)rows.size();
    DrawPanel(170, 36, 230, 290, Color{246, 244, 236, 255}, pc);
    DrawRectangle(170, 36, 230, 18, pc);
    GDrawText(kPockets[g.bagTab], 178, 40, 10, UI_INK);
    if (n == 0) {
        const char* empty[5] = {"No Poke Balls. Buy some at the Poke Mart!", "No held items. The Poke Mart sells them.",
                                "No evolution stones.", "No TMs. The Poke Mart sells them.", ""};
        DrawWrapped(empty[g.bagTab], 180, 66, 210, 10, Color{90, 90, 100, 255});
    }
    int rowsShown = 13, sel = std::min(g.bagSel, std::max(0, n - 1));
    int first = std::max(0, std::min(sel - rowsShown / 2, n - rowsShown));
    for (int k = first; k < std::min(n, first + rowsShown); k++) {
        const BagRow& r = rows[k];
        int y = 58 + (k - first) * 20;
        bool s = k == sel;
        if (s) {
            DrawRectangle(174, y, 222, 18, Fade(pc, 0.35f));
            DrawRectangleLines(174, y, 222, 18, ColorBrightness(pc, -0.3f));
        }
        if (r.item) DrawItemIcon(*r.item, 186, y + 9, 1.0f);
        else DrawKeyIcon(r.name, 186, y + 9, 1.0f);
        std::string nm = r.name;
        if (r.item && IsTM(*r.item)) nm = TextFormat("TM%02d %s", r.item->tmNo, r.item->move.c_str());
        GDrawText(nm.c_str(), 198, y + 5, 10, Color{40, 40, 50, 255});
        if (r.count > 0) {
            const char* c = TextFormat("x%d", r.count);
            GDrawText(c, 388 - TextW(c, 10), y + 5, 10, Color{80, 80, 90, 255});
        }
    }

    // details
    DrawPanel(410, 36, 220, 170, Color{24, 60, 70, 235}, pc);
    if (n > 0) {
        const BagRow& r = rows[sel];
        if (r.item) DrawItemIcon(*r.item, 432, 62, 2.2f);
        else DrawKeyIcon(r.name, 432, 62, 2.2f);
        DrawWrapped(r.name.c_str(), 452, 52, 172, 10, UI_GOLD);
        int y = 82;
        if (r.item && IsTM(*r.item)) {
            if (const MoveDef* mv = FindMove(r.item->move)) {
                DrawMoveLine(*mv, 416, y, 206, false);
                y += 16;
                y = DrawWrapped(DescribeMove(*mv).c_str(), 420, y, 200, 10, Color{150, 190, 220, 255}) + 4;
            }
            DrawWrapped("Teach it to a Pokemon from Party & Box (used up).", 420, y, 200, 10, UI_DIM);
        } else {
            y = DrawWrapped(r.desc.c_str(), 420, y, 200, 10, UI_TEXT) + 6;
            if (r.item) {
                const char* how = IsBall(*r.item) ? "Thrown at weakened wild Pokemon after a battle."
                                : IsStone(*r.item) ? "Use it on a Pokemon from Party & Box."
                                : IsHoldable(*r.item) ? "Give it to a Pokemon to hold from Party & Box." : "";
                if (*how) DrawWrapped(how, 420, y, 200, 10, UI_DIM);
            }
        }
    }
    // who's holding what
    DrawPanel(410, 212, 220, 114, Color{24, 60, 70, 235}, pc);
    GDrawText("Held by your Pokemon", 418, 218, 10, UI_GOLD);
    int k = 0;
    for (const auto& m : p.mons) {
        if (m.item.empty()) continue;
        if (k >= 7) { GDrawText("...", 418, 232 + k * 12, 10, UI_DIM); break; }
        int si = MonSpecies(m);
        if (si >= 0) DrawSpeciesFit(ShinyIdx(si, m.shiny), VIEW_FRONT, 424, 243 + k * 12, 11);
        GDrawText(TextFormat("%s: %s%s", m.species.c_str(), m.item.c_str(), m.boxed ? " (Box)" : ""), 434, 233 + k * 12, 10, UI_TEXT);
        k++;
    }
    if (k == 0) GDrawText("(nobody is holding an item)", 418, 234, 10, UI_DIM);
    GDrawText("Left/Right pocket   Up/Down item   Esc / B back", 12, 340, 10, Color{220, 240, 240, 255});
    DrawToast(g);
}
