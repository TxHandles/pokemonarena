#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "arena.h"
#include "game.h"
#include "settings.h"
#include "sprites.h"
#include "ui.h"

void Toast(Game& g, const std::string& msg) {
    g.toast = msg;
    g.toastT = 2.5f;
}

void DrawToast(const Game& g) {
    if (g.toastT <= 0 || g.toast.empty()) return;
    float a = std::min(1.0f, g.toastT * 2);
    int w = TextW(g.toast.c_str(), 10) + 16;
    int x = VW / 2 - w / 2, y = VH - 30;
    DrawRectangle(x, y, w, 16, Fade(UI_INK, 0.9f * a));
    RectLines(x, y, w, 16, Fade(UI_GOLD, a));
    GDrawText(g.toast.c_str(), x + 8, y + 3, 10, Fade(UI_TEXT, a));
}

void DrawMenuItem(const char* text, int cx, int y, int size, bool selected, bool dim) {
    // staggered slide-in when the screen opens (by row position), and a little pop when the cursor lands
    float e = UiEnterT(std::max(0, (y - 40) / 16));
    const int keyX = cx;
    cx -= (int)std::lround((1 - e) * 36);
    // remember which rows were selected last frame, so each newly selected row pops once
    struct Sel { int x, y; double at; };
    static std::vector<Sel> prev, cur;
    static double lastT = -1;
    if (GetTime() != lastT) { lastT = GetTime(); prev.swap(cur); cur.clear(); }
    double popAt = GetTime() - 1.0;
    if (selected) {
        bool found = false;
        for (const auto& q : prev) if (q.x == keyX && q.y == y) { popAt = q.at; found = true; break; }
        if (!found) popAt = GetTime();
        cur.push_back({keyX, y, popAt});
    }
    if (selected) {   // a rounded pill with a Poke Ball cursor
        float pu = (float)std::min(1.0, (GetTime() - popAt) / 0.18);
        float pop = Cfg().fxLevel >= 2 ? 1.0f : 0.82f + 0.18f * pu + std::sin(pu * PI) * 0.08f;
        int w = (int)((TextW(text, size) + 30) * pop);
        Rectangle rc{(float)(cx - w / 2), (float)(y - 3), (float)w, (float)(size + 5)};
        DrawRectangleRounded({rc.x + 1, rc.y + 1.5f, rc.width, rc.height}, 0.6f, 8, Fade(BLACK, 0.3f * e));
        DrawRectangleRounded(rc, 0.6f, 8, Fade(UI_PANEL_LIGHT, 0.95f * e));
        DrawRectangleRoundedLinesEx(rc, 0.6f, 8, 1.0f, Fade(UI_GOLD, e));
        if (pu < 1) DrawRectangleRounded(rc, 0.6f, 8, Fade(WHITE, 0.25f * (1 - pu)));
        float bob = std::sin(GetTime() * 6) * 1.0f;
        DrawPokeBall({rc.x - 2 + bob, rc.y + rc.height / 2}, 4.2f, (float)std::sin(GetTime() * 4) * 20 + (1 - pu) * 180, 0, Color{226, 52, 52, 255}, (unsigned char)(255 * e));
    }
    Color c = selected ? UI_GOLD : (dim ? UI_DIM : UI_TEXT);
    DrawTextCentered(text, cx, y, size, Fade(c, e), e > 0.6f);
}

static std::string Keys(int layout, Act a, Act b, Act c, Act d) {
    const auto& k = Cfg().keys[layout];
    return std::string(KeyName(k[(int)a][0])) + " " + KeyName(k[(int)b][0]) + " " + KeyName(k[(int)c][0]) + " " + KeyName(k[(int)d][0]);
}

void DrawControlsHelp(int x, int y, bool twoPlayer) {
    const auto& K = Cfg().keys;
    if (twoPlayer) {
        std::string p1 = "P1: move " + Keys(LAYOUT_P1, Act::Up, Act::Left, Act::Down, Act::Right) + "   moves " +
                         Keys(LAYOUT_P1, Act::Move1, Act::Move2, Act::Move3, Act::Move4) + "   swap " + KeyName(K[LAYOUT_P1][(int)Act::SwapPrev][0]) + "/" + KeyName(K[LAYOUT_P1][(int)Act::Swap][0]) +
                         "   dodge " + KeyName(K[LAYOUT_P1][(int)Act::Dodge][0]) + "   jump " + KeyName(K[LAYOUT_P1][(int)Act::Jump][0]);
        std::string p2 = "P2: move " + Keys(LAYOUT_P2, Act::Up, Act::Left, Act::Down, Act::Right) + "   moves " +
                         Keys(LAYOUT_P2, Act::Move1, Act::Move2, Act::Move3, Act::Move4) + "   swap " + KeyName(K[LAYOUT_P2][(int)Act::SwapPrev][0]) + "/" + KeyName(K[LAYOUT_P2][(int)Act::Swap][0]) +
                         "   dodge " + KeyName(K[LAYOUT_P2][(int)Act::Dodge][0]) + "   jump " + KeyName(K[LAYOUT_P2][(int)Act::Jump][0]);
        GDrawText(p1.c_str(), x, y, 10, UI_TEXT);
        GDrawText(p2.c_str(), x, y + 12, 10, UI_TEXT);
    } else {
        std::string l = "Move: " + Keys(LAYOUT_SOLO, Act::Up, Act::Left, Act::Down, Act::Right) + "   Moves: " +
                        Keys(LAYOUT_SOLO, Act::Move1, Act::Move2, Act::Move3, Act::Move4) + "   Swap: " + KeyName(K[LAYOUT_SOLO][(int)Act::SwapPrev][0]) + "/" + KeyName(K[LAYOUT_SOLO][(int)Act::Swap][0]) +
                        "   Dodge: " + KeyName(K[LAYOUT_SOLO][(int)Act::Dodge][0]) + "   Jump: " + KeyName(K[LAYOUT_SOLO][(int)Act::Jump][0]);
        GDrawText(l.c_str(), x, y, 10, UI_TEXT);
        GDrawText("Gamepad: stick moves, A B X Y moves, LB dodge, RB jump, LT / RT swap back / next (Settings > Controls)", x, y + 12, 10, UI_TEXT);
    }
    GDrawText(Cfg().autoAim ? "Attacks aim at your opponent automatically.   Esc / P: pause   F11: window / fullscreen"
                           : "Attacks fire the way you face.   Esc / P: pause   F11: window / fullscreen", x, y + 24, 10, UI_DIM);
}

void DrawMoveLine(const MoveDef& m, int x, int y, int w, bool details) {
    DrawRectangle(x, y + 1, 6, 8, TypeColor(m.type));
    GDrawText(m.name.c_str(), x + 10, y, 10, UI_TEXT);
    char buf[48];
    if (IsDamaging(m)) std::snprintf(buf, sizeof buf, "%s  pow %d  %.1fs", MoveKindName(m.kind), m.power, m.cooldown);
    else std::snprintf(buf, sizeof buf, "%s  %.1fs", MoveKindName(m.kind), m.cooldown);
    GDrawText(buf, x + w - TextW(buf, 10), y, 10, UI_DIM);
    if (details) {
        std::string d = DescribeMove(m);
        while (d.size() > 4 && TextW(d.c_str(), 10) > w - 10) d = d.substr(0, d.size() - 4) + "...";   // keep it inside its panel
        GDrawText(d.c_str(), x + 10, y + 11, 10, Color{150, 190, 220, 255});
    }
}

void DrawCreatureCard(int species, int level, int x, int y, int w, int h, bool selected) {
    bool shiny = (species & SHINY_FLAG) != 0;   // ShinyIdx(): the card shows it in shiny colours
    const int sprite = species;
    species &= ~SHINY_FLAG;
    const SpeciesDef& sp = AllSpecies()[species];
    DrawPanel(x, y, w, h, UI_PANEL, selected ? UI_GOLD : Color{80, 74, 104, 255});
    DrawRectangle(x + 1, y + 1, w - 2, 3, TypeColor(sp.type1));
    DrawEllipse(x + w / 2, y + h - 30, 16, 4, Fade(BLACK, 0.3f));
    DrawSpeciesFit(sprite, VIEW_FRONT, x + w / 2, y + h - 28, std::min(w - 20, h - 50));
    if (shiny) { DrawShinyStar((float)x + w - 9, (float)y + 11, 4); GDrawText("Shiny!", x + w - 46, y + 7, 10, Color{255, 214, 80, 255}); }
    DrawTextCentered(sp.name.c_str(), x + w / 2, y + h - 24, 10, UI_TEXT, false);
    int bw = TextW(TypeName(sp.type1), 10) + 6 + (sp.type2 != Type::None ? TextW(TypeName(sp.type2), 10) + 8 : 0);
    int bx = x + w / 2 - bw / 2;
    bx += DrawTypeBadge(sp.type1, bx, y + h - 13) + 2;
    if (sp.type2 != Type::None) DrawTypeBadge(sp.type2, bx, y + h - 13);
    if (level > 0) GDrawText(TextFormat("Lv%d", level), x + 5, y + 7, 10, UI_GOLD);
}

std::vector<RosterEntry> FreePlayRoster() {
    std::vector<RosterEntry> r;
    for (int i = 0; i < (int)AllSpecies().size(); i++) {
        if (!InSpeciesPool(i)) continue;
        RosterEntry e;
        e.species = i;
        e.level = FREE_PLAY_LEVEL;
        e.moves = FreePlayMoves(AllSpecies()[i]);
        for (int k = 0; k < 6; k++) e.iv[k] = 31;   // free play: perfect IVs all round, so it's even
        r.push_back(e);
    }
    return r;
}

std::vector<RosterEntry> ProfileRoster(const Profile& p, bool partyOnly) {
    std::vector<RosterEntry> r;
    for (int i = 0; i < (int)p.mons.size(); i++) {
        if (MonSpecies(p.mons[i]) < 0 || (partyOnly && p.mons[i].boxed)) continue;
        CombatantSpec c = MonCombatant(p, i);
        RosterEntry e;
        e.species = c.species;
        e.level = c.level;
        e.xp = c.xp;
        e.moves = c.moves;
        e.ownedId = i;
        e.hpFrac = p.mons[i].hp;
        e.item = p.mons[i].item;
        e.abilitySlot = c.abilitySlot;
        e.nature = c.nature;
        e.shiny = c.shiny;
        for (int k = 0; k < 6; k++) { e.iv[k] = c.iv[k]; e.ev[k] = c.ev[k]; }
        e.nick = c.nick;
        r.push_back(e);
    }
    return r;
}
