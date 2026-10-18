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
    const int DATA_VERSION = 10;
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
        {"Pokemon/_TEMPLATE/pokemon.txt", 8}, {"Pokemon/HOW-TO-ADD-POKEMON.txt", 8},   // TM moves
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
        if (have < 8) {   // (version 5: Scarlet/Violet movesets; version 8: TM moves)
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
