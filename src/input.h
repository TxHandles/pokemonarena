#pragma once
#include "raylib.h"

// One frame of input for one player, from keyboard and/or gamepad (bindings come from Settings).
struct PlayerInput {
    Vector2 move{0, 0};      // analog movement, length <= 1
    bool attack[4] = {false, false, false, false};
    bool swap = false;       // swap to the next creature in the party
    bool swapPrev = false;   // swap to the previous one
    bool pause = false;
    bool dodge = false;      // quick invulnerable dash (cooldown)
    bool jump = false;       // hop: over water, puddles and low obstacles (no invulnerability)
    int target = 0;          // switch targets: +1 next foe, -1 previous (right stick / key)
    // Menu navigation (edge-triggered)
    bool up = false, down = false, left = false, right = false;
    bool confirm = false, back = false;
    bool sort = false, filter = false;   // menus: Tab / R keys, gamepad X / Y (the Box)
};

// player 0 or 1. twoPlayer picks the shared-keyboard layouts instead of the solo layout.
PlayerInput ReadInput(int player, bool twoPlayer);
// Combined navigation for menus: either player, any layout, plus Enter/Space/Esc/Backspace/arrows.
PlayerInput MenuInput();

// On-screen labels for a player's bindings (gamepad names if they last used a gamepad).
const char* AttackKeyLabel(int player, int slot, bool twoPlayer);
const char* SwapKeyLabel(int player, bool twoPlayer);
const char* DodgeKeyLabel(int player, bool twoPlayer);
const char* JumpKeyLabel(int player, bool twoPlayer);
const char* SwapPrevKeyLabel(int player, bool twoPlayer);
const char* TargetKeyLabel(int player, bool twoPlayer);
const char* ConfirmLabel(int player, bool twoPlayer);
const char* BackLabel(int player, bool twoPlayer);
