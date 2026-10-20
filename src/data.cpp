#include "data.h"
#include "natures.h"
#include <algorithm>
#include "raylib.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <utility>

namespace {

struct L { int level; const char* move; };

MoveDef MoveOrDie(const char* name) {
    const MoveDef* m = FindMove(name);
    if (!m) {
        std::fprintf(stderr, "Unknown move in species data: %s\n", name);
        return MakeMove(name, Type::Normal, MoveKind::Projectile, 40, 1.0f);
    }
    return *m;
}

std::vector<SpeciesDef> BuildSpecies() {
    using T = Type;
    std::vector<SpeciesDef> v;
    auto add = [&](const char* name, T t1, T t2, int hp, int atk, int def, int spd, bool floats,
                   std::initializer_list<const char*> moves, std::initializer_list<L> learn,
                   const char* evolvesTo = "", int evolveLevel = 0, float size = 1.0f, const char* art = nullptr) {
        SpeciesDef s;
        s.name = name; s.type1 = t1; s.type2 = t2;
        s.hp = hp; s.atk = atk; s.def = def; s.spd = spd; s.floats = floats;
        for (const char* m : moves) s.moves.push_back(MoveOrDie(m));
        for (const L& l : learn) s.learnset.push_back({l.level, MoveOrDie(l.move)});
        s.evolvesTo = evolvesTo;
        s.evolveLevel = evolveLevel;
        s.size = size;
        s.artName = art ? art : name;
        v.push_back(s);
    };
    const T N = T::None;

    //   name          types               HP  ATK DEF SPD floats
    add("Furbit", T::Normal, N, 95, 80, 70, 85, false,
        {"Quick Attack", "Swift", "Body Slam", "Bite"},
        {{1, "Tackle"}, {1, "Growl"}, {6, "Quick Attack"}, {10, "Swift"}, {15, "Headbutt"}, {20, "Bite"},
         {26, "Body Slam"}, {32, "Swords Dance"}, {38, "Slash"}, {45, "Double-Edge"}, {55, "Hyper Beam"}});

    add("Cindercub", T::Fire, N, 70, 85, 55, 90, false,
        {"Ember", "Flame Wheel", "Flamethrower", "Will-O-Wisp"},
        {{1, "Scratch"}, {1, "Growl"}, {5, "Ember"}, {10, "Quick Attack"}, {14, "Fire Fang"}, {19, "Flame Wheel"},
         {24, "Will-O-Wisp"}, {30, "Fire Spin"}, {36, "Flamethrower"}, {44, "Heat Wave"}, {52, "Flare Blitz"}},
        "Blazelynx", 16);

    add("Ripplit", T::Water, N, 90, 65, 75, 65, false,
        {"Water Gun", "Bubble Beam", "Surf", "Aqua Ring"},
        {{1, "Tackle"}, {1, "Growl"}, {5, "Water Gun"}, {9, "Bubble"}, {13, "Aqua Jet"}, {18, "Water Pulse"},
         {23, "Bubble Beam"}, {29, "Aqua Ring"}, {35, "Surf"}, {42, "Aqua Tail"}, {50, "Hydro Pump"}},
        "Torrentoad", 16);

    add("Joltelly", T::Electric, N, 65, 84, 50, 100, true,
        {"Thunder Shock", "Thunderbolt", "Discharge", "Thunder Wave"},
        {{1, "Thunder Shock"}, {1, "Charge"}, {6, "Thunder Wave"}, {11, "Spark"}, {16, "Shock Wave"},
         {21, "Electro Ball"}, {27, "Discharge"}, {33, "Thunderbolt"}, {40, "Confuse Ray"}, {48, "Thunder"}});

    add("Yetiny", T::Ice, N, 95, 92, 80, 65, false,
        {"Ice Shard", "Ice Punch", "Avalanche", "Blizzard"},
        {{1, "Powder Snow"}, {1, "Growl"}, {6, "Ice Shard"}, {11, "Icy Wind"}, {16, "Ice Punch"}, {22, "Avalanche"},
         {28, "Aurora Beam"}, {34, "Ice Beam"}, {42, "Freeze-Dry"}, {50, "Blizzard"}});

    add("Punchip", T::Fighting, N, 75, 95, 60, 85, false,
        {"Mach Punch", "Brick Break", "Aura Sphere", "Bulk Up"},
        {{1, "Karate Chop"}, {1, "Leer"}, {6, "Mach Punch"}, {10, "Double Kick"}, {15, "Low Kick"},
         {20, "Vacuum Wave"}, {25, "Brick Break"}, {30, "Bulk Up"}, {36, "Aura Sphere"}, {42, "Cross Chop"},
         {50, "Close Combat"}, {56, "Detect"}});

    add("Toxlug", T::Poison, N, 95, 70, 80, 50, false,
        {"Sludge", "Acid", "Smog", "Sludge Bomb"},
        {{1, "Poison Sting"}, {1, "Harden"}, {6, "Acid"}, {11, "Smog"}, {16, "Sludge"}, {22, "Toxic"},
         {28, "Venoshock"}, {34, "Sludge Bomb"}, {40, "Acid Armor"}, {48, "Gunk Shot"}});

    add("Dustadillo", T::Ground, N, 85, 80, 100, 45, false,
        {"Mud Shot", "Dig", "Earthquake", "Harden"},
        {{1, "Tackle"}, {1, "Harden"}, {5, "Mud-Slap"}, {10, "Sand Tomb"}, {14, "Mud Shot"}, {19, "Bulldoze"},
         {24, "Dig"}, {30, "Magnitude"}, {36, "Earth Power"}, {44, "Earthquake"}});

    add("Gustling", T::Flying, N, 70, 75, 55, 110, false,
        {"Gust", "Aerial Ace", "Air Slash", "Brave Bird"},
        {{1, "Peck"}, {1, "Growl"}, {5, "Gust"}, {9, "Quick Attack"}, {13, "Wing Attack"}, {18, "Air Cutter"},
         {23, "Aerial Ace"}, {28, "Tailwind"}, {33, "Air Slash"}, {38, "Fly"}, {44, "Roost"}, {50, "Brave Bird"},
         {56, "Hurricane"}});

    add("Mystling", T::Psychic, N, 75, 95, 60, 80, true,
        {"Confusion", "Psybeam", "Psychic", "Calm Mind"},
        {{1, "Confusion"}, {1, "Teleport"}, {6, "Hypnosis"}, {11, "Psybeam"}, {16, "Calm Mind"},
         {22, "Zen Headbutt"}, {28, "Psychic"}, {34, "Future Sight"}, {40, "Rest"}, {46, "Agility"}});

    add("Beetlit", T::Bug, N, 80, 85, 90, 55, false,
        {"Pin Missile", "X-Scissor", "Bug Buzz", "Megahorn"},
        {{1, "Bug Bite"}, {1, "String Shot"}, {6, "Twineedle"}, {11, "Fury Cutter"}, {16, "Pin Missile"},
         {21, "X-Scissor"}, {27, "Signal Beam"}, {33, "Iron Defense"}, {40, "Bug Buzz"}, {48, "Megahorn"}});

    add("Pebblor", T::Rock, N, 90, 90, 110, 35, false,
        {"Rock Throw", "Rock Slide", "Stone Edge", "Rock Tomb"},
        {{1, "Tackle"}, {1, "Harden"}, {6, "Rock Throw"}, {11, "Rock Tomb"}, {16, "Rock Blast"},
         {21, "Ancient Power"}, {25, "Rock Polish"}, {27, "Rock Slide"}, {33, "Stealth Rock"}, {39, "Stone Edge"},
         {47, "Head Smash"}});

    add("Wispurr", T::Ghost, N, 70, 90, 60, 90, true,
        {"Night Shade", "Shadow Ball", "Hex", "Dark Pulse"},
        {{1, "Lick"}, {1, "Confuse Ray"}, {6, "Night Shade"}, {11, "Shadow Sneak"}, {16, "Hex"}, {21, "Bite"},
         {27, "Shadow Ball"}, {33, "Ominous Wind"}, {39, "Dark Pulse"}, {45, "Phantom Force"}});

    add("Drakelet", T::Dragon, N, 80, 88, 70, 75, false,
        {"Dragon Breath", "Dragon Claw", "Dragon Pulse", "Draco Meteor"},
        {{1, "Dragon Rage"}, {1, "Leer"}, {6, "Twister"}, {12, "Dragon Breath"}, {17, "Dragon Claw"},
         {23, "Dragon Tail"}, {29, "Dragon Pulse"}, {35, "Dragon Dance"}, {41, "Dragon Rush"}, {48, "Outrage"},
         {56, "Draco Meteor"}});

    add("Murkoon", T::Dark, N, 75, 95, 60, 95, false,
        {"Bite", "Feint Attack", "Sucker Punch", "Night Slash"},
        {{1, "Scratch"}, {1, "Leer"}, {5, "Bite"}, {10, "Pursuit"}, {15, "Feint Attack"}, {20, "Snarl"},
         {26, "Sucker Punch"}, {31, "Night Slash"}, {37, "Nasty Plot"}, {43, "Crunch"}, {50, "Dark Pulse"}});

    add("Gearbit", T::Steel, N, 75, 78, 105, 50, false,
        {"Magnet Bomb", "Gyro Ball", "Flash Cannon", "Iron Defense"},
        {{1, "Metal Claw"}, {1, "Metal Sound"}, {6, "Bullet Punch"}, {11, "Magnet Bomb"}, {16, "Mirror Shot"},
         {22, "Gyro Ball"}, {28, "Flash Cannon"}, {34, "Iron Defense"}, {40, "Iron Head"}, {48, "Meteor Mash"}});

    add("Glimmerbun", T::Fairy, N, 85, 75, 75, 80, false,
        {"Fairy Wind", "Dazzling Gleam", "Moonblast", "Moonlight"},
        {{1, "Fairy Wind"}, {1, "Charm"}, {6, "Disarming Voice"}, {11, "Sweet Kiss"}, {16, "Draining Kiss"},
         {21, "Quick Attack"}, {27, "Dazzling Gleam"}, {33, "Moonlight"}, {40, "Play Rough"}, {48, "Moonblast"}});

    // Test evolutions (placeholder art: recolored and bigger).
    add("Blazelynx", T::Fire, N, 85, 110, 70, 110, false,
        {"Ember", "Flamethrower", "Flare Blitz", "Heat Wave"},
        {{1, "Ember"}, {1, "Scratch"}, {16, "Slash"}, {22, "Flame Wheel"}, {28, "Will-O-Wisp"}, {34, "Flamethrower"},
         {40, "Fire Spin"}, {46, "Heat Wave"}, {54, "Flare Blitz"}, {60, "Fire Blast"}},
        "", 0, 1.2f);
    add("Torrentoad", T::Water, N, 110, 85, 95, 75, false,
        {"Water Pulse", "Surf", "Hydro Pump", "Dive"},
        {{1, "Water Gun"}, {1, "Bubble"}, {16, "Bite"}, {24, "Water Pulse"}, {30, "Dive"}, {36, "Surf"},
         {44, "Aqua Tail"}, {52, "Hydro Pump"}, {58, "Ice Beam"}},
        "", 0, 1.2f);
    for (auto& sp : v) {
        // The built-ins were designed on the old scale: convert them to base-stat numbers.
        auto conv = [](int x) { return std::max(1, (int)std::lround(x / STAT_SCALE)); };
        sp.hp = conv(sp.hp); sp.atk = conv(sp.atk); sp.def = conv(sp.def); sp.spd = conv(sp.spd);
        if (sp.name == "Dustadillo" || sp.name == "Pebblor" || sp.name == "Yetiny" || sp.name == "Gearbit") sp.heavy = true;
        if (sp.name == "Drakelet") sp.winged = true;
        if (sp.name == "Drakelet" || sp.name == "Wispurr" || sp.name == "Mystling" || sp.name == "Glimmerbun" ||
            sp.name == "Gearbit")
            sp.rare = true;
    }
    return v;
}

} // namespace

bool IsHeavy(const SpeciesDef& sp) { return sp.heavy || sp.size >= 1.15f; }
bool IsAirborne(const SpeciesDef& sp) {
    return sp.floats || sp.winged || sp.type1 == Type::Flying || sp.type2 == Type::Flying;
}

std::vector<SpeciesDef>& SpeciesRegistry() {
    static std::vector<SpeciesDef> species = BuildSpecies();
    return species;
}

const std::vector<SpeciesDef>& AllSpecies() { return SpeciesRegistry(); }

const std::vector<SpeciesDef>& BuiltinSpeciesList() {
    static const std::vector<SpeciesDef> b = BuildSpecies();
    return b;
}

int SpeciesIndex(const std::string& name) {
    std::string k = Squash(name);
    if (k.empty()) return -1;
    const auto& all = AllSpecies();
    for (size_t i = 0; i < all.size(); i++)
        if (Squash(all[i].name) == k) return (int)i;
    // A renamed built-in slot still answers to its original name (e.g. "Thistlet").
    for (size_t i = 0; i < all.size(); i++)
        if (Squash(all[i].artName) == k) return (int)i;
    return -1;
}

int XpForLevel(int level) {
    if (level <= 1) return 0;
    return level * level * level;
}

float StatMul(int level) { return 0.5f + std::max(1, std::min(MAX_LEVEL, level)) / 100.0f; }

int XpYield(const SpeciesDef& sp) {
    return sp.xpYield > 0 ? sp.xpYield : (int)std::lround(Scaled(sp.hp + sp.atk + sp.def + sp.spd) / 5);
}

int KoXp(const SpeciesDef& d, int lvl, bool trainer) {
    float xp = XpYield(d) * lvl / 7.0f * (trainer ? 1.5f : 1.0f);
    return std::max(1, (int)std::lround(xp));
}

std::vector<MoveDef> MovesAtLevel(const SpeciesDef& sp, int level) {
    if (sp.learnset.empty()) return sp.moves;
    std::vector<LearnEntry> ls = sp.learnset;
    std::stable_sort(ls.begin(), ls.end(), [](const LearnEntry& a, const LearnEntry& b) { return a.level < b.level; });
    std::vector<MoveDef> out;
    for (const auto& e : ls) {
        if (e.level <= 0) continue;   // only learned when evolving
        if (e.level > level) break;
        out.erase(std::remove_if(out.begin(), out.end(), [&](const MoveDef& m) { return m.name == e.move.name; }), out.end());
        out.push_back(e.move);
        if (out.size() > 4) out.erase(out.begin());
    }
    if (out.empty() && !sp.moves.empty()) out.push_back(sp.moves[0]);
    return out;
}

std::vector<MoveDef> MovesLearnedAt(const SpeciesDef& sp, int level, bool evolving) {
    std::vector<MoveDef> out;
    if (evolving)
        for (const auto& e : sp.learnset)
            if (e.level == 0) out.push_back(e.move);
    for (const auto& e : sp.learnset)
        if (e.level == level && level > 0) out.push_back(e.move);
    return out;
}

std::vector<MoveDef> FreePlayMoves(const SpeciesDef& sp) {
    if (!sp.moves.empty()) return std::vector<MoveDef>(sp.moves.begin(), sp.moves.begin() + std::min<size_t>(4, sp.moves.size()));
    return MovesAtLevel(sp, FREE_PLAY_LEVEL);
}

int EvolutionIndexFor(int idx, int nature) {
    int e = EvolutionIndex(idx);
    if (e < 0) return e;
    const SpeciesDef& sp = AllSpecies()[idx];
    if (sp.statEvos.size() != 3) return e;
    // Attack vs Defense: the base stats are equal for Tyrogue, so its nature decides.
    float atk = sp.atk * (nature >= 0 ? NatureMul(nature, NS_ATK) : 1.0f);
    float def = sp.def * (nature >= 0 ? NatureMul(nature, NS_DEF) : 1.0f);
    int pick = atk > def + 0.01f ? 0 : (def > atk + 0.01f ? 1 : 2);
    int t = SpeciesIndex(sp.statEvos[pick]);
    return t >= 0 && t != idx ? t : e;
}

int EvolutionIndex(int idx) {
    if (idx < 0 || idx >= (int)AllSpecies().size()) return -1;
    const SpeciesDef& sp = AllSpecies()[idx];
    if (sp.evolvesTo.empty() || sp.evolveLevel <= 0) return -1;
    int e = SpeciesIndex(sp.evolvesTo);
    return e == idx ? -1 : e;
}

namespace {
// Which species something evolves from (by any method). Rebuilt if the registry changes size.
const std::vector<int>& PreEvolutions() {
    static std::vector<int> pre;
    static size_t builtFor = 0;
    const auto& all = AllSpecies();
    if (builtFor != all.size() || pre.size() != all.size()) {
        pre.assign(all.size(), -1);
        for (int i = 0; i < (int)all.size(); i++) {
            int e = EvolutionIndex(i);
            if (e >= 0 && e != i) pre[e] = i;
            for (const auto& a : all[i].evoAlts) {
                int t = SpeciesIndex(a.to);
                if (t >= 0 && t != i) pre[t] = i;
            }
            for (const auto& n : all[i].statEvos) {
                int t = SpeciesIndex(n);
                if (t >= 0 && t != i) pre[t] = i;
            }
        }
        builtFor = all.size();
    }
    return pre;
}
} // namespace

bool IsBaseForm(int idx) {
    const auto& pre = PreEvolutions();
    return idx < 0 || idx >= (int)pre.size() || pre[idx] < 0;
}

int BaseFormOf(int idx) {
    const auto& pre = PreEvolutions();
    for (int g = 0; g < 8 && idx >= 0 && idx < (int)pre.size() && pre[idx] >= 0; g++) idx = pre[idx];
    return idx;
}

bool InSpeciesPool(int idx) {
    // Once the Pokemon folder holds a real roster, the built-in test creatures stop showing up
    // as opponents, rewards and free-play picks (ones you already own still work).
    static int customCount = -1;
    static size_t builtFor = 0;
    const auto& all = AllSpecies();
    if (customCount < 0 || builtFor != all.size()) {
        customCount = 0;
        for (const auto& s : all) customCount += s.custom;
        builtFor = all.size();
    }
    if (idx < 0 || idx >= (int)all.size()) return false;
    return customCount < 20 || all[idx].custom;
}

bool CanEvolve(int idx) {
    if (EvolutionIndex(idx) >= 0) return true;
    if (idx < 0 || idx >= (int)AllSpecies().size()) return false;
    for (const auto& e : AllSpecies()[idx].evoAlts)
        if (SpeciesIndex(e.to) >= 0 && SpeciesIndex(e.to) != idx) return true;
    return false;
}

int StoneEvolution(int idx, const std::string& item) {
    if (idx < 0 || idx >= (int)AllSpecies().size()) return -1;
    if (Squash(item) == "linkingcord") return TradeEvolution(idx);   // Legends: Arceus' stand-in for trading
    for (const auto& e : AllSpecies()[idx].evoAlts)
        if (!e.item.empty() && Squash(e.item) == Squash(item)) {
            int t = SpeciesIndex(e.to);
            if (t >= 0 && t != idx) return t;
        }
    return -1;
}

int FriendshipEvolution(int idx, int friendship, int night) {
    if (idx < 0 || idx >= (int)AllSpecies().size()) return -1;
    for (const auto& e : AllSpecies()[idx].evoAlts)
        if (e.friendship > 0 && friendship >= e.friendship && !(night == 1 && e.time == 1) && !(night == 0 && e.time == 2)) {
            int t = SpeciesIndex(e.to);
            if (t >= 0 && t != idx) return t;
        }
    return -1;
}

std::vector<std::string> EvolutionStonesFor(int idx) {
    std::vector<std::string> v;
    if (idx < 0 || idx >= (int)AllSpecies().size()) return v;
    for (const auto& e : AllSpecies()[idx].evoAlts)
        if (!e.item.empty() && SpeciesIndex(e.to) >= 0 && std::find(v.begin(), v.end(), e.item) == v.end()) v.push_back(e.item);
    if (TradeEvolution(idx) >= 0) v.push_back("Linking Cord");
    return v;
}

int TradeEvolution(int idx) {
    if (idx < 0 || idx >= (int)AllSpecies().size()) return -1;
    for (const auto& e : AllSpecies()[idx].evoAlts)
        if (e.trade) {
            int t = SpeciesIndex(e.to);
            if (t >= 0 && t != idx) return t;
        }
    return -1;
}

std::string SpeciesAbilityName(int idx, int slot) {
    if (idx < 0 || idx >= (int)AllSpecies().size()) return "";
    const SpeciesDef& sp = AllSpecies()[idx];
    if (slot == 2 && !sp.hiddenAbility.empty()) return sp.hiddenAbility;
    if (slot == 1 && !sp.ability2.empty()) return sp.ability2;
    return sp.ability1;
}

int AbilitySlotCount(int idx) {
    if (idx < 0 || idx >= (int)AllSpecies().size()) return 1;
    return AllSpecies()[idx].ability2.empty() ? 1 : 2;
}

namespace {
struct DexRow { const char* name; const char* category; int h, w; const char* entry; const char* a1; const char* a2; const char* hidden; };
const DexRow kDex[] = {
#include "dexdata.inc"
};
} // namespace

void ApplyDexData() {
    auto& reg = SpeciesRegistry();
    for (const DexRow& r : kDex) {
        int idx = SpeciesIndex(r.name);
        if (idx < 0) continue;
        SpeciesDef& sp = reg[idx];
        if (sp.category.empty()) sp.category = r.category;
        if (sp.dexEntry.empty()) sp.dexEntry = r.entry;
        if (sp.heightDm <= 0) sp.heightDm = r.h;
        if (sp.weightHg <= 0) sp.weightHg = r.w;
        if (sp.ability1.empty()) { sp.ability1 = r.a1; sp.ability2 = r.a2; }
        if (sp.hiddenAbility.empty()) sp.hiddenAbility = r.hidden;
    }
    // Trade evolutions (Kadabra, Machoke, Graveler, Haunter). Older data files had them evolve by
    // level; they now evolve when traded to your co-op partner.
    static const char* kTrade[][2] = {{"Kadabra", "Alakazam"}, {"Machoke", "Machamp"}, {"Graveler", "Golem"}, {"Haunter", "Gengar"}};
    for (auto& t : kTrade) {
        int idx = SpeciesIndex(t[0]);
        if (idx < 0) continue;
        SpeciesDef& sp = reg[idx];
        bool has = false;
        for (const auto& e : sp.evoAlts) has |= e.trade;
        if (Squash(sp.evolvesTo) == Squash(t[1])) { sp.evolvesTo.clear(); sp.evolveLevel = 0; }
        if (!has) { EvoAlt e; e.to = t[1]; e.trade = true; sp.evoAlts.push_back(e); }
    }
    // How big each Pokemon looks in battle (and how big a target it is): from its Pokedex height
    // and weight, so Onix towers over Pikachu. Stats and the "heavy" rule don't change.
    for (SpeciesDef& sp : reg) {
        if (sp.spriteSize > 0) sp.drawScale = sp.spriteSize;
        else if (sp.heightDm > 0 && sp.weightHg > 0) {
            float h = sp.heightDm / 10.0f, w = sp.weightHg / 10.0f;
            sp.drawScale = std::max(0.6f, std::min(1.7f, 0.65f * std::pow(h, 0.35f) + 0.35f * std::pow(w / 20.0f, 0.2f)));
        } else sp.drawScale = sp.size;
    }
}

std::string EvolutionHint(int idx) {
    if (idx < 0 || idx >= (int)AllSpecies().size()) return "";
    const SpeciesDef& sp = AllSpecies()[idx];
    std::vector<std::string> parts;
    int e = EvolutionIndex(idx);
    if (e >= 0 && sp.statEvos.size() == 3)
        parts.push_back(sp.statEvos[0] + ", " + sp.statEvos[1] + " or " + sp.statEvos[2] + " at level " + std::to_string(sp.evolveLevel) + " (whichever suits its Attack and Defense)");
    else if (e >= 0) parts.push_back(AllSpecies()[e].name + " at level " + std::to_string(sp.evolveLevel));
    for (const auto& a : sp.evoAlts) {
        if (SpeciesIndex(a.to) < 0) continue;
        if (!a.item.empty()) parts.push_back(a.to + " with a " + a.item);
        else if (a.friendship > 0) parts.push_back(a.to + " when very friendly" + (a.time == 1 ? " during the day" : (a.time == 2 ? " at night" : "")));
        else if (a.trade) parts.push_back(a.to + " when traded (or with a Linking Cord)");
    }
    if (parts.empty()) return "";
    std::string s = "Evolves into ";
    for (size_t i = 0; i < parts.size(); i++) s += (i ? (i + 1 == parts.size() ? " or " : ", ") : "") + parts[i];
    return s;
}

// CPU creatures: wild and trainer creatures above these levels have used their stone or friendship.
constexpr int CPU_STONE_LEVEL = 32;
constexpr int CPU_FRIEND_LEVEL = 28;
constexpr int CPU_TRADE_LEVEL = 38;

int EvolvedFormAt(int idx, int level) {
    for (int guard = 0; guard < 8; guard++) {
        int e = EvolutionIndex(idx);
        if (e >= 0 && level >= AllSpecies()[idx].evolveLevel) {
            const auto& se = AllSpecies()[idx].statEvos;   // Tyrogue: any of the three
            int pick = se.size() == 3 ? SpeciesIndex(se[GetRandomValue(0, 2)]) : -1;
            idx = pick >= 0 ? pick : e;
            continue;
        }
        std::vector<int> opts;
        for (const auto& a : AllSpecies()[idx].evoAlts) {
            int t = SpeciesIndex(a.to);
            if (t < 0 || t == idx) continue;
            if ((!a.item.empty() && level >= CPU_STONE_LEVEL) || (a.friendship > 0 && level >= CPU_FRIEND_LEVEL) || (a.trade && level >= CPU_TRADE_LEVEL)) opts.push_back(t);
        }
        if (opts.empty()) break;
        idx = opts[GetRandomValue(0, (int)opts.size() - 1)];
    }
    return idx;
}
