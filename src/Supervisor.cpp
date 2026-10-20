#include "Supervisor.hpp"
#include "AnmManager.hpp"
#include "AsciiManager.hpp"
#include "Chain.hpp"
#include "ChainPriorities.hpp"
#include "Ending.hpp"
#include "FileSystem.hpp"
#include "GameErrorContext.hpp"
#include "GameManager.hpp"
#include "GameWindow.hpp"
#include "Gui.hpp"
#include "MainMenu.hpp"
#include "MusicRoom.hpp"
#include "Network.hpp"
#include "Player.hpp"
#include "QuickSettings.hpp"
#include "ReplayManager.hpp"
#include "render/renderer.hpp"
#include "ResultScreen.hpp"
#include "Rng.hpp"
#include "Screenshot.hpp"
#include "SoundFontSynth.hpp"
#include "SoundPlayer.hpp"
#include "TextHelper.hpp"
#include "i18n.hpp"
#include "inttypes.hpp"
#include "utils.hpp"
#include <D3DX8.h>

#include <stdio.h>
#include <string.h>
#include <windows.h>

namespace th06
{
Supervisor g_Supervisor;
ControllerMapping g_ControllerMapping;
IDirect3DSurface8 *g_TextBufferSurface;
i32 g_ShowInputDisplay;
i32 g_IsSpellPracticeGame;
i32 g_SpellPracticeMode;
i32 g_SpellPracticeSection;
i32 g_SpellPracticeWarpTime;
i32 g_ThpracSection;
i32 g_ThpracDlg;
i32 g_ThpracSectionReplay;
i32 g_SpellPracticeBossSubId;
i32 g_SpellPracticeResultDelay;
i32 g_ReturnToSpellPractice;
i32 g_ReplaySpeed = 1;
i32 g_SavedReplaySpeed = 1;
i32 g_ReplayFastForwardActive = 0;
i32 g_ReplayTotalFrames = 0;
u16 g_RealCurFrameInput;
i32 g_LastSpellPracticeIndex = -1;
i32 g_SpellPracticeRank = 16;
i32 g_ReturnToPractice;
i32 g_SpellPracticeLastSelectedSpellIndex;
i32 g_MoveShift;
i32 g_CustomWindowWidth;
i32 g_CustomWindowHeight;
i32 g_CustomWindowPosX;
i32 g_CustomWindowPosY;
i32 g_CustomBgmVolume;
i32 g_CustomBgmVolumeMidi;
i32 g_CustomSeVolume;
i32 g_ShowCollision;
i32 g_HitDisp;
i32 g_Backup;
i32 g_BackupKeepCount;
i32 g_IsOnline;
i32 g_Vsync;
i32 g_BossHpCircle;
i32 g_ShowEnemyMarker;
i32 g_ShowDifficultyLabel;
i32 g_ShowFrameCounter;
i32 g_VerticalMode;
i32 g_P2BombsRemaining;
i32 g_P2GrazeInStage;
i32 g_P2LivesRemaining;
u32 g_P2Score;
u32 g_P2HighScore;
u32 g_P2PointItems;
u16 g_P2Power;
u16 g_NetRandomSeed;
u16 g_LastFrameInput;
u16 g_CurFrameInput;
u16 g_IsEigthFrameOfHeldInput;
u16 g_NumOfFramesInputsWereHeld;
u16 g_SuppressedInputFrames;
u8 g_SkipNextFrameClear;

#define INPUT_BUFFER_SIZE 512
static GameFramePacket g_LocalInputBuffer[INPUT_BUFFER_SIZE];
static GameFramePacket g_RemoteInputBuffer[INPUT_BUFFER_SIZE];
static bool g_LocalBufferValid[INPUT_BUFFER_SIZE] = {false};
static bool g_RemoteBufferValid[INPUT_BUFFER_SIZE] = {false};
static u32 g_NetFrameCount = 0;
u32 g_InputDelay = 3;

i32 g_BgmTrackMode[BGM_TRACK_COUNT + 1] = {0};

static i32 s_activeMusicMode = OFF;

static i32 s_musicModeOverride = 0;

static char s_midiSlotPath[32][64];
static bool s_midiSlotLoaded[32];

void Supervisor::ResetInputState()
{
    g_LastFrameInput = 0;
    g_CurFrameInput = 0;
    g_IsEigthFrameOfHeldInput = 0;
    g_NumOfFramesInputsWereHeld = 0;
}

void Supervisor::SuppressInputFrames(u16 frames)
{
    g_SuppressedInputFrames = frames;
    ResetInputState();
}

bool Supervisor::IsQuickSettingsOpen()
{
    return QuickSettings::IsOpen();
}

bool Supervisor::IsQuickSettingsVisible()
{
    return QuickSettings::IsVisible();
}

bool Supervisor::IsQuickSettingsWaitingKey()
{
    return QuickSettings::IsWaitingKey();
}

static ZunResult ReinitMainMenu(Supervisor *s)
{
    s->curState = SUPERVISOR_STATE_MAINMENU;
    g_Supervisor.d3dDevice->ResourceManagerDiscardBytes(0);
    return MainMenu::RegisterChain(0);
}

static ZunResult ReturnToMenuFromGame(Supervisor *s)
{
    GameManager::CutChain();
    s->curState = SUPERVISOR_STATE_INIT;
    ReplayManager::SaveReplay(NULL, NULL);
    return ReinitMainMenu(s);
}

ChainCallbackResult Supervisor::OnUpdate(Supervisor *s)
{
    static i32 lastNetState = SUPERVISOR_STATE_INIT;
    static bool disconnectSent = false;

    bool newlyEnteredGame =
        (s->curState == SUPERVISOR_STATE_GAMEMANAGER && lastNetState != SUPERVISOR_STATE_GAMEMANAGER);
    lastNetState = s->curState;

    if (g_SoundPlayer.backgroundMusic != NULL)
    {
        g_SoundPlayer.backgroundMusic->UpdateFadeOut();
    }
    g_LastFrameInput = g_CurFrameInput;
    g_IsEigthFrameOfHeldInput = 0;
    if (g_SuppressedInputFrames != 0)
    {
        g_CurFrameInput = 0;
        g_SuppressedInputFrames--;
    }
    else
    {
        g_CurFrameInput = Controller::GetInput();
    }
    g_RealCurFrameInput = g_CurFrameInput;

    if (g_NetworkManager.IsConnected())
    {
        if (s->curState == SUPERVISOR_STATE_GAMEMANAGER)
        {
            if (newlyEnteredGame)
            {
                memset(g_LocalBufferValid, 0, sizeof(g_LocalBufferValid));
                memset(g_RemoteBufferValid, 0, sizeof(g_RemoteBufferValid));
                for (u32 i = 0; i < g_InputDelay; i++)
                {
                    memset(&g_LocalInputBuffer[i], 0, sizeof(GameFramePacket));
                    g_LocalInputBuffer[i].frame = i;
                    g_LocalBufferValid[i] = true;

                    memset(&g_RemoteInputBuffer[i], 0, sizeof(GameFramePacket));
                    g_RemoteInputBuffer[i].frame = i;
                    g_RemoteBufferValid[i] = true;
                }
                g_NetFrameCount = 0;
                disconnectSent = false;
            }

            Player *localPlayer = g_NetworkManager.IsHost() ? &g_Player : &g_Player2;
            Player *remotePlayer = g_NetworkManager.IsHost() ? &g_Player2 : &g_Player;

            u32 sendFrame = g_NetFrameCount + g_InputDelay;
            GameFramePacket localPacket;
            memset(&localPacket, 0, sizeof(GameFramePacket));
            localPacket.frame = sendFrame;
            localPacket.input = g_CurFrameInput;
            localPacket.posX = localPlayer->positionCenter.x;
            localPacket.posY = localPlayer->positionCenter.y;
            localPacket.playerState = (u8)localPlayer->playerState;
            localPacket.respawnTimer = (u8)localPlayer->respawnTimer;
            localPacket.invulnFrames = (i16)localPlayer->invulnerabilityTimer.AsFrames();
            localPacket.livesRemaining = (u8)g_GameManager.livesRemaining;
            localPacket.bombsRemaining = (u8)g_GameManager.bombsRemaining;
            localPacket.isInGameMenu = g_GameManager.isInGameMenu;
            localPacket.isInRetryMenu = g_GameManager.isInRetryMenu;
            localPacket.nextSupervisorState = (u8)s->curState;
            localPacket.rngSeed = g_Rng.seed;
            localPacket.rngGenCount = g_Rng.generationCount;
            localPacket.score = g_GameManager.score;
            localPacket.highScore = g_GameManager.highScore;
            localPacket.guiScore = g_GameManager.guiScore;
            localPacket.currentPower = g_GameManager.currentPower;
            localPacket.grazeInStage = g_GameManager.grazeInStage;
            localPacket.rank = g_GameManager.rank;
            localPacket.subRank = g_GameManager.subRank;
            localPacket.pointItemsCollectedInStage = g_GameManager.pointItemsCollectedInStage;

            g_LocalInputBuffer[sendFrame % INPUT_BUFFER_SIZE] = localPacket;
            g_LocalBufferValid[sendFrame % INPUT_BUFFER_SIZE] = true;

            if (g_NetworkManager.SendPacket(&localPacket, sizeof(GameFramePacket)) != ZUN_SUCCESS)
            {
                s->curState = SUPERVISOR_STATE_MAINMENU;
                return CHAIN_CALLBACK_RESULT_CONTINUE;
            }

            bool received = false;
            do
            {
                GameFramePacket recvPacket;
                memset(&recvPacket, 0, sizeof(GameFramePacket));
                if (g_NetworkManager.RecvPacketNonBlocking(&recvPacket, sizeof(GameFramePacket), &received) !=
                    ZUN_SUCCESS)
                {
                    s->curState = SUPERVISOR_STATE_MAINMENU;
                    return CHAIN_CALLBACK_RESULT_CONTINUE;
                }
                if (received)
                {
                    if (recvPacket.input == 0xFFFF)
                    {
                        g_NetworkManager.Cleanup(false);
                        if (recvPacket.nextSupervisorState == SUPERVISOR_STATE_MAINMENU ||
                            recvPacket.nextSupervisorState == SUPERVISOR_STATE_MAINMENU_REPLAY)
                        {
                            s->curState = (SupervisorState)recvPacket.nextSupervisorState;
                        }
                        else
                        {
                            s->curState = SUPERVISOR_STATE_RESULTSCREEN_FROMGAME;
                        }
                        g_GameManager.isInRetryMenu = 0;
                        g_GameManager.isInGameMenu = 0;
                        for (int i = 0; i < 6; i++)
                        {
                            g_AsciiManager.retryMenu.menuSprites[i].SetInvisible();
                            g_AsciiManager.gameMenu.menuSprites[i].SetInvisible();
                        }
                        g_AsciiManager.retryMenu.menuBackground.SetInvisible();
                        g_AsciiManager.gameMenu.menuBackground.SetInvisible();
                        g_AsciiManager.retryMenu.curState = 0;
                        g_AsciiManager.gameMenu.curState = 0;
                        g_AsciiManager.retryMenu.numFrames = 0;
                        g_AsciiManager.gameMenu.numFrames = 0;
                        return CHAIN_CALLBACK_RESULT_CONTINUE;
                    }
                    else
                    {
                        g_RemoteInputBuffer[recvPacket.frame % INPUT_BUFFER_SIZE] = recvPacket;
                        g_RemoteBufferValid[recvPacket.frame % INPUT_BUFFER_SIZE] = true;
                    }
                }
            } while (received);

            DWORD startTime = GetTickCount();
            while (!g_RemoteBufferValid[g_NetFrameCount % INPUT_BUFFER_SIZE])
            {
                GameFramePacket recvPacket;
                memset(&recvPacket, 0, sizeof(GameFramePacket));
                if (g_NetworkManager.RecvPacketBlocking(&recvPacket, sizeof(GameFramePacket), 1) == ZUN_SUCCESS)
                {
                    if (recvPacket.input == 0xFFFF)
                    {
                        g_NetworkManager.Cleanup(false);
                        if (recvPacket.nextSupervisorState == SUPERVISOR_STATE_MAINMENU ||
                            recvPacket.nextSupervisorState == SUPERVISOR_STATE_MAINMENU_REPLAY)
                        {
                            s->curState = (SupervisorState)recvPacket.nextSupervisorState;
                        }
                        else
                        {
                            s->curState = SUPERVISOR_STATE_RESULTSCREEN_FROMGAME;
                        }
                        g_GameManager.isInRetryMenu = 0;
                        g_GameManager.isInGameMenu = 0;
                        for (int i = 0; i < 6; i++)
                        {
                            g_AsciiManager.retryMenu.menuSprites[i].SetInvisible();
                            g_AsciiManager.gameMenu.menuSprites[i].SetInvisible();
                        }
                        g_AsciiManager.retryMenu.menuBackground.SetInvisible();
                        g_AsciiManager.gameMenu.menuBackground.SetInvisible();
                        g_AsciiManager.retryMenu.curState = 0;
                        g_AsciiManager.gameMenu.curState = 0;
                        g_AsciiManager.retryMenu.numFrames = 0;
                        g_AsciiManager.gameMenu.numFrames = 0;
                        return CHAIN_CALLBACK_RESULT_CONTINUE;
                    }
                    else
                    {
                        g_RemoteInputBuffer[recvPacket.frame % INPUT_BUFFER_SIZE] = recvPacket;
                        g_RemoteBufferValid[recvPacket.frame % INPUT_BUFFER_SIZE] = true;
                    }
                }
                else
                {
                    if (GetTickCount() - startTime > 2000)
                    {
                        s->curState = SUPERVISOR_STATE_MAINMENU;
                        g_NetworkManager.Cleanup(false);
                        return CHAIN_CALLBACK_RESULT_CONTINUE;
                    }
                }
            }

            GameFramePacket remotePacket = g_RemoteInputBuffer[g_NetFrameCount % INPUT_BUFFER_SIZE];

            localPlayer->previousFrameInput = localPlayer->input;
            localPlayer->input = g_LocalInputBuffer[g_NetFrameCount % INPUT_BUFFER_SIZE].input;
            remotePlayer->previousFrameInput = remotePlayer->input;
            remotePlayer->input = remotePacket.input;

            if (g_NetFrameCount >= g_InputDelay)
            {
                remotePlayer->positionCenter.x = remotePacket.posX;
                remotePlayer->positionCenter.y = remotePacket.posY;

                if (remotePlayer->playerState != (PlayerState)remotePacket.playerState)
                {
                    if (remotePacket.playerState == PLAYER_STATE_DEAD && remotePlayer->playerState != PLAYER_STATE_DEAD)
                    {
                        remotePlayer->Die();
                    }
                    remotePlayer->playerState = (PlayerState)remotePacket.playerState;
                }

                remotePlayer->respawnTimer = remotePacket.respawnTimer;
                remotePlayer->invulnerabilityTimer.SetCurrent(remotePacket.invulnFrames);
                g_P2BombsRemaining = (i32)remotePacket.bombsRemaining;
                g_P2GrazeInStage = remotePacket.grazeInStage;
                g_P2LivesRemaining = (i32)remotePacket.livesRemaining;
                g_P2Score = remotePacket.score;
                g_P2HighScore = remotePacket.highScore;
                g_P2PointItems = remotePacket.pointItemsCollectedInStage;
                g_P2Power = remotePacket.currentPower;

                if (remotePacket.rngGenCount != g_Rng.generationCount)
                {
                    utils::LogDebug("DESYNC frame=%u local_rngGenCount=%u remote_rngGenCount=%u\n", g_NetFrameCount,
                                    g_Rng.generationCount, remotePacket.rngGenCount);
                }
            }

            if (remotePacket.isInGameMenu >= 4)
            {

                g_GameManager.isInGameMenu = 0;
                g_AsciiManager.gameMenu.curState = GAME_MENU_PAUSE_OPENING;
                g_AsciiManager.gameMenu.numFrames = 0;
                for (int i = 0; i < ARRAY_SIZE_SIGNED(g_AsciiManager.gameMenu.menuSprites); i++)
                {
                    g_AsciiManager.gameMenu.menuSprites[i].SetInvisible();
                }
                g_AsciiManager.gameMenu.menuBackground.SetInvisible();

                if (remotePacket.isInGameMenu == 6)
                {

                    if (g_GameManager.isInPracticeMode)
                        g_GameManager.currentStage = g_GameManager.currentStage - 1;
                    else
                        g_GameManager.currentStage = (g_GameManager.difficulty == EXTRA) ? EXTRA_STAGE : 0;
                    g_GameManager.score = 0;
                    g_GameManager.guiScore = 0;
                    g_GameManager.livesRemaining = g_Supervisor.cfg.lifeCount;
                    g_GameManager.bombsRemaining = g_Supervisor.cfg.bombCount;
                    g_GameManager.currentPower = 0;
                    g_GameManager.pointItemsCollectedInStage = 0;
                    g_GameManager.grazeInStage = 0;
                    if (g_GameManager.difficulty == EXTRA || g_GameManager.isInPracticeMode)
                    {
                        g_GameManager.livesRemaining = 2;
                        g_GameManager.bombsRemaining = 3;
                    }

                    g_GameManager.counat = 0;
                    s->curState = SUPERVISOR_STATE_GAMEMANAGER_REINIT;
                }
                else
                {

                    s->curState = SUPERVISOR_STATE_MAINMENU;
                }
            }
            else if (remotePacket.isInGameMenu == 1 && g_GameManager.isInGameMenu == 0)
            {

                g_GameManager.isInGameMenu = 1;
            }
            else if (remotePacket.isInGameMenu == 0 && g_GameManager.isInGameMenu == 1)
            {

                g_GameManager.isInGameMenu = 0;
            }

            if (g_GameManager.isInRetryMenu != 0)
            {
                if (remotePacket.isInRetryMenu == 2)
                {

                    g_GameManager.isInRetryMenu = 2;
                    g_AsciiManager.retryMenu.curState = RETRY_MENU_SELECTED_YES;
                    g_AsciiManager.retryMenu.numFrames = 0;
                }
                else if (remotePacket.isInRetryMenu == 3)
                {

                    g_GameManager.isInRetryMenu = 3;
                    g_AsciiManager.retryMenu.curState = RETRY_MENU_SELECTED_NO;
                    g_AsciiManager.retryMenu.numFrames = 0;
                }
            }

            g_CurFrameInput = localPlayer->input | remotePacket.input;

            g_LocalBufferValid[g_NetFrameCount % INPUT_BUFFER_SIZE] = false;
            g_RemoteBufferValid[g_NetFrameCount % INPUT_BUFFER_SIZE] = false;
            g_NetFrameCount++;
        }
        else
        {

            if (!disconnectSent)
            {
                disconnectSent = true;
                GameFramePacket disconnectPacket;
                memset(&disconnectPacket, 0, sizeof(GameFramePacket));
                disconnectPacket.input = 0xFFFF;
                disconnectPacket.nextSupervisorState = (u8)s->curState;
                g_NetworkManager.SendPacket(&disconnectPacket, sizeof(GameFramePacket));
                Sleep(50);
                g_NetworkManager.Cleanup(false);
            }
        }
    }

    QuickSettings::Update(s);
    Screenshot_Update();

    if (QuickSettings::IsOpen())
    {
        g_CurFrameInput = 0;
        g_LastFrameInput = 0;
    }

    if (!g_NetworkManager.IsConnected() && !g_GameManager.isInReplay)
    {
        g_Player.previousFrameInput = g_Player.input;
        g_Player.input = g_CurFrameInput;
        g_Player2.previousFrameInput = g_Player2.input;
        g_Player2.input = 0;
    }

    if (!g_GameManager.isInReplay)
    {
        if (g_LastFrameInput == g_CurFrameInput)
        {
            if (g_NumOfFramesInputsWereHeld >= 30)
            {
                if (g_NumOfFramesInputsWereHeld % 8 == 0)
                {
                    g_IsEigthFrameOfHeldInput = 1;
                }
                if (g_NumOfFramesInputsWereHeld >= 38)
                {
                    g_NumOfFramesInputsWereHeld = 30;
                }
            }
            g_NumOfFramesInputsWereHeld++;
        }
        else
        {
            g_NumOfFramesInputsWereHeld = 0;
        }
    }

    if (s->wantedState != s->curState)
    {
        g_SkipNextFrameClear = 1;
        s->wantedState2 = s->wantedState;
        switch (s->wantedState)
        {
        case SUPERVISOR_STATE_INIT:
            if (ReinitMainMenu(s) != ZUN_SUCCESS)
            {
                return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
            }
            break;
        case SUPERVISOR_STATE_MAINMENU:
            switch (s->curState)
            {
            case SUPERVISOR_STATE_EXITSUCCESS:
                return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
            case SUPERVISOR_STATE_GAMEMANAGER:
                if (GameManager::RegisterChain() != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            case SUPERVISOR_STATE_EXITERROR:
                return CHAIN_CALLBACK_RESULT_EXIT_GAME_ERROR;
            case SUPERVISOR_STATE_RESULTSCREEN:
                if (ResultScreen::RegisterChain(NULL) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            case SUPERVISOR_STATE_MUSICROOM:
                if (MusicRoom::RegisterChain() != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            case SUPERVISOR_STATE_ENDING:
                GameManager::CutChain();
                if (Ending::RegisterChain() != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            }
            break;

        case SUPERVISOR_STATE_RESULTSCREEN:
            switch (s->curState)
            {
            case SUPERVISOR_STATE_EXITSUCCESS:
                return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
            case SUPERVISOR_STATE_MAINMENU:
                s->curState = SUPERVISOR_STATE_INIT;
                if (ReinitMainMenu(s) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            }
            break;
        case SUPERVISOR_STATE_GAMEMANAGER:
            switch (s->curState)
            {
            case SUPERVISOR_STATE_EXITSUCCESS:
                return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;

            case SUPERVISOR_STATE_MAINMENU:
                if (ReturnToMenuFromGame(s) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;

            case SUPERVISOR_STATE_RESULTSCREEN_FROMGAME:
                GameManager::CutChain();
                if (ResultScreen::RegisterChain(TRUE) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            case SUPERVISOR_STATE_GAMEMANAGER_REINIT:
                GameManager::CutChain();
                if (GameManager::RegisterChain() != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                if (s->curState == SUPERVISOR_STATE_MAINMENU)
                {
                    if (ReturnToMenuFromGame(s) != ZUN_SUCCESS)
                    {
                        return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                    }
                    break;
                }
                s->curState = SUPERVISOR_STATE_GAMEMANAGER;
                break;
            case SUPERVISOR_STATE_MAINMENU_REPLAY:
                GameManager::CutChain();
                s->curState = SUPERVISOR_STATE_MAINMENU;
                ReplayManager::SaveReplay(NULL, NULL);
                g_Supervisor.d3dDevice->ResourceManagerDiscardBytes(0);
                g_GameManager.isInReplay = 0;
                if (MainMenu::RegisterChain(1) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;

            case SUPERVISOR_STATE_ENDING:
                GameManager::CutChain();
                if (Ending::RegisterChain() != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            }
            break;
        case SUPERVISOR_STATE_RESULTSCREEN_FROMGAME:
            switch (s->curState)
            {
            case SUPERVISOR_STATE_EXITSUCCESS:
                ReplayManager::SaveReplay(NULL, NULL);
                return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
            case SUPERVISOR_STATE_MAINMENU:
                s->curState = SUPERVISOR_STATE_INIT;
                ReplayManager::SaveReplay(NULL, NULL);
                if (g_GameManager.isInReplay)
                {
                    g_GameManager.isInReplay = 0;
                    s->curState = SUPERVISOR_STATE_MAINMENU;
                    g_Supervisor.d3dDevice->ResourceManagerDiscardBytes(0);
                    if (MainMenu::RegisterChain(1) != ZUN_SUCCESS)
                    {
                        return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                    }
                    break;
                }
                if (ReinitMainMenu(s) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            }
            break;
        case SUPERVISOR_STATE_MUSICROOM:
            switch (s->curState)
            {
            case SUPERVISOR_STATE_EXITSUCCESS:
                return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;

            case SUPERVISOR_STATE_MAINMENU:
                s->curState = SUPERVISOR_STATE_INIT;
                if (ReinitMainMenu(s) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            }
            break;
        case SUPERVISOR_STATE_ENDING:
            switch (s->curState)
            {
            case SUPERVISOR_STATE_EXITSUCCESS:
                return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
            case SUPERVISOR_STATE_MAINMENU:
                s->curState = SUPERVISOR_STATE_INIT;
                if (ReinitMainMenu(s) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
                break;
            case SUPERVISOR_STATE_RESULTSCREEN_FROMGAME:
                if (ResultScreen::RegisterChain(TRUE) != ZUN_SUCCESS)
                {
                    return CHAIN_CALLBACK_RESULT_EXIT_GAME_SUCCESS;
                }
            }
            break;
        }
        g_CurFrameInput = Controller::GetInput();
        g_LastFrameInput = g_CurFrameInput;
        g_IsEigthFrameOfHeldInput = 0;
    }

    s->wantedState = s->curState;
    s->calcCount++;
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult Supervisor::OnDraw(Supervisor *s)
{
    g_AnmManager->currentVertexShader = 0xff;
    g_AnmManager->currentSprite = NULL;
    g_AnmManager->currentTexture = NULL;
    g_AnmManager->currentColorOp = 0xff;
    g_AnmManager->currentBlendMode = 0xff;
    g_AnmManager->currentZWriteDisable = 0xff;

    Supervisor::DrawFpsCounter();
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

static ChainCallbackResult OnDrawQuickSettings(Supervisor *s)
{

    g_AnmManager->currentVertexShader = 0xff;
    g_AnmManager->currentSprite = NULL;
    g_AnmManager->currentTexture = NULL;
    g_AnmManager->currentColorOp = 0xff;
    g_AnmManager->currentBlendMode = 0xff;
    g_AnmManager->currentZWriteDisable = 0xff;

    QuickSettings::Draw();
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

BOOL CALLBACK Supervisor::ControllerCallback(LPCDIDEVICEOBJECTINSTANCEA lpddoi, LPVOID pvRef)
{
    LPVOID pvRefBackup;
    DIPROPRANGE diprange;
    pvRefBackup = pvRef;

    if (lpddoi->dwType & DIDFT_AXIS)
    {
        diprange.diph.dwSize = sizeof(diprange);
        diprange.diph.dwHeaderSize = sizeof(diprange.diph);
        diprange.diph.dwHow = DIPH_BYID;
        diprange.diph.dwObj = lpddoi->dwType;
        diprange.lMin = -1000;
        diprange.lMax = 1000;

        if (g_Supervisor.controller->SetProperty(DIPROP_RANGE, &diprange.diph) < 0)
        {
            return FALSE;
        }
    }
    return TRUE;
}

ZunResult Supervisor::RegisterChain()
{
    ChainElem *chain;
    Supervisor *supervisor = &g_Supervisor;

    supervisor->wantedState = 0;
    supervisor->curState = -1;
    supervisor->calcCount = 0;
    g_SkipNextFrameClear = 0;

    chain = g_Chain.CreateElem((ChainCallback)Supervisor::OnUpdate);
    chain->arg = supervisor;
    chain->addedCallback = (ChainAddedCallback)Supervisor::AddedCallback;
    chain->deletedCallback = (ChainDeletedCallback)Supervisor::DeletedCallback;
    if (g_Chain.AddToCalcChain(chain, TH_CHAIN_PRIO_CALC_SUPERVISOR) != 0)
    {
        return ZUN_ERROR;
    }

    chain = g_Chain.CreateElem((ChainCallback)Supervisor::OnDraw);
    chain->arg = supervisor;
    g_Chain.AddToDrawChain(chain, TH_CHAIN_PRIO_DRAW_SUPERVISOR);

    chain = g_Chain.CreateElem((ChainCallback)OnDrawQuickSettings);
    chain->arg = supervisor;
    g_Chain.AddToDrawChain(chain, TH_CHAIN_PRIO_DRAW_QUICKSETTINGS);

    return ZUN_SUCCESS;
}

ZunResult Supervisor::AddedCallback(Supervisor *s)
{
    i32 i;

    for (i = 0; i < (i32)(sizeof(s->pbg3Archives) / sizeof(s->pbg3Archives[0])); i++)
    {
        s->pbg3Archives[i] = NULL;
    }

    g_Pbg3Archives = s->pbg3Archives;
    if (s->LoadPbg3(IN_PBG3_INDEX, TH_IN_DAT_FILE))
    {
        return ZUN_ERROR;
    }
    g_AnmManager->LoadSurface(0, "data/title/th06logo.jpg");

    g_AnmManager->CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    RECT logoSrcRect = {0, 0, GAME_WINDOW_WIDTH, GAME_WINDOW_HEIGHT};
    RECT logoDestRect = GameWindow::GetAspectRatioDestRect(g_GameWindow.window);
    if (g_Supervisor.d3dDevice->Present(&logoSrcRect, &logoDestRect, 0, 0) < 0)
        g_Supervisor.d3dDevice->Reset(&g_Supervisor.presentParameters);

    g_AnmManager->CopySurfaceToBackBuffer(0, 0, 0, 0, 0);
    if (g_Supervisor.d3dDevice->Present(&logoSrcRect, &logoDestRect, 0, 0) < 0)
        g_Supervisor.d3dDevice->Reset(&g_Supervisor.presentParameters);

    g_AnmManager->ReleaseSurface(0);

    s->startupTimeBeforeMenuMusic = timeGetTime();
    Supervisor::SetupDInput(s);

    s->midiOutput = new MidiOutput();

    // FluidSynth を起動時に先行初期化する。
    // 最初に再生されるトラックが WAV の場合、MidiOutput::OpenDevice が呼ばれず
    // SoundFontSynth::Open が実行されないため、後で MIDI に切り替わっても
    // シンセが使えない問題を防ぐ。
    if (g_MidiSynthType == 1)
    {
        SoundFontSynth::Open(g_MidiSoundFont[0] ? g_MidiSoundFont : NULL);
    }

    g_Rng.Initialize(timeGetTime());

    g_SoundPlayer.InitSoundBuffers();
    if (g_AnmManager->LoadAnm(ANM_FILE_TEXT, "data/text.anm", ANM_OFFSET_TEXT) != 0)
    {
        return ZUN_ERROR;
    }

    if (AsciiManager::RegisterChain() != 0)
    {
        g_GameErrorContext.Log(TH_ERR_ASCIIMANAGER_INIT_FAILED);
        return ZUN_ERROR;
    }

    s->unk198 = 0;
    g_AnmManager->SetupVertexBuffer();
    TextHelper::CreateTextBuffer();
    s->ReleasePbg3(IN_PBG3_INDEX);
    if (g_Supervisor.LoadPbg3(MD_PBG3_INDEX, TH_MD_DAT_FILE) != 0)
        return ZUN_ERROR;

    return ZUN_SUCCESS;
}

ZunResult Supervisor::SetupDInput(Supervisor *supervisor)
{
    HINSTANCE hInst;

    hInst = (HINSTANCE)GetWindowLongA(supervisor->hwndGameWindow, GWL_HINSTANCE);
    if (supervisor->cfg.opts >> GCOS_NO_DIRECTINPUT_PAD & 1)
    {
        return ZUN_ERROR;
    }

    if (DirectInput8Create(hInst, DIRECTINPUT_VERSION, IID_IDirectInput8A, (LPVOID *)&supervisor->dinputIface, NULL) <
        0)
    {
        supervisor->dinputIface = NULL;
        g_GameErrorContext.Log(TH_ERR_DIRECTINPUT_NOT_AVAILABLE);
        return ZUN_ERROR;
    }

    if (supervisor->dinputIface->CreateDevice(GUID_SysKeyboard, &supervisor->keyboard, NULL) < 0)
    {
        if (supervisor->dinputIface)
        {
            supervisor->dinputIface->Release();
            supervisor->dinputIface = NULL;
        }
        g_GameErrorContext.Log(TH_ERR_DIRECTINPUT_NOT_AVAILABLE);
        return ZUN_ERROR;
    }

    if (supervisor->keyboard->SetDataFormat(&c_dfDIKeyboard) < 0)
    {
        if (supervisor->keyboard)
        {
            supervisor->keyboard->Release();
            supervisor->keyboard = NULL;
        }

        if (supervisor->dinputIface)
        {
            supervisor->dinputIface->Release();
            supervisor->dinputIface = NULL;
        }

        g_GameErrorContext.Log(TH_ERR_DIRECTINPUT_SETDATAFORMAT_NOT_AVAILABLE);
        return ZUN_ERROR;
    }

    if (supervisor->keyboard->SetCooperativeLevel(supervisor->hwndGameWindow,
                                                  DISCL_NONEXCLUSIVE | DISCL_FOREGROUND | DISCL_NOWINKEY) < 0)
    {
        if (supervisor->keyboard)
        {
            supervisor->keyboard->Release();
            supervisor->keyboard = NULL;
        }

        if (supervisor->dinputIface)
        {
            supervisor->dinputIface->Release();
            supervisor->dinputIface = NULL;
        }

        g_GameErrorContext.Log(TH_ERR_DIRECTINPUT_SETCOOPERATIVELEVEL_NOT_AVAILABLE);
        return ZUN_ERROR;
    }

    supervisor->keyboard->Acquire();
    g_GameErrorContext.Log(TH_ERR_DIRECTINPUT_INITIALIZED);

    supervisor->dinputIface->EnumDevices(DI8DEVCLASS_GAMECTRL, Supervisor::EnumGameControllersCb, NULL,
                                         DIEDFL_ATTACHEDONLY);
    if (supervisor->controller)
    {
        supervisor->controller->SetDataFormat(&c_dfDIJoystick2);
        supervisor->controller->SetCooperativeLevel(supervisor->hwndGameWindow, DISCL_EXCLUSIVE | DISCL_FOREGROUND);

        g_Supervisor.controllerCaps.dwSize = sizeof(g_Supervisor.controllerCaps);

        supervisor->controller->GetCapabilities(&g_Supervisor.controllerCaps);
        supervisor->controller->EnumObjects(Supervisor::ControllerCallback, NULL, DIDFT_ALL);

        g_GameErrorContext.Log(TH_ERR_PAD_FOUND);
    }
    return ZUN_SUCCESS;
}

BOOL CALLBACK Supervisor::EnumGameControllersCb(LPCDIDEVICEINSTANCEA pdidInstance, LPVOID pContext)
{
    HRESULT result;

    if (!g_Supervisor.controller)
    {
        result = g_Supervisor.dinputIface->CreateDevice(pdidInstance->guidInstance, &g_Supervisor.controller, NULL);
        if (result < 0)
        {
            return TRUE;
        }
    }
    return FALSE;
}

ZunResult Supervisor::DeletedCallback(Supervisor *s)
{
    i32 pbg3Idx;

    g_AnmManager->ReleaseVertexBuffer();
    for (pbg3Idx = 0; pbg3Idx < ARRAY_SIZE_SIGNED(s->pbg3Archives); pbg3Idx += 1)
    {
        s->ReleasePbg3(pbg3Idx);
    }
    g_AnmManager->ReleaseAnm(0);
    AsciiManager::CutChain();
    g_SoundPlayer.StopBGM();
    if (s->midiOutput != NULL)
    {
        s->midiOutput->StopPlayback();
        delete s->midiOutput;
        s->midiOutput = NULL;
    }
    SoundFontSynth::Shutdown();
    ReplayManager::SaveReplay(NULL, NULL);
    TextHelper::ReleaseTextBuffer();
    if (s->keyboard != NULL)
    {
        s->keyboard->Unacquire();
    }
    if (s->keyboard != NULL)
    {
        s->keyboard->Release();
        s->keyboard = NULL;
    }
    if (s->controller != NULL)
    {
        s->controller->Unacquire();
    }
    if (s->controller != NULL)
    {
        s->controller->Release();
        s->controller = NULL;
    }
    if (s->dinputIface != NULL)
    {
        s->dinputIface->Release();
        s->dinputIface = NULL;
    }
    return ZUN_SUCCESS;
}

void Supervisor::DrawFpsCounter()
{
    float framerate;
    float elapsed;
    float fps;
    D3DXVECTOR3 fpsCounterPos;
    f32 fpsCharW;

    static u32 g_NumFramesSinceLastTime = 0;
    static LARGE_INTEGER g_QpcFrequency = {0};
    static LARGE_INTEGER g_LastTime = {0};
    static char g_FpsCounterBuffer[256];

    LARGE_INTEGER curTime;
    if (g_QpcFrequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&g_QpcFrequency);
        QueryPerformanceCounter(&g_LastTime);
    }
    QueryPerformanceCounter(&curTime);
    g_NumFramesSinceLastTime = g_NumFramesSinceLastTime + 1 + (u32)g_Supervisor.cfg.frameskipConfig;
    if ((curTime.QuadPart - g_LastTime.QuadPart) >= g_QpcFrequency.QuadPart)
    {
        elapsed = (float)(curTime.QuadPart - g_LastTime.QuadPart) / (float)g_QpcFrequency.QuadPart;
        fps = g_NumFramesSinceLastTime / elapsed;
        g_LastTime = curTime;
        g_NumFramesSinceLastTime = 0;
        sprintf(g_FpsCounterBuffer, "%.02ffps", fps);
        if (g_GameManager.isInMenu != 0)
        {
            framerate = 60.f / g_Supervisor.framerateMultiplier;
            g_Supervisor.unk1b8 = g_Supervisor.unk1b8 + framerate;

            if (framerate * .89999998f < fps)
                g_Supervisor.unk1b4 = g_Supervisor.unk1b4 + framerate;
            else if (framerate * 0.69999999f < fps)
                g_Supervisor.unk1b4 = framerate * .8f + g_Supervisor.unk1b4;
            else if (framerate * 0.5f < fps)
                g_Supervisor.unk1b4 = framerate * .6f + g_Supervisor.unk1b4;
            else
                g_Supervisor.unk1b4 = framerate * .5f + g_Supervisor.unk1b4;
        }
    }
    if (!g_Supervisor.isInEnding)
    {
        fpsCharW = (f32)strlen(g_FpsCounterBuffer) * 14.0f;
        if (g_VerticalMode && GameWindow::verticalSurface != NULL)
        {

            AnmVm *vm = &g_AsciiManager.vm0;
            u32 savedAnchor = vm->flags.anchor;
            u32 savedVisible = vm->flags.isVisible;
            D3DXVECTOR3 savedPos = vm->pos;
            f32 savedScaleX = vm->scaleX;
            f32 savedScaleY = vm->scaleY;
            ZunColor savedColor = vm->color;
            AnmLoadedSprite *savedSprite = vm->sprite;

            vm->flags.anchor = AnmVmAnchor_TopLeft;
            vm->flags.isVisible = 1;
            vm->flags.flag1 = 1;
            vm->scaleX = 1.0f;
            vm->scaleY = 1.0f;
            vm->color = 0xFFFFFFFF;

            const f32 sidebarX = 496.0f;
            const f32 pointRowY = 226.0f;
            const f32 fpsRowY = pointRowY + 20.0f;

            f32 x = sidebarX;
            for (const char *ch = g_FpsCounterBuffer; *ch != '\0'; ch++)
            {
                if (*ch != ' ')
                {
                    vm->sprite = &g_AnmManager->sprites[(unsigned char)*ch - 0x15];
                    vm->pos = D3DXVECTOR3(x, fpsRowY, 0.49f);
                    g_AnmManager->DrawNoRotation(vm);
                }
                x += 14.0f;
            }

            vm->flags.anchor = savedAnchor;
            vm->flags.isVisible = savedVisible;
            vm->pos = savedPos;
            vm->scaleX = savedScaleX;
            vm->scaleY = savedScaleY;
            vm->color = savedColor;
            vm->sprite = savedSprite;
        }
        else
        {
            fpsCounterPos.x = 640.0f - fpsCharW;
            fpsCounterPos.y = 464.0f;
            fpsCounterPos.z = 0.0;
            g_AsciiManager.AddString(&fpsCounterPos, g_FpsCounterBuffer);
        }
    }
    return;
}

void Supervisor::TickTimer(i32 *frames, f32 *subframes)
{
    if (this->framerateMultiplier <= 0.99f)
    {
        *subframes = *subframes + this->effectiveFramerateMultiplier;
        if (*subframes >= 1.0f)
        {
            *frames = *frames + 1;
            *subframes = *subframes - 1.0f;
        }
    }
    else
    {
        *frames = *frames + 1;
    }
}

void Supervisor::ReleasePbg3(i32 pbg3FileIdx)
{
    if (this->pbg3Archives[pbg3FileIdx] == NULL)
    {
        return;
    }

    delete this->pbg3Archives[pbg3FileIdx];
    this->pbg3Archives[pbg3FileIdx] = NULL;
}

i32 Supervisor::LoadPbg3(i32 pbg3FileIdx, char *filename)
{
    if (this->pbg3Archives[pbg3FileIdx] == NULL || strcmp(filename, this->pbg3ArchiveNames[pbg3FileIdx]) != 0)
    {
        this->ReleasePbg3(pbg3FileIdx);
        this->pbg3Archives[pbg3FileIdx] = new Pbg3Archive();
        utils::DebugPrint("%s open ...\n", filename);
        if (this->pbg3Archives[pbg3FileIdx]->Load(filename) != 0)
        {
            strncpy(this->pbg3ArchiveNames[pbg3FileIdx], filename, sizeof(Pbg3ArchiveName) - 1);
            this->pbg3ArchiveNames[pbg3FileIdx][sizeof(Pbg3ArchiveName) - 1] = '\0';

            char verPath[128];
            sprintf(verPath, "ver%.4x.dat", GAME_VERSION);
            i32 res = this->pbg3Archives[pbg3FileIdx]->FindEntry(verPath);
            if (res < 0)
            {
                g_GameErrorContext.Fatal("error : データのバージョンが違います\n");
                return 1;
            }
        }
        else
        {
            delete this->pbg3Archives[pbg3FileIdx];

            this->pbg3Archives[pbg3FileIdx] = NULL;
            this->pbg3Archives[pbg3FileIdx] = NULL;
        }
    }
    return 0;
}

static void LoadBgmTrackModes(const char *ini)
{
    i32 track;
    i32 value;
    char key[16];

    for (track = 1; track <= BGM_TRACK_COUNT; track++)
    {
        sprintf(key, "track%02d", track);
        value = (i32)GetPrivateProfileIntA("bgm", key, -1, ini);
        if (value == -1)
        {
            WritePrivateProfileStringA("bgm", key, "0", ini);
            value = 0;
        }
        if (value != WAV && value != MIDI)
        {
            value = 0;
        }
        g_BgmTrackMode[track] = value;
    }
}

static bool HasIniSection(const char *iniPath, const char *section)
{
    FILE *f = fopen(iniPath, "r");
    if (!f)
        return false;
    char line[256];
    char target[64];
    snprintf(target, sizeof(target), "[%s]", section);
    bool found = false;
    while (fgets(line, sizeof(line), f))
    {
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (_strnicmp(p, target, strlen(target)) == 0)
        {
            found = true;
            break;
        }
    }
    fclose(f);
    return found;
}

ZunResult Supervisor::LoadConfig(char *path)
{
    {
        char _ini[MAX_PATH];
        GetFullPathNameA("./custom_new.ini", MAX_PATH, _ini, NULL);
        FILE *_f = fopen(_ini, "rb");
        if (!_f)
        {
            WritePrivateProfileStringA("display", "show_input", "0", _ini);
            WritePrivateProfileStringA("display", "move_shift", "0", _ini);
            WritePrivateProfileStringA("display", "window_width", "0", _ini);
            WritePrivateProfileStringA("display", "window_height", "0", _ini);
            WritePrivateProfileStringA("display", "window_pos_x", "-32000", _ini);
            WritePrivateProfileStringA("display", "window_pos_y", "-32000", _ini);
            WritePrivateProfileStringA("display", "show_collision", "0", _ini);
            WritePrivateProfileStringA("display", "hitdisp", "0", _ini);
            WritePrivateProfileStringA("display", "boss_hp_circle", "0", _ini);
            WritePrivateProfileStringA("display", "enemy_marker", "0", _ini);
            WritePrivateProfileStringA("display", "show_difficulty", "0", _ini);
            WritePrivateProfileStringA("display", "show_frame_counter", "0", _ini);
            WritePrivateProfileStringA("display", "vsync", "0", _ini);
            WritePrivateProfileStringA("display", "vertical_mode", "0", _ini);
            WritePrivateProfileStringA("system", "backup", "0", _ini);
            WritePrivateProfileStringA("system", "backup_keep_count", "30", _ini);
            WritePrivateProfileStringA("audio", "bgm_volume", "100", _ini);
            WritePrivateProfileStringA("audio", "se_volume", "100", _ini);
            WritePrivateProfileStringA("audio", "internal_synth", "1", _ini);
            WritePrivateProfileStringA("audio", "sf2", "", _ini);
        }
        else
        {
            fclose(_f);
            if (GetPrivateProfileIntA("display", "show_input", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "show_input", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "move_shift", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "move_shift", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "window_width", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "window_width", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "window_height", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "window_height", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "window_pos_x", -32000, _ini) == -32000)
            {
                WritePrivateProfileStringA("display", "window_pos_x", "-32000", _ini);
            }
            if (GetPrivateProfileIntA("display", "window_pos_y", -32000, _ini) == -32000)
            {
                WritePrivateProfileStringA("display", "window_pos_y", "-32000", _ini);
            }
            if (GetPrivateProfileIntA("display", "show_collision", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "show_collision", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "hitdisp", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "hitdisp", "0", _ini);
            }
            if (GetPrivateProfileIntA("system", "backup", -1, _ini) == -1 &&
                GetPrivateProfileIntA("display", "backup", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("system", "backup", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "boss_hp_circle", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "boss_hp_circle", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "enemy_marker", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "enemy_marker", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "show_difficulty", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "show_difficulty", "0", _ini);
            }
            if (GetPrivateProfileIntA("display", "show_frame_counter", -1, _ini) == -1)
            {
                WritePrivateProfileStringA("display", "show_frame_counter", "0", _ini);
            }
        }
        g_ShowInputDisplay = GetPrivateProfileIntA("display", "show_input", 0, _ini);
        g_MoveShift = GetPrivateProfileIntA("display", "move_shift", 0, _ini);
        g_CustomWindowWidth = GetPrivateProfileIntA("display", "window_width", 0, _ini);
        g_CustomWindowHeight = GetPrivateProfileIntA("display", "window_height", 0, _ini);
        g_CustomWindowPosX = GetPrivateProfileIntA("display", "window_pos_x", -32000, _ini);
        g_CustomWindowPosY = GetPrivateProfileIntA("display", "window_pos_y", -32000, _ini);
        g_ShowCollision = GetPrivateProfileIntA("display", "show_collision", 0, _ini);
        g_HitDisp = GetPrivateProfileIntA("display", "hitdisp", 0, _ini);
        g_Backup = GetPrivateProfileIntA("system", "backup", -1, _ini);
        if (g_Backup == -1)
        {
            g_Backup = GetPrivateProfileIntA("display", "backup", 0, _ini);
        }
        g_BackupKeepCount = GetPrivateProfileIntA("system", "backup_keep_count", -1, _ini);
        if (g_BackupKeepCount == -1)
        {
            WritePrivateProfileStringA("system", "backup_keep_count", "30", _ini);
            g_BackupKeepCount = 30;
        }
        g_BossHpCircle = GetPrivateProfileIntA("display", "boss_hp_circle", 0, _ini);
        g_ShowEnemyMarker = GetPrivateProfileIntA("display", "enemy_marker", 0, _ini);
        g_ShowDifficultyLabel = GetPrivateProfileIntA("display", "show_difficulty", 0, _ini);
        g_ShowFrameCounter = GetPrivateProfileIntA("display", "show_frame_counter", 0, _ini);

        if (GetPrivateProfileIntA("audio", "bgm_volume", -1, _ini) == -1)
        {
            WritePrivateProfileStringA("audio", "bgm_volume", "100", _ini);
        }
        if (GetPrivateProfileIntA("audio", "se_volume", -1, _ini) == -1)
        {
            WritePrivateProfileStringA("audio", "se_volume", "100", _ini);
        }
        g_CustomBgmVolume = GetPrivateProfileIntA("audio", "bgm_volume", 100, _ini);

        if (GetPrivateProfileIntA("audio", "bgm_volume_midi", -1, _ini) == -1)
        {
            char midiVolBuf[16];
            sprintf(midiVolBuf, "%d", (int)g_CustomBgmVolume);
            WritePrivateProfileStringA("audio", "bgm_volume_midi", midiVolBuf, _ini);
        }
        g_CustomBgmVolumeMidi = GetPrivateProfileIntA("audio", "bgm_volume_midi", 100, _ini);
        g_CustomSeVolume = GetPrivateProfileIntA("audio", "se_volume", 100, _ini);
        LoadBgmTrackModes(_ini);

        if (GetPrivateProfileIntA("audio", "internal_synth", -1, _ini) == -1)
        {
            int oldMidi = GetPrivateProfileIntA("midi", "internal_synth", -1, _ini);
            if (oldMidi != -1)
            {
                g_MidiSynthType = (oldMidi == 1) ? 1 : 0;
            }
            else
            {
                g_MidiSynthType = 1;
            }
            WritePrivateProfileStringA("audio", "internal_synth", g_MidiSynthType ? "1" : "0", _ini);
        }
        else
        {
            g_MidiSynthType = GetPrivateProfileIntA("audio", "internal_synth", 1, _ini);
        }

        GetPrivateProfileStringA("audio", "sf2", "", g_MidiSoundFont, sizeof(g_MidiSoundFont), _ini);

        SoundFontSynth::Initialize();

        g_IsOnline = 0;

        g_InputDelay = 0;

        if (GetPrivateProfileIntA("display", "vsync", -1, _ini) == -1)
        {
            WritePrivateProfileStringA("display", "vsync", "0", _ini);
        }
        g_Vsync = GetPrivateProfileIntA("display", "vsync", 0, _ini);
        if (GetPrivateProfileIntA("display", "vertical_mode", -1, _ini) == -1)
        {
            WritePrivateProfileStringA("display", "vertical_mode", "0", _ini);
        }
        g_VerticalMode = GetPrivateProfileIntA("display", "vertical_mode", 0, _ini);

        if (!HasIniSection(_ini, "input"))
        {
            FILE *_fi = fopen(_ini, "ab");
            if (_fi != NULL)
            {
                fputs("\r\n[input]\r\n"
                      "; DirectInput (DIK_*) scan code reference:\r\n"
                      ";  1=Escape   2=1       3=2       4=3       5=4       6=5       7=6       8=7\r\n"
                      ";  9=8       10=9      11=0      12=Minus  13=Equals 14=Back   15=Tab    16=Q\r\n"
                      "; 17=W       18=E      19=R      20=T      21=Y      22=U      23=I      24=O\r\n"
                      "; 25=P       26=LBrack 27=RBrack 28=Return 29=LCtrl  30=A      31=S      32=D\r\n"
                      "; 33=F       34=G      35=H      36=J      37=K      38=L      39=Semi   40=Apost\r\n"
                      "; 41=Grave   42=LShift 43=BackSl 44=Z      45=X      46=C      47=V      48=B\r\n"
                      "; 49=N       50=M      51=Comma  52=Period 53=Slash  54=RShift 56=LAlt   57=Space\r\n"
                      "; 58=Caps    59=F1     60=F2     61=F3     62=F4     63=F5     64=F6     65=F7\r\n"
                      "; 66=F8      67=F9     68=F10    87=F11    88=F12    157=RCtrl 184=RAlt\r\n"
                      "; 71=Num7   72=Num8   73=Num9   75=Num4   76=Num5   77=Num6   79=Num1   80=Num2   81=Num3   "
                      "82=Num0\r\n"
                      "; 200=Up    208=Down  203=Left  205=Right\r\n",
                      _fi);
                fclose(_fi);
            }
        }
        g_KeyConfig.shoot = GetPrivateProfileIntA("input", "shoot", 0, _ini);
        g_KeyConfig.bomb = GetPrivateProfileIntA("input", "bomb", 0, _ini);
        g_KeyConfig.focus = GetPrivateProfileIntA("input", "focus", 0, _ini);
        g_KeyConfig.menu = GetPrivateProfileIntA("input", "menu", 0, _ini);
        g_KeyConfig.up = GetPrivateProfileIntA("input", "up", 0, _ini);
        g_KeyConfig.down = GetPrivateProfileIntA("input", "down", 0, _ini);
        g_KeyConfig.left = GetPrivateProfileIntA("input", "left", 0, _ini);
        g_KeyConfig.right = GetPrivateProfileIntA("input", "right", 0, _ini);
        g_KeyConfig.skip = GetPrivateProfileIntA("input", "skip", 0, _ini);
    }

    GameConfiguration *data;
    FILE *wavFile;
    FILE *wavFile2;

    memset(&g_Supervisor.cfg, 0, sizeof(GameConfiguration));
    g_Supervisor.cfg.opts = g_Supervisor.cfg.opts | (1 << GCOS_USE_D3D_HW_TEXTURE_BLENDING);
    data = (GameConfiguration *)FileSystem::OpenPath(path, 1);
    if (data == NULL)
    {
        g_Supervisor.cfg.lifeCount = 2;
        g_Supervisor.cfg.bombCount = 3;
        g_Supervisor.cfg.colorMode16bit = 0;
        g_Supervisor.cfg.version = GAME_VERSION;
        g_Supervisor.cfg.padXAxis = 600;
        g_Supervisor.cfg.padYAxis = 600;
        wavFile = fopen("bgm/th06_01.wav", "rb");
        if (wavFile != NULL)
        {
            g_Supervisor.cfg.musicMode = WAV;
            fclose(wavFile);
        }
        else
        {
            g_Supervisor.cfg.musicMode = MIDI;
            utils::DebugPrint(TH_ERR_NO_WAVE_FILE);
        }
        g_Supervisor.cfg.playSounds = 1;
        g_Supervisor.cfg.defaultDifficulty = 1;
        g_Supervisor.cfg.windowed = false;
        g_Supervisor.cfg.frameskipConfig = 0;
        g_Supervisor.cfg.controllerMapping = g_ControllerMapping;
        g_GameErrorContext.Log(TH_ERR_CONFIG_NOT_FOUND);
    }
    else
    {
        g_Supervisor.cfg = *data;
        if ((g_Supervisor.cfg.lifeCount >= 5) || (g_Supervisor.cfg.bombCount >= 4) ||
            (g_Supervisor.cfg.colorMode16bit >= 2) || (g_Supervisor.cfg.musicMode >= 3) ||
            (g_Supervisor.cfg.defaultDifficulty >= 5) || (g_Supervisor.cfg.playSounds >= 2) ||
            (g_Supervisor.cfg.windowed >= 2) || (g_Supervisor.cfg.frameskipConfig >= 3) ||
            (g_Supervisor.cfg.version != GAME_VERSION) || (g_LastFileSize != 0x38))
        {
            g_Supervisor.cfg.lifeCount = 2;
            g_Supervisor.cfg.bombCount = 3;
            g_Supervisor.cfg.colorMode16bit = 0xff;
            g_Supervisor.cfg.version = GAME_VERSION;
            g_Supervisor.cfg.padXAxis = 600;
            g_Supervisor.cfg.padYAxis = 600;
            wavFile2 = fopen("bgm/th06_01.wav", "rb");
            if (wavFile2 != NULL)
            {
                g_Supervisor.cfg.musicMode = WAV;
                fclose(wavFile2);
            }
            else
            {
                g_Supervisor.cfg.musicMode = MIDI;
                utils::DebugPrint(TH_ERR_NO_WAVE_FILE);
            }
            g_Supervisor.cfg.playSounds = 1;
            g_Supervisor.cfg.defaultDifficulty = 1;
            g_Supervisor.cfg.windowed = false;
            g_Supervisor.cfg.frameskipConfig = 0;
            g_Supervisor.cfg.controllerMapping = g_ControllerMapping;
            memset(&g_Supervisor.cfg.opts, 0, sizeof(GameConfigOptsShifts));
            g_Supervisor.cfg.opts |= (1 << GCOS_USE_D3D_HW_TEXTURE_BLENDING);
            g_GameErrorContext.Log(TH_ERR_CONFIG_CORRUPTED);
        }
        g_ControllerMapping = g_Supervisor.cfg.controllerMapping;
        free(data);
    }

    g_Supervisor.cfg.colorMode16bit = 0;
    g_Supervisor.cfg.opts &= ~(1 << GCOS_FORCE_16BIT_COLOR_MODE);

    if (g_VerticalMode)
        g_Supervisor.cfg.windowed = false;
    if (((this->cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_NO_VERTEX_BUFFER);
    }
    if (((this->cfg.opts >> GCOS_DONT_USE_FOG) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_NO_FOG);
    }
    if (((this->cfg.opts >> GCOS_FORCE_16BIT_COLOR_MODE) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_USE_16BIT_TEXTURES);
    }
    if (this->IsUnknown())
    {
        g_GameErrorContext.Log(TH_ERR_FORCE_BACKBUFFER_CLEAR);
    }
    if (((this->cfg.opts >> GCOS_DISPLAY_MINIMUM_GRAPHICS) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_DONT_RENDER_ITEMS);
    }
    if (((this->cfg.opts >> GCOS_SUPPRESS_USE_OF_GOROUD_SHADING) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_NO_GOURAUD_SHADING);
    }
    if (((this->cfg.opts >> GCOS_TURN_OFF_DEPTH_TEST) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_NO_DEPTH_TESTING);
    }
    if (((this->cfg.opts >> GCOS_FORCE_60FPS) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_FORCE_60FPS_MODE);
        this->vsyncEnabled = 0;
    }
    if (((this->cfg.opts >> GCOS_NO_COLOR_COMP) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_NO_TEXTURE_COLOR_COMPOSITING);
    }
    if (((this->cfg.opts >> GCOS_NO_COLOR_COMP) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_LAUNCH_WINDOWED);
    }
    if (((this->cfg.opts >> GCOS_REFERENCE_RASTERIZER_MODE) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_FORCE_REFERENCE_RASTERIZER);
    }
    if (((this->cfg.opts >> GCOS_NO_DIRECTINPUT_PAD) & 1) != 0)
    {
        g_GameErrorContext.Log(TH_ERR_DO_NOT_USE_DIRECTINPUT);
    }
    if (FileSystem::WriteDataToFile(path, &g_Supervisor.cfg, sizeof(GameConfiguration)) != 0)
    {
        g_GameErrorContext.Fatal(TH_ERR_FILE_CANNOT_BE_EXPORTED, path);
        g_GameErrorContext.Fatal(TH_ERR_FOLDER_HAS_WRITE_PROTECT_OR_DISK_FULL);
        return ZUN_ERROR;
    }

    char _ini[MAX_PATH];
    GetFullPathNameA("./custom_new.ini", MAX_PATH, _ini, NULL);
    char buf[16];
    sprintf(buf, "%d", g_CustomWindowPosX);
    WritePrivateProfileStringA("display", "window_pos_x", buf, _ini);
    sprintf(buf, "%d", g_CustomWindowPosY);
    WritePrivateProfileStringA("display", "window_pos_y", buf, _ini);

    return ZUN_SUCCESS;
}

static i32 ParseBgmTrackNo(const char *path)
{
    const char *p = strstr(path, "th06_");

    if (p == NULL)
    {
        return 0;
    }
    p += 5;
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9')
    {
        return 0;
    }
    return (p[0] - '0') * 10 + (p[1] - '0');
}

static bool BgmWavExists(const char *path)
{
    char wavName[256];
    char *pathExtension;
    FILE *wavFile;

    strncpy(wavName, path, sizeof(wavName) - 1);
    wavName[sizeof(wavName) - 1] = '\0';
    pathExtension = strrchr(wavName, '.');
    if (pathExtension == NULL || strlen(pathExtension) < 4)
    {
        return false;
    }
    pathExtension[1] = 'w';
    pathExtension[2] = 'a';
    pathExtension[3] = 'v';
    wavFile = fopen(wavName, "rb");
    if (wavFile == NULL)
    {
        return false;
    }
    fclose(wavFile);
    return true;
}

i32 Supervisor::ResolveMusicMode(const char *path)
{
    i32 mode = g_Supervisor.cfg.musicMode;
    i32 track;

    if (mode == OFF)
    {
        return OFF;
    }

    if (s_musicModeOverride == MIDI)
    {
        return MIDI;
    }
    if (s_musicModeOverride == WAV)
    {

        return BgmWavExists(path) ? WAV : MIDI;
    }

    track = ParseBgmTrackNo(path);
    if (track >= 1 && track <= BGM_TRACK_COUNT)
    {
        if (g_BgmTrackMode[track] == MIDI)
        {
            mode = MIDI;
        }
        else if (g_BgmTrackMode[track] == WAV)
        {

            mode = BgmWavExists(path) ? WAV : MIDI;
        }
    }
    return mode;
}

i32 Supervisor::ActiveMusicMode()
{
    return s_activeMusicMode;
}

void Supervisor::SetMusicModeOverride(i32 mode)
{
    s_musicModeOverride = (mode == WAV || mode == MIDI) ? mode : 0;
}

i32 Supervisor::GetMusicModeOverride()
{
    return s_musicModeOverride;
}

ZunBool Supervisor::ReadMidiFile(u32 midiFileIdx, char *path)
{
    if (midiFileIdx < 32)
    {
        s_midiSlotLoaded[midiFileIdx] = false;
    }

    if (ResolveMusicMode(path) == MIDI)
    {
        if (g_Supervisor.midiOutput != NULL && midiFileIdx < 32)
        {
            if (g_Supervisor.midiOutput->ReadFileData(midiFileIdx, path) == ZUN_SUCCESS)
            {
                strncpy(s_midiSlotPath[midiFileIdx], path, sizeof(s_midiSlotPath[midiFileIdx]) - 1);
                s_midiSlotPath[midiFileIdx][sizeof(s_midiSlotPath[midiFileIdx]) - 1] = '\0';
                s_midiSlotLoaded[midiFileIdx] = true;
            }
        }

        return FALSE;
    }

    return TRUE;
}

i32 Supervisor::PlayMidiFile(i32 midiFileIdx)
{
    MidiOutput *globalMidiController;

    if (midiFileIdx >= 0 && midiFileIdx < 32 && s_midiSlotLoaded[midiFileIdx] &&
        ResolveMusicMode(s_midiSlotPath[midiFileIdx]) == MIDI)
    {
        if (g_Supervisor.midiOutput != NULL)
        {
            if (s_activeMusicMode == WAV)
            {
                g_SoundPlayer.StopBGM();
            }
            globalMidiController = g_Supervisor.midiOutput;
            globalMidiController->StopPlayback();
            globalMidiController->ParseFile(midiFileIdx);
            globalMidiController->Play();
            s_activeMusicMode = MIDI;
        }

        return FALSE;
    }

    return TRUE;
}

ZunResult Supervisor::SetupMidiPlayback(char *path)
{
    if (g_Supervisor.cfg.musicMode == MIDI || g_Supervisor.cfg.musicMode == WAV)
    {
        return ZUN_SUCCESS;
    }
    return ZUN_ERROR;
}

ZunResult Supervisor::PlayAudio(char *path)
{
    char wavName[256];
    char wavPos[256];
    char *pathExtension;

    i32 mode = Supervisor::ResolveMusicMode(path);

    if (mode == MIDI)
    {
        if (g_Supervisor.midiOutput != NULL)
        {
            MidiOutput *midiOutput = g_Supervisor.midiOutput;
            if (s_activeMusicMode == WAV)
            {
                g_SoundPlayer.StopBGM();
            }
            midiOutput->StopPlayback();
            midiOutput->LoadFile(path);
            midiOutput->Play();
            s_activeMusicMode = MIDI;
        }
    }
    else if (mode == WAV)
    {
        if (s_activeMusicMode == MIDI && g_Supervisor.midiOutput != NULL)
        {
            g_Supervisor.midiOutput->StopPlayback();
        }
        s_activeMusicMode = WAV;
        strcpy(wavName, path);
        strcpy(wavPos, path);
        pathExtension = strrchr(wavName, L'.');
        pathExtension[1] = 'w';
        pathExtension[2] = 'a';
        pathExtension[3] = 'v';
        pathExtension = strrchr(wavPos, L'.');
        pathExtension[1] = 'p';
        pathExtension[2] = 'o';
        pathExtension[3] = 's';
        g_SoundPlayer.LoadWav(wavName);
        if (g_SoundPlayer.LoadPos(wavPos) < ZUN_SUCCESS)
        {
            g_SoundPlayer.PlayBGM(FALSE);
        }
        else
        {
            g_SoundPlayer.PlayBGM(TRUE);
        }
    }
    else
    {
        return ZUN_ERROR;
    }
    return ZUN_SUCCESS;
}

ZunResult Supervisor::StopAudio()
{

    if (s_activeMusicMode == MIDI)
    {
        if (g_Supervisor.midiOutput != NULL)
        {
            g_Supervisor.midiOutput->StopPlayback();
        }
    }
    else
    {
        if (s_activeMusicMode == WAV)
        {
            g_SoundPlayer.StopBGM();
        }
        else
        {
            return ZUN_ERROR;
        }
    }

    return ZUN_SUCCESS;
}

ZunResult Supervisor::PauseAudio()
{
    if (s_activeMusicMode == MIDI)
    {
        if (g_Supervisor.midiOutput != NULL)
        {
            return g_Supervisor.midiOutput->PausePlayback();
        }
    }
    else if (s_activeMusicMode == WAV)
    {
        return g_SoundPlayer.PauseBGM();
    }

    return ZUN_ERROR;
}

ZunResult Supervisor::ResumeAudio()
{
    if (s_activeMusicMode == MIDI)
    {
        if (g_Supervisor.midiOutput != NULL)
        {
            return g_Supervisor.midiOutput->ResumePlayback();
        }
    }
    else if (s_activeMusicMode == WAV)
    {
        return g_SoundPlayer.ResumeBGM();
    }

    return ZUN_ERROR;
}

ZunResult Supervisor::FadeOutMusic(f32 fadeOutSeconds)
{
    if (s_activeMusicMode == MIDI)
    {
        if (g_Supervisor.midiOutput != NULL)
        {
            g_Supervisor.midiOutput->SetFadeOut(1000.0f * fadeOutSeconds);
        }
    }
    else
    {
        if (s_activeMusicMode == WAV)
        {
            if (this->effectiveFramerateMultiplier == 0.0f)
            {
                g_SoundPlayer.FadeOut(fadeOutSeconds);
            }
            else
            {
                if (this->effectiveFramerateMultiplier > 1.0f)
                {
                    g_SoundPlayer.FadeOut(fadeOutSeconds);
                }
                else
                {
                    g_SoundPlayer.FadeOut(fadeOutSeconds / this->effectiveFramerateMultiplier);
                }
            }
        }
        else
        {
            return ZUN_ERROR;
        }
    }

    return ZUN_SUCCESS;
}

void Supervisor::SaveWindowSettings()
{
    if (g_Supervisor.cfg.windowed)
    {
        char _ini[MAX_PATH];
        GetFullPathNameA("./custom_new.ini", MAX_PATH, _ini, NULL);
        char buf[16];
        sprintf(buf, "%d", g_CustomWindowPosX);
        WritePrivateProfileStringA("display", "window_pos_x", buf, _ini);
        sprintf(buf, "%d", g_CustomWindowPosY);
        WritePrivateProfileStringA("display", "window_pos_y", buf, _ini);
        sprintf(buf, "%d", g_CustomWindowWidth);
        WritePrivateProfileStringA("display", "window_width", buf, _ini);
        sprintf(buf, "%d", g_CustomWindowHeight);
        WritePrivateProfileStringA("display", "window_height", buf, _ini);
    }
}

}; // namespace th06
