#include "GameManager.hpp"
#include "AsciiManager.hpp"
#include "BulletManager.hpp"
#include "ChainPriorities.hpp"
#include "EclManager.hpp"
#include "EffectManager.hpp"
#include "EnemyManager.hpp"
#include "Gui.hpp"
#include "Player.hpp"
#include "ReplayManager.hpp"
#include "ResultScreen.hpp"
#include "Rng.hpp"
#include "ScreenEffect.hpp"
#include "SoundPlayer.hpp"
#include "SpellPracticeDllApi.hpp"
#include "Stage.hpp"
#include "Supervisor.hpp"
#include "utils.hpp"
#include <stdio.h>

namespace th06
{
namespace EnemyEclInstr
{
extern i32 g_PlayerShot;
}
} // namespace th06

#include "GameExtractedData.hpp"
#include <d3d8types.h>
#include <d3dx8math.h>

namespace th06
{

char *g_EclFiles[9] = {"dummy",
                       "data/ecldata1.ecl",
                       "data/ecldata2.ecl",
                       "data/ecldata3.ecl",
                       "data/ecldata4.ecl",
                       "data/ecldata5.ecl",
                       "data/ecldata6.ecl",
                       "data/ecldata7.ecl",
                       NULL};

struct AnmStageFiles
{
    char *file1;
    char *file2;
};

AnmStageFiles g_AnmStageFiles[8] = {
    {"dummy", "dummy"},
    {"data/stg1enm.anm", "data/stg1enm2.anm"},
    {"data/stg2enm.anm", "data/stg2enm2.anm"},
    {"data/stg3enm.anm", NULL},
    {"data/stg4enm.anm", NULL},
    {"data/stg5enm.anm", "data/stg5enm2.anm"},
    {"data/stg6enm.anm", "data/stg6enm2.anm"},
    {"data/stg7enm.anm", "data/stg7enm2.anm"},
};
struct DifficultyInfo
{
    u32 rank;
    u32 minRank;
    u32 maxRank;
};
ZUN_ASSERT_SIZE(DifficultyInfo, 0xc);

DifficultyInfo g_DifficultyInfoForReplay[5] = {

    /* */ {16, 12, 20},
    /* */ {16, 10, 32},
    /* */ {16, 10, 32},
    /* */ {16, 10, 32},
    /* */ {16, 14, 18},
};

DifficultyInfo g_DifficultyInfo[5] = {

    /* */ {16, 12, 20},
    /* */ {16, 10, 32},
    /* */ {16, 10, 32},
    /* */ {16, 10, 32},
    /* */ {16, 14, 18},
};

GameManager g_GameManager;

ChainElem g_GameManagerCalcChain;
ChainElem g_GameManagerDrawChain;

#define MAX_SCORE 999999999

#define DEMO_FADEOUT_FRAMES 3600
#define DEMO_FRAMES 3720

#define GUI_SCORE_STEP 78910

#define MAX_LIVES 8

i32 GameManager::IsInBounds(f32 x, f32 y, f32 width, f32 height)
{
    if (width / 2.0f + x < 0.0f)
    {
        return false;
    }
    if ((x - width / 2.0f) > g_GameManager.arcadeRegionSize.x)
    {
        return false;
    }
    if (height / 2.0f + y < 0.0f)
    {
        return false;
    }
    if (y - height / 2.0f > g_GameManager.arcadeRegionSize.y)
    {
        return false;
    }

    return true;
}

ChainCallbackResult GameManager::OnUpdate(GameManager *gameManager)
{
    u32 isInMenu;
    u32 scoreIncrement;

    static u32 lastMenuState = 0;

    if (lastMenuState != 0 && gameManager->isInGameMenu == 0)
    {
        g_LastFrameInput = 0;
        g_CurFrameInput = 0;
        g_NumOfFramesInputsWereHeld = 0;
        g_IsEigthFrameOfHeldInput = 0;
    }
    lastMenuState = gameManager->isInGameMenu;
    if (g_GameManager.isInReplay && !gameManager->demoMode)
    {
        static bool ctrlWasDown = false;
        static bool shiftWasDown = false;

        bool ctrlDown = (g_RealCurFrameInput & TH_BUTTON_SKIP) != 0;
        if (ctrlDown)
        {
            if (!ctrlWasDown)
            {
                g_SavedReplaySpeed = 2;
                if (g_Supervisor.midiOutput != NULL && Supervisor::ActiveMusicMode() == MIDI)
                {
                    g_Supervisor.midiOutput->StopTimer();
                }
            }
            else
            {
                bool shiftDown = (g_RealCurFrameInput & TH_BUTTON_FOCUS) != 0;
                if (shiftDown && !shiftWasDown)
                {
                    if (g_SavedReplaySpeed < 5)
                    {
                        g_SavedReplaySpeed++;
                    }
                    else
                    {
                        g_SavedReplaySpeed = 2;
                    }
                }
                shiftWasDown = shiftDown;
            }
            g_ReplaySpeed = g_SavedReplaySpeed;
            g_ReplayFastForwardActive = 1;
        }
        else
        {
            shiftWasDown = false;
            if (ctrlWasDown)
            {
                if (g_Supervisor.midiOutput != NULL && Supervisor::ActiveMusicMode() == MIDI)
                {
                    g_Supervisor.midiOutput->StartTimer(1, NULL, NULL);
                }
            }
            g_ReplaySpeed = 1;
            g_ReplayFastForwardActive = 0;
        }
        ctrlWasDown = ctrlDown;
    }

    static bool s_qsWasOpen = false;
    static bool s_qsSkipDemo = false;
    s_qsSkipDemo = Supervisor::IsQuickSettingsOpen() || s_qsWasOpen;
    s_qsWasOpen = Supervisor::IsQuickSettingsOpen();
    if (gameManager->demoMode && !s_qsSkipDemo)
    {
        if (g_RealCurFrameInput & TH_BUTTON_ANY)
        {
            g_Supervisor.curState = SUPERVISOR_STATE_MAINMENU;
        }
        gameManager->demoFrames++;
        if (gameManager->demoFrames == DEMO_FADEOUT_FRAMES)
        {
            ScreenEffect::RegisterChain(SCREEN_EFFECT_FADE_OUT, 120, 0x000000, 0, 0);
        }
        if (gameManager->demoFrames >= DEMO_FRAMES)
        {
            g_Supervisor.curState = SUPERVISOR_STATE_MAINMENU;
        }
    }
    if (!gameManager->isInRetryMenu && !gameManager->isInGameMenu && WAS_PRESSED(TH_BUTTON_MENU))
    {
        GameManager::OpenPauseMenu();
    }

    if (!gameManager->isInRetryMenu && !gameManager->isInGameMenu)
    {
        isInMenu = 1;
    }
    else
    {
        isInMenu = 0;
    }

    gameManager->isInMenu = isInMenu;

    g_Supervisor.viewport.X = gameManager->arcadeRegionTopLeftPos.x;
    g_Supervisor.viewport.Y = gameManager->arcadeRegionTopLeftPos.y;
    g_Supervisor.viewport.Width = gameManager->arcadeRegionSize.x;
    g_Supervisor.viewport.Height = gameManager->arcadeRegionSize.y;
    g_Supervisor.viewport.MinZ = 0.5;
    g_Supervisor.viewport.MaxZ = 1.0;

    SetupCamera(0);

    g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
    g_Supervisor.d3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, g_Stage.skyFog.color, 1.0, 0);

    if ((gameManager->isInGameMenu != 0 || gameManager->isInRetryMenu) && !Supervisor::IsQuickSettingsOpen())
    {
        return CHAIN_CALLBACK_RESULT_BREAK;
    }

    if (Supervisor::IsQuickSettingsOpen())
    {
        return CHAIN_CALLBACK_RESULT_BREAK;
    }
    if (g_SpellPracticeResultDelay > 1)
    {
        g_SpellPracticeResultDelay--;
        if (g_SpellPracticeResultDelay == 1)
        {
            g_SpellPracticeResultDelay = 0;
            if (g_GameManager.isInReplay)
                g_Supervisor.curState = SUPERVISOR_STATE_MAINMENU_REPLAY;
            else
            {
                ResultScreen::SaveCatk();
                g_Supervisor.curState = SUPERVISOR_STATE_RESULTSCREEN_FROMGAME;
            }
        }
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }

    if (g_ReturnToSpellPractice > 2)
    {
        g_ReturnToSpellPractice--;
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    if (g_ReturnToSpellPractice == 2)
    {
        g_ReturnToSpellPractice = 1;
        g_Supervisor.curState = SUPERVISOR_STATE_MAINMENU;
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }

    if (g_ReturnToSpellPractice > 1)
    {
        g_ReturnToSpellPractice--;
        if (g_ReturnToSpellPractice == 1)
        {
            g_Supervisor.curState = SUPERVISOR_STATE_MAINMENU;
        }
    }

    if (gameManager->score >= MAX_SCORE + 1)
    {
        gameManager->score = MAX_SCORE - 9;
    }
    if (gameManager->guiScore != gameManager->score)
    {
        if (gameManager->score < gameManager->guiScore)
        {
            gameManager->score = gameManager->guiScore;
        }

        scoreIncrement = (gameManager->score - gameManager->guiScore) >> 5;
        if (scoreIncrement >= GUI_SCORE_STEP)
        {
            scoreIncrement = GUI_SCORE_STEP;
        }
        else if (scoreIncrement < 10)
        {
            scoreIncrement = 10;
        }
        scoreIncrement = scoreIncrement - scoreIncrement % 10;

        if (gameManager->nextScoreIncrement < scoreIncrement)
        {
            gameManager->nextScoreIncrement = scoreIncrement;
        }
        if (gameManager->guiScore + gameManager->nextScoreIncrement > gameManager->score)
        {
            gameManager->nextScoreIncrement = gameManager->score - gameManager->guiScore;
        }

        gameManager->guiScore += gameManager->nextScoreIncrement;
        if (gameManager->guiScore >= gameManager->score)
        {
            gameManager->nextScoreIncrement = 0;
            gameManager->guiScore = gameManager->score;
        }
        if (gameManager->extraLives >= 0 && g_ExtraLivesScores[gameManager->extraLives] <= gameManager->guiScore)
        {
            if (gameManager->livesRemaining < MAX_LIVES)
            {
                gameManager->livesRemaining++;
                g_SoundPlayer.PlaySoundByIdx(SOUND_1UP, 0);
            }
            g_Gui.flags.flag0 = 2;
            gameManager->extraLives++;
            g_GameManager.IncreaseSubrank(200);
        }
        if (gameManager->highScore < gameManager->guiScore)
        {
            gameManager->highScore = gameManager->guiScore;
        }
    }
    gameManager->gameFrames++;
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

void GameManager::OpenPauseMenu()
{
    GameManager *gameManager = &g_GameManager;

    if (g_GameManager.isInReplay && !gameManager->demoMode && g_ReplayFastForwardActive)
    {
        g_ReplaySpeed = 1;
        g_SavedReplaySpeed = 1;
        g_ReplayFastForwardActive = 0;
        if (g_Supervisor.midiOutput != NULL && Supervisor::ActiveMusicMode() == MIDI)
        {
            g_Supervisor.midiOutput->StartTimer(1, NULL, NULL);
        }
    }
    gameManager->isInGameMenu = 1;
    g_GameManager.arcadeRegionTopLeftPos.x = GAME_REGION_LEFT;
    g_GameManager.arcadeRegionTopLeftPos.y = GAME_REGION_TOP;
    g_GameManager.arcadeRegionSize.x = GAME_REGION_WIDTH;
    g_GameManager.arcadeRegionSize.y = GAME_REGION_HEIGHT;
    g_Supervisor.unk198 = 3;
}

ChainCallbackResult GameManager::OnDraw(GameManager *gameManager)
{
    if (gameManager->isInGameMenu)
    {
        gameManager->isInGameMenu = 2;
    }
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult GameManager::RegisterChain()
{
    GameManager *mgr = &g_GameManager;

    g_GameManagerCalcChain.callback = (ChainCallback)GameManager::OnUpdate;
    g_GameManagerCalcChain.addedCallback = NULL;
    g_GameManagerCalcChain.deletedCallback = NULL;
    g_GameManagerCalcChain.addedCallback = (ChainAddedCallback)GameManager::AddedCallback;
    g_GameManagerCalcChain.deletedCallback = (ChainDeletedCallback)GameManager::DeletedCallback;
    g_GameManagerCalcChain.arg = mgr;

    mgr->gameFrames = 0;

    if (g_Chain.AddToCalcChain(&g_GameManagerCalcChain, TH_CHAIN_PRIO_CALC_GAMEMANAGER))
    {
        return ZUN_ERROR;
    }
    g_GameManagerDrawChain.callback = (ChainCallback)GameManager::OnDraw;
    g_GameManagerDrawChain.addedCallback = NULL;
    g_GameManagerDrawChain.deletedCallback = NULL;
    g_GameManagerDrawChain.arg = mgr;
    g_Chain.AddToDrawChain(&g_GameManagerDrawChain, TH_CHAIN_PRIO_DRAW_GAMEMANAGER);
    return ZUN_SUCCESS;
}

static void GameManagerSpellRetryMergeCatk(GameManager *mgr, ScoreDat *scoredat)
{
    i32 i;
    Catk *catk;
    u32 catkCursor;

    u16 *savedAttempts = (u16 *)malloc(CATK_NUM_CAPTURES * sizeof(u16));
    u16 *savedSuccess = (u16 *)malloc(CATK_NUM_CAPTURES * sizeof(u16));
    u8 *savedNameCsum = (u8 *)malloc(CATK_NUM_CAPTURES * sizeof(u8));
    char (*savedName)[32] = (char (*)[32])malloc(CATK_NUM_CAPTURES * 32);
    for (i = 0; i < CATK_NUM_CAPTURES; i++)
    {
        savedAttempts[i] = mgr->catk[i].numAttempts;
        savedSuccess[i] = mgr->catk[i].numSuccess;
        savedNameCsum[i] = mgr->catk[i].nameCsum;
        memcpy(savedName[i], mgr->catk[i].name, 32);
    }
    for (catk = mgr->catk, i = 0; i < ARRAY_SIZE_SIGNED(mgr->catk); i++, catk++)
    {
        for (catkCursor = 0; catkCursor < sizeof(Catk) / sizeof(u16); catkCursor++)
        {
            ((u16 *)catk)[catkCursor] = g_Rng.GetRandomU16();
        }
        catk->base.magic = CATK_MAGIC;
        catk->base.unkLen = sizeof(Catk);
        catk->base.th6kLen = sizeof(Catk);
        catk->base.version = TH6K_VERSION;
        catk->idx = i;
        catk->numAttempts = 0;
        catk->numSuccess = 0;
    }
    ResultScreen::ParseCatk(scoredat, mgr->catk);
    for (i = 0; i < CATK_NUM_CAPTURES; i++)
    {
        if (savedAttempts[i] > mgr->catk[i].numAttempts)
            mgr->catk[i].numAttempts = savedAttempts[i];
        if (savedSuccess[i] > mgr->catk[i].numSuccess)
            mgr->catk[i].numSuccess = savedSuccess[i];

        if (savedNameCsum[i] != 0)
        {
            mgr->catk[i].nameCsum = savedNameCsum[i];
            memcpy(mgr->catk[i].name, savedName[i], 32);
        }
    }
    free(savedAttempts);
    free(savedSuccess);
    free(savedNameCsum);
    free(savedName);
}

static bool GetFrameStartBgmFrame(i32 section, i32 stage, i32 *outFrame)
{

    static const i32 bossStartFrame[8] = {0, 5279, 5984, 5844, 10695, 7704, 3169, 8594};
    i32 frame;

    if (section < SPELL_PRACTICE_FRAME_SECTION_BASE || stage < 1 || stage > 7)
    {
        return false;
    }
    frame = section - SPELL_PRACTICE_FRAME_SECTION_BASE;
    if (frame <= 0 || frame >= bossStartFrame[stage])
    {
        return false;
    }
    *outFrame = frame;
    return true;
}

ZunResult GameManager::AddedCallback(GameManager *mgr)
{
    ScoreDat *scoredat;
    u32 clrdIdx;
    u32 catkCursor;
    i32 i;
    Catk *catk;
    ZunBool failedToLoadReplay;
    i32 padding[3];

    failedToLoadReplay = false;
    g_Supervisor.d3dDevice->ResourceManagerDiscardBytes(0);
    if (g_Supervisor.curState != SUPERVISOR_STATE_GAMEMANAGER_REINIT)
    {
        g_Supervisor.defaultConfig.bombCount = g_GameManager.bombsRemaining;
        g_Supervisor.defaultConfig.lifeCount = g_GameManager.livesRemaining;
        mgr->arcadeRegionTopLeftPos.x = 32.0;
        mgr->arcadeRegionTopLeftPos.y = 16.0;
        mgr->arcadeRegionSize.x = 384.0;
        mgr->arcadeRegionSize.y = 448.0;
        mgr->playerMovementAreaTopLeftPos.x = 8.0;
        mgr->playerMovementAreaTopLeftPos.y = 16.0;
        mgr->playerMovementAreaSize.x = 368.0;
        mgr->playerMovementAreaSize.y = 416.0;
        mgr->counat = 0;
        mgr->guiScore = 0;
        mgr->score = 0;
        mgr->nextScoreIncrement = 0;
        mgr->highScore = 100000;
        mgr->currentPower = 0;
        mgr->numRetries = 0;
        if (mgr->currentStage >= 6)
        {
            mgr->difficulty = EXTRA;
        }
        if (mgr->difficulty < EXTRA)
        {
            mgr->extraLives = 0;
        }
        else
        {
            mgr->extraLives = 4;
        }
        g_GameManager.powerItemCountForScore = 0;
        mgr->rank = 8;
        mgr->grazeInTotal = 0;
        mgr->pointItemsCollected = 0;
        scoredat = ResultScreen::OpenScore("score.dat");
        g_GameManager.highScore =
            ResultScreen::GetHighScore(scoredat, NULL, g_GameManager.CharacterShotType(), g_GameManager.difficulty);
        if (g_IsSpellPracticeGame == 0)
        {

            for (catk = mgr->catk, i = 0; i < ARRAY_SIZE_SIGNED(mgr->catk); i++, catk++)
            {

                for (catkCursor = 0; catkCursor < sizeof(Catk) / sizeof(u16); catkCursor++)
                {
                    ((u16 *)catk)[catkCursor] = g_Rng.GetRandomU16();
                }
                catk->base.magic = CATK_MAGIC;
                catk->base.unkLen = sizeof(Catk);
                catk->base.th6kLen = sizeof(Catk);
                catk->base.version = TH6K_VERSION;
                catk->idx = i;
                catk->numAttempts = 0;
                catk->numSuccess = 0;
            }
            ResultScreen::ParseCatk(scoredat, mgr->catk);
        }
        else
        {

            GameManagerSpellRetryMergeCatk(mgr, scoredat);
        }
        ResultScreen::ParseClrd(scoredat, mgr->clrd);
        ResultScreen::ParsePscr(scoredat, (Pscr *)mgr->pscr);
        if (mgr->isInPracticeMode != 0 && g_GameManager.currentStage < PSCR_NUM_STAGES)
        {
            g_GameManager.highScore =
                mgr->pscr[g_GameManager.CharacterShotType()][g_GameManager.currentStage][g_GameManager.difficulty]
                    .score;
        }
        ResultScreen::ReleaseScoreDat(scoredat);
        mgr->rank = (mgr->isInPracticeMode != 0 && g_SpellPracticeRank >= 1)
                        ? g_SpellPracticeRank
                        : g_DifficultyInfo[g_GameManager.difficulty].rank;
        mgr->minRank = g_DifficultyInfo[g_GameManager.difficulty].minRank;
        mgr->maxRank = g_DifficultyInfo[g_GameManager.difficulty].maxRank;
        mgr->deaths = 0;
        mgr->bombsUsed = 0;
        mgr->spellcardsCaptured = 0;
    }
    else
    {
        mgr->guiScore = mgr->score;
        mgr->nextScoreIncrement = 0;

        while (mgr->extraLives >= 0 && mgr->extraLives < ARRAY_SIZE_SIGNED(g_ExtraLivesScores) &&
               g_ExtraLivesScores[mgr->extraLives] <= mgr->guiScore)
        {
            if (mgr->livesRemaining < MAX_LIVES)
            {
                mgr->livesRemaining++;
            }
            mgr->extraLives++;
        }
    }
    mgr->subRank = 0;
    mgr->pointItemsCollectedInStage = 0;
    mgr->grazeInStage = 0;
    mgr->isInGameMenu = 0;
    mgr->currentStage = mgr->currentStage + 1;
    if (g_GameManager.isInReplay == 0)
    {
        clrdIdx = g_GameManager.CharacterShotType();
        if (mgr->numRetries == 0 &&
            mgr->clrd[clrdIdx].difficultyClearedWithRetries[g_GameManager.difficulty] < mgr->currentStage - 1)
        {
            mgr->clrd[clrdIdx].difficultyClearedWithRetries[g_GameManager.difficulty] = mgr->currentStage - 1;
        }
        if (mgr->clrd[clrdIdx].difficultyClearedWithoutRetries[g_GameManager.difficulty] < mgr->currentStage - 1)
        {
            mgr->clrd[clrdIdx].difficultyClearedWithoutRetries[g_GameManager.difficulty] = mgr->currentStage - 1;
        }
    }
    if (mgr->isInPracticeMode != 0)
    {
        switch (mgr->currentStage)
        {
        case STAGE2:
            break;
        case STAGE3:
            mgr->currentPower = 64;
            break;
        default:
            mgr->currentPower = 128;
        }
    }
    else if (g_IsSpellPracticeGame == 0 && g_Supervisor.curState != SUPERVISOR_STATE_GAMEMANAGER_REINIT)
    {
        mgr->currentPower = 0;
    }
    g_Supervisor.LoadPbg3(CM_PBG3_INDEX, TH_CM_DAT_FILE);
    g_Supervisor.LoadPbg3(ST_PBG3_INDEX, TH_ST_DAT_FILE);
    if (g_GameManager.isInReplay == 1)
    {
        if (ReplayManager::RegisterChain(1, g_GameManager.replayFile) != ZUN_SUCCESS)
        {
            failedToLoadReplay = true;
        }
        while (g_ExtraLivesScores[mgr->extraLives] <= mgr->guiScore)
        {
            mgr->extraLives++;
        }
        if (mgr->minRank != mgr->maxRank)
        {
            mgr->minRank = g_DifficultyInfoForReplay[g_GameManager.difficulty].minRank;
            mgr->maxRank = g_DifficultyInfoForReplay[g_GameManager.difficulty].maxRank;
        }
    }
    if (g_IsOnline && g_GameManager.isInReplay == 0)
    {
        g_Rng.Initialize(g_NetRandomSeed);
    }
    g_Rng.generationCount = 0;
    mgr->randomSeed = g_Rng.seed;
    if (Stage::RegisterChain(mgr->currentStage) != ZUN_SUCCESS)
    {
        th06::utils::LogDebug("AddedCallback: Stage::RegisterChain failed");
        g_GameErrorContext.Log(TH_ERR_GAMEMANAGER_FAILED_TO_INITIALIZE_STAGE);
        return ZUN_ERROR;
    }

    if (Player::RegisterChain(0) != ZUN_SUCCESS)
    {
        th06::utils::LogDebug("AddedCallback: Player::RegisterChain failed");
        g_GameErrorContext.Log(TH_ERR_GAMEMANAGER_FAILED_TO_INITIALIZE_PLAYER);
        return ZUN_ERROR;
    }
    if (BulletManager::RegisterChain("data/etama.anm") != ZUN_SUCCESS)
    {
        th06::utils::LogDebug("AddedCallback: BulletManager::RegisterChain failed");
        g_GameErrorContext.Log(TH_ERR_GAMEMANAGER_FAILED_TO_INITIALIZE_BULLETMANAGER);
        return ZUN_ERROR;
    }
    if (EnemyManager::RegisterChain(g_AnmStageFiles[mgr->currentStage].file1,
                                    g_AnmStageFiles[mgr->currentStage].file2) != ZUN_SUCCESS)
    {
        th06::utils::LogDebug("AddedCallback: EnemyManager::RegisterChain failed");
        g_GameErrorContext.Log(TH_ERR_GAMEMANAGER_FAILED_TO_INITIALIZE_ENEMYMANAGER);
        return ZUN_ERROR;
    }
    if (g_EclManager.Load(g_EclFiles[mgr->currentStage]) != ZUN_SUCCESS)
    {
        th06::utils::LogDebug("AddedCallback: g_EclManager.Load failed");
        g_GameErrorContext.Log(TH_ERR_GAMEMANAGER_FAILED_TO_INITIALIZE_ECLMANAGER);
        return ZUN_ERROR;
    }
    if (g_IsSpellPracticeGame > 0)
    {

        HMODULE hDll = LoadLibraryA("th06_spell_practice.dll");
        if (hDll == NULL)
            hDll = LoadLibraryA("spell_practice.dll");
        SpellPracticeApplyEclPatchFn applyFn = NULL;
        if (hDll != NULL)
            applyFn = (SpellPracticeApplyEclPatchFn)GetProcAddress(hDll, "th06_spell_practice_apply_ecl_patch");
        th06::utils::LogDebug("SpellPractice: g_IsSpellPracticeGame=%d hDll=%p applyFn=%p eclFile=%p",
                              g_IsSpellPracticeGame, hDll, applyFn, g_EclManager.eclFile);
        if (applyFn != NULL)
        {

            int shot = mgr->character * 2 + mgr->shotType;
            int res = applyFn(g_IsSpellPracticeGame, g_EclManager.eclFile, shot);
            g_SpellPracticeWarpTime = res & 0xFFFF;
            g_SpellPracticeBossSubId = (res >> 16) & 0xFFFF;

            EnemyEclInstr::g_PlayerShot = shot;
            th06::utils::LogDebug("SpellPractice: res=%d warpTime=%d bossSubId=%d shot=%d", res,
                                  g_SpellPracticeWarpTime, g_SpellPracticeBossSubId, shot);
        }
        if (hDll != NULL)
            FreeLibrary(hDll);
    }
    else
    {

        g_SpellPracticeBossSubId = 0;
        g_SpellPracticeWarpTime = 0;
    }
    if (EffectManager::RegisterChain() != ZUN_SUCCESS)
    {
        g_GameErrorContext.Log(TH_ERR_GAMEMANAGER_FAILED_TO_INITIALIZE_EFFECTMANAGER);
        return ZUN_ERROR;
    }
    if (Gui::RegisterChain() != ZUN_SUCCESS)
    {
        g_GameErrorContext.Log(TH_ERR_GAMEMANAGER_FAILED_TO_INITIALIZE_GUI);
        return ZUN_ERROR;
    }
    if (g_GameManager.isInReplay == 0)
    {
        ReplayManager::RegisterChain(0, "replay/th6_00.rpy");
    }
    if (g_GameManager.demoMode == 0)
    {

        g_Supervisor.ReadMidiFile(1, g_Stage.stdData->songPaths[1]);

        {
            bool useBossBgm;
            if (g_ThpracSection > 0)
            {
                static const i8 thpracBossBgm[71] = {
                    /* */ 0,
                    /* */ 0, /* */ 0,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 0,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 0, /* */ 0,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 1, /* */ 1, /* */ 1,
                    /* */ 0, /* */ 0,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 1, /* */ 1, /* */ 1,
                    /* */ 0, /* */ 0,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 1, /* */ 1,
                    /* */ 0, /* */ 0,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 0, /* */ 0, /* */ 0,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 1, /* */ 1, /* */ 1, /* */ 1,
                    /* */ 1, /* */ 1,
                };
                i32 sec = g_ThpracSection;
                useBossBgm = (sec >= 1 && sec <= 70) ? (thpracBossBgm[sec] != 0) : false;
            }
            else
            {

                i32 s = g_IsSpellPracticeGame;
                bool isMidOrBooks = (s == 0 || s == 1 || s == 2 || s == 7 || s == 13 || s == 14 || s == 22 || s == 23 ||
                                     s == 31 || s == 32 || s == 39 || s == 40 || s == 50 || s == 51 || s == 52 ||
                                     s == 71 || s == 72 || s == 80 || s == 81 || s == 89 || s == 90 || s == 98 ||
                                     s == 99 || (s >= 107 && s <= 116) || s >= SPELL_PRACTICE_FRAME_SECTION_BASE);
                useBossBgm = !isMidOrBooks;
            }
            g_Supervisor.PlayAudio(useBossBgm ? g_Stage.stdData->songPaths[1] : g_Stage.stdData->songPaths[0]);
        }

        if (Supervisor::ActiveMusicMode() == WAV && g_SoundPlayer.backgroundMusic != NULL)
        {
            i32 s = g_IsSpellPracticeGame;
            u32 ms = 0;
            if (s == 51)
            {
                ms = 97000;
            }
            else if (s == 50)
            {
                ms = 86000;
            }
            else if (s == 52)
            {
                ms = (mgr->character == 1 && mgr->shotType == 0) ? 106000 : 113000;
            }
            else if (s == 2)
            {
                ms = 43000;
            }
            else if (s == 4)
            {
                ms = 8000;
            }
            else if (s == 6)
            {
                ms = 40000;
            }
            else if (s == 9 || s == 10)
            {
                ms = 15000;
            }
            else if (s == 11)
            {
                ms = 42000;
            }
            else if (s == 31)
            {
                ms = 57000;
            }
            else if (s == 32)
            {
                ms = 94000;
            }
            else if (s == 13)
            {
                ms = 63000;
            }
            else if (s == 14)
            {
                ms = 76000;
            }
            else if (s == 16)
            {
                ms = 11000;
            }
            else if (s == 17)
            {
                ms = 33000;
            }
            else if (s == 18)
            {
                ms = 41000;
            }
            else if (s == 19)
            {
                ms = (g_GameManager.difficulty <= NORMAL) ? 41000 : 55000;
            }
            else if (s == 20)
            {
                ms = 55000;
            }
            else if (s == 21)
            {
                ms = 82000;
            }
            else if (s == 34)
            {
                ms = 18000;
            }
            else if (s == 22 || s == 71 || s == 80 || s == 89 || s == 98)
            {
                ms = 57000;
            }
            else if (s == 75 || s == 84 || s == 93 || s == 102)
            {
                if (g_GameManager.difficulty == EASY)
                {
                    ms = 25000;
                }
            }
            else if (s == 76 || s == 85 || s == 94 || s == 103)
            {
                if (g_GameManager.difficulty == EASY)
                {
                    ms = 15000;
                }
            }
            else if (s == 86 || s == 105)
            {
                ms = 88000;
            }
            else if (s == 77 || s == 88 || s == 95 || s == 96 || s == 104)
            {
                ms = (g_GameManager.difficulty == EASY) ? 50000 : 64000;
            }
            else if ((s >= 74 && s <= 79) || (s >= 92 && s <= 97) || (s >= 101 && s <= 106))
            {
                if (g_GameManager.difficulty == EASY)
                {
                    ms = 50000;
                }
                else if (g_GameManager.difficulty == NORMAL)
                {
                    ms = 13000;
                }
            }
            th06::utils::LogDebug("BGMOffset: s=%d ms=%d", s, ms);
            if (ms > 0)
            {
                g_SoundPlayer.backgroundMusic->SeekToByte(44100 * 2 * 2 * (ms / 1000));
            }
            {

                i32 startFrame;
                if (GetFrameStartBgmFrame(s, mgr->currentStage, &startFrame))
                {
                    g_SoundPlayer.backgroundMusic->SeekToByte((u32)startFrame * 2940);
                }
            }
        }
        else if (Supervisor::ActiveMusicMode() == MIDI && g_Supervisor.midiOutput != NULL)
        {
            i32 s = g_IsSpellPracticeGame;
            u32 ms = 0;
            if (s == 51)
            {
                ms = 97000;
            }
            else if (s == 50)
            {
                ms = 86000;
            }
            else if (s == 52)
            {
                ms = (mgr->character == 1 && mgr->shotType == 0) ? 106000 : 113000;
            }
            else if (s == 2)
            {
                ms = 43000;
            }
            else if (s == 4)
            {
                ms = 8000;
            }
            else if (s == 6)
            {
                ms = 40000;
            }
            else if (s == 9 || s == 10)
            {
                ms = 15000;
            }
            else if (s == 11)
            {
                ms = 42000;
            }
            else if (s == 31)
            {
                ms = 57000;
            }
            else if (s == 32)
            {
                ms = 94000;
            }
            else if (s == 13)
            {
                ms = 63000;
            }
            else if (s == 14)
            {
                ms = 76000;
            }
            else if (s == 16)
            {
                ms = 11000;
            }
            else if (s == 17)
            {
                ms = 33000;
            }
            else if (s == 18)
            {
                ms = 41000;
            }
            else if (s == 19)
            {
                ms = (g_GameManager.difficulty <= NORMAL) ? 41000 : 55000;
            }
            else if (s == 20)
            {
                ms = 55000;
            }
            else if (s == 21)
            {
                ms = 82000;
            }
            else if (s == 34)
            {
                ms = 18000;
            }
            else if (s == 22 || s == 71 || s == 80 || s == 89 || s == 98)
            {
                ms = 57000;
            }
            else if (s == 75 || s == 84 || s == 93 || s == 102)
            {
                if (g_GameManager.difficulty == EASY)
                {
                    ms = 25000;
                }
            }
            else if (s == 76 || s == 85 || s == 94 || s == 103)
            {
                if (g_GameManager.difficulty == EASY)
                {
                    ms = 15000;
                }
            }
            else if (s == 86 || s == 105)
            {
                ms = 88000;
            }
            else if (s == 77 || s == 88 || s == 95 || s == 96 || s == 104)
            {
                ms = (g_GameManager.difficulty == EASY) ? 50000 : 64000;
            }
            else if ((s >= 74 && s <= 79) || (s >= 92 && s <= 97) || (s >= 101 && s <= 106))
            {
                if (g_GameManager.difficulty == EASY)
                {
                    ms = 50000;
                }
                else if (g_GameManager.difficulty == NORMAL)
                {
                    ms = 13000;
                }
            }
            if (ms > 0)
            {
                g_Supervisor.midiOutput->SeekToMs(ms);
            }
            {
                i32 startFrame;
                if (GetFrameStartBgmFrame(s, mgr->currentStage, &startFrame))
                {
                    g_Supervisor.midiOutput->SeekToMs((u32)startFrame * 50 / 3);
                }
            }
        }
        if (g_IsSpellPracticeGame != 0)
        {
            if (g_IsSpellPracticeGame == 2)
                mgr->currentPower = 30; // ST1_MID2 ムーンライトレイ
            else if (g_IsSpellPracticeGame == 4)
                mgr->currentPower = 72; // ST1_BOSS2 ナイトバード
            else if (g_IsSpellPracticeGame == 6)
                mgr->currentPower = 84; // ST1_BOSS4 ディマーケイション
            else
                mgr->currentPower = 128;
        }
    }
    mgr->isInRetryMenu = 0;
    mgr->isInMenu = 1;
    if (g_Supervisor.curState != SUPERVISOR_STATE_GAMEMANAGER_REINIT)
    {
        g_Supervisor.unk1b4 = 0.0;
        g_Supervisor.unk1b8 = 0.0;
    }
    mgr->isTimeStopped = false;
    mgr->score = 0;
    mgr->isGameCompleted = 0;
    g_AsciiManager.InitializeVms();
    if (failedToLoadReplay)
    {

        g_Supervisor.curState = (g_GameManager.isInReplay && g_ThpracSectionReplay) ? SUPERVISOR_STATE_MAINMENU_REPLAY
                                                                                    : SUPERVISOR_STATE_MAINMENU;
    }
    g_Supervisor.unk198 = 3;
    return ZUN_SUCCESS;
}

ZunResult GameManager::DeletedCallback(GameManager *mgr)
{
    i32 padding1, padding2, padding3;

    g_SpellPracticeResultDelay = 0;
    g_ThpracSectionReplay = 0;

    if (g_Supervisor.curState == SUPERVISOR_STATE_GAMEMANAGER_REINIT ||
        g_Supervisor.curState == SUPERVISOR_STATE_RESULTSCREEN_FROMGAME)
    {

        if (g_Supervisor.curState == SUPERVISOR_STATE_GAMEMANAGER_REINIT && g_IsSpellPracticeGame > 0 &&
            g_SpellPracticeBossSubId > 0)
        {
            ResultScreen::SaveCatk();
        }
    }
    else
    {
        g_IsSpellPracticeGame = 0;
        g_SpellPracticeWarpTime = 0;

        if (g_ReturnToSpellPractice == 0 && g_ReturnToPractice == 0)
        {
            g_ReturnToSpellPractice = 0;
        }
    }
    g_Supervisor.d3dDevice->ResourceManagerDiscardBytes(0);
    if (!g_GameManager.demoMode)
    {
        g_Supervisor.StopAudio();
    }
    Stage::CutChain();
    BulletManager::CutChain();
    Player::CutChain();
    EnemyManager::CutChain();
    g_EclManager.Unload();
    EffectManager::CutChain();
    Gui::CutChain();
    ReplayManager::StopRecording();
    mgr->isInMenu = 0;
    g_AsciiManager.InitializeVms();
    return ZUN_SUCCESS;
}

void GameManager::CutChain()
{
    g_Chain.Cut(&g_GameManagerCalcChain);
    g_Chain.Cut(&g_GameManagerDrawChain);
}

void GameManager::SetupCameraStageBackground(f32 extraRenderDistance)
{
    D3DXVECTOR3 eyeVec;
    D3DXVECTOR3 atVec;
    D3DXVECTOR3 upVec;
    f32 fov;
    f32 aspectRatio;
    f32 viewportMiddleWidth;
    f32 viewportMiddleHeight;
    f32 cameraDistance;

    viewportMiddleWidth = g_Supervisor.viewport.Width / 2.0f;
    viewportMiddleHeight = g_Supervisor.viewport.Height / 2.0f;
    aspectRatio = (f32)g_Supervisor.viewport.Width / (f32)g_Supervisor.viewport.Height;
    fov = D3DXToRadian(30);
    cameraDistance = viewportMiddleHeight / tanf(fov / 2);
    upVec.x = 0.0f;
    upVec.y = 1.0f;
    upVec.z = 0.0f;
    atVec.x = viewportMiddleWidth;
    atVec.y = -viewportMiddleHeight;
    atVec.z = 0.0f;
    eyeVec.x = viewportMiddleWidth;
    eyeVec.y = -viewportMiddleHeight;
    eyeVec.z = -cameraDistance;
    D3DXMatrixLookAtLH(&g_Supervisor.viewMatrix, &eyeVec, &atVec, &upVec);
    g_GameManager.cameraDistance = fabsf(cameraDistance);
    D3DXMatrixPerspectiveFovLH(&g_Supervisor.projectionMatrix, fov, aspectRatio, 100.0f,
                               10000.0f + extraRenderDistance);
    g_Supervisor.d3dDevice->SetTransform(D3DTS_VIEW, &g_Supervisor.viewMatrix);
    g_Supervisor.d3dDevice->SetTransform(D3DTS_PROJECTION, &g_Supervisor.projectionMatrix);
    return;
}

void GameManager::SetupCamera(f32 extraRenderDistance)
{
    D3DXVECTOR3 eyeVec;
    D3DXVECTOR3 atVec;
    D3DXVECTOR3 upVec;
    f32 fov;
    f32 aspectRatio;
    f32 viewportMiddleWidth;
    f32 viewportMiddleHeight;
    f32 cameraDistance;

    f32 atVecY;
    f32 atVecX;
    f32 eyeVecZ;

    viewportMiddleWidth = g_Supervisor.viewport.Width / 2.0f;
    viewportMiddleHeight = g_Supervisor.viewport.Height / 2.0f;
    aspectRatio = (f32)g_Supervisor.viewport.Width / (f32)g_Supervisor.viewport.Height;
    fov = D3DXToRadian(30);
    cameraDistance = viewportMiddleHeight / tanf(fov / 2);
    upVec.x = 0.0f;
    upVec.y = 1.0f;
    upVec.z = 0.0f;
    atVecY = -viewportMiddleHeight + (f32)g_GameManager.stageCameraFacingDir.y;
    atVecX = viewportMiddleWidth + (f32)g_GameManager.stageCameraFacingDir.x;
    atVec.x = atVecX;
    atVec.y = atVecY;
    atVec.z = 0;
    eyeVecZ = -cameraDistance * (f32)g_GameManager.stageCameraFacingDir.z;
    eyeVec.x = viewportMiddleWidth;
    eyeVec.y = -viewportMiddleHeight;
    eyeVec.z = eyeVecZ;
    D3DXMatrixLookAtLH(&g_Supervisor.viewMatrix, &eyeVec, &atVec, &upVec);
    g_GameManager.cameraDistance = fabsf(cameraDistance);
    D3DXMatrixPerspectiveFovLH(&g_Supervisor.projectionMatrix, fov, aspectRatio, 100.0f,
                               10000.0f + extraRenderDistance);
    g_Supervisor.d3dDevice->SetTransform(D3DTS_VIEW, &g_Supervisor.viewMatrix);
    g_Supervisor.d3dDevice->SetTransform(D3DTS_PROJECTION, &g_Supervisor.projectionMatrix);
    return;
}

void GameManager::IncreaseSubrank(i32 amount)
{
    this->subRank = this->subRank + amount;
    while (this->subRank >= 100)
    {
        this->rank++;
        this->subRank -= 100;
    }
    if (this->rank > this->maxRank)
    {
        this->rank = this->maxRank;
    }
}

void GameManager::DecreaseSubrank(i32 amount)
{
    this->subRank = this->subRank - amount;
    while (this->subRank < 0)
    {
        this->rank--;
        this->subRank += 100;
    }
    if (this->rank < this->minRank)
    {
        this->rank = this->minRank;
    }
}

GameManager::GameManager()
{

    memset(this, 0, sizeof(GameManager));

    (this->arcadeRegionTopLeftPos).x = GAME_REGION_LEFT;
    (this->arcadeRegionTopLeftPos).y = GAME_REGION_TOP;
    (this->arcadeRegionSize).x = GAME_REGION_WIDTH;
    (this->arcadeRegionSize).y = GAME_REGION_HEIGHT;
}
}; // namespace th06
