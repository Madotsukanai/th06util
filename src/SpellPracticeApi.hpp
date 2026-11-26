#pragma once

#include <Windows.h>

#include "diffbuild.hpp"
#include "inttypes.hpp"

namespace th06
{

// SPELL_PRACTICE_FRAME_SECTION_BASE + frame (frame 0 .. 65535). The spell practice menu's top row (フレーム指定)
// reports it

static const i32 SPELL_PRACTICE_FRAME_SECTION_BASE = 1000;

static const i32 SPELL_PRACTICE_POWER_AUTO = -1;
static const i32 SPELL_PRACTICE_POWER_MAX = 128;
static const i32 SPELL_PRACTICE_POWER_STEP_MAX = 16;
static const i32 SPELL_PRACTICE_SCORE_MAX = 999999999;
static const i32 SPELL_PRACTICE_SCORE_DIGITS = 9;

enum SpellPracticeOpenMenuResult
{

    SPELL_PRACTICE_OPEN_RESULT_NOT_HANDLED = 0,

    SPELL_PRACTICE_OPEN_RESULT_STAY_IN_MENU = 1,

    SPELL_PRACTICE_OPEN_RESULT_START_GAME = 2,

    SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING = 3,
};

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

extern i32 g_SpellPracticeSelectedSpellIndex;
}; // namespace th06

extern "C" i32 __stdcall th06_spell_practice_open_menu(HWND gameWindow, i32 difficulty, i32 character, i32 shotType,
                                                      i32 stageIndex, void *fontPtr);
extern "C" void __stdcall th06_spell_practice_draw_menu();
extern "C" i32 __stdcall th06_spell_practice_update_menu(u16 curInput, u16 lastInput, u16 isEighthFrameHeldInput);
extern "C" i32 __stdcall th06_spell_practice_get_draw_data(th06::SpellPracticeDrawData *outDrawData,
                                                          const void *catkData, i32 catkCount);
extern "C" i32 __stdcall th06_spell_practice_get_lives();
extern "C" void __stdcall th06_spell_practice_set_lives(i32 lives);
extern "C" i32 __stdcall th06_spell_practice_get_bombs();
extern "C" void __stdcall th06_spell_practice_set_bombs(i32 bombs);
extern "C" i32 __stdcall th06_spell_practice_get_rank();
extern "C" void __stdcall th06_spell_practice_set_rank(i32 rank);
extern "C" i32 __stdcall th06_spell_practice_get_power();
extern "C" void __stdcall th06_spell_practice_set_power(i32 power);
extern "C" i32 __stdcall th06_spell_practice_get_score();
extern "C" void __stdcall th06_spell_practice_set_score(i32 score);
extern "C" i32 __stdcall th06_spell_practice_get_selected_spell();
extern "C" void __stdcall th06_spell_practice_get_selected_params(i32 *mode, i32 *section);
extern "C" void __stdcall th06_spell_practice_set_cursor_to_spell(i32 spellIndex);
extern "C" int __cdecl th06_spell_practice_apply_ecl_patch(i32 spellIndex, void *eclFile, i32 shot);
