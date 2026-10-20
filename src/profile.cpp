#include "profile.h"
#include "story.h"
#include "embedded.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include "raylib.h"

namespace {

std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::string FileNameFor(const std::string& name) {
    std::string f;
    for (char c : name) f += (std::isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
    if (f.empty()) f = "player";
    return SavesDir() + "/" + f + ".save";
}

} // namespace

std::string SavesDir() {
    std::string d = DataDir() + "saves";
    if (!DirectoryExists(d.c_str())) MakeDirectory(d.c_str());
    return d;
}

std::vector<std::string> ListProfiles() {
    std::vector<std::string> out;
    FilePathList files = LoadDirectoryFilesEx(SavesDir().c_str(), ".save", false);
    for (unsigned i = 0; i < files.count; i++) {
        Profile p;
        if (LoadProfile(GetFileNameWithoutExt(files.paths[i]), p)) out.push_back(p.name);
    }
    UnloadDirectoryFiles(files);
    std::sort(out.begin(), out.end());
    return out;
}

bool ProfileExists(const std::string& name) { return FileExists(FileNameFor(name).c_str()); }

bool LoadProfile(const std::string& name, Profile& out, std::string* problems) {
    char* text = LoadFileText(FileNameFor(name).c_str());
    if (!text) return false;
    std::string t = text;
    UnloadFileText(text);
    return ProfileFromText(t, name, out, problems);
}

bool ProfileFromText(const std::string& text, const std::string& name, Profile& out, std::string* problems) {
    std::istringstream ss(text);
    out = Profile();
    out.name = name;
    OwnedMon* cur = nullptr;
    std::string line;
    bool sawBag = false;
    while (std::getline(ss, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line == "[creature]") {
            out.mons.push_back(OwnedMon());
            cur = &out.mons.back();
            continue;
        }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        int iv = std::atoi(v.c_str());
        if (!cur) {
            if (k == "name") out.name = v;
            else if (k == "story_stage") out.storyStage = iv;
            else if (k == "story_rev") out.storyRev = iv;
            else if (k == "clock") out.clock = std::max(0, iv);
            else if (k == "starter") out.starter = v;
            else if (k == "train_area") out.trainArea = v;
            else if (k == "fossil") out.fossil = v;
            else if (k == "fossil_revived") out.fossilRevived = iv != 0;
            else if (k == "eevee_gift") out.eeveeGift = iv != 0;
            else if (k == "old_amber") { out.amber = iv != 0; out.amberRevived = iv >= 2; }
            else if (k == "dojo") out.dojo = v;
            else if (k == "snorlax") out.snorlax = iv;
            else if (k == "casino_coins") out.casino = std::max(0, iv);
            else if (k == "box_sort") out.boxSort = std::max(0, iv);
            else if (k == "box_filter") out.boxFilter = v;
            else if (k == "tower") { std::istringstream ts(v); ts >> out.towerFloor >> out.towerBest >> out.towerRuns; }
            else if (k == "hof") { if (!v.empty()) out.hof.push_back(v); }
            else if (k == "legends") out.legends = std::max(0, iv);
            else if (k == "rival") out.rival = v;
            else if (k == "wins") out.wins = iv;
            else if (k == "losses") out.losses = iv;
            else if (k == "training_wins") out.trainingWins = iv;
            else if (k == "coins") out.coins = std::max(0, iv);
            else if (k == "bag") {
                sawBag = true;
                std::string part;
                std::istringstream bs(v);
                while (std::getline(bs, part, '|')) {
                    part = Trim(part);
                    size_t c = part.rfind(':');
                    if (c == std::string::npos) continue;
                    std::string nm = Trim(part.substr(0, c));
                    int cnt = std::atoi(part.c_str() + c + 1);
                    if (!nm.empty() && cnt > 0) out.bag[nm] += cnt;
                }
            } else if (k == "dex_seen" || k == "dex_caught") {
                std::string part;
                std::istringstream bs(v);
                while (std::getline(bs, part, '|')) {
                    part = Trim(part);
                    if (!part.empty()) (k == "dex_seen" ? out.dexSeen : out.dexCaught).insert(part);
                }
            } else if (k == "badges") {
                std::string part;
                std::istringstream bs(v);
                while (std::getline(bs, part, '|')) {
                    part = Trim(part);
                    if (!part.empty()) out.badges.push_back(part);
                }
            }
        } else {
            if (k == "species") cur->species = v;
            else if (k == "level") cur->level = std::max(1, std::min(MAX_LEVEL, iv));
            else if (k == "xp") cur->xp = std::max(0, iv);
            else if (k == "battles") cur->battles = iv;
            else if (k == "wins") cur->wins = iv;
            else if (k == "kos") cur->kos = iv;
            else if (k == "hp") cur->hp = std::max(0.0f, std::min(1.0f, (float)std::atof(v.c_str())));
            else if (k == "caught") cur->caught = iv != 0;
            else if (k == "item") cur->item = v;
            else if (k == "boxed") cur->boxed = iv != 0;
            else if (k == "friendship") cur->friendship = std::max(0, std::min(MAX_FRIENDSHIP, iv));
            else if (k == "ability_slot") cur->abilitySlot = std::max(0, std::min(2, iv));
            else if (k == "ot") cur->ot = v;
            else if (k == "nature") cur->nature = NatureByName(v);
            else if (k == "shiny") cur->shiny = iv != 0;
            else if (k == "forgotten") {
                cur->forgotten.clear();
                std::string part;
                std::istringstream fs(v);
                while (std::getline(fs, part, '|')) {
                    part = Trim(part);
                    if (!part.empty()) cur->forgotten.push_back(part);
                }
            }
            else if (k == "moves") {
                cur->moves.clear();
                std::string part;
                std::istringstream ms(v);
                while (std::getline(ms, part, '|')) {
                    part = Trim(part);
                    if (!part.empty() && cur->moves.size() < 4) cur->moves.push_back(part);
                }
            }
        }
    }
    if (!sawBag) out.bag["Poke Ball"] = START_BALLS;  // saves from before the shop
    for (auto& m : out.mons) {
        // The old grass starter slot became Bulbasaur.
        if (SpeciesIndex(m.species) < 0 && (m.species == "Thistlet" || m.species == "Thornbloom")) m.species = "Bulbasaur";
        m.xp = std::max(m.xp, XpForLevel(m.level));
        if (m.abilitySlot < 0) m.abilitySlot = RollAbilitySlot(MonSpecies(m), false);   // saves from before abilities
        if (m.nature < 0) m.nature = GetRandomValue(0, NATURE_COUNT - 1);                 // saves from before natures
        if (problems && MonSpecies(m) < 0) *problems += "  " + m.species + " is missing from the Pokemon folder\n";
    }
    FixParty(out);
    DexRegisterOwned(out);
    if (out.storyRev <= 0) out.storyRev = out.mons.empty() ? Story().rev : 1;
    if (out.storyRev < Story().rev) { out.storyStage = StoryStageForRev(out.storyStage, out.storyRev); out.storyRev = Story().rev; }
    return true;
}

bool SaveProfile(const Profile& p) {
    std::string o = ProfileToText(p);
    return SaveFileText(FileNameFor(p.name).c_str(), (char*)o.c_str());
}

std::string ProfileToText(const Profile& p) {
    std::string o = "# Arena Clash save file\n";
    o += "name = " + p.name + "\n";
    o += TextFormat("story_stage = %d\nwins = %d\nlosses = %d\ntraining_wins = %d\ncoins = %d\n", p.storyStage, p.wins, p.losses, p.trainingWins, p.coins);
    o += TextFormat("story_rev = %d\nclock = %d\n", p.storyRev > 0 ? p.storyRev : Story().rev, p.clock);
    if (!p.trainArea.empty()) o += "train_area = " + p.trainArea + "\n";
    if (!p.fossil.empty()) o += "fossil = " + p.fossil + TextFormat("\nfossil_revived = %d\n", p.fossilRevived ? 1 : 0);
    if (p.eeveeGift) o += "eevee_gift = 1\n";
    if (p.amber) o += TextFormat("old_amber = %d\n", p.amberRevived ? 2 : 1);
    if (!p.dojo.empty()) o += "dojo = " + p.dojo + "\n";
    if (p.snorlax) o += TextFormat("snorlax = %d\n", p.snorlax);
    if (p.casino) o += TextFormat("casino_coins = %d\n", p.casino);
    if (p.boxSort) o += TextFormat("box_sort = %d\n", p.boxSort);
    if (!p.boxFilter.empty()) o += "box_filter = " + p.boxFilter + "\n";
    if (p.towerFloor || p.towerBest || p.towerRuns) o += TextFormat("tower = %d %d %d\n", p.towerFloor, p.towerBest, p.towerRuns);
    for (const auto& h : p.hof) o += "hof = " + h + "\n";
    if (p.legends) o += TextFormat("legends = %d\n", p.legends);
    if (!p.rival.empty()) o += "rival = " + p.rival + "\n";
    if (!p.starter.empty()) o += "starter = " + p.starter + "\n";
    std::string bd;
    for (size_t i = 0; i < p.badges.size(); i++) bd += (i ? " | " : "") + p.badges[i];
    o += "badges = " + bd + "\n";
    std::string bg;
    for (const auto& kv : p.bag)
        if (kv.second > 0) bg += (bg.empty() ? "" : " | ") + kv.first + ":" + std::to_string(kv.second);
    o += "bag = " + bg + "\n";
    {
        std::set<std::string> caught = p.dexCaught, seen = p.dexSeen;
        for (const auto& m : p.mons) caught.insert(m.species);
        for (const auto& c : caught) seen.insert(c);
        std::string a, b;
        for (const auto& s : seen) a += (a.empty() ? "" : " | ") + s;
        for (const auto& s : caught) b += (b.empty() ? "" : " | ") + s;
        o += "dex_seen = " + a + "\n";
        o += "dex_caught = " + b + "\n";
    }
    for (const auto& m : p.mons) {
        o += "\n[creature]\nspecies = " + m.species + "\n";
        o += TextFormat("level = %d\nxp = %d\nbattles = %d\nwins = %d\nkos = %d\nhp = %.3f\ncaught = %d\n", m.level, m.xp, m.battles, m.wins, m.kos, m.hp, m.caught ? 1 : 0);
        std::string mv;
        for (size_t i = 0; i < m.moves.size(); i++) mv += (i ? " | " : "") + m.moves[i];
        if (!m.forgotten.empty()) {
            std::string fg;
            for (size_t i = 0; i < m.forgotten.size(); i++) fg += (i ? " | " : "") + m.forgotten[i];
            o += "forgotten = " + fg + "\n";
        }
        o += "moves = " + mv + "\n";
        if (!m.item.empty()) o += "item = " + m.item + "\n";
        if (m.boxed) o += "boxed = 1\n";
        o += TextFormat("friendship = %d\n", m.friendship);
        if (m.abilitySlot >= 0) o += TextFormat("ability_slot = %d\n", m.abilitySlot);
        if (!m.ot.empty()) o += "ot = " + m.ot + "\n";
        if (m.nature >= 0) o += std::string("nature = ") + NatureName(m.nature) + "\n";
        if (m.shiny) o += "shiny = 1\n";
    }
    return o;
}

bool DeleteProfile(const std::string& name) { return std::remove(FileNameFor(name).c_str()) == 0; }

OwnedMon NewMon(int idx, int level) {
    OwnedMon m;
    const SpeciesDef& sp = AllSpecies()[idx];
    m.species = sp.name;
    m.level = std::max(1, std::min(MAX_LEVEL, level));
    m.xp = XpForLevel(m.level);
    for (const auto& mv : MovesAtLevel(sp, m.level)) m.moves.push_back(mv.name);
    m.abilitySlot = RollAbilitySlot(idx);
    m.nature = GetRandomValue(0, NATURE_COUNT - 1);
    m.shiny = RollShiny();
    return m;
}

bool RollShiny() {
    int odds = SHINY_ODDS;
#ifdef ARENA_AUTOTEST
    if (const char* e = std::getenv("ARENA_SHINY")) odds = std::max(1, std::atoi(e));
#endif
    return GetRandomValue(1, odds) == 1;
}

int RollAbilitySlot(int idx, bool allowHidden) {
    if (idx < 0 || idx >= (int)AllSpecies().size()) return 0;
    const SpeciesDef& sp = AllSpecies()[idx];
    if (allowHidden && !sp.hiddenAbility.empty() && GetRandomValue(1, 20) == 1) return 2;
    return AbilitySlotCount(idx) > 1 ? GetRandomValue(0, 1) : 0;
}

std::string MonAbility(const OwnedMon& m) { return SpeciesAbilityName(MonSpecies(m), m.abilitySlot < 0 ? 0 : m.abilitySlot); }

void DexSee(Profile& p, const std::string& species) { if (!species.empty()) p.dexSeen.insert(species); }
bool DexHasCaught(const Profile& p, const std::string& species) {
    if (p.dexCaught.count(species)) return true;
    for (const auto& m : p.mons) if (m.species == species) return true;
    return false;
}
bool DexHasSeen(const Profile& p, const std::string& species) { return p.dexSeen.count(species) || DexHasCaught(p, species); }
void DexRegisterOwned(Profile& p) {
    for (const auto& m : p.mons) { p.dexCaught.insert(m.species); p.dexSeen.insert(m.species); }
}

int MonSpecies(const OwnedMon& m) { return SpeciesIndex(m.species); }

std::vector<MoveDef> MonMoves(const OwnedMon& m) {
    std::vector<MoveDef> out;
    int idx = MonSpecies(m);
    for (const auto& name : m.moves) {
        const MoveDef* d = FindMove(name);
        if (!d && idx >= 0) {  // custom move defined in the creature's own file
            const SpeciesDef& sp = AllSpecies()[idx];
            for (const auto& e : sp.learnset) if (e.move.name == name) d = &e.move;
            for (const auto& mv : sp.moves) if (mv.name == name) d = &mv;
        }
        if (d) out.push_back(*d);
    }
    if (out.empty() && idx >= 0) out = MovesAtLevel(AllSpecies()[idx], m.level);
    return out;
}

CombatantSpec MonCombatant(const Profile& p, int i) {
    CombatantSpec c;
    const OwnedMon& m = p.mons[i];
    c.species = std::max(0, MonSpecies(m));
    c.level = m.level;
    c.xp = m.xp;
    c.moves = MonMoves(m);
    c.ownedId = i;
    c.hpFrac = m.hp;
    c.item = m.item;
    c.abilitySlot = m.abilitySlot < 0 ? 0 : m.abilitySlot;
    c.nature = m.nature < 0 ? 0 : m.nature;
    c.shiny = m.shiny;
    return c;
}

int BagCount(const Profile& p, const std::string& item) {
    auto it = p.bag.find(item);
    return it == p.bag.end() ? 0 : it->second;
}

void BagAdd(Profile& p, const std::string& item, int n) {
    if (!item.empty() && n > 0) p.bag[item] += n;
}

bool BagTake(Profile& p, const std::string& item) {
    auto it = p.bag.find(item);
    if (it == p.bag.end() || it->second <= 0) return false;
    if (--it->second == 0) p.bag.erase(it);
    return true;
}

int HealCost(const OwnedMon& m) {
    if (m.hp >= 0.999f) return 0;
    if (m.hp <= 0) return m.level * 6 + 10;
    return std::max(1, (int)std::ceil((1.0f - m.hp) * m.level * 3));
}

bool HasBadge(const Profile& p, const std::string& b) {
    return std::find(p.badges.begin(), p.badges.end(), b) != p.badges.end();
}

int TeamAverageLevel(const Profile& p, const std::vector<int>& idx) {
    if (idx.empty()) return 5;
    int sum = 0;
    for (int i : idx) sum += p.mons[i].level;
    return (int)std::lround((float)sum / idx.size());
}

void CollectNewMoves(OwnedMon& m, int speciesIndex, int level, Growth& g, bool evolving) {
    if (speciesIndex < 0) return;
    for (const MoveDef& mv : MovesLearnedAt(AllSpecies()[speciesIndex], level, evolving)) {
        if (std::find(m.moves.begin(), m.moves.end(), mv.name) != m.moves.end()) continue;
        if (std::find(g.wantsToLearn.begin(), g.wantsToLearn.end(), mv.name) != g.wantsToLearn.end()) continue;
        if (m.moves.size() < 4) {
            m.moves.push_back(mv.name);
            g.learned.push_back(mv.name);
        } else {
            g.wantsToLearn.push_back(mv.name);
        }
    }
}

void AddXp(OwnedMon& m, int xp, Growth& g) {
    if (m.level >= MAX_LEVEL || xp <= 0) return;
    m.xp += xp;
    int idx = MonSpecies(m);
    while (m.level < MAX_LEVEL && m.xp >= XpForLevel(m.level + 1)) {
        m.level++;
        g.levelsGained++;
        CollectNewMoves(m, idx, m.level, g);
    }
    if (idx >= 0 && g.evolveTo < 0) {
        int evo = EvolutionIndexFor(idx, m.nature);
        if (evo >= 0 && m.level >= AllSpecies()[idx].evolveLevel) g.evolveTo = evo;
    }
}

void Evolve(OwnedMon& m, int to, Growth& g) {
    if (to < 0) return;
    m.species = AllSpecies()[to].name;
    CollectNewMoves(m, to, m.level, g, true);
}

// ------------------------------------------------------------------ party and Box

int PartyCount(const Profile& p) {
    int n = 0;
    for (const auto& m : p.mons) n += !m.boxed;
    return n;
}

bool AddMon(Profile& p, OwnedMon m) {
    m.boxed = PartyCount(p) >= PARTY_SIZE;
    p.mons.push_back(m);
    return m.boxed;
}

void FixParty(Profile& p) {
    int n = 0;
    for (auto& m : p.mons) {
        if (m.boxed) continue;
        if (++n > PARTY_SIZE) m.boxed = true;
    }
    if (n == 0)
        for (int i = 0; i < (int)p.mons.size() && i < PARTY_SIZE; i++) p.mons[i].boxed = false;
}

// ------------------------------------------------------------------ co-op saves

namespace {
std::string CoopFile(std::string a, std::string b) {
    if (b < a) std::swap(a, b);
    auto clean = [](const std::string& n) {
        std::string f;
        for (char c : n) f += (std::isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_';
        return f;
    };
    return SavesDir() + "/" + clean(a) + "+" + clean(b) + ".coop";
}
} // namespace

bool LoadCoop(const std::string& a, const std::string& b, CoopSave& out) {
    out = CoopSave();
    out.p[0] = a;
    out.p[1] = b;
    char* text = LoadFileText(CoopFile(a, b).c_str());
    if (!text) return false;
    std::istringstream ss(text);
    UnloadFileText(text);
    std::string line;
    while (std::getline(ss, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = Trim(line.substr(0, eq)), v = Trim(line.substr(eq + 1));
        int iv = std::atoi(v.c_str());
        if (k == "story_stage") out.storyStage = iv;
        else if (k == "story_rev") out.storyRev = iv;
        else if (k == "clock") out.clock = std::max(0, iv);
        else if (k == "train_area") out.trainArea = v;
        else if (k == "wins") out.wins = iv;
        else if (k == "losses") out.losses = iv;
        else if (k == "training_wins") out.trainingWins = iv;
        else if (k == "snorlax_who") { std::istringstream rs(v); rs >> out.snorlaxWho[0] >> out.snorlaxWho[1]; }
        else if (k == "dojo_first") out.dojoFirst = iv;
        else if (k == "tower") { std::istringstream ts(v); ts >> out.towerFloor >> out.towerBest >> out.towerRuns; }
        else if (k == "legends") { std::istringstream ls(v); ls >> out.legends; for (int i = 0; i < 5; i++) ls >> out.legendWho[i]; }
        else if (k == "rival") { size_t c = v.find(','); out.rival[0] = Trim(v.substr(0, c)); out.rival[1] = c == std::string::npos ? "" : Trim(v.substr(c + 1)); }
        else if (k == "run1" || k == "run2") {
            CoopSave::RunStats& r = out.run[k == "run2" ? 1 : 0];
            std::istringstream rs(v);
            rs >> r.kos >> r.damage >> r.caught >> r.mvp >> r.faints >> r.battles;
        }
        else if (k == "badges") {
            std::string part;
            std::istringstream bs(v);
            while (std::getline(bs, part, '|')) {
                part = Trim(part);
                if (!part.empty()) out.badges.push_back(part);
            }
        }
    }
    if (out.storyRev <= 0) out.storyRev = 1;
    if (out.storyRev < Story().rev) { out.storyStage = StoryStageForRev(out.storyStage, out.storyRev); out.storyRev = Story().rev; }
    return true;
}

bool SaveCoop(const CoopSave& c) {
    std::string o = "# Arena Clash co-op save: the progress these two profiles share\n";
    o += "player1 = " + c.p[0] + "\nplayer2 = " + c.p[1] + "\n";
    o += TextFormat("story_stage = %d\nwins = %d\nlosses = %d\ntraining_wins = %d\n", c.storyStage, c.wins, c.losses, c.trainingWins);
    o += TextFormat("story_rev = %d\nclock = %d\n", c.storyRev > 0 ? c.storyRev : Story().rev, c.clock);
    if (!c.trainArea.empty()) o += "train_area = " + c.trainArea + "\n";
    o += TextFormat("snorlax_who = %d %d\ndojo_first = %d\n", c.snorlaxWho[0], c.snorlaxWho[1], c.dojoFirst);
    o += TextFormat("tower = %d %d %d\n", c.towerFloor, c.towerBest, c.towerRuns);
    o += TextFormat("legends = %d %d %d %d %d %d\n", c.legends, c.legendWho[0], c.legendWho[1], c.legendWho[2], c.legendWho[3], c.legendWho[4]);
    if (!c.rival[0].empty()) o += "rival = " + c.rival[0] + ", " + c.rival[1] + "\n";
    o += "# leaderboard: KOs, damage, caught, MVP battles, fainted, battles\n";
    for (int p = 0; p < 2; p++)
        o += TextFormat("run%d = %d %d %d %d %d %d\n", p + 1, c.run[p].kos, c.run[p].damage, c.run[p].caught, c.run[p].mvp, c.run[p].faints, c.run[p].battles);
    std::string bd;
    for (size_t i = 0; i < c.badges.size(); i++) bd += (i ? " | " : "") + c.badges[i];
    o += "badges = " + bd + "\n";
    return SaveFileText(CoopFile(c.p[0], c.p[1]).c_str(), (char*)o.c_str());
}
