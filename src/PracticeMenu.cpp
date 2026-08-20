#include "PracticeMenu.hpp"

#include <cmath>

#include "GameManager.hpp"

namespace PracticeMenu
{

static bool s_enabled[FEAT_COUNT] = {};
static bool s_touchScheme2 = false; // 第2套触控（红魔乡式 1:1）
static bool s_open = false;
static bool s_openBtnHeld = false;
static SDL_FingerID s_openBtnFinger = -1;
static bool s_rowHeld[FEAT_COUNT + 1] = {};
static SDL_FingerID s_rowFinger[FEAT_COUNT + 1] = {};

// Open button: right pillarbox, near the X button height.
static constexpr f32 kOpenBtnGameY = 300.0f;
static constexpr f32 kOpenBtnRadius = 24.0f;
static constexpr f32 kOpenBtnHitRadius = 36.0f;
static constexpr f32 kOpenBtnGameX = 640.0f + kOpenBtnRadius; // right pillarbox center

// Panel: in the game area, top-left (matches the renderer's kPanelGameX/Y).
static constexpr f32 kPanelX = 12.0f;
static constexpr f32 kPanelW = 216.0f;
static constexpr f32 kPanelTop = 14.0f;
static constexpr f32 kRowH = 34.0f;
static constexpr f32 kRowHitH = 38.0f;

static const char *kFeatureLabels[FEAT_COUNT] = {
    "无敌",
    "锁残",
    "无限符卡",
    "锁火力",
    "自动符卡",
};

static const char *kTouchRowLabel = "第2套触控";

void Init()
{
    for (i32 i = 0; i < FEAT_COUNT; i++)
    {
        s_enabled[i] = false;
        s_rowHeld[i] = false;
        s_rowFinger[i] = -1;
    }
    s_touchScheme2 = false;
    s_rowHeld[FEAT_COUNT] = false;
    s_rowFinger[FEAT_COUNT] = -1;
    s_open = false;
    s_openBtnHeld = false;
    s_openBtnFinger = -1;
}

bool IsEnabled(Feature f)
{
    return f >= 0 && f < FEAT_COUNT && s_enabled[f];
}

void SetEnabled(Feature f, bool on)
{
    if (f >= 0 && f < FEAT_COUNT)
    {
        s_enabled[f] = on;
    }
}

bool IsTouchScheme2()
{
    return s_touchScheme2;
}

void SetTouchScheme2(bool on)
{
    s_touchScheme2 = on;
}

bool IsOpen()
{
    return s_open;
}

void ToggleOpen()
{
    s_open = !s_open;
}

bool HandleFingerDown(SDL_FingerID fingerId, f32 gameX, f32 gameY)
{
    // Open button on the right pillarbox.
    f32 dx = gameX - kOpenBtnGameX;
    f32 dy = gameY - kOpenBtnGameY;
    if (!s_openBtnHeld && std::sqrt(dx * dx + dy * dy) <= kOpenBtnHitRadius)
    {
        s_openBtnHeld = true;
        s_openBtnFinger = fingerId;
        ToggleOpen();
        return true;
    }

    if (s_open)
    {
        // Panel rows: FEAT_COUNT practice features + 1 touch row.
        for (i32 i = 0; i <= FEAT_COUNT; i++)
        {
            if (s_rowHeld[i])
            {
                continue;
            }
            f32 rowY = kPanelTop + i * kRowH + kRowH * 0.5f;
            if (gameX >= kPanelX && gameX <= kPanelX + kPanelW &&
                gameY >= rowY - kRowHitH * 0.5f && gameY <= rowY + kRowHitH * 0.5f)
            {
                s_rowHeld[i] = true;
                s_rowFinger[i] = fingerId;
                if (i < FEAT_COUNT)
                {
                    s_enabled[i] = !s_enabled[i];
                }
                else
                {
                    s_touchScheme2 = !s_touchScheme2;
                }
                return true;
            }
        }
    }

    return false;
}

bool HandleFingerUp(SDL_FingerID fingerId)
{
    if (s_openBtnHeld && s_openBtnFinger == fingerId)
    {
        s_openBtnHeld = false;
        s_openBtnFinger = -1;
        return true;
    }
    for (i32 i = 0; i <= FEAT_COUNT; i++)
    {
        if (s_rowHeld[i] && s_rowFinger[i] == fingerId)
        {
            s_rowHeld[i] = false;
            s_rowFinger[i] = -1;
            return true;
        }
    }
    return false;
}

void Update()
{
    if (s_enabled[FEAT_INFBOMBS])
    {
        if (g_GameManager.globals->bombsRemaining < 1.0f)
        {
            g_GameManager.globals->bombsRemaining = 1.0f;
        }
    }
    if (s_enabled[FEAT_INFPOWER])
    {
        if (g_GameManager.globals->currentPower < 128.0f)
        {
            g_GameManager.globals->currentPower = 128.0f;
        }
    }
}

bool GetOpenButtonInfo(OpenButtonInfo *out)
{
    out->gameY = kOpenBtnGameY;
    out->gameRadius = kOpenBtnRadius;
    out->label = "PRAC";
    out->held = s_openBtnHeld;
    return true;
}

i32 GetPanelItems(PanelItemInfo *out, i32 maxCount)
{
    if (!s_open)
    {
        return 0;
    }
    i32 count = (FEAT_COUNT + 1) < maxCount ? (FEAT_COUNT + 1) : maxCount;
    for (i32 i = 0; i < count; i++)
    {
        out[i].gameY = kPanelTop + i * kRowH + kRowH * 0.5f;
        if (i < FEAT_COUNT)
        {
            out[i].label = kFeatureLabels[i];
            out[i].enabled = s_enabled[i];
            out[i].isTouchRow = false;
        }
        else
        {
            out[i].label = kTouchRowLabel;
            out[i].enabled = s_touchScheme2;
            out[i].isTouchRow = true;
        }
    }
    return count;
}

bool IsPanelOpen()
{
    return s_open;
}

} // namespace PracticeMenu
