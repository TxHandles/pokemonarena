// The Pokedex and the trade machine (co-op: local or online).
#include <algorithm>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "wild.h"
#include "sprites.h"
#include "ui.h"

namespace {

const Color kP2{120, 230, 255, 255};
Color PCol(int p) { return p == 0 ? UI_GOLD : kP2; }
const Color kAbility{255, 225, 130, 255};

// Pokedex order: every species with a dex number (the Pokemon folder's roster).
const std::vector<int>& DexList() {
    static std::vector<int> list;
    static size_t builtFor = 0;
    if (builtFor != AllSpecies().size()) {
        list.clear();
        for (int i = 0; i < (int)AllSpecies().size(); i++)
            if (AllSpecies()[i].dex > 0 && InSpeciesPool(i)) list.push_back(i);
        std::stable_sort(list.begin(), list.end(), [](int a, int b) { return AllSpecies()[a].dex < AllSpecies()[b].dex; });
        builtFor = AllSpecies().size();
    }
    return list;
}

const Profile& DexProfile(const Game& g) { return g.coop ? Prof(g, g.dexWho) : g.profile; }
int DexStage(const Game& g) { return g.coop ? g.coopSave.storyStage : g.profile.storyStage; }
int DexBadges(const Game& g) { return (int)(g.coop ? g.coopSave.badges.size() : g.profile.badges.size()); }

const char* HowCommon(float share) {
    if (share >= 0.25f) return "Common";
    if (share >= 0.10f) return "Uncommon";
    return "Rare";
}

// Where a Pokemon lives: every area and method, how common, and what still locks it.
void DrawWherePage(const Game& g, int idx, int x, int y, int w, int h) {
    const SpeciesDef& sp = AllSpecies()[idx];
    const Color ink{40, 34, 50, 255}, soft{100, 92, 110, 255}, green{40, 120, 60, 255}, lock{150, 140, 150, 255};
    DrawPanel(x, y, w, h, Color{240, 236, 228, 255}, Color{60, 40, 50, 255});
    DrawPanel(x + 8, y + 8, 52, 52, Color{200, 225, 210, 255}, Color{90, 110, 100, 255});
    DrawSpeciesFit(idx, VIEW_FRONT, x + 34, y + 57, 46);
    DrawText(TextFormat("No. %03d", sp.dex), x + 68, y + 10, 10, soft);
    DrawText(sp.name.c_str(), x + 68, y + 22, 20, ink);
    DrawText("WHERE TO FIND IT", x + w - 10 - TextW("WHERE TO FIND IT", 10), y + 10, 10, Color{190, 50, 50, 255});
    int cur = TrainingAreaIndex(g);
    if (cur >= 0) {
        std::string t = "Training now: " + WildAreas()[cur].name;
        DrawText(t.c_str(), x + w - 10 - TextW(t.c_str(), 10), y + 24, 10, soft);
    }
    auto spots = WildSpotsFor(idx, DexStage(g), DexBadges(g));
    int ty = y + 66;
    // column heads
    int cPlace = x + 10, cHow = x + 124, cCommon = x + 184, cLv = x + 262, cNote = x + 298;
    DrawText("Place", cPlace, ty, 10, soft);
    DrawText("How", cHow, ty, 10, soft);
    DrawText("How common", cCommon, ty, 10, soft);
    DrawText("Note", cNote, ty, 10, soft);
    DrawText("Lv", cLv, ty, 10, soft);
    DrawRectangle(x + 8, ty + 12, w - 16, 1, Color{200, 190, 190, 255});
    ty += 16;
    int N = (int)spots.size();
    const int rows = 15;
    int sel = std::max(0, std::min(g.dexWhereSel, N - 1));
    int first = std::max(0, std::min(sel - rows / 2, N - rows));
    for (int r = 0; r < rows && first + r < N; r++) {
        const WildSpot& s = spots[first + r];
        int ry = ty + r * 14;
        bool on = first + r == sel;
        bool here = s.open && s.area == cur;
        if (on) DrawRectangle(x + 6, ry - 2, w - 12, 14, Color{220, 200, 160, 255});
        bool avail = s.share > 0;
        Color c = avail ? ink : (s.open ? soft : lock);
        std::string place = s.place;
        while (TextW(place.c_str(), 10) > 110 && place.size() > 3) place = place.substr(0, place.size() - 4) + "...";
        if (here) DrawCircle(cPlace - 4, ry + 5, 2, green);
        DrawText(place.c_str(), cPlace, ry, 10, c);
        DrawText(WildMethodName(s.method).c_str(), cHow, ry, 10, c);
        if (avail) DrawText(TextFormat("%s %d%%", HowCommon(s.share), std::max(1, (int)std::lround(s.share * 100))), cCommon, ry, 10, green);
        else if (s.rare && s.open) DrawText("rare find", cCommon, ry, 10, Color{190, 60, 150, 255});
        else DrawText("-", cCommon, ry, 10, lock);
        DrawText(s.lo == s.hi ? TextFormat("%d", s.lo) : TextFormat("%d-%d", s.lo, std::min(s.hi, 100)), cLv, ry, 10, c);
        std::string note;
        if (!s.open) note = "after " + s.opensAt;
        else if (!s.methodOpen) note = (s.method == "surf" ? std::string("Surf") : WildMethodName(s.method)) + ": after " + s.methodAt;
        else if (DexBadges(g) < s.badges) note = TextFormat("needs %d badges", s.badges);
        else if (s.rare) note = "rare finds only";
        else if (here) note = "training here";
        while (TextW(note.c_str(), 10) > x + w - 8 - cNote && note.size() > 3) note = note.substr(0, note.size() - 4) + "...";
        DrawText(note.c_str(), cNote, ry, 10, here ? green : lock);
    }
    if (N == 0) {
        const std::string& n = sp.name;
        const char* special = n == "Eevee" ? "A gift for a Pokemon that loves you completely."
                            : n == "Omanyte" || n == "Kabuto" ? "Revived from a Mt. Moon fossil at the Cinnabar Lab."
                            : n == "Aerodactyl" ? "Revived from the Pewter Museum's Old Amber at the Cinnabar Lab."
                            : n == "Hitmonlee" || n == "Hitmonchan" ? "The Saffron Dojo's prize."
                            : n == "Snorlax" ? "Sleeping on Routes 16 and 12."
                            : n == "Porygon" ? "A Celadon Game Corner prize."
                            : sp.starter ? "Not in the wild - a starter Pokemon."
                            : "Not in the wild - evolve or trade for it.";
        DrawWrapped(special, x + 10, ty, w - 20, 10, ink);
    } else if (N > rows) {
        DrawText(TextFormat("%d-%d of %d", first + 1, std::min(N, first + rows), N), x + w - 70, y + h - 30, 10, soft);
    }
    DrawText("Levels are the games'; training battles scale to your team.", x + 10, y + h - 30, 10, soft);
    DrawText("Enter: train here   Left/Right: other Pokemon   Esc: back", x + 10, y + h - 16, 10, Color{190, 50, 50, 255});
}

void DrawBall(int x, int y, bool caught) {
    if (caught) {
        DrawCircle(x, y, 4, UI_INK);
        DrawCircle(x, y, 3, Color{230, 70, 60, 255});
        DrawRectangle(x - 3, y, 7, 3, WHITE);
        DrawRectangle(x - 3, y, 7, 1, UI_INK);
        DrawCircle(x, y, 1, WHITE);
    } else {
        DrawCircle(x, y, 2, Color{110, 104, 130, 255});
    }
}

std::string MethodText(int from, int to) {
    const SpeciesDef& sp = AllSpecies()[from];
    if (EvolutionIndex(from) == to) return "Lv" + std::to_string(sp.evolveLevel);
    for (const auto& a : sp.evoAlts) {
        if (SpeciesIndex(a.to) != to) continue;
        if (a.trade) return "trade";
        if (!a.item.empty()) return a.item;
        if (a.friendship > 0) return "friendship";
    }
    return "";
}

// The family as "A > B (Lv16)" lines, starting from the base form.
std::vector<std::pair<int, int>> FamilyLinks(int idx) {
    std::vector<std::pair<int, int>> links;
    int base = BaseFormOf(idx);
    std::vector<int> todo{base};
    for (size_t k = 0; k < todo.size() && k < 16; k++) {
        int a = todo[k];
        std::vector<int> kids;
        int e = EvolutionIndex(a);
        if (e >= 0) kids.push_back(e);
        for (const auto& alt : AllSpecies()[a].evoAlts) {
            int t = SpeciesIndex(alt.to);
            if (t >= 0 && t != a && std::find(kids.begin(), kids.end(), t) == kids.end()) kids.push_back(t);
        }
        for (int t : kids) { links.push_back({a, t}); todo.push_back(t); }
    }
    return links;
}

} // namespace

// ------------------------------------------------------------------ Pokedex

void OpenPokedex(Game& g, int who) {
    g.dexWho = who;
    g.dexSel = 0;
    g.dexScroll = 0;
    g.dexWhere = false;
    g.dexWhereSel = 0;
    g.screen = Screen::Pokedex;
}

void UpdatePokedex(Game& g) {
    PlayerInput in = MenuInput();
    const auto& list = DexList();
    int n = (int)list.size();
    if (n > 0 && g.dexWhere) {   // the 'where to find it' page
        int idx = list[std::max(0, std::min(n - 1, g.dexSel))];
        auto spots = WildSpotsFor(idx, DexStage(g), DexBadges(g));
        int N = (int)spots.size();
        if (in.back) { g.dexWhere = false; PlaySfx(Sfx::MenuBack); return; }
        if (N > 0 && in.up) { g.dexWhereSel = (g.dexWhereSel + N - 1) % N; PlaySfx(Sfx::MenuMove); }
        if (N > 0 && in.down) { g.dexWhereSel = (g.dexWhereSel + 1) % N; PlaySfx(Sfx::MenuMove); }
        if (in.left || in.right) {   // the previous / next Pokemon you've seen, page stays open
            int d = in.left ? -1 : 1;
            for (int i = g.dexSel + d; i >= 0 && i < n; i += d)
                if (DexHasSeen(DexProfile(g), AllSpecies()[list[i]].name)) { g.dexSel = i; g.dexWhereSel = 0; PlaySfx(Sfx::MenuMove); break; }
            const int rows = 19;
            if (g.dexSel < g.dexScroll) g.dexScroll = g.dexSel;
            if (g.dexSel >= g.dexScroll + rows) g.dexScroll = g.dexSel - rows + 1;
        }
        if (in.confirm && N > 0) {
            const WildSpot& w = spots[std::min(g.dexWhereSel, N - 1)];
            if (!w.open) Toast(g, w.place + " opens after " + w.opensAt + ".");
            else if (SetTrainArea(g, w.area)) PlaySfx(Sfx::MenuOk);
        }
        return;
    }
    if (in.back || n == 0) { PlaySfx(Sfx::MenuBack); EnterStoryHub(g); g.hubSel = 6; return; }
    if (in.confirm) {
        int idx = list[std::max(0, std::min(n - 1, g.dexSel))];
        if (DexHasSeen(DexProfile(g), AllSpecies()[idx].name)) { g.dexWhere = true; g.dexWhereSel = 0; PlaySfx(Sfx::MenuOk); }
        else PlaySfx(Sfx::MenuBack);
        return;
    }
    static float holdT = 0;
    bool upHeld = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W), downHeld = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
    int step = 0;
    if (in.up) step = -1;
    if (in.down) step = 1;
    if (upHeld || downHeld) {
        holdT += GetFrameTime();
        if (holdT > 0.35f) { step = upHeld ? -1 : 1; holdT = 0.30f; }
    } else {
        holdT = 0;
    }
    if (in.left) step = -10;
    if (in.right) step = 10;
    if (step) {
        g.dexSel = std::max(0, std::min(n - 1, g.dexSel + step));
        PlaySfx(Sfx::MenuMove);
    }
    const int rows = 19;
    if (g.dexSel < g.dexScroll) g.dexScroll = g.dexSel;
    if (g.dexSel >= g.dexScroll + rows) g.dexScroll = g.dexSel - rows + 1;
}

void DrawPokedex(const Game& g) {
    ClearBackground(Color{150, 30, 40, 255});
    DrawRectangle(0, 0, VW, 24, Color{110, 20, 30, 255});
    const Profile& P = DexProfile(g);
    const auto& list = DexList();
    int seen = 0, caught = 0;
    for (int idx : list) {
        const std::string& nm = AllSpecies()[idx].name;
        if (DexHasCaught(P, nm)) caught++;
        if (DexHasSeen(P, nm)) seen++;
    }
    DrawTextShadow("POKeDEX", 8, 6, 10, WHITE);
    DrawText(P.name.c_str(), 64, 7, 10, g.coop ? PCol(g.dexWho) : UI_GOLD);
    std::string tally = TextFormat("Seen %d    Caught %d    of %d", seen, caught, (int)list.size());
    DrawText(tally.c_str(), VW - 8 - TextW(tally.c_str(), 10), 7, 10, WHITE);

    // List
    const int rows = 19;
    DrawPanel(6, 28, 200, rows * 16 + 8, Color{30, 26, 40, 245});
    for (int r = 0; r < rows; r++) {
        int i = g.dexScroll + r;
        if (i >= (int)list.size()) break;
        int idx = list[i];
        const SpeciesDef& sp = AllSpecies()[idx];
        bool s = DexHasSeen(P, sp.name), c = DexHasCaught(P, sp.name);
        int y = 32 + r * 16;
        bool sel = i == g.dexSel;
        if (sel) DrawRectangle(8, y - 1, 196, 15, Color{90, 60, 110, 255});
        DrawBall(18, y + 6, c);
        DrawText(TextFormat("%03d", sp.dex), 26, y + 2, 10, sel ? UI_GOLD : UI_DIM);
        if (s) DrawSpeciesFit(idx, VIEW_FRONT, 60, y + 14, 14, WHITE, false);
        DrawText(s ? sp.name.c_str() : "-----", 72, y + 2, 10, sel ? UI_GOLD : (s ? UI_TEXT : UI_DIM));
    }
    if (list.size() > (size_t)rows) {   // scrollbar
        float f0 = (float)g.dexScroll / list.size(), f1 = (float)(g.dexScroll + rows) / list.size();
        DrawRectangle(202, 30 + (int)(f0 * rows * 16), 2, std::max(4, (int)((f1 - f0) * rows * 16)), UI_DIM);
    }
    if (list.empty()) return;

    // Entry
    int idx = list[std::max(0, std::min((int)list.size() - 1, g.dexSel))];
    const SpeciesDef& sp = AllSpecies()[idx];
    bool s = DexHasSeen(P, sp.name), c = DexHasCaught(P, sp.name);
    int x = 212, w = VW - x - 6;
    if (g.dexWhere && s) {
        DrawWherePage(g, idx, x, 28, w, 312);
        return;
    }
    DrawPanel(x, 28, w, 312, Color{240, 236, 228, 255}, Color{60, 40, 50, 255});
    DrawPanel(x + 8, 36, 116, 112, Color{200, 225, 210, 255}, Color{90, 110, 100, 255});
    DrawSpeciesFit(idx, VIEW_FRONT, x + 66, 142, 100, s ? WHITE : Color{30, 34, 40, 255});
    const Color ink{40, 34, 50, 255}, soft{100, 92, 110, 255};
    int tx = x + 134;
    DrawText(TextFormat("No. %03d", sp.dex), tx, 38, 10, soft);
    DrawText(s ? sp.name.c_str() : "?????", tx, 52, 20, ink);
    if (s) {
        std::string cat = (sp.category.empty() ? std::string("Unknown") : sp.category) + " Pokemon";
        DrawText(cat.c_str(), tx, 76, 10, soft);
        int bx = tx;
        bx += DrawTypeBadge(sp.type1, bx, 92) + 4;
        if (sp.type2 != Type::None) DrawTypeBadge(sp.type2, bx, 92);
        if (sp.heightDm > 0) DrawText(TextFormat("Height %.1f m", sp.heightDm / 10.0f), tx, 112, 10, ink);
        if (sp.weightHg > 0) DrawText(TextFormat("Weight %.1f kg", sp.weightHg / 10.0f), tx + 100, 112, 10, ink);
        if (c) { DrawBall(tx + 6, 132, true); DrawText("Caught", tx + 14, 128, 10, Color{190, 50, 50, 255}); }
        else DrawText("Seen - catch one to learn more", tx, 128, 10, soft);
    } else {
        DrawText("Not seen yet.", tx, 80, 10, soft);
    }
    int y = 156;
    if (s) y = DrawWrapped(sp.dexEntry.empty() ? "No data." : sp.dexEntry.c_str(), x + 10, y, w - 20, 10, ink, 12) + 4;
    if (s) {   // where to find it (any Pokemon you've seen, even in a gym battle)
        int stage = g.coop ? g.coopSave.storyStage : g.profile.storyStage, locked = 0;
        auto places = WildPlacesFor(idx, stage, locked);
        std::string where;
        for (size_t k = 0; k < places.size(); k++) where += (k ? ", " : "") + places[k];
        if (!places.empty() && locked > 0) where += TextFormat(" (+%d more further on)", locked);
        const std::string& n = sp.name;
        std::string special = n == "Eevee" ? "a gift for a Pokemon that loves you"
                            : n == "Omanyte" || n == "Kabuto" ? "revived from a Mt. Moon fossil at the Cinnabar Lab"
                            : n == "Aerodactyl" ? "revived from the Pewter Museum's Old Amber at the Cinnabar Lab"
                            : n == "Hitmonlee" || n == "Hitmonchan" ? "the Saffron Dojo's prize"
                            : n == "Snorlax" ? "sleeping on Routes 16 and 12"
                            : n == "Porygon" || n == "Abra" || n == "Clefairy" || n == "Scyther" || n == "Pinsir" || n == "Dratini" ? "Celadon Game Corner prize"
                            : "";
        if (places.empty()) {
            if (!special.empty()) where = special;
            else if (locked > 0) where = "somewhere further along your journey";
            else if (sp.starter) where = "not in the wild - a starter Pokemon";
            else where = "not in the wild - evolve or trade for it";
        } else if (!special.empty()) {
            where += "; " + special;
        }
        DrawText("Found:", x + 10, y, 10, Color{40, 110, 60, 255});
        y = DrawWrapped((where + "   (Enter: details)").c_str(), x + 52, y, w - 62, 10, Color{40, 110, 60, 255}, 11) + 3;
    }
    if (c) {
        // Abilities
        auto abLine = [&](const std::string& name, const char* label) {
            if (name.empty()) return;
            Ab a = AbilityByName(name);
            DrawText(label, x + 10, y, 10, soft);
            DrawText(name.c_str(), x + 56, y, 10, Color{140, 80, 20, 255});
            y = DrawWrapped(a != Ab::None ? AbilityInfo(a).desc : "", x + 66 + TextW(name.c_str(), 10), y, w - 76 - TextW(name.c_str(), 10), 10, soft, 11);
            y += 1;
        };
        abLine(sp.ability1, "Ability");
        abLine(sp.ability2, "   or");
        abLine(sp.hiddenAbility, "Hidden");
        // Base stats
        y += 2;
        int sx = x + 10, colW = (w - 20) / 3;
        struct St { const char* n; int v; } st[6] = {{"HP", sp.hp}, {"ATK", sp.atk}, {"DEF", sp.def}, {"SpA", sp.SpAtk()}, {"SpD", sp.SpDef()}, {"SPD", sp.spd}};
        for (int k = 0; k < 6; k++) {
            int cx = sx + (k % 3) * colW, cy = y + (k / 3) * 12;
            DrawText(st[k].n, cx, cy, 10, soft);
            DrawText(TextFormat("%d", st[k].v), cx + 24, cy, 10, ink);
            float f = std::min(1.0f, st[k].v / 160.0f);
            DrawRectangle(cx + 48, cy + 3, colW - 56, 4, Color{210, 204, 196, 255});
            DrawRectangle(cx + 48, cy + 3, (int)((colW - 56) * f), 4, st[k].v >= 100 ? Color{80, 180, 90, 255} : (st[k].v >= 70 ? Color{220, 170, 50, 255} : Color{220, 110, 70, 255}));
        }
        y += 28;
    }
    // Evolution family
    if (s) {
        auto links = FamilyLinks(idx);
        if (!links.empty() && y < 330) {
            DrawText("Evolution", x + 10, y, 10, soft);
            int lx = x + 70, ly = y;
            for (const auto& l : links) {
                if (ly > 326) break;
                auto nm = [&](int i) { return DexHasSeen(P, AllSpecies()[i].name) ? AllSpecies()[i].name : std::string("?????"); };
                std::string t = nm(l.first) + "  >  " + nm(l.second) + "  (" + MethodText(l.first, l.second) + ")";
                DrawText(t.c_str(), lx, ly, 10, (l.first == idx || l.second == idx) ? ink : soft);
                ly += 11;
            }
        }
    }
    DrawText("Up/Down browse   Left/Right jump 10   Enter where to find it   Esc back", 8, 346, 10, Color{255, 220, 220, 255});
}

// ------------------------------------------------------------------ Trading

std::string MonToText(const OwnedMon& m) {
    Profile p;
    p.name = "x";
    p.mons.push_back(m);
    return ProfileToText(p);
}

bool MonFromText(const std::string& text, OwnedMon& out) {
    Profile p;
    if (!ProfileFromText(text, "", p) || p.mons.empty()) return false;
    out = p.mons[0];
    return true;
}

namespace {

// A player's creatures in trade order: party first, then the Box.
std::vector<int> TradeOrder(const Profile& P) {
    std::vector<int> v;
    for (int i = 0; i < (int)P.mons.size(); i++) if (!P.mons[i].boxed) v.push_back(i);
    for (int i = 0; i < (int)P.mons.size(); i++) if (P.mons[i].boxed) v.push_back(i);
    return v;
}

void ResetTrade(Game& g) {
    for (int p = 0; p < 2; p++) {
        g.tradeOffer[p] = -1;
        g.tradeReady[p] = false;
        g.tradeCur[p] = std::min(g.tradeCur[p], std::max(0, (int)Prof(g, p).mons.size() - 1));
    }
    g.tradePhase = 0;
    g.tradeT = 0;
}

void LeaveTrade(Game& g) {
    ResetTrade(g);
    g.screen = Screen::CoopHub;   // OnlineSendTrade below sees we left
    OnlineSendTrade(g);
    EnterStoryHub(g);
    g.hubSel = 7;
}

} // namespace

void OpenTrade(Game& g) {
    if (g.coopActive >= 0) { g.coopP[g.coopActive] = g.profile; g.coopActive = -1; }
    g.tradeCur[0] = g.tradeCur[1] = 0;
    g.tradeScroll[0] = g.tradeScroll[1] = 0;
    ResetTrade(g);
    g.screen = Screen::Trade;
    OnlineSendTrade(g);
}

void ApplyTrade(Game& g, const int idx[2], const OwnedMon mon[2]) {
    std::string names[2] = {Prof(g, 0).name, Prof(g, 1).name};
    for (int p = 0; p < 2; p++) {
        Profile& P = Prof(g, p);
        int i = idx[p];
        g.tradeSent[p] = mon[p].species;
        g.tradeGot[p] = mon[1 - p].species;
        if (i < 0 || i >= (int)P.mons.size() || P.mons[i].species != mon[p].species) continue;
        OwnedMon in = mon[1 - p];
        in.boxed = P.mons[i].boxed;
        if (in.ot == P.name) in.ot.clear();               // came home to its original trainer
        else if (in.ot.empty()) in.ot = names[1 - p];
        in.friendship = START_FRIENDSHIP;                   // a new trainer: friendship starts over
        P.mons[i] = in;
        DexRegisterOwned(P);
        if (OnlineMine(g, p)) {
            int to = TradeEvolution(MonSpecies(in));
            if (to >= 0) g.evoQueue.push_back({i, to, p});
            SaveWho(g, p);
        }
    }
    OnlineSendProfile(g);
    g.tradePhase = 1;
    g.tradeT = 0;
    PlaySfx(Sfx::Heal);
}

void UpdateTrade(Game& g, float dt) {
    bool online = g.on.active;
    g.tradeT += dt;
    if (g.tradePhase == 1) {
        if (g.tradeT > 2.4f) { g.tradePhase = 2; g.tradeT = 0; PlaySfx(Sfx::LevelUp); }
        return;
    }
    if (g.tradePhase == 2) {
        PlayerInput in = MenuInput();
        if (g.tradeT > 0.4f && (in.confirm || in.back)) {
            PlaySfx(Sfx::MenuOk);
            ResetTrade(g);
            OnlineSendTrade(g);
            if (!g.evoQueue.empty()) {   // trade evolution!
                g.returnToTrade = true;
                AdvanceStoryQueues(g);
            }
        }
        return;
    }
    if (online && IsKeyPressed(KEY_ESCAPE) && g.tradeOffer[g.on.me] < 0) { PlaySfx(Sfx::MenuBack); LeaveTrade(g); return; }
    if (!online && IsKeyPressed(KEY_ESCAPE) && g.tradeOffer[0] < 0 && g.tradeOffer[1] < 0) { PlaySfx(Sfx::MenuBack); LeaveTrade(g); return; }
    bool changed = false;
    for (int p = 0; p < 2; p++) {
        if (online && p != g.on.me) continue;
        PlayerInput in = online ? MenuInput() : ReadInput(p, true);
        Profile& P = Prof(g, p);
        auto order = TradeOrder(P);
        int n = (int)order.size();
        if (n == 0) continue;
        if (!g.tradeReady[p]) {
            if (in.up) { g.tradeCur[p] = (g.tradeCur[p] + n - 1) % n; PlaySfx(Sfx::MenuMove); }
            if (in.down) { g.tradeCur[p] = (g.tradeCur[p] + 1) % n; PlaySfx(Sfx::MenuMove); }
            g.tradeCur[p] = std::max(0, std::min(n - 1, g.tradeCur[p]));
            int pick = order[g.tradeCur[p]];
            if (in.confirm) {
                if (g.tradeOffer[p] == pick) {
                    g.tradeReady[p] = true;
                    PlaySfx(Sfx::MenuOk);
                } else {
                    g.tradeOffer[p] = pick;
                    g.tradeReady[1 - p] = false;   // the deal changed: the other player looks again
                    PlaySfx(Sfx::MenuMove);
                }
                changed = true;
            } else if (in.back && g.tradeOffer[p] >= 0) {
                g.tradeOffer[p] = -1;
                g.tradeReady[1 - p] = false;
                PlaySfx(Sfx::MenuBack);
                changed = true;
            } else if (in.back && !online && p == 0 && g.tradeOffer[1] < 0) {
                PlaySfx(Sfx::MenuBack);
                LeaveTrade(g);
                return;
            }
        } else if (in.back) {
            g.tradeReady[p] = false;
            PlaySfx(Sfx::MenuBack);
            changed = true;
        }
        const int rows = 12;
        if (g.tradeCur[p] < g.tradeScroll[p]) g.tradeScroll[p] = g.tradeCur[p];
        if (g.tradeCur[p] >= g.tradeScroll[p] + rows) g.tradeScroll[p] = g.tradeCur[p] - rows + 1;
    }
    if (online) {
        if (changed) OnlineSendTrade(g);
        OnlineTradeUpdate(g);
        return;
    }
    if (g.tradeReady[0] && g.tradeReady[1] && g.tradeOffer[0] >= 0 && g.tradeOffer[1] >= 0) {
        int idx[2] = {g.tradeOffer[0], g.tradeOffer[1]};
        OwnedMon mons[2] = {g.coopP[0].mons[idx[0]], g.coopP[1].mons[idx[1]]};
        ApplyTrade(g, idx, mons);
    }
}

void DrawTrade(const Game& g) {
    ClearBackground(Color{18, 24, 40, 255});
    for (int i = 0; i < 12; i++) DrawRectangle(0, i * 30, VW, 15, Color{22, 30, 50, 255});
    bool online = g.on.active;
    DrawTextCentered("TRADE MACHINE", VW / 2, 6, 20, UI_GOLD);
    // Each player's creatures
    for (int p = 0; p < 2; p++) {
        const Profile& P = Prof(g, p);
        auto order = TradeOrder(P);
        int x = p == 0 ? 6 : VW - 6 - 200, w = 200, y0 = 32;
        bool canSee = !online || p == g.on.me;
        DrawPanel(x, y0, w, 296, UI_PANEL, PCol(p));
        DrawText(P.name.c_str(), x + 6, y0 + 5, 10, PCol(p));
        if (online && p != g.on.me && !g.on.partnerInTrade) {
            DrawWrapped(("Waiting for " + P.name + " to come to the trade machine...").c_str(), x + 8, y0 + 30, w - 16, 10, UI_DIM);
            continue;
        }
        const int rows = 12;
        int scroll = canSee ? g.tradeScroll[p] : 0;
        int offer = (online && p != g.on.me) ? g.on.partnerOffer : g.tradeOffer[p];
        if (online && p != g.on.me && offer >= 0) {   // keep the partner's offer in view
            auto it = std::find(order.begin(), order.end(), offer);
            int k = it == order.end() ? 0 : (int)(it - order.begin());
            scroll = std::max(0, k - rows + 1);
        }
        for (int r = 0; r < rows; r++) {
            int k = scroll + r;
            if (k >= (int)order.size()) break;
            const OwnedMon& m = P.mons[order[k]];
            int idx = MonSpecies(m);
            int y = y0 + 20 + r * 22;
            bool cur = canSee && k == g.tradeCur[p] && g.tradePhase == 0;
            bool off = order[k] == offer;
            DrawPanel(x + 4, y, w - 8, 20, off ? UI_PANEL_LIGHT : UI_PANEL, off ? PCol(p) : (cur ? UI_TEXT : Color{70, 64, 92, 255}));
            if (idx >= 0) DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, x + 16, y + 19, 18);
            if (m.shiny) DrawShinyStar((float)x + 9, (float)y + 5, 3);
            DrawText(m.species.c_str(), x + 28, y + 5, 10, cur ? UI_GOLD : UI_TEXT);
            DrawText(TextFormat("Lv%d", m.level), x + w - 54, y + 5, 10, UI_GOLD);
            if (m.boxed) DrawText("Box", x + w - 28, y + 5, 10, UI_DIM);
            if (!m.item.empty()) DrawRectangle(x + 24, y + 3, 3, 3, Color{140, 220, 255, 255});
        }
        bool ready = (online && p != g.on.me) ? g.on.partnerTradeReady : g.tradeReady[p];
        const char* st = ready ? "READY!" : (offer >= 0 ? "Offering - confirm again to accept" : "Pick a Pokemon to offer");
        DrawText(st, x + 6, y0 + 282, 10, ready ? Color{140, 230, 150, 255} : UI_DIM);
    }
    // The deal in the middle
    int cx = VW / 2, mx = 212, mw = VW - 424;
    DrawPanel(mx, 32, mw, 296);
    for (int p = 0; p < 2; p++) {
        const Profile& P = Prof(g, p);
        int offer = (online && p != g.on.me) ? g.on.partnerOffer : g.tradeOffer[p];
        OwnedMon m;
        bool have = false;
        if (online && p != g.on.me) have = offer >= 0 && MonFromText(g.on.partnerOfferMon, m);
        else if (offer >= 0 && offer < (int)P.mons.size()) { m = P.mons[offer]; have = true; }
        int y = 40 + p * 142;
        if (g.tradePhase != 0) {   // the profiles already hold the result: show what was sent
            DrawText(TextFormat("%s sends", P.name.c_str()), mx + 8, y, 10, PCol(p));
            int si = SpeciesIndex(g.tradeSent[p]);
            if (si >= 0 && g.tradePhase == 2) DrawSpeciesFit(si, VIEW_FRONT, mx + 34, y + 66, 50);
            DrawText(g.tradeSent[p].c_str(), mx + 66, y + 16, 10, UI_TEXT);
            continue;
        }
        DrawText(TextFormat("%s offers", P.name.c_str()), mx + 8, y, 10, PCol(p));
        if (!have) { DrawText("...", mx + 8, y + 16, 10, UI_DIM); continue; }
        int idx = MonSpecies(m);
        if (g.tradePhase == 0 && idx >= 0) DrawSpeciesFit(ShinyIdx(idx, m.shiny), VIEW_FRONT, mx + 34, y + 66, 50);
        if (g.tradePhase == 0 && m.shiny) DrawShinyStar((float)mx + 12, (float)y + 22, 4);
        DrawText(m.species.c_str(), mx + 66, y + 16, 10, UI_TEXT);
        DrawText(TextFormat("Lv%d", m.level), mx + 66, y + 28, 10, UI_GOLD);
        DrawText(TextFormat("%s nature", NatureName(m.nature)), mx + 106, y + 28, 10, Color{255, 200, 150, 255});
        std::string ab = MonAbility(m);
        if (!ab.empty()) DrawText(("Ability: " + ab).c_str(), mx + 66, y + 40, 10, kAbility);
        DrawText(m.item.empty() ? "No held item" : ("Holds " + m.item).c_str(), mx + 66, y + 52, 10, UI_DIM);
        if (!m.ot.empty()) DrawText(("From " + m.ot).c_str(), mx + 66, y + 64, 10, UI_DIM);
        if (idx >= 0 && TradeEvolution(idx) >= 0)
            DrawText(TextFormat("Evolves into %s when traded!", AllSpecies()[TradeEvolution(idx)].name.c_str()), mx + 8, y + 80, 10, Color{255, 150, 230, 255});
    }
    // Swap animation
    if (g.tradePhase == 1) {
        float u = std::min(1.0f, g.tradeT / 2.0f);
        for (int p = 0; p < 2; p++) {
            float sx = p == 0 ? 106.0f : VW - 106.0f, ex = p == 0 ? VW - 106.0f : 106.0f;
            float x = sx + (ex - sx) * u;
            float y = 180 - std::sin(u * 3.14159f) * 120 + (p ? 20 : -20) * std::sin(u * 3.14159f);
            DrawCircle((int)x, (int)y, 8, UI_INK);
            DrawCircle((int)x, (int)y, 7, Color{230, 70, 60, 255});
            DrawRectangle((int)x - 7, (int)y, 15, 7, WHITE);
            DrawRectangle((int)x - 7, (int)y, 15, 1, UI_INK);
            DrawCircle((int)x, (int)y, 2, WHITE);
            for (int k = 1; k < 6; k++) {
                float uu = std::max(0.0f, u - k * 0.03f);
                DrawCircle((int)(sx + (ex - sx) * uu), (int)(180 - std::sin(uu * 3.14159f) * 120 + (p ? 20 : -20) * std::sin(uu * 3.14159f)), 3 - k * 0.4f, Fade(PCol(p), 0.6f - k * 0.1f));
            }
        }
        DrawTextCentered("Trading...", cx, 300, 10, UI_TEXT, false);
    } else if (g.tradePhase == 2) {
        DrawRectangle(0, 120, VW, 100, Fade(UI_INK, 0.9f));
        for (int p = 0; p < 2; p++) {
            std::string msg = Prof(g, p).name + " sent " + g.tradeSent[p] + " and received " + g.tradeGot[p] + "!";
            DrawTextCentered(msg.c_str(), cx, 140 + p * 24, 10, PCol(p), false);
        }
        DrawTextCentered("Enter to continue", cx, 196, 10, UI_DIM, false);
    }
    if (online) DrawText("Up/Down choose   Enter offer, then Enter again to accept   Esc back", 8, 340, 10, UI_DIM);
    else DrawText("Each player: Up/Down choose, Confirm to offer, Confirm again to accept, Back to undo.  Esc leave", 8, 340, 10, UI_DIM);
}
