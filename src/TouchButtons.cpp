#include "TouchButtons.hpp"

#include <cmath>

#include "Controller.hpp"
#include "GameManager.hpp"

namespace TouchButtons
{

struct ButtonDef
{
    Anchor anchor;
    f32 centerX;    // game-coordinate X for hit-test (left: -radius, right: 640+radius)
    f32 centerY;
    f32 radius;     // visual radius
    f32 hitRadius;  // touch hit radius (larger than visual)
    u16 buttonFlag;
    bool isToggle;  // tap toggles on/off (Z shoot, S focus)
    bool isPulse;   // tap fires exactly one frame (ESC pause, X bomb, menu < >)
    const char *label;
    u32 fillColor;      // 0xAARRGGBB
    u32 fillPressed;
    u32 borderColor;
};

// Gameplay buttons on the left pillarbox (th06-sdl2 layout).
static const ButtonDef kGameButtons[] = {
    {Anchor::LeftPillar, -24.0f, 46.0f, 24.0f, 36.0f, TH_BUTTON_MENU,  false, true,  "ESC", 0x60FFFFFF, 0x98FFFFFF, 0xB0FFFFFF},
    {Anchor::LeftPillar, -28.0f, 222.0f, 28.0f, 40.0f, TH_BUTTON_SHOOT, true,  false, "Z",   0x60FFFFFF, 0x98FFFFFF, 0xB0FFFFFF},
    {Anchor::LeftPillar, -28.0f, 302.0f, 28.0f, 40.0f, TH_BUTTON_FOCUS, true,  false, "S",   0x60FFFFFF, 0x98FFFFFF, 0xB0FFFFFF},
    {Anchor::LeftPillar, -28.0f, 382.0f, 28.0f, 40.0f, TH_BUTTON_BOMB,  false, true,  "X",   0x60FFFFFF, 0x98FFFFFF, 0xB0FFFFFF},
};

// Menu buttons: "<" back (BOMB -> RETURNMENU edge), ">" confirm (SHOOT -> SELECTMENU edge).
static const ButtonDef kMenuButtons[] = {
    {Anchor::LeftPillar,  -22.0f, 30.0f, 22.0f, 34.0f, TH_BUTTON_BOMB,  false, true, "<", 0x60FFFFFF, 0x98FFFFFF, 0xB0FFFFFF},
    {Anchor::RightPillar, 640.0f + 22.0f, 30.0f, 22.0f, 34.0f, TH_BUTTON_SHOOT, false, true, ">", 0x60FFFFFF, 0x98FFFFFF, 0xB0FFFFFF},
};

static constexpr i32 kMaxButtons = 8;

static SDL_FingerID g_HeldFinger[kMaxButtons];
static bool g_Held[kMaxButtons] = {};
static u16 g_ToggleFlags = 0;
static u16 g_PulseFlags = 0;

static bool IsGameplayScene()
{
    return g_GameManager.notInMenu && !g_GameManager.isInPauseMenu &&
           !g_GameManager.isInRetryMenu && !g_GameManager.replay;
}

static const ButtonDef *GetButtons(i32 *count)
{
    if (IsGameplayScene())
    {
        *count = (i32)(sizeof(kGameButtons) / sizeof(kGameButtons[0]));
        return kGameButtons;
    }

    *count = (i32)(sizeof(kMenuButtons) / sizeof(kMenuButtons[0]));
    return kMenuButtons;
}

static void Unpack(u32 argb, f32 *r, f32 *g, f32 *b, f32 *a)
{
    *a = (f32)((argb >> 24) & 0xFF) / 255.0f;
    *r = (f32)((argb >> 16) & 0xFF) / 255.0f;
    *g = (f32)((argb >> 8) & 0xFF) / 255.0f;
    *b = (f32)(argb & 0xFF) / 255.0f;
}

void Init()
{
    Reset();
}

void Reset()
{
    for (i32 i = 0; i < kMaxButtons; i++)
    {
        g_Held[i] = false;
        g_HeldFinger[i] = -1;
    }
    g_ToggleFlags = 0;
    g_PulseFlags = 0;
}

bool HandleFingerDown(SDL_FingerID fingerId, f32 gameX, f32 gameY)
{
    i32 count;
    const ButtonDef *btns = GetButtons(&count);

    for (i32 i = 0; i < count; i++)
    {
        if (g_Held[i])
        {
            continue;
        }

        f32 dx = gameX - btns[i].centerX;
        f32 dy = gameY - btns[i].centerY;
        if (std::sqrt(dx * dx + dy * dy) <= btns[i].hitRadius)
        {
            g_Held[i] = true;
            g_HeldFinger[i] = fingerId;

            if (btns[i].isToggle)
            {
                g_ToggleFlags ^= btns[i].buttonFlag;
            }
            if (btns[i].isPulse)
            {
                g_PulseFlags |= btns[i].buttonFlag;
            }
            return true;
        }
    }

    return false;
}

bool HandleFingerUp(SDL_FingerID fingerId)
{
    i32 count;
    const ButtonDef *btns = GetButtons(&count);

    for (i32 i = 0; i < count; i++)
    {
        if (g_Held[i] && g_HeldFinger[i] == fingerId)
        {
            g_Held[i] = false;
            g_HeldFinger[i] = -1;
            return true;
        }
    }

    return false;
}

u16 GetButtonFlags()
{
    u16 flags = g_ToggleFlags | g_PulseFlags;
    g_PulseFlags = 0;
    return flags;
}

i32 GetButtonInfo(ButtonInfo *out, i32 maxCount)
{
    i32 count;
    const ButtonDef *btns = GetButtons(&count);
    if (count > maxCount)
    {
        count = maxCount;
    }

    for (i32 i = 0; i < count; i++)
    {
        out[i].anchor = btns[i].anchor;
        out[i].gameY = btns[i].centerY;
        out[i].gameRadius = btns[i].radius;
        Unpack(g_Held[i] ? btns[i].fillPressed : btns[i].fillColor,
               &out[i].fillR, &out[i].fillG, &out[i].fillB, &out[i].fillA);
        Unpack(btns[i].borderColor,
               &out[i].borderR, &out[i].borderG, &out[i].borderB, &out[i].borderA);
        out[i].label = btns[i].label;
        out[i].held = g_Held[i];
    }

    return count;
}

} // namespace TouchButtons
