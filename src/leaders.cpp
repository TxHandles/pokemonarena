#include "leaders.h"
#include <cctype>
#include <cmath>
#include <map>
#include <vector>
#include "embedded.h"

namespace {
struct Pic { bool tried = false; Texture2D tex{}; };
std::map<std::string, Pic> gPics;   // by lower-case short name

std::string Lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }

std::vector<std::string> Words(const std::string& s) {
    std::vector<std::string> w;
    std::string cur;
    for (char c : s) {
        if (std::isalnum((unsigned char)c) || c == '\'') cur += (char)std::tolower((unsigned char)c);
        else if (!cur.empty()) { w.push_back(cur); cur.clear(); }
    }
    if (!cur.empty()) w.push_back(cur);
    return w;
}

// The picture files there are (lower-case names without extension -> path).
const std::map<std::string, std::string>& Files() {
    static std::map<std::string, std::string> files;
    static bool scanned = false;
    if (scanned) return files;
    scanned = true;
    std::string dir = DataDir() + "Leaders";
    if (!DirectoryExists(dir.c_str())) return files;
    FilePathList fl = LoadDirectoryFiles(dir.c_str());
    for (unsigned i = 0; i < fl.count; i++) {
        std::string ext = Lower(GetFileExtension(fl.paths[i]));
        if (ext != ".png") continue;
        files[Lower(GetFileNameWithoutExt(fl.paths[i]))] = fl.paths[i];
    }
    UnloadDirectoryFiles(fl);
    return files;
}

std::string Key(const std::string& trainer) {
    const auto& f = Files();
    auto w = Words(trainer);
    for (int i = (int)w.size() - 1; i >= 0; i--)   // the name is usually last ("Leader Brock")
        if (f.count(w[i])) return w[i];
    return "";
}
} // namespace

const Texture2D* LeaderPicture(const std::string& trainer) {
    std::string k = Key(trainer);
    if (k.empty() || !IsWindowReady()) return nullptr;
    Pic& p = gPics[k];
    if (!p.tried) {
        p.tried = true;
        Image img = LoadImage(Files().at(k).c_str());
        if (img.data) {
            p.tex = LoadTextureFromImage(img);
            SetTextureFilter(p.tex, TEXTURE_FILTER_POINT);
            UnloadImage(img);
        }
    }
    return p.tex.id ? &p.tex : nullptr;
}

std::string LeaderShortName(const std::string& trainer) {
    auto w = Words(trainer);
    std::string k = Key(trainer);
    std::string n = !k.empty() ? k : (w.empty() ? trainer : w.back());
    if (!n.empty()) n[0] = (char)std::toupper((unsigned char)n[0]);
    return n;
}

const char* LeaderLastStandLine(const std::string& trainer) {
    static const std::map<std::string, const char*> lines = {
        {"brock", "My defense is solid as bedrock - break this one if you can!"},
        {"misty", "One last wave - and it's a big one!"},
        {"surge", "Last unit standing, soldier! Let's see you survive this!"},
        {"erika", "Even the last flower in the garden blooms its brightest."},
        {"koga", "A ninja is most dangerous when cornered."},
        {"sabrina", "I foresaw this moment... and how it ends."},
        {"blaine", "Down to my last flame? Then I'll make it a bonfire!"},
        {"giovanni", "You've cornered me. Few live to regret it."},
        {"falkner", "My last wing is my strongest - watch it soar!"},
        {"bugsy", "My research says my last bug is my best bug!"},
        {"whitney", "No fair! ...Fine, my last one's gonna flatten you!"},
        {"morty", "I can see how this ends... can you?"},
        {"chuck", "One more! My fists are still burning!"},
        {"jasmine", "Steel holds firm... even at the very end."},
        {"pryce", "Ice that has lasted this long doesn't crack easily."},
        {"clair", "Dragons never yield - and neither do I!"},
        {"roxanne", "The textbook says a cornered rock is the hardest one to move."},
        {"brawly", "Here comes the big wave - ride it or wipe out!"},
        {"wattson", "Wahahaha! Still got one more jolt in me!"},
        {"flannery", "I'm not burned out yet - this last one's my hottest!"},
        {"norman", "A father doesn't go down easily. Show me everything you've got."},
        {"winona", "Even with one wing left, we'll fly."},
        {"tate", "One left? We can still move as one!"},
        {"liza", "One left? We can still move as one!"},
        {"juan", "Allow me one final, elegant flourish."},
        {"wallace", "The last of my Pokemon is the most beautiful of all."},
    };
    auto it = lines.find(Lower(LeaderShortName(trainer)));
    return it != lines.end() ? it->second : "Down to the last Pokemon - but not out yet!";
}

void DrawLeaderPicture(const Texture2D& t, float cx, float footY, float scale, Color tint, bool flip) {
    float w = t.width * scale, h = t.height * scale;
    Rectangle src{0, 0, (float)(flip ? -t.width : t.width), (float)t.height};
    DrawTexturePro(t, src, {std::floor(cx - w / 2), std::floor(footY - h), w, h}, {0, 0}, 0, tint);
}

void UnloadLeaderPictures() {
    for (auto& kv : gPics) if (kv.second.tex.id) UnloadTexture(kv.second.tex);
    gPics.clear();
}
