#pragma once

#include "inttypes.hpp"
#include <SDL3/SDL.h>

// Self-drawn practice menu (no ImGui). A button on the right pillarbox
// toggles a panel listing practice features; tapping a row toggles it.
// Touch scheme 2 (th06-style 1:1) is a separate row, not a practice feature.
namespace PracticeMenu
{
enum Feature
{
    FEAT_MUTEKI = 0,    // 无敌
    FEAT_LOCK_LIVES,    // 锁残（死后无残机也不触发续关，继续玩）
    FEAT_INFBOMBS,      // 无限符卡
    FEAT_INFPOWER,      // 锁火力
    FEAT_AUTOBOMB,      // 自动符卡（被弹瞬间自动放符卡）
    FEAT_COUNT,
};

void Init();

bool IsEnabled(Feature f);
void SetEnabled(Feature f, bool on);

// Touch scheme 2 (th06-sdl2 style 1:1 finger mapping, uncapped).
bool IsTouchScheme2();
void SetTouchScheme2(bool on);

bool IsOpen();
void ToggleOpen();

// Returns true when the finger was consumed by the practice menu
// (open button or panel row).
bool HandleFingerDown(SDL_FingerID fingerId, f32 gameX, f32 gameY);
bool HandleFingerUp(SDL_FingerID fingerId);

// Called every frame; enforces InfBombs / InfPower.
void Update();

// Rendering info.
struct OpenButtonInfo
{
    f32 gameY;
    f32 gameRadius;
    const char *label;
    bool held;
};
bool GetOpenButtonInfo(OpenButtonInfo *out);

struct PanelItemInfo
{
    f32 gameY;
    const char *label;
    bool enabled;
    bool isTouchRow; // renders as "第2套触控" with its own state
};
i32 GetPanelItems(PanelItemInfo *out, i32 maxCount);
bool IsPanelOpen();
} // namespace PracticeMenu
