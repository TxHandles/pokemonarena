#include "creatures.h"
#include "embedded.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <vector>
#include "data.h"
#include "raylib.h"

namespace {

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string Lower(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<std::string> Split(const std::string& s, const std::string& seps) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (seps.find(c) != std::string::npos) { out.push_back(Trim(cur)); cur.clear(); }
        else cur += c;
    }
    out.push_back(Trim(cur));
    return out;
}

bool ParseBool(const std::string& v) {
    std::string l = Lower(v);
    return l == "yes" || l == "true" || l == "1" || l == "y" || l == "on";
}

int FindSpecies(const std::string& name) { return SpeciesIndex(Trim(name)); }

bool HasImages(const std::string& dir) {
    FilePathList f = LoadDirectoryFiles(dir.c_str());
    bool any = false;
    for (unsigned i = 0; i < f.count && !any; i++)
        any = IsPathFile(f.paths[i]) && IsFileExtension(f.paths[i], ".png;.PNG;.jpg;.JPG;.jpeg;.JPEG;.bmp;.BMP;.gif;.GIF;.qoi");
    UnloadDirectoryFiles(f);
    return any;
}

// Parse "Name" (library lookup) or "Name, Type, Kind, Power, Cooldown [, extras...]".
bool ParseMoveSpec(const std::string& val, MoveDef& out, std::string& err) {
    if (val.find(',') == std::string::npos) {
        const MoveDef* m = FindMove(val);
        if (!m) { err = "no move called '" + val + "' (see MOVE-LIST.txt)"; return false; }
        out = *m;
        return true;
    }
    std::vector<std::string> f = Split(val, ",");
    Type t;
    MoveKind k;
    if (f.size() < 5 || !ParseType(f[1], t) || t == Type::None || !ParseMoveKind(f[2], k)) {
        err = "custom move should be 'Name, Type, Kind, Power, Cooldown': " + val;
        return false;
    }
    MoveDef m = MakeMove(f[0], t, k, std::max(0, std::atoi(f[3].c_str())), std::max(0.2f, (float)std::atof(f[4].c_str())));
    for (size_t i = 5; i < f.size(); i++) {
        std::vector<std::string> w = Split(Lower(f[i]), " ");
        w.erase(std::remove(w.begin(), w.end(), std::string()), w.end());
        if (w.empty()) continue;
        auto pct = [&](size_t idx, float def) {
            if (idx >= w.size()) return def;
            float v = (float)std::atof(w[idx].c_str());
            return v > 1.0f ? v / 100.0f : (v > 0 ? v : def);
        };
        auto num = [&](size_t idx, float def) { return idx < w.size() ? (float)std::atof(w[idx].c_str()) : def; };
        Status st;
        const std::string& a = w[0];
        if (ParseStatus(a, st)) { m.status = st; m.statusChance = pct(1, 1.0f); }
        else if (a == "drain") m.drain = pct(1, 0.5f);
        else if (a == "recoil") m.recoil = pct(1, 0.33f);
        else if (a == "homing") m.homing = num(1, 5);
        else if (a == "crit") m.highCrit = true;
        else if (a == "physical") m.cat = MoveCat::Physical;
        else if (a == "special") m.cat = MoveCat::Special;
        else if (a == "hits") m.hits = std::max(1, std::min(6, (int)num(1, 2)));
        else if (a == "quick") m.quick = true;
        else if (a == "knock" || a == "knockback") m.knock = num(1, 2);
        else if (a == "range") m.range = num(1, m.range);
        else if (a == "radius") m.radius = num(1, m.radius);
        else if (a == "count") m.count = std::max(1, (int)num(1, 5));
        else if (a == "orbs" || a == "segments") m.count = std::max(1, std::min(8, (int)num(1, 3)));
        else if (a == "bounce" || a == "bounces" || a == "ricochet") m.bounces = std::max(1, std::min(6, (int)num(1, 2)));
        else if (a == "pull") m.amount = num(1, 70);
        else if (a == "rain") m.weather = Weather::Rain;
        else if (a == "sun") m.weather = Weather::Sun;
        else if (a == "sand" || a == "sandstorm") m.weather = Weather::Sand;
        else if (a == "hail" || a == "snow") m.weather = Weather::Snow;
        else if (a == "ohko") m.ohko = true;
        else if (a == "selfko" || a == "explode") m.selfKO = true;
        else if (a == "force" || a == "forceswap") m.forceSwap = true;
        else if (a == "falseswipe" || a == "nokill") m.falseSwipe = true;
        else if (a == "fixed") m.fixedDmg = std::max(1, (int)num(1, 40));
        else if (a == "leveldamage") m.fixedDmg = -1;
        else if (a == "halfhp") m.fixedDmg = -2;
        else if (a == "counter") m.counter = true;
        else if (a == "cure") m.cure = true;
        else if (a == "duration" || a == "time") m.duration = num(1, m.duration);
        else if (a == "uses") m.maxUses = std::max(0, (int)num(1, 2));
        else if (a == "patch") { m.patchPower = std::max(1, m.power / 5); m.patchTime = 2.5f; m.patchRadius = 14; }
        else if (a == "hex") m.hex = true;
        else if (a == "lower" && w.size() >= 2) {
            m.lowers = true;
            m.lowerStat = w[1] == "def" || w[1] == "defense" ? StatId::Def : (w[1] == "spd" || w[1] == "speed" ? StatId::Spd : StatId::Atk);
            m.lowerChance = pct(2, 1.0f);
        } else if (a == "atk" || a == "attack") m.stat = StatId::Atk;
        else if (a == "def" || a == "defense") m.stat = StatId::Def;
        else if (a == "spd" || a == "speed") m.stat = StatId::Spd;
        else if (a == "heal") m.amount = pct(1, 0.3f);
        else if (k == MoveKind::Heal && std::atof(a.c_str()) > 0) m.amount = std::min(1.0f, (float)std::atof(a.c_str()) / 100.0f);
        else { err = "unknown move extra '" + f[i] + "'"; }
    }
    out = m;
    return true;
}

// Parse one creature.txt into 'sp'. Returns false if it is unusable.
bool ParseCreatureFile(const std::string& path, SpeciesDef& sp, std::string& replaces, std::string& log) {
    char* text = LoadFileText(path.c_str());
    if (!text) { log += "    could not read Pokemon.txt\n"; return false; }
    std::istringstream ss(text);
    UnloadFileText(text);

    std::vector<MoveDef> moves;
    std::vector<LearnEntry> learn;
    int spAtk = 0, spDef = 0;  // optional: averaged into attack/defense
    bool typeSet = false, sawLearn = false;
    std::string line;
    int ln = 0;
    while (std::getline(ss, line)) {
        ln++;
        size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = Trim(line);
        if (line.empty()) continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) eq = line.find(':');
        if (eq == std::string::npos) {
            log += TextFormat("    line %d: expected 'key = value', ignored: %s\n", ln, line.c_str());
            continue;
        }
        std::string key = Lower(Trim(line.substr(0, eq)));
        std::string val = Trim(line.substr(eq + 1));
        auto num = [&](int& dst) {
            int v = std::atoi(val.c_str());
            if (v <= 0) log += TextFormat("    line %d: '%s' needs a number above 0\n", ln, key.c_str());
            else dst = std::min(v, 255);
        };

        if (key == "name") sp.name = val;
        else if (key == "replaces" || key == "replace" || key == "slot") replaces = val;
        else if (key == "type" || key == "types" || key == "type1") {
            std::vector<std::string> parts = Split(val, "/,");
            Type t;
            if (ParseType(parts[0], t) && t != Type::None) { sp.type1 = t; typeSet = true; }
            else log += TextFormat("    line %d: unknown type '%s'\n", ln, parts[0].c_str());
            sp.type2 = Type::None;
            if (parts.size() > 1) {
                if (ParseType(parts[1], t)) sp.type2 = t;
                else log += TextFormat("    line %d: unknown type '%s'\n", ln, parts[1].c_str());
            }
        } else if (key == "type2") {
            Type t;
            if (ParseType(val, t)) sp.type2 = t;
            else log += TextFormat("    line %d: unknown type '%s'\n", ln, val.c_str());
        } else if (key == "hp") num(sp.hp);
        else if (key == "attack" || key == "atk") num(sp.atk);
        else if (key == "defense" || key == "defence" || key == "def") num(sp.def);
        else if (key == "speed" || key == "spd") num(sp.spd);
        else if (key == "sp_attack" || key == "special_attack" || key == "spatk" || key == "sp_atk") num(spAtk);
        else if (key == "sp_defense" || key == "special_defense" || key == "spdef" || key == "sp_def") num(spDef);
        else if (key == "starter") sp.starter = ParseBool(val);
        else if (key == "hovers" || key == "floats" || key == "hover") sp.floats = ParseBool(val);
        else if (key == "heavy" || key == "large" || key == "big") sp.heavy = ParseBool(val);
        else if (key == "rare") sp.rare = ParseBool(val);
        else if (key == "legendary" || key == "mythical") sp.legendary = ParseBool(val);
        else if (key == "dex" || key == "number" || key == "dex_number") sp.dex = std::max(0, std::atoi(val.c_str()));
        else if (key == "placeholder_art" || key == "art") sp.artName = val;
        else if (key == "winged" || key == "wings" || key == "flies") sp.winged = ParseBool(val);
        else if (key == "size") {
            float s = (float)std::atof(val.c_str());
            if (s >= 0.25f && s <= 4.0f) sp.size = s;
            else log += TextFormat("    line %d: size should be between 0.25 and 4\n", ln);
        } else if (key == "evolves_to" || key == "evolves_into" || key == "evolution" || key == "evolves") {
            sp.evolvesTo = (Lower(val) == "none" || Lower(val) == "nothing") ? "" : val;
        } else if (key == "evolve_item" || key == "evolve_stone" || key == "evolve_friendship") {
            // evolve_item = Vaporeon, Water Stone      evolve_friendship = Crobat, 160
            size_t c = val.find(',');
            EvoAlt e;
            e.to = Trim(val.substr(0, c));
            std::string arg = c == std::string::npos ? "" : Trim(val.substr(c + 1));
            if (key == "evolve_friendship") {   // evolve_friendship = Espeon, 160, day
                size_t c2 = arg.find(',');
                std::string when = c2 == std::string::npos ? "" : Lower(Trim(arg.substr(c2 + 1)));
                if (c2 != std::string::npos) arg = Trim(arg.substr(0, c2));
                e.friendship = std::max(1, std::min(MAX_FRIENDSHIP, arg.empty() ? 160 : std::atoi(arg.c_str())));
                e.time = when == "day" ? 1 : (when == "night" ? 2 : 0);
            }
            else e.item = arg;
            if (e.to.empty() || (key != "evolve_friendship" && e.item.empty()))
                log += TextFormat("    line %d: write it like  %s = Vaporeon, Water Stone\n", ln, key.c_str());
            else if (Lower(e.to) != "none")
                sp.evoAlts.push_back(e);
        } else if (key == "evolve_by_stats" || key == "evolve_stats") {
            // evolve_by_stats = Hitmonlee, Hitmonchan, Hitmontop   (Attack higher, Defense higher, equal)
            sp.statEvos.clear();
            size_t p = 0;
            while (p <= val.size()) {
                size_t c = val.find(',', p);
                std::string nm = Trim(val.substr(p, c == std::string::npos ? std::string::npos : c - p));
                if (!nm.empty()) sp.statEvos.push_back(nm);
                if (c == std::string::npos) break;
                p = c + 1;
            }
            if (sp.statEvos.size() != 3) { log += TextFormat("    line %d: write it like  evolve_by_stats = Hitmonlee, Hitmonchan, Hitmontop\n", ln); sp.statEvos.clear(); }
            else if (sp.evolvesTo.empty()) sp.evolvesTo = sp.statEvos[0];
        } else if (key == "evolve_trade" || key == "evolves_by_trade" || key == "trade_evolution") {
            EvoAlt e;
            e.to = Trim(val);
            e.trade = true;
            if (!e.to.empty() && Lower(e.to) != "none") sp.evoAlts.push_back(e);
        } else if (key == "ability" || key == "abilities") {
            size_t c = val.find(',');
            sp.ability1 = Trim(val.substr(0, c));
            sp.ability2 = c == std::string::npos ? "" : Trim(val.substr(c + 1));
        } else if (key == "hidden_ability") {
            sp.hiddenAbility = Trim(val);
        } else if (key == "category" || key == "species_category") {
            sp.category = Trim(val);
        } else if (key == "dex_entry" || key == "pokedex_entry" || key == "pokedex") {
            sp.dexEntry = Trim(val);
        } else if (key == "sprite_size" || key == "picture_size") {
            float s = (float)std::atof(val.c_str());
            if (s >= 0.25f && s <= 4.0f) sp.spriteSize = s;
        } else if (key == "height") {
            sp.heightDm = (int)std::round(std::atof(val.c_str()) * 10);   // metres
        } else if (key == "weight") {
            sp.weightHg = (int)std::round(std::atof(val.c_str()) * 10);   // kilograms
        } else if (key == "evolve_level" || key == "evolves_at" || key == "evolution_level") {
            sp.evolveLevel = std::max(0, std::min(MAX_LEVEL, std::atoi(val.c_str())));
        } else if (key == "xp_yield" || key == "exp_yield") {
            sp.xpYield = std::max(0, std::atoi(val.c_str()));
        } else if (key == "move") {
            MoveDef m;
            std::string err;
            if (ParseMoveSpec(val, m, err)) moves.push_back(m);
            if (!err.empty()) log += TextFormat("    line %d: %s\n", ln, err.c_str());
        } else if (key == "wild") {   // wild = Route 1, 20   (area from wild.txt, how common)
            size_t c = val.find_last_of(',');
            if (c == std::string::npos) sp.wildAreas.push_back({Trim(val), 10});
            else sp.wildAreas.push_back({Trim(val.substr(0, c)), std::atoi(val.c_str() + c + 1)});
        } else if (key == "tm" || key == "tms") {   // tm = Thunderbolt, Thunder, ...
            std::string part;
            std::istringstream ts(val);
            while (std::getline(ts, part, ',')) {
                part = Trim(part);
                if (part.empty()) continue;
                const MoveDef* tmv = FindMove(part);
                if (!tmv) { log += TextFormat("    line %d: unknown TM move '%s'\n", ln, part.c_str()); continue; }
                if (std::find(sp.tmMoves.begin(), sp.tmMoves.end(), tmv->name) == sp.tmMoves.end()) sp.tmMoves.push_back(tmv->name);
            }
        } else if (key == "learn" || key == "learns") {
            size_t c = val.find(',');
            int lv = std::atoi(val.c_str());
            bool onEvo = Lower(Trim(val.substr(0, c == std::string::npos ? val.size() : c))).rfind("evo", 0) == 0;
            if (onEvo) lv = 0;   // learn = evo, Fire Blast : learned when it evolves into this Pokemon
            if (c == std::string::npos || (lv <= 0 && !onEvo)) {
                log += TextFormat("    line %d: learn should be 'level, Move Name'\n", ln);
                continue;
            }
            MoveDef m;
            std::string err;
            if (ParseMoveSpec(Trim(val.substr(c + 1)), m, err)) {
                if (!sawLearn) { sp.learnset.clear(); sawLearn = true; }
                learn.push_back({std::min(lv, MAX_LEVEL), m});
            }
            if (!err.empty()) log += TextFormat("    line %d: %s\n", ln, err.c_str());
        } else {
            log += TextFormat("    line %d: unknown setting '%s', ignored\n", ln, key.c_str());
        }
    }

    if (sp.name.empty()) { log += "    missing 'name ='\n"; return false; }
    if (!typeSet && replaces.empty()) log += "    no 'type =' given, using Normal\n";
    if (sp.type2 == sp.type1) sp.type2 = Type::None;
    if (spAtk > 0) sp.spAtk = spAtk;
    if (spDef > 0) sp.spDef = spDef;
    if (sawLearn) sp.learnset = learn;
    if (moves.size() > 4) {
        log += "    more than 4 moves listed, using the first 4\n";
        moves.resize(4);
    }
    if (!moves.empty()) sp.moves = moves;
    if (sp.moves.empty() && sp.learnset.empty()) {
        const auto& b = BuiltinSpeciesList();
        int donor = 0;
        for (size_t i = 0; i < b.size(); i++)
            if (b[i].type1 == sp.type1) { donor = (int)i; break; }
        sp.moves = b[donor].moves;
        sp.learnset = b[donor].learnset;
        log += TextFormat("    no moves listed: borrowing %s's moves\n", b[donor].name.c_str());
    }
    if (!sp.evolvesTo.empty() && sp.evolveLevel <= 0) {
        log += "    evolves_to is set but evolve_level isn't: it won't evolve\n";
    }
    return true;
}

} // namespace

std::string CreaturesDir() {
    std::string base = DataDir();
    std::string d = base + "Pokemon";
    if (DirectoryExists(d.c_str())) return d;
    if (DirectoryExists("Pokemon")) return "Pokemon";
    return d;
}

void LoadCustomCreatures(std::string& log) {
    std::string root = CreaturesDir();
    log += "Pokemon folder: " + root + "\n\n";
    if (!DirectoryExists(root.c_str())) {
        log += "  (folder not found - only the built-in Pokemon are available)\n";
        ApplyDexData();
        return;
    }
    FilePathList dirs = LoadDirectoryFiles(root.c_str());
    std::vector<std::string> folders;
    for (unsigned i = 0; i < dirs.count; i++)
        if (!IsPathFile(dirs.paths[i])) folders.push_back(dirs.paths[i]);
    UnloadDirectoryFiles(dirs);
    std::sort(folders.begin(), folders.end());

    auto& reg = SpeciesRegistry();
    for (const std::string& dir : folders) {
        std::string folder = GetFileName(dir.c_str());
        if (folder.empty() || folder[0] == '_' || folder[0] == '.' || Lower(folder) == "converted") continue;
        log += "[" + folder + "]\n";

        std::string file = dir + "/pokemon.txt";
        if (!FileExists(file.c_str())) file = dir + "/creature.txt";
        bool images = HasImages(dir);
        if (!FileExists(file.c_str())) {
            // Images only: reskin the built-in creature with the same name as the folder.
            int idx = FindSpecies(folder);
            if (idx >= 0 && images) {
                reg[idx].spriteDir = dir;
                reg[idx].custom = true;
                log += "  no Pokemon.txt: new sprites for " + reg[idx].name + "\n\n";
            } else {
                log += "  skipped: no pokemon.txt in this folder\n\n";
            }
            continue;
        }

        std::string replaces;
        SpeciesDef sp;
        std::string flog;
        // Start from the creature being replaced so unspecified values keep their old settings.
        {
            std::string probeReplaces;
            SpeciesDef probe;
            std::string tmp;
            ParseCreatureFile(file, probe, probeReplaces, tmp);
            std::string target = !probeReplaces.empty() ? probeReplaces : probe.name;
            int idx = FindSpecies(target);
            if (idx >= 0 && !probeReplaces.empty()) sp = reg[idx];
            else if (idx >= 0 && reg[idx].name == probe.name) sp = reg[idx];
            else sp = SpeciesDef();
            if (!probeReplaces.empty() && idx < 0)
                log += "  'replaces = " + probeReplaces + "' does not match any Pokemon, adding as new\n";
        }
        std::string oldName = sp.name;
        sp.name.clear();
        if (!ParseCreatureFile(file, sp, replaces, flog)) {
            log += flog + "  skipped\n\n";
            continue;
        }
        if (sp.name.empty()) sp.name = oldName;
        sp.custom = true;
        if (images) sp.spriteDir = dir;

        int target = FindSpecies(!replaces.empty() ? replaces : sp.name);
        if (target >= 0 && (!replaces.empty() || reg[target].name == sp.name)) {
            if (sp.artName.empty()) sp.artName = reg[target].artName;
            log += "  replaces " + reg[target].name + (sp.name != reg[target].name ? " -> now called " + sp.name : "") + "\n";
            reg[target] = sp;
        } else {
            reg.push_back(sp);
            log += "  added\n";
        }
        log += TextFormat("  %s  %s%s%s  HP %d  ATK %d  DEF %d  SpA %d  SpD %d  SPD %d\n", sp.name.c_str(), TypeName(sp.type1),
                          sp.type2 != Type::None ? "/" : "", sp.type2 != Type::None ? TypeName(sp.type2) : "",
                          sp.hp, sp.atk, sp.def, sp.SpAtk(), sp.SpDef(), sp.spd);
        for (const auto& m : sp.moves)
            log += TextFormat("    move: %s (%s %s, power %d, %.1fs)\n", m.name.c_str(), TypeName(m.type), MoveKindName(m.kind), m.power, m.cooldown);
        if (!sp.learnset.empty()) {
            log += "    learns:";
            for (const auto& e : sp.learnset) log += TextFormat(" Lv%d %s,", e.level, e.move.name.c_str());
            log.back() = '\n';
        }
        if (!sp.evolvesTo.empty() && sp.evolveLevel > 0) log += TextFormat("    evolves into %s at level %d\n", sp.evolvesTo.c_str(), sp.evolveLevel);
        for (const auto& e : sp.evoAlts) {
            if (e.trade) log += TextFormat("    evolves into %s when traded\n", e.to.c_str());
            else if (!e.item.empty()) log += TextFormat("    evolves into %s with a %s\n", e.to.c_str(), e.item.c_str());
            else log += TextFormat("    evolves into %s at friendship %d\n", e.to.c_str(), e.friendship);
        }
        if (!images) log += "  no image files yet, using " + (sp.artName.empty() ? std::string("a placeholder") : "built-in art") + "\n";
        log += flog + "\n";
    }
    for (const auto& sp : reg)
        if (sp.custom && !sp.evolvesTo.empty() && SpeciesIndex(sp.evolvesTo) < 0)
            log += "Note: " + sp.name + " evolves into " + sp.evolvesTo + ", which isn't in the Pokemon folder yet - it won't evolve until it is.\n";
    for (const auto& sp : reg)
        for (const auto& e : sp.evoAlts)
            if (sp.custom && SpeciesIndex(e.to) < 0)
                log += "Note: " + sp.name + " evolves into " + e.to + ", which isn't in the Pokemon folder yet.\n";
    // Pokemon added from folders are listed in Pokedex order after the built-ins.
    size_t firstNew = BuiltinSpeciesList().size();
    if (reg.size() > firstNew)
        std::stable_sort(reg.begin() + firstNew, reg.end(), [](const SpeciesDef& x, const SpeciesDef& y) {
            return (x.dex ? x.dex : 100000) < (y.dex ? y.dex : 100000);
        });
    ApplyDexData();
}

void WriteCreatureFiles(const std::string& log) {
    std::string root = CreaturesDir();
    if (!DirectoryExists(root.c_str())) return;

    std::string txt = "Arena Clash - load report (rewritten every time the game starts)\n"
                      "==============================================================\n\n" + log;
    SaveFileText((root + "/load_log.txt").c_str(), (char*)txt.c_str());

    std::string ml =
        "MOVE LIST\n"
        "=========\n"
        "Use any of these in a pokemon.txt by name:\n"
        "    move = Thunder Shock             (free-play moveset, up to 4)\n"
        "    learn = 16, Thunderbolt          (learned at a level in Story Mode)\n"
        "\n"
        "Or make your own:   move = Name, Type, Kind, Power, Cooldown [, extras]\n"
        "    move = Razor Wind, Normal, Spread, 40, 1.5, crit\n"
        "    move = Lava Plume, Fire, Wave, 80, 4, burn 30%\n"
        "    move = Growth, Grass, Buff, 0, 10, atk\n"
        "    move = Synthesis, Grass, Heal, 0, 12, heal 35%\n"
        "  Extras: burn/poison/toxic/paralyze/freeze/sleep/confuse/flinch/seed [chance%],\n"
        "          drain 50%, recoil 33%, homing, crit, hits 3, quick, knock 2, patch,\n"
        "          lower atk/def/spd [chance%], atk/def/spd (for Buff), heal 40%, uses 2,\n"
        "          range 200, radius 30, count 5 (Rain), hex\n"
        "          physical / special (otherwise: close-range kinds are physical, ranged kinds special),\n"
        "          bounce 2 (Shot ricochets), orbs 4 (Orbit), segments 3 (Wall), pull 90 (Vortex),\n"
        "          duration 5 (Zone/Orbit/Wall/Trap/Vortex)\n"
        "\n"
        "Physical moves (Melee, Dash, Burst, Wave, Blink, Burrow, Orbit, Leap) use Attack vs Defense.\n"
        "Special moves (Shot, Spread, Beam, Lob, Zone, Strike, Rain, Boomerang, Trap, Vortex, Cone)\n"
        "use Sp. Atk vs Sp. Def.\n"
        "\n"
        "Kinds: Shot, Spread, Melee, Dash, Burst, Beam, Lob, Zone, Buff, Heal,\n"
        "       Strike (instant hit/lightning), Wave, Rain, Blink, Shield, Burrow,\n"
        "       Boomerang (out and back), Orbit (orbs circle you), Wall (blocks shots),\n"
        "       Trap (springs when stepped on), Vortex (drags the foe in), Cone (breath/spray),\n"
        "       Leap (jump up, slam down)\n\n";
    for (int t = 0; t < TYPE_COUNT; t++) {
        ml += std::string("--- ") + TypeName((Type)t) + " ---\n";
        for (const auto& m : MoveLibrary()) {
            if ((int)m.type != t) continue;
            std::string pw = IsDamaging(m) ? std::to_string(m.power) : "-";
            ml += TextFormat("  %-18s %-9s pow %-4s cd %4.1fs  %s%s\n", m.name.c_str(), MoveKindName(m.kind), pw.c_str(), m.cooldown,
                             DescribeMove(m).c_str(), m.original ? "  [original move]" : "");
        }
        ml += "\n";
    }
    SaveFileText((root + "/MOVE-LIST.txt").c_str(), (char*)ml.c_str());
}
