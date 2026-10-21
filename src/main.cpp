#define _WIN32_WINNT 0x0500

#include <mmsystem.h>
#include <windows.h>

#include <D3DX8.h>
#include <cstdlib>
#include <stdio.h>

#include "AnmManager.hpp"
#include "Chain.hpp"
#include "FileSystem.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "MidiOutput.hpp"
#include "SoundPlayer.hpp"
#include "Stage.hpp"
#include "Supervisor.hpp"
#include "ZunResult.hpp"
#include "i18n.hpp"
#include "utils.hpp"
#include <signal.h>

#ifdef USE_VULKAN
#include "platform_sdl.hpp"
#include "renderer.hpp"
#endif

using namespace th06;

static void MidiResetAtExit()
{
    MidiForceResetAll();
}

static LONG WINAPI CrashHandler(EXCEPTION_POINTERS *pExceptionInfo)
{
    (void)pExceptionInfo;
    MidiForceResetAll();
    return EXCEPTION_CONTINUE_SEARCH;
}

static void SignalHandler(int signum)
{
    (void)signum;
    MidiForceResetAll();
    _exit(128 + signum);
}

static BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    (void)dwCtrlType;
    MidiForceResetAll();
    return FALSE;
}

static LONG WINAPI VectoredCrashHandler(PEXCEPTION_POINTERS pExceptionInfo)
{
    (void)pExceptionInfo;
    if (pExceptionInfo && pExceptionInfo->ExceptionRecord)
    {
        DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
        if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
            code == EXCEPTION_STACK_OVERFLOW)
        {
            MidiForceResetAll();
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void SetCurrentDirectoryToModuleDir()
{
    char modulePath[MAX_PATH];
    DWORD modulePathLen;
    char *lastSlash;

    modulePathLen = GetModuleFileNameA(NULL, modulePath, MAX_PATH);
    if (modulePathLen == 0 || modulePathLen >= MAX_PATH)
    {
        return;
    }

    lastSlash = strrchr(modulePath, '\\');
    if (lastSlash == NULL)
    {
        lastSlash = strrchr(modulePath, '/');
    }
    if (lastSlash == NULL)
    {
        return;
    }

    *lastSlash = '\0';
    SetCurrentDirectoryA(modulePath);
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{

    MidiForceResetAll();

    atexit(MidiResetAtExit);
    SetUnhandledExceptionFilter(CrashHandler);
    AddVectoredExceptionHandler(1, VectoredCrashHandler);
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    signal(SIGINT, SignalHandler);
    signal(SIGTERM, SignalHandler);
    signal(SIGABRT, SignalHandler);
#ifdef SIGBREAK
    signal(SIGBREAK, SignalHandler);
#endif

    timeBeginPeriod(1);

    th06::utils::LogDebug("WinMain: Started");

    i32 renderResult;
    i32 testCoopLevelRes;
    i32 testResetRes;
    MSG msg;

    if (utils::CheckForRunningGameInstance())
    {
        g_GameErrorContext.Flush();

        return 1;
    }

    g_Supervisor.hInstance = hInstance;
    SetCurrentDirectoryToModuleDir();

    th06::utils::LogDebug("WinMain: Current directory set to module dir");

    if (g_Supervisor.LoadConfig(TH_CONFIG_FILE) != ZUN_SUCCESS)
    {
        g_GameErrorContext.Flush();
        return -1;
    }

    if (GameWindow::InitD3dInterface())
    {
        g_GameErrorContext.Flush();
        return 1;
    }

    SystemParametersInfo(SPI_GETSCREENSAVEACTIVE, 0, &g_GameWindow.screenSaveActive, 0);
    SystemParametersInfo(SPI_GETLOWPOWERACTIVE, 0, &g_GameWindow.lowPowerActive, 0);
    SystemParametersInfo(SPI_GETPOWEROFFACTIVE, 0, &g_GameWindow.powerOffActive, 0);
    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, 0, NULL, SPIF_SENDCHANGE);
    SystemParametersInfo(SPI_SETLOWPOWERACTIVE, 0, NULL, SPIF_SENDCHANGE);
    SystemParametersInfo(SPI_SETPOWEROFFACTIVE, 0, NULL, SPIF_SENDCHANGE);

restart:
    th06::utils::LogDebug("WinMain: Creating Game Window");
    GameWindow::CreateGameWindow(hInstance);

    if (GameWindow::InitD3dRendering())
    {

        if (GetFileAttributesA("d3d8.dll") == INVALID_FILE_ATTRIBUTES)
        {
            g_GameErrorContext.Log("d3d8.dll not found in game folder.\n"
                                   "On Windows 8/10/11, place d3d8to9's d3d8.dll here.\n"
                                   "Download: https://github.com/crosire/d3d8to9/releases\n");
        }
        g_GameErrorContext.Flush();
        return 1;
    }

    th06::utils::LogDebug("WinMain: Initializing DSound");
    ZunResult dsoundInitRes = g_SoundPlayer.InitializeDSound(g_GameWindow.window);
    th06::utils::LogDebug("WinMain: DSound Initialized (result=%d manager=%p)", dsoundInitRes,
                          (LPVOID)g_SoundPlayer.manager);
    if (dsoundInitRes != ZUN_SUCCESS)
    {
        th06::utils::LogDebug("WinMain: DSound init FAILED - SE and WAV BGM will be silent");
    }
    Controller::GetJoystickCaps();
    Controller::ResetKeyboard();
    Supervisor::ResetInputState();
    Supervisor::SuppressInputFrames(8);

    g_AnmManager = new AnmManager();

    th06::utils::LogDebug("WinMain: Registering Supervisor Chain");
    if (Supervisor::RegisterChain() != ZUN_SUCCESS)
    {
        goto stop;
    }
    th06::utils::LogDebug("WinMain: Supervisor Chain Registered");
    if (!g_Supervisor.cfg.windowed)
    {
        ShowCursor(FALSE);
    }

    g_GameWindow.curFrame = 0;

    while (!g_GameWindow.isAppClosing)
    {
#ifdef USE_VULKAN
        if (!PlatformWindow::Instance().PollEvents())
        {
            g_GameWindow.isAppClosing = 1;
            break;
        }
#endif
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            testCoopLevelRes = g_Supervisor.d3dDevice->TestCooperativeLevel();
            if (testCoopLevelRes == D3D_OK)
            {
                renderResult = g_GameWindow.Render();
                if (renderResult != 0)
                {
                    goto stop;
                }
            }
            else if (testCoopLevelRes == D3DERR_DEVICENOTRESET)
            {
                g_AnmManager->ReleaseSurfaces();
                GameWindow::ReleaseVerticalSurface();
                testResetRes = g_Supervisor.d3dDevice->Reset(&g_Supervisor.presentParameters);
                if (testResetRes == D3D_OK)
                {
                    GameWindow::InitD3dDevice();
                    if (g_VerticalMode)
                    {
                        GameWindow::CreateVerticalSurface();
                    }
                    g_Supervisor.unk198 = 3;
                }
                else if (testResetRes != D3DERR_DEVICELOST)
                {
                    goto stop;
                }
            }
        }
    }

stop:

    if (g_Supervisor.midiOutput != NULL)
    {
        g_Supervisor.midiOutput->StopPlayback();
    }
    g_Chain.Release();
    g_SoundPlayer.Release();

    delete g_AnmManager;
    g_AnmManager = NULL;
    if (g_Supervisor.d3dDevice != NULL)
    {
        g_Supervisor.d3dDevice->Release();
        g_Supervisor.d3dDevice = NULL;
    }

#ifdef USE_VULKAN
    if (g_Renderer)
    {
        g_Renderer->Shutdown();
        delete g_Renderer;
        g_Renderer = nullptr;
    }
    PlatformWindow::Instance().Shutdown();
#endif

    ShowWindow(g_GameWindow.window, 0);
    MoveWindow(g_GameWindow.window, 0, 0, 0, 0, 0);
    DestroyWindow(g_GameWindow.window);

    if (renderResult == 2)
    {
        g_GameErrorContext.ResetContext();

        g_GameErrorContext.Log(TH_ERR_OPTION_CHANGED_RESTART);

        if (!g_Supervisor.cfg.windowed)
        {
            ShowCursor(TRUE);
        }
        goto restart;
    }

    FileSystem::WriteDataToFile(TH_CONFIG_FILE, &g_Supervisor.cfg, sizeof(g_Supervisor.cfg));

    if (g_Supervisor.cfg.windowed)
    {
        char _ini[MAX_PATH];
        GetFullPathNameA("./custom_new.ini", MAX_PATH, _ini, NULL);
        char buf[16];
        sprintf(buf, "%d", g_CustomWindowPosX);
        WritePrivateProfileStringA("display", "window_pos_x", buf, _ini);
        sprintf(buf, "%d", g_CustomWindowPosY);
        WritePrivateProfileStringA("display", "window_pos_y", buf, _ini);
    }

    SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, g_GameWindow.screenSaveActive, NULL, SPIF_SENDCHANGE);
    SystemParametersInfo(SPI_SETLOWPOWERACTIVE, g_GameWindow.lowPowerActive, NULL, SPIF_SENDCHANGE);
    SystemParametersInfo(SPI_SETPOWEROFFACTIVE, g_GameWindow.powerOffActive, NULL, SPIF_SENDCHANGE);

    if (g_Supervisor.d3dIface != NULL)
    {
        g_Supervisor.d3dIface->Release();
        g_Supervisor.d3dIface = NULL;
    }

    ShowCursor(TRUE);
    g_GameErrorContext.Flush();
    timeEndPeriod(1);
    return 0;
}
