#include "storymap.h"
#include "embedded.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include "sprites.h"
#include "story.h"
#include "ui.h"
#include "vec.h"

namespace {
Texture2D gMap{};
bool gHasMap = false;
float gW = 1142, gH = 857;   // map coordinate space (image size, or a default for the drawn map)
} // namespace

void LoadStoryMap() {
    gHasMap = false;
    if (!Story().mapImage.empty()) {
        std::string path = DataDir() + Story().mapImage;
        if (FileExists(path.c_str())) {
            Image img = LoadImage(path.c_str());
            if (img.data) {
                gMap = LoadTextureFromImage(img);
                SetTextureFilter(gMap, TEXTURE_FILTER_BILINEAR);
                gW = (float)img.width;
                gH = (float)img.height;
                gHasMap = true;
                UnloadImage(img);
            }
        }
    }
    if (!gHasMap) {  // size the drawn map to fit every point
        float mx = 400, my = 300;
        for (const auto& s : Story().stages) { mx = std::max(mx, s.mapX + 60); my = std::max(my, s.mapY + 60); }
        for (const auto& t : Story().towns) { mx = std::max(mx, t.x + 60); my = std::max(my, t.y + 60); }
        gW = std::max(mx, my * 4 / 3);
        gH = gW * 3 / 4;
    }
}

void UnloadStoryMap() {
    if (gHasMap) UnloadTexture(gMap);
    gHasMap = false;
}

bool StoryMapHasImage() { return gHasMap; }

Color BadgeColor(const std::string& b) {
    if (b == "Boulder") return {170, 150, 120, 255};
    if (b == "Cascade") return {90, 160, 240, 255};
    if (b == "Thunder") return {250, 210, 60, 255};
    if (b == "Rainbow") return {120, 210, 110, 255};
    if (b == "Soul") return {200, 100, 220, 255};
    if (b == "Marsh") return {240, 190, 60, 255};
    if (b == "Volcano") return {240, 100, 50, 255};
    if (b == "Earth") return {100, 200, 120, 255};
    return {200, 200, 210, 255};
}

void DrawBadge(const std::string& b, int x, int y, bool have, float r) {
    Color c = have ? BadgeColor(b) : Color{60, 56, 72, 255};
    DrawPoly({(float)x, (float)y}, 6, r, 0, have ? Color{30, 26, 40, 255} : Color{40, 36, 50, 255});
    DrawPoly({(float)x, (float)y}, 6, r - 1.5f, 30, c);
    if (have) DrawRectangle(x - 2, y - 3, 2, 2, Fade(WHITE, 0.8f));
}

void DrawStoryMap(const Profile& p, int x, int y, int w, int h, float t, bool townLabels, int focusStage) {
    float sc = std::min(w / gW, h / gH);
    float ox = x + (w - gW * sc) / 2, oy = y + (h - gH * sc) / 2;
    auto P = [&](float mx, float my) { return Vector2{ox + mx * sc, oy + my * sc}; };

    if (gHasMap) {
        DrawTexturePro(gMap, {0, 0, gW, gH}, {ox, oy, gW * sc, gH * sc}, {0, 0}, 0, WHITE);
    } else {
        DrawRectangle((int)ox, (int)oy, (int)(gW * sc), (int)(gH * sc), Color{70, 110, 170, 255});
        for (int yy = (int)oy; yy < (int)(oy + gH * sc); yy += 4) DrawRectangle((int)ox, yy, (int)(gW * sc), 1, Color{80, 122, 182, 255});
        for (const auto& tw : Story().towns) {
            Vector2 c = P(tw.x, tw.y);
            DrawCircleV(c, 26 * sc + 10, Color{90, 160, 80, 255});
        }
        for (const auto& st : Story().stages) {
            if (st.mapX < 0) continue;
            Vector2 c = P(st.mapX, st.mapY);
            DrawCircleV(c, 14 * sc + 6, Color{100, 170, 86, 255});
        }
    }
    if (IsNight(p.clock)) {   // night: a deep blue wash, with the towns' lights still on
        DrawRectangle((int)ox, (int)oy, (int)(gW * sc), (int)(gH * sc), Color{8, 12, 44, 105});
        for (const auto& tw : Story().towns) {
            Vector2 c = P(tw.x, tw.y);
            float flick = 0.75f + 0.25f * std::sin(t * 2.3f + tw.x * 0.13f);
            DrawCircleV(c, 10 * sc + 4, Fade(Color{255, 210, 120, 255}, 0.16f * flick));
            DrawCircleV(c, 4 * sc + 2, Fade(Color{255, 225, 150, 255}, 0.28f * flick));
        }
    }
    RectLines((int)ox - 1, (int)oy - 1, (int)(gW * sc) + 2, (int)(gH * sc) + 2, UI_BORDER);

    const auto& stages = Story().stages;
    int n = (int)stages.size();
    int cur = std::min(p.storyStage, n - 1);
    bool done = p.storyStage >= n;

    if (townLabels) {
        std::vector<Rectangle> used;
        for (const auto& tw : Story().towns) {
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

    // Stage markers
    for (int i = 0; i < n; i++) {
        const StageDef& st = stages[i];
        if (st.mapX < 0) continue;
        Vector2 c = P(st.mapX, st.mapY);
        bool beaten = i < p.storyStage;
        if (st.IsGym()) {
            DrawBadge(st.badge, (int)c.x, (int)c.y, beaten, 6.5f);
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
    if (!done && stages[cur].mapX >= 0) {
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
