#include "breeding.h"
#include "profile.h"
#include "data.h"
#include "moves.h"
#include "raylib.h"
#include "rlgl.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace {
struct BreedInfo {
    const char* name;
    const char* g1;
    const char* g2;
    int gender;       // -1 genderless, else eighths female
    int cycles;       // egg cycles
    int ev[6];        // EV yield
    const char* egg;  // what hatches from its Eggs
    const char* moves;   // egg moves "a|b|c"
};
#include "breeding.inc"

const BreedInfo* Info(const std::string& species) {
    static std::map<std::string, const BreedInfo*> idx;
    if (idx.empty())
        for (const auto& b : kBreed) idx[b.name] = &b;
    auto it = idx.find(species);
    if (it != idx.end()) return it->second;
    int si = SpeciesIndex(species);   // a Mega form: its base Pokemon's
    if (si >= 0 && !AllSpecies()[si].megaOf.empty()) {
        it = idx.find(AllSpecies()[si].megaOf);
        if (it != idx.end()) return it->second;
    }
    return nullptr;
}

int R(int lo, int hi) { return GetRandomValue(lo, hi); }
}  // namespace

const char* StatShort(int k) {
    static const char* n[6] = {"HP", "Atk", "Def", "SpA", "SpD", "Spe"};
    return n[k < 0 || k > 5 ? 0 : k];
}

void RollIVs(int iv[6]) {
    for (int k = 0; k < 6; k++) iv[k] = R(0, IV_MAX);
}

int EvTotal(const OwnedMon& m) {
    int t = 0;
    for (int k = 0; k < 6; k++) t += m.ev[k];
    return t;
}

int AddEvs(OwnedMon& m, const int add[6]) {
    int got = 0;
    for (int k = 0; k < 6; k++) {
        int room = std::min(EV_STAT_MAX - m.ev[k], EV_TOTAL_MAX - EvTotal(m));
        int a = add[k] > 0 ? std::min(add[k], std::max(0, room)) : std::max(add[k], -m.ev[k]);   // (negative: an EV-lowering berry)
        m.ev[k] += a;
        got += a;
    }
    return got;
}

void FixMonStats(OwnedMon& m) {
    bool rolled = true;
    for (int k = 0; k < 6; k++) rolled = rolled && m.iv[k] >= 0;
    if (!rolled) {
        int si = SpeciesIndex(m.species);
        RollIVs(m.iv);
        if (si >= 0 && AllSpecies()[si].legendary) {   // legendaries: three perfect IVs (X and Y on)
            int done = 0;
            while (done < 3) {
                int k = R(0, 5);
                if (m.iv[k] != IV_MAX) { m.iv[k] = IV_MAX; done++; }
            }
        }
    }
    for (int k = 0; k < 6; k++) m.iv[k] = std::max(0, std::min(IV_MAX, m.iv[k]));
    // vitamins taken before EVs existed: each one is 10 EVs now (Protein and Calcium raised both attacks, Iron and Zinc both defenses)
    if (m.vit[0] || m.vit[1] || m.vit[2] || m.vit[3]) {
        int add[6] = {m.vit[0] * VITAMIN_EVS, m.vit[1] * VITAMIN_EVS, m.vit[2] * VITAMIN_EVS, m.vit[1] * VITAMIN_EVS, m.vit[2] * VITAMIN_EVS, m.vit[3] * VITAMIN_EVS};
        for (int& v : m.vit) v = 0;
        AddEvs(m, add);
    }
    for (int k = 0; k < 6; k++) m.ev[k] = std::max(0, std::min(EV_STAT_MAX, m.ev[k]));
    while (EvTotal(m) > EV_TOTAL_MAX) {
        int big = 0;
        for (int k = 1; k < 6; k++) if (m.ev[k] > m.ev[big]) big = k;
        m.ev[big]--;
    }
    if (m.gender < 0) m.gender = RollGender(m.species);
    if ((int)m.nick.size() > NICK_MAX) m.nick.resize(NICK_MAX);
}

void EvYield(const std::string& species, int out[6]) {
    const BreedInfo* b = Info(species);
    for (int k = 0; k < 6; k++) out[k] = b ? b->ev[k] : 0;
    if (!b) out[ST_HP] = 1;   // a Pokemon of your own: 1 HP EV
}

std::string IvJudge(int iv) {   // the Judge in Sun and Moon on
    if (iv >= 31) return "Best";
    if (iv >= 30) return "Fantastic";
    if (iv >= 26) return "Very good";
    if (iv >= 16) return "Pretty good";
    if (iv >= 1) return "Decent";
    return "No good";
}

std::string IvVerdict(const OwnedMon& m) {
    int t = 0;
    for (int k = 0; k < 6; k++) t += std::max(0, m.iv[k]);
    if (t >= 151) return "Outstanding potential!";
    if (t >= 121) return "Relatively superior potential overall.";
    if (t >= 91) return "Above-average ability.";
    return "Decent ability all around.";
}

int GenderRate(const std::string& species) {
    const BreedInfo* b = Info(species);
    return b ? b->gender : 4;
}

int RollGender(const std::string& species) {
    int r = GenderRate(species);
    if (r < 0) return 2;
    if (r == 0) return 0;
    if (r >= 8) return 1;
    return R(0, 7) < r ? 1 : 0;
}

void DrawGender(int gender, float x, float y, float s) {
    if (gender < 0 || gender > 1) return;
    Color c = gender == 0 ? Color{90, 160, 255, 255} : Color{255, 110, 160, 255};
    float r = s * 0.32f;
    if (gender == 0) {   // the male sign: a ring with an arrow up and to the right
        Vector2 o{x + r, y + s - r};
        DrawRing(o, r - 1.2f, r, 0, 360, 16, c);
        Vector2 tip{x + s, y};
        DrawLineEx({o.x + r * 0.7f, o.y - r * 0.7f}, tip, 1.4f, c);
        DrawLineEx(tip, {tip.x - s * 0.36f, tip.y}, 1.4f, c);
        DrawLineEx(tip, {tip.x, tip.y + s * 0.36f}, 1.4f, c);
    } else {             // the female sign: a ring with a cross below
        Vector2 o{x + s * 0.5f, y + r};
        DrawRing(o, r - 1.2f, r, 0, 360, 16, c);
        DrawLineEx({o.x, o.y + r}, {o.x, y + s}, 1.4f, c);
        DrawLineEx({o.x - s * 0.22f, y + s * 0.75f}, {o.x + s * 0.22f, y + s * 0.75f}, 1.4f, c);
    }
}

std::string MonName(const OwnedMon& m) { return m.nick.empty() ? m.species : m.nick; }

// ------------------------------------------------------------------ the Day Care

std::vector<std::string> EggGroups(const std::string& species) {
    const BreedInfo* b = Info(species);
    std::vector<std::string> v;
    if (!b) { v.push_back("Undiscovered"); return v; }
    if (b->g1[0]) v.push_back(b->g1);
    if (b->g2[0]) v.push_back(b->g2);
    return v;
}

int EggCycles(const std::string& species) {
    const BreedInfo* b = Info(species);
    return b ? std::max(1, b->cycles) : 20;
}

namespace {
bool Has(const std::vector<std::string>& v, const char* s) { return std::find(v.begin(), v.end(), s) != v.end(); }
}

// How often an Egg turns up (the games' 20 / 50 / 70%): Pokemon need an egg group in common and to be a
// male and a female - or one of them is Ditto. The same species gets along better; so do two trainers' Pokemon.
int DaycareCompat(const OwnedMon& a, const OwnedMon& b) {
    auto ga = EggGroups(a.species), gb = EggGroups(b.species);
    if (Has(ga, "Undiscovered") || Has(gb, "Undiscovered")) return 0;
    bool da = Has(ga, "Ditto"), db = Has(gb, "Ditto");
    if (da && db) return 0;
    bool sameOt = a.ot == b.ot;
    if (da || db) return sameOt ? 20 : 50;   // Ditto: with anything that can breed (genderless ones too)
    if (a.gender == 2 || b.gender == 2 || a.gender == b.gender) return 0;
    bool common = false;
    for (const auto& g : ga) common = common || Has(gb, g.c_str());
    if (!common) return 0;
    bool same = a.species == b.species || std::string(Info(a.species) ? Info(a.species)->egg : "") == (Info(b.species) ? Info(b.species)->egg : "x");
    if (same) return sameOt ? 50 : 70;
    return sameOt ? 20 : 50;
}

std::string DaycareLine(int c) {
    if (c >= 70) return "The two seem to get along very well.";
    if (c >= 50) return "The two seem to get along.";
    if (c >= 20) return "The two don't seem to like each other much.";
    return "The two prefer to play with other Pokemon than each other.";
}

// An Egg from these two (they must be compatible), the way Sun and Moon make them:
// the species is the mother's first stage (or the non-Ditto parent's); an Everstone passes its holder's
// nature; three IVs come from the parents (five with a Destiny Knot); egg moves either parent knows are
// passed down; the mother (or a Ditto's partner) may pass her hidden ability; Pokemon from two different
// countries' games (here: two trainers) make shiny Eggs more often (the Masuda method).
bool MakeEgg(const OwnedMon& a, const OwnedMon& b, OwnedMon& egg, const std::string& trainer) {
    if (DaycareCompat(a, b) <= 0) return false;
    bool da = Has(EggGroups(a.species), "Ditto"), db = Has(EggGroups(b.species), "Ditto");
    const OwnedMon* mother = &a;
    const OwnedMon* father = &b;
    if (da) std::swap(mother, father);              // the non-Ditto one decides
    else if (!db && a.gender != 1) std::swap(mother, father);
    const BreedInfo* mi = Info(mother->species);
    if (!mi) return false;
    std::string child = mi->egg;
    // two species that come in two halves
    if (child == "Nidoran F" && R(0, 1)) child = "Nidoran M";
    if (child == "Illumise") child = R(0, 1) ? "Volbeat" : "Illumise";
    if (child == "Manaphy") child = "Phione";
    int si = SpeciesIndex(child);
    if (si < 0) return false;
    egg = NewMon(si, 1);
    egg.caught = false;
    egg.metAt = "Day Care";
    egg.gender = RollGender(child);
    // nature: an Everstone
    bool ea = a.item == "Everstone", eb = b.item == "Everstone";
    if (ea && eb) egg.nature = R(0, 1) ? a.nature : b.nature;
    else if (ea) egg.nature = a.nature;
    else if (eb) egg.nature = b.nature;
    // IVs: 3 from the parents (a Destiny Knot: 5), the rest random
    RollIVs(egg.iv);
    int pass = (a.item == "Destiny Knot" || b.item == "Destiny Knot") ? 5 : 3;
    int stats[6] = {0, 1, 2, 3, 4, 5};
    for (int k = 5; k > 0; k--) std::swap(stats[k], stats[R(0, k)]);
    for (int k = 0; k < pass; k++) {
        const OwnedMon& p = R(0, 1) ? a : b;
        egg.iv[stats[k]] = std::max(0, p.iv[stats[k]]);
    }
    // the Power items: the stat they're for is passed on for sure
    static const char* kPower[6] = {"Power Weight", "Power Bracer", "Power Belt", "Power Lens", "Power Band", "Power Anklet"};
    for (int k = 0; k < 6; k++) {
        if (a.item == kPower[k]) egg.iv[k] = std::max(0, a.iv[k]);
        if (b.item == kPower[k]) egg.iv[k] = std::max(0, b.iv[k]);
    }
    // ability: a hidden ability comes from the mother 60% of the time
    if (mother->abilitySlot == 2 && R(1, 100) <= 60) egg.abilitySlot = 2;
    else egg.abilitySlot = RollAbilitySlot(si, false);
    // moves: the level-1 moves, then the moves both parents know that it learns by level, then egg moves
    std::vector<std::string> mv;
    for (const auto& m : MovesAtLevel(AllSpecies()[si], 1)) mv.push_back(m.name);
    auto knows = [&](const std::string& n) {
        return std::find(a.moves.begin(), a.moves.end(), n) != a.moves.end() || std::find(b.moves.begin(), b.moves.end(), n) != b.moves.end();
    };
    auto add = [&](const std::string& n) {
        if (std::find(mv.begin(), mv.end(), n) != mv.end() || !FindMove(n)) return;
        mv.push_back(n);
    };
    for (const auto& e : AllSpecies()[si].learnset)
        if (std::find(a.moves.begin(), a.moves.end(), e.move.name) != a.moves.end() &&
            std::find(b.moves.begin(), b.moves.end(), e.move.name) != b.moves.end()) add(e.move.name);
    const BreedInfo* ci = Info(child);
    if (ci && ci->moves[0]) {
        std::stringstream ss(ci->moves);
        std::string n;
        while (std::getline(ss, n, '|'))
            if (knows(n)) add(n);
    }
    while (mv.size() > 4) mv.erase(mv.begin());   // the newest four, like the games
    if (!mv.empty()) egg.moves = mv;
    // shiny: the Masuda method (the parents come from two different trainers): 6x the odds
    int odds = SHINY_ODDS;
    if (a.ot != b.ot) odds = std::max(1, odds / 6);
    egg.shiny = egg.shiny || GetRandomValue(1, odds) == 1;
    egg.eggSteps = EggCycles(child) * EGG_CYCLE_STEPS;
    egg.ot = "";
    (void)trainer;
    egg.hp = 1;
    egg.friendship = 120;   // a Pokemon hatched from an Egg starts out fond of you
    return true;
}

std::vector<int> DaycareSteps(Profile& p, int steps) {
    std::vector<int> ready;
    if (steps <= 0) return ready;
    // Flame Body, Magma Armor or Steam Engine in the party: Eggs hatch twice as fast
    int mul = 1;
    for (const auto& m : p.mons) {
        if (m.boxed) continue;
        std::string ab = MonAbility(m);
        if (ab == "Flame Body" || ab == "Magma Armor" || ab == "Steam Engine") mul = 2;
    }
    for (int i = 0; i < (int)p.eggs.size(); i++) {
        OwnedMon& e = p.eggs[i];
        if (e.eggSteps <= 0) continue;
        e.eggSteps = std::max(0, e.eggSteps - steps * mul);
        if (e.eggSteps == 0) ready.push_back(i);
    }
    // the Day Care Pokemon: 1 XP a step (they grow, and learn moves - the oldest is forgotten - but don't evolve)
    for (auto& m : p.daycare) {
        Growth g;
        AddXp(m, steps, g);
        for (const auto& n : g.wantsToLearn) {
            if (std::find(m.moves.begin(), m.moves.end(), n) != m.moves.end()) continue;
            if (m.moves.size() >= 4) m.moves.erase(m.moves.begin());
            m.moves.push_back(n);
        }
    }
    // every 256 steps the old man may find an Egg
    if (p.daycare.size() == 2 && !p.daycareEgg) {
        int c = DaycareCompat(p.daycare[0], p.daycare[1]);
        p.daycareSteps += steps;
        while (p.daycareSteps >= EGG_CYCLE_STEPS && !p.daycareEgg) {
            p.daycareSteps -= EGG_CYCLE_STEPS;
            if (c > 0 && R(1, 100) <= c) p.daycareEgg = true;
        }
        if (p.daycareEgg) p.daycareSteps = 0;
    } else p.daycareSteps = 0;
    return ready;
}

std::string EggStatusLine(const OwnedMon& egg) {
    int cyc = egg.eggSteps / EGG_CYCLE_STEPS;
    if (cyc <= 5) return "It's making sounds inside. It's going to hatch soon!";
    if (cyc <= 10) return "It appears to move occasionally. It may be close to hatching.";
    if (cyc <= 40) return "Wonder what's inside? It needs more time, though.";
    return "It looks as if it will take a long time to hatch.";
}

bool HatchEgg(Profile& p, int i, int* monIndex) {
    if (i < 0 || i >= (int)p.eggs.size()) return false;
    OwnedMon m = p.eggs[i];
    p.eggs.erase(p.eggs.begin() + i);
    m.eggSteps = 0;
    m.metAt = "Hatched from an Egg";
    bool boxed = AddMon(p, m);
    DexSee(p, m.species);
    p.dexCaught.insert(m.species);
    if (monIndex) *monIndex = (int)p.mons.size() - 1;
    return boxed;
}

void DrawEgg(float x, float y, float s, float wobble, int species) {
    // a cream Egg with green spots (like the games'), standing on (x, y), tipping side to side when it wobbles
    (void)species;
    rlPushMatrix();
    rlTranslatef(x, y, 0);
    rlRotatef(wobble * RAD2DEG, 0, 0, 1);
    float rx = s * 0.36f, ry = s * 0.46f;
    Color ink{90, 80, 70, 255}, shell{250, 244, 222, 255};
    DrawEllipse(0, (int)(-ry), rx + 1, ry + 1, ink);                 // outline
    DrawEllipse(0, (int)(-ry * 1.08f), rx * 0.86f + 1, ry * 0.9f + 1, ink);
    DrawEllipse(0, (int)(-ry), rx, ry, shell);
    DrawEllipse(0, (int)(-ry * 1.08f), rx * 0.86f, ry * 0.9f, shell);   // pointier on top
    Color spot{120, 196, 110, 255};
    DrawCircleV({-s * 0.12f, -s * 0.66f}, s * 0.07f, spot);
    DrawCircleV({s * 0.15f, -s * 0.5f}, s * 0.09f, spot);
    DrawCircleV({-s * 0.16f, -s * 0.3f}, s * 0.08f, spot);
    DrawCircleV({s * 0.08f, -s * 0.16f}, s * 0.06f, spot);
    DrawCircleV({-s * 0.1f, -s * 0.8f}, s * 0.05f, Fade(WHITE, 0.8f));
    rlPopMatrix();
}
