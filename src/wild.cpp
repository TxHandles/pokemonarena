#include "wild.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <sstream>
#include "embedded.h"
#include "raylib.h"
#include "story.h"

namespace {

std::vector<WildArea> gAreas;
std::map<std::string, std::string> gMethodUnlock;   // method -> story stage
int gCtxArea = -1, gCtxStage = 0, gCtxBadges = 0;

std::string Tr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
std::string Low(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }

// Story index of a stage name: an area opens when the story reaches it.
int StageIndexOf(const std::string& name) {
    if (name.empty()) return 0;
    if (name == "@done") return (int)Story().stages.size();
    std::string l = Low(name);
    for (int i = 0; i < (int)Story().stages.size(); i++)
        if (Low(Story().stages[i].name) == l) return i;
    return 0;   // unknown stage: open from the start
}

bool MethodOpen(const std::string& m, int storyStage) {
    auto it = gMethodUnlock.find(m);
    return it == gMethodUnlock.end() || storyStage >= StageIndexOf(it->second);
}

float MethodShare(const std::string& m) {
    if (m == "walk") return 1.0f;
    if (m == "surf") return 0.5f;
    if (m == "old-rod") return 0.2f;
    if (m == "good-rod") return 0.3f;
    if (m == "super-rod") return 0.35f;
    return 0.25f;   // rock-smash
}

// Weighted entries usable right now in an area (rare-only ones excluded unless asked).
struct Pick { const WildEntry* e; float w; };
std::vector<Pick> Usable(int area, int stage, int badges, bool includeRare) {
    std::vector<Pick> v;
    if (area < 0 || area >= (int)gAreas.size()) return v;
    std::map<std::string, int> total;
    for (const auto& e : gAreas[area].mons)
        if (!e.rare) total[e.method] += e.rate;
    for (const auto& e : gAreas[area].mons) {
        if (e.species < 0 || !MethodOpen(e.method, stage) || badges < e.badges) continue;
        if (e.rare && !includeRare) continue;
        float t = (float)std::max(1, total[e.method]);
        v.push_back({&e, (e.rare ? 5.0f : e.rate) / t * MethodShare(e.method)});
    }
    return v;
}

const char* kDefaultWild = "[Route 1]\nunlock = Route 1\npokemon = walk, Pidgey, 50, 2-5\npokemon = walk, Rattata, 50, 2-4\n";

void Parse(const char* text, std::string& log) {
    gAreas.clear();
    gMethodUnlock.clear();
    std::istringstream ss(text);
    std::string line;
    int ln = 0;
    WildArea* cur = nullptr;
    while (std::getline(ss, line)) {
        ln++;
        line = Tr(line);
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            size_t e = line.find(']');
            gAreas.push_back(WildArea());
            cur = &gAreas.back();
            cur->name = Tr(line.substr(1, e == std::string::npos ? std::string::npos : e - 1));
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Low(Tr(line.substr(0, eq))), v = Tr(line.substr(eq + 1));
        std::vector<std::string> parts;
        std::string part;
        std::istringstream ps(v);
        while (std::getline(ps, part, ',')) parts.push_back(Tr(part));
        if (k == "method_unlock" && parts.size() >= 2) { gMethodUnlock[Low(parts[0])] = parts[1]; continue; }
        if (!cur) continue;
        if (k == "unlock") cur->unlock = v;
        else if (k == "pokemon" && parts.size() >= 4) {
            WildEntry e;
            e.method = Low(parts[0]);
            e.species = SpeciesIndex(parts[1]);
            if (e.species < 0) { log += TextFormat("  wild.txt line %d: unknown Pokemon '%s'\n", ln, parts[1].c_str()); continue; }
            e.rate = std::max(1, std::atoi(parts[2].c_str()));
            e.lo = std::max(1, std::atoi(parts[3].c_str()));
            size_t dash = parts[3].find('-');
            e.hi = dash == std::string::npos ? e.lo : std::max(e.lo, std::atoi(parts[3].c_str() + dash + 1));
            for (size_t i = 4; i < parts.size(); i++) {
                std::string p = Low(parts[i]);
                if (p == "rare") e.rare = true;
                else if (p.rfind("badges", 0) == 0) e.badges = std::atoi(p.c_str() + 6);
            }
            cur->mons.push_back(e);
        }
    }
}

} // namespace

void LoadWild(std::string& log) {
    std::string path = DataDir() + "wild.txt";
    char* text = LoadFileText(path.c_str());
    if (text) {
        Parse(text, log);
        UnloadFileText(text);
        log += TextFormat("Wild: %d areas from wild.txt\n", (int)gAreas.size());
    } else {
        Parse(kDefaultWild, log);
        log += "Wild: built-in Route 1 only (no wild.txt found)\n";
    }
    // Pokemon added through their own pokemon.txt ("wild = Area, rate") join those areas.
    for (int i = 0; i < (int)AllSpecies().size(); i++)
        for (const auto& w : AllSpecies()[i].wildAreas) {
            int a = WildAreaIndex(w.first);
            if (a < 0) { log += "  " + AllSpecies()[i].name + ": unknown wild area '" + w.first + "'\n"; continue; }
            WildEntry e;
            e.species = i;
            e.method = "walk";
            e.rate = std::max(1, w.second);
            e.lo = 2;
            e.hi = 100;
            gAreas[a].mons.push_back(e);
        }
}

const std::vector<WildArea>& WildAreas() { return gAreas; }

int WildAreaIndex(const std::string& name) {
    std::string l = Low(Tr(name));
    for (int i = 0; i < (int)gAreas.size(); i++)
        if (Low(gAreas[i].name) == l) return i;
    return -1;
}

bool WildAreaOpen(int area, int storyStage) {
    return area >= 0 && area < (int)gAreas.size() && storyStage >= StageIndexOf(gAreas[area].unlock);
}

std::vector<int> OpenWildAreas(int storyStage) {
    std::vector<int> v;
    for (int i = 0; i < (int)gAreas.size(); i++)
        if (WildAreaOpen(i, storyStage) && !Usable(i, storyStage, 99, false).empty()) v.push_back(i);
    std::stable_sort(v.begin(), v.end(), [](int a, int b) { return StageIndexOf(gAreas[a].unlock) < StageIndexOf(gAreas[b].unlock); });
    return v;
}

int DefaultWildArea(int storyStage) {
    auto v = OpenWildAreas(storyStage);
    // The newest area that has Pokemon in the grass (not just fishing spots).
    for (int k = (int)v.size() - 1; k >= 0; k--)
        for (const auto& e : gAreas[v[k]].mons)
            if (e.method == "walk") return v[k];
    return v.empty() ? -1 : v.back();
}

int ResolveWildArea(const std::string& chosen, int storyStage) {
    int a = WildAreaIndex(chosen);
    if (a >= 0 && WildAreaOpen(a, storyStage) && !Usable(a, storyStage, 99, false).empty()) return a;
    return DefaultWildArea(storyStage);
}

std::string WildMethodName(const std::string& m) {
    if (m == "walk") return "grass";
    if (m == "surf") return "surfing";
    if (m == "old-rod") return "Old Rod";
    if (m == "good-rod") return "Good Rod";
    if (m == "super-rod") return "Super Rod";
    if (m == "rock-smash") return "Rock Smash";
    return m;
}

std::vector<std::string> WildPlacesFor(int species, int storyStage, int& locked) {
    std::vector<std::string> open;
    locked = 0;
    for (int i = 0; i < (int)gAreas.size(); i++) {
        bool here = false;
        for (const auto& e : gAreas[i].mons) here |= e.species == species;
        if (!here) continue;
        if (WildAreaOpen(i, storyStage)) open.push_back(gAreas[i].name);
        else locked++;
    }
    return open;
}

void SetWildContext(int area, int storyStage, int badges) {
    gCtxArea = area;
    gCtxStage = storyStage;
    gCtxBadges = badges;
}
bool WildActive() { return gCtxArea >= 0 && gCtxArea < (int)gAreas.size(); }
int WildContextArea() { return gCtxArea; }

std::vector<CombatantSpec> MakeWildTeam(int n, int avg) {
    std::vector<CombatantSpec> team;
    auto pool = Usable(gCtxArea, gCtxStage, gCtxBadges, false);
    if (pool.empty()) return team;
    float sum = 0;
    for (const auto& p : pool) sum += p.w;
    std::vector<int> used;
    for (int i = 0; i < n; i++) {
        const WildEntry* pick = nullptr;
        for (int tries = 0; tries < 6 && !pick; tries++) {   // a mixed group if the area allows it
            float r = GetRandomValue(0, 100000) / 100000.0f * sum;
            const WildEntry* e = pool.back().e;
            for (const auto& p : pool) { if (r < p.w) { e = p.e; break; } r -= p.w; }
            if (std::find(used.begin(), used.end(), e->species) == used.end() || tries == 5) pick = e;
        }
        used.push_back(pick->species);
        CombatantSpec c;
        c.species = pick->species;   // as found in the games: no evolving to the team's level
        // Near the team's level, but an early route never turns into a late-game challenge.
        int lvl = avg + GetRandomValue(-1, 1);
        lvl = std::min(lvl, pick->hi + 8);
        lvl = std::max(lvl, pick->lo);
        c.level = std::max(2, std::min(MAX_LEVEL, lvl));
        c.moves = MovesAtLevel(AllSpecies()[c.species], c.level);
        team.push_back(c);
    }
    return team;
}

bool WildAvailableNow(int species, int storyStage, int badges) {
    for (int a = 0; a < (int)gAreas.size(); a++) {
        if (!WildAreaOpen(a, storyStage)) continue;
        for (const auto& p : Usable(a, storyStage, badges, false))
            if (p.e->species == species) return true;
    }
    return false;
}

int PickRareWild(const std::vector<std::string>& owned) {
    auto pool = Usable(gCtxArea, gCtxStage, gCtxBadges, true);
    if (pool.empty()) return -1;
    float sum = 0;
    for (const auto& p : pool) if (!p.e->rare) sum += p.w;
    // Rare = flagged rare, or 10% or less of this area's encounters.
    std::vector<const WildEntry*> rare, fresh;
    for (const auto& p : pool)
        if (p.e->rare || p.w <= sum * 0.10f) rare.push_back(p.e);
    if (rare.empty()) {   // nothing rare here: the least common one
        const Pick* lo = &pool[0];
        for (const auto& p : pool) if (p.w < lo->w) lo = &p;
        rare.push_back(lo->e);
    }
    for (const WildEntry* e : rare)
        if (std::find(owned.begin(), owned.end(), AllSpecies()[e->species].name) == owned.end()) fresh.push_back(e);
    const auto& from = fresh.empty() ? rare : fresh;
    return from[GetRandomValue(0, (int)from.size() - 1)]->species;
}
