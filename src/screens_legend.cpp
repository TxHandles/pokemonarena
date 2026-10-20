// Legendary Pokemon: the list of encounters (where they live, what unlocks them, who caught
// them), their intros, and the legendary catch: 3 balls alone, 6 in co-op taking turns (the MVP
// of the battle throws first). Whoever catches it keeps it, and the encounter closes for good.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "legend.h"
#include "moves.h"
#include "sprites.h"
#include "storymap.h"
#include "ui.h"
#include "vec.h"

namespace {
const Color kP1{240, 200, 80, 255};
const Color kP2{120, 230, 255, 255};
Color WhoCol(int p) { return p == 1 ? kP2 : kP1; }

void Ball(float x, float y, float r, float rot, Color top) {
    Vector2 c{x, y};
    DrawCircleV(c, r + 1, UI_INK);
    DrawCircleSector(c, r, -180 + rot, 0 + rot, 16, top);
    DrawCircleSector(c, r, 0 + rot, 180 + rot, 16, Color{240, 240, 240, 255});
    Vector2 d{std::cos(rot * DEG2RAD), std::sin(rot * DEG2RAD)};
    DrawLineEx(c - d * r, c + d * r, std::max(1.0f, r * 0.25f), UI_INK);
    DrawCircleV(c, r * 0.35f, UI_INK);
    DrawCircleV(c, r * 0.22f, WHITE);
}
Color BallCol(const std::string& n) {
    if (n == "Master Ball") return {150, 80, 200, 255};
    if (n == "Ultra Ball") return {50, 50, 56, 255};
    if (n == "Great Ball") return {70, 120, 230, 255};
    return {230, 60, 60, 255};
}
std::vector<const ItemDef*> Balls(const Profile& p) {
    std::vector<const ItemDef*> v;
    for (const auto& d : ItemList())
        if (IsBall(d) && BagCount(p, d.name) > 0) v.push_back(&d);
    return v;
}
float WithBall(float base, const ItemDef* ball) {
    if (!ball) return base;
    if (ball->value <= 0) return 1.0f;   // Master Ball
    return std::min(0.98f, 1.0f - std::pow(1.0f - base, ball->value));
}
int PartyAvg(const Game& g) {
    int sum = 0, n = 0;
    for (int p = 0; p < (g.coop ? 2 : 1); p++)
        for (const auto& m : Prof(g, p).mons) if (!m.boxed) { sum += m.level; n++; }
    return n ? sum / n : 5;
}
int SpeciesOf(int id) { return SpeciesIndex(Legend(id).species); }
int LegendLevel(int id, int avg) { return std::max(1, std::min(MAX_LEVEL, std::max(Legend(id).minLevel, avg + Legend(id).bonus))); }
int HubLegendItem(const Game&) { return 2; }   // the legendaries are a tab on the hub's Map
} // namespace

// ------------------------------------------------------------------ state

int LegendCaughtBy(const Game& g, int id) {
    if (id < 0 || id >= LEGEND_COUNT) return -1;
    if (g.coop) {
        if (!(g.coopSave.legends >> id & 1)) return -1;
        return std::max(0, g.coopSave.legendWho[id]);
    }
    return (g.profile.legends >> id & 1) ? 0 : -1;
}

bool LegendOpen(const Game& g, int id, std::string* why) {
    const LegendDef& L = Legend(id);
    int by = LegendCaughtBy(g, id);
    if (by >= 0) {
        if (why) *why = g.coop ? Prof(g, by).name + " caught it - it's gone from " + L.place + "." : std::string("You caught it - it's gone from ") + L.place + ".";
        return false;
    }
    switch (L.unlock) {
        case LegendUnlock::Badges8:
            if (BadgeCount(g) < 8) { if (why) *why = "Collect all 8 badges first."; return false; }
            break;
        case LegendUnlock::Champion:
            if (!KantoDone(g)) { if (why) *why = "Only the Pokemon League Champion may enter."; return false; }
            break;
        case LegendUnlock::AfterMewtwo:
            if (!KantoDone(g) || LegendCaughtBy(g, 3) < 0) { if (why) *why = "You need the Old Sea Map. It's said to turn up once Mewtwo is caught..."; return false; }
            break;
        case LegendUnlock::AfterStage: {
            int at = -1;
            for (int i = 0; i < (int)Story().stages.size(); i++)
                if (Story().stages[i].region == L.region && Story().stages[i].name == L.after) { at = i; break; }
            if (at < 0 || CurStage(g) <= at) { if (why) *why = std::string(L.hint) + "."; return false; }
        } break;
    }
    if (SpeciesOf(id) < 0) { if (why) *why = "This Pokemon is missing from the Pokemon folder."; return false; }
    return true;
}

CombatantSpec LegendBoss(int id, int avgLevel, int brought, bool coop) {
    const LegendDef& L = Legend(id);
    CombatantSpec c;
    c.species = std::max(0, SpeciesOf(id));
    c.level = LegendLevel(id, avgLevel);
    c.moves = MovesAtLevel(AllSpecies()[c.species], c.level);
    // Two players hit it at once (2 on 1), so it gets more HP per Pokemon in co-op.
    c.shiny = RollShiny();
    c.boss = 1.0f + L.hpPer * (coop ? 1.25f : 0.7f) * std::max(1, brought);
#ifdef ARENA_AUTOTEST
    if (std::getenv("ARENA_LEG_EASY")) { c.boss = 1.0f; c.level = 5; c.hpFrac = 0.27f; c.moves = MovesAtLevel(AllSpecies()[c.species], c.level); }
#endif
    return c;
}

StageDef LegendStage(int id) {
    const LegendDef& L = Legend(id);
    StageDef st;
    st.name = L.place;
    st.trainer = std::string("Legendary ") + L.species;
    st.say = L.cry;
    st.team = {L.species};
    st.cpu = AILevel::Hard;
    st.arena = L.arena;
    st.coins = 400;
    st.catchMode = 1;
    st.reward = false;
    st.legend = id + 1;
    return st;
}

void BeginLegend(Game& g, int id) {
    g.legend = id;
    g.tower = false;
    g.stageIdx = -1;
    g.mode = g.coop ? Mode::CoopStory : Mode::Story;
    g.rareBattle = false;
    g.screen = Screen::StageIntro;
}

void OpenLegends(Game& g) {
    g.legendSel = std::max(0, std::min(LEGEND_COUNT - 1, g.legendSel));
    g.screen = Screen::Legends;
}

// ------------------------------------------------------------------ the list

namespace {
// The legendaries listed: the ones of the selected one's region (Kanto or Johto).
std::vector<int> RegionLegends(int region) {
    std::vector<int> v;
    for (int i = 0; i < LEGEND_COUNT; i++) if (Legend(i).region == region) v.push_back(i);
    return v;
}
} // namespace

void UpdateLegends(Game& g) {
    PlayerInput in = MenuInput();
    auto list = RegionLegends(Legend(g.legendSel).region);
    int at = (int)(std::find(list.begin(), list.end(), g.legendSel) - list.begin());
    int ln = (int)list.size();
    if (in.up) { g.legendSel = list[(at + ln - 1) % ln]; PlaySfx(Sfx::MenuMove); }
    if (in.down) { g.legendSel = list[(at + 1) % ln]; PlaySfx(Sfx::MenuMove); }
    if (in.back) { PlaySfx(Sfx::MenuBack); EnterStoryHub(g); g.hubSel = HubLegendItem(g); return; }
    if (in.sort) { MapTabSwitch(g, false); return; }
    if (!in.confirm) return;
    int id = g.legendSel;
    std::string why;
    if (!LegendOpen(g, id, &why)) { PlaySfx(Sfx::MenuBack); Toast(g, why); return; }
    PlaySfx(Sfx::MenuOk);
    if (g.coop && g.on.active) {   // online: both pick the same encounter
        if (OnlineWaiting(g)) return;
        if (HealthyCount(Prof(g, g.on.me)) == 0) { Toast(g, "Your party has all fainted! Visit the Healing Center first."); return; }
        OnlineSetReady(g, 200 + id);
        EnterStoryHub(g);
        g.hubSel = HubLegendItem(g);
        const std::string& pn = g.coopP[1 - g.on.me].name;
        Toast(g, g.on.partnerReady == 200 + id ? std::string("Off to find ") + Legend(id).species + "!"
                                               : "Waiting for " + pn + " to pick " + Legend(id).species + " on the Map's Legendary tab too.");
        return;
    }
    for (int p = 0; p < (g.coop ? 2 : 1); p++)
        if (HealthyCount(Prof(g, p)) == 0) {
            Toast(g, (g.coop ? Prof(g, p).name + "'s party has" : std::string("Your whole team has")) + " fainted! Visit the Healing Center first.");
            return;
        }
    BeginLegend(g, id);
}

void DrawLegends(const Game& g) {
    DrawMenuBackdrop(Color{16, 14, 28, 255}, (float)GetTime());
    Profile pv = ProgressView(g);
    const int mx = 8, my = 26, mw = 400, mh = 300;
    int region = Legend(g.legendSel).region;
    auto list = RegionLegends(region);
    DrawStoryMap(pv, mx, my, mw, mh, g.t, false, -1, region);
    DrawMapTabs(g, true);
    for (int i : list) {   // where they live
        const LegendDef& L = Legend(i);
        Vector2 c = StoryMapPoint(mx, my, mw, mh, L.mapX, L.mapY);
        bool open = LegendOpen(g, i), caught = LegendCaughtBy(g, i) >= 0;
        Color col = caught ? Color{120, 230, 130, 255} : open ? GymColor(L.arena) : Color{80, 74, 96, 255};
        float pulse = open ? 1.0f + 0.25f * std::sin(g.t * 4 + i) : 1.0f;
        DrawPoly(c, 5, 6.5f * pulse, -90, UI_INK);
        DrawPoly(c, 5, 5.0f * pulse, -90, col);
        if (i == g.legendSel) EllipsePix(c.x, c.y, 10 + std::sin(g.t * 6) * 1.5f, 10 + std::sin(g.t * 6) * 1.5f, UI_GOLD);
    }
    int x = 416, w = 216;
    int avg = PartyAvg(g);
    int rowH = std::min(44, 222 / std::max(1, (int)list.size()));
    for (int row = 0; row < (int)list.size(); row++) {
        int i = list[row];
        const LegendDef& L = Legend(i);
        int y = 6 + row * rowH;
        bool sel = g.legendSel == i;
        std::string why;
        bool open = LegendOpen(g, i, &why);
        int by = LegendCaughtBy(g, i);
        bool known = open || by >= 0 || L.unlock == LegendUnlock::Badges8 || (L.unlock == LegendUnlock::Champion && BadgeCount(g) >= 8) || KantoDone(g);
        if (L.region >= 1) known = open || by >= 0 || StoryDone(g) || L.unlock != LegendUnlock::AfterStage || std::string(L.after) != "Champion's Room";
        int ph = rowH - 2;
        DrawPanel(x, y, w, ph, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : (open ? GymColor(L.arena) : Color{70, 64, 92, 255}));
        int sp = SpeciesOf(i);
        if (sp >= 0) DrawSpeciesFit(sp, VIEW_FRONT, x + 22, y + ph - 3, ph - 6, by >= 0 || open ? WHITE : (known ? Color{20, 18, 30, 255} : Color{10, 10, 16, 255}));
        int ty = ph < 40 ? 2 : 4, step = ph < 40 ? 11 : 12;
        GDrawText(known ? L.species : "???", x + 44, y + ty, 10, sel ? UI_GOLD : UI_TEXT);
        GDrawText(known ? L.place : (L.region >= 1 ? "Somewhere in Johto" : "Somewhere in Kanto"), x + 44, y + ty + step, 10, UI_DIM);
        std::string status;
        Color sc = UI_DIM;
        if (by >= 0) { status = g.coop ? "Caught by " + Prof(g, by).name : std::string("Caught!"); sc = Color{120, 230, 130, 255}; }
        else if (open) { status = TextFormat("Waiting - about Lv%d", LegendLevel(i, avg)); sc = GymColor(L.arena); }
        else status = L.unlock == LegendUnlock::Badges8 ? "Needs all 8 badges" : L.unlock == LegendUnlock::Champion ? "Champion only"
                    : L.unlock == LegendUnlock::AfterStage ? L.hint : "Needs the Old Sea Map";
        GDrawText(status.c_str(), x + 44, y + ty + step * 2, 10, sc);
        if (g.on.active && g.on.partnerReady == 200 + i) GDrawText("<", x + w - 12, y + 4, 10, kP2);
    }
    // The selected one: its story and the rules
    const LegendDef& L = Legend(g.legendSel);
    std::string why;
    bool open = LegendOpen(g, g.legendSel, &why);
    int by = LegendCaughtBy(g, g.legendSel);
    bool known = open || by >= 0 || (L.unlock != LegendUnlock::AfterMewtwo && std::string(L.after) != "Champion's Room") || StoryDone(g);
    DrawPanel(x, 228, w, 104);
    DrawWrapped(known ? L.lore : "Old stories tell of a Pokemon far older than any other...", x + 6, 233, w - 12, 10, UI_TEXT);
    if (!open && by < 0) DrawWrapped(why.c_str(), x + 6, 306, w - 12, 10, Color{255, 170, 120, 255});
    GDrawText(g.coop ? "Win 2v1, then 6 balls - taking turns, the MVP first. One of you keeps it."
                    : "Weaken it, then you get 3 balls. If it breaks free, it flees - come back and try again.",
             10, 330, 10, UI_DIM);
    if (g.on.active && g.on.partnerReady >= 200 && g.on.partnerReady < 200 + LEGEND_COUNT)
        GDrawText(TextFormat("%s wants to go after %s - pick it too!", g.coopP[1 - g.on.me].name.c_str(), Legend(g.on.partnerReady - 200).species), 10, 316, 10, kP2);
    {
        const char* help = "Up/Down choose   Enter: find it   Esc: back";
        GDrawText(help, x + w - TextW(help, 10), 346, 10, UI_DIM);
    }
    DrawToast(g);
}

// ------------------------------------------------------------------ intro

void DrawLegendIntro(const Game& g) {
    int id = std::max(0, g.legend);
    const LegendDef& L = Legend(id);
    Color tc = GymColor(L.arena);
    float t = g.t;
    for (int y = 0; y < VH; y++) {
        float u = y / (float)VH;
        DrawRectangle(0, y, VW, 1, ColorBrightness(Color{(unsigned char)(tc.r / 3), (unsigned char)(tc.g / 3), (unsigned char)(tc.b / 3), 255}, -0.6f + u * 0.4f));
    }
    BeginBlendMode(BLEND_ADDITIVE);   // the legendary's aura
    for (int k = 0; k < 12; k++) {
        float a = k * 0.5236f + t * 0.25f;
        Vector2 c{VW / 2.0f, 170};
        Vector2 p1{c.x + std::cos(a - 0.06f) * 400, c.y + std::sin(a - 0.06f) * 400}, p2{c.x + std::cos(a + 0.06f) * 400, c.y + std::sin(a + 0.06f) * 400};
        DrawTriangle(c, p2, p1, Fade(tc, 0.07f));
        DrawTriangle(c, p1, p2, Fade(tc, 0.07f));
    }
    DrawCircle(VW / 2, 170, 70 + 6 * std::sin(t * 2), Fade(tc, 0.18f));
    EndBlendMode();
    for (int i = 0; i < 60; i++) {   // drifting motes: snow, sparks, embers, psychic dust, petals
        float x = std::fmod(i * 97.3f + std::sin(t + i) * 20, (float)VW);
        float y = std::fmod(i * 53.1f + t * (18 + i % 5 * 6), (float)VH);
        DrawRectangle((int)x, (int)y, 2, 2, Fade(ColorBrightness(tc, 0.4f), 0.6f));
    }
    int sp = SpeciesOf(id);
    float bob = std::sin(t * 1.8f) * 4;
    if (sp >= 0) DrawSpeciesFit(sp, VIEW_FRONT, VW / 2, (int)(222 + bob), 120);
    DrawRectangle(0, 8, VW, 46, Fade(UI_INK, 0.7f));
    DrawTextCentered("LEGENDARY ENCOUNTER", VW / 2, 12, 10, Fade(WHITE, 0.9f), false);
    DrawTextCentered(L.species, VW / 2, 24, 30, tc);
    DrawTextCentered(std::string(L.place) == ArenaName(L.arena) ? L.place : TextFormat("%s   -   %s", L.place, ArenaName(L.arena)), VW / 2, 236, 10, UI_TEXT, false);
    DrawPanel(60, 252, 520, 20, UI_PANEL, tc);
    DrawTextCentered(TextFormat("\"%s\"", L.cry), VW / 2, 257, 10, UI_TEXT, false);
    int avg = PartyAvg(g);
    DrawRectangle(30, 276, VW - 60, 56, Fade(UI_INK, 0.85f));
    DrawTextCentered(TextFormat("About Lv%d   -   a boss: huge HP, CPU skill HARD%s", LegendLevel(id, avg), g.coop ? "   -   2 vs 1" : ""), VW / 2, 280, 10, UI_TEXT, false);
    DrawWrapped(ArenaTwist(L.arena), 44, 293, VW - 88, 10, Color{150, 200, 240, 255});
    DrawTextCentered(g.coop ? "Weaken it, then 6 balls taking turns (the MVP first). It can't be knocked out."
                            : "Weaken it (it can't be knocked out), then you get 3 balls to catch it.", VW / 2, 318, 10, UI_GOLD, false);
    DrawRectangle(VW / 2 - 130, 336, 260, 16, Fade(UI_INK, 0.85f));
    DrawTextCentered("Enter: choose your team     Esc: back", VW / 2, 339, 10, UI_DIM, false);
}

// ------------------------------------------------------------------ after the battle: the catch

void LegendBattleOver(Game& g, const Battle& b, const float damage[2]) {
    if (g.legend < 0 || LegendCaughtBy(g, g.legend) >= 0) return;
    auto foes = b.TeamFighters(1);
    if (foes.empty()) return;
    const Fighter& f = *foes[0];
    Game::LegendCatch c;
    c.id = g.legend;
    c.species = f.RealSpecies();
    c.level = f.level;
    if (f.transformFrom >= 0) for (int i = 0; i < f.origN; i++) c.moves.push_back(f.origMoves[i].name);
    else for (int i = 0; i < f.nMoves; i++) c.moves.push_back(f.moves[i].name);
    c.abSlot = f.abSlot;
    c.nature = f.nature;
    c.shiny = f.shiny;
    c.hpFrac = std::max(0.01f, std::min(1.0f, f.hp / (float)std::max(1, f.maxHp)));
    float base = 0.08f + 0.30f * (1.0f - std::min(1.0f, c.hpFrac / 0.25f));
    c.chance = std::max(0.03f, base * Legend(c.id).catchMul);
    c.players = g.coop ? 2 : 1;
    int mvp = g.coop && damage[1] > damage[0] ? 1 : 0;
    c.order[0] = mvp;
    c.order[1] = 1 - mvp;
    c.maxThrows = g.coop ? LEGEND_THROWS_COOP : LEGEND_THROWS_SOLO;
    g.lc = c;
    g.legendCatchPending = true;
    g.on.partnerThrows.clear();
    if (g.coop) g.resultLines.push_back(TextFormat("%s dealt the most damage (%d vs %d) and throws first!", Prof(g, mvp).name.c_str(), (int)damage[mvp], (int)damage[1 - mvp]));
    g.resultLines.push_back(std::string(Legend(c.id).species) + " is weakened - time to catch it!");
}

namespace {
const float kResultAt0 = 1.3f, kShake = 0.6f;
float ResultAt(const Game::LegendCatch& c) { return c.passed ? 0.0f : kResultAt0 + c.shakes * kShake; }

void Caught(Game& g) {
    auto& c = g.lc;
    int who = c.Thrower();
    Profile& P = Prof(g, who);
    OwnedMon m = NewMon(c.species, c.level);
    if (!c.moves.empty()) m.moves = c.moves;
    m.abilitySlot = c.abSlot;
    m.nature = c.nature;
    m.shiny = c.shiny;
    m.hp = c.hpFrac;
    m.caught = true;
    bool mine = OnlineMine(g, who);   // online: the partner adds it on their computer
    if (mine) {
        c.boxed = AddMon(P, m);
        DexSee(P, AllSpecies()[c.species].name);
    }
    c.caughtBy = who;
    if (g.coop) {
        g.coopSave.legends |= 1 << c.id;
        g.coopSave.legendWho[c.id] = who;
        if (mine) g.coopSave.run[who & 1].caught++;
        SaveWho(g, who);
        if (g.on.active) OnlineSendCoop(g);
        else SaveCoop(g.coopSave);
    } else {
        g.profile.legends |= 1 << c.id;
        SaveWho(g, 0);
    }
    PlaySfx(Sfx::LevelUp);
}
} // namespace

void UpdateLegendCatch(Game& g, float dt) {
    auto& c = g.lc;
    if (c.id < 0) { AdvanceStoryQueues(g); return; }
    PlayerInput in = MenuInput();
    int who = c.Thrower();
    bool mine = !g.on.active || who == g.on.me;
    Profile& P = Prof(g, who);
    switch (c.phase) {
        case 0: {
            if (!mine) {   // the partner's throw: it arrives from their computer
                if (!g.on.partnerThrows.empty()) {
                    OnlineSession::Throw th = g.on.partnerThrows.front();
                    g.on.partnerThrows.pop_front();
                    c.passed = th.shakes == 9;
                    c.ok = !c.passed && th.ok;
                    c.shakes = c.passed ? 0 : th.shakes;
                    c.phase = c.passed ? 2 : 1;
                    c.t = 0;
                    if (!c.passed) PlaySfx(Sfx::Shoot);
                    else if (c.ok) Caught(g);
                } else if (g.on.reconnecting) {   // they dropped out: their turn passes
                    c.passed = true;
                    c.ok = false;
                    c.phase = 2;
                    c.t = 0;
                }
                return;
            }
            auto balls = Balls(P);
            if (balls.empty()) {   // no balls: this turn passes
                if (in.confirm) {
                    c.passed = true;
                    c.ok = false;
                    c.shakes = 0;
                    if (g.on.active) OnlineSendThrow(c.species, c.level, false, 9, false);
                    c.phase = 2;
                    c.t = 0;
                }
                return;
            }
            c.ballSel = std::min(c.ballSel, (int)balls.size() - 1);
            if (in.up || in.left) { c.ballSel = (c.ballSel + (int)balls.size() - 1) % (int)balls.size(); PlaySfx(Sfx::MenuMove); }
            if (in.down || in.right) { c.ballSel = (c.ballSel + 1) % (int)balls.size(); PlaySfx(Sfx::MenuMove); }
            if (!in.confirm) return;
            const ItemDef* ball = balls[c.ballSel];
            BagTake(P, ball->name);
            SaveWho(g, who);
            float ch = WithBall(c.chance, ball);
            c.passed = false;
            c.ok = GetRandomValue(0, 9999) < (int)(ch * 10000);
            c.shakes = c.ok ? 3 : GetRandomValue(0, 2);
            if (g.on.active) OnlineSendThrow(c.species, c.level, c.ok, c.shakes, false);
            c.phase = 1;
            c.t = 0;
            PlaySfx(Sfx::Shoot);
        } break;
        case 1: {
            float before = c.t;
            c.t += dt;
            for (int k = 0; k < c.shakes; k++) {
                float at = kResultAt0 + k * kShake;
                if (before < at && c.t >= at) PlaySfx(Sfx::Thud);
            }
            if (c.t >= ResultAt(c)) {
                c.phase = 2;
                if (c.ok) Caught(g);
                else PlaySfx(Sfx::Faint);
            }
        } break;
        case 2: {
            c.t += dt;
            if (c.ok) {
                if (c.t > 0.8f && (in.confirm || in.back)) { c.phase = 3; c.t = 0; PlaySfx(Sfx::MenuOk); }
                break;
            }
            // Missed: the next ball (this player confirms; a partner's miss moves on by itself).
            bool go = c.t > 0.6f && (in.confirm || (!mine && c.t > 2.2f) || (c.passed && c.t > 1.6f && !mine));
            if (!go) break;
            c.throwNo++;
            c.t = 0;
            c.passed = false;
            c.shakes = 0;
            if (c.throwNo >= c.maxThrows) { c.phase = 3; PlaySfx(Sfx::Faint); }
            else { c.phase = 0; PlaySfx(Sfx::MenuMove); }
        } break;
        default:
            c.t += dt;
            if (c.t > 0.8f && (in.confirm || in.back)) {
                PlaySfx(Sfx::MenuOk);
                c.id = -1;
                AdvanceStoryQueues(g);
            }
            break;
    }
}

void DrawLegendCatch(const Game& g) {
    const auto& c = g.lc;
    if (c.id < 0) return;
    const LegendDef& L = Legend(c.id);
    Color tc = GymColor(L.arena);
    for (int y = 0; y < VH; y++) DrawRectangle(0, y, VW, 1, ColorBrightness(Color{(unsigned char)(tc.r / 4), (unsigned char)(tc.g / 4), (unsigned char)(tc.b / 4), 255}, -0.5f + y / (float)VH * 0.3f));
    int who = c.Thrower();
    bool mine = !g.on.active || who == g.on.me;
    const Profile& P = Prof(g, who);
    const SpeciesDef& sp = AllSpecies()[c.species];
    DrawTextCentered(TextFormat("CATCH %s!", sp.name.c_str()), VW / 2, 8, 20, tc);
    if (c.shiny) {
        int tw = TextW(TextFormat("CATCH %s!", sp.name.c_str()), 20);
        DrawShinyStar(VW / 2 + tw / 2.0f + 10, 17, 5);
        GDrawText("Shiny!", VW / 2 + tw / 2 + 18, 12, 10, Color{255, 214, 80, 255});
    }
    // the balls: one pip per throw, in the thrower's colour
    int pips = c.maxThrows;
    for (int i = 0; i < pips; i++) {
        int px = VW / 2 - (pips - 1) * 11 + i * 22, py = 40;
        int pw = c.order[i % c.players];
        bool used = i < c.throwNo || (i == c.throwNo && c.phase >= 1);
        if (i == c.throwNo && c.phase < 3) EllipsePix((float)px, (float)py, 9, 9, UI_GOLD);
        Ball((float)px, (float)py, 6, 0, used ? Color{70, 70, 80, 255} : (c.players > 1 ? WhoCol(pw) : Color{230, 60, 60, 255}));
    }
    if (c.phase < 3) {
        std::string turn = c.players > 1 ? TextFormat("Ball %d of %d  -  %s's throw%s", c.throwNo + 1, c.maxThrows, P.name.c_str(), c.throwNo == 0 ? " (MVP)" : "")
                                         : TextFormat("Ball %d of %d", c.throwNo + 1, c.maxThrows);
        DrawTextCentered(turn.c_str(), VW / 2, 54, 10, c.players > 1 ? WhoCol(who) : UI_TEXT, false);
    }
    // the legendary, and the ball flying at it
    float t = c.t;
    Vector2 mon{VW / 2.0f, 190};
    bool inBall = (c.phase == 1 && t >= 0.9f) || (c.phase == 2 && c.ok && !c.passed) || (c.phase == 3 && c.caughtBy >= 0);
    bool fled = c.phase == 3 && c.caughtBy < 0;
    if (!inBall && !fled) {
        float flash = (c.phase == 1 && t > 0.7f && t < 0.9f) ? (t - 0.7f) / 0.2f : 0.0f;
        BeginBlendMode(BLEND_ADDITIVE);
        DrawCircle((int)mon.x, (int)mon.y - 30, 56 + 4 * std::sin(g.t * 3), Fade(tc, 0.15f));
        EndBlendMode();
        DrawSpeciesFit(ShinyIdx(c.species, c.shiny), VIEW_FRONT, (int)mon.x, (int)mon.y + 20, 110);
        if (flash > 0) DrawRectangle((int)mon.x - 40, (int)mon.y - 90, 80, 110, Fade(WHITE, flash * 0.5f));
    }
    if (c.phase == 1) {
        if (t < 0.7f) {
            float u = t / 0.7f;
            Vector2 p = LerpV(Vector2{120, 330}, mon, u);
            p.y -= std::sin(u * PI) * 90;
            Ball(p.x, p.y, 6, u * 720, Color{230, 60, 60, 255});
        } else {
            float rot = 0;
            if (t > 1.1f) { float k = std::fmod(t - 1.1f, kShake); if (k < 0.3f) rot = std::sin(k / 0.3f * PI * 2) * 25; }
            Ball(mon.x, mon.y + 18, 6, rot, Color{230, 60, 60, 255});
        }
        int shown = t > 1.1f ? std::min(c.shakes, (int)((t - 1.1f) / kShake) + 1) : 0;
        DrawTextCentered(TextFormat("%s threw a ball%s", mine && !g.coop ? "You" : P.name.c_str(), std::string(shown, '.').c_str()), VW / 2, 252, 20, UI_TEXT);
    } else if (c.phase == 2) {
        if (c.ok) {
            Ball(mon.x, mon.y + 18, 6, 0, Color{230, 60, 60, 255});
            for (int i = 0; i < 3; i++) DrawRectangle((int)mon.x - 14 + i * 12, (int)mon.y + 2 - (int)(std::fmod(g.t * 20 + i * 5, 8)), 2, 2, UI_GOLD);
        } else if (c.passed) {
            DrawTextCentered(TextFormat("%s has no Poke Balls - the turn passes.", P.name.c_str()), VW / 2, 252, 10, Color{255, 170, 120, 255}, false);
        } else {
            const char* lines[3] = {"Oh no! It broke free!", "Aww! It appeared to be caught!", "Argh! Almost had it!"};
            DrawTextCentered(lines[std::min(2, c.shakes)], VW / 2, 252, 20, Color{255, 130, 100, 255});
        }
        if (c.ok) DrawTextCentered(TextFormat("Gotcha! %s was caught!", sp.name.c_str()), VW / 2, 252, 20, UI_GOLD);
        else if (c.throwNo + 1 >= c.maxThrows) DrawTextCentered("That was the last ball...", VW / 2, 280, 10, UI_TEXT, false);
        else if (mine || !g.on.active) DrawTextCentered("Enter: next ball", VW / 2, 280, 10, UI_GOLD, false);
    } else if (c.phase == 3) {
        if (c.caughtBy >= 0) {
            Ball(mon.x, mon.y + 18, 7, 0, Color{230, 60, 60, 255});
            DrawTextCentered(TextFormat("%s caught %s!", g.coop ? Prof(g, c.caughtBy).name.c_str() : "You", sp.name.c_str()), VW / 2, 236, 20, UI_GOLD);
            DrawTextCentered(TextFormat("It joins %s at level %d%s. %s won't be seen at %s again.", g.coop ? (Prof(g, c.caughtBy).name + "'s team").c_str() : "your team",
                                        c.level, c.boxed ? " (in the Box)" : "", sp.name.c_str(), L.place), VW / 2, 264, 10, UI_TEXT, false);
        } else {
            DrawTextCentered(TextFormat("%s broke free and fled!", sp.name.c_str()), VW / 2, 236, 20, Color{255, 130, 100, 255});
            DrawTextCentered(TextFormat("It's still out there at %s - try again from the Map's Legendary tab.", L.place), VW / 2, 264, 10, UI_TEXT, false);
        }
        DrawTextCentered("Enter to continue", VW / 2, 330, 10, UI_DIM, false);
        return;
    }
    if (c.phase != 0) return;
    if (!mine) {
        DrawTextCentered(TextFormat("%s is choosing a ball...", P.name.c_str()), VW / 2, 262, 10, WhoCol(who), false);
        return;
    }
    auto balls = Balls(P);
    DrawPanel(150, 244, 340, 70);
    if (balls.empty()) {
        DrawTextCentered(TextFormat("%s has no Poke Balls!", g.coop ? P.name.c_str() : "You"), VW / 2, 262, 10, Color{255, 130, 100, 255}, false);
        DrawTextCentered("Enter: pass this ball", VW / 2, 280, 10, UI_GOLD, false);
        return;
    }
    const ItemDef* ball = balls[std::min(c.ballSel, (int)balls.size() - 1)];
    GDrawText(TextFormat("%s's ball:", P.name.c_str()), 160, 250, 10, c.players > 1 ? WhoCol(who) : UI_DIM);
    float ch = WithBall(c.chance, ball);
    GDrawText(TextFormat("%d%% chance", (int)std::round(ch * 100)), 400, 250, 10, ch >= 0.5f ? Color{120, 230, 130, 255} : (ch >= 0.25f ? UI_GOLD : Color{255, 130, 100, 255}));
    for (int i = 0; i < (int)balls.size(); i++) {
        int bx = 170 + i * 80, by = 284;
        bool sel = ball == balls[i];
        if (sel) RectLines(bx - 12, by - 14, 76, 30, UI_GOLD);
        Ball((float)bx, (float)by, 6, 0, BallCol(balls[i]->name));
        GDrawText(TextFormat("x%d", BagCount(P, balls[i]->name)), bx + 10, by - 10, 10, UI_TEXT);
        std::string nm = balls[i]->name;
        GDrawText(nm.substr(0, nm.find(' ')).c_str(), bx + 10, by + 1, 10, sel ? UI_GOLD : UI_DIM);
    }
    DrawTextCentered("Left/Right or Up/Down: ball     Enter: throw", VW / 2, 340, 10, UI_DIM, false);
}
