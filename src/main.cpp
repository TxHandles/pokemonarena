// Arena Clash — a 2D real-time creature arena battler.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "arena.h"
#include "wild.h"
#include "vfx.h"
#include "storymap.h"
#include "leaders.h"
#include "audio.h"
#include "creatures.h"
#include "embedded.h"
#include "game.h"
#include "look.h"
#include "raylib.h"
#include "rlgl.h"
#include "settings.h"
#include "sprites.h"
#include "ui.h"

#ifdef ARENA_AUTOTEST
bool gAutoPlayP1 = false;
bool gAutoNetInput = false;   // autotest: scripted button presses in online battles
void (*gAutoSheet)(int) = nullptr;   // autotest: draw a test sheet instead of the screen
int gAutoSheetPage = 0;
#include "autotest.inc"
#endif

// One frame of whatever screen a game is on (the overworld runs a second, smaller game in a
// split-screen half for a co-op player's own battles).
void UpdateScreenOf(Game& g, float dt) {
    if (g.encounterT > 0) {   // a wild encounter's wipe, then straight into the battle
        g.encounterT -= dt;
        if (g.encounterT <= 0) { g.encounterT = 0; g.encounterBlank = false; StartBattleFromSelect(g); }
        return;
    }
    switch (g.screen) {
        case Screen::Title: UpdateTitle(g); break;
        case Screen::Settings: UpdateSettings(g); break;
        case Screen::Controls: UpdateControls(g); break;
        case Screen::ProfileSelect: UpdateProfileSelect(g); break;
        case Screen::NameEntry: UpdateNameEntry(g); break;
        case Screen::Creator: UpdateCreator(g); break;
        case Screen::Starter: UpdateStarter(g); break;
        case Screen::RegionChoose: UpdateRegionChoose(g); break;
        case Screen::StoryHub: UpdateStoryHub(g); break;
        case Screen::MyCreatures: UpdateMyCreatures(g); break;
        case Screen::StageIntro: UpdateStageIntro(g); break;
        case Screen::RosterPick: UpdateRosterPick(g); break;
        case Screen::Select: UpdateSelect(g, dt); break;
        case Screen::Battle: UpdateBattleScreen(g, dt); break;
        case Screen::Results: UpdateResults(g); break;
        case Screen::StoryResults: UpdateStoryResults(g); break;
        case Screen::Evolve: UpdateEvolve(g, dt); break;
        case Screen::LearnMove: UpdateLearnMove(g); break;
        case Screen::Fossil: UpdateFossil(g); break;
        case Screen::Gift: UpdateGift(g); break;
        case Screen::HallOfFame: UpdateHallOfFame(g); break;
        case Screen::Legends: UpdateLegends(g); break;
        case Screen::LegendCatch: UpdateLegendCatch(g, dt); break;
        case Screen::Bag: UpdateBag(g); break;
        case Screen::Revive: UpdateRevive(g); break;
        case Screen::Slots: UpdateSlots(g); break;
        case Screen::CoopSetup: UpdateCoopSetup(g); break;
        case Screen::BattleStats: UpdateBattleStats(g); break;
        case Screen::Practice: UpdatePractice(g, dt); break;
        case Screen::CoopStarter: UpdateCoopStarter(g); break;
        case Screen::Reward: UpdateReward(g); break;
        case Screen::Catch: UpdateCatch(g, dt); break;
        case Screen::Heal: UpdateHeal(g); break;
        case Screen::DoublesMenu: UpdateDoublesMenu(g); break;
        case Screen::CoopSelect: UpdateCoopSelect(g); break;
        case Screen::CoopHub: UpdateCoopHub(g); break;
        case Screen::NetMenu: UpdateNetMenu(g); break;
        case Screen::Pokedex: UpdatePokedex(g); break;
        case Screen::Trade: UpdateTrade(g, dt); break;
        case Screen::Map: UpdateMap(g); break;
        case Screen::Shop: UpdateShop(g); break;
        case Screen::World: UpdateWorld(g, dt); break;
        case Screen::Nickname: UpdateNickname(g); break;
        case Screen::Hatch: UpdateHatch(g, dt); break;
        case Screen::Daycare: UpdateDaycare(g); break;
        case Screen::Contest: UpdateContest(g); break;
        case Screen::Safari: UpdateSafari(g); break;
        case Screen::UnownPuzzle: UpdateUnownPuzzle(g); break;
    }
}
void DrawScreenOf(const Game& g) {
    switch (g.screen) {
        case Screen::Title: DrawTitle(g); break;
        case Screen::Settings: DrawSettings(g); break;
        case Screen::Controls: DrawControls(g); break;
        case Screen::ProfileSelect: DrawProfileSelect(g); break;
        case Screen::NameEntry: DrawNameEntry(g); break;
        case Screen::Creator: DrawCreator(g); break;
        case Screen::Starter: DrawStarter(g); break;
        case Screen::RegionChoose: DrawRegionChoose(g); break;
        case Screen::StoryHub: DrawStoryHub(g); break;
        case Screen::MyCreatures: DrawMyCreatures(g); break;
        case Screen::StageIntro: DrawStageIntro(g); break;
        case Screen::RosterPick: DrawRosterPick(g); break;
        case Screen::Select: DrawSelect(g); break;
        case Screen::Battle: DrawBattleScreen(g); break;
        case Screen::Results: DrawResults(g); break;
        case Screen::StoryResults: DrawStoryResults(g); break;
        case Screen::Evolve: DrawEvolve(g); break;
        case Screen::LearnMove: DrawLearnMove(g); break;
        case Screen::Fossil: DrawFossil(g); break;
        case Screen::Gift: DrawGift(g); break;
        case Screen::HallOfFame: DrawHallOfFame(g); break;
        case Screen::Legends: DrawLegends(g); break;
        case Screen::LegendCatch: DrawLegendCatch(g); break;
        case Screen::Bag: DrawBag(g); break;
        case Screen::Revive: DrawRevive(g); break;
        case Screen::Slots: DrawSlots(g); break;
        case Screen::CoopSetup: DrawCoopSetup(g); break;
        case Screen::BattleStats: DrawBattleStats(g); break;
        case Screen::Practice: DrawPractice(g); break;
        case Screen::CoopStarter: DrawCoopStarter(g); break;
        case Screen::Reward: DrawReward(g); break;
        case Screen::Catch: DrawCatch(g); break;
        case Screen::Heal: DrawHeal(g); break;
        case Screen::DoublesMenu: DrawDoublesMenu(g); break;
        case Screen::CoopSelect: DrawCoopSelect(g); break;
        case Screen::CoopHub: DrawCoopHub(g); break;
        case Screen::NetMenu: DrawNetMenu(g); break;
        case Screen::Pokedex: DrawPokedex(g); break;
        case Screen::Trade: DrawTrade(g); break;
        case Screen::Map: DrawMap(g); break;
        case Screen::Shop: DrawShop(g); break;
        case Screen::World: DrawWorld(g); break;
        case Screen::Nickname: DrawNickname(g); break;
        case Screen::Hatch: DrawHatch(g); break;
        case Screen::Daycare: DrawDaycare(g); break;
        case Screen::Contest: DrawContest(g); break;
        case Screen::Safari: DrawSafari(g); break;
        case Screen::UnownPuzzle: DrawUnownPuzzle(g); break;
    }
    if (g.encounterT > 0) DrawEncounterWipe(g);
}

int main() {
    SetTraceLogLevel(LOG_WARNING);
    LoadSettings();

    // Creature data first (no window needed), then the window, then textures.
    std::string log = RestoreEmbeddedFiles();
    LoadCustomCreatures(log);
    LoadStory(log);
    LoadWild(log);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#ifdef ARENA_AUTOTEST
    InitWindow(1920, 1080, "Pokemon Battle Masters");
#else
    InitWindow(1280, 720, "Pokemon Battle Masters");
#endif
    SetWindowMinSize(VW, VH);
    SetExitKey(KEY_NULL);
    SetTargetFPS(60);
#ifndef ARENA_AUTOTEST
    if (Cfg().fullscreen) ToggleBorderlessWindowed();
#endif
    InitAudioDevice();
    InitSfx();
    ApplyAudioSettings();

    log += "\nSprites:\n";
    LoadAllSprites(log);
    WriteCreatureFiles(log);
    LoadArena();
    LoadVfx();
    LoadStoryMap();
    { std::string wl; LoadWorld(wl); }

    // Everything is drawn in 640x360 logical units onto a 3x hi-res canvas, then scaled to the window.
    const int CW = VW * CANVAS_ZOOM, CH = VH * CANVAS_ZOOM;
    RenderTexture2D target = LoadRenderTexture(CW, CH);
    Camera2D cam{};
    cam.zoom = (float)CANVAS_ZOOM;

    Game g;
    for (const auto& sp : AllSpecies()) g.customCount += sp.custom ? 1 : 0;

    while (!WindowShouldClose() && !g.quit) {
        float dt = std::min(GetFrameTime(), 1.0f / 30.0f);
        g.t += dt;
        g.toastT = std::max(0.0f, g.toastT - dt);
        if (IsKeyPressed(KEY_F11) && !g.capturing) {
            ToggleBorderlessWindowed();
            Cfg().fullscreen = IsWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            SaveSettings();
        }

        OnlinePoll(g);
        if (OnlineCodeEntry(g)) {
        } else UpdateScreenOf(g, dt);

        // Screen changes open with a Poke Ball iris.
        static Screen lastScreen = g.screen;
        static float transT = 0;
        if (g.screen != lastScreen) {
            bool minor = (lastScreen == Screen::Battle && g.screen == Screen::BattleStats) || (lastScreen == Screen::Settings) || (g.screen == Screen::Settings) ||
                         (lastScreen == Screen::Controls) || (g.screen == Screen::Controls);
            if (!minor) transT = 0.42f;
            lastScreen = g.screen;
            g.screenT = 0;
        }
        g.screenT += GetFrameTime();
        gUiScreenT = g.screenT;
        transT = std::max(0.0f, transT - GetFrameTime());

        WorldPreDraw(g);   // a co-op player's own battle, drawn for their half of the screen
        if (g.screen == Screen::Battle) g.battle.PrepareHud();   // the HP plates, so they can fade over the field
        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode2D(cam);
        DrawScreenOf(g);
        if (g.screen != Screen::Settings && g.screen != Screen::Controls && g.screen != Screen::ProfileSelect &&
            g.screen != Screen::NameEntry && g.screen != Screen::Creator && g.screen != Screen::Evolve && g.screen != Screen::Reward && g.screen != Screen::StoryHub &&
            g.screen != Screen::Heal && g.screen != Screen::Shop && g.screen != Screen::MyCreatures &&
            g.screen != Screen::CoopSelect && g.screen != Screen::CoopHub && g.screen != Screen::NetMenu)
            DrawToast(g);
        DrawOnlineOverlay(g);
#ifdef ARENA_AUTOTEST
        if (gAutoSheet) gAutoSheet(gAutoSheetPage);
#endif
        if (transT > 0) {
            float u = 1 - transT / 0.42f;
            float e = u * u * (3 - 2 * u);
            float R = 4 + e * 400;
            Vector2 c{VW / 2.0f, VH / 2.0f};
            DrawRing(c, R, 500, 0, 360, 64, Color{12, 10, 22, 255});
            DrawRing(c, R - 2.5f, R, 0, 360, 64, Fade(Color{226, 52, 52, 255}, 1 - e));
            if (u < 0.35f) DrawPokeBall(c, 18 * (1 - u / 0.35f) + 2, u * 600, u / 0.35f);
        }
        if (Cfg().showFps) GDrawText(TextFormat("%d FPS", GetFPS()), VW - 44, VH - 12, 10, UI_DIM);
        EndMode2D();
        EndTextureMode();

        // Fit the canvas to the window. Exact multiples stay razor sharp; other sizes are smoothed.
        BeginDrawing();
        ClearBackground(BLACK);
        float sc = std::min(GetScreenWidth() / (float)CW, GetScreenHeight() / (float)CH);
        bool exact = std::fabs(sc - std::round(sc)) < 0.001f;
        SetTextureFilter(target.texture, exact ? TEXTURE_FILTER_POINT : TEXTURE_FILTER_BILINEAR);
        float dw = CW * sc, dh = CH * sc;
        Rectangle src{0, 0, (float)CW, -(float)CH};
        Rectangle dst{std::floor((GetScreenWidth() - dw) / 2), std::floor((GetScreenHeight() - dh) / 2), dw, dh};
        // The canvas's colours are already final: copy them without blending. (Translucent drawing
        // lowers the canvas's alpha, which would otherwise darken everything see-through.)
        rlDrawRenderBatchActive();
        rlDisableColorBlend();
        DrawTexturePro(target.texture, src, dst, {0, 0}, 0, WHITE);
        rlDrawRenderBatchActive();
        rlEnableColorBlend();
        if (Cfg().screenFilter == 1) {   // soft glow: the picture again, spread out and added faintly
            SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
            BeginBlendMode(BLEND_ADDITIVE);
            float o = 2.5f * sc;
            const Vector2 offs[4] = {{o, 0}, {-o, 0}, {0, o}, {0, -o}};
            for (const Vector2& d : offs)
                DrawTexturePro(target.texture, src, Rectangle{dst.x + d.x, dst.y + d.y, dst.width, dst.height}, {0, 0}, 0, Fade(WHITE, 0.045f));
            EndBlendMode();
        } else if (Cfg().screenFilter == 2) {   // retro CRT: scanlines, a soft vignette and a glassy sheen
            int step = std::max(2, (int)std::round(sc * 1.0f));
            for (int y = (int)dst.y; y < (int)(dst.y + dst.height); y += step * 2)
                DrawRectangle((int)dst.x, y, (int)dst.width, std::max(1, step / 2 + 1), Fade(BLACK, 0.22f));
            int vw = (int)(dst.width * 0.12f), vh = (int)(dst.height * 0.14f);
            DrawRectangleGradientH((int)dst.x, (int)dst.y, vw, (int)dst.height, Fade(BLACK, 0.4f), Fade(BLACK, 0));
            DrawRectangleGradientH((int)(dst.x + dst.width - vw), (int)dst.y, vw, (int)dst.height, Fade(BLACK, 0), Fade(BLACK, 0.4f));
            DrawRectangleGradientV((int)dst.x, (int)dst.y, (int)dst.width, vh, Fade(BLACK, 0.35f), Fade(BLACK, 0));
            DrawRectangleGradientV((int)dst.x, (int)(dst.y + dst.height - vh), (int)dst.width, vh, Fade(BLACK, 0), Fade(BLACK, 0.35f));
            DrawRectangleGradientV((int)dst.x, (int)dst.y, (int)dst.width, (int)(dst.height * 0.3f), Fade(WHITE, 0.035f), Fade(WHITE, 0));
        }
        EndDrawing();
#ifdef ARENA_AUTOTEST
        if (AutoTestStep(g, dt)) break;
#endif
    }

    if (g.on.active) OnlineLeave(g, "");  // tell the partner, save
    UnloadRenderTexture(target);
    UnloadArena();
    UnloadStoryMap();
    UnloadLeaderPictures();
    UnloadAllSprites();
    UnloadSfx();
    CloseAudioDevice();
    UnloadLooks();
    CloseWindow();
    return 0;
}
