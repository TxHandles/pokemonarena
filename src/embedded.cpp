#include "embedded.h"
#include <cstring>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include "raylib.h"

namespace {
struct EmbeddedFile {
    const char* path;
    const unsigned char* data;
    size_t size;
};
} // namespace
#include "embedded_files.inc"   // the data itself is in embedded_data_NN.cpp
namespace {

void MakeDirs(const std::string& base, const std::string& rel) {
    size_t pos = 0;
    while ((pos = rel.find('/', pos)) != std::string::npos) {
        std::string d = base + rel.substr(0, pos);
        if (!DirectoryExists(d.c_str())) MakeDirectory(d.c_str());
        pos++;
    }
}
} // namespace

std::string DataDir() {
    static std::string dir;
    if (!dir.empty()) return dir;
    std::string base = GetApplicationDirectory();
    auto has = [&](const char* name) {
        std::string p = base + name;
        return DirectoryExists(p.c_str()) || FileExists(p.c_str());
    };
    if (has("Pokemon") || has("story.txt") || has("saves") || has("creatures")) {
        dir = base;
    } else {
        dir = base + "ArenaClash Data/";
        if (!DirectoryExists(dir.c_str())) MakeDirectory(dir.c_str());
        if (!DirectoryExists(dir.c_str())) dir = base;  // couldn't create it: fall back
    }
    return dir;
}

std::string RestoreEmbeddedFiles() {
    std::string base = DataDir();
    int restored = 0, failed = 0;
    // Data updates: files this game version changed are refreshed once (the old copy is kept
    // next to it as .old). Everything else is only written when it's missing, so your edits stay.
    const int DATA_VERSION = 30;
    struct Upd { const char* path; int version; };
    static const Upd kUpdated[] = {
        {"Pokemon/Bulbasaur/pokemon.txt", 2}, {"Pokemon/Pikachu/pokemon.txt", 2},
        {"Pokemon/_TEMPLATE/pokemon.txt", 3}, {"Pokemon/HOW-TO-ADD-POKEMON.txt", 3},
        {"Pokemon/Kadabra/pokemon.txt", 3}, {"Pokemon/Machoke/pokemon.txt", 3},   // trade evolutions
        {"Pokemon/Graveler/pokemon.txt", 3}, {"Pokemon/Haunter/pokemon.txt", 3},
        {"Pokemon/HOW-TO-ADD-POKEMON.txt", 5},
        {"story.txt", 6},   // rival battles (saves move their stage forward to match)
        {"story.txt", 7},   // outdoor gyms option
        {"story.txt", 9},   // venue key
        {"story.txt", 10},  // Snorlax on Routes 16 and 12, the Dojo moved to Saffron
        {"wild.txt", 10},   // rare trade Pokemon, Cinnabar Volcano
        {"story.txt", 11},  // trainer levels (lv) like the real games; no catching trainers' Pokemon
        {"story.txt", 12},  // gym leaders, Giovanni and other bosses use their FireRed/LeafGreen teams
        {"Pokemon/_TEMPLATE/pokemon.txt", 12},   // height / weight
        {"story.txt", 13},  // Victory Road, the Elite Four and the Champion
        {"story.txt", 14},  // the rival picks two of the six starters in co-op
        {"Pokemon/HOW-TO-ADD-POKEMON.txt", 15},   // shiny.txt
        {"Pokemon/Nidoking/Pokemon-034-Nidoking.png", 16}, {"Pokemon/Nidoking/shiny.txt", 16},   // the broken portrait, redrawn
        // Gen 2 evolutions for Kanto Pokemon (Crobat, Blissey, Bellossom, Politoed, Slowking, Steelix,
        // Scizor, Kingdra, Porygon2, Espeon and Umbreon)
        {"Pokemon/Golbat/pokemon.txt", 17}, {"Pokemon/Chansey/pokemon.txt", 17}, {"Pokemon/Gloom/pokemon.txt", 17},
        {"Pokemon/Poliwhirl/pokemon.txt", 17}, {"Pokemon/Slowpoke/pokemon.txt", 17}, {"Pokemon/Onix/pokemon.txt", 17},
        {"Pokemon/Scyther/pokemon.txt", 17}, {"Pokemon/Seadra/pokemon.txt", 17}, {"Pokemon/Porygon/pokemon.txt", 17},
        {"Pokemon/Eevee/pokemon.txt", 17}, {"Pokemon/_TEMPLATE/pokemon.txt", 17}, {"Pokemon/HOW-TO-ADD-POKEMON.txt", 17},
        {"Pokemon/_TEMPLATE/pokemon.txt", 8}, {"Pokemon/HOW-TO-ADD-POKEMON.txt", 8},   // TM moves
        {"story.txt", 18}, {"wild.txt", 18},   // Johto: the second region (its ladder and wild areas)
        {"Pokemon/Treecko/pokemon.txt", 19}, {"Pokemon/Torchic/pokemon.txt", 19}, {"Pokemon/Mudkip/pokemon.txt", 19},   // Hoenn starters join the pool
        {"story.txt", 19},
        {"story.txt", 20},   // double battles: twins on Route 8, Celadon Park and Dragon's Den, Lance beside you vs Ariana
        {"story.txt", 21}, {"wild.txt", 21},   // Hoenn: the third region; Red on Mt. Silver becomes a post-game challenge
        {"story.txt", 22},  // free region order: co-op rival lines for three starters
        {"Pokemon/Aipom/pokemon.txt", 23}, {"Pokemon/Misdreavus/pokemon.txt", 23}, {"Pokemon/Murkrow/pokemon.txt", 23}, {"Pokemon/Sneasel/pokemon.txt", 23}, {"Pokemon/Magneton/pokemon.txt", 23}, {"Pokemon/Lickitung/pokemon.txt", 23}, {"Pokemon/Rhydon/pokemon.txt", 23}, {"Pokemon/Tangela/pokemon.txt", 23}, {"Pokemon/Electabuzz/pokemon.txt", 23}, {"Pokemon/Magmar/pokemon.txt", 23}, {"Pokemon/Togetic/pokemon.txt", 23}, {"Pokemon/Yanma/pokemon.txt", 23}, {"Pokemon/Eevee/pokemon.txt", 23}, {"Pokemon/Gligar/pokemon.txt", 23}, {"Pokemon/Piloswine/pokemon.txt", 23}, {"Pokemon/Porygon2/pokemon.txt", 23}, {"Pokemon/Kirlia/pokemon.txt", 23}, {"Pokemon/Snorunt/pokemon.txt", 23}, {"Pokemon/Nosepass/pokemon.txt", 23}, {"Pokemon/Dusclops/pokemon.txt", 23}, {"Pokemon/Roselia/pokemon.txt", 23},   // Gen 4 evolutions
        {"story.txt", 24}, {"wild.txt", 24},   // Sinnoh: the fourth region
        {"story.txt", 25}, {"wild.txt", 25},   // Unova: the fifth region
        {"story.txt", 26},   // Mega Evolution: bosses that Mega Evolve
        {"story.txt", 27}, {"Pokemon/Eevee/pokemon.txt", 27},   // Roxanne's team; Sylveon on hold with Gen 6
        {"wild.txt", 28}, {"World/world.txt", 28},   // the walked Kanto: other regions' cousins in the wild, field moves on the map
        {"wild.txt", 29}, {"World/world.txt", 29}, {"World/chars.png", 29}, {"story.txt", 29},   // the FireRed maps, people and story order
        {"wild.txt", 30}, {"World/world.txt", 30},   // FireRed's items on the map, Fly spots
    };
    std::string verPath = base + "data_version.txt";
    int have = 1;
    if (char* v = LoadFileText(verPath.c_str())) { have = std::atoi(v); UnloadFileText(v); }
    int updated = 0;
    if (have < DATA_VERSION) {
        for (const Upd& up : kUpdated) {
            if (have >= up.version) continue;
            std::string path = base + up.path;
            if (!FileExists(path.c_str())) continue;
            int n = 0;
            unsigned char* old = LoadFileData(path.c_str(), &n);
            if (old) { SaveFileData((path + ".old").c_str(), old, n); UnloadFileData(old); }
            std::remove(path.c_str());
            updated++;
        }
        // Version 4: every Pokemon's moves and evolutions were checked against the Pokedex.
        // Changed pokemon.txt files are replaced; the old ones are kept in Pokemon/_old_files.
        if (have < 12) {   // (version 5: Scarlet/Violet movesets; 8: TM moves; 12: sizes from Pokedex height)
            for (const auto& f : kEmbeddedFiles) {
                std::string rel = f.path;
                if (rel.rfind("Pokemon/", 0) != 0 || rel.size() < 20 || rel.substr(rel.size() - 12) != "/pokemon.txt") continue;
                std::string folder = rel.substr(8, rel.size() - 20);
                if (folder.empty() || folder[0] == '_' || folder.find('/') != std::string::npos) continue;
                std::string path = base + rel;
                if (!FileExists(path.c_str())) continue;
                int n = 0;
                unsigned char* old = LoadFileData(path.c_str(), &n);
                bool same = old && n == (int)f.size && std::memcmp(old, f.data, f.size) == 0;
                if (old && !same) {
                    MakeDirs(base, "Pokemon/_old_files/x");
                    SaveFileData((base + "Pokemon/_old_files/" + folder + ".txt").c_str(), old, n);
                }
                if (old) UnloadFileData(old);
                if (!same) { std::remove(path.c_str()); updated++; }
            }
        }
        if (have < 27) {   // Gen 6 (Kalos) is on hold until its sprites match: its folders move aside (nothing is deleted)
            static const char* kGen6[] = {"Aegislash", "Amaura", "Aromatisse", "Aurorus", "Avalugg", "Barbaracle", "Bergmite", "Binacle", "Braixen", "Bunnelby", "Carbink", "Chesnaught", "Chespin", "Clauncher", "Clawitzer", "Dedenne", "Delphox", "Diancie", "Diggersby", "Doublade", "Dragalge", "Espurr", "Fennekin", "Flabebe", "Fletchinder", "Fletchling", "Floette", "Florges", "Froakie", "Frogadier", "Furfrou", "Gogoat", "Goodra", "Goomy", "Gourgeist", "Greninja", "Hawlucha", "Heliolisk", "Helioptile", "Honedge", "Hoopa", "Inkay", "Klefki", "Litleo", "Malamar", "Meowstic", "Noibat", "Noivern", "Pancham", "Pangoro", "Phantump", "Pumpkaboo", "Pyroar", "Quilladin", "Scatterbug", "Skiddo", "Skrelp", "Sliggoo", "Slurpuff", "Spewpa", "Spritzee", "Swirlix", "Sylveon", "Talonflame", "Trevenant", "Tyrantrum", "Tyrunt", "Vivillon", "Volcanion", "Xerneas", "Yveltal", "Zygarde"};
            for (const char* n : kGen6) {
                std::string from = base + "Pokemon/" + n;
                if (!DirectoryExists(from.c_str())) continue;
                MakeDirs(base, "Pokemon/_gen6_on_hold/x");
                std::string to = base + "Pokemon/_gen6_on_hold/" + n;
                if (DirectoryExists(to.c_str())) continue;
                if (std::rename(from.c_str(), to.c_str()) == 0) updated++;
            }
        }
        SaveFileText(verPath.c_str(), (char*)TextFormat("%d", DATA_VERSION));
    }
    for (const auto& f : kEmbeddedFiles) {
        std::string path = base + f.path;
        if (FileExists(path.c_str())) continue;
        MakeDirs(base, f.path);
        if (SaveFileData(path.c_str(), (void*)f.data, (int)f.size)) restored++;
        else failed++;
    }
    std::string r;
    if (updated) r += TextFormat("Updated %d data file(s) for this version (your old copies were kept as .old files or in Pokemon/_old_files).\n", updated);
    if (restored) r += TextFormat("Restored %d built-in data file(s) next to the game.\n", restored);
    if (failed) r += TextFormat("Could not write %d built-in data file(s) (is the folder read-only?).\n", failed);
    return r;
}
