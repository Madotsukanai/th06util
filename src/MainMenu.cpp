#include "GameWindow.hpp"
#include <D3DX8.h>
#include <cstdio>
#include <direct.h>
#include <windows.h>

#include "MainMenu.hpp"

#include "AnmManager.hpp"
#include "AsciiManager.hpp"
#include "ChainPriorities.hpp"
#include "Filesystem.hpp"
#include "GameErrorContext.hpp"
#include "GameManager.hpp"
#include "ReplayData.hpp"
#include "ReplayManager.hpp"
#include "ResultScreen.hpp"
#include "ScreenEffect.hpp"
#include "SoundPlayer.hpp"
#include "SpellPracticeDllApi.hpp"
#include "Supervisor.hpp"
#include "ZunBool.hpp"
#include "ZunColor.hpp"
#include "i18n.hpp"
#include "utils.hpp"

#include "Network.hpp"
#include "Rng.hpp"

namespace th06
{

char *g_ShortCharacterList[4] = {"ReimuA ", "ReimuB ", "MarisaA", "MarisaB"};
char *g_DifficultyList[5] = {"Easy   ", "Normal ", "Hard   ", "Lunatic", "Extra  "};
char *g_StageList[7] = {"Stage1", "Stage2", "Stage3", "Stage4", "Stage5", "Stage6", "Extra "};

#define MAX_REPLAY_FILES 150
static char s_replayFilePaths[MAX_REPLAY_FILES][MAX_PATH];
static char s_replayFileName[MAX_REPLAY_FILES][16];
static ReplayData s_replayFileData[MAX_REPLAY_FILES];
static i32 s_lastChosenReplay = 0;
// -1 = 未確認、>=0 = 確認待ちのリプレイインデックス
static i32 s_deleteConfirmIdx = -1;

static const char *g_ThpracSectionNames[71] = {
    "",           "ST1_MID1",    "ST1_MID2",   "ST1_BOSS1",   "ST1_BOSS2",  "ST1_BOSS3",   "ST1_BOSS4",   "ST2_MID1",
    "ST2_BOSS1",  "ST2_BOSS2",   "ST2_BOSS3",  "ST2_BOSS4",   "ST2_BOSS5",  "ST3_MID1",    "ST3_MID2",    "ST3_BOSS1",
    "ST3_BOSS2",  "ST3_BOSS3",   "ST3_BOSS4",  "ST3_BOSS5",   "ST3_BOSS6",  "ST3_BOSS7",   "ST4_BOOKS",   "ST4_MID1",
    "ST4_BOSS1",  "ST4_BOSS2",   "ST4_BOSS3",  "ST4_BOSS4",   "ST4_BOSS5",  "ST4_BOSS6",   "ST4_BOSS7",   "ST5_MID1",
    "ST5_MID2",   "ST5_BOSS1",   "ST5_BOSS2",  "ST5_BOSS3",   "ST5_BOSS4",  "ST5_BOSS5",   "ST5_BOSS6",   "ST6_MID1",
    "ST6_MID2",   "ST6_BOSS1",   "ST6_BOSS2",  "ST6_BOSS3",   "ST6_BOSS4",  "ST6_BOSS5",   "ST6_BOSS6",   "ST6_BOSS7",
    "ST6_BOSS8",  "ST6_BOSS9",   "ST7_MID1",   "ST7_MID2",    "ST7_MID3",   "ST7_END_NS1", "ST7_END_S1",  "ST7_END_NS2",
    "ST7_END_S2", "ST7_END_NS3", "ST7_END_S3", "ST7_END_NS4", "ST7_END_S4", "ST7_END_NS5", "ST7_END_S5",  "ST7_END_NS6",
    "ST7_END_S6", "ST7_END_NS7", "ST7_END_S7", "ST7_END_NS8", "ST7_END_S8", "ST7_END_S9",  "ST7_END_S10",
};

static ThpracReplayInfo g_CurrentReplayThprac = {0, -1, -1, -1, -1, -1, -1};

static bool g_SkipTitleIntro = false;

struct NetConfig
{
    char ip[16];
    char localIp[16];
    char portStr[6];
    i32 cursor;
    i32 port;
    ZunBool isHost;
};
static NetConfig g_NetConfig = {"127.000.000.001", "127.0.0.1", "10800", 0, 10800, false};

static void FormatIpAddress3Digits(const char *src, char *dst, int dstSize)
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;

    if (dstSize < 16)
    {
        return;
    }

    if (sscanf(src, "%u.%u.%u.%u", &a, &b, &c, &d) == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255)
    {
        sprintf(dst, "%03u.%03u.%03u.%03u", a, b, c, d);
    }
    else
    {
        strcpy(dst, "127.000.000.001");
    }
}

enum NetHostPrepareStatus
{
    NET_HOST_PREPARE_IDLE,
    NET_HOST_PREPARE_RUNNING,
    NET_HOST_PREPARE_READY,
    NET_HOST_PREPARE_READY_NO_UPNP,
    NET_HOST_PREPARE_FAILED,
    NET_HOST_PREPARE_CANCELLED,
};

static volatile LONG g_NetHostPrepareStatus = NET_HOST_PREPARE_IDLE;
static volatile LONG g_NetHostPrepareCancel = 0;

static DWORD WINAPI NetHostPrepareThread(void *)
{
    char ip[16];
    char formattedIp[16];

    g_NetworkManager.GetLocalIP(g_NetConfig.localIp, sizeof(g_NetConfig.localIp));

    strcpy(ip, "127.000.000.001");
    if (g_NetworkManager.GetPublicIP(ip, sizeof(ip)) != ZUN_SUCCESS)
    {
        strcpy(ip, g_NetConfig.localIp);
    }
    FormatIpAddress3Digits(ip, formattedIp, sizeof(formattedIp));

    if (g_NetHostPrepareCancel)
    {
        InterlockedExchange(&g_NetHostPrepareStatus, NET_HOST_PREPARE_CANCELLED);
        return 0;
    }

    strcpy(g_NetConfig.ip, formattedIp);
    if (g_NetworkManager.StartServer(g_NetConfig.port) == ZUN_SUCCESS)
    {
        if (g_NetHostPrepareCancel)
        {
            g_NetworkManager.Cleanup(false);
            InterlockedExchange(&g_NetHostPrepareStatus, NET_HOST_PREPARE_CANCELLED);
        }
        else
        {

            LONG readyStatus =
                g_NetworkManager.IsPortMapped() ? NET_HOST_PREPARE_READY : NET_HOST_PREPARE_READY_NO_UPNP;
            InterlockedExchange(&g_NetHostPrepareStatus, readyStatus);
        }
    }
    else
    {
        InterlockedExchange(&g_NetHostPrepareStatus, NET_HOST_PREPARE_FAILED);
    }
    return 0;
}

static ZunBool StartNetHostPrepareThread()
{
    HANDLE thread;

    if (g_NetHostPrepareStatus == NET_HOST_PREPARE_RUNNING)
    {
        return true;
    }

    InterlockedExchange(&g_NetHostPrepareCancel, 0);
    InterlockedExchange(&g_NetHostPrepareStatus, NET_HOST_PREPARE_RUNNING);
    thread = CreateThread(NULL, 0, NetHostPrepareThread, NULL, 0, NULL);
    if (thread == NULL)
    {
        InterlockedExchange(&g_NetHostPrepareStatus, NET_HOST_PREPARE_FAILED);
        return false;
    }
    CloseHandle(thread);
    return true;
}

i16 g_LastJoystickInput;
HMODULE g_SpellPracticeDllHandle;
SpellPracticeOpenMenuFn g_SpellPracticeOpenMenuFn;
SpellPracticeUpdateMenuFn g_SpellPracticeUpdateMenuFn;
SpellPracticeGetDrawDataFn g_SpellPracticeGetDrawDataFn;
th06::SpellPracticeCloseMenuFn g_SpellPracticeCloseMenuFn;
th06::SpellPracticeDrawMenuFn g_SpellPracticeDrawMenuFn;
th06::SpellPracticeGetSelectedSpellFn g_SpellPracticeGetSelectedSpellFn;
th06::SpellPracticeGetSelectedParamsFn g_SpellPracticeGetSelectedParamsFn;
th06::SpellPracticeSetCursorToSpellFn g_SpellPracticeSetCursorToSpellFn;
th06::SpellPracticeApplyEclPatchFn g_SpellPracticeApplyEclPatchFn;
th06::SpellPracticeGetLivesFn g_SpellPracticeGetLivesFn;
th06::SpellPracticeSetLivesFn g_SpellPracticeSetLivesFn;
th06::SpellPracticeGetBombsFn g_SpellPracticeGetBombsFn;
th06::SpellPracticeSetBombsFn g_SpellPracticeSetBombsFn;
th06::SpellPracticeGetRankFn g_SpellPracticeGetRankFn;
th06::SpellPracticeSetRankFn g_SpellPracticeSetRankFn;
static i32 g_SpellPracticeSavedLives = 8;
static i32 g_SpellPracticeSavedBombs = 8;
static ZunBool g_SpellPracticeFrontAnmLoaded = false;
static ZunBool g_SpellPracticeStageConfiguring = false;
static i32 g_SpellPracticeConfigCursor = 0;
static i32 g_SpellPracticeSelectedStage = 0;
i32 g_SpellPracticeSelectedSpellIndex = -1;
ZunBool g_IsSpellPracticeDllMenuActive;
AnmVm g_SpellPracticeVms[24];
static AnmVm g_SpellPracticeLivesIconVm;
static AnmVm g_SpellPracticeBombsIconVm;
static ChainElem g_SpellPracticeDrawChain;

static void LoadSpellPracticeFrontAnm()
{
    if (!g_SpellPracticeFrontAnmLoaded)
    {
        g_Supervisor.LoadPbg3(CM_PBG3_INDEX, TH_CM_DAT_FILE);
        g_AnmManager->LoadAnm(ANM_FILE_FRONT, "data/front.anm", ANM_OFFSET_FRONT);
        g_AnmManager->SetAndExecuteScriptIdx(&g_SpellPracticeLivesIconVm, ANM_SCRIPT_FRONT_START + 16);
        g_AnmManager->SetAndExecuteScriptIdx(&g_SpellPracticeBombsIconVm, ANM_SCRIPT_FRONT_START + 17);
        g_SpellPracticeFrontAnmLoaded = true;
    }
}

static void UnloadSpellPracticeFrontAnm()
{
    if (g_SpellPracticeFrontAnmLoaded)
    {
        g_AnmManager->ReleaseAnm(ANM_FILE_FRONT);
        g_Supervisor.ReleasePbg3(CM_PBG3_INDEX);
        g_SpellPracticeFrontAnmLoaded = false;
    }
}

static void CloseSpellPracticeDllMenu()
{
    if (g_IsSpellPracticeDllMenuActive && g_SpellPracticeCloseMenuFn != NULL)
    {
        g_SpellPracticeCloseMenuFn();
    }
    for (i32 i = 0; i < 24; i++)
    {
        g_SpellPracticeVms[i].flags.isVisible = 0;
    }
    UnloadSpellPracticeFrontAnm();
    g_IsSpellPracticeDllMenuActive = false;
}

static void UnloadSpellPracticeDll()
{
    CloseSpellPracticeDllMenu();
    g_SpellPracticeOpenMenuFn = NULL;
    g_SpellPracticeUpdateMenuFn = NULL;
    g_SpellPracticeGetDrawDataFn = NULL;
    g_SpellPracticeCloseMenuFn = NULL;
    g_SpellPracticeDrawMenuFn = NULL;
    g_SpellPracticeGetSelectedSpellFn = NULL;
    g_SpellPracticeApplyEclPatchFn = NULL;
    g_SpellPracticeGetLivesFn = NULL;
    g_SpellPracticeSetLivesFn = NULL;
    g_SpellPracticeGetBombsFn = NULL;
    g_SpellPracticeSetBombsFn = NULL;
    g_SpellPracticeGetRankFn = NULL;
    g_SpellPracticeSetRankFn = NULL;
    g_SpellPracticeSetCursorToSpellFn = NULL;
    g_SpellPracticeSelectedSpellIndex = -1;
    if (g_SpellPracticeDllHandle != NULL)
    {
        FreeLibrary(g_SpellPracticeDllHandle);
        g_SpellPracticeDllHandle = NULL;
    }
}

static HMODULE EnsureSpellPracticeDllLoaded()
{
    if (g_SpellPracticeDllHandle != NULL)
    {
        return g_SpellPracticeDllHandle;
    }

    g_SpellPracticeDllHandle = LoadLibraryA("th06_spell_practice.dll");
    if (g_SpellPracticeDllHandle == NULL)
    {
        g_SpellPracticeDllHandle = LoadLibraryA("spell_practice.dll");
    }
    if (g_SpellPracticeDllHandle == NULL)
    {
        return NULL;
    }

    g_SpellPracticeOpenMenuFn =
        (SpellPracticeOpenMenuFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_open_menu");
    g_SpellPracticeUpdateMenuFn =
        (SpellPracticeUpdateMenuFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_update_menu");
    g_SpellPracticeGetDrawDataFn =
        (SpellPracticeGetDrawDataFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_get_draw_data");
    g_SpellPracticeCloseMenuFn =
        (SpellPracticeCloseMenuFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_close_menu");
    g_SpellPracticeDrawMenuFn =
        (th06::SpellPracticeDrawMenuFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_draw_menu");
    g_SpellPracticeApplyEclPatchFn = (th06::SpellPracticeApplyEclPatchFn)GetProcAddress(
        g_SpellPracticeDllHandle, "th06_spell_practice_apply_ecl_patch");
    g_SpellPracticeGetSelectedSpellFn = (th06::SpellPracticeGetSelectedSpellFn)GetProcAddress(
        g_SpellPracticeDllHandle, "th06_spell_practice_get_selected_spell");
    g_SpellPracticeGetSelectedParamsFn = (th06::SpellPracticeGetSelectedParamsFn)GetProcAddress(
        g_SpellPracticeDllHandle, "th06_spell_practice_get_selected_params");
    g_SpellPracticeSetCursorToSpellFn = (th06::SpellPracticeSetCursorToSpellFn)GetProcAddress(
        g_SpellPracticeDllHandle, "th06_spell_practice_set_cursor_to_spell");
    g_SpellPracticeGetLivesFn =
        (th06::SpellPracticeGetLivesFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_get_lives");
    g_SpellPracticeSetLivesFn =
        (th06::SpellPracticeSetLivesFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_set_lives");
    g_SpellPracticeGetBombsFn =
        (th06::SpellPracticeGetBombsFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_get_bombs");
    g_SpellPracticeSetBombsFn =
        (th06::SpellPracticeSetBombsFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_set_bombs");
    g_SpellPracticeGetRankFn =
        (th06::SpellPracticeGetRankFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_get_rank");
    g_SpellPracticeSetRankFn =
        (th06::SpellPracticeSetRankFn)GetProcAddress(g_SpellPracticeDllHandle, "th06_spell_practice_set_rank");

    return g_SpellPracticeDllHandle;
}

static Difficulty g_SpellPracticePreExtraDifficulty = NORMAL;

static i32 TryOpenSpellPracticeDllMenu(i32 stageIndex)
{
    if (EnsureSpellPracticeDllLoaded() == NULL || g_SpellPracticeOpenMenuFn == NULL)
    {
        return SPELL_PRACTICE_OPEN_RESULT_NOT_HANDLED;
    }

    {
        ScoreDat *sd = ResultScreen::OpenScore("score.dat");
        if (sd != NULL)
        {
            ResultScreen::ParseCatk(sd, g_GameManager.catk);
            ResultScreen::ReleaseScoreDat(sd);
        }
    }

    if (stageIndex == 6 && g_GameManager.difficulty != EXTRA)
    {
        g_SpellPracticePreExtraDifficulty = (Difficulty)g_GameManager.difficulty;
        g_GameManager.difficulty = EXTRA;
    }

    LoadSpellPracticeFrontAnm();

    i32 openMenuResult = g_SpellPracticeOpenMenuFn(g_Supervisor.hwndGameWindow, g_GameManager.difficulty,
                                                   g_GameManager.character, g_GameManager.shotType, stageIndex, NULL);

    if (g_SpellPracticeSetLivesFn != NULL)
        g_SpellPracticeSetLivesFn(g_SpellPracticeSavedLives);
    if (g_SpellPracticeSetBombsFn != NULL)
        g_SpellPracticeSetBombsFn(g_SpellPracticeSavedBombs);
    if (g_SpellPracticeSetRankFn != NULL)
        g_SpellPracticeSetRankFn(g_SpellPracticeRank);
    g_IsSpellPracticeDllMenuActive = (openMenuResult == SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING);
    if (!g_IsSpellPracticeDllMenuActive)
    {
        if (stageIndex == 6)
            g_GameManager.difficulty = g_SpellPracticePreExtraDifficulty;
        if (openMenuResult == SPELL_PRACTICE_OPEN_RESULT_NOT_HANDLED)
            UnloadSpellPracticeDll();
    }
    return openMenuResult;
}

static i32 UpdateSpellPracticeDllMenu()
{
    if (!g_IsSpellPracticeDllMenuActive || g_SpellPracticeUpdateMenuFn == NULL)
    {
        return SPELL_PRACTICE_OPEN_RESULT_NOT_HANDLED;
    }
    i32 result = g_SpellPracticeUpdateMenuFn(g_CurFrameInput, g_LastFrameInput, g_IsEigthFrameOfHeldInput);
    if (result != SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING)
    {
        g_IsSpellPracticeDllMenuActive = false;
    }
    return result;
}

static ChainCallbackResult DrawSpellPracticeDllMenu(void *)
{
    if (Supervisor::IsQuickSettingsVisible())
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    if (!g_IsSpellPracticeDllMenuActive || g_SpellPracticeGetDrawDataFn == NULL)
        return CHAIN_CALLBACK_RESULT_CONTINUE;

    SpellPracticeDrawData drawData;
    if (g_SpellPracticeGetDrawDataFn(&drawData, g_GameManager.catk, CATK_NUM_CAPTURES) <= 0)
        return CHAIN_CALLBACK_RESULT_CONTINUE;

    const bool vertScale = (g_VerticalMode != 0);
    const int fontSz = vertScale ? 16 : 12;
    const float posScale = vertScale ? (16.0f / 12.0f) : 1.0f;

    const float vertXShift = vertScale ? -60.0f : 0.0f;

    const int count = drawData.itemCount < 24 ? drawData.itemCount : 24;
    for (int i = 0; i < count; i++)
    {
        AnmVm *vm = &g_SpellPracticeVms[i];
        g_AnmManager->InitializeAndSetSprite(vm, ANM_SCRIPT_TEXT_RESULTSCREEN_CHARACTER_NAME);
        vm->fontWidth = fontSz;
        vm->fontHeight = fontSz;
        vm->flags.anchor = AnmVmAnchor_TopLeft;
        float itemX = drawData.items[i].x;
        if (vertScale && itemX >= 350.0f)
            itemX += vertXShift;
        vm->pos = D3DXVECTOR3(itemX * posScale, drawData.items[i].y * posScale, 0.0f);
        AnmManager::DrawVmTextFmt(g_AnmManager, vm, COLOR_RGB(COLOR_WHITE), COLOR_RGB(COLOR_BLACK),
                                  drawData.items[i].text);
        g_AnmManager->DrawNoRotation(vm);
    }

    i32 lives = (g_SpellPracticeGetLivesFn != NULL) ? g_SpellPracticeGetLivesFn() : g_SpellPracticeSavedLives;
    g_SpellPracticeSavedLives = lives;
    const float iconSpacing = vertScale ? 21.0f : 16.0f;
    const float iconScaleXY = vertScale ? (21.0f / 16.0f) : 1.0f;
    const float livesBaseX = (380.0f + vertXShift) * posScale;
    const float livesY = 120.0f * posScale;
    for (i32 i = 0; i < lives; i++)
    {
        g_SpellPracticeLivesIconVm.pos = D3DXVECTOR3(livesBaseX + i * iconSpacing, livesY, 0.0f);
        g_SpellPracticeLivesIconVm.scaleX = iconScaleXY;
        g_SpellPracticeLivesIconVm.scaleY = iconScaleXY;
        g_AnmManager->DrawNoRotation(&g_SpellPracticeLivesIconVm);
    }

    i32 bombs = (g_SpellPracticeGetBombsFn != NULL) ? g_SpellPracticeGetBombsFn() : g_SpellPracticeSavedBombs;
    g_SpellPracticeSavedBombs = bombs;
    const float bombsY = 138.0f * posScale;
    for (i32 i = 0; i < bombs; i++)
    {
        g_SpellPracticeBombsIconVm.pos = D3DXVECTOR3(livesBaseX + i * iconSpacing, bombsY, 0.0f);
        g_SpellPracticeBombsIconVm.scaleX = iconScaleXY;
        g_SpellPracticeBombsIconVm.scaleY = iconScaleXY;
        g_AnmManager->DrawNoRotation(&g_SpellPracticeBombsIconVm);
    }

    if (g_SpellPracticeGetRankFn != NULL)
        g_SpellPracticeRank = g_SpellPracticeGetRankFn();

    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

static void DrawCollisionRecordWarning()
{
    // 当たり判定表示中
    static char line1_collision[] = "\x93"
                                    "\x96"
                                    "\x82"
                                    "\xbd"
                                    "\x82"
                                    "\xe8"
                                    "\x94"
                                    "\xbb"
                                    "\x92"
                                    "\xe8"
                                    "\x95"
                                    "\x5c"
                                    "\x8e"
                                    "\xa6"
                                    "\x92"
                                    "\x86";
    // エネミーマーカー表示中
    static char line1_marker[] = "\x83"
                                 "\x47"
                                 "\x83"
                                 "\x6c"
                                 "\x83"
                                 "\x7e"
                                 "\x81"
                                 "\x5b"
                                 "\x83"
                                 "\x7d"
                                 "\x81"
                                 "\x5b"
                                 "\x83"
                                 "\x4a"
                                 "\x81"
                                 "\x5b"
                                 "\x95"
                                 "\x5c"
                                 "\x8e"
                                 "\xa6"
                                 "\x92"
                                 "\x86";
    // 当たり判定・マーカー表示中
    static char line1_both[] = "\x93"
                               "\x96"
                               "\x82"
                               "\xbd"
                               "\x82"
                               "\xe8"
                               "\x94"
                               "\xbb"
                               "\x92"
                               "\xe8"
                               "\x81"
                               "\x45"
                               "\x83"
                               "\x7d"
                               "\x81"
                               "\x5b"
                               "\x83"
                               "\x4a"
                               "\x81"
                               "\x5b"
                               "\x95"
                               "\x5c"
                               "\x8e"
                               "\xa6"
                               "\x92"
                               "\x86";
    // スコア・リプレイは保存されません
    static char line2[] = "\x83"
                          "\x58"
                          "\x83"
                          "\x52"
                          "\x83"
                          "\x41"
                          "\x81"
                          "\x45"
                          "\x83"
                          "\x8a"
                          "\x83"
                          "\x76"
                          "\x83"
                          "\x8c"
                          "\x83"
                          "\x43"
                          "\x82"
                          "\xcd"
                          "\x95"
                          "\xdb"
                          "\x91"
                          "\xb6"
                          "\x82"
                          "\xb3"
                          "\x82"
                          "\xea"
                          "\x82"
                          "\xdc"
                          "\x82"
                          "\xb9"
                          "\x82"
                          "\xf1";
    char *line1;
    if ((g_ShowCollision == 1 || g_HitDisp == 1) && g_ShowEnemyMarker == 1)
        line1 = line1_both;
    else if (g_ShowEnemyMarker == 1)
        line1 = line1_marker;
    else
        line1 = line1_collision;

    AnmVm vm;

    g_AnmManager->InitializeAndSetSprite(&vm, ANM_SCRIPT_TEXT_RESULTSCREEN_CHARACTER_NAME);
    vm.fontWidth = 12;
    vm.fontHeight = 12;
    vm.flags.anchor = AnmVmAnchor_TopLeft;
    vm.pos = D3DXVECTOR3(32.0f, 435.0f, 0.0f);
    AnmManager::DrawVmTextFmt(g_AnmManager, &vm, COLOR_RGB(COLOR_LIGHTYELLOW), COLOR_RGB(COLOR_BLACK), line1);
    g_AnmManager->DrawNoRotation(&vm);

    g_AnmManager->InitializeAndSetSprite(&vm, ANM_SCRIPT_TEXT_RESULTSCREEN_CHARACTER_NAME + 1);
    vm.fontWidth = 12;
    vm.fontHeight = 12;
    vm.flags.anchor = AnmVmAnchor_TopLeft;
    vm.pos = D3DXVECTOR3(32.0f, 450.0f, 0.0f);
    AnmManager::DrawVmTextFmt(g_AnmManager, &vm, COLOR_RGB(COLOR_LIGHTYELLOW), COLOR_RGB(COLOR_BLACK), line2);
    g_AnmManager->DrawNoRotation(&vm);
}

MainMenu::MainMenu()
{
}

static bool ReturnToTitleWithQ(MainMenu *menu)
{
    i32 i;

    if (!WAS_PRESSED(TH_BUTTON_Q))
    {
        return false;
    }
    switch (menu->gameState)
    {
    case STATE_DIFFICULTY_SELECT:
    case STATE_CHARACTER_SELECT:
    case STATE_SHOT_SELECT:
    case STATE_PRACTICE_LVL_SELECT:

        if (g_IsSpellPracticeDllMenuActive && (menu->cursor == 6 || g_SpellPracticeSelectedStage == 6))
        {
            g_GameManager.difficulty = g_SpellPracticePreExtraDifficulty;
        }
        CloseSpellPracticeDllMenu();
        g_SpellPracticeStageConfiguring = false;
        g_ReturnToSpellPractice = 0;
        g_ReturnToPractice = 0;

        menu->gameState = STATE_CHARACTER_LOAD;
        menu->stateTimer = 0;
        for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
        {
            menu->vm[i].pendingInterrupt = 4;
        }
        menu->cursor = (g_GameManager.isInPracticeMode == 0) ? 0 : 2;
        g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
        return true;
    case STATE_REPLAY_ANIM:
        if (menu->stateTimer < 0x28)
        {
            return false;
        }

    case STATE_REPLAY_SELECT:
        if (menu->currentReplay != NULL)
        {
            free(menu->currentReplay);
            menu->currentReplay = NULL;
        }
        menu->gameState = STATE_REPLAY_UNLOAD;
        menu->stateTimer = 0;
        for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
        {
            menu->vm[i].pendingInterrupt = 4;
        }
        g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
        return true;
    default:
        return false;
    }
}

static ChainCallbackResult StartGameFromMenu(MainMenu *menu)
{
    g_GameManager.livesRemaining = g_Supervisor.cfg.lifeCount;
    g_GameManager.bombsRemaining = g_Supervisor.cfg.bombCount;
    if (g_GameManager.isInPracticeMode != 0)
    {
        g_GameManager.livesRemaining =
            (g_SpellPracticeGetLivesFn != NULL) ? g_SpellPracticeGetLivesFn() : g_SpellPracticeSavedLives;
        g_GameManager.bombsRemaining =
            (g_SpellPracticeGetBombsFn != NULL) ? g_SpellPracticeGetBombsFn() : g_SpellPracticeSavedBombs;
    }
    else if (g_GameManager.difficulty == EXTRA)
    {
        g_GameManager.livesRemaining = 2;
        g_GameManager.bombsRemaining = 3;
    }
    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
    g_GameManager.isInReplay = 0;
    f32 avgFps = 0.0f;
    if (menu->timeRelatedArrSize >= 2)
    {
        for (i32 i = 0; i < menu->timeRelatedArrSize; i++)
        {
            avgFps += menu->timeRelatedArr[i];
        }
        avgFps /= menu->timeRelatedArrSize;
    }
    else
    {
        avgFps = 60.0f;
    }

    f32 refreshRate;
    if (avgFps >= 155.0f)
        refreshRate = 60.0f / 160.0f;
    else if (avgFps >= 135.0f)
        refreshRate = 60.0f / 150.0f;
    else if (avgFps >= 110.0f)
        refreshRate = 60.0f / 120.0f;
    else if (avgFps >= 95.0f)
        refreshRate = 60.0f / 100.0f;
    else if (avgFps >= 87.5f)
        refreshRate = 60.0f / 90.0f;
    else if (avgFps >= 82.5f)
        refreshRate = 60.0f / 85.0f;
    else if (avgFps >= 77.5f)
        refreshRate = 60.0f / 80.0f;
    else if (avgFps >= 73.5f)
        refreshRate = 60.0f / 75.0f;
    else if (avgFps >= 68.0f)
        refreshRate = 60.0f / 70.0f;
    else
        refreshRate = 1.0f;
    utils::DebugPrint("Reflesh Rate = %f\n", 60.0f / refreshRate);
    g_Supervisor.framerateMultiplier = refreshRate;

    if (g_GameManager.isInPracticeMode == 0 && (g_RealCurFrameInput & TH_BUTTON_FOCUS) != 0)
    {
        g_IsOnline = 1;
        menu->gameState = STATE_ONLINE_SELECT;
        menu->stateTimer = 0;
        menu->cursor = 0;
        strcpy(g_NetConfig.portStr, "10800");
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    g_IsOnline = 0;

    g_Supervisor.StopAudio();
    g_Supervisor.curState = SUPERVISOR_STATE_GAMEMANAGER;
    return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
}

static ChainCallbackResult StartDemoReplay()
{
    g_GameManager.isInReplay = 1;
    g_GameManager.demoMode = 1;
    g_GameManager.demoFrames = 0;
    g_Supervisor.framerateMultiplier = 1.0;
    strcpy(g_GameManager.replayFile, "data/demo/demo00.rpy");
    g_GameManager.currentStage = 3;
    g_GameManager.difficulty = LUNATIC;
    g_Supervisor.curState = SUPERVISOR_STATE_GAMEMANAGER;
    return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
}

ChainCallbackResult MainMenu::OnUpdate(MainMenu *menu)
{
    i32 i;
    AnmVm *vmList;
    DWORD time;
    i32 deltaTime;
    f32 deltaTimeAsFrames;
    f32 deltaTimeAsMs;
    i16 mapping;
    ZunResult startedUp;
    i16 sVar1;
    u8 *controllerData;
    ControllerMapping mappingData;
    f32 refreshRate;
    f32 local_48;
    i32 local_4c;
    u32 chosenStage;
    D3DXVECTOR3 pos1;
    D3DXVECTOR3 pos2;
    D3DXVECTOR3 pos3;
    D3DXVECTOR3 pos4;
    D3DXVECTOR3 pos5;
    AnmVm *vm;
    u32 hasLoadedSprite;
    char localIp[16];
    char formattedIp[16];
    i32 portIdx;
    ZunBool wasHost;
    HandshakeData localHandshake;
    HandshakeData remoteHandshake;

    if (menu->timeRelatedArrSize < ARRAY_SIZE_SIGNED(menu->timeRelatedArr))
    {
        timeBeginPeriod(1);
        if (menu->lastFrameTime == 0)
        {
            menu->lastFrameTime = timeGetTime();
        }
        time = timeGetTime();
        timeEndPeriod(1);
        menu->frameCountForRefreshRateCalc = menu->frameCountForRefreshRateCalc + 1;
        deltaTime = time - menu->lastFrameTime;
        if (deltaTime >= 700)
        {
            menu->lastFrameTime = time;
            menu->frameCountForRefreshRateCalc = 0;
        }
        else
        {
            if (deltaTime >= 500)
            {
                deltaTimeAsMs = deltaTime / 1000.f;
                deltaTimeAsFrames = menu->frameCountForRefreshRateCalc * 1000.f / deltaTime;
                if (deltaTimeAsFrames >= 57.f)
                {
                    menu->timeRelatedArr[menu->timeRelatedArrSize] = deltaTimeAsFrames;
                    menu->timeRelatedArrSize = menu->timeRelatedArrSize + 1;
                }
                menu->lastFrameTime = time;
                menu->frameCountForRefreshRateCalc = 0;
            }
        }
    }
    ReturnToTitleWithQ(menu);
    switch (menu->gameState)
    {
    case STATE_STARTUP:
        startedUp = menu->BeginStartup();
        if (startedUp == ZUN_ERROR)
        {
            return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
        }
        if (menu->gameState != STATE_PRE_INPUT)
            break;
    case STATE_PRE_INPUT:
        if (!Supervisor::IsQuickSettingsVisible())
        {
            menu->idleFrames++;
        }
        if ((g_CurFrameInput & 0xffff) != 0)
        {
            menu->idleFrames = 0;
        }
        if (menu->idleFrames >= 720)
        {
            return StartDemoReplay();
        }
        if (menu->WeirdSecondInputCheck())
            break;
        menu->idleFrames = 0;
    case STATE_MAIN_MENU:
        menu->DrawStartMenu();
        if ((g_CurFrameInput & 0xffff) != 0)
        {
            menu->idleFrames = 0;
        }

        if (!Supervisor::IsQuickSettingsVisible())
        {
            menu->idleFrames++;
        }
        if (menu->idleFrames >= 720)
        {
            return StartDemoReplay();
        }
        break;
    case STATE_REPLAY_LOAD:
    case STATE_REPLAY_ANIM:
    case STATE_REPLAY_UNLOAD:
    case STATE_REPLAY_SELECT:
        if (menu->ReplayHandling() != 0)
        {
            return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
        }
        break;
    case STATE_OPTIONS:
        if (menu->OnUpdateOptionsMenu() != 0)
        {
            return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
        }
        break;
    case STATE_KEYCONFIG:
        MoveCursor(menu, 11);
        vmList = &menu->vm[34];
        for (i = 0; i < 11; i++, vmList++)
        {
            DrawMenuItem(vmList, i, menu->cursor, menu->color2, menu->color1, 0x73);
        }
        for (i = 0; i < 9; i++, vmList++)
        {
            if (menu->controlMapping[i] < 0)
            {
                vmList->flags.flag1 = 0;
                continue;
            }
            vmList->flags.flag1 = 1;
            DrawMenuItem(vmList, i, menu->cursor, menu->color2, menu->color1, 0x73);
        }
        for (i = 0; i < 18; i++, vmList++)
        {
            if (menu->controlMapping[i / 2] < 0)
            {
                vmList->flags.flag1 = 0;
                continue;
            }
            vmList->flags.flag1 = 1;
            mapping = menu->controlMapping[i / 2];
            if (i % 2 == 0)
            {
                g_AnmManager->SetActiveSprite(vmList, mapping / 10 + ANM_SPRITE_TITLE01_START);
            }
            else
            {
                g_AnmManager->SetActiveSprite(vmList, mapping % 10 + ANM_SPRITE_TITLE01_START);
            }
            vmList->baseSpriteIndex = vmList->activeSpriteIndex;
            DrawMenuItem(vmList, i / 2, menu->cursor, menu->color2, menu->color1, 0x7a);
        }
        if (menu->stateTimer >= 32)
        {
            controllerData = Controller::GetControllerState();
            for (sVar1 = 0; sVar1 < 32; sVar1++)
            {
                if ((controllerData[sVar1] & 0x80) != 0)
                    break;
            }
            if (sVar1 < 32 && g_LastJoystickInput != sVar1)
            {
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                switch (menu->cursor)
                {
                case 0:
                    SwapMapping(menu, sVar1, menu->controlMapping[0], 1);
                    menu->controlMapping[0] = sVar1;
                    break;
                case 1:
                    SwapMapping(menu, sVar1, menu->controlMapping[1], 0);
                    menu->controlMapping[1] = sVar1;
                    break;
                case 2:
                    SwapMapping(menu, sVar1, menu->controlMapping[2], 1);
                    menu->controlMapping[2] = sVar1;
                    break;
                case 3:
                    SwapMapping(menu, sVar1, menu->controlMapping[3], 0);
                    menu->controlMapping[3] = sVar1;
                    break;
                case 4:
                    SwapMapping(menu, sVar1, menu->controlMapping[4], 0);
                    menu->controlMapping[4] = sVar1;
                    break;
                case 5:
                    SwapMapping(menu, sVar1, menu->controlMapping[5], 0);
                    menu->controlMapping[5] = sVar1;
                    break;
                case 6:
                    SwapMapping(menu, sVar1, menu->controlMapping[6], 0);
                    menu->controlMapping[6] = sVar1;
                    break;
                case 7:
                    SwapMapping(menu, sVar1, menu->controlMapping[7], 0);
                    menu->controlMapping[7] = sVar1;
                    break;
                case 8:
                    SwapMapping(menu, sVar1, menu->controlMapping[8], 0);
                    menu->controlMapping[8] = sVar1;
                }
            }
            g_LastJoystickInput = sVar1;
            if (WAS_PRESSED(TH_BUTTON_BOMB) && menu->cursor == 7)
            {
                menu->gameState = STATE_QUIT;
                menu->stateTimer = 0;
                for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
                    menu->vm[i].pendingInterrupt = 4;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
            if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
            {
                switch (menu->cursor)
                {
                case 9:
                    mappingData.shootButton = 0;
                    mappingData.bombButton = 1;
                    mappingData.focusButton = 0;
                    mappingData.menuButton = 0xffff;
                    mappingData.upButton = 0xffff;
                    mappingData.downButton = 0xffff;
                    mappingData.leftButton = 0xffff;
                    mappingData.rightButton = 0xffff;
                    mappingData.skipButton = 0xffff;
                    memcpy(menu->controlMapping, &mappingData, sizeof(ControllerMapping));
                    break;
                case 10:
                    menu->gameState = STATE_OPTIONS;
                    menu->stateTimer = 0;
                    for (sVar1 = 0; sVar1 < ARRAY_SIZE_SIGNED(menu->vm); sVar1++)
                    {
                        menu->vm[sVar1].pendingInterrupt = 3;
                    }
                    menu->cursor = 7;
                    g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                    memcpy(&g_ControllerMapping, menu->controlMapping, sizeof(ControllerMapping));
                    memcpy(&g_Supervisor.cfg.controllerMapping, menu->controlMapping, sizeof(ControllerMapping));
                    break;
                }
            }
        }
        break;
    case STATE_DIFFICULTY_LOAD:
        if (menu->stateTimer == 60)
        {
            if (LoadDiffCharSelect(menu) != ZUN_SUCCESS)
            {
                g_GameErrorContext.Log(TH_ERR_MAINMENU_LOAD_SELECT_SCREEN_FAILED);
                g_Supervisor.curState = SUPERVISOR_STATE_EXITSUCCESS;
                return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
            }
            if (g_ReturnToSpellPractice || g_ReturnToPractice)
            {
                g_ReturnToPractice = 0;
                g_GameManager.isInPracticeMode = 1;
                menu->gameState = STATE_PRACTICE_LVL_SELECT;
                menu->stateTimer = 0;
                LoadSpellPracticeFrontAnm();
                for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
                    menu->vm[i].pendingInterrupt = 19;
                vmList = &menu->vm[81 + g_GameManager.difficulty];
                vmList->pendingInterrupt = 0;
                vmList = &menu->vm[86];
                for (i = 0; i < 2; i++, vmList += 2)
                    if (i != g_GameManager.character)
                    {
                        vmList[0].pendingInterrupt = 0;
                        vmList[1].pendingInterrupt = 0;
                    }
                vmList = &menu->vm[92];
                for (i = 0; i < 2; i++, vmList += 2)
                    if (i != g_GameManager.character)
                    {
                        vmList[0].pendingInterrupt = 0;
                        vmList[1].pendingInterrupt = 0;
                    }
                menu->cursor = g_GameManager.menuCursorBackup;
                break;
            }
            menu->gameState = STATE_DIFFICULTY_SELECT;
            menu->minimumOpacity = 0;
            menu->framesInactive = menu->framesActive;
            menu->framesActive = 0;
            if (g_GameManager.difficulty < 4)
            {
                for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
                {
                    menu->vm[i].pendingInterrupt = 6;
                }
                menu->cursor = g_Supervisor.cfg.defaultDifficulty;
            }
            else
            {
                for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
                {
                    menu->vm[i].pendingInterrupt = 18;
                }
                menu->cursor = 0;
            }
        }
        else
        {
            break;
        }
    case STATE_CHARACTER_LOAD:
        if (menu->stateTimer == 36)
        {
            menu->gameState = STATE_STARTUP;
            menu->stateTimer = 0;
        }
        break;
    case STATE_DIFFICULTY_SELECT:
        vmList = &menu->vm[81];
        if (g_GameManager.difficulty < 4)
        {
            MoveCursor(menu, 4);
            for (i = 0; i < 4; i++, vmList++)
            {
                if (i != menu->cursor)
                {
                    if (((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1) == 0)
                    {
                        vmList->color = 0x60000000;
                    }
                    else
                    {
                        vmList->color = 0x60ffffff;
                    }
                    pos1.x = 0.0;
                    pos1.y = 0.0;
                    pos1.z = 0.0;
                    memcpy(vmList->posOffset, &pos1, sizeof(D3DXVECTOR3));
                    vmList->alphaInterpEndTime = 0;
                }
                else
                {
                    if (((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1) == 0)
                    {
                        vmList->color = COLOR_BLACK;
                    }
                    else
                    {
                        vmList->color = COLOR_WHITE;
                    }
                    pos2.x = -6.0f;
                    pos2.y = -6.0f;
                    pos2.z = 0.0;
                    memcpy(vmList->posOffset, &pos2, sizeof(D3DXVECTOR3));
                }
            }
            vmList->flags.flag1 = 0;
        }
        else
        {
            for (i = 0; i < 4; i++, vmList++)
            {
                vmList->flags.flag1 = 0;
            }
            for (i = 4; i < 5; i++, vmList++)
            {
                if (((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1) == 0)
                {
                    vmList->color = COLOR_BLACK;
                }
                else
                {
                    vmList->color = COLOR_WHITE;
                }
                pos3.x = -6.0f;
                pos3.y = -6.0f;
                pos3.z = 0.0;
                memcpy(vmList->posOffset, &pos3, sizeof(D3DXVECTOR3));
            }
        }
        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {
            menu->gameState = STATE_CHARACTER_LOAD;
            menu->stateTimer = 0;
            for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
            {
                menu->vm[i].pendingInterrupt = 4;
            }
            g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            if (g_GameManager.difficulty < 4)
            {
                g_Supervisor.cfg.defaultDifficulty = menu->cursor;
                if (g_GameManager.isInPracticeMode == 0)
                {
                    menu->cursor = 0;
                }
                else
                {
                    menu->cursor = 2;
                }
            }
            else
            {
                menu->cursor = 1;
            }
            break;
        }
        else if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
        {
            menu->gameState = STATE_CHARACTER_SELECT;
            menu->stateTimer = 0;
            for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
            {
                menu->vm[i].pendingInterrupt = 7;
            }
            g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
            if (g_GameManager.difficulty < 4)
            {
                vmList = &menu->vm[81 + menu->cursor];
                vmList->pendingInterrupt = 8;
                g_GameManager.difficulty = (Difficulty)menu->cursor;
                menu->cursor = g_GameManager.character;
            }
            else
            {
                vmList = &menu->vm[85];
                vmList->pendingInterrupt = 8;
                g_GameManager.difficulty = EXTRA;
                if (g_GameManager.HasReachedMaxClears(g_GameManager.character, 0) ||
                    g_GameManager.HasReachedMaxClears(g_GameManager.character, 1))
                {
                    menu->cursor = g_GameManager.character;
                }
                else
                {
                    menu->cursor = 1 - g_GameManager.character;
                }
            }
            g_Supervisor.cfg.defaultDifficulty = g_GameManager.difficulty;
            vmList = &menu->vm[86];
            for (i = 0; i < 2; i++, vmList += 2)
            {
                if (i != menu->cursor)
                {
                    vmList[0].pendingInterrupt = 0;
                    vmList[1].pendingInterrupt = 0;
                }
            }
            break;
        }
        break;
    case STATE_CHARACTER_SELECT:
        if (menu->stateTimer < 30)
            break;
        if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_LEFT))
        {
            menu->cursor = menu->cursor + 1;
            if (menu->cursor >= 2)
            {
                menu->cursor = menu->cursor - 2;
            }
            if (g_GameManager.difficulty == EXTRA && g_GameManager.HasReachedMaxClears(menu->cursor, 0) == 0 &&
                g_GameManager.HasReachedMaxClears(menu->cursor, 1) == 0)
            {
                menu->cursor = menu->cursor - 1;
                if (menu->cursor < 0)
                {
                    menu->cursor = menu->cursor + 2;
                }
            }
            else
            {
                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                vmList = &menu->vm[86];
                for (i = 0; i < 2; i++, vmList++)
                {
                    if (i == menu->cursor)
                    {
                        vmList->pendingInterrupt = 9;
                        vmList++;
                        vmList->pendingInterrupt = 9;
                    }
                    else
                    {
                        vmList->pendingInterrupt = 12;
                        vmList++;
                        vmList->pendingInterrupt = 12;
                    }
                }
            }
        }
        else if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_RIGHT))
        {
            menu->cursor = menu->cursor - 1;
            if (menu->cursor < 0)
            {
                menu->cursor = menu->cursor + 2;
            }
            if (g_GameManager.difficulty == EXTRA && g_GameManager.HasReachedMaxClears(menu->cursor, 0) == 0 &&
                g_GameManager.HasReachedMaxClears(menu->cursor, 1) == 0)
            {
                menu->cursor = menu->cursor + 1;
                if (menu->cursor >= 2)
                {
                    menu->cursor = menu->cursor - 2;
                }
            }
            else
            {
                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                vmList = &menu->vm[86];
                for (i = 0; i < 2; i++, vmList++)
                {
                    if (i == menu->cursor)
                    {
                        vmList->pendingInterrupt = 10;
                        vmList++;
                        vmList->pendingInterrupt = 10;
                    }
                    else
                    {
                        vmList->pendingInterrupt = 11;
                        vmList++;
                        vmList->pendingInterrupt = 11;
                    }
                }
            }
        }
        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {
            menu->gameState = STATE_DIFFICULTY_SELECT;
            menu->stateTimer = 0;
            if (g_GameManager.difficulty < 4)
            {
                for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
                {
                    menu->vm[i].pendingInterrupt = 6;
                }
                menu->cursor = g_Supervisor.cfg.defaultDifficulty;
            }
            else
            {
                for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
                {
                    menu->vm[i].pendingInterrupt = 18;
                }
                menu->cursor = 0;
            }
            g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            break;
        }
        if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
        {
            menu->gameState = STATE_SHOT_SELECT;
            menu->stateTimer = 0;
            for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
            {
                menu->vm[i].pendingInterrupt = 13;
            }
            vmList = &menu->vm[g_GameManager.difficulty + 81];
            vmList->pendingInterrupt = 0;
            vmList = &menu->vm[86];
            for (i = 0; i < 2; i++, vmList += 2)
            {
                if (i != menu->cursor)
                {
                    vmList[0].pendingInterrupt = 0;
                    vmList[1].pendingInterrupt = 0;
                }
            }
            vmList = &menu->vm[92];
            for (i = 0; i < 2; i++, vmList += 2)
            {
                if (i != menu->cursor)
                {
                    vmList[0].pendingInterrupt = 0;
                    vmList[1].pendingInterrupt = 0;
                }
            }
            g_GameManager.character = menu->cursor;
            if (g_GameManager.difficulty < 4)
            {
                menu->cursor = g_GameManager.shotType;
            }
            else
            {
                if (g_GameManager.HasReachedMaxClears(g_GameManager.character, g_GameManager.shotType) != 0)
                {
                    menu->cursor = g_GameManager.shotType;
                }
                else
                {
                    menu->cursor = 1 - g_GameManager.shotType;
                }
            }
            g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
        }
        break;
    case STATE_SHOT_SELECT:
        MoveCursor(menu, 2);
        if (g_GameManager.difficulty == EXTRA &&
            g_GameManager.HasReachedMaxClears(g_GameManager.character, menu->cursor) == 0)
        {
            menu->cursor = 1 - menu->cursor;
        }
        vmList = &menu->vm[92];
        for (i = 0; i < 2; i++, vmList += 2)
        {
            vmList[1].flags.colorOp = AnmVmColorOp_Add;
        }
        vmList = &menu->vm[92 + g_GameManager.character * 2];
        for (i = 0; i < 2; i++, vmList++)
        {
            vmList->flags.colorOp = AnmVmColorOp_Add;
            vmList->flags.isVisible = 1;
            if (i != menu->cursor)
            {
                if (((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1) == 0)
                {
                    vmList->color = 0xa0000000;
                }
                else
                {
                    vmList->color = 0xa0d0d0d0;
                }
                pos4.x = 0.0;
                pos4.y = 0.0;
                pos4.z = 0.0;
                memcpy(&vmList->posOffset, &pos4, sizeof(D3DXVECTOR3));
            }
            else
            {
                if (((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1) == 0)
                {
                    vmList->color = 0xff202020;
                }
                else
                {
                    vmList->color = 0xffffffff;
                }
                pos5.x = -6.f;
                pos5.y = -6.f;
                pos5.z = 0.0;
                memcpy(&vmList->posOffset, &pos5, sizeof(D3DXVECTOR3));
            }
        }
        if (30 > menu->stateTimer)
        {
            break;
        }
        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {
            menu->gameState = STATE_CHARACTER_SELECT;
            menu->stateTimer = 0;
            for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
            {
                menu->vm[i].pendingInterrupt = 7;
            }
            vmList = &menu->vm[92];
            for (i = 0; i < 2; i++, vmList += 2)
            {
                if (i != g_GameManager.character)
                {
                    vmList[0].pendingInterrupt = 0;
                    vmList[1].pendingInterrupt = 0;
                }
            }
            vmList = &menu->vm[81 + g_GameManager.difficulty];
            vmList->pendingInterrupt = 0;
            g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            g_GameManager.shotType = menu->cursor;
            menu->cursor = g_GameManager.character;
            vmList = &menu->vm[86];
            for (i = 0; i < 2; i++, vmList += 2)
            {
                if (i != menu->cursor)
                {
                    vmList[0].pendingInterrupt = 0;
                    vmList[1].pendingInterrupt = 0;
                }
            }
            break;
        }
        else if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
        {
            g_GameManager.shotType = menu->cursor;
            if (g_GameManager.isInPracticeMode == 0)
            {
                if (g_GameManager.difficulty < 4)
                {
                    g_GameManager.currentStage = 0;
                }
                else
                {
                    g_GameManager.currentStage = 6;
                }
                return StartGameFromMenu(menu);
            }
            menu->gameState = STATE_PRACTICE_LVL_SELECT;
            menu->stateTimer = 0;
            LoadSpellPracticeFrontAnm();
            for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
            {
                menu->vm[i].pendingInterrupt = 19;
            }
            vmList = &menu->vm[81 + g_GameManager.difficulty];
            vmList->pendingInterrupt = 0;
            vmList = &menu->vm[86];
            for (i = 0; i < 2; i++, vmList += 2)
            {
                if (i != g_GameManager.character)
                {
                    vmList[0].pendingInterrupt = 0;
                    vmList[1].pendingInterrupt = 0;
                }
            }
            vmList = &menu->vm[92];
            for (i = 0; i < 2; i++, vmList += 2)
            {
                if (i != g_GameManager.character)
                {
                    vmList[0].pendingInterrupt = 0;
                    vmList[1].pendingInterrupt = 0;
                }
            }
            menu->cursor = g_GameManager.menuCursorBackup;
            local_4c = g_GameManager.clrd[g_GameManager.CharacterShotType()]
                                   .difficultyClearedWithoutRetries[g_GameManager.difficulty] > 6
                           ? 6
                           : g_GameManager.clrd[g_GameManager.CharacterShotType()]
                                 .difficultyClearedWithoutRetries[g_GameManager.difficulty];
            if (g_GameManager.difficulty == EASY && local_4c == 6)
            {
                local_4c = 5;
            }
            if (g_GameManager.clrd[g_GameManager.CharacterShotType()].difficultyClearedWithoutRetries[EXTRA] == 99)
            {
                local_4c = 7;
            }
            if (menu->cursor >= local_4c)
            {
                menu->cursor = 0;
            }
        }
        break;
    case STATE_PRACTICE_LVL_SELECT:
        if (g_ReturnToSpellPractice && !g_IsSpellPracticeDllMenuActive)
        {
            g_ReturnToSpellPractice = 0;
            i32 autoOpenResult = TryOpenSpellPracticeDllMenu(menu->cursor);
            if (autoOpenResult == SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING)
            {
                if (g_SpellPracticeSetCursorToSpellFn != NULL && g_LastSpellPracticeIndex >= 0)
                    g_SpellPracticeSetCursorToSpellFn(g_LastSpellPracticeIndex);
                break;
            }
        }
        if (g_IsSpellPracticeDllMenuActive)
        {
            i32 dllMenuResult = UpdateSpellPracticeDllMenu();
            if (dllMenuResult == SPELL_PRACTICE_OPEN_RESULT_START_GAME)
            {
                g_GameManager.currentStage = menu->cursor;
                g_GameManager.menuCursorBackup = menu->cursor;
                g_SpellPracticeSelectedSpellIndex =
                    (g_SpellPracticeGetSelectedSpellFn != NULL) ? g_SpellPracticeGetSelectedSpellFn() : -1;

                g_IsSpellPracticeGame =
                    (g_SpellPracticeSelectedSpellIndex >= 0) ? g_SpellPracticeSelectedSpellIndex : 0;
                if (g_SpellPracticeSelectedSpellIndex >= 0)
                {
                    g_LastSpellPracticeIndex = g_SpellPracticeSelectedSpellIndex;
                    if (g_SpellPracticeGetSelectedParamsFn != NULL)
                    {
                        g_SpellPracticeGetSelectedParamsFn(&g_SpellPracticeMode, &g_SpellPracticeSection);
                    }
                    else
                    {
                        g_SpellPracticeMode = 2;
                        g_SpellPracticeSection = 1;
                    }
                }
                g_GameManager.isInPracticeMode = 1;
                return StartGameFromMenu(menu);
            }
            if (dllMenuResult == SPELL_PRACTICE_OPEN_RESULT_STAY_IN_MENU)
            {
                if (menu->cursor == 6)
                    g_GameManager.difficulty = g_SpellPracticePreExtraDifficulty;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
            break;
        }

        chosenStage = g_GameManager.clrd[g_GameManager.CharacterShotType()]
                                  .difficultyClearedWithoutRetries[g_GameManager.difficulty] > 6
                          ? 6
                          : g_GameManager.clrd[g_GameManager.CharacterShotType()]
                                .difficultyClearedWithoutRetries[g_GameManager.difficulty];
        if (g_GameManager.difficulty == EASY && chosenStage == 6)
        {
            chosenStage = 5;
        }
        if (g_GameManager.clrd[g_GameManager.CharacterShotType()].difficultyClearedWithoutRetries[EXTRA] == 99)
        {
            chosenStage = 7;
        }
        if (g_SpellPracticeStageConfiguring)
        {

            if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_UP))
            {
                if (g_SpellPracticeConfigCursor > 0)
                    g_SpellPracticeConfigCursor--;
            }
            if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_DOWN))
            {
                if (g_SpellPracticeConfigCursor < 2)
                    g_SpellPracticeConfigCursor++;
            }
            if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_LEFT))
            {
                if (g_SpellPracticeConfigCursor == 0)
                {
                    if (g_SpellPracticeSavedLives > 0)
                        g_SpellPracticeSavedLives--;
                }
                else if (g_SpellPracticeConfigCursor == 1)
                {
                    if (g_SpellPracticeSavedBombs > 0)
                        g_SpellPracticeSavedBombs--;
                }
                else if (g_SpellPracticeConfigCursor == 2)
                {
                    if (g_SpellPracticeRank > 1)
                        g_SpellPracticeRank--;
                }
            }
            if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_RIGHT))
            {
                if (g_SpellPracticeConfigCursor == 0)
                {
                    i32 maxLives = (g_GameManager.difficulty == EXTRA) ? 4 : 8;
                    if (g_SpellPracticeSavedLives < maxLives)
                        g_SpellPracticeSavedLives++;
                }
                else if (g_SpellPracticeConfigCursor == 1)
                {
                    i32 maxBombs = (g_GameManager.difficulty == EXTRA) ? 3 : 8;
                    if (g_SpellPracticeSavedBombs < maxBombs)
                        g_SpellPracticeSavedBombs++;
                }
                else if (g_SpellPracticeConfigCursor == 2)
                {
                    if (g_SpellPracticeRank < 32)
                        g_SpellPracticeRank++;
                }
            }
            if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
            {

                g_SpellPracticeStageConfiguring = false;
                menu->cursor = g_SpellPracticeSelectedStage;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
            else if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
            {
                i32 spellPracticeResult = (g_CurFrameInput & TH_BUTTON_FOCUS)
                                              ? TryOpenSpellPracticeDllMenu(g_SpellPracticeSelectedStage)
                                              : SPELL_PRACTICE_OPEN_RESULT_NOT_HANDLED;
                if (spellPracticeResult == SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING)
                {
                    g_SpellPracticeStageConfiguring = false;
                    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                    break;
                }
                if (spellPracticeResult == SPELL_PRACTICE_OPEN_RESULT_STAY_IN_MENU)
                {
                    g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                    break;
                }
                g_GameManager.currentStage = g_SpellPracticeSelectedStage;
                g_GameManager.menuCursorBackup = g_SpellPracticeSelectedStage;
                g_SpellPracticeStageConfiguring = false;
                return StartGameFromMenu(menu);
            }
        }
        else
        {
            MoveCursor(menu, chosenStage);
            if (menu->stateTimer < 30)
            {
                break;
            }
            if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
            {
                menu->gameState = STATE_SHOT_SELECT;
                menu->stateTimer = 0;
                for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
                {
                    menu->vm[i].pendingInterrupt = 13;
                }
                vmList = &menu->vm[81 + g_GameManager.difficulty];
                vmList->pendingInterrupt = 0;
                vmList = &menu->vm[86];
                for (i = 0; i < 2; i++, vmList += 2)
                {
                    if (i != g_GameManager.character)
                    {
                        vmList[0].pendingInterrupt = 0;
                        vmList[1].pendingInterrupt = 0;
                    }
                }
                vmList = &menu->vm[92];
                for (i = 0; i < 2; i++, vmList += 2)
                {
                    if (i != g_GameManager.character)
                    {
                        vmList[0].pendingInterrupt = 0;
                        vmList[1].pendingInterrupt = 0;
                    }
                }
                menu->cursor = g_GameManager.shotType;
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                UnloadSpellPracticeFrontAnm();
                g_SpellPracticeStageConfiguring = false;
                break;
            }
            else if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
            {
                if (g_CurFrameInput & TH_BUTTON_FOCUS)
                {

                    i32 spellPracticeResult = TryOpenSpellPracticeDllMenu(menu->cursor);
                    if (spellPracticeResult == SPELL_PRACTICE_OPEN_RESULT_MENU_RUNNING)
                    {
                        g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                    }
                    else if (spellPracticeResult == SPELL_PRACTICE_OPEN_RESULT_STAY_IN_MENU)
                    {
                        g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                    }
                }
                else
                {

                    g_SpellPracticeSelectedStage = menu->cursor;
                    g_SpellPracticeStageConfiguring = true;
                    g_SpellPracticeConfigCursor = 0;
                    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                }
            }
        }
        break;
    case STATE_QUIT:
        if (menu->stateTimer >= 60)
        {
            g_Supervisor.curState = SUPERVISOR_STATE_EXITSUCCESS;
            return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
        }
        break;
    case STATE_SCORE:
        if (menu->stateTimer >= 60)
        {

            g_SkipTitleIntro = true;
            g_Supervisor.curState = SUPERVISOR_STATE_RESULTSCREEN;
            return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
        }
        break;
    case STATE_MUSIC_ROOM:
        if (menu->stateTimer >= 60)
        {
            g_Supervisor.curState = SUPERVISOR_STATE_MUSICROOM;
            return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
        }
        break;
    case STATE_ONLINE_SELECT: {

        if (WAS_PRESSED(TH_BUTTON_UP) || WAS_PRESSED(TH_BUTTON_DOWN))
        {
            menu->cursor = 1 - menu->cursor;
            g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
        }
        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {

            g_IsOnline = 0;
            menu->gameState = STATE_SHOT_SELECT;
            menu->stateTimer = 0;
            break;
        }
        if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
        {
            g_NetConfig.isHost = (menu->cursor == 0);
            if (g_NetConfig.isHost)
            {
                g_NetConfig.cursor = 16;
            }
            else
            {
                g_NetConfig.cursor = 0;
            }
            menu->gameState = STATE_IP_INPUT;
            menu->stateTimer = 0;
            g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
        }
    }
    break;
    case STATE_IP_INPUT: {
        if (g_NetConfig.cursor <= 14)
        {
            if (g_NetConfig.ip[g_NetConfig.cursor] != '.')
            {
                if (WAS_PRESSED(TH_BUTTON_UP))
                {
                    g_NetConfig.ip[g_NetConfig.cursor]++;
                    if (g_NetConfig.ip[g_NetConfig.cursor] > '9')
                        g_NetConfig.ip[g_NetConfig.cursor] = '0';
                }
                if (WAS_PRESSED(TH_BUTTON_DOWN))
                {
                    g_NetConfig.ip[g_NetConfig.cursor]--;
                    if (g_NetConfig.ip[g_NetConfig.cursor] < '0')
                        g_NetConfig.ip[g_NetConfig.cursor] = '9';
                }
            }
        }
        else if (g_NetConfig.cursor >= 16)
        {
            portIdx = g_NetConfig.cursor - 16;
            if (WAS_PRESSED(TH_BUTTON_UP))
            {
                g_NetConfig.portStr[portIdx]++;
                if (g_NetConfig.portStr[portIdx] > '9')
                    g_NetConfig.portStr[portIdx] = '0';
            }
            if (WAS_PRESSED(TH_BUTTON_DOWN))
            {
                g_NetConfig.portStr[portIdx]--;
                if (g_NetConfig.portStr[portIdx] < '0')
                    g_NetConfig.portStr[portIdx] = '9';
            }
        }

        if (WAS_PRESSED(TH_BUTTON_RIGHT))
        {
            g_NetConfig.cursor++;
            if (g_NetConfig.isHost)
            {
                if (g_NetConfig.cursor > 20)
                    g_NetConfig.cursor = 16;
            }
            else
            {
                if (g_NetConfig.cursor > 20)
                    g_NetConfig.cursor = 0;
                while (g_NetConfig.cursor <= 14 && g_NetConfig.ip[g_NetConfig.cursor] == '.')
                    g_NetConfig.cursor++;
                if (g_NetConfig.cursor == 15)
                    g_NetConfig.cursor = 16;
            }
        }
        if (WAS_PRESSED(TH_BUTTON_LEFT))
        {
            g_NetConfig.cursor--;
            if (g_NetConfig.isHost)
            {
                if (g_NetConfig.cursor < 16)
                    g_NetConfig.cursor = 20;
            }
            else
            {
                if (g_NetConfig.cursor < 0)
                    g_NetConfig.cursor = 20;
                if (g_NetConfig.cursor == 15)
                    g_NetConfig.cursor = 14;
                while (g_NetConfig.cursor >= 0 && g_NetConfig.cursor <= 14 && g_NetConfig.ip[g_NetConfig.cursor] == '.')
                    g_NetConfig.cursor--;
            }
        }

        if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
        {
            menu->gameState = STATE_MATCHING;
            menu->stateTimer = 0;
            if (!g_NetConfig.isHost)
            {
                FormatIpAddress3Digits(g_NetConfig.ip, formattedIp, sizeof(formattedIp));
                strcpy(g_NetConfig.ip, formattedIp);
            }
            g_NetConfig.port = atoi(g_NetConfig.portStr);
        }
        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {
            menu->gameState = STATE_ONLINE_SELECT;
            menu->stateTimer = 0;
            menu->cursor = g_NetConfig.isHost ? 0 : 1;
        }
    }
    break;
    case STATE_MATCHING: {
        static i32 matchingSubState = 0;
        static i32 attemptTimer = 0;

        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {
            if (g_NetHostPrepareStatus == NET_HOST_PREPARE_RUNNING)
            {
                InterlockedExchange(&g_NetHostPrepareCancel, 1);
            }
            else
            {
                g_NetworkManager.Cleanup(false);
                InterlockedExchange(&g_NetHostPrepareStatus, NET_HOST_PREPARE_IDLE);
            }
            matchingSubState = 0;
            attemptTimer = 0;
            menu->gameState = STATE_IP_INPUT;
            menu->cursor = g_NetConfig.isHost ? 16 : 0;
            menu->stateTimer = 0;
            break;
        }

        if (menu->stateTimer == 0)
        {
            g_NetworkManager.Initialize();
            if (g_NetConfig.isHost)
            {
                if (g_NetHostPrepareStatus == NET_HOST_PREPARE_IDLE ||
                    g_NetHostPrepareStatus == NET_HOST_PREPARE_FAILED ||
                    g_NetHostPrepareStatus == NET_HOST_PREPARE_CANCELLED)
                {
                    StartNetHostPrepareThread();
                }
                matchingSubState = 1;
            }
            else
            {
                matchingSubState = 0;
            }
            attemptTimer = 0;
        }

        if (g_NetConfig.isHost && matchingSubState == 0)
        {
            StartNetHostPrepareThread();
            matchingSubState = 1;
        }
        else if (g_NetConfig.isHost && matchingSubState == 1)
        {
            if (g_NetHostPrepareStatus == NET_HOST_PREPARE_READY ||
                g_NetHostPrepareStatus == NET_HOST_PREPARE_READY_NO_UPNP)
            {
                matchingSubState = 2;
            }
            else if (g_NetHostPrepareStatus == NET_HOST_PREPARE_FAILED ||
                     g_NetHostPrepareStatus == NET_HOST_PREPARE_CANCELLED)
            {
                InterlockedExchange(&g_NetHostPrepareStatus, NET_HOST_PREPARE_IDLE);
                attemptTimer++;
                if (attemptTimer % 60 == 0)
                {
                    StartNetHostPrepareThread();
                }
            }
        }

        if (matchingSubState == 0)
        {
            attemptTimer++;
            if (attemptTimer % 30 == 0)
            {
                if (g_NetworkManager.BeginConnect(g_NetConfig.ip, g_NetConfig.port) == ZUN_SUCCESS)
                {
                    matchingSubState = 4;
                    attemptTimer = 0;
                }
            }
        }
        else if (matchingSubState == 4)
        {
            attemptTimer++;
            int status = g_NetworkManager.CheckConnect();
            if (status == 0)
            {
                matchingSubState = 3;
                attemptTimer = 0;
            }
            else if (status == -1 || attemptTimer > 180)
            {

                g_NetworkManager.Cleanup(false);
                matchingSubState = 0;
                attemptTimer = 0;
            }
        }
        else if (matchingSubState == 2)
        {
            if (g_NetworkManager.CheckForConnection())
            {
                matchingSubState = 3;
            }
        }

        if (matchingSubState == 3)
        {
            localHandshake.magic = 0x54483036;
            localHandshake.character = g_GameManager.character;
            localHandshake.shotType = g_GameManager.shotType;
            localHandshake.difficulty = g_GameManager.difficulty;
            localHandshake.rngSeed = (u16)timeGetTime();

            if (g_NetworkManager.ExchangeHandshake(&localHandshake, &remoteHandshake) == ZUN_SUCCESS)
            {

                if (!g_NetworkManager.IsHost())
                {
                    g_GameManager.difficulty = (Difficulty)remoteHandshake.difficulty;
                }

                g_NetRandomSeed = g_NetworkManager.IsHost() ? localHandshake.rngSeed : remoteHandshake.rngSeed;
                g_Rng.Initialize(g_NetRandomSeed);

                g_InputDelay = g_NetworkManager.NegotiateDelay(g_InputDelay);

                g_Supervisor.StopAudio();
                g_Supervisor.curState = SUPERVISOR_STATE_GAMEMANAGER;
                return CHAIN_CALLBACK_RESULT_CONTINUE_AND_REMOVE_JOB;
            }
            else if (!g_NetworkManager.IsHost())
            {
                g_NetworkManager.Cleanup(false);
                matchingSubState = 0;
                attemptTimer = 0;
            }
            else
            {
                InterlockedExchange(&g_NetHostPrepareStatus, NET_HOST_PREPARE_IDLE);
                StartNetHostPrepareThread();
                matchingSubState = 1;
            }
        }
    }
    break;
    }
    menu->stateTimer = menu->stateTimer + 1;
    for (i = 0; i < ARRAY_SIZE_SIGNED(menu->vm); i++)
    {
        vm = &menu->vm[i];
        if (vm->sprite == NULL)
        {
            hasLoadedSprite = false;
        }
        else if (vm->sprite->sourceFileIndex < 0)
        {
            hasLoadedSprite = false;
        }
        else
        {
            hasLoadedSprite = g_AnmManager->textures[vm->sprite->sourceFileIndex] != NULL;
        }
        if (hasLoadedSprite)
        {
            g_AnmManager->ExecuteScript(&menu->vm[i]);
        }
    }
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

CursorMovement MainMenu::MoveCursor(MainMenu *menu, i32 menuLength)
{
    if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_UP))
    {
        menu->cursor--;
        g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
        if (menu->cursor < 0)
        {
            menu->cursor = menuLength - 1;
        }
        if (menu->cursor >= menuLength)
        {
            menu->cursor = 0;
        }
        return CURSOR_MOVE_UP;
    }

    if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_DOWN))
    {
        menu->cursor++;
        g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
        if (menu->cursor < 0)
        {
            menu->cursor = menuLength - 1;
        }
        if (menu->cursor >= menuLength)
        {
            menu->cursor = 0;
        }
        return CURSOR_MOVE_DOWN;
    }

    return CURSOR_DONT_MOVE;
}

void MainMenu::SwapMapping(MainMenu *menu, i16 btnPressed, i16 oldMapping, ZunBool unk)
{
    if (unk == 0 && menu->controlMapping[0] == btnPressed)
    {
        menu->controlMapping[0] = oldMapping;
    }
    if (menu->controlMapping[1] == btnPressed)
    {
        menu->controlMapping[1] = oldMapping;
    }
    if (unk == 0 && menu->controlMapping[2] == btnPressed)
    {
        menu->controlMapping[2] = oldMapping;
    }
    if (menu->controlMapping[4] == btnPressed)
    {
        menu->controlMapping[4] = oldMapping;
    }
    if (menu->controlMapping[5] == btnPressed)
    {
        menu->controlMapping[5] = oldMapping;
    }
    if (menu->controlMapping[6] == btnPressed)
    {
        menu->controlMapping[6] = oldMapping;
    }
    if (menu->controlMapping[7] == btnPressed)
    {
        menu->controlMapping[7] = oldMapping;
    }
    if (menu->controlMapping[3] == btnPressed)
    {
        menu->controlMapping[3] = oldMapping;
    }
    if (menu->controlMapping[8] == btnPressed)
    {
        menu->controlMapping[8] = oldMapping;
    }
}

void MainMenu::DrawMenuItem(AnmVm *vm, int itemNumber, int cursor, D3DCOLOR currentItemColor, D3DCOLOR otherItemColor,
                            int vm_amount)
{
    D3DXVECTOR3 otherItemPos;
    D3DXVECTOR3 currentItemPos;

    if (itemNumber == cursor)
    {
        if (!g_Supervisor.cfg.IsSoftwareTexturing())
        {
            vm->color = currentItemColor;
        }
        else
        {
            g_AnmManager->SetActiveSprite(vm, vm->baseSpriteIndex + vm_amount);
            vm->color =
                currentItemColor & D3DCOLOR_RGBA(0x00, 0x00, 0x00, 0xff) | D3DCOLOR_RGBA(0xff, 0xff, 0xff, 0x00);
        }

        currentItemPos.x = -4.0f;
        currentItemPos.y = -4.0f;
        currentItemPos.z = 0.0f;
        vm->posOffset = currentItemPos;
    }
    else
    {
        if (!g_Supervisor.cfg.IsSoftwareTexturing())
        {
            vm->color = otherItemColor;
        }

        else
        {
            g_AnmManager->SetActiveSprite(vm, vm->baseSpriteIndex);
            vm->color = otherItemColor & D3DCOLOR_RGBA(0x00, 0x00, 0x00, 0xff) | D3DCOLOR_RGBA(0xff, 0xff, 0xff, 0x00);
        }
        otherItemPos.x = 0.0f;
        otherItemPos.y = 0.0f;
        otherItemPos.z = 0.0f;
        vm->posOffset = otherItemPos;
    }
}

static void FastForwardTitleIntro(MainMenu *menu)
{
    const i32 introFrames = 120;

    for (i32 frame = 0; frame < introFrames; frame++)
    {
        for (i32 j = 0; j < ARRAY_SIZE_SIGNED(menu->vm); j++)
        {
            AnmVm *v = &menu->vm[j];
            if (v->sprite == NULL || v->sprite->sourceFileIndex < 0 ||
                g_AnmManager->textures[v->sprite->sourceFileIndex] == NULL)
            {
                continue;
            }
            g_AnmManager->ExecuteScript(v);
        }
    }
}

ZunResult MainMenu::BeginStartup()
{
    D3DXVECTOR3 vector3Ptr;
    DWORD time;
    int i;

    if (LoadTitleAnm(this) != ZUN_SUCCESS)
    {
        g_Supervisor.curState = SUPERVISOR_STATE_EXITSUCCESS;
        return ZUN_ERROR;
    }

    if (g_Supervisor.startupTimeBeforeMenuMusic <= 0)
    {
        g_SkipTitleIntro = true;
    }
    if (g_Supervisor.startupTimeBeforeMenuMusic > 0)
    {
        time = timeGetTime();
        while ((time - g_Supervisor.startupTimeBeforeMenuMusic >= 0) &&
               (3000 > time - g_Supervisor.startupTimeBeforeMenuMusic))
        {
            time = timeGetTime();
        }
        g_Supervisor.startupTimeBeforeMenuMusic = 0;
        g_Supervisor.PlayAudio("bgm/th06_01.mid");
    }
    for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
    {
        this->vm[i].pendingInterrupt = 1;
        this->vm[i].flags.colorOp = AnmVmColorOp_Add;
        if ((g_Supervisor.cfg.opts & (1 << GCOS_USE_D3D_HW_TEXTURE_BLENDING)) == 0)
        {
            this->vm[i].color = COLOR_BLACK;
        }
        else
        {
            this->vm[i].color = COLOR_WHITE;
        }
        vector3Ptr.x = 0.0;
        vector3Ptr.y = 0.0;
        vector3Ptr.z = 0.0;
        this->vm[i].posOffset = vector3Ptr;
    }
    this->gameState = STATE_PRE_INPUT;
    if (g_ReturnToSpellPractice || g_ReturnToPractice)
    {
        this->gameState = STATE_DIFFICULTY_LOAD;
        this->stateTimer = 59;
    }

    switch (g_Supervisor.wantedState2)
    {
    case SUPERVISOR_STATE_RESULTSCREEN:
    case SUPERVISOR_STATE_RESULTSCREEN_FROMGAME:
    case SUPERVISOR_STATE_MUSICROOM:
    case SUPERVISOR_STATE_ENDING:
        g_SkipTitleIntro = true;
        break;
    default:
        break;
    }
    if (g_SkipTitleIntro && this->gameState != STATE_PRE_INPUT)
    {

        g_SkipTitleIntro = false;
    }
    if (g_SkipTitleIntro)
    {
        g_SkipTitleIntro = false;
        FastForwardTitleIntro(this);
        this->stateTimer = 0;
        this->gameState = STATE_MAIN_MENU;
        for (i = 0; i < 122; i++)
        {
            this->vm[i].pendingInterrupt = 2;
        }
        if (!((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1))
        {
            this->vm[this->cursor].color = COLOR_RED;
        }
        else
        {
            this->vm[this->cursor].color = COLOR_PINK;
        }
        vector3Ptr.x = -6.0f;
        vector3Ptr.y = -6.0f;
        vector3Ptr.z = 0.0f;
        this->vm[this->cursor].posOffset = vector3Ptr;
        this->minimumOpacity = 0;
        this->menuTextColor = COLOR_MENU_ACTIVE_BACKGROUND;
        this->numFramesSinceActive = 0;
        this->framesActive = 60;
    }
    return ZUN_SUCCESS;
}

ZunBool MainMenu::WeirdSecondInputCheck()
{
    i32 vm;
    D3DXVECTOR3 d3dVec;

    if (this->stateTimer < 0x1e)
    {
        return true;
    }

    if (!WAS_PRESSED_OR_REPEAT(TH_BUTTON_SELECTMENU | TH_BUTTON_BOMB | TH_BUTTON_MENU | TH_BUTTON_Q | TH_BUTTON_S))
    {
        return true;
    }

    this->stateTimer = 0;
    this->gameState = STATE_MAIN_MENU;
    for (vm = 0; vm < 122; vm++)
    {
        this->vm[vm].pendingInterrupt = 2;
    }
    if (!((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1))
    {
        this->vm[this->cursor].color = COLOR_RED;
    }
    else
    {
        this->vm[this->cursor].color = COLOR_PINK;
    }
    d3dVec.x = -6.0;
    d3dVec.y = -6.0;
    d3dVec.z = 0.0;
    this->vm[this->cursor].posOffset = d3dVec;

    this->minimumOpacity = 0;
    this->menuTextColor = COLOR_MENU_ACTIVE_BACKGROUND;
    this->numFramesSinceActive = 0;
    this->framesActive = 60;
    return false;
}

ZunResult MainMenu::DrawStartMenu(void)
{
    i32 i;
    i = MoveCursor(this, 8);
    if ((this->cursor == 1) && !g_GameManager.HasReachedMaxClears(0, 0) && !g_GameManager.HasReachedMaxClears(0, 1) &&
        !g_GameManager.HasReachedMaxClears(1, 0) && !g_GameManager.HasReachedMaxClears(1, 1))
    {
        this->cursor += i;
    }
    AnmVm *drawVm = this->vm;
    for (i = 0; i < 8; i++, drawVm++ /* */)
    {
        DrawMenuItem(drawVm, i, this->cursor, COLOR_RED, COLOR_START_MENU_ITEM_INACTIVE, 122);
    }
    if (this->stateTimer >= 0x14)
    {
        if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
        {
            switch (this->cursor)
            {
            case 0:
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 4;
                }
                this->gameState = STATE_DIFFICULTY_LOAD;
                g_GameManager.isInPracticeMode = 0;
                if (EXTRA <= g_GameManager.difficulty)
                {
                    g_GameManager.difficulty = NORMAL;
                }
                if (EXTRA <= g_Supervisor.cfg.defaultDifficulty)
                {
                    g_Supervisor.cfg.defaultDifficulty = NORMAL;
                }
                this->stateTimer = 0;
                this->minimumOpacity = 0x40000000;
                this->menuTextColor = COLOR_BLACK;
                this->numFramesSinceActive = 0;
                this->framesActive = 60;
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                break;
            case 1:
                if (!(!g_GameManager.HasReachedMaxClears(0, 0) && !g_GameManager.HasReachedMaxClears(0, 1) &&
                      !g_GameManager.HasReachedMaxClears(1, 0) && !g_GameManager.HasReachedMaxClears(1, 1)))
                {
                    for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                    {
                        this->vm[i].pendingInterrupt = 4;
                    }
                    this->gameState = STATE_DIFFICULTY_LOAD;
                    g_GameManager.isInPracticeMode = 0;
                    g_GameManager.difficulty = EXTRA;
                    this->stateTimer = 0;
                    this->minimumOpacity = 0x40000000;
                    this->menuTextColor = COLOR_BLACK;
                    this->numFramesSinceActive = 0;
                    this->framesActive = 60;
                    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                }
                else
                {
                    g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                }
                break;
            case 2:
                g_GameManager.isInPracticeMode = 1;
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 4;
                }
                this->gameState = STATE_DIFFICULTY_LOAD;
                if (EXTRA <= g_GameManager.difficulty)
                {
                    g_GameManager.difficulty = NORMAL;
                }
                if (EXTRA <= g_Supervisor.cfg.defaultDifficulty)
                {
                    g_Supervisor.cfg.defaultDifficulty = NORMAL;
                }
                this->stateTimer = 0;
                this->minimumOpacity = 0x40000000;
                this->menuTextColor = COLOR_BLACK;
                this->numFramesSinceActive = 0;
                this->framesActive = 60;
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                break;
            case 3:
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 4;
                }
                this->gameState = STATE_REPLAY_LOAD;
                g_GameManager.isInPracticeMode = 0;
                this->stateTimer = 0;
                this->minimumOpacity = 0x40000000;
                this->menuTextColor = COLOR_BLACK;
                this->numFramesSinceActive = 0;
                this->framesActive = 60;
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                break;
            case 4:
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 4;
                }
                this->gameState = STATE_SCORE;
                this->stateTimer = 0;
                this->minimumOpacity = 0x40000000;
                this->menuTextColor = COLOR_BLACK;
                this->numFramesSinceActive = 0;
                this->framesActive = 60;
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                break;
            case 5:
                this->gameState = STATE_MUSIC_ROOM;
                this->stateTimer = 0;
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 4;
                }
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                break;
            case 6:
                this->gameState = STATE_OPTIONS;
                this->stateTimer = 0;
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 3;
                }
                this->cursor = 0;
                this->colorMode16bit = g_Supervisor.cfg.colorMode16bit;
                this->windowed = g_Supervisor.cfg.windowed;
                this->frameskipConfig = g_Supervisor.cfg.frameskipConfig;
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                break;
            case 7:
                this->gameState = STATE_QUIT;
                this->stateTimer = 0;
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 4;
                }
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                break;
            }
        }
        if (WAS_PRESSED(TH_BUTTON_Q))
        {
            this->gameState = STATE_QUIT;
            this->stateTimer = 0;
            for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
            {
                this->vm[i].pendingInterrupt = 4;
            }
            g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
        }
        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {
            if (this->cursor == 7)
            {
                this->gameState = STATE_QUIT;
                this->stateTimer = 0;
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 4;
                }
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
            else
            {
                this->cursor = 7;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
        }
    }
    return ZUN_SUCCESS;
}

i32 MainMenu::ReplayHandling()
{
    AnmVm *anmVm;
    i32 cur;
    HANDLE replayFileHandle;
    u32 replayFileIdx;
    ReplayData *replayData;
    char replayFilePath[32];
    WIN32_FIND_DATA replayFileInfo;
    u8 padding[0x20];

    switch (this->gameState)
    {
    case STATE_REPLAY_LOAD:
        if (this->stateTimer == 60)
        {
            if (LoadReplayMenu(this))
            {
                g_GameErrorContext.Log(TH_ERR_MAINMENU_LOAD_SELECT_SCREEN_FAILED);
                g_Supervisor.curState = SUPERVISOR_STATE_EXITSUCCESS;
                return ZUN_SUCCESS;
            }
            else
            {
                replayFileIdx = 0;
                for (cur = 0; cur < 99 && replayFileIdx < MAX_REPLAY_FILES; cur++)
                {
                    sprintf(replayFilePath, "./replay/th6_%.2d.rpy", cur + 1);
                    replayData = (ReplayData *)FileSystem::OpenPath(replayFilePath, 1);
                    if (replayData == NULL)
                    {
                        continue;
                    }
                    if (!ReplayManager::ValidateReplayData(replayData, g_LastFileSize))
                    {
                        s_replayFileData[replayFileIdx] = *replayData;
                        strcpy(s_replayFilePaths[replayFileIdx], replayFilePath);
                        sprintf(s_replayFileName[replayFileIdx], "No.%.2d", cur + 1);
                        if (replayFileIdx < 60)
                        {
                            this->replayFileData[replayFileIdx] = *replayData;
                            strcpy(this->replayFilePaths[replayFileIdx], replayFilePath);
                            strcpy(this->replayFileName[replayFileIdx], s_replayFileName[replayFileIdx]);
                        }
                        replayFileIdx++;
                    }
                    free(replayData);
                }
                _mkdir("./replay");
                _chdir("./replay");
                replayFileHandle = FindFirstFileA("th6_ud????.rpy", &replayFileInfo);
                if (replayFileHandle != INVALID_HANDLE_VALUE)
                {
                    for (cur = 0; cur < 0x2d && replayFileIdx < MAX_REPLAY_FILES; cur++)
                    {
                        sprintf(replayFilePath, "./replay/%s", replayFileInfo.cFileName);
                        replayData = (ReplayData *)FileSystem::OpenPath(replayFilePath, 1);
                        if (replayData == NULL)
                        {
                            continue;
                        }
                        if (!ReplayManager::ValidateReplayData(replayData, g_LastFileSize))
                        {
                            s_replayFileData[replayFileIdx] = *replayData;
                            strcpy(s_replayFilePaths[replayFileIdx], replayFilePath);
                            sprintf(s_replayFileName[replayFileIdx], "User ");
                            if (replayFileIdx < 60)
                            {
                                this->replayFileData[replayFileIdx] = *replayData;
                                strcpy(this->replayFilePaths[replayFileIdx], replayFilePath);
                                strcpy(this->replayFileName[replayFileIdx], s_replayFileName[replayFileIdx]);
                            }
                            replayFileIdx++;
                        }
                        free(replayData);
                        if (!FindNextFileA(replayFileHandle, &replayFileInfo))
                            break;
                    }
                }
                FindClose(replayFileHandle);
                _chdir("../");
                this->replayFilesNum = replayFileIdx;
                this->minimumOpacity = 0;
                this->framesInactive = this->framesActive;
                this->framesActive = 0;
                this->gameState = STATE_REPLAY_ANIM;
                this->stateTimer = 0;
                anmVm = this->vm;
                for (cur = 0; cur < ARRAY_SIZE_SIGNED(this->vm); cur++, anmVm++)
                {
                    anmVm->pendingInterrupt = 15;
                }
                i32 targetReplay = s_lastChosenReplay;
                if (g_GameManager.replayFile[0] != '\0')
                {
                    for (cur = 0; cur < (i32)this->replayFilesNum; cur++)
                    {
                        if (strcmp(s_replayFilePaths[cur], g_GameManager.replayFile) == 0)
                        {
                            targetReplay = cur;
                            break;
                        }
                    }
                }
                if (targetReplay >= 0 && targetReplay < (i32)this->replayFilesNum)
                {
                    this->cursor = targetReplay;
                    this->chosenReplay = targetReplay;
                    s_lastChosenReplay = targetReplay;
                }
                else
                {
                    this->cursor = 0;
                    this->chosenReplay = 0;
                }
            }
            break;
        }
        break;
    case STATE_REPLAY_UNLOAD:
        if (this->stateTimer == 0x24)
        {
            this->gameState = STATE_STARTUP;
            this->stateTimer = 0;

            g_SkipTitleIntro = true;
            this->cursor = 3;
        }
        break;
    case STATE_REPLAY_ANIM:
        if (this->stateTimer < 0x28)
        {
            break;
        }
        if (this->replayFilesNum != NULL)
        {
            i32 prevCursor = this->cursor;
            MoveCursor(this, this->replayFilesNum);

            // カーソルが動いたら削除確認をキャンセル
            if (this->cursor != prevCursor)
            {
                s_deleteConfirmIdx = -1;
            }

            // Delete キーによるリプレイ削除
            static bool s_prevDeleteDown = false;
            bool deleteDown =
                (GetForegroundWindow() == g_Supervisor.hwndGameWindow) && (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;
            bool deletePressed = deleteDown && !s_prevDeleteDown;
            s_prevDeleteDown = deleteDown;

            if (deletePressed)
            {
                if (s_deleteConfirmIdx == this->cursor)
                {
                    // 2回目: ファイルを削除してリストを詰める
                    DeleteFileA(s_replayFilePaths[s_deleteConfirmIdx]);
                    i32 del = s_deleteConfirmIdx;
                    i32 total = (i32)this->replayFilesNum;
                    for (i32 k = del; k < total - 1; k++)
                    {
                        s_replayFileData[k] = s_replayFileData[k + 1];
                        memcpy(s_replayFilePaths[k], s_replayFilePaths[k + 1], MAX_PATH);
                        memcpy(s_replayFileName[k], s_replayFileName[k + 1], 16);
                        if (k < 60)
                        {
                            this->replayFileData[k] = this->replayFileData[k + 1];
                            memcpy(this->replayFilePaths[k], this->replayFilePaths[k + 1], MAX_PATH);
                            memcpy(this->replayFileName[k], this->replayFileName[k + 1], 8);
                        }
                    }
                    this->replayFilesNum = (u32)(total - 1);
                    if (this->cursor >= (i32)this->replayFilesNum && this->cursor > 0)
                        this->cursor--;
                    this->chosenReplay = this->cursor;
                    s_lastChosenReplay = this->cursor;
                    s_deleteConfirmIdx = -1;
                    g_SoundPlayer.PlaySoundByIdx(SOUND_1UP, 0);
                }
                else
                {
                    // 1回目: 確認待ちに設定
                    s_deleteConfirmIdx = this->cursor;
                    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                }
            }

            i32 totalPages = (this->replayFilesNum + 14) / 15;
            if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_LEFT) && totalPages > 1)
            {
                i32 curPage = this->cursor / 15;
                i32 row = this->cursor % 15;
                i32 prevPage = (curPage + totalPages - 1) % totalPages;
                i32 targetCursor = prevPage * 15 + row;
                if (targetCursor < (i32)this->replayFilesNum)
                {
                    this->cursor = targetCursor;
                }
                else
                {
                    this->cursor = prevPage * 15;
                }
                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
            }
            else if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_RIGHT) && totalPages > 1)
            {
                i32 curPage = this->cursor / 15;
                i32 row = this->cursor % 15;
                i32 nextPage = (curPage + 1) % totalPages;
                i32 targetCursor = nextPage * 15 + row;
                if (targetCursor < (i32)this->replayFilesNum)
                {
                    this->cursor = targetCursor;
                }
                else
                {
                    this->cursor = nextPage * 15;
                }
                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
            }
            this->chosenReplay = this->cursor;
            s_lastChosenReplay = this->cursor;
            if (WAS_PRESSED(TH_BUTTON_SELECTMENU))
            {
                this->gameState = STATE_REPLAY_SELECT;
                anmVm = &(this->vm[97]);
                for (cur = 0; cur < 0x19; cur += 1, anmVm++)
                {
                    anmVm->pendingInterrupt = 0x11;
                }
                anmVm = &this->vm[99 + (this->chosenReplay % 15)];
                anmVm->pendingInterrupt = 0x10;
                this->stateTimer = 0;
                this->cursor = 0;
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                this->currentReplay = (ReplayData *)FileSystem::OpenPath(s_replayFilePaths[this->chosenReplay], 1);

                ReplayManager::ReadThpracInfo((const u8 *)this->currentReplay, g_LastFileSize, &g_CurrentReplayThprac);
                ReplayManager::ValidateReplayData(this->currentReplay, g_LastFileSize);
                for (cur = 0; cur < ARRAY_SIZE_SIGNED(this->currentReplay->stageReplayData); cur++)
                {
                    if (this->currentReplay->stageReplayData[cur] != NULL)
                    {
                        this->currentReplay->stageReplayData[cur] =
                            (StageReplayData *)((u32)this->currentReplay +
                                                (u32)this->currentReplay->stageReplayData[cur]);
                    }
                }

                while (s_replayFileData[this->chosenReplay].stageReplayData[this->cursor] == NULL)
                {
                    this->cursor = this->cursor + 1;

                    if ((int)this->cursor >= ARRAY_SIZE_SIGNED(this->currentReplay->stageReplayData))
                    {
                        return ZUN_SUCCESS;
                    }
                }
            }
        }
        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {
            if (s_deleteConfirmIdx >= 0)
            {
                // 確認中に戻るボタン → キャンセル
                s_deleteConfirmIdx = -1;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                break;
            }
            this->gameState = STATE_REPLAY_UNLOAD;
            this->stateTimer = 0;
            for (cur = 0; cur < ARRAY_SIZE_SIGNED(this->vm); cur++)
            {
                this->vm[cur].pendingInterrupt = 4;
            }
            g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            this->cursor = 0;
            break;
        }
        break;
    case STATE_REPLAY_SELECT:
        if (this->stateTimer < 0x28)
        {
            break;
        }
        cur = MoveCursor(this, 7);
        if (cur < 0)
        {
            while (s_replayFileData[this->chosenReplay].stageReplayData[this->cursor] == NULL)
            {
                this->cursor--;
                if (this->cursor < 0)
                {
                    this->cursor = 6;
                }
            }
        }
        else if (cur > 0)
        {
            while (s_replayFileData[this->chosenReplay].stageReplayData[this->cursor] == NULL)
            {
                this->cursor++;
                if (this->cursor >= 7)
                {
                    this->cursor = 0;
                }
            }
        }
        if (WAS_PRESSED(TH_BUTTON_SELECTMENU) && this->currentReplay[this->cursor].stageReplayData)
        {
            g_GameManager.isInReplay = 1;
            g_Supervisor.framerateMultiplier = 1.0;
            s_lastChosenReplay = this->chosenReplay;
            strcpy(g_GameManager.replayFile, s_replayFilePaths[this->chosenReplay]);
            g_GameManager.difficulty = (Difficulty)this->currentReplay->difficulty;
            g_GameManager.character = this->currentReplay->shottypeChara / 2;
            g_GameManager.shotType = this->currentReplay->shottypeChara % 2;
            cur = 0;
            while (this->currentReplay->stageReplayData[cur] == NULL)
            {
                cur++;
            }
            g_GameManager.livesRemaining = this->currentReplay->stageReplayData[cur]->livesRemaining;
            g_GameManager.bombsRemaining = this->currentReplay->stageReplayData[cur]->bombsRemaining;
            ReplayData *uh = this->currentReplay;
            free(uh);
            this->currentReplay = NULL;
            g_GameManager.currentStage = this->cursor;
            g_Supervisor.curState = SUPERVISOR_STATE_GAMEMANAGER;
            return 1;
        }
        if (WAS_PRESSED(TH_BUTTON_RETURNMENU))
        {
            ReplayData *uh2 = this->currentReplay;
            free(uh2);
            this->currentReplay = NULL;
            this->gameState = STATE_REPLAY_ANIM;
            this->stateTimer = 0;
            for (cur = 0; cur < ARRAY_SIZE_SIGNED(this->vm); cur++)
            {
                this->vm[cur].pendingInterrupt = 4;
            }
            g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            this->gameState = STATE_REPLAY_ANIM;
            anmVm = this->vm;
            for (cur = 0; cur < ARRAY_SIZE_SIGNED(this->vm); cur += 1, anmVm++)
            {
                anmVm->pendingInterrupt = 0xf;
            }
            this->cursor = this->chosenReplay;
        }
    }
    return 0;
}

ZunResult MainMenu::DrawReplayMenu()
{
    i32 replayAmount;
    i32 i;
    AnmVm *vmRef;
    ZunBool isSelected;
    ZunBool isSelected2;

    vmRef = &this->vm[98];
    i32 curPage = (this->chosenReplay / 15) + 1;
    i32 totalPages = (this->replayFilesNum + 14) / 15;
    g_AsciiManager.AddFormatText(&vmRef->pos, "No.   Name      Date     Player   Rank");
    if (totalPages > 1)
    {
        D3DXVECTOR3 pagePos = vmRef->pos;
        pagePos.x += 380.0f;
        pagePos.y -= 18.0f;
        g_AsciiManager.AddFormatText(&pagePos, "< %d / %d >", curPage, totalPages);
    }

    for (i = this->chosenReplay - this->chosenReplay % 15, replayAmount = i; i < replayAmount + 15; i++)
    {
        if (i >= this->replayFilesNum)
        {
            break;
        }
        vmRef++;
        if (!g_Supervisor.cfg.IsSoftwareTexturing())
        {
            if (i == this->chosenReplay)
            {
                g_AsciiManager.color = COLOR_LIGHT_RED;
            }
            else
            {
                g_AsciiManager.color = COLOR_GREY;
            }
        }
        else
        {
            isSelected = (i == this->chosenReplay);
            g_AsciiManager.isSelected = isSelected;

            if (i == this->chosenReplay)
            {
                g_AsciiManager.color = COLOR_WHITE;
            }
            else
            {
                g_AsciiManager.color = COLOR_GREY;
            }
        }

        if (i == s_deleteConfirmIdx)
        {
            // 確認中: 通常行テキストの代わりに確認メッセージを上書き表示
            g_AsciiManager.color = 0xFFFF4040;
            g_AsciiManager.AddFormatText(&vmRef->pos, "%s Delete? [Del]:Yes  [Esc]:No", s_replayFileName[i]);
        }
        else
        {
            g_AsciiManager.AddFormatText(&vmRef->pos, "%s %8s  %8s %7s  %7s", s_replayFileName[i],
                                         s_replayFileData[i].name, s_replayFileData[i].date,
                                         g_ShortCharacterList[s_replayFileData[i].shottypeChara],
                                         g_DifficultyList[s_replayFileData[i].difficulty]);
        }
    }
    if (this->gameState == STATE_REPLAY_SELECT && this->currentReplay)
    {
        g_AsciiManager.color = COLOR_WHITE;
        g_AsciiManager.isSelected = false;

        vmRef = &this->vm[97];
        g_AsciiManager.AddFormatText(&vmRef->pos, "       %2.3f%%", this->currentReplay->slowdownRate);

        vmRef = &this->vm[114];
        g_AsciiManager.AddFormatText(&vmRef->pos, "Stage  LastScore");

        for (i = 0; i < 7; i++)
        {
            vmRef++;
            if (!g_Supervisor.cfg.IsSoftwareTexturing())
            {
                if (i == this->cursor)
                {
                    g_AsciiManager.color = COLOR_LIGHT_RED;
                }
                else
                {
                    g_AsciiManager.color = COLOR_GREY;
                }
            }
            else
            {
                isSelected2 = (i == this->cursor);
                g_AsciiManager.isSelected = isSelected2;
                if (i == this->cursor)
                {
                    g_AsciiManager.color = COLOR_WHITE;
                }
                else
                {
                    g_AsciiManager.color = COLOR_GREY;
                }
            }
            if (this->currentReplay->stageReplayData[i])
            {
                g_AsciiManager.AddFormatText(&vmRef->pos, "%s %9d", g_StageList[i],
                                             this->currentReplay->stageReplayData[i]->score);
            }
            else
            {
                g_AsciiManager.AddFormatText(&vmRef->pos, "%s ---------", g_StageList[i]);
            }
        }

        if (g_CurrentReplayThprac.isThprac)
        {
            D3DXVECTOR3 thpracPos = vmRef->pos;

            g_AsciiManager.color = COLOR_LIGHTBLUE;
            g_AsciiManager.isSelected = false;
            thpracPos.y += 24.0f;
            g_AsciiManager.AddFormatText(&thpracPos, "thprac Replay");
            if (g_CurrentReplayThprac.section > 0)
            {
                thpracPos.y += 16.0f;
                if (g_CurrentReplayThprac.section < ARRAY_SIZE_SIGNED(g_ThpracSectionNames))
                {
                    g_AsciiManager.AddFormatText(&thpracPos, "Section: %s",
                                                 g_ThpracSectionNames[g_CurrentReplayThprac.section]);
                }
                else
                {
                    g_AsciiManager.AddFormatText(&thpracPos, "Section: %d", g_CurrentReplayThprac.section);
                }
            }

            if (g_CurrentReplayThprac.frame > 0)
            {
                thpracPos.y += 16.0f;
                g_AsciiManager.AddFormatText(&thpracPos, "Start Frame: %d", g_CurrentReplayThprac.frame);
            }
        }
    }
    g_AsciiManager.color = COLOR_WHITE;
    g_AsciiManager.isSelected = false;
    return ZUN_SUCCESS;
}

void MainMenu::ColorMenuItem(AnmVm *vm, i32 item, i32 subItem, i32 subItemSelected)
{
    if (subItem != subItemSelected)
    {
        if (!g_Supervisor.cfg.IsSoftwareTexturing())
        {
            vm->color = COLOR_MENU_ITEM_DEFAULT;
        }
        else
        {
            g_AnmManager->SetActiveSprite(vm, vm->baseSpriteIndex);
        }
        vm->scaleX = 1.0;
        vm->scaleY = 1.0;
        vm->posOffset = D3DXVECTOR3(0.0, 0.0, 0.0);
    }
    else
    {
        if (!g_Supervisor.cfg.IsSoftwareTexturing())
        {
            vm->color = COLOR_MENU_ITEM_HIGHLIGHT;
        }
        else if (vm->baseSpriteIndex < ANM_OFFSET_TITLE04)
        {
            g_AnmManager->SetActiveSprite(vm, vm->baseSpriteIndex + (ANM_OFFSET_TITLE01S - ANM_OFFSET_TITLE01));
        }
        else
        {
            g_AnmManager->SetActiveSprite(vm, vm->baseSpriteIndex + (ANM_OFFSET_TITLE04S - ANM_OFFSET_TITLE04));
        }
        vm->posOffset = D3DXVECTOR3(-2.0, -2.0, 0.0);
    }

    if (item != this->cursor)
    {
        if ((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING & 1) == 0)
        {
            vm->color = COLOR_SET_ALPHA2(vm->color, 128);
        }
        else
        {
            vm->color = COLOR_SET_ALPHA2(vm->color, 128);
        }

        vm->posOffset += D3DXVECTOR3(0.0, 0.0, 0.0);
    }
    else
    {
        if ((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING & 1) == 0)
        {
            vm->color = COLOR_SET_ALPHA2(vm->color, 255);
        }
        else
        {
            vm->color = COLOR_SET_ALPHA2(vm->color, 255);
        }

        vm->posOffset += D3DXVECTOR3(-4.0, -4.0, 0.0);
    }
}

u32 MainMenu::OnUpdateOptionsMenu()
{

    AnmVm *optionsVm;
    i32 i;

    MoveCursor(this, 9);
    optionsVm = &this->vm[8];
    for (i = 0; i < 9; i++)
    {
        if (i >= 5 && i <= 7)
        {
            this->ColorMenuItem(&this->vm[i + 67], i, i, this->cursor);
        }
        else
        {
            this->ColorMenuItem(optionsVm, i, i, this->cursor);
            optionsVm++;
        }
    }

    for (i = 0; i < 5; i++, optionsVm++)
    {
        this->ColorMenuItem(optionsVm, CURSOR_OPTIONS_POS_LIFECOUNT, i, g_Supervisor.cfg.lifeCount);
    }

    for (i = 0; i < 4; i++, optionsVm++)
    {
        this->ColorMenuItem(optionsVm, CURSOR_OPTIONS_POS_BOMBCOUNT, i, g_Supervisor.cfg.bombCount);
    }
    for (i = 0; i < 2; i++, optionsVm++)
    {
        this->ColorMenuItem(optionsVm, CURSOR_OPTIONS_POS_COLORMODE, i, g_Supervisor.cfg.colorMode16bit);
    }
    for (i = 0; i < 2; i++, optionsVm++)
    {
        this->ColorMenuItem(optionsVm, CURSOR_OPTIONS_POS_PLAYSOUNDS, i, g_Supervisor.cfg.playSounds);
    }
    optionsVm = &this->vm[77];

    for (i = 0; i < 3; i++, optionsVm++)
    {
        this->ColorMenuItem(optionsVm, CURSOR_OPTIONS_POS_MUSICMODE, i, g_Supervisor.cfg.musicMode);
    }
    optionsVm = &this->vm[75];
    for (i = 0; i < 2; i++, optionsVm++)
    {
        this->ColorMenuItem(optionsVm, CURSOR_OPTIONS_POS_SCREENMODE, i, this->windowed);
    }
    if (this->stateTimer >= 32)
    {
        if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_LEFT))
        {
            switch (this->cursor)
            {
            case CURSOR_OPTIONS_POS_LIFECOUNT:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                if (g_Supervisor.cfg.lifeCount <= 0)
                {
                    g_Supervisor.cfg.lifeCount = 5;
                }
                g_Supervisor.cfg.lifeCount -= 1;
                break;

            case CURSOR_OPTIONS_POS_BOMBCOUNT:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                if (g_Supervisor.cfg.bombCount <= 0)
                {
                    g_Supervisor.cfg.bombCount = 4;
                }
                g_Supervisor.cfg.bombCount -= 1;
                break;

            case CURSOR_OPTIONS_POS_COLORMODE:

                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                g_Supervisor.cfg.colorMode16bit = 0;
                break;

            case CURSOR_OPTIONS_POS_MUSICMODE:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                g_Supervisor.StopAudio();
                if (g_Supervisor.cfg.musicMode <= OFF)
                {
                    g_Supervisor.cfg.musicMode = MIDI + 1;
                }
                g_Supervisor.cfg.musicMode -= 1;
                g_Supervisor.SetupMidiPlayback("bgm/th06_01.mid");
                g_Supervisor.PlayAudio("bgm/th06_01.mid");
                break;

            case CURSOR_OPTIONS_POS_PLAYSOUNDS:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                if (g_Supervisor.cfg.playSounds <= 0)
                {
                    g_Supervisor.cfg.playSounds = 2;
                }
                g_Supervisor.cfg.playSounds -= 1;
                break;

            case CURSOR_OPTIONS_POS_SCREENMODE:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                if (this->windowed <= 0)
                {
                    this->windowed = 2;
                }
                this->windowed -= 1;
                g_Supervisor.cfg.windowed = this->windowed;
                GameWindow::ApplyWindowMode();
                break;
            }
        }
        bool cancelToExit = false;
        if (WAS_PRESSED(TH_BUTTON_MENU | TH_BUTTON_BOMB))
        {
            if (this->cursor == CURSOR_OPTIONS_POS_EXIT)
            {
                cancelToExit = true;
            }
            else
            {
                this->cursor = CURSOR_OPTIONS_POS_EXIT;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
        }
        if (WAS_PRESSED_OR_REPEAT(TH_BUTTON_RIGHT))
        {
            switch (this->cursor)
            {
            case CURSOR_OPTIONS_POS_LIFECOUNT:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                g_Supervisor.cfg.lifeCount += 1;
                if (g_Supervisor.cfg.lifeCount >= 5)
                {
                    g_Supervisor.cfg.lifeCount = 0;
                }
                break;
            case CURSOR_OPTIONS_POS_BOMBCOUNT:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                g_Supervisor.cfg.bombCount += 1;
                if (g_Supervisor.cfg.bombCount >= 4)
                {
                    g_Supervisor.cfg.bombCount = 0;
                }
                break;
            case CURSOR_OPTIONS_POS_COLORMODE:

                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                g_Supervisor.cfg.colorMode16bit = 0;
                break;
            case CURSOR_OPTIONS_POS_MUSICMODE:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                g_Supervisor.StopAudio();
                g_Supervisor.cfg.musicMode += 1;
                if (g_Supervisor.cfg.musicMode >= MIDI + 1)
                {
                    g_Supervisor.cfg.musicMode = OFF;
                }
                g_Supervisor.SetupMidiPlayback("bgm/th06_01.mid");
                g_Supervisor.PlayAudio("bgm/th06_01.mid");
                break;
            case CURSOR_OPTIONS_POS_PLAYSOUNDS:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                g_Supervisor.cfg.playSounds += 1;
                if (g_Supervisor.cfg.playSounds >= 2)
                {
                    g_Supervisor.cfg.playSounds = 0;
                }
                break;
            case CURSOR_OPTIONS_POS_SCREENMODE:

                g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                this->windowed += 1;
                if (this->windowed >= 2)
                {
                    this->windowed = 0;
                }
                g_Supervisor.cfg.windowed = this->windowed;
                GameWindow::ApplyWindowMode();
                break;
            }
        }
        if (WAS_PRESSED(TH_BUTTON_SELECTMENU) || cancelToExit)
        {
            switch (this->cursor)
            {
            case CURSOR_OPTIONS_POS_KEYCONFIG:

                this->gameState = STATE_KEYCONFIG;
                this->stateTimer = 0;
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 5;
                }
                this->cursor = 0;
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);

                memcpy(this->controlMapping, &g_ControllerMapping, sizeof(ControllerMapping));

                g_ControllerMapping.upButton = -1;
                g_ControllerMapping.downButton = -1;
                break;

            case CURSOR_OPTIONS_POS_SETDEFAULT:

                g_Supervisor.StopAudio();
                g_Supervisor.cfg.lifeCount = 2;
                g_Supervisor.cfg.bombCount = 3;
                g_Supervisor.cfg.musicMode = WAV;
                g_Supervisor.cfg.playSounds = true;
                g_Supervisor.cfg.defaultDifficulty = NORMAL;
                g_Supervisor.cfg.windowed = false;
                g_Supervisor.cfg.frameskipConfig = 0;
                this->windowed = 0;
                this->frameskipConfig = 0;
                GameWindow::ApplyWindowMode();
                g_Supervisor.SetupMidiPlayback("bgm/th06_01.mid");
                g_Supervisor.PlayAudio("bgm/th06_01.mid");
                break;

            case CURSOR_OPTIONS_POS_EXIT:
                FileSystem::WriteDataToFile(TH_CONFIG_FILE, &g_Supervisor.cfg, sizeof(GameConfiguration));
                Supervisor::SaveWindowSettings();
                this->gameState = STATE_MAIN_MENU;
                this->stateTimer = 0;
                for (i = 0; i < ARRAY_SIZE_SIGNED(this->vm); i++)
                {
                    this->vm[i].pendingInterrupt = 2;
                }

                this->cursor = 6;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                if (this->colorMode16bit != g_Supervisor.cfg.colorMode16bit ||
                    this->windowed != g_Supervisor.cfg.windowed ||
                    this->frameskipConfig != g_Supervisor.cfg.frameskipConfig)
                {
                    g_Supervisor.cfg.frameskipConfig = this->frameskipConfig;

                    if (g_VerticalMode)
                        this->windowed = false;
                    bool windowedChanged = (this->windowed != g_Supervisor.cfg.windowed);
                    g_Supervisor.cfg.windowed = this->windowed;
                    if (this->colorMode16bit != g_Supervisor.cfg.colorMode16bit)
                    {
                        g_Supervisor.cfg.colorMode16bit = this->colorMode16bit;
                        g_Supervisor.curState = SUPERVISOR_STATE_EXITERROR;
                        return 1;
                    }
                    if (windowedChanged)
                        GameWindow::ApplyWindowMode();
                }
                break;
            }
        }
    }
    return 0;
}

static u32 g_ExtraBestScores[PSCR_NUM_CHARS_SHOTTYPES];

ZunResult MainMenu::ChoosePracticeLevel()
{
    if (this->gameState == STATE_PRACTICE_LVL_SELECT)
    {
        if (this->stateTimer == 0)
        {
            ScoreDat *sd = ResultScreen::OpenScore("score.dat");
            for (i32 cs = 0; cs < PSCR_NUM_CHARS_SHOTTYPES; cs++)
            {
                g_ExtraBestScores[cs] = ResultScreen::GetHighScore(sd, NULL, cs, EXTRA);
            }
            ResultScreen::ReleaseScoreDat(sd);
        }

        D3DXVECTOR3 textPos(320.0, 200.0, 0.0);
        u32 color = (this->stateTimer < 30) ? this->stateTimer * 0xFF / 30 : 0xff;
        i32 charShotType = (g_GameManager.character << 1) + g_GameManager.shotType;
        i32 selectedStage =
            (g_GameManager.clrd[charShotType].difficultyClearedWithoutRetries[g_GameManager.difficulty] > 6)
                ? 6
                : g_GameManager.clrd[charShotType].difficultyClearedWithoutRetries[g_GameManager.difficulty];

        if (g_GameManager.difficulty == EASY && selectedStage == 6)
        {
            selectedStage = 5;
        }
        if (g_GameManager.clrd[charShotType].difficultyClearedWithoutRetries[EXTRA] == 99)
        {
            selectedStage = 7;
        }

        if (g_SpellPracticeFrontAnmLoaded && !g_IsSpellPracticeDllMenuActive)
        {

            g_AsciiManager.color = (g_SpellPracticeStageConfiguring && g_SpellPracticeConfigCursor == 0)
                                       ? (color << 0x18 | 0x00C0F0F0)
                                       : ((color >> 1) << 0x18 | 0x0080C0C0);
            if (g_SpellPracticeStageConfiguring && g_SpellPracticeConfigCursor == 0)
            {
                textPos.x = 90.0f;
                textPos.y = 160.0f;
                g_AsciiManager.AddFormatText(&textPos, ">>");
            }
            i32 dispLives = g_SpellPracticeSavedLives;
            if (g_GameManager.difficulty == EXTRA && dispLives > 4)
                dispLives = 4;
            for (i32 k = 0; k < dispLives; k++)
            {
                g_SpellPracticeLivesIconVm.pos = D3DXVECTOR3(120.0f + k * 16.0f, 160.0f, 0.0f);
                g_AnmManager->DrawNoRotation(&g_SpellPracticeLivesIconVm);
            }

            g_AsciiManager.color = (g_SpellPracticeStageConfiguring && g_SpellPracticeConfigCursor == 1)
                                       ? (color << 0x18 | 0x00C0F0F0)
                                       : ((color >> 1) << 0x18 | 0x0080C0C0);
            if (g_SpellPracticeStageConfiguring && g_SpellPracticeConfigCursor == 1)
            {
                textPos.x = 90.0f;
                textPos.y = 176.0f;
                g_AsciiManager.AddFormatText(&textPos, ">>");
            }
            i32 dispBombs = g_SpellPracticeSavedBombs;
            if (g_GameManager.difficulty == EXTRA && dispBombs > 3)
                dispBombs = 3;
            for (i32 k = 0; k < dispBombs; k++)
            {
                g_SpellPracticeBombsIconVm.pos = D3DXVECTOR3(120.0f + k * 16.0f, 176.0f, 0.0f);
                g_AnmManager->DrawNoRotation(&g_SpellPracticeBombsIconVm);
            }

            g_AsciiManager.color = (g_SpellPracticeStageConfiguring && g_SpellPracticeConfigCursor == 2)
                                       ? (color << 0x18 | 0x00C0F0F0)
                                       : ((color >> 1) << 0x18 | 0x0080C0C0);
            if (g_SpellPracticeStageConfiguring && g_SpellPracticeConfigCursor == 2)
            {
                textPos.x = 90.0f;
                textPos.y = 192.0f;
                g_AsciiManager.AddFormatText(&textPos, ">>");
            }
            textPos.x = 120.0f;
            textPos.y = 192.0f;
            g_AsciiManager.AddFormatText(&textPos, "Rank: %d", g_SpellPracticeRank);
        }

        textPos.x = 320.0f;
        textPos.y = 216.0f;
        i32 stageNum;
        for (stageNum = 0; stageNum < selectedStage; stageNum++)
        {
            if (!g_SpellPracticeStageConfiguring && stageNum == this->cursor)
            {
                g_AsciiManager.color = color << 0x18 | 0x00C0F0F0;
            }
            else if (g_SpellPracticeStageConfiguring && stageNum == g_SpellPracticeSelectedStage)
            {
                g_AsciiManager.color = color << 0x18 | 0x00C0F0F0;
            }
            else
            {
                g_AsciiManager.color = (color >> 1) << 0x18 | 0x0080C0C0;
            }
            if (stageNum < PSCR_NUM_STAGES)
            {
                g_AsciiManager.AddFormatText(
                    &textPos, "STAGE %d  %.9d", stageNum + 1,
                    g_GameManager.pscr[charShotType][stageNum][g_GameManager.difficulty].score);
            }
            else
            {
                g_AsciiManager.AddFormatText(&textPos, "Extra    %.9d", g_ExtraBestScores[charShotType]);
            }
            textPos.y += 24;
        }
        g_AsciiManager.color = 0xFFFFFFFF;
    }
    return ZUN_SUCCESS;
}

ChainCallbackResult MainMenu::OnDraw(MainMenu *menu)
{
    D3DXVECTOR3 posBackup;
    D3DXVECTOR3 *pos;
    D3DXVECTOR3 *offset;
    BOOL shouldDraw;
    AnmVm *curVm;
    i32 vmIdx;
    ZunRect window;
    i32 targetOpacity;
    AnmManager *mgr;

    curVm = menu->vm;
    window.left = 0.0;
    window.top = 0.0;
    window.right = 640.0;
    window.bottom = 480.0;
    if (menu->gameState == STATE_STARTUP)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    mgr = g_AnmManager;
    mgr->currentTexture = NULL;
    g_AnmManager->CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    if (menu->framesActive != 0)
    {

        if (menu->numFramesSinceActive < (i32)menu->framesActive)
        {
            menu->numFramesSinceActive += 1;
        }
        targetOpacity = COLOR_ALPHA(menu->menuTextColor) - COLOR_ALPHA(menu->minimumOpacity);
        ScreenEffect::DrawSquare(
            &window,
            COLOR_SET_ALPHA(menu->menuTextColor, targetOpacity * menu->numFramesSinceActive / menu->framesActive +
                                                     COLOR_ALPHA(menu->minimumOpacity)));
    }
    else if (menu->numFramesSinceActive != 0)
    {
        menu->numFramesSinceActive -= 1;
        targetOpacity = COLOR_ALPHA(menu->menuTextColor) - COLOR_ALPHA(menu->minimumOpacity);
        ScreenEffect::DrawSquare(
            &window,
            COLOR_SET_ALPHA(menu->menuTextColor, targetOpacity * menu->numFramesSinceActive / menu->framesInactive +
                                                     COLOR_ALPHA(menu->minimumOpacity)));
    }
    for (vmIdx = 0; vmIdx < 98; vmIdx++, curVm++)
    {
        if (curVm->sprite == NULL)
        {
            shouldDraw = false;
        }
        else if (curVm->sprite->sourceFileIndex < 0)
        {
            shouldDraw = false;
        }
        else
        {
            shouldDraw = g_AnmManager->textures[curVm->sprite->sourceFileIndex] != NULL;
        }
        if (shouldDraw)
        {
            memcpy(posBackup, curVm->pos, sizeof(D3DXVECTOR3));
            offset = &curVm->posOffset;
            pos = &curVm->pos;
            pos->x += offset->x;
            pos->y += offset->y;
            pos->z += offset->z;
            g_AnmManager->Draw(curVm);
            memcpy(curVm->pos, posBackup, sizeof(D3DXVECTOR3));
        }
    }
    switch (menu->gameState)
    {
    case STATE_REPLAY_ANIM:
    case STATE_REPLAY_UNLOAD:
    case STATE_REPLAY_SELECT:
        menu->DrawReplayMenu();
        break;
    case STATE_SHOT_SELECT:
        if (g_IsOnline)
        {
            D3DXVECTOR3 pos(60.0, 420.0, 0.0);
            g_AsciiManager.color = COLOR_WHITE;
            g_AsciiManager.AddFormatText(&pos, "HOLD SHIFT+ENTER TO START AS HOST");
        }
        break;
    case STATE_ONLINE_SELECT: {
        D3DXVECTOR3 pos(100.0, 180.0, 0.0);
        g_AsciiManager.color = COLOR_WHITE;
        g_AsciiManager.AddFormatText(&pos, "ONLINE MODE");
        pos.y += 40;
        g_AsciiManager.color = (menu->cursor == 0) ? COLOR_LIGHT_RED : COLOR_WHITE;
        g_AsciiManager.AddFormatText(&pos, " HOST");
        pos.y += 20;
        g_AsciiManager.color = (menu->cursor == 1) ? COLOR_LIGHT_RED : COLOR_WHITE;
        g_AsciiManager.AddFormatText(&pos, " GUEST");
        pos.y += 40;
        g_AsciiManager.color = COLOR_WHITE;
        g_AsciiManager.AddFormatText(&pos, "Z: Select  X: Back");
    }
    break;
    case STATE_IP_INPUT: {
        D3DXVECTOR3 basePos(100.0, 200.0, 0.0);
        D3DXVECTOR3 pos = basePos;
        g_AsciiManager.color = COLOR_WHITE;

        if (g_NetConfig.isHost)
        {
            g_AsciiManager.AddFormatText(&pos, "ENTER HOST PORT");
            pos = basePos;
            pos.y += 30;
            f32 lineStartX = pos.x;
            g_AsciiManager.AddFormatText(&pos, "PORT: %s", g_NetConfig.portStr);

            pos.x = lineStartX + ((g_NetConfig.cursor - 16) + 6) * 14.0f;
            pos.y += 20;
            g_AsciiManager.AddFormatText(&pos, "^");
        }
        else
        {
            g_NetConfig.ip[3] = '.';
            g_NetConfig.ip[7] = '.';
            g_NetConfig.ip[11] = '.';
            g_NetConfig.ip[15] = '\0';
            g_AsciiManager.AddFormatText(&pos, "ENTER HOST IP:PORT");

            pos = basePos;
            pos.y += 30;
            f32 lineStartX = pos.x;
            g_AsciiManager.AddFormatText(&pos, "%s:%s", g_NetConfig.ip, g_NetConfig.portStr);
            pos.x = lineStartX + g_NetConfig.cursor * 14.0f;
            pos.y += 20;
            g_AsciiManager.AddFormatText(&pos, "^");
        }
    }
    break;
    case STATE_MATCHING: {
        D3DXVECTOR3 pos(100.0, 200.0, 0.0);
        if (g_NetConfig.isHost && g_NetHostPrepareStatus == NET_HOST_PREPARE_RUNNING)
        {
            g_AsciiManager.AddFormatText(&pos, "PREPARING HOST...");
            pos.y += 30;
            g_AsciiManager.AddFormatText(&pos, "OPENING PORT %s", g_NetConfig.portStr);
        }
        else if (g_NetConfig.isHost && g_NetHostPrepareStatus == NET_HOST_PREPARE_READY_NO_UPNP)
        {
            g_AsciiManager.AddFormatText(&pos, "WAITING AT %s:%s...", g_NetConfig.ip, g_NetConfig.portStr);
            pos.y += 30;
            g_AsciiManager.AddFormatText(&pos, "(LOCAL: %s)", g_NetConfig.localIp);
            pos.y += 30;
            g_AsciiManager.color = COLOR_LIGHT_RED;
            g_AsciiManager.AddFormatText(&pos, "UPnP FAILED: FORWARD PORT %s MANUALLY", g_NetConfig.portStr);
            g_AsciiManager.color = COLOR_WHITE;
        }
        else if (g_NetConfig.isHost)
        {
            g_AsciiManager.AddFormatText(&pos, "WAITING AT %s:%s...", g_NetConfig.ip, g_NetConfig.portStr);
            pos.y += 30;
            g_AsciiManager.AddFormatText(&pos, "(LOCAL: %s)", g_NetConfig.localIp);
        }
        else
        {
            g_AsciiManager.AddFormatText(&pos, "CONNECTING TO %s:%s...", g_NetConfig.ip, g_NetConfig.portStr);
        }
        pos.y += 30;
        g_AsciiManager.AddFormatText(&pos, "PRESS ESC TO CANCEL");
    }
    break;
    default:
        menu->ChoosePracticeLevel();
    }
    if ((g_ShowCollision == 1 || g_ShowEnemyMarker == 1 || g_HitDisp == 1) &&
        (menu->gameState == STATE_CHARACTER_SELECT || menu->gameState == STATE_SHOT_SELECT ||
         menu->gameState == STATE_PRACTICE_LVL_SELECT))
    {
        DrawCollisionRecordWarning();
    }
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult MainMenu::LoadTitleAnm(MainMenu *menu)
{
    i32 i;

    g_Supervisor.LoadPbg3(3, TH_TL_DAT_FILE);
    for (i = ANM_FILE_SELECT01; i <= ANM_FILE_REPLAY; i++)
    {
        g_AnmManager->ReleaseAnm(i);
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_TITLE01, "data/title01.anm", ANM_OFFSET_TITLE01))
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_TITLE02, "data/title02.anm", ANM_OFFSET_TITLE02))
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_TITLE03, "data/title03.anm", ANM_OFFSET_TITLE03))
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_TITLE04, "data/title04.anm", ANM_OFFSET_TITLE04))
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_TITLE01S, "data/title01s.anm", ANM_OFFSET_TITLE01S))
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_TITLE04S, "data/title04s.anm", ANM_OFFSET_TITLE04S))
    {
        return ZUN_ERROR;
    }

    for (i = 0; i < 80; i++)
    {
        g_AnmManager->ExecuteAnmIdx(&menu->vm[i], ANM_SCRIPT_TITLE01_START + i);
        menu->vm[i].flags.isVisible = 0;
        menu->vm[i].baseSpriteIndex = menu->vm[i].activeSpriteIndex;
        menu->vm[i].flags.zWriteDisable = 1;
    }

    {
        static bool s_optionLabelLogged = false;
        if (!s_optionLabelLogged)
        {
            s_optionLabelLogged = true;
            for (i = 72; i < 82; i++)
            {
                AnmVm *v = &menu->vm[i];
                if (v->sprite == NULL)
                {
                    th06::utils::LogDebug("optionLabel vm[%d]: sprite==NULL base=%d", i, (int)v->baseSpriteIndex);
                }
                else
                {
                    th06::utils::LogDebug(
                        "optionLabel vm[%d]: idx=%d srcFile=%d rect=(%.0f,%.0f)-(%.0f,%.0f) uv=(%.3f,%.3f)-(%.3f,%.3f) "
                        "tw=%.0f th=%.0f",
                        i, (int)v->baseSpriteIndex, (int)v->sprite->sourceFileIndex, (float)v->sprite->startPixelInclusive.x,
                        (float)v->sprite->startPixelInclusive.y, (float)v->sprite->endPixelInclusive.x,
                        (float)v->sprite->endPixelInclusive.y, (float)v->sprite->uvStart.x,
                        (float)v->sprite->uvStart.y, (float)v->sprite->uvEnd.x, (float)v->sprite->uvEnd.y,
                        (float)v->sprite->textureWidth, (float)v->sprite->textureHeight);
                }
            }
        }
    }

    if (g_AnmManager->LoadSurface(0, "data/title/title00.jpg"))
    {
        return ZUN_ERROR;
    }

    return ZUN_SUCCESS;
}

ZunResult MainMenu::LoadDiffCharSelect(MainMenu *menu)
{
    AnmVm *vm;
    i32 i;

    for (i = ANM_FILE_TITLE01; i <= ANM_FILE_TITLE04; i++)
    {
        g_AnmManager->ReleaseAnm(i);
    }
    if (g_AnmManager->LoadSurface(0, "data/title/select00.jpg") != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SELECT01, "data/select01.anm", ANM_OFFSET_SELECT01) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SELECT02, "data/select02.anm", ANM_OFFSET_SELECT02) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SELECT03, "data/select03.anm", ANM_OFFSET_SELECT03) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SELECT04, "data/select04.anm", ANM_OFFSET_SELECT04) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SELECT05, "data/select05.anm", ANM_OFFSET_SELECT05) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SLPL00A, "data/slpl00a.anm", ANM_OFFSET_SLPL00A) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SLPL00B, "data/slpl00b.anm", ANM_OFFSET_SLPL00B) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SLPL01A, "data/slpl01a.anm", ANM_OFFSET_SLPL01A) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    if (g_AnmManager->LoadAnm(ANM_FILE_SLPL01B, "data/slpl01b.anm", ANM_OFFSET_SLPL01B) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }
    for (vm = &menu->vm[0x50], i = ANM_SCRIPT_SELECT01_START; i <= ANM_SCRIPT_SELECT01_END; i++, vm++)
    {
        g_AnmManager->ExecuteAnmIdx(vm, i);
        vm->flags.isVisible = 0;
        vm->flags.colorOp = AnmVmColorOp_Add;
        if (((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1) == 0)
        {
            vm->color = COLOR_BLACK;
        }
        else
        {
            vm->color = COLOR_WHITE;
        }
        vm->posOffset = D3DXVECTOR3(0, 0, 0);
        vm->baseSpriteIndex = vm->activeSpriteIndex;
        vm->flags.zWriteDisable = 1;
    }
    return ZUN_SUCCESS;
}

ZunResult MainMenu::LoadReplayMenu(MainMenu *menu)
{
    AnmVm *vm;
    i32 fileIdx;

    for (fileIdx = ANM_FILE_TITLE01; fileIdx <= ANM_FILE_TITLE04; fileIdx++)
    {
        g_AnmManager->ReleaseAnm(fileIdx);
    }

    if (g_AnmManager->LoadSurface(0, "data/title/select00.jpg") != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }

    if (g_AnmManager->LoadAnm(ANM_FILE_REPLAY, "data/replay00.anm", ANM_OFFSET_REPLAY) != ZUN_SUCCESS)
    {
        return ZUN_ERROR;
    }

    vm = &menu->vm[96];
    for (fileIdx = ANM_SCRIPT_REPLAY_START; fileIdx <= ANM_SCRIPT_REPLAY_END; fileIdx++, vm++)
    {
        g_AnmManager->ExecuteAnmIdx(vm, fileIdx);
        vm->flags.isVisible = 0;
        vm->flags.colorOp = AnmVmColorOp_Add;

        if ((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING & 1) == 0)
        {
            vm->color = COLOR_BLACK;
        }
        else
        {
            vm->color = COLOR_WHITE;
        }
        vm->posOffset = D3DXVECTOR3(0, 0, 0);
        vm->baseSpriteIndex = vm->activeSpriteIndex;
        vm->flags.zWriteDisable = 1;
    }
    return ZUN_SUCCESS;
}

ZunResult MainMenu::RegisterChain(u32 isDemo)
{
    MainMenu *menu = &g_MainMenu;

    memset(menu, 0, sizeof(MainMenu));
    g_GameManager.isInGameMenu = 0;
    g_GameManager.isInReplay = 0;
    utils::DebugPrint(TH_DBG_MAINMENU_VRAM, g_Supervisor.d3dDevice->GetAvailableTextureMem());
    menu->gameState = isDemo ? STATE_REPLAY_LOAD : STATE_STARTUP;
    g_Supervisor.framerateMultiplier = 0.0;
    menu->chainCalc = g_Chain.CreateElem((ChainCallback)MainMenu::OnUpdate);
    menu->chainCalc->arg = menu;
    menu->chainCalc->addedCallback = (ChainAddedCallback)MainMenu::AddedCallback;
    menu->chainCalc->deletedCallback = (ChainDeletedCallback)MainMenu::DeletedCallback;
    menu->stateTimer = 0;
    if (g_Chain.AddToCalcChain(menu->chainCalc, TH_CHAIN_PRIO_CALC_MAINMENU) != 0)
    {
        return ZUN_ERROR;
    }
    menu->chainDraw = g_Chain.CreateElem((ChainCallback)MainMenu::OnDraw);
    menu->chainDraw->arg = menu;
    g_Chain.AddToDrawChain(menu->chainDraw, TH_CHAIN_PRIO_DRAW_MAINMENU);
    g_SpellPracticeDrawChain.callback = (ChainCallback)DrawSpellPracticeDllMenu;
    g_SpellPracticeDrawChain.addedCallback = NULL;
    g_SpellPracticeDrawChain.deletedCallback = NULL;
    g_SpellPracticeDrawChain.arg = NULL;
    g_Chain.AddToDrawChain(&g_SpellPracticeDrawChain, TH_CHAIN_PRIO_DRAW_ASCIIMANAGER_MENUS + 1);
    menu->lastFrameTime = 0;
    menu->stateTimer = 60;
    menu->frameCountForRefreshRateCalc = 0;
    return ZUN_SUCCESS;
}

ZunResult MainMenu::AddedCallback(MainMenu *m)
{
    i32 i;
    ScoreDat *scoredat;
    AnmManager *anmmgr;

    if (g_GameManager.demoMode == 0)
    {
        g_Supervisor.SetupMidiPlayback("bgm/th06_01.mid");
    }

    anmmgr = g_AnmManager;

    for (i = 0; i < ANM_OFFSET_TITLE01S - ANM_OFFSET_TITLE01; i++)
    {
        anmmgr->scripts[i + ANM_OFFSET_TITLE01] = NULL;
    }
    m->unk_81e4 = 0;

    switch (g_Supervisor.wantedState2)
    {
    case SUPERVISOR_STATE_GAMEMANAGER:
    case SUPERVISOR_STATE_GAMEMANAGER_REINIT:
    case SUPERVISOR_STATE_RESULTSCREEN_FROMGAME:
        m->cursor = g_GameManager.difficulty == EXTRA;
        break;
    case SUPERVISOR_STATE_RESULTSCREEN:
        m->cursor = 4;
        break;
    case SUPERVISOR_STATE_MUSICROOM:
        m->cursor = 5;
        break;
    case SUPERVISOR_STATE_INIT:
    case SUPERVISOR_STATE_MAINMENU:
    default:
        m->cursor = 0;
    }

    if (g_GameManager.isInPracticeMode != 0)
    {
        m->cursor = 2;
    }

    g_GameManager.isInPracticeMode = 0;
    g_GameManager.isInReplay = 0;
    if ((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING & 1) == 0)
    {
        m->color1 = 0x80004000;
        m->color2 = 0xff008000;
    }
    else
    {
        m->color1 = 0x80ffffff;
        m->color2 = 0xffffffff;
    }
    m->minimumOpacity = 0;
    m->menuTextColor = 0x40000000;
    m->numFramesSinceActive = 0;
    m->framesActive = 0;
    m->unk_10f28 = 0x10;
    m->currentReplay = NULL;
    scoredat = ResultScreen::OpenScore("score.dat");
    ResultScreen::ParseClrd(scoredat, g_GameManager.clrd);
    ResultScreen::ParsePscr(scoredat, (Pscr *)g_GameManager.pscr);
    ResultScreen::ReleaseScoreDat(scoredat);
    if (g_GameManager.demoMode == 0)
    {
        if (g_Supervisor.startupTimeBeforeMenuMusic == 0)
        {
            g_Supervisor.PlayAudio("bgm/th06_01.mid");
            if (g_Supervisor.wantedState2 != SUPERVISOR_STATE_RESULTSCREEN &&
                g_Supervisor.wantedState2 != SUPERVISOR_STATE_MUSICROOM)
            {
                ScreenEffect::RegisterChain(SCREEN_EFFECT_FADE_IN, 120, 0xffffff, 0, 0);
            }
        }
        else
        {
            ScreenEffect::RegisterChain(SCREEN_EFFECT_FADE_IN, 200, 0xffffff, 0, 0);
        }
    }
    g_GameManager.demoMode = 0;
    g_GameManager.demoFrames = 0;
    g_GameManager.currentStage = 0;
    return ZUN_SUCCESS;
}

ZunResult MainMenu::DeletedCallback(MainMenu *menu)
{
    UnloadSpellPracticeDll();
    AnmManager *mgr;
    void *replay;
    i32 i1, i2;

    g_Supervisor.d3dDevice->ResourceManagerDiscardBytes(0);
    MainMenu::ReleaseTitleAnm();
    for (i1 = ANM_FILE_SELECT01; i1 <= ANM_FILE_REPLAY; i1++)
    {
        g_AnmManager->ReleaseAnm(i1);
    }
    g_AnmManager->ReleaseSurface(0);

    mgr = g_AnmManager;
    for (i2 = 0; i2 < ANM_OFFSET_TITLE01S - ANM_OFFSET_TITLE01; i2++)
    {
        mgr->scripts[ANM_OFFSET_TITLE01 + i2] = NULL;
    }
    g_Chain.Cut(menu->chainDraw);
    menu->chainDraw = NULL;
    g_Chain.Cut(&g_SpellPracticeDrawChain);

    replay = menu->currentReplay;
    free(replay);
    return ZUN_SUCCESS;
}

void MainMenu::ReleaseTitleAnm()
{

    for (i32 i = ANM_FILE_TITLE01; i <= ANM_FILE_SELECT01; i++)
    {
        g_AnmManager->ReleaseAnm(i);
    }
}

MainMenu g_MainMenu;

}; // namespace th06
