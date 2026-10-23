// Pokemon Contests, as in Emerald (tools/emerald_contest.py makes contest_data.inc from pret/pokeemerald):
// Primary Judging scores a Pokemon's condition (the contest's own, half its two neighbours' and its sheen), then
// five turns of appeals: each move has a category and an effect (so many hearts, startling those who went before,
// guarding against it, better first or last...). A move of the contest's category excites the crowd - five in a
// row and they go wild; the two unrelated categories cool them off. A combo (Rain Dance, then Thunder...) earns
// its appeal again; the same move twice in a row bores the judge. Total = Primary Judging + appeals x 2.
#include "contest.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "berries.h"
#include "breeding.h"
#include "game.h"
#include "items.h"
#include "natures.h"
#include "profile.h"
#include "sprites.h"
#include "types.h"
#include "ui.h"

namespace {
struct ContestMoveInfo { const char* name; int cat, effect; const char* starter; const char* follows; };
struct ContestEffectInfo { int kind, appeal, jam; const char* desc; };
struct ContestOpponentInfo { const char* trainer; const char* nick; const char* species; int rank, pools; const char* moves[4]; int cond[6]; };
#include "contest_data.inc"
enum { K_APPEAL = 0, K_AVOID = 1, K_STARTLE_ONE = 2, K_STARTLE_ALL = 3, K_WORSEN = 4, K_SPECIAL = 5, K_ORDER = 6 };

std::string Squash(const std::string& n) {
    std::string k;
    for (char c : n) if (std::isalnum((unsigned char)c)) k += (char)std::tolower((unsigned char)c);
    return k;
}
const ContestMoveInfo* MoveInfo(const std::string& move) {
    static std::vector<std::pair<std::string, const ContestMoveInfo*>> idx;
    if (idx.empty()) for (const auto& m : kContestMoves) idx.push_back({Squash(m.name), &m});
    std::string k = Squash(move);
    for (const auto& [n, p] : idx) if (n == k) return p;
    return nullptr;
}
const MoveDef* FindMoveSquashed(const std::string& n) {   // "Softboiled" finds Soft-Boiled
    if (const MoveDef* d = FindMove(n)) return d;
    std::string k = Squash(n);
    for (const auto& m : MoveLibrary()) if (Squash(m.name) == k) return &m;
    return nullptr;
}
// Moves Emerald never had: their category from their type
int TypeCat(Type t) {
    switch (t) {
        case Type::Fire: case Type::Water: case Type::Ice: case Type::Flying: case Type::Fairy: return 1;
        case Type::Normal: case Type::Bug: case Type::Grass: return 2;
        case Type::Psychic: case Type::Ghost: case Type::Dark: case Type::Poison: return 3;
        case Type::Fighting: case Type::Rock: case Type::Ground: case Type::Steel: return 4;
        default: return 0;   // Electric, Dragon: Cool
    }
}
int Excitement(int contestCat, int moveCat) {   // the crowd: +1 its own category, 0 a neighbour, -1 the others
    if (moveCat == contestCat) return 1;
    static const int kNext[5] = {4, 0, 1, 2, 3}, kPrev[5] = {1, 2, 3, 4, 0};   // Cool's neighbours are Tough and Beauty, ...
    if (moveCat == kNext[contestCat] || moveCat == kPrev[contestCat]) return 0;
    return -1;
}
int Round1(const ContestEntrant& e, int cat) {   // Emerald's CalculateContestantRound1Points
    static const int kSub[5][2] = {{4, 1}, {0, 2}, {1, 3}, {2, 4}, {3, 0}};
    return e.cond[cat] + (e.cond[kSub[cat][0]] + e.cond[kSub[cat][1]] + e.cond[5]) / 2;
}
Profile& WhoP(Game& g) { return Prof(g, g.contest.who); }
std::vector<int> Party(const Profile& P) {
    std::vector<int> v;
    for (int i = 0; i < (int)P.mons.size(); i++) if (!P.mons[i].boxed && P.mons[i].eggSteps <= 0) v.push_back(i);
    return v;
}
const Color kCatCol[5] = {{240, 90, 70, 255}, {90, 150, 255, 255}, {255, 140, 200, 255}, {110, 210, 110, 255}, {250, 200, 60, 255}};
}   // namespace

const char* ContestCatName(int c) { static const char* n[5] = {"Cool", "Beauty", "Cute", "Smart", "Tough"}; return c >= 0 && c < 5 ? n[c] : ""; }
const char* ContestRankName(int r) { static const char* n[4] = {"Normal", "Super", "Hyper", "Master"}; return r >= 0 && r < 4 ? n[r] : ""; }
int MoveContestCat(const std::string& move) {
    if (const ContestMoveInfo* m = MoveInfo(move)) return m->cat;
    const MoveDef* d = FindMove(move);
    return d ? TypeCat(d->type) : 2;
}
std::string RibbonName(int cat, int rank) { return std::string(ContestCatName(cat)) + " Ribbon" + (rank > 0 ? std::string(" ") + ContestRankName(rank) : ""); }
int ContestRankOpen(const OwnedMon& m, int cat) {
    int r = 0;
    while (r < 3 && std::find(m.ribbons.begin(), m.ribbons.end(), RibbonName(cat, r)) != m.ribbons.end()) r++;
    return r;
}

std::string FeedPokeblock(OwnedMon& m, int bi) {
    if (m.cond[5] >= 255) return MonName(m) + " won't eat any more...";
    const BerryInfo& b = BerryAt(bi);
    static const int kLike[5] = {0, 4, 2, 1, 3};   // the flavour a nature's raised stat likes: Atk Spicy, Def Sour, Spe Sweet, SpA Dry, SpD Bitter
    int up = NatureUp(m.nature), down = NatureDown(m.nature);
    int like = up >= 0 ? kLike[up] : -1, hate = down >= 0 ? kLike[down] : -1;
    std::string gain;
    for (int k = 0; k < 5; k++) {
        if (b.flavor[k] <= 0) continue;
        int v = b.flavor[k] * 2;
        if (k == like) v = v * 11 / 10;
        if (k == hate) v = v * 9 / 10;
        m.cond[k] = std::min(255, m.cond[k] + v);
        gain += std::string(gain.empty() ? "" : ", ") + ContestCatName(k) + TextFormat(" +%d", v);
    }
    m.cond[5] = std::min(255, m.cond[5] + std::max(1, b.smooth / 2));
    bool liked = like >= 0 && b.flavor[like] > 0, hated = hate >= 0 && b.flavor[hate] > 0;
    return MonName(m) + (liked && !hated ? " happily ate the Pokeblock!" : hated && !liked ? " ate the Pokeblock... it doesn't seem to like that flavour." : " ate the Pokeblock.") +
           " (" + gain + ")";
}

void OpenContest(Game& g, int who) {
    g.contest = ContestState();
    g.contest.who = who;
    g.contest.back = (int)g.screen;
    g.screen = Screen::Contest;
}

namespace {
void Log(Game& g, const std::string& l) {
    auto& L = g.contest.log;
    L.push_back(l);
    if (L.size() > 6) L.erase(L.begin());
}
void Start(Game& g, int monIdx) {
    ContestState& c = g.contest;
    Profile& P = WhoP(g);
    const OwnedMon& m = P.mons[monIdx];
    ContestEntrant& me = c.e[0];
    me = ContestEntrant();
    me.trainer = P.name;
    me.name = MonName(m);
    me.species = MonSpecies(m);
    me.moves = m.moves;
    for (int k = 0; k < 6; k++) me.cond[k] = m.cond[k];
    me.player = true;
    me.mon = monIdx;
    // three of Emerald's contestants for this rank and category
    std::vector<int> pool, any;
    for (int i = 0; i < (int)(sizeof kContestOpponents / sizeof kContestOpponents[0]); i++)
        if (kContestOpponents[i].rank == c.rank) { any.push_back(i); if (kContestOpponents[i].pools >> c.cat & 1) pool.push_back(i); }
    if (pool.size() < 3) pool = any;
    for (int k = 1; k < 4 && !pool.empty(); k++) {
        int j = GetRandomValue(0, (int)pool.size() - 1);
        const ContestOpponentInfo& o = kContestOpponents[pool[j]];
        pool.erase(pool.begin() + j);
        ContestEntrant& e = c.e[k];
        e = ContestEntrant();
        e.trainer = o.trainer;
        e.name = o.nick;
        e.species = SpeciesIndex(o.species);
        for (const char* mv : o.moves) {   // Emerald's "DOUBLE-EDGE": the game's own spelling if it has the move
            if (!mv || !*mv) continue;
            std::string n = mv;
            for (size_t q = 0; q < n.size(); q++) n[q] = (q == 0 || !std::isalpha((unsigned char)n[q - 1])) ? (char)std::toupper((unsigned char)n[q]) : (char)std::tolower((unsigned char)n[q]);
            if (const MoveDef* d = FindMoveSquashed(n)) n = d->name;
            e.moves.push_back(n);
        }
        for (int s = 0; s < 6; s++) e.cond[s] = o.cond[s];
    }
    for (int k = 0; k < 4; k++) c.e[k].r1 = Round1(c.e[k], c.cat);
    c.order = {0, 1, 2, 3};
    std::sort(c.order.begin(), c.order.end(), [&](int a, int b) { return c.e[a].r1 > c.e[b].r1; });   // the best condition goes first
    c.phase = 3;
    c.t = 0;
    c.turn = 0;
    c.step = 0;
    c.excite = 0;
    c.log.clear();
    Log(g, std::string(ContestCatName(c.cat)) + " Contest, " + ContestRankName(c.rank) + " Rank: Primary Judging!");
}
// One appeal: the move's hearts, its effect, the crowd.
void Appeal(Game& g, int who, const std::string& move) {
    ContestState& c = g.contest;
    ContestEntrant& e = c.e[who];
    const ContestMoveInfo* mi = MoveInfo(move);
    int cat = MoveContestCat(move);
    int effect = mi ? mi->effect : 0;
    const ContestEffectInfo& fx = kContestEffects[std::max(0, std::min((int)(sizeof kContestEffects / sizeof kContestEffects[0]) - 1, effect))];
    int pos = 0;
    while (pos < (int)c.order.size() && c.order[pos] != who) pos++;
    if (e.nervous && GetRandomValue(0, 1) == 0) {
        e.turnPts = 0;
        e.nervous = false;
        Log(g, e.name + " was too nervous to move!");
        return;
    }
    int pts = mi ? fx.appeal : 30;
    std::string note;
    if (fx.kind == K_SPECIAL) {   // the ones that depend on when it goes
        switch (effect) {
            case 29: pts = pos == 0 ? 60 : 20; break;                       // better if first
            case 30: pts = pos == 3 ? 60 : 20; break;                       // better if last
            case 31: case 32: {                                             // as good as the one(s) before
                int best = 10;
                for (int p = 0; p < pos; p++) best = effect == 31 ? std::max(best, c.e[c.order[p]].turnPts) : c.e[c.order[p]].turnPts;
                pts = std::max(10, best);
            } break;
            case 33: pts = 10 + pos * 20; break;                            // better when later
            case 34: pts = 10 * GetRandomValue(1, 8); break;                // depends on timing
            case 35: pts = (pos > 0 && MoveContestCat(c.e[c.order[pos - 1]].last) == cat) ? 60 : 20; break;
            case 36: pts = (pos > 0 && MoveContestCat(c.e[c.order[pos - 1]].last) != cat) ? 60 : 20; break;
            case 39: pts = 10 + e.cond[c.cat] / 30; break;                  // better with good condition
            case 46: pts = 10 + c.excite * 15; break;                       // better when the crowd's excited
            default: break;
        }
    }
    if (effect == 2) e.done = true;   // a great appeal, but no more moves
    if (fx.kind == K_AVOID) e.guard = true;
    if (!e.last.empty() && e.last == move && effect != 3) { pts -= 10; note = " The judge looked at it disapprovingly..."; }   // the same move again
    if (mi && *mi->follows && !e.last.empty()) {   // a combo: this move follows the last one
        const ContestMoveInfo* lm = MoveInfo(e.last);
        std::string f = std::string("|") + mi->follows + "|";
        if (lm && *lm->starter && f.find(std::string("|") + lm->starter + "|") != std::string::npos) { pts += mi ? fx.appeal : 20; note += " It's a combo!"; }
    }
    int ex = Excitement(c.cat, cat);
    if (ex > 0 && effect != 47) {
        c.excite++;
        pts += 10;   // the crowd likes it
        if (c.excite >= 5) { pts += 60; c.excite = 0; note += " The crowd went wild!"; PlaySfx(Sfx::Win); }
    } else if (ex < 0) { c.excite = std::max(0, c.excite - 1); pts -= 10; }
    if (effect == 44) c.excite = std::min(4, c.excite + 1);   // excites the crowd in any contest
    e.turnPts = std::max(0, pts);
    e.appeal += e.turnPts;
    e.last = move;
    Log(g, e.name + " used " + move + "! (+" + std::to_string(e.turnPts) + ")" + note);
    // startling those who went before (and making those after nervous)
    if (fx.kind == K_STARTLE_ONE || fx.kind == K_STARTLE_ALL || fx.kind == K_WORSEN) {
        int jam = std::max(10, fx.jam);
        int from = fx.kind == K_STARTLE_ONE ? std::max(0, pos - 1) : 0;
        int hit = 0;
        for (int p = from; p < pos; p++) {
            ContestEntrant& o = c.e[c.order[p]];
            if (o.guard) continue;
            int lose = std::min(o.turnPts > 0 ? jam : jam / 2, std::max(0, o.appeal));
            o.appeal -= lose;
            hit++;
        }
        if (hit) Log(g, "  It startled the Pokemon that went before!");
    }
    if (effect == 25 && pos < 3) c.e[c.order[pos + 1]].nervous = true;
    if (effect == 26) for (int p = pos + 1; p < 4; p++) c.e[c.order[p]].nervous = true;
    PlaySfx(e.turnPts >= 40 ? Sfx::Buff : Sfx::MenuOk);
}
std::string CpuMove(const ContestState& c, const ContestEntrant& e) {   // the best appeal it can see
    std::string best;
    int bestV = -999;
    for (const auto& mv : e.moves) {
        const ContestMoveInfo* mi = MoveInfo(mv);
        int v = mi ? kContestEffects[mi->effect].appeal + kContestEffects[mi->effect].jam / 2 : 25;
        v += 15 * Excitement(c.cat, MoveContestCat(mv));
        if (mv == e.last) v -= 20;
        v += GetRandomValue(0, 12);
        if (v > bestV) { bestV = v; best = mv; }
    }
    return best.empty() ? std::string("Tackle") : best;
}
void EndTurn(Game& g) {
    ContestState& c = g.contest;
    for (auto& e : c.e) e.guard = false;
    c.turn++;
    if (c.turn >= 5) {   // all out of appeal time: the results
        int score[4];
        for (int k = 0; k < 4; k++) score[k] = c.e[k].r1 + c.e[k].appeal * 2;
        int ord[4] = {0, 1, 2, 3};
        std::sort(ord, ord + 4, [&](int a, int b) { return score[a] > score[b]; });
        for (int k = 0; k < 4; k++) c.place[k] = ord[k];
        c.phase = 5;
        c.t = 0;
        Profile& P = WhoP(g);
        if (ord[0] == 0 && c.e[0].mon >= 0 && c.e[0].mon < (int)P.mons.size()) {
            OwnedMon& m = P.mons[c.e[0].mon];
            std::string rib = RibbonName(c.cat, c.rank);
            if (std::find(m.ribbons.begin(), m.ribbons.end(), rib) == m.ribbons.end()) { m.ribbons.push_back(rib); c.prize = MonName(m) + " won the " + rib + "!"; }
            else c.prize = MonName(m) + " won the contest again!";
            int coins = 300 * (c.rank + 1);
            P.coins += coins;
            c.prize += TextFormat("  (+%d coins)", coins);
            PlaySfx(Sfx::Win);
        } else c.prize = "Better luck next time! Feed it Pokeblocks to make it shine.";
        SaveProfile(P);
        return;
    }
    // next turn: the best appeal this turn goes first
    std::vector<int> o = c.order;
    std::stable_sort(o.begin(), o.end(), [&](int a, int b) { return c.e[a].turnPts > c.e[b].turnPts; });
    c.order = o;
    c.step = 0;
    Log(g, TextFormat("Appeal %d of 5!", c.turn + 1));
}
}   // namespace

void UpdateContest(Game& g) {
    ContestState& c = g.contest;
    PlayerInput in = MenuInput();
    c.t += GetFrameTime();
    Profile& P = WhoP(g);
    auto leave = [&]() { g.screen = (Screen)c.back; PlaySfx(Sfx::MenuBack); };
    if (c.phase == 0 || c.phase == 1 || c.phase == 2) {
        int n = c.phase == 0 ? CONTEST_CATS : c.phase == 1 ? CONTEST_RANKS : (int)Party(P).size();
        if (in.back) { if (c.phase == 0) leave(); else { c.phase--; c.sel = 0; PlaySfx(Sfx::MenuBack); } return; }
        if (n <= 0) { leave(); return; }
        if (in.up) { c.sel = (c.sel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
        if (in.down) { c.sel = (c.sel + 1) % n; PlaySfx(Sfx::MenuMove); }
        if (!in.confirm) return;
        if (c.phase == 0) { c.cat = c.sel; c.phase = 1; c.sel = 0; PlaySfx(Sfx::MenuOk); return; }
        if (c.phase == 1) { c.rank = c.sel; c.phase = 2; c.sel = 0; PlaySfx(Sfx::MenuOk); return; }
        int mi = Party(P)[c.sel];
        if (ContestRankOpen(P.mons[mi], c.cat) < c.rank) {
            Toast(g, MonName(P.mons[mi]) + " needs the " + RibbonName(c.cat, c.rank - 1) + " to enter " + ContestRankName(c.rank) + " Rank.");
            PlaySfx(Sfx::MenuBack);
            return;
        }
        PlaySfx(Sfx::MenuOk);
        Start(g, mi);
        return;
    }
    if (c.phase == 3) {   // Primary Judging: just shown
        if (c.t > 1.5f && (in.confirm || c.t > 4)) { c.phase = 4; c.t = 0; c.step = 0; Log(g, "Appeal 1 of 5!"); }
        return;
    }
    if (c.phase == 4) {
        if (c.step >= 4) { if (c.t > 0.6f) { c.t = 0; EndTurn(g); } return; }
        int who = c.order[c.step];
        ContestEntrant& e = c.e[who];
        if (e.done) { e.turnPts = 0; c.step++; c.t = 0; return; }
        if (e.player) {
            int n = (int)e.moves.size();
            if (n == 0) { Appeal(g, who, "Tackle"); c.step++; c.t = 0; return; }
            c.choosing = true;
            if (in.up) { c.moveSel = (c.moveSel + n - 1) % n; PlaySfx(Sfx::MenuMove); }
            if (in.down) { c.moveSel = (c.moveSel + 1) % n; PlaySfx(Sfx::MenuMove); }
            if (in.confirm && c.t > 0.3f) { c.choosing = false; Appeal(g, who, e.moves[std::min(c.moveSel, n - 1)]); c.step++; c.t = 0; }
            return;
        }
        if (c.t > 1.0f) { Appeal(g, who, CpuMove(c, e)); c.step++; c.t = 0; }
        return;
    }
    if (c.phase == 5 && c.t > 1.0f && (in.confirm || in.back)) leave();
}

void DrawContest(const Game& g) {
    const ContestState& c = g.contest;
    const Profile& P = Prof(const_cast<Game&>(g), c.who);
    DrawMenuBackdrop(Color{40, 26, 46, 255}, (float)GetTime());
    DrawTextCentered("POKEMON CONTEST", VW / 2, 8, 20, UI_GOLD);
    if (c.phase <= 2) {
        static const char* kTitle[3] = {"Which contest would you like to enter?", "Which rank?", "Which Pokemon will appeal?"};
        DrawTextCentered(kTitle[c.phase], VW / 2, 36, 10, UI_TEXT, false);
        if (c.phase == 0)
            for (int k = 0; k < CONTEST_CATS; k++) {
                int y = 70 + k * 30;
                DrawPanel(VW / 2 - 120, y, 240, 24, k == c.sel ? Color{70, 50, 80, 255} : UI_PANEL, k == c.sel ? UI_GOLD : UI_BORDER);
                DrawRectangle(VW / 2 - 112, y + 7, 10, 10, kCatCol[k]);
                GDrawText((std::string(ContestCatName(k)) + " Contest").c_str(), VW / 2 - 94, y + 7, 10, UI_TEXT);
            }
        else if (c.phase == 1)
            for (int k = 0; k < CONTEST_RANKS; k++) {
                int y = 70 + k * 30;
                DrawPanel(VW / 2 - 120, y, 240, 24, k == c.sel ? Color{70, 50, 80, 255} : UI_PANEL, k == c.sel ? UI_GOLD : UI_BORDER);
                GDrawText((std::string(ContestRankName(k)) + " Rank" + (k > 0 ? std::string("  (needs the ") + RibbonName(c.cat, k - 1) + ")" : "")).c_str(), VW / 2 - 108, y + 7, 10, UI_TEXT);
            }
        else {
            auto party = Party(P);
            for (int k = 0; k < (int)party.size(); k++) {
                const OwnedMon& m = P.mons[party[k]];
                int y = 56 + k * 44;
                bool open = ContestRankOpen(m, c.cat) >= c.rank;
                DrawPanel(VW / 2 - 200, y, 400, 40, k == c.sel ? Color{70, 50, 80, 255} : UI_PANEL, k == c.sel ? UI_GOLD : UI_BORDER);
                int si = MonSpecies(m);
                if (si >= 0) DrawSpeciesFit(si, VIEW_FRONT, VW / 2 - 180, y + 38, 34);
                GDrawText(TextFormat("%s  Lv%d", MonName(m).c_str(), m.level), VW / 2 - 156, y + 5, 10, open ? UI_TEXT : UI_DIM);
                for (int s = 0; s < 5; s++) {
                    int bx = VW / 2 - 156 + s * 62;
                    DrawRectangle(bx, y + 22, 56, 6, Color{30, 20, 34, 255});
                    DrawRectangle(bx, y + 22, 56 * m.cond[s] / 255, 6, kCatCol[s]);
                }
                GDrawText(TextFormat("Sheen %d", m.cond[5]), VW / 2 + 158, y + 20, 10, UI_DIM);
                GDrawText(TextFormat("%d ribbons", (int)m.ribbons.size()), VW / 2 + 120, y + 5, 10, UI_GOLD);
            }
        }
        DrawTextCentered("Up/Down choose   Enter pick   Esc back", VW / 2, VH - 18, 10, UI_DIM, false);
        return;
    }
    // the stage: four contestants
    GDrawText(TextFormat("%s Contest - %s Rank", ContestCatName(c.cat), ContestRankName(c.rank)), 16, 36, 10, kCatCol[c.cat]);
    if (c.phase == 4) GDrawText(TextFormat("Appeal %d of 5", std::min(5, c.turn + 1)), VW - 120, 36, 10, UI_TEXT);
    for (int k = 0; k < 5; k++) DrawRectangle(VW / 2 - 60 + k * 24, 38, 20, 8, k < c.excite ? kCatCol[c.cat] : Color{60, 50, 66, 255});   // the crowd
    for (int k = 0; k < 4; k++) {
        const ContestEntrant& e = c.e[k];
        int x = 16 + k * ((VW - 32) / 4), w = (VW - 32) / 4 - 8, y = 56;
        bool acting = c.phase == 4 && c.step < 4 && c.order[c.step] == k;
        DrawPanel(x, y, w, 150, acting ? Color{80, 60, 90, 255} : UI_PANEL, e.player ? UI_GOLD : UI_BORDER);
        if (e.species >= 0) DrawSpeciesFit(e.species, VIEW_FRONT, x + w / 2, y + 92, 74 + (acting ? (int)(std::sin(c.t * 10) * 3) : 0));
        DrawTextCentered(e.name.c_str(), x + w / 2, y + 96, 10, UI_TEXT, false);
        DrawTextCentered(e.trainer.c_str(), x + w / 2, y + 108, 10, UI_DIM, false);
        int shown = c.phase == 3 ? e.r1 : c.phase >= 5 ? e.r1 + e.appeal * 2 : e.appeal;
        DrawTextCentered(TextFormat(c.phase == 3 ? "Condition %d" : c.phase >= 5 ? "Total %d" : "Appeal %d", shown), x + w / 2, y + 122, 10, UI_GOLD, false);
        int hearts = std::min(8, e.turnPts / 10);
        for (int h = 0; h < hearts && c.phase == 4; h++) DrawCircle(x + 10 + h * 10, y + 140, 4, Color{255, 90, 120, 255});
        if (e.nervous) GDrawText("nervous", x + 6, y + 4, 10, Color{255, 150, 130, 255});
    }
    int ly = 214;
    for (const auto& l : c.log) { GDrawText(l.c_str(), 20, ly, 10, UI_TEXT); ly += 13; }
    if (c.phase == 4 && c.choosing) {
        const ContestEntrant& me = c.e[0];
        int bx = VW - 260, by = 214;
        DrawPanel(bx, by - 4, 244, 22 + 16 * (int)me.moves.size() + 30, Color{30, 22, 36, 245}, UI_GOLD);
        GDrawText("Your appeal:", bx + 8, by, 10, UI_GOLD);
        for (int k = 0; k < (int)me.moves.size(); k++) {
            int cat = MoveContestCat(me.moves[k]);
            int y = by + 16 + k * 16;
            if (k == c.moveSel) DrawRectangle(bx + 4, y - 2, 236, 15, Color{70, 50, 80, 255});
            DrawRectangle(bx + 8, y + 2, 8, 8, kCatCol[cat]);
            GDrawText(me.moves[k].c_str(), bx + 22, y, 10, UI_TEXT);
        }
        if (!me.moves.empty()) {
            const ContestMoveInfo* mi = MoveInfo(me.moves[std::min(c.moveSel, (int)me.moves.size() - 1)]);
            const char* d = mi ? kContestEffects[mi->effect].desc : "A plain appeal (it isn't one of Emerald's moves).";
            DrawWrapped(d, bx + 8, by + 18 + 16 * (int)me.moves.size(), 228, 10, UI_DIM);
        }
    }
    if (c.phase == 5) {
        DrawPanel(VW / 2 - 220, VH - 92, 440, 76, Color{30, 22, 36, 245}, UI_GOLD);
        std::string places;
        for (int k = 0; k < 4; k++) places += TextFormat("%d. %s   ", k + 1, c.e[c.place[k]].name.c_str());
        DrawTextCentered(places.c_str(), VW / 2, VH - 84, 10, UI_TEXT, false);
        DrawTextCentered(c.prize.c_str(), VW / 2, VH - 64, 10, UI_GOLD, false);
        DrawTextCentered("Enter to leave", VW / 2, VH - 36, 10, UI_DIM, false);
    }
}
