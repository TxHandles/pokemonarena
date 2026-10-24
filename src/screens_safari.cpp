// The Safari Zone (FireRed / Emerald) and the Bug-Catching Contest (HeartGold): a wild Pokemon met without a battle.
// Safari: throw a Safari Ball, bait (it eats - harder to catch, slower to flee) or a rock (it's angry - easier to
// catch, quicker to run), or run. The Contest: Park Balls only, and you keep the one bug you like best for the judges.
// The run itself (balls and steps left, the gate it ends at, the contest's bug) is g.ow.safari (overworld.cpp).
#include <algorithm>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "data.h"
#include "game.h"
#include "sprites.h"
#include "ui.h"


namespace {
int CatchRate(const SpeciesDef& sp) { return sp.legendary ? 3 : sp.evolveLevel > 0 || !sp.evolvesTo.empty() ? 190 : 75; }
std::vector<const char*> Options(const Game& g) {
    if (g.ow.safari.contest) return {"Park Ball", "Run"};
    return {"Safari Ball", "Bait", "Rock", "Run"};
}
void Fled(Game& g, const std::string& why) {
    auto& e = g.safariEnc;
    e.phase = 2;
    e.msg = why;
    PlaySfx(Sfx::Flee);
}
// after a throw that missed, bait or a rock: does it run?
void MaybeFlee(Game& g) {
    auto& e = g.safariEnc;
    const SpeciesDef& sp = AllSpecies()[e.species];
    if (g.ow.safari.contest) {   // (a contest bug doesn't run off - you battle it in HeartGold)
        if (g.ow.safari.balls <= 0) { e.phase = 2; e.msg = "You're out of Park Balls!"; }
        return;
    }
    float p = std::min(0.75f, std::max(0.05f, sp.spd * 2 / 255.0f * 0.5f));
    if (e.angry > 0) p *= 2;
    if (e.eating > 0) p *= 0.25f;
    if (e.angry > 0) e.angry--;
    if (e.eating > 0) e.eating--;
    if (GetRandomValue(0, 999) < (int)(p * 1000)) Fled(g, "The wild " + sp.name + " fled!");
    else if (g.ow.safari.balls <= 0) { e.phase = 2; e.msg = "PA: Ding-dong! You're out of Safari Balls!"; }
}
}   // namespace

void OpenSafariEncounter(Game& g, int species, int level, bool shiny, int nature, const int iv[6]) {
    auto& e = g.safariEnc;
    e = Game::SafariEnc();
    e.species = species;
    e.level = level;
    e.shiny = shiny;
    e.nature = nature;
    for (int k = 0; k < 6; k++) e.iv[k] = iv ? iv[k] : GetRandomValue(0, 31);
    e.msg = "A wild " + AllSpecies()[species].name + " appeared!";
    DexSee(g.profile, AllSpecies()[species].name);
    g.screen = Screen::Safari;
    PlaySfx(Sfx::Buff);
}

void UpdateSafari(Game& g) {
    auto& e = g.safariEnc;
    auto& run = g.ow.safari;
    PlayerInput in = MenuInput();
    e.t += GetFrameTime();
    const SpeciesDef& sp = AllSpecies()[e.species];
    if (e.phase == 1) {   // the ball shakes
        if (e.t < 0.5f + 0.55f * std::min(3, e.shakes)) return;
        if (e.shakes >= 4) {
            e.caught = true;
            e.phase = 2;
            PlaySfx(Sfx::Caught);
            if (run.contest) {
                bool had = run.bug >= 0;
                e.msg = "Gotcha! " + sp.name + " was caught!" + (had ? "|You already have " + AllSpecies()[run.bug].name + " - keep the new one?" : "");
                if (!had) {
                    run.bug = e.species; run.bugLevel = e.level; run.bugShiny = e.shiny; run.bugNature = e.nature;
                    for (int k = 0; k < 6; k++) run.bugIv[k] = e.iv[k];
                }
            } else {
                OwnedMon m = NewMon(e.species, e.level);
                m.caught = true;
                m.shiny = e.shiny;
                if (e.nature >= 0) m.nature = e.nature;
                for (int k = 0; k < 6; k++) m.iv[k] = e.iv[k];
                m.metAt = g.ow.wildArea.empty() ? "Safari Zone" : g.ow.wildArea;
                bool boxed = AddMon(g.profile, m);
                g.profile.dexCaught.insert(m.species);
                SaveProfile(g.profile);
                e.msg = "Gotcha! " + sp.name + " was caught!" + std::string(boxed ? "|It was sent to the Box." : "");
            }
            return;
        }
        e.phase = 0;
        static const char* kBroke[4] = {"Oh no! The Pokemon broke free!", "Aww! It appeared to be caught!", "Aargh! Almost had it!", "Shoot! It was so close, too!"};
        e.msg = kBroke[std::min(3, e.shakes)];
        PlaySfx(Sfx::BallOpen);
        MaybeFlee(g);
        return;
    }
    if (e.phase == 2) {
        if (e.t < 0.6f || !(in.confirm || in.back)) return;
        if (run.contest && e.caught && e.msg.find("keep the new one?") != std::string::npos) {
            // the contest: one bug at a time - Enter keeps the new one, Esc the old
            if (in.confirm) {
                run.bug = e.species; run.bugLevel = e.level; run.bugShiny = e.shiny; run.bugNature = e.nature;
                for (int k = 0; k < 6; k++) run.bugIv[k] = e.iv[k];
            }
        }
        g.screen = Screen::World;
        if (run.balls <= 0) SafariEnded(g);
        return;
    }
    auto opts = Options(g);
    int n = (int)opts.size();
    if (in.up || in.left) { e.sel = (e.sel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.down || in.right) { e.sel = (e.sel + 1) % n; PlaySfx(Sfx::MenuMove); }
    if (in.back) e.sel = n - 1;
    if (!in.confirm || e.t < 0.35f) return;
    std::string what = opts[std::min(e.sel, n - 1)];
    e.t = 0;
    if (what == "Run") { Fled(g, "Got away safely!"); return; }
    if (what == "Bait") { e.eating = GetRandomValue(1, 5); e.angry = 0; e.catchMul = 0.5f; e.msg = "You threw some bait! The wild " + sp.name + " is eating!"; PlaySfx(Sfx::MenuOk); MaybeFlee(g); return; }
    if (what == "Rock") { e.angry = GetRandomValue(1, 5); e.eating = 0; e.catchMul = 2.0f; e.msg = "You threw a rock! The wild " + sp.name + " is angry!"; PlaySfx(Sfx::Thud); MaybeFlee(g); return; }
    // a ball
    if (run.balls <= 0) { e.phase = 2; e.msg = "You're out of balls!"; return; }
    run.balls--;
    PlaySfx(Sfx::BallThrow);
    float mul = e.angry > 0 ? 2.0f : e.eating > 0 ? 0.5f : e.catchMul;
    float a = CatchRate(sp) * 1.5f / 3 * mul * (run.contest ? 3.0f : 1.0f);   // (the Contest: you'd have weakened it in battle first)
    e.shakes = 0;
    if (a >= 255) e.shakes = 4;
    else {
        float b = 1048560.0f / std::sqrt(std::sqrt(16711680.0f / std::max(1.0f, a)));
        while (e.shakes < 4 && GetRandomValue(0, 65535) < (int)b) e.shakes++;
    }
    e.phase = 1;
    e.msg = std::string(run.contest ? "You threw a Park Ball!" : "You threw a Safari Ball!");
}

void DrawSafari(const Game& g) {
    const auto& e = g.safariEnc;
    const auto& run = g.ow.safari;
    const SpeciesDef& sp = AllSpecies()[e.species];
    // a grassy field, the Pokemon in it
    DrawRectangleGradientV(0, 0, VW, VH / 2, Color{150, 200, 240, 255}, Color{200, 230, 250, 255});
    DrawRectangleGradientV(0, VH / 2, VW, VH / 2, Color{110, 180, 90, 255}, Color{70, 140, 60, 255});
    DrawEllipse(VW * 2 / 3, 190, 90, 18, Fade(Color{40, 90, 40, 255}, 0.6f));
    if (!(e.phase == 1) && !(e.phase == 2 && e.caught)) {
        int bob = e.eating > 0 ? (int)(std::sin(g.t * 8) * 3) : e.angry > 0 ? (int)(std::sin(g.t * 25) * 2) : 0;
        DrawSpeciesFit(e.species | (e.shiny ? SHINY_FLAG : 0), VIEW_FRONT, VW * 2 / 3, 190 + bob, 130);
    } else {   // the ball, shaking
        float wob = e.phase == 1 ? std::sin(e.t * 14) * 8 * (e.t > 0.5f ? 1 : 0) : 0;
        Vector2 c{VW * 2 / 3.0f, 175};
        DrawCircleV(c, 12, Color{40, 40, 40, 255});
        DrawCircleSector(c, 11, 180 + wob, 360 + wob, 24, run.contest ? Color{230, 170, 40, 255} : Color{110, 160, 60, 255});
        DrawCircleSector(c, 11, 0 + wob, 180 + wob, 24, RAYWHITE);
        DrawCircleV(c, 3.5f, RAYWHITE);
        DrawCircleLines((int)c.x, (int)c.y, 3.5f, Color{40, 40, 40, 255});
    }
    // its name, the run's counters
    DrawPanel(20, 20, 240, 44);
    GDrawText(TextFormat("%s%s  Lv%d", sp.name.c_str(), e.shiny ? " *" : "", e.level), 30, 28, 10, UI_TEXT);
    GDrawText(e.eating > 0 ? "Eating..." : e.angry > 0 ? "Angry!" : (run.contest ? "Bug-Catching Contest" : "Safari Zone"), 30, 44, 10,
              e.angry > 0 ? Color{255, 120, 100, 255} : UI_DIM);
    DrawPanel(VW - 200, 20, 180, 44);
    GDrawText(TextFormat("%s: %d", run.contest ? "Park Balls" : "Safari Balls", run.balls), VW - 190, 28, 10, UI_TEXT);
    GDrawText(TextFormat("Steps left: %d", run.steps), VW - 190, 44, 10, UI_DIM);
    if (run.contest && run.bug >= 0) GDrawText(TextFormat("Keeping: %s Lv%d", AllSpecies()[run.bug].name.c_str(), run.bugLevel), VW - 190, 70, 10, UI_GOLD);
    // the message and the choices
    DrawPanel(16, VH - 92, VW - 32, 76, Color{250, 250, 245, 255}, UI_INK);
    DrawWrapped(e.msg.c_str(), 28, VH - 82, VW - 230, 10, Color{40, 40, 50, 255}, 13);
    if (e.phase == 0) {
        std::vector<const char*> opts = g.ow.safari.contest ? std::vector<const char*>{"Park Ball", "Run"} : std::vector<const char*>{"Safari Ball", "Bait", "Rock", "Run"};
        for (int k = 0; k < (int)opts.size(); k++) {
            int x = VW - 200 + (k % 2) * 90, y = VH - 80 + (k / 2) * 24;
            if (k == e.sel) DrawRectangle(x - 6, y - 4, 86, 20, Color{255, 220, 120, 255});
            GDrawText(opts[k], x, y, 10, Color{40, 40, 50, 255});
        }
    } else if (e.phase == 2) {
        bool keep = e.msg.find("keep the new one?") != std::string::npos;
        GDrawText(keep ? "Enter keep the new one   Esc keep the old one" : "Enter continue", VW - 330, VH - 30, 10, UI_DIM);
    }
}
