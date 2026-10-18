#pragma once
#include <string>
#include "battle.h"

// Rebindable actions.
enum class Act { Up, Down, Left, Right, Move1, Move2, Move3, Move4, Swap, Pause, Dodge, Jump, SwapPrev, Count };
constexpr int ACT_COUNT = (int)Act::Count;

// Keyboard layouts: one player alone, or the two halves of a shared keyboard.
enum KeyLayout { LAYOUT_SOLO = 0, LAYOUT_P1 = 1, LAYOUT_P2 = 2, LAYOUT_COUNT = 3 };

struct Settings {
    // Display
    bool fullscreen = true;
    bool showFps = false;
    // Audio (0-100)
    int masterVolume = 80;
    int sfxVolume = 80;
    // Gameplay
    bool autoAim = true;
    bool screenShake = true;
    bool damageNumbers = true;
    int teamSize = 6;
    int quickArena = -1;          // Quick Battle / 2P arena: -1 random, else an ArenaKind
    AILevel cpuLevel = AILevel::Normal;
    // Controls: two keys per action per keyboard layout, one button per action per gamepad (0 = none)
    int keys[LAYOUT_COUNT][ACT_COUNT][2] = {};
    int pad[2][ACT_COUNT] = {};
};

Settings& Cfg();
void LoadSettings();          // settings.txt next to the game (defaults if missing)
void SaveSettings();
void ResetKeys(int layout);
void ResetPad(int player);
void ApplyAudioSettings();

const char* ActName(Act a);
const char* KeyName(int key);          // short label for a keyboard key
const char* PadButtonName(int button); // short label for a gamepad button
