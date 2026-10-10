#pragma once

#include "AnmVm.hpp"
#include "inttypes.hpp"

namespace th06
{
enum RetryGameMenuState
{
    RETRY_MENU_OPENING,
    RETRY_MENU_CURSOR_YES,
    RETRY_MENU_CURSOR_NO,
    RETRY_MENU_SELECTED_YES,
    RETRY_MENU_SELECTED_NO,
};

enum UpdateGameMenuState
{
    GAME_MENU_PAUSE_OPENING,
    GAME_MENU_PAUSE_CURSOR_UNPAUSE,
    GAME_MENU_PAUSE_CURSOR_QUIT,
    GAME_MENU_PAUSE_SELECTED_UNPAUSE,
    GAME_MENU_QUIT_CURSOR_YES,
    GAME_MENU_QUIT_CURSOR_NO,
    GAME_MENU_QUIT_SELECTED_YES,
};

struct StageMenu
{
    StageMenu();

    i32 OnUpdateGameMenu();
    i32 OnUpdateRetryMenu();

    void OnDrawGameMenu();
    void OnDrawRetryMenu();

    u32 curState;

    i32 numFrames;
    AnmVm menuSprites[6];
    AnmVm menuBackground;
};
ZUN_ASSERT_SIZE(StageMenu, 0x778);
}; // namespace th06
