#pragma once

#include <Windows.h>

#include "diffbuild.hpp"
#include "inttypes.hpp"

namespace th06
{

// SPELL_PRACTICE_FRAME_SECTION_BASE + frame (frame 0 .. 65535). The spell practice menu's top row (フレーム指定)
// reports it

static const i32 SPELL_PRACTICE_FRAME_SECTION_BASE = 1000;

enum SpellPracticeOpenMenuResult
{

    SPELL_PRACTICE_OPEN_RESULT_NOT_HANDLED = 0,

    SPELL_PRACTICE_OPEN_RESULT_STAY_IN_MENU = 1,

    SPELL_PRACTICE_OPEN_RESULT_START_GAME = 2,

    SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING = 3,
};

typedef i32(__stdcall *SpellPracticeOpenMenuFn)(HWND gameWindow, i32 difficulty, i32 character, i32 shotType,
                                                i32 stageIndex, void *fontPtr);
typedef void(__stdcall *SpellPracticeSetCursorFn)(i32 cursorIndex);
typedef i32(__stdcall *SpellPracticeUpdateMenuFn)(u16 curInput, u16 lastInput, u16 isEighthFrameHeldInput);

struct SpellPracticeDrawItem
{
    f32 x;
    f32 y;
    u32 color;
    char text[128];
};

struct SpellPracticeDrawData
{
    i32 itemCount;
    SpellPracticeDrawItem items[32];
};

typedef i32(__stdcall *SpellPracticeGetDrawDataFn)(SpellPracticeDrawData *outDrawData, const void *catkData,
                                                   i32 catkCount);
typedef void(__stdcall *SpellPracticeCloseMenuFn)();
typedef void(__stdcall *SpellPracticeDrawMenuFn)();
typedef i32(__stdcall *SpellPracticeGetSelectedSpellFn)();
typedef void(__stdcall *SpellPracticeGetSelectedParamsFn)(i32 *mode, i32 *section);
typedef void(__stdcall *SpellPracticeSetCursorToSpellFn)(i32 spellIndex);
typedef i32(__cdecl *SpellPracticeApplyEclPatchFn)(i32 spellIndex, void *eclFile, i32 shot);

typedef i32(__stdcall *SpellPracticeGetLivesFn)();
typedef void(__stdcall *SpellPracticeSetLivesFn)(i32 lives);
typedef i32(__stdcall *SpellPracticeGetBombsFn)();
typedef void(__stdcall *SpellPracticeSetBombsFn)(i32 bombs);
typedef i32(__stdcall *SpellPracticeGetRankFn)();
typedef void(__stdcall *SpellPracticeSetRankFn)(i32 rank);

extern SpellPracticeApplyEclPatchFn g_SpellPracticeApplyEclPatchFn;
extern i32 g_SpellPracticeSelectedSpellIndex;
extern HMODULE g_SpellPracticeDllHandle;
}; // namespace th06
