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

namespace {
// Menus use the default keys / buttons, whatever the battle controls are bound to.
bool MenuKeyPressed(int layout, Act a) {
    if (layout < 0) return false;
    const int* k = DefaultKeys(layout, a);
    return (k[0] && IsKeyPressed(k[0])) || (k[1] && IsKeyPressed(k[1]));
}
bool MenuPadPressed(int p, Act a) {
    int b = a == Act::Up ? GAMEPAD_BUTTON_LEFT_FACE_UP : a == Act::Down ? GAMEPAD_BUTTON_LEFT_FACE_DOWN : a == Act::Left ? GAMEPAD_BUTTON_LEFT_FACE_LEFT
          : a == Act::Right ? GAMEPAD_BUTTON_LEFT_FACE_RIGHT : DefaultPadButton(a);   // menus: the D-pad always navigates
    return b && IsGamepadAvailable(p) && IsGamepadButtonPressed(p, b);
}
} // namespace

int gInputOnly = -1;   // see input.h

PlayerInput ReadInput(int p, bool twoPlayer) {
#ifdef ARENA_AUTOTEST
    if (gAutoPadOn && p == 0) return gAutoPad;
#endif
    PlayerInput in;
    if (gInputOnly >= 0 && p != gInputOnly) return in;
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
    if (KeyPressed(lay, Act::Target)) in.target = 1;
    in.jump = KeyPressed(lay, Act::Jump);
    in.mega = KeyPressed(lay, Act::Mega);
    const Act items[4] = {Act::Item1, Act::Item2, Act::Item3, Act::Item4};
    for (int i = 0; i < 4; i++) in.item[i] = KeyPressed(lay, items[i]);
    in.swapHeld = KeyDown(lay, Act::Swap);
    if (!twoPlayer) {   // keys 1-6: straight to that team slot
        const int nums[6] = {KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE, KEY_SIX};
        for (int k = 0; k < 6; k++) if (IsKeyPressed(nums[k])) in.swapTo = k;
    }
    in.confirm = MenuKeyPressed(lay, Act::Move1);
    in.back = MenuKeyPressed(lay, Act::Move2);
    in.up = MenuKeyPressed(lay, Act::Up);
    in.down = MenuKeyPressed(lay, Act::Down);
    in.left = MenuKeyPressed(lay, Act::Left);
    in.right = MenuKeyPressed(lay, Act::Right);
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
        if (IsGamepadButtonDown(p, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) in.dpad.x += 1;
        if (IsGamepadButtonDown(p, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) in.dpad.x -= 1;
        if (IsGamepadButtonDown(p, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) in.dpad.y += 1;
        if (IsGamepadButtonDown(p, GAMEPAD_BUTTON_LEFT_FACE_UP)) in.dpad.y -= 1;
        if (PadPressed(p, Act::Mega)) { in.mega = true; any = true; }
        for (int i = 0; i < 4; i++)
            if (PadPressed(p, items[i])) { in.item[i] = true; any = true; }
        for (int i = 0; i < 4; i++)
            if (PadPressed(p, moves[i])) { in.attack[i] = true; any = true; }
        if (PadPressed(p, Act::Swap)) { in.swap = true; any = true; }
        if (PadDown(p, Act::Swap)) in.swapHeld = true;
        in.stick2 = {GetGamepadAxisMovement(p, GAMEPAD_AXIS_RIGHT_X), GetGamepadAxisMovement(p, GAMEPAD_AXIS_RIGHT_Y)};
        if (PadPressed(p, Act::Pause)) in.pause = true;
        if (PadPressed(p, Act::Dodge)) { in.dodge = true; any = true; }
        if (PadPressed(p, Act::SwapPrev)) { in.swapPrev = true; any = true; }
        if (PadPressed(p, Act::Target)) { in.target = 1; any = true; }
        {   // flick the right stick left / right (or up / down) to switch targets
            static float prevR[2] = {0, 0};
            float rx = GetGamepadAxisMovement(p, GAMEPAD_AXIS_RIGHT_X), ry = GetGamepadAxisMovement(p, GAMEPAD_AXIS_RIGHT_Y);
            float r = std::fabs(rx) > std::fabs(ry) ? rx : ry;
            if (r > 0.6f && prevR[p] <= 0.6f) { in.target = 1; any = true; }
            if (r < -0.6f && prevR[p] >= -0.6f) { in.target = -1; any = true; }
            prevR[p] = r;
        }
        if (PadPressed(p, Act::Jump)) { in.jump = true; any = true; }
        if (MenuPadPressed(p, Act::Move1)) { in.confirm = true; any = true; }
        if (MenuPadPressed(p, Act::Move2)) { in.back = true; any = true; }
        if (MenuPadPressed(p, Act::Up)) in.up = true;
        if (MenuPadPressed(p, Act::Down)) in.down = true;
        if (MenuPadPressed(p, Act::Left)) in.left = true;
        if (MenuPadPressed(p, Act::Right)) in.right = true;

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
    PlayerInput m;
#ifdef ARENA_AUTOTEST
    if (gAutoMenu) {
        m.up = gAutoMenu & 1; m.down = gAutoMenu & 2; m.left = gAutoMenu & 4; m.right = gAutoMenu & 8;
        m.confirm = gAutoMenu & 16; m.back = gAutoMenu & 32;
        m.sort = gAutoMenu & 64; m.filter = gAutoMenu & 128;
        gAutoMenu = 0;
        return m;
    }
#endif
    if (gInputOnly >= 0) {   // split screen: only this player's keys / pad
        PlayerInput p = ReadInput(gInputOnly, true);
        m.up = p.up; m.down = p.down; m.left = p.left; m.right = p.right;
        m.confirm = p.confirm; m.back = p.back;
        m.swap = p.swap || p.swapPrev; m.pause = p.pause;
        return m;
    }
    PlayerInput a = ReadInput(0, false), b = ReadInput(1, true);
    m.up = a.up || b.up || IsKeyPressed(KEY_UP);
    m.down = a.down || b.down || IsKeyPressed(KEY_DOWN);
    m.left = a.left || b.left || IsKeyPressed(KEY_LEFT);
    m.right = a.right || b.right || IsKeyPressed(KEY_RIGHT);
    m.confirm = a.confirm || b.confirm || IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_KP_ENTER);
    m.back = a.back || b.back || IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACKSPACE);
    m.swap = a.swap || b.swap || a.swapPrev || b.swapPrev;
    m.pause = a.pause || b.pause;
    m.sort = IsKeyPressed(KEY_TAB);   // (F / G are confirm / back in some layouts)
    m.filter = IsKeyPressed(KEY_R);
    for (int p = 0; p < 2; p++) {
        if (!IsGamepadAvailable(p)) continue;
        m.sort = m.sort || IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
        m.filter = m.filter || IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_FACE_UP);
    }
    return m;
}

// Just the Sort / X button (doesn't use up a scripted autotest press meant for something else).
bool MenuSortPressed() {
#ifdef ARENA_AUTOTEST
    if (gAutoMenu) { if (gAutoMenu & 64) { gAutoMenu = 0; return true; } return false; }
#endif
    bool r = IsKeyPressed(KEY_TAB);
    for (int p = 0; p < 2; p++) if (IsGamepadAvailable(p) && IsGamepadButtonPressed(p, GAMEPAD_BUTTON_RIGHT_FACE_LEFT)) r = true;
    return r;
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
const char* TargetKeyLabel(int player, bool twoPlayer) { return Label(player, twoPlayer, Act::Target); }
const char* MegaKeyLabel(int player, bool twoPlayer) { return Label(player, twoPlayer, Act::Mega); }
const char* ItemKeyLabel(int player, int slot, bool twoPlayer) {
    const Act items[4] = {Act::Item1, Act::Item2, Act::Item3, Act::Item4};
    return Label(player, twoPlayer, items[slot & 3]);
}
static const char* MenuLabel(int player, bool twoPlayer, Act a) {   // menus use the default bindings
    if (gUsedPad[player] && IsGamepadAvailable(player)) return PadButtonName(DefaultPadButton(a));
    int lay = Layout(player, twoPlayer);
    if (lay < 0) return PadButtonName(DefaultPadButton(a));
    return KeyName(DefaultKeys(lay, a)[0]);
}
const char* ConfirmLabel(int player, bool twoPlayer) { return MenuLabel(player, twoPlayer, Act::Move1); }
const char* BackLabel(int player, bool twoPlayer) { return MenuLabel(player, twoPlayer, Act::Move2); }
