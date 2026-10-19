#pragma once
#include <string>
#include "raylib.h"

// Sprite views. The order matches Fighter::dir.
enum SpriteView { VIEW_FRONT = 0, VIEW_BACK = 1, VIEW_LEFT = 2, VIEW_RIGHT = 3 };

struct SpriteFrame {
    Texture2D tex{};
    Texture2D white{};  // solid white silhouette for hit flashes and send-out effects
    int w = 0, h = 0;
};

struct CreatureSprites {
    SpriteFrame view[4][2];
    int frames[4] = {1, 1, 1, 1};   // 1 or 2 (walking animation)
    SpriteFrame portrait;           // optional menu picture
    bool hasPortrait = false;
    bool fromFiles = false;         // loaded from a creatures/ folder
    float pixel = 4.0f / 3.0f;      // logical units per sprite pixel in battle
};

// Shiny: OR this into a species index for the shiny colours (Pokemon/<Name>/shiny.txt, made by
// tools/shinygen.py from the official shiny sprites). Only sprite drawing looks at it.
constexpr int SHINY_FLAG = 1 << 20;
inline int ShinyIdx(int species, bool shiny) { return species < 0 ? species : (shiny ? (species | SHINY_FLAG) : species); }

void LoadAllSprites(std::string& log);  // call after InitWindow and after creature files are loaded
void UnloadAllSprites();
const CreatureSprites& SpritesFor(int speciesIndex);

// Battle drawing. Feet at (x, bottom). sx/sy squash for send-out/faint animations.
void DrawCreature(int speciesIndex, int view, int frame, float x, float bottom, float sx, float sy,
                  Color tint, float whiteAmount);
void DrawCreatureReflect(int speciesIndex, int view, int frame, float x, float feet, float sx, float sy, Color tint);   // upside down below the feet
float CreatureHeight(int speciesIndex, int view);  // in logical units

// Menu drawing: fit the creature (portrait if it has one and usePortrait) into a box, crisp.
void DrawSpeciesFit(int speciesIndex, int view, int cx, int bottomY, int box, Color tint = WHITE,
                    bool usePortrait = true);
// Old API kept for menus: integer scale.
void DrawSpecies(int speciesIndex, int view, int x, int bottomY, int scale, Color tint = WHITE);
