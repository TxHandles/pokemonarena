// Arena Clash — a 2D real-time creature arena battler.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include "arena.h"
#include "wild.h"
#include "vfx.h"
#include "storymap.h"
#include "audio.h"
#include "creatures.h"
#include "embedded.h"
#include "game.h"
#include "raylib.h"
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
    InitWindow(1920, 1080, "Arena Clash");
#else
    InitWindow(1280, 720, "Arena Clash");
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
        switch (g.screen) {
            case Screen::Title: UpdateTitle(g); break;
            case Screen::Settings: UpdateSettings(g); break;
            case Screen::Controls: UpdateControls(g); break;
            case Screen::ProfileSelect: UpdateProfileSelect(g); break;
            case Screen::NameEntry: UpdateNameEntry(g); break;
            case Screen::Starter: UpdateStarter(g); break;
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
        }

        BeginTextureMode(target);
        ClearBackground(BLACK);
        BeginMode2D(cam);
        switch (g.screen) {
            case Screen::Title: DrawTitle(g); break;
            case Screen::Settings: DrawSettings(g); break;
            case Screen::Controls: DrawControls(g); break;
            case Screen::ProfileSelect: DrawProfileSelect(g); break;
            case Screen::NameEntry: DrawNameEntry(g); break;
            case Screen::Starter: DrawStarter(g); break;
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
        }
        if (g.screen != Screen::Settings && g.screen != Screen::Controls && g.screen != Screen::ProfileSelect &&
            g.screen != Screen::NameEntry && g.screen != Screen::Evolve && g.screen != Screen::Reward && g.screen != Screen::StoryHub &&
            g.screen != Screen::Heal && g.screen != Screen::Shop && g.screen != Screen::MyCreatures &&
            g.screen != Screen::CoopSelect && g.screen != Screen::CoopHub && g.screen != Screen::NetMenu)
            DrawToast(g);
        DrawOnlineOverlay(g);
#ifdef ARENA_AUTOTEST
        if (gAutoSheet) gAutoSheet(gAutoSheetPage);
#endif
        if (Cfg().showFps) DrawText(TextFormat("%d FPS", GetFPS()), VW - 44, VH - 12, 10, UI_DIM);
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
        DrawTexturePro(target.texture, src, dst, {0, 0}, 0, WHITE);
        EndDrawing();
#ifdef ARENA_AUTOTEST
        if (AutoTestStep(g, dt)) break;
#endif
    }

    if (g.on.active) OnlineLeave(g, "");  // tell the partner, save
    UnloadRenderTexture(target);
    UnloadArena();
    UnloadStoryMap();
    UnloadAllSprites();
    UnloadSfx();
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
