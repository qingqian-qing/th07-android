#pragma once

#include "inttypes.hpp"
#include <SDL3/SDL.h>

// Screen-space virtual buttons for touch input (no ImGui).
// Gameplay: left pillarbox ESC/Z/S/X (pause/shoot/focus/bomb).
// Menus:    top-left "<" (back), top-right ">" (confirm).
// Hit-testing happens in game coordinates where X<0 is the left pillarbox
// and X>640 is the right pillarbox (mirrors th06-sdl2).
namespace TouchButtons
{
enum class Anchor : u8
{
    LeftPillar,
    RightPillar,
};

struct ButtonInfo
{
    Anchor anchor;
    f32 gameY;        // game-coordinate Y (0..480)
    f32 gameRadius;   // visual radius in game coordinates
    f32 fillR, fillG, fillB, fillA;   // normal fill color
    f32 borderR, borderG, borderB, borderA; // border ring color
    const char *label;
    bool held;
};

void Init();
void Reset();

// Returns true when the finger was consumed by a button (caller must NOT
// pass it to the gesture/movement system).
bool HandleFingerDown(SDL_FingerID fingerId, f32 gameX, f32 gameY);
bool HandleFingerUp(SDL_FingerID fingerId);

// Per-frame button flags (toggle state + one-shot pulses, pulses cleared).
u16 GetButtonFlags();

// Fill out[] with rendering info for the buttons visible in the current
// scene. Returns the number of buttons.
i32 GetButtonInfo(ButtonInfo *out, i32 maxCount);
} // namespace TouchButtons
