#include "chart.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <map>
#include <vector>

namespace {

struct Px {
    int w = 0, h = 0;
    std::vector<Color> c;
    const Color& at(int x, int y) const { return c[(size_t)y * w + x]; }
};

int Diff(Color a, Color b) { return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b); }
bool Same(Color a, Color b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
// Near-white counts as white: lossy pictures (JPG/WEBP saves) blur pure white slightly.
bool PureWhite(Color c) {
    int lo = std::min(c.r, std::min(c.g, c.b)), hi = std::max(c.r, std::max(c.g, c.b));
    return lo >= 244 && hi - lo <= 6;
}
int Luma(Color c) { return (c.r * 299 + c.g * 587 + c.b * 114) / 1000; }
unsigned Key(Color c) { return ((unsigned)(c.r >> 3) << 10) | ((unsigned)(c.g >> 3) << 5) | (c.b >> 3); }  // 5-bit bins

Image Crop(const Px& p, int x0, int y0, int x1, int y1) {
    // [x0,x1) x [y0,y1) from p, alpha kept
    int w = x1 - x0, h = y1 - y0;
    Image img = GenImageColor(w, h, BLANK);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) ImageDrawPixel(&img, x, y, p.at(x0 + x, y0 + y));
    return img;
}

Image CropToAlpha(const Px& p) {
    int x0 = p.w, y0 = p.h, x1 = -1, y1 = -1;
    for (int y = 0; y < p.h; y++)
        for (int x = 0; x < p.w; x++)
            if (p.at(x, y).a > 8) {
                x0 = std::min(x0, x); y0 = std::min(y0, y);
                x1 = std::max(x1, x); y1 = std::max(y1, y);
            }
    if (x1 < 0) return Image{};
    return Crop(p, x0, y0, x1 + 1, y1 + 1);
}

// Find the repeating grid spacing in a 1D edge profile. Returns pitch (0 if none) and phase.
int FindPitch(const std::vector<int>& e, int& phase, float& quality) {
    int n = (int)e.size();
    double mean = 0;
    for (int v : e) mean += v;
    mean = mean / std::max(1, n) + 1e-6;
    std::vector<float> score(81, 0);
    std::vector<int> bestPhase(81, 0);
    float top = 0;
    for (int p = 8; p <= 80 && p * 4 < n; p++) {
        for (int ph = 0; ph < p; ph++) {
            double s = 0;
            int k = 0;
            for (int i = ph; i < n; i += p, k++) s += e[i];
            float sc = (float)(s / std::max(1, k) / mean);
            if (sc > score[p]) { score[p] = sc; bestPhase[p] = ph; }
        }
        top = std::max(top, score[p]);
    }
    for (int p = 8; p <= 80; p++) {
        if (score[p] >= top * 0.8f && score[p] >= 2.5f) {
            phase = bestPhase[p];
            quality = score[p];
            return p;
        }
    }
    return 0;
}

struct Cell {
    Color mode{};
    float ink = 0;  // fraction of pixels far from the mode color (digits, symbols)
    float mark = 0; // fraction of pixels clearly a different color (any printed digit)
};

Image FromChart(const Px& p, std::string& note, bool numberedHint) {
    std::vector<int> colE(p.w, 0), rowE(p.h, 0);
    for (int y = 0; y < p.h; y++)
        for (int x = 1; x < p.w; x++)
            if (Diff(p.at(x, y), p.at(x - 1, y)) > 40) colE[x]++;
    for (int y = 1; y < p.h; y++)
        for (int x = 0; x < p.w; x++)
            if (Diff(p.at(x, y), p.at(x, y - 1)) > 40) rowE[y]++;

    int phx = 0, phy = 0;
    float qx = 0, qy = 0;
    int px = FindPitch(colE, phx, qx);
    int py = FindPitch(rowE, phy, qy);
    if (!px || !py || std::abs(px - py) > 2) { note = "no grid found"; return Image{}; }

    int nc = (p.w - phx) / px, nr = (p.h - phy) / py;
    if (nc < 4 || nr < 4) { note = "grid too small"; return Image{}; }

    int mx = std::max(2, px / 5), my = std::max(2, py / 5);
    std::vector<Cell> cells((size_t)nc * nr);
    for (int r = 0; r < nr; r++) {
        for (int c = 0; c < nc; c++) {
            std::map<unsigned, int> hist;
            int x0 = phx + c * px + mx, x1 = phx + (c + 1) * px - mx;
            int y0 = phy + r * py + my, y1 = phy + (r + 1) * py - my;
            for (int y = y0; y < y1; y++)
                for (int x = x0; x < x1; x++) hist[Key(p.at(x, y))]++;
            unsigned best = 0;
            int bestN = -1, total = 0;
            for (auto& kv : hist) {
                total += kv.second;
                if (kv.second > bestN) { bestN = kv.second; best = kv.first; }
            }
            // The cell's color: the average of the pixels in its most common color bin.
            long sr = 0, sg = 0, sb = 0, sn = 0;
            for (int y = y0; y < y1; y++)
                for (int x = x0; x < x1; x++) {
                    const Color& q = p.at(x, y);
                    if (Key(q) == best) { sr += q.r; sg += q.g; sb += q.b; sn++; }
                }
            Cell& cell = cells[(size_t)r * nc + c];
            sn = std::max(1L, sn);
            cell.mode = Color{(unsigned char)(sr / sn), (unsigned char)(sg / sn), (unsigned char)(sb / sn), 255};
            int ink = 0;
            for (int y = y0; y < y1; y++)
                for (int x = x0; x < x1; x++)
                    if (std::abs(Luma(p.at(x, y)) - Luma(cell.mode)) > 90) ink++;
            cell.ink = total ? (float)ink / total : 0;
            int mark = 0;
            for (int y = y0; y < y1; y++)
                for (int x = x0; x < x1; x++)
                    if (Diff(p.at(x, y), cell.mode) > 90) mark++;
            cell.mark = total ? (float)mark / total : 0;
        }
    }
    auto C = [&](int r, int c) -> const Cell& { return cells[(size_t)r * nc + c]; };
    auto IsLabel = [&](int r, int c) {  // white square with a printed number
        const Cell& k = C(r, c);
        return PureWhite(k.mode) && k.ink > 0.02f && k.ink < 0.6f;
    };

    // Header row = topmost row of numbered squares; header column = leftmost column of them.
    int hr = -1, hc = -1;
    for (int r = 0; r < nr && hr < 0; r++) {
        int n = 0;
        for (int c = 0; c < nc; c++) n += IsLabel(r, c);
        if (n >= std::max(3, nc / 4)) hr = r;
    }
    for (int c = 0; c < nc && hc < 0; c++) {
        int n = 0;
        for (int r = 0; r < nr; r++) n += IsLabel(r, c);
        if (n >= std::max(3, nr / 4)) hc = c;
    }
    if (hr < 0 || hc < 0) { note = "grid has no numbered header row/column"; return Image{}; }

    // Grid columns run right from the header column for as long as the header row is numbered
    // (allowing a gap of one, e.g. an empty corner square). Same for rows.
    int c1 = hc, r1 = hr;
    for (int c = hc + 1, gap = 0; c < nc; c++) {
        if (IsLabel(hr, c)) { c1 = c; gap = 0; } else if (++gap > 1) break;
    }
    for (int r = hr + 1, gap = 0; r < nr; r++) {
        if (IsLabel(r, hc)) { r1 = r; gap = 0; } else if (++gap > 1) break;
    }
    int gw = c1 - hc, gh = r1 - hr;
    if (gw < 2 || gh < 2) { note = "could not find the grid area"; return Image{}; }

    Px out;
    out.w = gw;
    out.h = gh;
    out.c.assign((size_t)gw * gh, BLANK);
    // White squares fully enclosed by the sprite (eye shines, teeth) are part of it;
    // white squares connected to the edge are background.
    std::vector<char> outside((size_t)gw * gh, 0);
    // Numbered charts print a number in every colored square, white ones included, so there
    // a white square is part of the sprite exactly when it has a number. That also gets white
    // gaps right that are enclosed by the body (Charizard's tail loop).
    int colored = 0, inked = 0;
    for (int r = 0; r < gh; r++)
        for (int c = 0; c < gw; c++) {
            const Cell& k = C(hr + 1 + r, hc + 1 + c);
            if (PureWhite(k.mode)) continue;
            colored++;
            inked += k.mark > 0.03f && k.mark < 0.6f;
        }
    // A file named as a numbered copy ("_No") only needs most squares to show a number: light
    // colors with white numbers in a lossy file can hide a few.
    bool numbered = colored > 10 && (inked >= colored * 0.9f || (numberedHint && inked >= colored * 0.5f));
    if (numbered) {
        for (int r = 0; r < gh; r++)
            for (int c = 0; c < gw; c++) {
                const Cell& k = C(hr + 1 + r, hc + 1 + c);
                outside[(size_t)r * gw + c] = PureWhite(k.mode) && k.mark <= 0.03f;
            }
    } else {
        std::vector<int> stack;
        auto white = [&](int r, int c) { return PureWhite(C(hr + 1 + r, hc + 1 + c).mode); };
        auto push = [&](int r, int c) {
            if (r < 0 || c < 0 || r >= gh || c >= gw) return;
            size_t i = (size_t)r * gw + c;
            if (outside[i] || !white(r, c)) return;
            outside[i] = 1;
            stack.push_back((int)i);
        };
        for (int c = 0; c < gw; c++) { push(0, c); push(gh - 1, c); }
        for (int r = 0; r < gh; r++) { push(r, 0); push(r, gw - 1); }
        while (!stack.empty()) {
            int i = stack.back();
            stack.pop_back();
            int r = i / gw, c = i % gw;
            push(r - 1, c); push(r + 1, c); push(r, c - 1); push(r, c + 1);
        }
    }
    int filled = 0;
    std::vector<Color> pal;  // merge near-identical colors so lossy files give a clean palette
    for (int r = 0; r < gh; r++)
        for (int c = 0; c < gw; c++) {
            const Cell& k = C(hr + 1 + r, hc + 1 + c);
            if (outside[(size_t)r * gw + c]) continue;
            Color col = PureWhite(k.mode) ? Color{250, 250, 250, 255} : k.mode;
            bool found = false;
            for (const Color& q : pal)
                if (Diff(q, col) < 30) { col = q; found = true; break; }
            if (!found) pal.push_back(col);
            out.c[(size_t)r * gw + c] = col;
            filled++;
        }
    note = TextFormat("%s chart %dx%d squares (%dpx each), %d colored", numbered ? "numbered" : "grid", gw, gh, px, filled);
    return CropToAlpha(out);
}

} // namespace

Image ExtractSprite(const Image& src, std::string& note, bool& wasChart, bool numberedHint) {
    wasChart = false;
    if (!src.data || src.width <= 0) { note = "could not read image"; return Image{}; }

    Px p;
    p.w = src.width;
    p.h = src.height;
    Color* cols = LoadImageColors(src);
    p.c.assign(cols, cols + (size_t)p.w * p.h);
    UnloadImageColors(cols);

    bool hasAlpha = false;
    for (const Color& c : p.c)
        if (c.a < 250) { hasAlpha = true; break; }

    if (!hasAlpha && (p.w > 160 || p.h > 160)) {
        Image chart = FromChart(p, note, numberedHint);
        if (chart.width > 0) { wasChart = true; return chart; }
        // Not a chart: fall through and treat as a big plain picture.
        std::string why = note;
        note = "large image without transparency (" + why + "), using it as a plain sprite";
    } else {
        note = TextFormat("plain sprite %dx%d", p.w, p.h);
    }

    if (!hasAlpha) {
        Color bg = p.at(0, 0);
        for (Color& c : p.c)
            if (Same(c, bg)) c = BLANK;
    }
    Image out = CropToAlpha(p);
    if (out.width == 0) note = "image is empty";
    return out;
}
