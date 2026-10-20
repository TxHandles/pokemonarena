#include "storymap.h"
#include "embedded.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <vector>
#include "chart.h"
#include "sprites.h"
#include "story.h"
#include "ui.h"
#include "vec.h"
#include "arena.h"

namespace {
struct RegionMap { Texture2D tex{}; bool has = false; float w = 1142, h = 857; };
std::vector<RegionMap> gMaps;   // one per story region
float gW = 1142, gH = 857;   // the map coordinate space last drawn (image size, or a default for the drawn map)

// Badge pictures: Badges/<Name>.png next to the game (a plain picture or a pixel-art chart).
struct BadgePic { bool tried = false; Texture2D tex{}; };
std::map<std::string, BadgePic> gBadges;

std::string Lower(std::string s) { for (char& c : s) c = (char)std::tolower((unsigned char)c); return s; }

const Texture2D* BadgePicture(const std::string& badge) {
    BadgePic& e = gBadges[badge];
    if (e.tried) return e.tex.id ? &e.tex : nullptr;
    if (!IsWindowReady()) return nullptr;
    e.tried = true;
    std::string dir = DataDir() + "Badges";
    if (!DirectoryExists(dir.c_str())) return nullptr;
    FilePathList fl = LoadDirectoryFiles(dir.c_str());
    std::string want = Lower(badge), best;
    int bestRank = 0;
    for (unsigned i = 0; i < fl.count; i++) {
        std::string path = fl.paths[i];
        std::string file = Lower(GetFileName(fl.paths[i]));
        std::string ext = Lower(GetFileExtension(fl.paths[i]));
        if (ext != ".png" && ext != ".jpg" && ext != ".bmp" && ext != ".gif" && ext != ".qoi") continue;
        if (file.find(want) == std::string::npos) continue;
        // a chart with numbers in the squares reads best (like the Pokemon pictures)
        int rank = (file.find("_no.") != std::string::npos || file.find("-no.") != std::string::npos) ? 2 : 1;
        if (rank > bestRank) { bestRank = rank; best = path; }
    }
    UnloadDirectoryFiles(fl);
    if (best.empty()) return nullptr;
    Image img = LoadImage(best.c_str());
    if (!img.data) return nullptr;
    std::string note;
    bool chart = false;
    Image spr = ExtractSprite(img, note, chart, bestRank == 2);
    UnloadImage(img);
    if (spr.width <= 0) return nullptr;
    e.tex = LoadTextureFromImage(spr);
    SetTextureFilter(e.tex, TEXTURE_FILTER_POINT);
    UnloadImage(spr);
    return &e.tex;
}
} // namespace

bool HasBadgePicture(const std::string& badge) { return BadgePicture(badge) != nullptr; }

void LoadStoryMap() {
    UnloadStoryMap();
    gMaps.assign(RegionCount(), RegionMap());
    for (int r = 0; r < RegionCount(); r++) {
        RegionMap& m = gMaps[r];
        const auto& R = RegionInfo(r);
        if (!R.mapImage.empty()) {
            std::string path = DataDir() + R.mapImage;
            if (FileExists(path.c_str())) {
                Image img = LoadImage(path.c_str());
                if (img.data) {
                    m.tex = LoadTextureFromImage(img);
                    SetTextureFilter(m.tex, TEXTURE_FILTER_BILINEAR);
                    m.w = (float)img.width;
                    m.h = (float)img.height;
                    m.has = true;
                    UnloadImage(img);
                }
            }
        }
        if (!m.has) {  // size the drawn map to fit every point
            float mx = 400, my = 300;
            for (int i = R.first; i < R.end; i++) { mx = std::max(mx, Story().stages[i].mapX + 60); my = std::max(my, Story().stages[i].mapY + 60); }
            for (const auto& t : R.towns) { mx = std::max(mx, t.x + 60); my = std::max(my, t.y + 60); }
            m.w = std::max(mx, my * 4 / 3);
            m.h = m.w * 3 / 4;
        }
    }
    if (!gMaps.empty()) { gW = gMaps[0].w; gH = gMaps[0].h; }
}

void UnloadStoryMap() {
    for (auto& m : gMaps) if (m.has) UnloadTexture(m.tex);
    gMaps.clear();
    for (auto& kv : gBadges) if (kv.second.tex.id) UnloadTexture(kv.second.tex);
    gBadges.clear();
}

bool StoryMapHasImage() { return !gMaps.empty() && gMaps[0].has; }

Color BadgeColor(const std::string& b) {
    if (b == "Boulder") return {170, 150, 120, 255};
    if (b == "Cascade") return {90, 160, 240, 255};
    if (b == "Thunder") return {250, 210, 60, 255};
    if (b == "Rainbow") return {120, 210, 110, 255};
    if (b == "Soul") return {200, 100, 220, 255};
    if (b == "Marsh") return {240, 190, 60, 255};
    if (b == "Volcano") return {240, 100, 50, 255};
    if (b == "Earth") return {100, 200, 120, 255};
    if (b == "Zephyr") return {150, 190, 230, 255};
    if (b == "Hive") return {200, 60, 60, 255};
    if (b == "Plain") return {240, 200, 80, 255};
    if (b == "Fog") return {170, 120, 220, 255};
    if (b == "Storm") return {230, 140, 60, 255};
    if (b == "Mineral") return {190, 200, 215, 255};
    if (b == "Glacier") return {150, 220, 250, 255};
    if (b == "Rising") return {90, 110, 230, 255};
    return {200, 200, 210, 255};
}

void DrawBadge(const std::string& b, int x, int y, bool have, float r) {
    if (const Texture2D* t = BadgePicture(b)) {   // the real badge; a dark outline of it until it's won
        float box = r * 2 + 2;
        float sc = box / std::max(t->width, t->height);
        if (sc >= 1) sc = std::floor(sc);         // whole-pixel steps stay crisp
        float w = t->width * sc, h = t->height * sc;
        Rectangle dst{std::floor(x - w / 2), std::floor(y - h / 2), w, h};
        DrawTexturePro(*t, {0, 0, (float)t->width, (float)t->height}, dst, {0, 0}, 0, have ? WHITE : Color{48, 44, 60, 255});
        return;
    }
    Color c = have ? BadgeColor(b) : Color{60, 56, 72, 255};
    DrawPoly({(float)x, (float)y}, 6, r, 0, have ? Color{30, 26, 40, 255} : Color{40, 36, 50, 255});
    DrawPoly({(float)x, (float)y}, 6, r - 1.5f, 30, c);
    if (have) DrawRectangle(x - 2, y - 3, 2, 2, Fade(WHITE, 0.8f));
}

Vector2 StoryMapPoint(int x, int y, int w, int h, float mx, float my) {
    float sc = std::min(w / gW, h / gH);
    float ox = x + (w - gW * sc) / 2, oy = y + (h - gH * sc) / 2;
    return Vector2{ox + mx * sc, oy + my * sc};
}

void DrawLeagueMark(const StageDef& st, int x, int y, bool beaten, float r) {
    Color c = beaten ? GymColor(st.arena) : Color{70, 64, 86, 255};
    if (st.IsChampion()) {   // a crown
        float w = r * 1.2f, h = r;
        Color ink{30, 26, 40, 255};
        DrawRectangle(x - (int)w - 1, y - 1, (int)(w * 2) + 2, (int)(h * 0.7f) + 2, ink);
        DrawRectangle(x - (int)w, y, (int)(w * 2), (int)(h * 0.7f), c);
        for (int k = -1; k <= 1; k++) {
            Vector2 b1{x + k * w * 0.8f - w * 0.35f, (float)y + 1}, b2{x + k * w * 0.8f + w * 0.35f, (float)y + 1}, tp{x + k * w * 0.8f, y - h * (k == 0 ? 0.95f : 0.7f)};
            DrawTriangle(b1, b2, tp, ink);
            DrawTriangle({b1.x + 1, b1.y}, {b2.x - 1, b2.y}, {tp.x, tp.y + 1.5f}, c);
        }
        if (beaten) DrawRectangle(x - 1, y + 1, 2, 2, Color{220, 60, 70, 255});
        return;
    }
    DrawPoly({(float)x, (float)y}, 4, r, 0, Color{30, 26, 40, 255});
    DrawPoly({(float)x, (float)y}, 4, r - 1.5f, 0, c);
    if (beaten) DrawRectangle(x - 1, y - (int)(r * 0.4f), 2, 2, Fade(WHITE, 0.8f));
}

void DrawStoryMap(const Profile& p, int x, int y, int w, int h, float t, bool townLabels, int focusStage, int region) {
    const auto& stages = Story().stages;
    int n = (int)stages.size();
    if (region < 0) region = focusStage >= 0 ? StageRegion(focusStage) : StageRegion(std::min(p.storyStage, std::max(0, n - 1)));
    region = std::max(0, std::min(RegionCount() - 1, region));
    const auto& R = RegionInfo(region);
    bool hasMap = region < (int)gMaps.size() && gMaps[region].has;
    if (region < (int)gMaps.size()) { gW = gMaps[region].w; gH = gMaps[region].h; }
    float sc = std::min(w / gW, h / gH);
    float ox = x + (w - gW * sc) / 2, oy = y + (h - gH * sc) / 2;
    auto P = [&](float mx, float my) { return Vector2{ox + mx * sc, oy + my * sc}; };

    if (hasMap) {
        DrawTexturePro(gMaps[region].tex, {0, 0, gW, gH}, {ox, oy, gW * sc, gH * sc}, {0, 0}, 0, WHITE);
    } else {
        DrawRectangle((int)ox, (int)oy, (int)(gW * sc), (int)(gH * sc), Color{70, 110, 170, 255});
        for (int yy = (int)oy; yy < (int)(oy + gH * sc); yy += 4) DrawRectangle((int)ox, yy, (int)(gW * sc), 1, Color{80, 122, 182, 255});
        for (const auto& tw : R.towns) {
            Vector2 c = P(tw.x, tw.y);
            DrawCircleV(c, 26 * sc + 10, Color{90, 160, 80, 255});
        }
        for (int i = R.first; i < R.end; i++) {
            const StageDef& st = stages[i];
            if (st.mapX < 0) continue;
            Vector2 c = P(st.mapX, st.mapY);
            DrawCircleV(c, 14 * sc + 6, Color{100, 170, 86, 255});
        }
    }
    if (IsNight(p.clock)) {   // night: a deep blue wash, with the towns' lights still on
        DrawRectangle((int)ox, (int)oy, (int)(gW * sc), (int)(gH * sc), Color{8, 12, 44, 105});
        for (const auto& tw : R.towns) {
            Vector2 c = P(tw.x, tw.y);
            float flick = 0.75f + 0.25f * std::sin(t * 2.3f + tw.x * 0.13f);
            DrawCircleV(c, 10 * sc + 4, Fade(Color{255, 210, 120, 255}, 0.16f * flick));
            DrawCircleV(c, 4 * sc + 2, Fade(Color{255, 225, 150, 255}, 0.28f * flick));
        }
    }
    RectLines((int)ox - 1, (int)oy - 1, (int)(gW * sc) + 2, (int)(gH * sc) + 2, UI_BORDER);

    int cur = std::min(p.storyStage, n - 1);
    bool done = p.storyStage >= n;

    if (townLabels) {
        std::vector<Rectangle> used;
        for (const auto& tw : R.towns) {
            Vector2 c = P(tw.x, tw.y);
            int tw2 = TextW(tw.name.c_str(), 10);
            Rectangle r{c.x - tw2 / 2.0f, c.y + 7, (float)tw2, 10};
            bool clash = false;
            for (const auto& u : used) clash |= CheckCollisionRecs(r, u);
            if (clash) r.y = c.y - 19;
            used.push_back(r);
            DrawTextOutlined(tw.name.c_str(), (int)r.x, (int)r.y, 10, WHITE);
        }
    }

    // Stage markers (this region's)
    for (int i = R.first; i < R.end; i++) {
        const StageDef& st = stages[i];
        if (st.mapX < 0) continue;
        Vector2 c = P(st.mapX, st.mapY);
        bool beaten = i < p.storyStage;
        if (st.IsGym()) {
            DrawBadge(st.badge, (int)c.x, (int)c.y, beaten, 6.5f);
        } else if (st.IsLeague()) {
            DrawLeagueMark(st, (int)c.x, (int)c.y, beaten, 5.5f);
        } else {
            Color col = beaten ? UI_GOLD : (i == cur && !done ? WHITE : Color{120, 112, 140, 255});
            DrawCircleV(c, 3.2f, UI_INK);
            DrawCircleV(c, 2.2f, col);
        }
        if (i == focusStage) {
            float r = 9 + std::sin(t * 6) * 1.5f;
            EllipsePix(c.x, c.y, r, r, UI_GOLD);
        }
    }

    // You are here: the lead creature at the next stage.
    if (!done && stages[cur].mapX >= 0 && stages[cur].region == region) {
        Vector2 c = P(stages[cur].mapX, stages[cur].mapY);
        float r = 6 + std::fmod(t * 8, 6.0f);
        EllipsePix(c.x, c.y, r, r, Fade(WHITE, 1.0f - std::fmod(t * 8, 6.0f) / 6.0f));
        int lead = -1;
        for (const auto& m : p.mons)
            if (MonSpecies(m) >= 0 && m.hp > 0) { lead = MonSpecies(m); break; }
        if (lead < 0 && !p.mons.empty()) lead = MonSpecies(p.mons[0]);
        if (lead >= 0) {
            int bob = ((int)(t * 3) % 2);
            DrawEllipse((int)c.x, (int)c.y - 4, 6, 2, Fade(BLACK, 0.35f));
            DrawSpeciesFit(lead, VIEW_FRONT, (int)c.x, (int)c.y - 4 - bob, 20);
        }
    }
}
