// FireRed's Fame Checker and Teachy TV, opened from the Bag.
//   The Fame Checker (World/fame.txt, tools/fame_checker.py): sixteen famous people, six things to learn about each,
//   picked up as you talk to people and read signs around Kanto and the Sevii Islands (overworld.cpp sets "fame:KEY:n").
//   Teachy TV: the old man's grandson explains the basics - this game's version of them.
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "input.h"
#include "ui.h"
#include "embedded.h"

bool OwHasFlag(const Game& g, const std::string& f);
void DrawPersonSprite(const std::string& name, float x, float y, float scale);
void EnterStoryHub(Game& g);

namespace {
struct FameFact { std::string q, a, where, who; };
struct FamePerson { std::string key, name, sprite; FameFact facts[6]; };
struct FameBook {
    std::vector<FamePerson> people;
    std::map<std::string, std::vector<std::string>> at, enter;   // "map|x|y" / "map" -> "KEY:n"
    bool loaded = false;
} gFame;

std::vector<std::string> SplitBar(const std::string& s) {
    std::vector<std::string> v;
    std::string cur;
    for (char c : s) { if (c == '|') { v.push_back(cur); cur.clear(); } else cur += c; }
    v.push_back(cur);
    return v;
}
void LoadFame() {
    if (gFame.loaded) return;
    gFame.loaded = true;
    char* txt = LoadFileText((DataDir() + "World/fame.txt").c_str());
    if (!txt) return;
    std::stringstream ss(txt);
    UnloadFileText(txt);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find(" = ");
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq);
        auto v = SplitBar(line.substr(eq + 3));
        if (k == "person" && v.size() >= 3) gFame.people.push_back({v[0], v[1], v[2], {}});
        else if (k == "fact" && v.size() >= 6) {
            int n = std::atoi(v[1].c_str());
            for (auto& p : gFame.people) if (p.key == v[0] && n >= 0 && n < 6) p.facts[n] = {v[2], v[3], v[4], v[5]};
        } else if (k == "at" && v.size() >= 5) gFame.at[v[0] + "|" + v[1] + "|" + v[2]].push_back(v[3] + ":" + v[4]);
        else if (k == "enter" && v.size() >= 3) gFame.enter[v[0]].push_back(v[1] + ":" + v[2]);
    }
}
bool Known(const Game& g, const std::string& key, int n) { return OwHasFlag(g, "fame:" + key + ":" + std::to_string(n)); }
int KnownCount(const Game& g, const FamePerson& p) { int c = 0; for (int n = 0; n < 6; n++) c += Known(g, p.key, n); return c; }

struct FameUi { int sel = 0, fact = 0; bool inFacts = false; } gUi;
struct TeachyUi { int ep = 0, page = 0; bool watching = false; float t = 0; } gTv;

// Teachy TV: the episodes (FireRed's six, about this game's battles)
struct Episode { const char* title; std::vector<const char*> pages; };
const std::vector<Episode>& Episodes() {
    static const std::vector<Episode> k = {
        {"Battle Basics", {
            "Hi! I'm the Teachy TV host! Today: how battles work here.",
            "Battles happen in real time. Move your Pokemon around the arena with the stick, or WASD.",
            "Each of its four moves has its own button - A, B, X, Y on a gamepad, J K L I on the keyboard.",
            "A move needs a moment to recharge after you use it. Mix your moves so something is always ready!",
            "Swap with RT / LT (U / Y). The Pokemon coming in is a fresh start - use it to dodge a bad matchup."}},
        {"Dodging and Jumping", {
            "Nobody waits their turn here, so keep moving!",
            "Dodge (LB, or Space) is a quick dash. Attacks pass straight through you while you dash. It needs 6 seconds to come back.",
            "Jump (RB, or E) clears water, puddles, ice and obstacles - but you can't use a move until you land.",
            "Watch for a foe's wind-up, then dodge sideways. A good dodge beats a strong Pokemon!"}},
        {"Type Matchups", {
            "Every move has a type. Hitting a Pokemon with a type it's weak to deals double damage - or more!",
            "Water beats Fire, Fire beats Grass, Grass beats Water... and there are eighteen types in all.",
            "Before a battle, the team screen marks your Pokemon with arrows: green ones are strong against the next foes, red ones are weak.",
            "A Pokemon also hits harder with moves of its own type. Bring a team that covers each other!"}},
        {"Catching Pokemon", {
            "To catch a wild Pokemon, wear it down first. At a quarter of its HP it's weakened and gives up.",
            "After the battle you throw a ball at it. The lower its HP was, the better your chances.",
            "Great Balls and Ultra Balls work better than Poke Balls. A Master Ball never misses!",
            "Weaken it carefully - a Pokemon that faints can't be caught."}},
        {"TMs and Moves", {
            "TMs teach a move to a Pokemon that can learn it. Use them from Party & Box.",
            "A Pokemon knows up to four moves. Learning a fifth means forgetting one.",
            "Forgot a move you liked? A Move Relearner can teach it again. Move Tutors around the world teach special moves.",
            "Out in the world, HMs clear the way - Cut, Surf, Strength... once you have the right Badge."}},
        {"Items in Battle", {
            "Potions and other Medicine go on your item buttons: open the Bag, pick the Medicine and choose a slot.",
            "In battle, press that button (the D-pad, or C V B N) to use it on the Pokemon that's out.",
            "Each item button has a 30-second cooldown, so pick your moment!",
            "Held items work all the time - give one to each Pokemon from Party & Box."}},
    };
    return k;
}
}  // namespace

std::vector<std::string> FameNotesAt(const std::string& map, int x, int y) {
    LoadFame();
    auto it = gFame.at.find(map + "|" + std::to_string(x) + "|" + std::to_string(y));
    return it == gFame.at.end() ? std::vector<std::string>{} : it->second;
}
std::vector<std::string> FameNotesOnEnter(const std::string& map) {
    LoadFame();
    auto it = gFame.enter.find(map);
    return it == gFame.enter.end() ? std::vector<std::string>{} : it->second;
}
std::string FamePersonName(const std::string& key) {
    LoadFame();
    for (const auto& p : gFame.people) if (p.key == key) return p.name;
    return key;
}

// ------------------------------------------------------------------ the Fame Checker
void OpenFameChecker(Game& g) {
    LoadFame();
    gUi = FameUi{};
    for (int k = 0; k < (int)gFame.people.size(); k++) if (KnownCount(g, gFame.people[k])) { gUi.sel = k; break; }   // (the first one you know about)
    g.screen = Screen::FameChecker;
    PlaySfx(Sfx::MenuOk);
}

void UpdateFameChecker(Game& g) {
    PlayerInput in = MenuInput();
    int n = (int)gFame.people.size();
    if (n == 0) { EnterStoryHub(g); return; }
#ifdef ARENA_AUTOTEST
    static float at = 0;
    at += GetFrameTime();
    if (at > 1.2f && !gUi.inFacts) { gUi.inFacts = true; }
    if (at > 2.4f) { at = 0; in.back = true; gUi.inFacts = false; }
#endif
    if (!gUi.inFacts) {
        if (in.up) { gUi.sel = (gUi.sel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
        if (in.down) { gUi.sel = (gUi.sel + 1) % n; PlaySfx(Sfx::MenuMove); }
        if (in.confirm || in.right) {
            if (KnownCount(g, gFame.people[gUi.sel]) == 0) { PlaySfx(Sfx::MenuBack); return; }
            gUi.inFacts = true; gUi.fact = 0; PlaySfx(Sfx::MenuOk);
        }
        if (in.back) { PlaySfx(Sfx::MenuBack); EnterStoryHub(g); }
        return;
    }
    if (in.left && gUi.fact % 3 == 0) { gUi.inFacts = false; PlaySfx(Sfx::MenuBack); return; }
    if (in.left) { gUi.fact--; PlaySfx(Sfx::MenuMove); }
    if (in.right && gUi.fact % 3 < 2) { gUi.fact++; PlaySfx(Sfx::MenuMove); }
    if (in.up && gUi.fact >= 3) { gUi.fact -= 3; PlaySfx(Sfx::MenuMove); }
    if (in.down && gUi.fact < 3) { gUi.fact += 3; PlaySfx(Sfx::MenuMove); }
    if (in.back) { gUi.inFacts = false; PlaySfx(Sfx::MenuBack); }
}

void DrawFameChecker(const Game& g) {
    ClearBackground(Color{30, 44, 70, 255});
    DrawTextCentered("FAME CHECKER", VW / 2, 6, 10, UI_GOLD);
    int n = (int)gFame.people.size();
    DrawPanel(8, 24, 150, VH - 50);
    for (int k = 0; k < n; k++) {
        const auto& p = gFame.people[k];
        int c = KnownCount(g, p);
        int y = 30 + k * 17;
        if (k == gUi.sel) DrawRectangle(10, y - 2, 146, 16, gUi.inFacts ? Color{60, 80, 120, 255} : Color{90, 120, 170, 255});
        GDrawText(c ? p.name.c_str() : "? ? ? ? ?", 16, y + 1, 10, c ? (k == gUi.sel ? UI_GOLD : UI_TEXT) : UI_DIM);
        if (c) GDrawText(TextFormat("%d/6", c), 126, y + 1, 10, UI_DIM);
    }
    if (n == 0) return;
    const auto& p = gFame.people[gUi.sel];
    int known = KnownCount(g, p);
    DrawPanel(166, 24, VW - 174, VH - 50);
    if (!known) {
        DrawTextCentered("You haven't heard anything about this person yet.", 166 + (VW - 174) / 2, 120, 10, UI_DIM, false);
    } else {
        DrawPersonSprite(p.sprite, 176, 30, 2.0f);
        GDrawText(p.name.c_str(), 250, 40, 20, UI_GOLD);
        GDrawText(TextFormat("%d of 6 things you've heard", known), 250, 66, 10, UI_DIM);
        // six notes: a picture of who told you, the question it answers
        for (int f = 0; f < 6; f++) {
            int x = 180 + (f % 3) * 150, y = 98 + (f / 3) * 40;
            bool k = Known(g, p.key, f);
            bool sel = gUi.inFacts && gUi.fact == f;
            DrawRectangle(x, y, 140, 32, sel ? Color{90, 120, 170, 255} : Color{40, 56, 90, 255});
            if (sel) RectLines(x - 1, y - 1, 142, 34, UI_GOLD);
            GDrawText(k ? p.facts[f].q.c_str() : "? ? ?", x + 4, y + 4, 10, k ? UI_TEXT : UI_DIM);
            if (k) GDrawText(p.facts[f].where.c_str(), x + 4, y + 18, 10, UI_DIM);
        }
        int f = gUi.inFacts ? gUi.fact : -1;
        if (f >= 0 && Known(g, p.key, f)) {
            const auto& ff = p.facts[f];
            int y = DrawWrapped(ff.a.c_str(), 180, 186, VW - 200, 10, UI_TEXT);
            std::string src = "Heard at " + ff.where + (ff.who.empty() ? "" : " - " + ff.who);
            GDrawText(src.c_str(), 180, y + 6, 10, UI_DIM);
        } else if (f >= 0) DrawWrapped("You haven't heard about this yet. Keep talking to people!", 180, 186, VW - 200, 10, UI_DIM);
        else DrawWrapped("Pick this person to read what you've heard.", 180, 186, VW - 200, 10, UI_DIM);
    }
    GDrawText(gUi.inFacts ? "Arrows: a note   Esc: back to the list" : "Up/Down: a person   Enter: their notes   Esc: close", 10, VH - 18, 10, UI_DIM);
}

// ------------------------------------------------------------------ Teachy TV
void OpenTeachyTV(Game& g) {
    gTv = TeachyUi{};
    g.screen = Screen::TeachyTV;
    PlaySfx(Sfx::MenuOk);
}

void UpdateTeachyTV(Game& g) {
    PlayerInput in = MenuInput();
    gTv.t += GetFrameTime();
    const auto& eps = Episodes();
    int n = (int)eps.size();
#ifdef ARENA_AUTOTEST
    if (gTv.t > 1.0f && !gTv.watching) { gTv.watching = true; gTv.t = 0; }
    else if (gTv.watching && gTv.t > 1.0f) { gTv.t = 0; if (++gTv.page >= (int)eps[gTv.ep].pages.size()) { gTv.watching = false; EnterStoryHub(g); return; } }
    return;
#endif
    if (!gTv.watching) {
        if (in.up) { gTv.ep = (gTv.ep + n - 1) % n; PlaySfx(Sfx::MenuMove); }
        if (in.down) { gTv.ep = (gTv.ep + 1) % n; PlaySfx(Sfx::MenuMove); }
        if (in.confirm) { gTv.watching = true; gTv.page = 0; gTv.t = 0; PlaySfx(Sfx::MenuOk); }
        if (in.back) { PlaySfx(Sfx::MenuBack); EnterStoryHub(g); }
        return;
    }
    if (in.back) { gTv.watching = false; PlaySfx(Sfx::MenuBack); return; }
    if (in.confirm || in.right) {
        PlaySfx(Sfx::MenuMove);
        if (++gTv.page >= (int)eps[gTv.ep].pages.size()) { gTv.watching = false; gTv.ep = (gTv.ep + 1) % n; }
        gTv.t = 0;
    }
    if (in.left && gTv.page > 0) { gTv.page--; PlaySfx(Sfx::MenuMove); }
}

void DrawTeachyTV(const Game& g) {
    (void)g;
    ClearBackground(Color{24, 24, 30, 255});
    // the set: a TV with a wooden frame
    DrawRectangleRounded({60, 30, VW - 120.0f, VH - 80.0f}, 0.06f, 8, Color{150, 100, 50, 255});
    DrawRectangleRounded({76, 44, VW - 152.0f, VH - 120.0f}, 0.05f, 8, Color{20, 40, 50, 255});
    for (int y = 46; y < VH - 78; y += 3) DrawRectangle(78, y, VW - 156, 1, Fade(Color{120, 200, 220, 255}, 0.05f));
    DrawTextCentered("TEACHY TV", VW / 2, 14, 10, UI_GOLD);
    const auto& eps = Episodes();
    if (!gTv.watching) {
        DrawTextCentered("What would you like to watch?", VW / 2, 60, 10, UI_TEXT);
        for (int k = 0; k < (int)eps.size(); k++) {
            int y = 90 + k * 24;
            if (k == gTv.ep) DrawRectangle(140, y - 4, VW - 280, 20, Color{60, 110, 130, 255});
            DrawTextCentered(eps[k].title, VW / 2, y, 10, k == gTv.ep ? UI_GOLD : UI_TEXT, false);
        }
        GDrawText("Up/Down: an episode   Enter: watch it   Esc: turn it off", 10, VH - 18, 10, UI_DIM);
        return;
    }
    const auto& e = eps[gTv.ep];
    DrawTextCentered(e.title, VW / 2, 56, 20, Color{140, 230, 255, 255});
    DrawPersonSprite("youngster", VW / 2 - 32.0f, 86, 2.0f);
    std::string page = e.pages[std::min(gTv.page, (int)e.pages.size() - 1)];
    int shown = std::min((int)page.size(), (int)(gTv.t * 60));
    DrawPanel(96, 166, VW - 192, 96, Color{30, 34, 50, 240});
    DrawWrapped(page.substr(0, shown).c_str(), 108, 176, VW - 216, 10, UI_TEXT, 14);
    GDrawText(TextFormat("%d / %d", gTv.page + 1, (int)e.pages.size()), VW - 150, 248, 10, UI_DIM);
    GDrawText("Enter: next   Left: back a page   Esc: the episode list", 10, VH - 18, 10, UI_DIM);
}
