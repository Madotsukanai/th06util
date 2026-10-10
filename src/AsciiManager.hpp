#pragma once

#include <d3dx8math.h>

#include "AnmManager.hpp"
#include "Chain.hpp"
#include "StageMenu.hpp"
#include "ZunResult.hpp"
#include "ZunTimer.hpp"
#include "inttypes.hpp"

namespace th06
{
#define TEXT_RIGHT_ARROW 0x7f

#define TEXT_LEFT_ARROW 0x80

struct AsciiManagerString
{
    char text[64];
    D3DXVECTOR3 position;
    D3DCOLOR color;
    D3DXVECTOR2 scale;

    u32 isSelected;

    u32 isGui;
};
ZUN_ASSERT_SIZE(AsciiManagerString, 0x60);

struct AsciiManagerPopup
{
    char digits[8];
    D3DXVECTOR3 position;
    D3DCOLOR color;
    ZunTimer timer;
    u8 inUse;
    u8 characterCount;
};
ZUN_ASSERT_SIZE(AsciiManagerPopup, 0x28);

struct WeirdPadding
{
    WeirdPadding()
    {
        i32 pad11, pad12, pad13, pad14, pad15, pad16, pad17, pad18;
        i32 pad21, pad22, pad23, pad24, pad25, pad26, pad27, pad28;
        i32 pad31, pad32;
    }
    u32 unk;
};

#define ASCII_SCORE_POPUPS_COUNT 512
#define ASCII_PLAYER_POPUPS_COUNT 3
#define ASCII_TOTAL_POPUPS_COUNT (ASCII_SCORE_POPUPS_COUNT + ASCII_PLAYER_POPUPS_COUNT)

#define ASCII_SCORE_POPUPS_START 0
#define ASCII_PLAYER_POPUPS_START ASCII_SCORE_POPUPS_COUNT

#define ASCII_STRING_COUNT 256

struct AsciiManager
{
    AsciiManager();

    static ZunResult RegisterChain();
    static void CutChain();

    static ChainCallbackResult OnUpdate(AsciiManager *s);
    static ChainCallbackResult OnDrawMenus(AsciiManager *s);
    static ChainCallbackResult OnDrawPopups(AsciiManager *s);
    static ZunResult AddedCallback(AsciiManager *s);
    static ZunResult DeletedCallback(AsciiManager *s);

    void InitializeVms();

    void DrawStrings();
    void DrawPopupsWithHwVertexProcessing();
    void DrawPopupsWithoutHwVertexProcessing();

    void AddString(D3DXVECTOR3 *position, const char *text);
    void AddFormatText(D3DXVECTOR3 *position, const char *fmt, ...);
    void CreatePopup1(D3DXVECTOR3 *position, i32 value, D3DCOLOR color);
    void CreatePopup2(D3DXVECTOR3 *position, i32 value, D3DCOLOR color);

    void SetColor(ZunColor color)
    {
        this->color = color;
    }

    AnmVm vm0;
    AnmVm vm1;
    AsciiManagerString strings[ASCII_STRING_COUNT];
    i32 numStrings;
    D3DCOLOR color;
    D3DXVECTOR2 scale;

    u32 isGui;

    ZunBool isSelected;
    i32 nextPopupIndex1;
    i32 nextPopupIndex2;

    WeirdPadding unk3;

    StageMenu gameMenu;

    StageMenu retryMenu;
    AsciiManagerPopup popups[ASCII_TOTAL_POPUPS_COUNT];
};
ZUN_ASSERT_SIZE(AsciiManager, 0xc1ac);
extern AsciiManager g_AsciiManager;

extern bool g_PopupForceDraw;
}; // namespace th06
