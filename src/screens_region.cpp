// Choosing a region: where a new story starts, or where it goes after a region's Champion falls.
// Whatever region comes first plays at Map 1's levels (a level 5 starter), the next at Map 2's...
#include <algorithm>
#include <cctype>
#include <cmath>
#include "arena.h"
#include "audio.h"
#include "game.h"
#include "sprites.h"
#include "story.h"
#include "storymap.h"
#include "ui.h"

namespace {
bool ForCoop(const Game& g) { return g.regionFor >= 2; }
bool ForNew(const Game& g) { return g.regionFor == 0 || g.regionFor == 2; }
std::vector<int> AllRegions() {
    std::vector<int> v;
    for (int r = 0; r < RegionCount(); r++) if (RegionInfo(r).end > RegionInfo(r).first) v.push_back(r);
    return v;
}
// Can this one be chosen now?
bool Pickable(const Game& g, int r) { return ForNew(g) || !RegionChosen(r); }
// The place in the order it would take.
int SlotIfChosen(const Game& g) { return ForNew(g) ? 0 : (int)RegionOrder().size(); }
std::string Upper(std::string s) { for (char& c : s) c = (char)std::toupper((unsigned char)c); return s; }
} // namespace

void BeginRegionChoose(Game& g, int forWhat) {
    g.regionFor = forWhat;
    g.regionPick = 0;
    g.newJourney = false;
    if (ForNew(g)) ApplyRegionOrder({});   // nothing chosen yet: every region is open
    else ApplySaveOrder(g);
    auto all = AllRegions();
    g.regionSel = 0;
    for (int i = 0; i < (int)all.size(); i++) if (Pickable(g, all[i])) { g.regionSel = i; break; }
    g.screen = Screen::RegionChoose;
}

void UpdateRegionChoose(Game& g) {
    auto all = AllRegions();
    int n = (int)all.size();
    if (n == 0) { EnterStoryHub(g); return; }
    bool online = g.on.active;
    PlayerInput in = MenuInput();
    if (in.left || in.right) {
        int d = in.left ? -1 : 1;
        for (int k = 1; k <= n; k++) {
            int i = ((g.regionSel + d * k) % n + n) % n;
            if (Pickable(g, all[i])) { g.regionSel = i; break; }
        }
        PlaySfx(Sfx::MenuMove);
    }
    if (in.back) {
        PlaySfx(Sfx::MenuBack);
        if (g.regionFor == 0) { g.profiles = ListProfiles(); g.screen = Screen::ProfileSelect; }
        else if (g.regionFor == 2) { if (online) Toast(g, "Pick where your journey starts first."); else StartCoopFlow(g); }
        else EnterStoryHub(g);
        return;
    }
    if (!in.confirm) return;
    int r = all[std::max(0, std::min(n - 1, g.regionSel))];
    if (!Pickable(g, r)) { PlaySfx(Sfx::MenuBack); return; }
    PlaySfx(Sfx::MenuOk);
    switch (g.regionFor) {
        case 0: {   // a new story: the region, then the first partner (level 5)
            g.profile.regionOrder = {r};
            ApplySaveOrder(g);
            g.profile.storyStage = RegionInfo(r).first;
            if (!SaveProfile(g.profile)) Toast(g, "Could not save the profile!");
            g.starterSel = 0;
            if (r == 0) { g.regionPick = 0; g.screen = Screen::Starter; }   // Professor Oak's lab, as always
            else { g.regionPick = r + 1; g.newJourney = true; g.regionRevealT = -1; g.screen = Screen::Starter; }
        } break;
        case 1: {   // the next region: the professor there has a Pokemon for you
            g.profile.regionOrder.push_back(r);
            ApplySaveOrder(g);
            g.profile.storyStage = RegionInfo(r).first;
            if (!SaveProfile(g.profile)) Toast(g, "Could not save the profile!");
            Toast(g, "Off to " + RegionInfo(r).name + "!");
            if (NeedsRegionStarter(g) >= 0) BeginRegionStarter(g);
            else EnterStoryHub(g);
        } break;
        case 2: {   // a new co-op journey
            g.journeyRegion = r;
            if (online) {
                g.coopSave.regionOrder = {r};
                ApplySaveOrder(g);
                g.coopSave.storyStage = RegionInfo(r).first;
                SaveCoop(g.coopSave);
                OnlineSendCoop(g);
                if (CoopNeedsStarter(g)) { g.coopStarterSel[0] = g.coopStarterSel[1] = 0; g.screen = Screen::CoopStarter; }
                else EnterStoryHub(g);
            } else {
                CoopContinueSetup(g);
            }
        } break;
        default: {   // co-op's next region: then "Travel to ... together!" from the hub
            g.coopSave.regionOrder.push_back(r);
            ApplySaveOrder(g);
            g.coopSave.storyStage = RegionInfo(r).first;
            if (online) OnlineSendCoop(g);
            else SaveCoop(g.coopSave);
            Toast(g, "Off to " + RegionInfo(r).name + " together!");
            EnterStoryHub(g);
        } break;
    }
}

void DrawRegionChoose(const Game& g) {
    DrawMenuBackdrop(Color{24, 20, 38, 255}, (float)GetTime());
    auto all = AllRegions();
    int n = (int)all.size();
    const char* title = ForNew(g) ? (ForCoop(g) ? "WHERE DOES YOUR JOURNEY BEGIN?" : "WHERE DOES YOUR STORY BEGIN?") : "WHERE TO NEXT?";
    DrawTextCentered(title, VW / 2, 12, 20, UI_GOLD);
    int slot = SlotIfChosen(g);
    DrawTextCentered(ForNew(g) ? "Start in any region - you'll travel to the others as you beat each League."
                               : "Your next region plays as the next step up, whichever one you pick.",
                     VW / 2, 38, 10, UI_TEXT, false);
    int gap = 10;
    int cw = std::min(196, (620 - (n - 1) * gap) / std::max(1, n));
    int x0 = VW / 2 - (n * cw + (n - 1) * gap) / 2;
    Profile pv = ProgressView(g);
    for (int i = 0; i < n; i++) {
        int r = all[i];
        const auto& R = RegionInfo(r);
        int x = x0 + i * (cw + gap), y = 56, h = 250;
        bool sel = g.regionSel == i;
        bool can = Pickable(g, r);
        bool done = !ForNew(g) && RegionDone(g, r);
        bool here = !ForNew(g) && RegionChosen(r) && !done;
        DrawPanel(x, y, cw, h, sel ? UI_PANEL_LIGHT : UI_PANEL, sel ? UI_GOLD : (can ? UI_BORDER : Color{60, 56, 80, 255}));
        DrawTextCentered(Upper(R.name).c_str(), x + cw / 2, y + 6, 20, can ? (sel ? UI_GOLD : UI_TEXT) : UI_DIM);
        // its map
        int mx = x + 6, my = y + 30, mw = cw - 12, mh = 104;
        DrawRectangle(mx, my, mw, mh, Color{12, 10, 20, 255});
        DrawStoryMap(pv, mx, my, mw, mh, g.t, false, -1, r);
        if (!can) DrawRectangle(mx, my, mw, mh, Fade(UI_INK, 0.55f));
        // its starters
        auto st = RegionStarters(r);
        int k = 0, m = std::min(3, (int)st.size());
        for (int s : st) {
            if (k >= 3) break;
            int cx = x + cw / 2 + (k - (m - 1) / 2.0f) * 50;
            DrawEllipse(cx, y + 186, 18, 4, Fade(BLACK, 0.35f));
            DrawSpeciesFit(s, VIEW_FRONT, cx, y + 184, 44);
            k++;
        }
        GDrawText(TextFormat("Rival: %s", RegionRivalName(r).c_str()), x + 8, y + 194, 10, UI_DIM);
        const char* status;
        Color sc = UI_TEXT;
        if (done) { status = "League beaten"; sc = Color{140, 230, 150, 255}; }
        else if (here) { status = "You're here"; sc = UI_GOLD; }
        else status = TextFormat("Map %d - starter at Lv%d", slot + 1, slot == 0 ? Story().startLevel : SlotGiftLevelBase(slot));
        GDrawText(status, x + 8, y + 210, 10, sc);
        int gyms = 0;
        for (int s = R.first; s < R.end; s++) gyms += Story().stages[s].IsGym();
        GDrawText(TextFormat("%d battles, %d gyms", R.end - R.first, gyms), x + 8, y + 226, 10, UI_DIM);
    }
    const char* hint = g.regionFor == 2 && g.on.active ? "Left/Right choose   Enter: start here" : "Left/Right choose   Enter: go   Esc: back";
    DrawTextCentered(hint, VW / 2, 324, 10, UI_DIM, false);
    DrawToast(g);
}
