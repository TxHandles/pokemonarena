#include "sprites.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "arena.h"
#include "chart.h"
#include "creature_art.h"
#include "data.h"

namespace {

// ------------------------------------------------------------------ built-in text art

struct ArtEntry {
    std::string name;
    std::map<char, Color> pal;
    std::vector<std::string> front, side;
};

constexpr int GW = 16, GH = 16, ART_SIZE = 18;  // 16x16 art + 1px outline

std::vector<CreatureSprites> gSprites;
CreatureSprites gEmpty;

Color Hex(const std::string& h) {
    unsigned v = (unsigned)std::strtoul(h.c_str(), nullptr, 16);
    return Color{(unsigned char)((v >> 16) & 255), (unsigned char)((v >> 8) & 255), (unsigned char)(v & 255), 255};
}

std::vector<ArtEntry> ParseArt(const char* text) {
    std::vector<ArtEntry> out;
    std::istringstream ss(text);
    std::string line;
    int mode = 0;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (line[0] == '[') {
            ArtEntry e;
            std::string head = line.substr(1, line.find(']') - 1);
            size_t lt = head.find('<');
            if (lt != std::string::npos) {  // derived art: copy another entry, then recolor
                std::string base = head.substr(lt + 1);
                head = head.substr(0, lt);
                while (!head.empty() && head.back() == ' ') head.pop_back();
                while (!base.empty() && base.front() == ' ') base.erase(base.begin());
                for (const auto& o : out)
                    if (o.name == base) e = o;
            }
            e.name = head;
            out.push_back(e);
            mode = 0;
            continue;
        }
        if (out.empty()) continue;
        ArtEntry& e = out.back();
        if (line == "FRONT") { mode = 1; continue; }
        if (line == "SIDE") { mode = 2; continue; }
        if (mode == 0 && line.size() >= 3 && line[1] == ' ') { e.pal[line[0]] = Hex(line.substr(2)); continue; }
        if (mode == 1) e.front.push_back(line);
        else if (mode == 2) e.side.push_back(line);
    }
    return out;
}

std::vector<std::string> Normalize(const std::vector<std::string>& rows, int w) {
    std::vector<std::string> out(GH, std::string(w, '.'));
    for (int y = 0; y < GH && y < (int)rows.size(); y++)
        for (int x = 0; x < w && x < (int)rows[y].size(); x++) out[y][x] = rows[y][x];
    return out;
}

std::vector<std::string> MirrorHalf(const std::vector<std::string>& half) {
    std::vector<std::string> out = Normalize(half, GW / 2);
    for (auto& r : out) r = r + std::string(r.rbegin(), r.rend());
    return out;
}

std::vector<std::string> MakeBack(std::vector<std::string> front) {
    for (auto& r : front)
        for (auto& c : r)
            if (c == 'P' || c == 'W' || c == 'R' || c == 'M' || c == 'E') c = 'B';
    return front;
}

Image ArtImage(const std::vector<std::string>& rows, const std::map<char, Color>& pal) {
    Image img = GenImageColor(ART_SIZE, ART_SIZE, BLANK);
    bool filled[ART_SIZE][ART_SIZE] = {};
    Color shade = pal.count('S') ? pal.at('S') : Color{60, 60, 60, 255};
    Color outline{(unsigned char)(shade.r * 0.35f), (unsigned char)(shade.g * 0.35f), (unsigned char)(shade.b * 0.40f), 255};
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++) {
            char c = rows[y][x];
            if (c == '.' || c == ' ') continue;
            auto it = pal.find(c);
            ImageDrawPixel(&img, x + 1, y + 1, it != pal.end() ? it->second : MAGENTA);
            filled[y + 1][x + 1] = true;
        }
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    for (int y = 0; y < ART_SIZE; y++)
        for (int x = 0; x < ART_SIZE; x++) {
            if (filled[y][x]) continue;
            for (int k = 0; k < 4; k++) {
                int nx = x + dx[k], ny = y + dy[k];
                if (nx >= 0 && ny >= 0 && nx < ART_SIZE && ny < ART_SIZE && filled[ny][nx]) {
                    ImageDrawPixel(&img, x, y, outline);
                    break;
                }
            }
        }
    return img;
}

// ------------------------------------------------------------------ frames

SpriteFrame MakeFrame(const Image& img) {
    SpriteFrame f;
    f.w = img.width;
    f.h = img.height;
    f.tex = LoadTextureFromImage(img);
    SetTextureFilter(f.tex, TEXTURE_FILTER_POINT);
    Image wh = ImageCopy(img);
    Color* px = LoadImageColors(wh);
    for (int i = 0; i < wh.width * wh.height; i++) px[i] = Color{255, 255, 255, px[i].a};
    Image wimg = {px, wh.width, wh.height, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    f.white = LoadTextureFromImage(wimg);
    SetTextureFilter(f.white, TEXTURE_FILTER_POINT);
    UnloadImageColors(px);
    UnloadImage(wh);
    return f;
}

Image Mirrored(const Image& img) {
    Image c = ImageCopy(img);
    ImageFlipHorizontal(&c);
    return c;
}

void FromArt(CreatureSprites& cs, const ArtEntry& e) {
    std::vector<std::string> front = MirrorHalf(e.front), side = Normalize(e.side, GW);
    Image v[4] = {ArtImage(front, e.pal), ArtImage(MakeBack(front), e.pal), ArtImage(side, e.pal), Image{}};
    v[3] = Mirrored(v[2]);
    for (int i = 0; i < 4; i++) {
        cs.view[i][0] = MakeFrame(v[i]);
        cs.frames[i] = 1;
        UnloadImage(v[i]);
    }
}

void Placeholder(CreatureSprites& cs) {
    ArtEntry e;
    e.pal['B'] = MAGENTA;
    e.pal['S'] = Color{120, 0, 120, 255};
    for (int i = 0; i < 16; i++) e.front.push_back(i < 2 ? "........" : "..BBBBBB");
    for (int i = 0; i < 16; i++) e.side.push_back(i < 2 ? "................" : "..BBBBBBBBBBBB..");
    FromArt(cs, e);
}

// ------------------------------------------------------------------ image files

std::vector<std::string> Tokens(const std::string& s) {
    std::vector<std::string> t;
    std::string cur;
    for (char c : s) {
        if (std::isalnum((unsigned char)c)) cur += (char)std::tolower((unsigned char)c);
        else if (!cur.empty()) { t.push_back(cur); cur.clear(); }
    }
    if (!cur.empty()) t.push_back(cur);
    return t;
}

int ViewFromToken(const std::string& t) {
    if (t == "front" || t == "mae" || t == "down" || t == "south") return VIEW_FRONT;
    if (t == "back" || t == "ushiro" || t == "up" || t == "north") return VIEW_BACK;
    if (t == "left" || t == "hidari" || t == "west") return VIEW_LEFT;
    if (t == "right" || t == "migi" || t == "east") return VIEW_RIGHT;
    return -1;
}

struct FileSlot {
    Image img{};
    int rank = -1;  // higher wins: numbered "_No" charts (2) beat plain copies (1)
    std::string file;
};

bool LoadFromDir(CreatureSprites& cs, const std::string& dir, std::string& log) {
    FilePathList files = LoadDirectoryFiles(dir.c_str());
    // Numbered "_No" charts first: their numbers say exactly which squares are colored
    // (white ones included), and they're usually lossless PNGs. Plain copies are the fallback.
    std::vector<std::string> paths(files.paths, files.paths + files.count);
    UnloadDirectoryFiles(files);
    auto numberedName = [](const std::string& p) {
        for (const auto& t : Tokens(GetFileNameWithoutExt(p.c_str()))) if (t == "no") return true;
        return false;
    };
    std::sort(paths.begin(), paths.end(), [&](const std::string& a, const std::string& b) {
        bool na = numberedName(a), nb = numberedName(b);
        return na != nb ? na : a < b;
    });
    FileSlot slots[4][2];
    FileSlot portrait;
    int used = 0;

    for (const std::string& path : paths) {
        if (!IsPathFile(path.c_str())) continue;
        if (!IsFileExtension(path.c_str(), ".png;.PNG;.jpg;.JPG;.jpeg;.JPEG;.bmp;.BMP;.gif;.GIF;.qoi")) continue;
        std::string name = GetFileNameWithoutExt(path.c_str());
        std::vector<std::string> tk = Tokens(name);

        int view = -1, frame = 0;
        bool numbered = false;
        for (size_t k = 0; k < tk.size(); k++) {
            if (tk[k] == "no") numbered = true;
            int v = ViewFromToken(tk[k]);
            if (v >= 0 && view < 0) {
                view = v;
                if (k + 1 < tk.size() && (tk[k + 1] == "1" || tk[k + 1] == "2")) frame = tk[k + 1][0] - '1';
            }
        }
        // Also accept front1 / walk2 style: trailing digit glued to the word.
        if (view < 0) {
            for (const auto& t : tk) {
                if (t.size() > 1 && (t.back() == '1' || t.back() == '2')) {
                    int v = ViewFromToken(t.substr(0, t.size() - 1));
                    if (v >= 0) { view = v; frame = t.back() - '1'; break; }
                }
            }
        }
        int rank = numbered ? 2 : 1;
        FileSlot& slot = view >= 0 ? slots[view][frame] : portrait;
        if (slot.rank >= rank) {
            log += "    skipped " + std::string(GetFileName(path.c_str())) + " (same view as " + slot.file + ")\n";
            continue;
        }

        Image raw = LoadImage(path.c_str());
        std::string note;
        bool chart = false;
        Image spr = ExtractSprite(raw, note, chart, numbered);
        UnloadImage(raw);
        if (spr.width == 0) {
            log += "    could not use " + std::string(GetFileName(path.c_str())) + ": " + note + "\n";
            continue;
        }
        if (slot.rank >= 0) UnloadImage(slot.img);
        slot.img = spr;
        slot.rank = rank;
        slot.file = GetFileName(path.c_str());
        used++;
        const char* vn[4] = {"front", "back", "left", "right"};
        std::string where = view >= 0 ? TextFormat("%s frame %d", vn[view], frame + 1) : "menu portrait";
        log += "    " + slot.file + " -> " + where + "  [" + note + "]\n";

        if (chart) {  // save what was read so it can be checked or hand-edited
            std::string outDir = dir + "/converted";
            if (!DirectoryExists(outDir.c_str())) MakeDirectory(outDir.c_str());
            std::string outName = view >= 0 ? TextFormat("%s_%d.png", vn[view], frame + 1) : "portrait.png";
            ExportImage(spr, (outDir + "/" + outName).c_str());
        }
    }
    if (!used) return false;

    // Fill in missing views: mirror left/right, front <-> back, anything -> front.
    Image views[4][2];
    bool have[4][2] = {};
    for (int v = 0; v < 4; v++)
        for (int f = 0; f < 2; f++)
            if (slots[v][f].rank >= 0) { views[v][f] = slots[v][f].img; have[v][f] = true; }
    auto copyView = [&](int dst, int src, bool mirror, const char* why) {
        for (int f = 0; f < 2; f++)
            if (have[src][f]) {
                views[dst][f] = mirror ? Mirrored(views[src][f]) : ImageCopy(views[src][f]);
                have[dst][f] = true;
            }
        if (*why) log += std::string("    ") + why + "\n";
    };
    if (!have[VIEW_FRONT][0]) {
        if (have[VIEW_LEFT][0]) copyView(VIEW_FRONT, VIEW_LEFT, false, "no front view: using the left view");
        else if (have[VIEW_RIGHT][0]) copyView(VIEW_FRONT, VIEW_RIGHT, false, "no front view: using the right view");
        else if (have[VIEW_BACK][0]) copyView(VIEW_FRONT, VIEW_BACK, false, "no front view: using the back view");
        else if (portrait.rank >= 0) {
            views[VIEW_FRONT][0] = ImageCopy(portrait.img);
            have[VIEW_FRONT][0] = true;
            log += "    no view names in file names: using the picture for every direction\n";
        }
    }
    if (!have[VIEW_LEFT][0] && have[VIEW_RIGHT][0]) copyView(VIEW_LEFT, VIEW_RIGHT, true, "no left view: mirroring the right view");
    if (!have[VIEW_RIGHT][0] && have[VIEW_LEFT][0]) copyView(VIEW_RIGHT, VIEW_LEFT, true, "no right view: mirroring the left view");
    if (!have[VIEW_LEFT][0]) copyView(VIEW_LEFT, VIEW_FRONT, false, "no side views: using the front view");
    if (!have[VIEW_RIGHT][0]) copyView(VIEW_RIGHT, VIEW_LEFT, true, "");
    if (!have[VIEW_BACK][0]) copyView(VIEW_BACK, VIEW_FRONT, false, "no back view: using the front view");

    int maxDim = 1;
    for (int v = 0; v < 4; v++) {
        cs.frames[v] = have[v][1] ? 2 : 1;
        for (int f = 0; f < cs.frames[v]; f++) {
            cs.view[v][f] = MakeFrame(views[v][f]);
            maxDim = std::max(maxDim, std::max(views[v][f].width, views[v][f].height));
            UnloadImage(views[v][f]);
        }
    }
    if (portrait.rank >= 0) {
        cs.portrait = MakeFrame(portrait.img);
        cs.hasPortrait = true;
        UnloadImage(portrait.img);
    }
    cs.fromFiles = true;
    cs.pixel = maxDim <= 32 ? SPRITE_PIXEL : 26.0f / maxDim;
    return true;
}

float Snap(float v) { return std::round(v * CANVAS_ZOOM) / CANVAS_ZOOM; }

float SnapScale(float s) {
    float q = std::round(s * CANVAS_ZOOM) / CANVAS_ZOOM;
    return q >= 1.0f / CANVAS_ZOOM ? q : s;
}

void DrawFrame(const SpriteFrame& fr, bool white, float x, float bottom, float pw, float ph, Color tint) {
    if (fr.w == 0) return;
    float w = fr.w * pw, h = fr.h * ph;
    Rectangle src{0, 0, (float)fr.w, (float)fr.h};
    Rectangle dst{Snap(x - w / 2), Snap(bottom - h), w, h};
    DrawTexturePro(white ? fr.white : fr.tex, src, dst, {0, 0}, 0, tint);
}

} // namespace

void LoadAllSprites(std::string& log) {
    std::vector<ArtEntry> art = ParseArt(kCreatureArt);
    const auto& species = AllSpecies();
    gSprites.clear();
    gSprites.resize(species.size());
    for (size_t i = 0; i < species.size(); i++) {
        const SpeciesDef& sp = species[i];
        CreatureSprites& cs = gSprites[i];
        bool done = false;
        if (!sp.spriteDir.empty()) {
            std::string sub;
            done = LoadFromDir(cs, sp.spriteDir, sub);
            log += "  " + sp.name + " sprites from " + std::string(GetFileName(sp.spriteDir.c_str())) + "/:\n" + sub;
            if (!done) log += "    no usable images found, using " + (sp.artName.empty() ? std::string("placeholder") : "built-in art for " + sp.artName) + "\n";
        }
        if (!done) {
            const ArtEntry* found = nullptr;
            for (const auto& e : art)
                if (e.name == sp.artName) { found = &e; break; }
            if (found) FromArt(cs, *found);
            else Placeholder(cs);
            cs.pixel = SPRITE_PIXEL;
        }
        cs.pixel = SnapScale(cs.pixel * sp.drawScale);   // Pokedex size (data.cpp)
    }
}

void UnloadAllSprites() {
    auto un = [](SpriteFrame& f) {
        if (f.w) { UnloadTexture(f.tex); UnloadTexture(f.white); }
    };
    for (auto& cs : gSprites) {
        for (int v = 0; v < 4; v++)
            for (int f = 0; f < 2; f++) un(cs.view[v][f]);
        un(cs.portrait);
    }
    gSprites.clear();
}

const CreatureSprites& SpritesFor(int speciesIndex) {
    if (speciesIndex < 0 || speciesIndex >= (int)gSprites.size()) return gEmpty;
    return gSprites[speciesIndex];
}

void DrawCreature(int idx, int view, int frame, float x, float bottom, float sx, float sy, Color tint, float whiteAmount) {
    const CreatureSprites& cs = SpritesFor(idx);
    const SpriteFrame& fr = cs.view[view][frame % std::max(1, cs.frames[view])];
    DrawFrame(fr, false, x, bottom, cs.pixel * sx, cs.pixel * sy, tint);
    if (whiteAmount > 0) DrawFrame(fr, true, x, bottom, cs.pixel * sx, cs.pixel * sy, Fade(WHITE, whiteAmount * tint.a / 255.0f));
}

float CreatureHeight(int idx, int view) {
    const CreatureSprites& cs = SpritesFor(idx);
    return cs.view[view][0].h * cs.pixel;
}

void DrawSpeciesFit(int idx, int view, int cx, int bottomY, int box, Color tint, bool usePortrait) {
    const CreatureSprites& cs = SpritesFor(idx);
    const SpriteFrame& fr = (usePortrait && cs.hasPortrait) ? cs.portrait : cs.view[view][0];
    if (fr.w == 0) return;
    float s = SnapScale((float)box / std::max(fr.w, fr.h));
    if (s * CANVAS_ZOOM >= 1.0f) s = std::floor(s * CANVAS_ZOOM) / CANVAS_ZOOM;
    DrawFrame(fr, false, (float)cx, (float)bottomY, s, s, tint);
}

void DrawSpecies(int idx, int view, int x, int bottomY, int scale, Color tint) {
    DrawSpeciesFit(idx, view, x, bottomY, ART_SIZE * scale, tint, true);
}
