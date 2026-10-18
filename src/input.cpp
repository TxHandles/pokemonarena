#include "input.h"
#include <cmath>
#include "settings.h"

namespace {

Vector2 gPrevStick[2] = {{0, 0}, {0, 0}};
bool gUsedPad[2] = {false, false};

int Layout(int player, bool twoPlayer) {
    if (!twoPlayer) return player == 0 ? LAYOUT_SOLO : -1;
    return player == 0 ? LAYOUT_P1 : LAYOUT_P2;
}

bool KeyDown(int layout, Act a) {
    if (layout < 0) return false;
    const int* k = Cfg().keys[layout][(int)a];
    return (k[0] && IsKeyDown(k[0])) || (k[1] && IsKeyDown(k[1]));
}
bool KeyPressed(int layout, Act a) {
    if (layout < 0) return false;
    const int* k = Cfg().keys[layout][(int)a];
    return (k[0] && IsKeyPressed(k[0])) || (k[1] && IsKeyPressed(k[1]));
}
bool PadDown(int p, Act a) {
    int b = Cfg().pad[p][(int)a];
    return b && IsGamepadAvailable(p) && IsGamepadButtonDown(p, b);
}
bool PadPressed(int p, Act a) {
    int b = Cfg().pad[p][(int)a];
    return b && IsGamepadAvailable(p) && IsGamepadButtonPressed(p, b);
}

} // namespace

#ifdef ARENA_AUTOTEST
PlayerInput gAutoPad;          // autotest: scripted battle input for player 1
bool gAutoPadOn = false;
#endif

PlayerInput ReadInput(int p, bool twoPlayer) {
#ifdef ARENA_AUTOTEST
    if (gAutoPadOn && p == 0) return gAutoPad;
#endif
    PlayerInput in;
    int lay = Layout(p, twoPlayer);
    float x = 0, y = 0;

    if (KeyDown(lay, Act::Right)) x += 1;
    if (KeyDown(lay, Act::Left)) x -= 1;
    if (KeyDown(lay, Act::Down)) y += 1;
    if (KeyDown(lay, Act::Up)) y -= 1;
    const Act moves[4] = {Act::Move1, Act::Move2, Act::Move3, Act::Move4};
    for (int i = 0; i < 4; i++) in.attack[i] = KeyPressed(lay, moves[i]);
    in.swap = KeyPressed(lay, Act::Swap);
    in.pause = KeyPressed(lay, Act::Pause);
    in.dodge = KeyPressed(lay, Act::Dodge);
    in.swapPrev = KeyPressed(lay, Act::SwapPrev);
    in.jump = KeyPressed(lay, Act::Jump);
    in.confirm = KeyPressed(lay, Act::Move1);
    in.back = KeyPressed(lay, Act::Move2);
    in.up = KeyPressed(lay, Act::Up);
    in.down = KeyPressed(lay, Act::Down);
    in.left = KeyPressed(lay, Act::Left);
    in.right = KeyPressed(lay, Act::Right);
    bool anyKey = x != 0 || y != 0 || in.confirm || in.swap || in.swapPrev || in.dodge || in.jump || in.attack[1] || in.attack[2] || in.attack[3];
    if (anyKey) gUsedPad[p] = false;

    if (IsGamepadAvailable(p)) {
        float ax = GetGamepadAxisMovement(p, GAMEPAD_AXIS_LEFT_X);
        float ay = GetGamepadAxisMovement(p, GAMEPAD_AXIS_LEFT_Y);
        if (std::fabs(ax) < 0.25f) ax = 0;
        if (std::fabs(ay) < 0.25f) ay = 0;
        x += ax;
        y += ay;
        if (PadDown(p, Act::Right)) x += 1;
        if (PadDown(p, Act::Left)) x -= 1;
        if (PadDown(p, Act::Down)) y += 1;
        if (PadDown(p, Act::Up)) y -= 1;
        bool any = ax != 0 || ay != 0;
        for (int i = 0; i < 4; i++)
            if (PadPressed(p, moves[i])) { in.attack[i] = true; any = true; }
        if (PadPressed(p, Act::Swap)) { in.swap = true; any = true; }
        if (PadPressed(p, Act::Pause)) in.pause = true;
        if (PadPressed(p, Act::Dodge)) { in.dodge = true; any = true; }
        if (PadPressed(p, Act::SwapPrev)) { in.swapPrev = true; any = true; }
        if (PadPressed(p, Act::Jump)) { in.jump = true; any = true; }
        if (PadPressed(p, Act::Move1)) in.confirm = true;
        if (PadPressed(p, Act::Move2)) in.back = true;
        if (PadPressed(p, Act::Up)) in.up = true;
        if (PadPressed(p, Act::Down)) in.down = true;
        if (PadPressed(p, Act::Left)) in.left = true;
        if (PadPressed(p, Act::Right)) in.right = true;

        Vector2 prev = gPrevStick[p];
        if (ay < -0.6f && prev.y >= -0.6f) in.up = true;
        if (ay > 0.6f && prev.y <= 0.6f) in.down = true;
        if (ax < -0.6f && prev.x >= -0.6f) in.left = true;
        if (ax > 0.6f && prev.x <= 0.6f) in.right = true;
        gPrevStick[p] = {ax, ay};
        if (any) gUsedPad[p] = true;
    }

    float l = std::sqrt(x * x + y * y);
    if (l > 1.0f) { x /= l; y /= l; }
    in.move = {x, y};
    return in;
}

#ifdef ARENA_AUTOTEST
int gAutoMenu = 0;   // autotest: one scripted menu press (1 up, 2 down, 4 left, 8 right, 16 confirm, 32 back)
#endif
PlayerInput MenuInput() {
    PlayerInput a = ReadInput(0, false), b = ReadInput(1, true);
    PlayerInput m;
#ifdef ARENA_AUTOTEST
    if (gAutoMenu) {
        m.up = gAutoMenu & 1; m.down = gAutoMenu & 2; m.left = gAutoMenu & 4; m.right = gAutoMenu & 8;
        m.confirm = gAutoMenu & 16; m.back = gAutoMenu & 32;
        gAutoMenu = 0;
        return m;
    }
#endif
    m.up = a.up || b.up || IsKeyPressed(KEY_UP);
    m.down = a.down || b.down || IsKeyPressed(KEY_DOWN);
    m.left = a.left || b.left || IsKeyPressed(KEY_LEFT);
    m.right = a.right || b.right || IsKeyPressed(KEY_RIGHT);
    m.confirm = a.confirm || b.confirm || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_KP_ENTER);
    m.back = a.back || b.back || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
    m.swap = a.swap || b.swap || a.swapPrev || b.swapPrev;
    m.pause = a.pause || b.pause;
    return m;
}

static const char* Label(int player, bool twoPlayer, Act a) {
    if (gUsedPad[player] && IsGamepadAvailable(player)) return PadButtonName(Cfg().pad[player][(int)a]);
    int lay = Layout(player, twoPlayer);
    if (lay < 0) return PadButtonName(Cfg().pad[player][(int)a]);
    return KeyName(Cfg().keys[lay][(int)a][0]);
}

const char* AttackKeyLabel(int player, int slot, bool twoPlayer) {
    const Act moves[4] = {Act::Move1, Act::Move2, Act::Move3, Act::Move4};
    if (slot < 0 || slot > 3) return "";
    return Label(player, twoPlayer, moves[slot]);
}
const char* SwapKeyLabel(int player, bool twoPlayer) { return Label(player, twoPlayer, Act::Swap); }
const char* DodgeKeyLabel(int player, bool twoPlayer) { return Label(player, twoPlayer, Act::Dodge); }
const char* JumpKeyLabel(int player, bool twoPlayer) { return Label(player, twoPlayer, Act::Jump); }
const char* SwapPrevKeyLabel(int player, bool twoPlayer) { return Label(player, twoPlayer, Act::SwapPrev); }
const char* ConfirmLabel(int player, bool twoPlayer) { return Label(player, twoPlayer, Act::Move1); }
const char* BackLabel(int player, bool twoPlayer) { return Label(player, twoPlayer, Act::Move2); }
