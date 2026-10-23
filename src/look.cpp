// The trainer creator's sprites: heads and outfits cut from FireRed's people (the cut is at the neck), recoloured.
#include "look.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include "embedded.h"

namespace {
constexpr int NECK = 23;   // FireRed's people: the rows above are the head and hair (drawn in the bottom of the cell)
constexpr int CW = 32;     // the cells: 32x32 (FireRed's 16-wide people in the middle, HeartGold's as wide as they are)

std::vector<std::string> gNames;   // World/chars.txt
Image gChars{}, gActs{};
bool gLoaded = false;
std::map<std::string, Texture2D> gCharTex, gActTex;

void LoadSheets() {
    if (gLoaded) return;
    gLoaded = true;
    std::string dir = DataDir() + "World/";
    if (char* t = LoadFileText((dir + "chars.txt").c_str())) {
        std::istringstream ss(t);
        std::string n;
        while (ss >> n) gNames.push_back(n);
        UnloadFileText(t);
    }
    if (FileExists((dir + "chars.png").c_str())) { gChars = LoadImage((dir + "chars.png").c_str()); ImageFormat(&gChars, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8); }
    if (FileExists((dir + "actions.png").c_str())) { gActs = LoadImage((dir + "actions.png").c_str()); ImageFormat(&gActs, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8); }
}
int RowOf(const std::string& n) {
    for (size_t i = 0; i < gNames.size(); i++) if (gNames[i] == n) return (int)i;
    return -1;
}
int TopIn(const Image& im, int cx0, int cy0, int cw = CW) {   // a cell's first row with something drawn
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < cw; x++) if (GetImageColor(im, cx0 + x, cy0 + y).a) return y;
    return 32;
}
// Where a person's head ends: FireRed's at row 23; HeartGold's people are taller, their heads ~15 rows from the top
int NeckOf(int row) {
    if (row < 0 || row >= (int)gNames.size() || gNames[row].rfind("hg_", 0) != 0 || !gChars.data) return NECK;
    return std::min(28, TopIn(gChars, CW, row * 32) + 15);
}

struct Hsv { float h, s, v; };
Hsv ToHsv(Color c) { Vector3 x = ColorToHSV(c); return {x.x, x.y, x.z}; }
bool SkinHue(Color c) {
    Hsv x = ToHsv(c);
    return c.a > 0 && (x.h <= 50 || x.h >= 345) && x.s >= 0.08f && x.s <= 0.7f && x.v >= 0.45f;
}
// A person's skin: the skin-coloured shades in the middle of their face (standing, facing down), so a brown hat
// or brown hair isn't taken for skin.
std::vector<unsigned> SkinOf(const Image& sheet, int row) {
    std::vector<unsigned> out;
    if (!sheet.data || row < 0) return out;
    int top = TopIn(sheet, CW, row * 32);
    for (int y = top + 6; y < std::min(32, top + 16); y++)
        for (int x = CW + 12; x < CW + 20; x++) {
            Color c = GetImageColor(sheet, x, row * 32 + y);
            if (!SkinHue(c)) continue;
            unsigned k = (unsigned)c.r << 16 | (unsigned)c.g << 8 | c.b;
            if (std::find(out.begin(), out.end(), k) == out.end()) out.push_back(k);
        }
    return out;
}
bool In(const std::vector<unsigned>& set, Color c) {
    unsigned k = (unsigned)c.r << 16 | (unsigned)c.g << 8 | c.b;
    return std::find(set.begin(), set.end(), k) != set.end();
}
// A colour preset: hue, saturation, brightness factor (index 0 = as drawn)
struct Preset { const char* name; float h, s, vm; };
const Preset kColours[] = {{"As drawn", 0, 0, 1}, {"Red", 2, 0.78f, 1.0f}, {"Orange", 24, 0.8f, 1.05f}, {"Brown", 28, 0.6f, 0.72f},
                           {"Blonde", 48, 0.62f, 1.12f}, {"Green", 120, 0.6f, 0.9f}, {"Teal", 172, 0.6f, 0.9f}, {"Blue", 215, 0.7f, 1.0f},
                           {"Navy", 228, 0.6f, 0.62f}, {"Purple", 275, 0.55f, 0.92f}, {"Pink", 330, 0.45f, 1.12f}, {"Black", 230, 0.18f, 0.42f},
                           {"Grey", 0, 0.0f, 0.85f}, {"White", 0, 0.0f, 1.3f}};
const char* kSkins[] = {"As drawn", "Fair", "Tan", "Brown", "Dark"};

Color Recolour(Color c, int preset) {
    if (preset <= 0 || preset >= (int)(sizeof kColours / sizeof kColours[0]) || c.a == 0) return c;
    Hsv x = ToHsv(c);
    if (x.v < 0.2f) return c;   // outlines stay
    const Preset& p = kColours[preset];
    float s = p.s <= 0 ? 0 : std::min(1.0f, p.s * (0.55f + x.s * 0.6f));
    float v = std::min(1.0f, x.v * p.vm);
    Color o = ColorFromHSV(p.h, s, v);
    o.a = c.a;
    return o;
}
Color Skin(Color c, int tone) {
    if (tone <= 0) return c;
    Hsv x = ToHsv(c);
    static const float kV[] = {1, 1.08f, 0.86f, 0.66f, 0.48f}, kS[] = {1, 0.8f, 1.15f, 1.35f, 1.4f};
    Color o = ColorFromHSV(tone >= 3 ? 24.0f : x.h, std::min(1.0f, x.s * kS[tone]), std::min(1.0f, x.v * kV[tone]));
    o.a = c.a;
    return o;
}
} // namespace

TrainerLook ParseLook(const std::string& s) {
    TrainerLook l;
    std::istringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ';')) {
        size_t eq = part.find('=');
        if (eq == std::string::npos) continue;
        std::string k = part.substr(0, eq), v = part.substr(eq + 1);
        if (k == "type") l.type = std::atoi(v.c_str()) ? 1 : 0;
        else if (k == "head") l.head = v;
        else if (k == "body") l.body = v;
        else if (k == "hair") l.hair = std::atoi(v.c_str());
        else if (k == "cloth") l.cloth = std::atoi(v.c_str());
        else if (k == "skin") l.skin = std::atoi(v.c_str());
    }
    return l;
}
std::string LookText(const TrainerLook& l) {
    if (l.Empty()) return "";
    return "type=" + std::to_string(l.type) + ";head=" + l.head + ";body=" + l.body + ";hair=" + std::to_string(l.hair) +
           ";cloth=" + std::to_string(l.cloth) + ";skin=" + std::to_string(l.skin);
}

// HeartGold / SoulSilver's people (tools/hgss_sprites.py)
#define HG_PEOPLE "hg_elm", "hg_silver", "hg_steven", "hg_kurt", "hg_little_boy", "hg_maizie", "hg_cap_boy", "hg_straw_hat", "hg_mom", \
    "hg_pigtails", "hg_young_man", "hg_boy", "hg_blond_boy", "hg_spiky_man", "hg_girl", "hg_jasmine", "hg_bun_woman", "hg_bald_man", \
    "hg_woman", "hg_pryce", "hg_granny", "hg_aunt", "hg_falkner", "hg_mr_pokemon", "hg_glasses_man", "hg_blue_hair", "hg_bugsy", \
    "hg_whitney", "hg_morty", "hg_bruno", "hg_erika", "hg_old_white", "hg_clair", "hg_will", "hg_koga", "hg_chuck", "hg_karen", \
    "hg_lance", "hg_twintails", "hg_surge", "hg_sabrina", "hg_misty", "hg_headband_girl", "hg_janine", "hg_brock", "hg_blaine", \
    "hg_blue", "hg_red", "hg_officer", "hg_monk", "hg_rocket_f", "hg_rocket_m", "hg_ariana", "hg_archer", "hg_cook", "hg_flame_man", \
    "hg_kimono_girl", "hg_lass", "hg_captain", "hg_dark_girl", "hg_spiky_punk", "hg_eusine", "hg_elder", "hg_tank_man", "hg_pink_girl", \
    "hg_green_girl", "hg_navy_girl", "hg_cap_girl", "hg_hat_girl", "hg_grandpa", "hg_giovanni", "hg_bearded_man", "hg_sage", \
    "hg_petrel", "hg_proton", "hg_worker"
static const std::vector<std::string>& Filter(std::vector<std::string>& cache, std::initializer_list<const char*> want) {
    LoadSheets();
    if (cache.empty()) for (const char* n : want) if (RowOf(n) >= 0) cache.push_back(n);
    if (cache.empty()) cache.push_back("player");
    return cache;
}
const std::vector<std::string>& LookHeads() {
    static std::vector<std::string> v;
    return Filter(v, {"player", "leaf", "rival", "youngster", "lass", "bug_catcher", "camper", "picnicker", "beauty", "cooltrainer_m",
                      "cooltrainer_f", "black_belt", "crush_girl", "hiker", "sailor", "fisher", "rocker", "swimmer_m_land", "swimmer_f_land",
                      "rocket_m", "rocket_f", "boy", "little_girl", "woman_1", "woman_2", "woman_3", "man", "scientist", "poke_maniac",
                      "gym_guy", "worker_m", "worker_f", "chef", "policeman", "gentleman", "channeler", "misty", "brock", "erika", "lorelei",
                      "sabrina", "lance", "bill", "daisy", "mom", "nurse", "old_man_1", "old_woman", HG_PEOPLE});
}
const std::vector<std::string>& LookBodies() {
    static std::vector<std::string> v;
    return Filter(v, {"player", "leaf", "rival", "youngster", "lass", "bug_catcher", "camper", "picnicker", "beauty", "cooltrainer_m",
                      "cooltrainer_f", "black_belt", "crush_girl", "hiker", "sailor", "fisher", "rocker", "swimmer_m_land", "swimmer_f_land",
                      "rocket_m", "rocket_f", "boy", "little_girl", "woman_1", "woman_2", "woman_3", "man", "scientist", "poke_maniac",
                      "gym_guy", "worker_m", "worker_f", "chef", "policeman", "gentleman", "channeler", "misty", "brock", "erika", "lorelei",
                      "sabrina", "lance", "bill", "daisy", "mom", "nurse", HG_PEOPLE});
}
const std::vector<const char*>& LookColourNames() {
    static std::vector<const char*> v;
    if (v.empty()) for (const auto& p : kColours) v.push_back(p.name);
    return v;
}
const std::vector<const char*>& LookSkinNames() {
    static std::vector<const char*> v(std::begin(kSkins), std::end(kSkins));
    return v;
}

namespace {
// Your walking sheet as an image: the head person's head and hair over the body person's outfit, recoloured.
Image ComposeWalk(const TrainerLook& l, int* neckOut = nullptr) {
    LoadSheets();
    Image out = GenImageColor(9 * CW, 32, BLANK);
    int hr = RowOf(l.head.empty() ? "player" : l.head), br = RowOf(l.body.empty() ? "player" : l.body);
    if (!gChars.data || hr < 0 || br < 0) return out;
    std::vector<unsigned> headSkin = SkinOf(gChars, hr), bodySkin = SkinOf(gChars, br);
    int nh = NeckOf(hr), nb = NeckOf(br), shift = nb - nh;   // the head sits on the outfit's neck
    if (neckOut) *neckOut = nb;
    for (int f = 0; f < 9; f++) {
        int top = TopIn(gChars, f * CW, hr * 32);   // a hat or hair can share the face's shades, so skin starts 6 rows down
        for (int y = 0; y < 32; y++)
            for (int x = 0; x < CW; x++) {
                bool headPart = y < nb;
                int sy = headPart ? y - shift : y;
                if (sy < 0 || sy >= 32 || (headPart && sy >= nh)) continue;
                Color c = GetImageColor(gChars, f * CW + x, (headPart ? hr : br) * 32 + sy);
                if (c.a == 0) continue;
                bool skin = headPart ? sy >= top + 6 && In(headSkin, c) : In(bodySkin, c);
                if (skin) c = Skin(c, l.skin);
                else if (ToHsv(c).s > 0.1f || ToHsv(c).v > 0.85f) c = Recolour(c, headPart ? l.hair : l.cloth);
                ImageDrawPixel(&out, f * CW + x, y, c);
            }
    }
    return out;
}
// A head in one cell: its top row, and the middle of its columns (rows top..bottom-1).
void HeadSpan(const Image& im, int cx0, int cy0, int cw, int bottom, int& top, float& mid, int& x0, int& x1) {
    top = 32;
    for (int y = 0; y < 32 && top == 32; y++)
        for (int x = 0; x < cw; x++) if (GetImageColor(im, cx0 + x, cy0 + y).a) { top = y; break; }
    x0 = cw; x1 = -1;
    for (int y = top; y < std::min(32, bottom); y++)
        for (int x = 0; x < cw; x++) if (GetImageColor(im, cx0 + x, cy0 + y).a) { x0 = std::min(x0, x); x1 = std::max(x1, x); }
    mid = x1 >= x0 ? (x0 + x1) / 2.0f : cw / 2.0f;
}
} // namespace

Texture2D LookChars(const TrainerLook& l) {
    std::string key = LookText(l);
    auto it = gCharTex.find(key);
    if (it != gCharTex.end()) return it->second;
    Image out = ComposeWalk(l);
    Texture2D t = LoadTextureFromImage(out);
    UnloadImage(out);
    gCharTex[key] = t;
    return t;
}

Texture2D LookActions(const TrainerLook& l) {
    std::string key = LookText(l) + "|act";
    auto it = gActTex.find(key);
    if (it != gActTex.end()) return it->second;
    LoadSheets();
    int w = gActs.data ? gActs.width : 32;
    Image out = GenImageColor(w, 96, BLANK);
    const char* base = l.type ? "leaf" : "player";
    bool ownHead = !l.head.empty() && l.head != base;   // someone else's head: put it on Red's / Leaf's poses
    int walkNeck = NECK;
    Image walk = ownHead ? ComposeWalk(l, &walkNeck) : Image{};
    int baseRow = RowOf(base);
    if (gActs.data) {
        // the poses' skin: the shades Red's / Leaf's own face uses on the walking sheet
        std::vector<unsigned> skin = SkinOf(gChars, baseRow);
        for (int row = 0; row < 3; row++) {   // bike, surf, fish: Red's (boy) or Leaf's (girl) rows, in your colours
            int src = row * 2 + (l.type ? 1 : 0);
            for (int cx = 0; cx < w / 32; cx++) {
                int top = 32;   // the character's top in this cell: the head is the 10 rows from there
                for (int y = 0; y < 32 && top == 32; y++)
                    for (int x = 0; x < 32; x++) if (GetImageColor(gActs, cx * 32 + x, src * 32 + y).a) { top = y; break; }
                if (top == 32) continue;
                for (int y = 0; y < 32; y++)
                    for (int x = 0; x < 32; x++) {
                        Color c = GetImageColor(gActs, cx * 32 + x, src * 32 + y);
                        if (c.a == 0) continue;
                        if (y >= top + 6 && In(skin, c)) c = Skin(c, l.skin);
                        else if (ToHsv(c).s > 0.25f) c = Recolour(c, y < top + 10 ? l.hair : l.cloth);
                        ImageDrawPixel(&out, cx * 32 + x, row * 32 + y, c);
                    }
                if (!ownHead || !walk.data || baseRow < 0) continue;
                // which way it faces: bike cells go down x3, up x3, left x3; surf down, up, left; fishing left x4, up x4, down x4
                int dir = row == 0 ? cx / 3 : row == 1 ? cx : (cx < 4 ? 2 : cx < 8 ? 1 : 0);
                if (dir > 2) continue;
                int wc = dir == 0 ? 1 : dir == 1 ? 4 : 7;   // the standing frame that way
                int rt, ax0, ax1, rx0, rx1, ot, ox0, ox1;
                float rm, am, om;
                HeadSpan(gChars, wc * CW, baseRow * 32, CW, NECK, rt, rm, rx0, rx1);   // Red's / Leaf's head walking
                int hh = NECK - rt;                                                   // ...its height
                int at;
                HeadSpan(gActs, cx * 32, src * 32, 32, top + hh, at, am, ax0, ax1);   // ...and on this pose
                int neck = at + hh;
                for (int y = at; y < neck; y++)   // off with Red's head
                    for (int x = ax0; x <= ax1; x++) ImageDrawPixel(&out, cx * 32 + x, row * 32 + y, BLANK);
                HeadSpan(walk, wc * CW, 0, CW, walkNeck, ot, om, ox0, ox1);         // your head
                int dx = (int)std::lround(am - om);
                for (int y = ot; y < walkNeck; y++)
                    for (int x = 0; x < CW; x++) {
                        Color c = GetImageColor(walk, wc * CW + x, y);
                        if (c.a == 0) continue;
                        int px = cx * 32 + x + dx, py = row * 32 + neck - (walkNeck - y);
                        if (px < cx * 32 || px >= cx * 32 + 32 || py < row * 32 || py >= row * 32 + 32) continue;
                        ImageDrawPixel(&out, px, py, c);
                    }
                (void)ox0; (void)ox1;
            }
        }
    }
    if (walk.data) UnloadImage(walk);
    Texture2D t = LoadTextureFromImage(out);
    UnloadImage(out);
    gActTex[key] = t;
    return t;
}

void UnloadLooks() {
    for (auto& [k, t] : gCharTex) UnloadTexture(t);
    for (auto& [k, t] : gActTex) UnloadTexture(t);
    gCharTex.clear();
    gActTex.clear();
    if (gChars.data) UnloadImage(gChars);
    if (gActs.data) UnloadImage(gActs);
    gChars = gActs = Image{};
    gNames.clear();
    gLoaded = false;
}
