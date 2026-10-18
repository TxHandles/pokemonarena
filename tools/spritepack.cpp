// spritepack: turns the pixel-art charts in Pokemon/<Name>/ into the small sprite images the game
// actually uses (one pixel per chart square), so the game files stay small.
//   spritepack <in Pokemon dir> <out Pokemon dir> [only this Pokemon]
// For each picture it reads the numbered "_No" chart when there is one (like the game does),
// else the plain chart, and writes <name without _No>.png. Other files are copied as they are.
#include "chart.h"
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
#include <filesystem>
namespace fs = std::filesystem;

static std::string Base(const std::string& stem) {   // "025_1_mae_1_No" -> "025_1_mae_1", "Pokemon-025-Pikachu-No" -> ...
    for (const char* suf : {"_No", "-No", "_no", "-no"}) {
        size_t n = std::string(suf).size();
        if (stem.size() > n && stem.compare(stem.size() - n, n, suf) == 0) return stem.substr(0, stem.size() - n);
    }
    return stem;
}

int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: spritepack <in> <out>\n"); return 1; }
    SetTraceLogLevel(LOG_NONE);
    fs::path in = argv[1], out = argv[2];
    long before = 0, after = 0;
    int pics = 0, failed = 0;
    for (auto& d : fs::directory_iterator(in)) {
        if (!d.is_directory()) continue;
        if (argc > 3 && d.path().filename().string() != argv[3]) continue;
        fs::path od = out / d.path().filename();
        fs::create_directories(od);
        std::map<std::string, std::vector<fs::path>> groups;   // base name -> files (numbered first)
        for (auto& f : fs::directory_iterator(d.path())) {
            if (!f.is_regular_file()) continue;
            before += (long)f.file_size();
            std::string ext = f.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext != ".png" && ext != ".webp" && ext != ".jpg" && ext != ".bmp") {
                if (!fs::exists(od / f.path().filename()))   // pokemon.txt etc.: only if missing (edit the game's copy)
                    fs::copy_file(f.path(), od / f.path().filename());
                after += (long)f.file_size();
                continue;
            }
            std::string stem = f.path().stem().string();
            groups[Base(stem)].push_back(f.path());
        }
        for (auto& g : groups) {
            auto& v = g.second;
            std::sort(v.begin(), v.end(), [](const fs::path& a, const fs::path& b) {
                bool na = Base(a.stem().string()) != a.stem().string(), nb = Base(b.stem().string()) != b.stem().string();
                return na != nb ? na : a < b;
            });
            Image spr{};
            for (auto& p : v) {
                Image raw = LoadImage(p.string().c_str());
                std::string note; bool chart = false;
                bool numbered = Base(p.stem().string()) != p.stem().string();
                spr = ExtractSprite(raw, note, chart, numbered);
                UnloadImage(raw);
                if (spr.width > 0) break;
                printf("  %s: %s\n", p.string().c_str(), note.c_str());
            }
            if (spr.width == 0) { failed++; continue; }
            fs::path op = od / (g.first + ".png");
            ExportImage(spr, op.string().c_str());
            UnloadImage(spr);
            after += (long)fs::file_size(op);
            pics++;
        }
    }
    printf("%d pictures, %d failed: %.1f MB -> %.2f MB\n", pics, failed, before / 1048576.0, after / 1048576.0);
    return failed ? 2 : 0;
}
