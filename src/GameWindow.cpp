#include "GameWindow.hpp"
#include "AnmManager.hpp"
#include "AsciiManager.hpp"
#include "Controller.hpp"
#include "GameErrorContext.hpp"
#include "GameManager.hpp"
#include "Gui.hpp"
#include "MainMenu.hpp"
#include "MidiOutput.hpp"
#include "ScreenEffect.hpp"
#include "Screenshot.hpp"
#include "SoundPlayer.hpp"
#include "Stage.hpp"
#include "Supervisor.hpp"
#include "diffbuild.hpp"
#include "i18n.hpp"
#include <cstdio>
#include <emmintrin.h>
#include <imm.h>

namespace th06
{
GameWindow g_GameWindow;
i32 g_TickCountToEffectiveFramerate;
f64 g_LastFrameTime;
i32 g_ShouldClearLetterbox;
u8 g_ResetDeviceOnResume;

IDirect3DSurface8 *GameWindow::verticalSurface = NULL;
IDirect3DSurface8 *GameWindow::verticalDepth = NULL;

static bool s_lastFrameWasInGame = false;

static bool s_verticalModeTransitionBlack = false;

static int s_diffBandCentreX_bb = 0;
static int s_diffBandBottom_bb = 0;

#define FRAME_TIME (1000. / 60.)

static RECT GetPreferredFullscreenRect()
{
    MONITORINFO monitorInfo;
    RECT fullscreenRect;
    HMONITOR monitor;

    fullscreenRect.left = 0;
    fullscreenRect.top = 0;
    fullscreenRect.right = GetSystemMetrics(SM_CXSCREEN);
    fullscreenRect.bottom = GetSystemMetrics(SM_CYSCREEN);

    POINT pt = {0, 0};
    monitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    if (monitor != NULL)
    {
        memset(&monitorInfo, 0, sizeof(monitorInfo));
        monitorInfo.cbSize = sizeof(monitorInfo);
        if (GetMonitorInfoA(monitor, &monitorInfo) != 0)
        {
            fullscreenRect = monitorInfo.rcMonitor;
        }
    }

    return fullscreenRect;
}

static DWORD GetWindowedStyle()
{
    return WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME;
}

static void GetWindowFrameSize(DWORD style, LONG *frameWidth, LONG *frameHeight)
{
    RECT windowRect;

    windowRect.left = 0;
    windowRect.top = 0;
    windowRect.right = GAME_WINDOW_WIDTH;
    windowRect.bottom = GAME_WINDOW_HEIGHT;
    AdjustWindowRect(&windowRect, style, FALSE);

    *frameWidth = (windowRect.right - windowRect.left) - GAME_WINDOW_WIDTH;
    *frameHeight = (windowRect.bottom - windowRect.top) - GAME_WINDOW_HEIGHT;
}

RECT GameWindow::GetAspectRatioDestRect(HWND window)
{
    RECT clientRect;
    RECT destRect;
    LONG clientWidth;
    LONG clientHeight;
    LONG scaledWidth;
    LONG scaledHeight;

    GetClientRect(window, &clientRect);
    clientWidth = clientRect.right - clientRect.left;
    clientHeight = clientRect.bottom - clientRect.top;

    LONG srcW, srcH;
    if (g_VerticalMode)
    {
        srcW = GAME_WINDOW_WIDTH;
        srcH = GAME_WINDOW_HEIGHT;
    }
    else
    {
        srcW = GAME_WINDOW_WIDTH;
        srcH = GAME_WINDOW_HEIGHT;
    }

    scaledWidth = clientWidth;
    scaledHeight = (clientWidth * srcH) / srcW;
    if (scaledHeight > clientHeight)
    {
        scaledHeight = clientHeight;
        scaledWidth = (clientHeight * srcW) / srcH;
    }

    destRect.left = (clientWidth - scaledWidth) / 2;
    destRect.top = (clientHeight - scaledHeight) / 2;
    destRect.right = destRect.left + scaledWidth;
    destRect.bottom = destRect.top + scaledHeight;
    return destRect;
}

void GameWindow::CreateVerticalSurface()
{
    ReleaseVerticalSurface();
    D3DDISPLAYMODE mode;
    g_Supervisor.d3dIface->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);

    g_Supervisor.d3dDevice->CreateRenderTarget(GAME_WINDOW_WIDTH, GAME_WINDOW_HEIGHT + 168, mode.Format,
                                               D3DMULTISAMPLE_NONE, TRUE, &verticalSurface);

    g_Supervisor.d3dDevice->CreateDepthStencilSurface(GAME_WINDOW_WIDTH, GAME_WINDOW_HEIGHT + 168, D3DFMT_D16,
                                                      D3DMULTISAMPLE_NONE, &verticalDepth);
}

void GameWindow::ReleaseVerticalSurface()
{
    if (verticalSurface)
    {
        verticalSurface->Release();
        verticalSurface = NULL;
    }
    if (verticalDepth)
    {
        verticalDepth->Release();
        verticalDepth = NULL;
    }
}

static void ConstrainWindowRectToAspect(RECT *windowRect, WPARAM edge)
{
    LONG frameWidth;
    LONG frameHeight;
    LONG targetWindowWidth;
    LONG targetWindowHeight;
    LONG currentWindowWidth;
    LONG currentWindowHeight;

    GetWindowFrameSize(GetWindowedStyle(), &frameWidth, &frameHeight);

    currentWindowWidth = windowRect->right - windowRect->left;
    currentWindowHeight = windowRect->bottom - windowRect->top;

    targetWindowWidth = (((currentWindowHeight - frameHeight) * GAME_WINDOW_WIDTH) / GAME_WINDOW_HEIGHT) + frameWidth;
    targetWindowHeight = (((currentWindowWidth - frameWidth) * GAME_WINDOW_HEIGHT) / GAME_WINDOW_WIDTH) + frameHeight;

    if (targetWindowWidth < currentWindowWidth)
    {
        switch (edge)
        {
        case WMSZ_LEFT:
        case WMSZ_TOPLEFT:
        case WMSZ_BOTTOMLEFT:
            windowRect->left = windowRect->right - targetWindowWidth;
            break;
        default:
            windowRect->right = windowRect->left + targetWindowWidth;
            break;
        }
    }
    else
    {
        switch (edge)
        {
        case WMSZ_TOP:
        case WMSZ_TOPLEFT:
        case WMSZ_TOPRIGHT:
            windowRect->top = windowRect->bottom - targetWindowHeight;
            break;
        default:
            windowRect->bottom = windowRect->top + targetWindowHeight;
            break;
        }
    }
}

static void ClearLetterboxBorders(HWND window, const RECT &destRect)
{
    RECT clientRect;
    RECT clearRect;
    HDC windowDc;

    GetClientRect(window, &clientRect);
    windowDc = GetDC(window);
    if (windowDc == NULL)
    {
        return;
    }

    clearRect = clientRect;
    clearRect.right = destRect.left;
    if (clearRect.right > clearRect.left)
    {
        FillRect(windowDc, &clearRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    clearRect = clientRect;
    clearRect.left = destRect.right;
    if (clearRect.right > clearRect.left)
    {
        FillRect(windowDc, &clearRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    clearRect = clientRect;
    clearRect.bottom = destRect.top;
    clearRect.left = destRect.left;
    clearRect.right = destRect.right;
    if (clearRect.bottom > clearRect.top)
    {
        FillRect(windowDc, &clearRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    clearRect = clientRect;
    clearRect.top = destRect.bottom;
    clearRect.left = destRect.left;
    clearRect.right = destRect.right;
    if (clearRect.bottom > clearRect.top)
    {
        FillRect(windowDc, &clearRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
    }

    ReleaseDC(window, windowDc);
}

RenderResult GameWindow::Render()
{
    i32 res;
    D3DVIEWPORT8 viewport;
    u32 curtime;

    static LARGE_INTEGER qpcLastTime = {0};
    static LARGE_INTEGER qpcFrequency = {0};
    static LARGE_INTEGER qpcNextFrame = {0};
    if (qpcFrequency.QuadPart == 0)
    {
        QueryPerformanceFrequency(&qpcFrequency);
        QueryPerformanceCounter(&qpcLastTime);
        qpcNextFrame = qpcLastTime;
    }

    if (this->lastActiveAppValue == 0)
    {
        HWND foregroundWindow = GetForegroundWindow();

        this->lastActiveAppValue = 1;
        this->isAppActive = 1;
        if (g_Supervisor.keyboard != NULL)
        {
            g_Supervisor.keyboard->Acquire();
        }
        if (g_Supervisor.controller != NULL)
        {
            g_Supervisor.controller->Acquire();
        }
        Controller::ResetKeyboard();
        Supervisor::ResetInputState();
        Supervisor::SuppressInputFrames(8);
        QueryPerformanceCounter(&qpcLastTime);
        qpcNextFrame = qpcLastTime;
    }

    if (g_ResetDeviceOnResume != 0)
    {
        HRESULT resetResult;

        g_AnmManager->ReleaseSurfaces();
        ReleaseVerticalSurface();
        resetResult = g_Supervisor.d3dDevice->Reset(&g_Supervisor.presentParameters);
        if (resetResult != D3D_OK)
        {
            return RENDER_RESULT_KEEP_RUNNING;
        }
        InitD3dDevice();
        if (g_VerticalMode)
        {
            CreateVerticalSurface();
        }
        g_Supervisor.unk198 = 3;
        g_ShouldClearLetterbox = 1;
        g_ResetDeviceOnResume = 0;
    }

    g_Supervisor.viewport.X = 0;
    g_Supervisor.viewport.Y = 0;
    g_Supervisor.viewport.Width = 640;
    g_Supervisor.viewport.Height = 480;
    g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);

    res = g_Chain.RunCalcChain();
    g_SoundPlayer.UpdateBGMSpeed();
    g_SoundPlayer.PlaySounds();
    if (res == 0)
        return RENDER_RESULT_EXIT_SUCCESS;
    if (res == -1)
        return RENDER_RESULT_EXIT_ERROR;

    bool shouldRenderAndPresent = true;
    static int replayFastForwardCounter = 0;
    if (g_ReplaySpeed > 1 && g_GameManager.isInReplay && !g_GameManager.demoMode)
    {
        replayFastForwardCounter++;
        if (replayFastForwardCounter < g_ReplaySpeed)
        {
            shouldRenderAndPresent = false;
        }
        else
        {
            replayFastForwardCounter = 0;
        }
    }
    else
    {
        replayFastForwardCounter = 0;
    }

    if (g_PopupForceDraw)
    {
        g_PopupForceDraw = false;
        if (!shouldRenderAndPresent)
        {
            shouldRenderAndPresent = true;
            replayFastForwardCounter = 0;
        }
    }

    if (shouldRenderAndPresent && g_Supervisor.cfg.frameskipConfig <= this->curFrame)
    {

        {
            bool nowInGame = (g_Supervisor.curState == SUPERVISOR_STATE_GAMEMANAGER);
            if (nowInGame != s_lastFrameWasInGame)
                s_verticalModeTransitionBlack = true;
            s_lastFrameWasInGame = nowInGame;
        }
        if (g_VerticalMode && verticalSurface != NULL)
        {
            IDirect3DSurface8 *origColor = NULL, *origDepth = NULL;
            g_Supervisor.d3dDevice->GetRenderTarget(&origColor);
            g_Supervisor.d3dDevice->GetDepthStencilSurface(&origDepth);
            g_Supervisor.d3dDevice->SetRenderTarget(verticalSurface, verticalDepth);

            if (g_SkipNextFrameClear != 0 && g_ReplayFastForwardActive != 0)
            {
                g_Supervisor.d3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0, 0);
                g_SkipNextFrameClear = 0;
            }
            else
            {
                g_Supervisor.d3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x000000, 1.0, 0);
                g_SkipNextFrameClear = 0;
            }

            g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
            g_Supervisor.d3dDevice->BeginScene();
            g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
            g_Chain.RunDrawChain();
            g_Supervisor.d3dDevice->EndScene();
            g_Supervisor.d3dDevice->SetTexture(0, NULL);

            g_Supervisor.d3dDevice->SetRenderTarget(origColor, origDepth);
            if (origColor)
                origColor->Release();
            if (origDepth)
                origDepth->Release();
        }
        else
        {

            if (g_SkipNextFrameClear != 0 && g_ReplayFastForwardActive != 0)
            {
                g_Supervisor.d3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, 0, 1.0, 0);
                g_SkipNextFrameClear = 0;
            }
            else
            {
                D3DRECT fullRect = {0, 0, GAME_WINDOW_WIDTH, GAME_WINDOW_HEIGHT};
                g_Supervisor.d3dDevice->Clear(1, &fullRect, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x000000, 1.0, 0);
                g_SkipNextFrameClear = 0;
            }
            if (g_Supervisor.IsUnknown())
            {
                viewport.X = 0;
                viewport.Y = 0;
                viewport.Width = 640;
                viewport.Height = 480;
                viewport.MinZ = 0.0;
                viewport.MaxZ = 1.0;
                g_Supervisor.d3dDevice->SetViewport(&viewport);
                g_Supervisor.d3dDevice->Clear(0, NULL, 3, g_Stage.skyFog.color, 1.0, 0);
                g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
            }
            g_Supervisor.d3dDevice->BeginScene();
            g_Chain.RunDrawChain();
            g_Supervisor.d3dDevice->EndScene();
            g_Supervisor.d3dDevice->SetTexture(0, NULL);
        }
    }

    {
        LARGE_INTEGER qpcNow;
        i32 targetFps = 60;

        static int midiTickAccum = 0;
        static bool wasFastForwarding = false;
        bool isFastForwarding = (g_ReplaySpeed > 1 && g_GameManager.isInReplay && !g_GameManager.demoMode);
        if (isFastForwarding)
        {
            targetFps = 60 * g_ReplaySpeed;

            if (g_Supervisor.midiOutput != NULL && Supervisor::ActiveMusicMode() == MIDI)
            {
                midiTickAccum += 1000;
                int ticks = midiTickAccum / 60;
                midiTickAccum -= ticks * 60;
                g_Supervisor.midiOutput->AdvanceTimeMultiplied(ticks);
            }
        }
        else
        {

            midiTickAccum = 0;

            if (wasFastForwarding && g_Supervisor.midiOutput != NULL && Supervisor::ActiveMusicMode() == MIDI)
                g_MidiSuppressAfterFastForward = 16;
        }
        wasFastForwarding = isFastForwarding;

        const LONGLONG frameTicks = (qpcFrequency.QuadPart + (targetFps / 2)) / targetFps;
        qpcNextFrame.QuadPart += frameTicks;

        for (;;)
        {
            QueryPerformanceCounter(&qpcNow);
            LONGLONG remainingTicks = qpcNextFrame.QuadPart - qpcNow.QuadPart;

            if (remainingTicks <= 0)
            {
                if (qpcNow.QuadPart - qpcNextFrame.QuadPart > frameTicks * 2)
                {
                    qpcNextFrame = qpcNow;
                }
                break;
            }

            double remainingMs = (double)remainingTicks * 1000.0 / (double)qpcFrequency.QuadPart;
            if (remainingMs > 1.0)
            {
                Sleep(1);
            }
            else
            {
                while (remainingTicks > 0)
                {
#ifdef __GNUC__
                    __asm__ volatile("rep; nop");
#else
                    __asm { rep nop }
#endif
                    QueryPerformanceCounter(&qpcNow);
                    remainingTicks = qpcNextFrame.QuadPart - qpcNow.QuadPart;
                }
                break;
            }
        }
        qpcLastTime = qpcNow;
    }

    if (shouldRenderAndPresent)
    {
        if (g_Supervisor.cfg.windowed == false && !g_Supervisor.ShouldRunAt60Fps())
        {

            if (this->curFrame == 0)
            {
                Present();
            }
        }
        else
        {
            Present();
        }
    }

    if (g_Supervisor.framerateMultiplier == 0.f)
    {
        g_Supervisor.effectiveFramerateMultiplier = 1.0;
    }
    else
    {
        g_Supervisor.effectiveFramerateMultiplier = g_Supervisor.framerateMultiplier;
    }

    this->curFrame++;
    if (this->curFrame > g_Supervisor.cfg.frameskipConfig)
    {
        this->curFrame = 0;
    }

    return RENDER_RESULT_KEEP_RUNNING;
}

void GameWindow::Present()
{
    RECT sourceRect;
    RECT destRect;
    i32 unused;

    if (g_VerticalMode && verticalSurface != NULL)
    {

        if (s_verticalModeTransitionBlack)
        {
            s_verticalModeTransitionBlack = false;
            IDirect3DSurface8 *backbuf = NULL;
            if (SUCCEEDED(g_Supervisor.d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuf)))
            {
                D3DLOCKED_RECT lr;
                if (SUCCEEDED(backbuf->LockRect(&lr, NULL, 0)))
                {
                    memset(lr.pBits, 0, lr.Pitch * GAME_WINDOW_HEIGHT);
                    backbuf->UnlockRect();
                }
                backbuf->Release();
            }
            sourceRect.left = 0;
            sourceRect.top = 0;
            sourceRect.right = GAME_WINDOW_WIDTH;
            sourceRect.bottom = GAME_WINDOW_HEIGHT;
            destRect.left = 0;
            destRect.top = 0;
            destRect.right = GAME_WINDOW_WIDTH;
            destRect.bottom = GAME_WINDOW_HEIGHT;
            ClearLetterboxBorders(g_GameWindow.window, destRect);
            g_ShouldClearLetterbox = 0;
            g_Supervisor.d3dDevice->Present(&sourceRect, &destRect, NULL, NULL);
            return;
        }

        bool inGame = s_lastFrameWasInGame;

        if (inGame)
        {

            RECT clientRect;
            GetClientRect(g_GameWindow.window, &clientRect);
            int clientW = clientRect.right - clientRect.left;
            int clientH = clientRect.bottom - clientRect.top;
            if (clientW <= 0)
                clientW = 1920;
            if (clientH <= 0)
                clientH = 1080;

            const int bbW = GAME_WINDOW_WIDTH;
            const int bbH = GAME_WINDOW_HEIGHT;
            const int playW = (int)GAME_REGION_WIDTH;
            const int playH = (int)GAME_REGION_HEIGHT;

            const int marginFps = 16;
            const int marginInput = 40;
            const int marginPoint = 16;
            const int marginGraze = 16;
            const int marginPower = 16;
            const int marginAbove = 0;
            const int marginBelow = (int)(GAME_WINDOW_HEIGHT - GAME_REGION_BOTTOM);
            const int marginLife = 16;
            const int marginScore = 16;
            const int marginDifficulty = 16;
            const int marginBestScore = 16;
            const int rotW = marginFps + marginInput + marginPoint + marginGraze + marginPower + marginAbove +
                             marginBelow + playH + marginLife + marginScore + marginDifficulty + marginBestScore;
            const int rotH = playW;
            const int dstX = (bbW - rotW) / 2;
            const int dstY = (bbH - rotH) / 2;

            const int patX = 416;
            const int patY = 0;
            const int patSize = 32;

            D3DSURFACE_DESC srcDesc;
            verticalSurface->GetDesc(&srcDesc);
            D3DLOCKED_RECT srcLR;
            if (SUCCEEDED(verticalSurface->LockRect(&srcLR, NULL, D3DLOCK_READONLY)))
            {
                IDirect3DSurface8 *backbuf = NULL;
                if (SUCCEEDED(g_Supervisor.d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuf)))
                {
                    D3DLOCKED_RECT dstLR;
                    if (SUCCEEDED(backbuf->LockRect(&dstLR, NULL, 0)))
                    {
                        const BYTE *srcBase = (const BYTE *)srcLR.pBits;
                        const int srcPitch = srcLR.Pitch;
                        BYTE *dstBase = (BYTE *)dstLR.pBits;
                        const int dstPitch = dstLR.Pitch;

                        if (srcDesc.Format == D3DFMT_X8R8G8B8 || srcDesc.Format == D3DFMT_A8R8G8B8)
                        {

                            for (int dy = 0; dy < bbH; dy++)
                            {
                                DWORD *dstRow = (DWORD *)(dstBase + dy * dstPitch);
                                int sy = patY + (dy % patSize);
                                const DWORD *patRow = (const DWORD *)(srcBase + sy * srcPitch);

                                for (int dx = 0; dx < dstX; dx++)
                                    dstRow[dx] = patRow[patX + (dx % patSize)];

                                for (int dx = dstX + rotW; dx < bbW; dx++)
                                    dstRow[dx] = patRow[patX + (dx % patSize)];
                            }

                            const BYTE *srcYTable[632];
                            {
                                int d = 0;
                                for (int dx = 0; dx < rotW; dx++, d = dx)
                                {
                                    int src_y;
                                    if (d < marginFps)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 0 + (marginFps - 1 - d);
                                    else if ((d -= marginFps) < marginInput)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 128 + (marginInput - 1 - d);
                                    else if ((d -= marginInput) < marginPoint)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 16 + (marginPoint - 1 - d);
                                    else if ((d -= marginPoint) < marginGraze)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 48 + (marginGraze - 1 - d);
                                    else if ((d -= marginGraze) < marginPower)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 64 + (marginPower - 1 - d);

                                    else if ((d -= marginPower) < marginBelow)
                                        src_y = (int)GAME_WINDOW_HEIGHT - 1 - d;
                                    else if ((d -= marginBelow) < playH)
                                        src_y = (int)GAME_REGION_BOTTOM - 1 - d;
                                    else if ((d -= playH) < marginLife)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 80 + (marginLife - 1 - d);
                                    else if ((d -= marginLife) < marginScore)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 96 + (marginScore - 1 - d);
                                    else if ((d -= marginScore) < marginBestScore)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 112 + (marginBestScore - 1 - d);
                                    else
                                    {
                                        d -= marginBestScore;
                                        src_y = (int)GAME_WINDOW_HEIGHT + 32 + (marginDifficulty - 1 - d);
                                    }
                                    srcYTable[dx] = srcBase + src_y * srcPitch;
                                }
                            }
                            for (int dy = 0; dy < rotH; dy++)
                            {
                                DWORD *dstRow = (DWORD *)(dstBase + (dstY + dy) * dstPitch) + dstX;
                                const int src_x = (int)GAME_REGION_LEFT + dy;
                                const int srcOff = src_x * 4;

                                if (dy + 1 < rotH)
                                    _mm_prefetch((const char *)(srcYTable[0] + (src_x + 1) * 4), _MM_HINT_T1);
                                for (int dx = 0; dx < rotW; dx++)
                                    dstRow[dx] = *(const DWORD *)(srcYTable[dx] + srcOff);
                            }

                            for (int dy = 0; dy < dstY; dy++)
                            {
                                DWORD *dstRow = (DWORD *)(dstBase + dy * dstPitch);
                                int sy = patY + (dy % patSize);
                                const DWORD *patRow = (const DWORD *)(srcBase + sy * srcPitch);
                                for (int dx = dstX; dx < dstX + rotW; dx++)
                                    dstRow[dx] = patRow[patX + (dx % patSize)];
                            }
                            for (int dy = dstY + rotH; dy < bbH; dy++)
                            {
                                DWORD *dstRow = (DWORD *)(dstBase + dy * dstPitch);
                                int sy = patY + (dy % patSize);
                                const DWORD *patRow = (const DWORD *)(srcBase + sy * srcPitch);
                                for (int dx = dstX; dx < dstX + rotW; dx++)
                                    dstRow[dx] = patRow[patX + (dx % patSize)];
                            }
                        }
                        else
                        {
                            for (int dy = 0; dy < bbH; dy++)
                            {
                                WORD *dstRow = (WORD *)(dstBase + dy * dstPitch);
                                int sy = patY + (dy % patSize);
                                const WORD *patRow = (const WORD *)(srcBase + sy * srcPitch);
                                for (int dx = 0; dx < dstX; dx++)
                                    dstRow[dx] = patRow[patX + (dx % patSize)];
                                for (int dx = dstX + rotW; dx < bbW; dx++)
                                    dstRow[dx] = patRow[patX + (dx % patSize)];
                            }
                            for (int dy = 0; dy < rotH; dy++)
                            {
                                WORD *dstRow = (WORD *)(dstBase + (dstY + dy) * dstPitch);
                                int src_x = (int)GAME_REGION_LEFT + dy;
                                for (int dx = 0; dx < rotW; dx++)
                                {
                                    int src_y;
                                    int d = dx;
                                    if (d < marginFps)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 0 + (marginFps - 1 - d);
                                    else if ((d -= marginFps) < marginInput)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 128 + (marginInput - 1 - d);
                                    else if ((d -= marginInput) < marginPoint)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 16 + (marginPoint - 1 - d);
                                    else if ((d -= marginPoint) < marginGraze)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 48 + (marginGraze - 1 - d);
                                    else if ((d -= marginGraze) < marginPower)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 64 + (marginPower - 1 - d);

                                    else if ((d -= marginPower) < marginBelow)
                                        src_y = (int)GAME_WINDOW_HEIGHT - 1 - d;
                                    else if ((d -= marginBelow) < playH)
                                        src_y = (int)GAME_REGION_BOTTOM - 1 - d;
                                    else if ((d -= playH) < marginLife)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 80 + (marginLife - 1 - d);
                                    else if ((d -= marginLife) < marginScore)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 96 + (marginScore - 1 - d);
                                    else if ((d -= marginScore) < marginBestScore)
                                        src_y = (int)GAME_WINDOW_HEIGHT + 112 + (marginBestScore - 1 - d);
                                    else
                                    {
                                        d -= marginBestScore;
                                        src_y = (int)GAME_WINDOW_HEIGHT + 32 + (marginDifficulty - 1 - d);
                                    }
                                    dstRow[dstX + dx] = *(const WORD *)(srcBase + src_y * srcPitch + src_x * 2);
                                }
                            }

                            for (int dy = 0; dy < dstY; dy++)
                            {
                                WORD *dstRow = (WORD *)(dstBase + dy * dstPitch);
                                int sy = patY + (dy % patSize);
                                const WORD *patRow = (const WORD *)(srcBase + sy * srcPitch);
                                for (int dx = dstX; dx < dstX + rotW; dx++)
                                    dstRow[dx] = patRow[patX + (dx % patSize)];
                            }
                            for (int dy = dstY + rotH; dy < bbH; dy++)
                            {
                                WORD *dstRow = (WORD *)(dstBase + dy * dstPitch);
                                int sy = patY + (dy % patSize);
                                const WORD *patRow = (const WORD *)(srcBase + sy * srcPitch);
                                for (int dx = dstX; dx < dstX + rotW; dx++)
                                    dstRow[dx] = patRow[patX + (dx % patSize)];
                            }
                        }
                        backbuf->UnlockRect();
                    }
                    backbuf->Release();
                }
                verticalSurface->UnlockRect();
            }

            sourceRect.left = dstX;
            sourceRect.top = dstY;
            sourceRect.right = dstX + rotW;
            sourceRect.bottom = dstY + rotH;

            int screenShort = clientW < clientH ? clientW : clientH;
            int screenLong = clientW < clientH ? clientH : clientW;
            LONG scaledLong = screenLong;
            LONG scaledShort = (LONG)((__int64)screenLong * rotH / rotW);
            if (scaledShort > screenShort)
            {
                scaledShort = screenShort;
                scaledLong = (LONG)((__int64)screenShort * rotW / rotH);
            }
            LONG scaledWidth = clientW < clientH ? scaledShort : scaledLong;
            LONG scaledHeight = clientW < clientH ? scaledLong : scaledShort;
            destRect.left = (clientW - scaledWidth) / 2;
            destRect.top = (clientH - scaledHeight) / 2;
            destRect.right = destRect.left + scaledWidth;
            destRect.bottom = destRect.top + scaledHeight;
        }
        else
        {

            const int srcW = GAME_WINDOW_WIDTH;
            const int srcH = GAME_WINDOW_HEIGHT;
            const int outW = GAME_WINDOW_WIDTH;
            const int outH = GAME_WINDOW_HEIGHT;

            D3DSURFACE_DESC srcDesc;
            verticalSurface->GetDesc(&srcDesc);
            D3DLOCKED_RECT srcLR;
            if (SUCCEEDED(verticalSurface->LockRect(&srcLR, NULL, D3DLOCK_READONLY)))
            {
                IDirect3DSurface8 *backbuf = NULL;
                if (SUCCEEDED(g_Supervisor.d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuf)))
                {
                    D3DLOCKED_RECT dstLR;
                    if (SUCCEEDED(backbuf->LockRect(&dstLR, NULL, 0)))
                    {
                        const BYTE *srcBase = (const BYTE *)srcLR.pBits;
                        const int srcPitch = srcLR.Pitch;
                        BYTE *dstBase = (BYTE *)dstLR.pBits;
                        const int dstPitch = dstLR.Pitch;

                        if (srcDesc.Format == D3DFMT_X8R8G8B8 || srcDesc.Format == D3DFMT_A8R8G8B8)
                        {
                            for (int dy = 0; dy < outH; dy++)
                            {
                                DWORD *dstRow = (DWORD *)(dstBase + dy * dstPitch);
                                int src_col = dy * srcW / outH;
                                for (int dx = 0; dx < outW; dx++)
                                {
                                    int src_row = (srcH - 1) - dx * srcH / outW;
                                    dstRow[dx] = *(const DWORD *)(srcBase + src_row * srcPitch + src_col * 4);
                                }
                            }
                        }
                        else
                        {
                            for (int dy = 0; dy < outH; dy++)
                            {
                                WORD *dstRow = (WORD *)(dstBase + dy * dstPitch);
                                int src_col = dy * srcW / outH;
                                for (int dx = 0; dx < outW; dx++)
                                {
                                    int src_row = (srcH - 1) - dx * srcH / outW;
                                    dstRow[dx] = *(const WORD *)(srcBase + src_row * srcPitch + src_col * 2);
                                }
                            }
                        }
                        backbuf->UnlockRect();
                    }
                    backbuf->Release();
                }
                verticalSurface->UnlockRect();
            }

            sourceRect.left = 0;
            sourceRect.top = 0;
            sourceRect.right = outW;
            sourceRect.bottom = outH;
            destRect = GetAspectRatioDestRect(g_GameWindow.window);
        }

        if (g_VerticalMode == 2)
        {
            IDirect3DSurface8 *flipBuf = NULL;
            if (SUCCEEDED(g_Supervisor.d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &flipBuf)))
            {
                D3DSURFACE_DESC flipDesc;
                flipBuf->GetDesc(&flipDesc);
                D3DLOCKED_RECT flipLR;
                if (SUCCEEDED(flipBuf->LockRect(&flipLR, NULL, 0)))
                {
                    BYTE *flipBase = (BYTE *)flipLR.pBits;
                    const int flipPitch = flipLR.Pitch;
                    const int fbW = GAME_WINDOW_WIDTH;
                    const int fbH = GAME_WINDOW_HEIGHT;
                    if (flipDesc.Format == D3DFMT_X8R8G8B8 || flipDesc.Format == D3DFMT_A8R8G8B8)
                    {
                        for (int y = 0; y < fbH / 2; y++)
                        {
                            DWORD *rowTop = (DWORD *)(flipBase + y * flipPitch);
                            DWORD *rowBot = (DWORD *)(flipBase + (fbH - 1 - y) * flipPitch);
                            for (int x = 0; x < fbW; x++)
                            {
                                DWORD tmp = rowTop[x];
                                rowTop[x] = rowBot[fbW - 1 - x];
                                rowBot[fbW - 1 - x] = tmp;
                            }
                        }
                    }
                    else
                    {
                        for (int y = 0; y < fbH / 2; y++)
                        {
                            WORD *rowTop = (WORD *)(flipBase + y * flipPitch);
                            WORD *rowBot = (WORD *)(flipBase + (fbH - 1 - y) * flipPitch);
                            for (int x = 0; x < fbW; x++)
                            {
                                WORD tmp = rowTop[x];
                                rowTop[x] = rowBot[fbW - 1 - x];
                                rowBot[fbW - 1 - x] = tmp;
                            }
                        }
                    }
                    flipBuf->UnlockRect();
                }
                flipBuf->Release();
            }
        }

        if (inGame && verticalSurface != NULL)
        {
            D3DSURFACE_DESC patDesc;
            verticalSurface->GetDesc(&patDesc);
            D3DLOCKED_RECT patLR;
            if (SUCCEEDED(verticalSurface->LockRect(&patLR, NULL, D3DLOCK_READONLY)))
            {
                const BYTE *patBase = (const BYTE *)patLR.pBits;
                const int patSize = 32;
                const int patX = 416;
                const int patY = 0;
                RGBQUAD tilePixels[32 * 32];
                if (patDesc.Format == D3DFMT_X8R8G8B8 || patDesc.Format == D3DFMT_A8R8G8B8)
                {
                    for (int ty = 0; ty < patSize; ty++)
                    {
                        const DWORD *row = (const DWORD *)(patBase + (patY + ty) * patLR.Pitch);
                        for (int tx = 0; tx < patSize; tx++)
                        {
                            DWORD px = row[patX + tx];
                            tilePixels[ty * patSize + tx].rgbBlue = (px) & 0xFF;
                            tilePixels[ty * patSize + tx].rgbGreen = (px >> 8) & 0xFF;
                            tilePixels[ty * patSize + tx].rgbRed = (px >> 16) & 0xFF;
                            tilePixels[ty * patSize + tx].rgbReserved = 0;
                        }
                    }
                }
                else
                {
                    for (int ty = 0; ty < patSize; ty++)
                    {
                        const WORD *row = (const WORD *)(patBase + (patY + ty) * patLR.Pitch);
                        for (int tx = 0; tx < patSize; tx++)
                        {
                            WORD px = row[patX + tx];
                            tilePixels[ty * patSize + tx].rgbBlue = (BYTE)((px & 0x1F) << 3);
                            tilePixels[ty * patSize + tx].rgbGreen = (BYTE)(((px >> 5) & 0x3F) << 2);
                            tilePixels[ty * patSize + tx].rgbRed = (BYTE)(((px >> 11) & 0x1F) << 3);
                            tilePixels[ty * patSize + tx].rgbReserved = 0;
                        }
                    }
                }
                verticalSurface->UnlockRect();

                HBITMAP hTile = CreateBitmap(patSize, patSize, 1, 32, tilePixels);
                if (hTile)
                {
                    HBRUSH hBrush = CreatePatternBrush(hTile);
                    if (hBrush)
                    {
                        HDC hdc = GetDC(g_GameWindow.window);
                        if (hdc)
                        {
                            RECT cliR;
                            GetClientRect(g_GameWindow.window, &cliR);
                            int cliW = cliR.right - cliR.left;
                            int cliH = cliR.bottom - cliR.top;
                            SetBrushOrgEx(hdc, 0, 0, NULL);
                            if (destRect.left > 0)
                            {
                                RECT r = {0, 0, destRect.left, cliH};
                                FillRect(hdc, &r, hBrush);
                            }
                            if (destRect.right < cliW)
                            {
                                RECT r = {destRect.right, 0, cliW, cliH};
                                FillRect(hdc, &r, hBrush);
                            }
                            if (destRect.top > 0)
                            {
                                RECT r = {destRect.left, 0, destRect.right, destRect.top};
                                FillRect(hdc, &r, hBrush);
                            }
                            if (destRect.bottom < cliH)
                            {
                                RECT r = {destRect.left, destRect.bottom, destRect.right, cliH};
                                FillRect(hdc, &r, hBrush);
                            }
                            ReleaseDC(g_GameWindow.window, hdc);
                        }
                        DeleteObject(hBrush);
                    }
                    DeleteObject(hTile);
                }
            }
        }
        else
        {
            ClearLetterboxBorders(g_GameWindow.window, destRect);
        }
        g_ShouldClearLetterbox = 0;
        if (g_Supervisor.d3dDevice->Present(&sourceRect, &destRect, NULL, NULL) < 0)
        {
            g_AnmManager->ReleaseSurfaces();
            ReleaseVerticalSurface();
            g_Supervisor.d3dDevice->Reset(&g_Supervisor.presentParameters);
            InitD3dDevice();
            g_Supervisor.unk198 = 2;
            g_ShouldClearLetterbox = 1;
        }
    }
    else
    {
        sourceRect.left = 0;
        sourceRect.top = 0;
        sourceRect.right = GAME_WINDOW_WIDTH;
        sourceRect.bottom = GAME_WINDOW_HEIGHT;
        destRect = GetAspectRatioDestRect(g_GameWindow.window);

        ClearLetterboxBorders(g_GameWindow.window, destRect);
        g_ShouldClearLetterbox = 0;

        if (g_Supervisor.d3dDevice->Present(&sourceRect, &destRect, NULL, NULL) < 0)
        {
            g_AnmManager->ReleaseSurfaces();
            g_Supervisor.d3dDevice->Reset(&g_Supervisor.presentParameters);
            InitD3dDevice();
            g_Supervisor.unk198 = 2;
            g_ShouldClearLetterbox = 1;
        }
    }
    g_AnmManager->TakeScreenshotIfRequested();
    Screenshot_SaveIfRequested();
    if (g_Supervisor.unk198 != 0)
    {
        g_Supervisor.unk198--;
    }
    return;
}

i32 GameWindow::InitD3dInterface(void)
{
    g_Supervisor.d3dIface = Direct3DCreate8(D3D_SDK_VERSION);

    if (g_Supervisor.d3dIface == NULL)
    {
        g_GameErrorContext.Fatal(TH_ERR_D3D_ERR_COULD_NOT_CREATE_OBJ);
        return 1;
    }
    return 0;
}

void GameWindow::CreateGameWindow(HINSTANCE hInstance)
{
    WNDCLASS base_class;
    RECT fullscreenRect;
    DWORD windowedStyle;
    i32 width;
    i32 height;

    memset(&base_class, 0, sizeof(base_class));

    base_class.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    base_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    base_class.hInstance = hInstance;
    base_class.lpfnWndProc = WindowProc;
    g_GameWindow.lastActiveAppValue = 0;
    g_GameWindow.isAppActive = 0;
    g_ShouldClearLetterbox = 1;
    g_ResetDeviceOnResume = 0;
    base_class.lpszClassName = "BASE";
    RegisterClass(&base_class);
    if (g_Supervisor.cfg.windowed == 0)
    {
        fullscreenRect = GetPreferredFullscreenRect();
        width = fullscreenRect.right - fullscreenRect.left;
        height = fullscreenRect.bottom - fullscreenRect.top;
        g_GameWindow.window = CreateWindowEx(0, "BASE", TH_WINDOW_TITLE, WS_VISIBLE | WS_POPUP, fullscreenRect.left,
                                             fullscreenRect.top, width, height, 0, 0, hInstance, 0);
    }
    else
    {
        windowedStyle = GetWindowedStyle();
        i32 windowWidth, windowHeight;
        if (g_CustomWindowWidth > 0 && g_CustomWindowHeight > 0)
        {

            windowWidth = g_CustomWindowWidth;
            windowHeight = g_CustomWindowHeight;
        }
        else
        {

            RECT windowRect = {0, 0, GAME_WINDOW_WIDTH, GAME_WINDOW_HEIGHT};
            AdjustWindowRect(&windowRect, windowedStyle, FALSE);
            windowWidth = windowRect.right - windowRect.left;
            windowHeight = windowRect.bottom - windowRect.top;
        }
        i32 xPos, yPos;

        if (g_CustomWindowPosX != -32000 && g_CustomWindowPosY != -32000)
        {
            xPos = g_CustomWindowPosX;
            yPos = g_CustomWindowPosY;
        }
        else
        {
            xPos = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
            yPos = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;
        }

        g_GameWindow.window = CreateWindowEx(0, "BASE", TH_WINDOW_TITLE, windowedStyle, xPos, yPos, windowWidth,
                                             windowHeight, 0, 0, hInstance, 0);
    }
    g_Supervisor.hwndGameWindow = g_GameWindow.window;
    ImmAssociateContext(g_GameWindow.window, NULL);
}

void GameWindow::ApplyWindowMode()
{
    HWND hWnd = g_GameWindow.window;
    if (!g_Supervisor.cfg.windowed)
    {
        RECT fs = GetPreferredFullscreenRect();
        SetWindowLongA(hWnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
        SetWindowPos(hWnd, HWND_TOP, fs.left, fs.top, fs.right - fs.left, fs.bottom - fs.top, SWP_FRAMECHANGED);
    }
    else
    {
        DWORD style = GetWindowedStyle();
        i32 windowWidth, windowHeight;
        if (g_CustomWindowWidth > 0 && g_CustomWindowHeight > 0)
        {
            windowWidth = g_CustomWindowWidth;
            windowHeight = g_CustomWindowHeight;
        }
        else
        {
            RECT wr = {0, 0, GAME_WINDOW_WIDTH, GAME_WINDOW_HEIGHT};
            AdjustWindowRect(&wr, style, FALSE);
            windowWidth = wr.right - wr.left;
            windowHeight = wr.bottom - wr.top;
        }
        SetWindowLongA(hWnd, GWL_STYLE, style);

        i32 xPos;
        i32 yPos;

        if (g_CustomWindowPosX != -32000 && g_CustomWindowPosY != -32000)
        {
            xPos = g_CustomWindowPosX;
            yPos = g_CustomWindowPosY;
        }
        else
        {
            xPos = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
            yPos = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;
        }

        SetWindowPos(hWnd, HWND_TOP, xPos, yPos, windowWidth, windowHeight, SWP_FRAMECHANGED);
    }
    g_ShouldClearLetterbox = 1;
    g_MainMenu.windowed = g_Supervisor.cfg.windowed;
    Supervisor::SuppressInputFrames(8);
}

LRESULT __stdcall GameWindow::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case 0x3c9:
        if (g_Supervisor.midiOutput != NULL)
        {
            g_Supervisor.midiOutput->UnprepareHeader((LPMIDIHDR)lParam);
        }
        break;
    case WM_ACTIVATEAPP:
        wParam = TRUE;
        g_GameWindow.lastActiveAppValue = wParam;
        if (g_GameWindow.lastActiveAppValue != 0)
        {
            g_GameWindow.isAppActive = 1;
            if (g_Supervisor.keyboard != NULL)
            {
                g_Supervisor.keyboard->Acquire();
            }
            if (g_Supervisor.controller != NULL)
            {
                g_Supervisor.controller->Acquire();
            }
        }
        else
        {
            g_GameWindow.isAppActive = 0;
            if (g_Supervisor.keyboard != NULL)
            {
                g_Supervisor.keyboard->Unacquire();
            }
            if (g_Supervisor.controller != NULL)
            {
                g_Supervisor.controller->Unacquire();
            }
        }
        Controller::ResetKeyboard();
        Supervisor::ResetInputState();
        Supervisor::SuppressInputFrames(2);
        break;
    case WM_POWERBROADCAST:
        switch (wParam)
        {
        case PBT_APMRESUMEAUTOMATIC:
        case PBT_APMRESUMESUSPEND:
            g_ResetDeviceOnResume = 1;
            Controller::ResetKeyboard();
            Supervisor::ResetInputState();
            Supervisor::SuppressInputFrames(8);
            break;
        }
        return TRUE;
    case WM_SETCURSOR:
        if (!g_Supervisor.cfg.windowed)
        {
            if (g_GameWindow.isAppActive != 0)
            {
                SetCursor(LoadCursorA(NULL, IDC_ARROW));
                ShowCursor(1);
            }
            else
            {
                ShowCursor(0);
                SetCursor((HCURSOR)0x0);
            }

            return 1;
        }
        else
        {
            if (LOWORD(lParam) == HTCLIENT)
            {
                SetCursor(LoadCursorA(NULL, IDC_ARROW));
                ShowCursor(1);
                return 1;
            }
        }
        break;
    case WM_GETMINMAXINFO:
        if (g_Supervisor.cfg.windowed)
        {
            MINMAXINFO *minMaxInfo;
            LONG frameWidth;
            LONG frameHeight;

            minMaxInfo = (MINMAXINFO *)lParam;
            GetWindowFrameSize(GetWindowedStyle(), &frameWidth, &frameHeight);
            minMaxInfo->ptMinTrackSize.x = GAME_WINDOW_WIDTH / 2 + frameWidth;
            minMaxInfo->ptMinTrackSize.y = GAME_WINDOW_HEIGHT / 2 + frameHeight;
        }
        return 0;
    case WM_SIZING:
        break;
    case WM_EXITSIZEMOVE:
        if (g_Supervisor.cfg.windowed)
        {
            RECT rect;
            GetWindowRect(hWnd, &rect);
            if (rect.left > -30000 && rect.top > -30000)
            {
                g_CustomWindowPosX = rect.left;
                g_CustomWindowPosY = rect.top;
                g_CustomWindowWidth = rect.right - rect.left;
                g_CustomWindowHeight = rect.bottom - rect.top;
                Supervisor::SaveWindowSettings();
            }
        }
        break;
    case WM_SIZE:
        g_ShouldClearLetterbox = 1;
        break;
    case WM_CLOSE:
        MidiForceResetAll();
        g_GameWindow.isAppClosing = 1;
        return 1;
    case WM_DESTROY:
    case WM_ENDSESSION:
        MidiForceResetAll();
        break;
    case WM_MENUCHAR:
        return MAKELRESULT(0, MNC_CLOSE);
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        if (wParam == VK_RETURN)
        {
            if (uMsg == WM_SYSKEYDOWN)
            {
                g_Supervisor.cfg.windowed = !g_Supervisor.cfg.windowed;
                GameWindow::ApplyWindowMode();
            }
            return 0;
        }
        break;
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

i32 GameWindow::InitD3dRendering(void)
{
    u8 using_d3d_hal;
    D3DPRESENT_PARAMETERS present_params;
    D3DDISPLAYMODE display_mode;
    D3DXVECTOR3 eye;
    D3DXVECTOR3 at;
    D3DXVECTOR3 up;
    float half_width;
    float half_height;
    float aspect_ratio;
    float field_of_view_y;
    float camera_distance;

    using_d3d_hal = 1;
    memset(&present_params, 0, sizeof(D3DPRESENT_PARAMETERS));
    g_Supervisor.d3dIface->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &display_mode);
    D3DFORMAT backBufferFormat = display_mode.Format;
    if (backBufferFormat == D3DFMT_R5G6B5 || backBufferFormat == D3DFMT_X1R5G5B5 || backBufferFormat == D3DFMT_A1R5G5B5)
    {
        backBufferFormat = D3DFMT_X8R8G8B8;
    }
    present_params.BackBufferFormat = backBufferFormat;
    present_params.SwapEffect = D3DSWAPEFFECT_COPY;
    present_params.Windowed = 1;

    if (g_VerticalMode)
    {

        present_params.BackBufferWidth = GAME_WINDOW_WIDTH;
        present_params.BackBufferHeight = GAME_WINDOW_HEIGHT;
    }
    else
    {
        present_params.BackBufferWidth = GAME_WINDOW_WIDTH;
        present_params.BackBufferHeight = GAME_WINDOW_HEIGHT;
    }
    present_params.EnableAutoDepthStencil = true;
    present_params.AutoDepthStencilFormat = D3DFMT_D16;
    present_params.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
    g_Supervisor.lockableBackbuffer = 1;

    if (g_Vsync == 0)
    {
        present_params.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        g_Supervisor.vsyncEnabled = 0;
    }
    else
    {
        present_params.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        g_Supervisor.vsyncEnabled = 1;
    }

    memcpy(&g_Supervisor.presentParameters, &present_params, sizeof(D3DPRESENT_PARAMETERS));
    for (;;)
    {
        if (((g_Supervisor.cfg.opts >> GCOS_REFERENCE_RASTERIZER_MODE) & 1) == 0)
        {
            if (g_Supervisor.d3dIface->CreateDevice(0, D3DDEVTYPE_HAL, g_GameWindow.window,
                                                    D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_params,
                                                    &g_Supervisor.d3dDevice) >= 0)
            {
                g_GameErrorContext.Log(TH_USING_TL_HAL_MODE);
                g_Supervisor.hasD3dHardwareVertexProcessing = 1;
                break;
            }
            g_GameErrorContext.Log(TH_ERR_TL_HAL_UNAVAILABLE);

            if (g_Supervisor.d3dIface->CreateDevice(0, D3DDEVTYPE_HAL, g_GameWindow.window,
                                                    D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_params,
                                                    &g_Supervisor.d3dDevice) >= 0)
            {
                g_GameErrorContext.Log(TH_USING_HAL_MODE);
                g_Supervisor.hasD3dHardwareVertexProcessing = 0;
                break;
            }
            g_GameErrorContext.Log(TH_ERR_HAL_UNAVAILABLE);

            if (present_params.Flags == D3DPRESENTFLAG_LOCKABLE_BACKBUFFER)
            {
                g_GameErrorContext.Log(TH_ERR_BACKBUFFER_NONLOCKED);
                present_params.Flags = 0;
                g_Supervisor.lockableBackbuffer = 0;

                if (g_Supervisor.d3dIface->CreateDevice(0, D3DDEVTYPE_HAL, g_GameWindow.window,
                                                        D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_params,
                                                        &g_Supervisor.d3dDevice) >= 0)
                {
                    g_GameErrorContext.Log(TH_USING_TL_HAL_MODE);
                    g_Supervisor.hasD3dHardwareVertexProcessing = 1;
                    break;
                }

                if (g_Supervisor.d3dIface->CreateDevice(0, D3DDEVTYPE_HAL, g_GameWindow.window,
                                                        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_params,
                                                        &g_Supervisor.d3dDevice) >= 0)
                {
                    g_GameErrorContext.Log(TH_USING_HAL_MODE);
                    g_Supervisor.hasD3dHardwareVertexProcessing = 0;
                    break;
                }
            }
        }

        if (g_Supervisor.d3dIface->CreateDevice(0, D3DDEVTYPE_REF, g_GameWindow.window,
                                                D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_params,
                                                &g_Supervisor.d3dDevice) >= 0)
        {
            g_GameErrorContext.Log(TH_USING_REF_MODE);
            g_Supervisor.hasD3dHardwareVertexProcessing = 0;
            using_d3d_hal = 0;
            break;
        }
        if (((g_Supervisor.cfg.opts >> GCOS_FORCE_60FPS) & 1) != 0 && !g_Supervisor.vsyncEnabled)
        {
            g_GameErrorContext.Log(TH_ERR_CANT_CHANGE_REFRESH_RATE_FORCE_VSYNC);
            present_params.FullScreen_RefreshRateInHz = 0;
            g_Supervisor.vsyncEnabled = 1;
            present_params.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
            continue;
        }
        g_GameErrorContext.Fatal(TH_ERR_D3D_INIT_FAILED);
        if (g_Supervisor.d3dIface != NULL)
        {
            g_Supervisor.d3dIface->Release();
            g_Supervisor.d3dIface = NULL;
        }
        return 1;
    }

    half_width = (float)GAME_WINDOW_WIDTH / 2.0;
    half_height = (float)GAME_WINDOW_HEIGHT / 2.0;
    aspect_ratio = (float)GAME_WINDOW_WIDTH / (float)GAME_WINDOW_HEIGHT;
    field_of_view_y = 0.52359879;
    camera_distance = half_height / tanf(field_of_view_y / 2.0f);
    up.x = 0.0;
    up.y = 1.0;
    up.z = 0.0;
    at.x = half_width;
    at.y = -half_height;
    at.z = 0.0;
    eye.x = half_width;
    eye.y = -half_height;
    eye.z = -camera_distance;
    D3DXMatrixLookAtLH(&g_Supervisor.viewMatrix, &eye, &at, &up);
    D3DXMatrixPerspectiveFovLH(&g_Supervisor.projectionMatrix, field_of_view_y, aspect_ratio, 100.0, 10000.0);
    g_Supervisor.d3dDevice->SetTransform(D3DTS_VIEW, &g_Supervisor.viewMatrix);
    g_Supervisor.d3dDevice->SetTransform(D3DTS_PROJECTION, &g_Supervisor.projectionMatrix);
    g_Supervisor.d3dDevice->GetViewport(&g_Supervisor.viewport);
    g_Supervisor.d3dDevice->GetDeviceCaps(&g_Supervisor.d3dCaps);
    if (((((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1) == 0) &&
         ((g_Supervisor.d3dCaps.TextureOpCaps & D3DTEXOPCAPS_ADD) == 0)))
    {
        g_GameErrorContext.Log(TH_ERR_NO_SUPPORT_FOR_D3DTEXOPCAPS_ADD);
        g_Supervisor.cfg.opts = g_Supervisor.cfg.opts | (1 << GCOS_USE_D3D_HW_TEXTURE_BLENDING);
    }
    if (g_Supervisor.ShouldRunAt60Fps() &&
        ((g_Supervisor.d3dCaps.PresentationIntervals & D3DPRESENT_INTERVAL_IMMEDIATE) == 0))
    {
        g_GameErrorContext.Log(TH_ERR_CANT_FORCE_60FPS_NO_ASYNC_FLIP);
        g_Supervisor.cfg.opts = g_Supervisor.cfg.opts & ~(1 << GCOS_FORCE_60FPS);
    }
    if ((((g_Supervisor.cfg.opts >> GCOS_FORCE_16BIT_COLOR_MODE) & 1) == 0) && (using_d3d_hal != 0))
    {
        if (g_Supervisor.d3dIface->CheckDeviceFormat(0, D3DDEVTYPE_HAL, present_params.BackBufferFormat, 0,
                                                     D3DRTYPE_TEXTURE, D3DFMT_A8R8G8B8) == 0)
        {
            g_Supervisor.colorMode16Bits = 1;
        }
        else
        {
            g_Supervisor.colorMode16Bits = 0;
            g_Supervisor.cfg.opts = g_Supervisor.cfg.opts | (1 << GCOS_FORCE_16BIT_COLOR_MODE);
            g_GameErrorContext.Log(TH_ERR_D3DFMT_A8R8G8B8_UNSUPPORTED);
        }
    }
    InitD3dDevice();
    ScreenEffect::SetViewport(0);
    g_GameWindow.isAppClosing = 0;
    g_Supervisor.lastFrameTime = 0;
    g_Supervisor.framerateMultiplier = 0.0;
    return 0;
}

void GameWindow::InitD3dDevice(void)
{
    f32 fogVal;
    f32 fogDensity;
    AnmManager *anm1;
    AnmManager *anm2;
    AnmManager *anm3;
    AnmManager *anm4;

    if (((g_Supervisor.cfg.opts >> GCOS_TURN_OFF_DEPTH_TEST) & 1) == 0)
    {
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZENABLE, TRUE);
    }
    else
    {
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    }
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    if (((g_Supervisor.cfg.opts >> GCOS_SUPPRESS_USE_OF_GOROUD_SHADING) & 1) == 0)
    {
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
    }
    else
    {
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_FLAT);
    }
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    if (((g_Supervisor.cfg.opts >> GCOS_TURN_OFF_DEPTH_TEST) & 1) == 0)
    {
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    }
    else
    {
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    }
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHAREF, 4);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_FOG) & 1) == 0)
    {
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);
    }
    else
    {
        g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
    }
    fogDensity = 1.0;
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGDENSITY, *(u32 *)&fogDensity);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGCOLOR, 0xffa0a0a0);
    fogVal = 1000.0;
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGSTART, *(u32 *)&fogVal);
    fogVal = 5000.0;
    g_Supervisor.d3dDevice->SetRenderState(D3DRS_FOGEND, *(u32 *)&fogVal);
    if (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0)
    {
        g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    }
    else
    {
        g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    }
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    }
    else
    {
        g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    }
    if (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0)
    {
        g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    }
    else
    {
        g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    }
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    }
    else
    {
        g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    }
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_NONE);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_COUNT2);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSW, D3DTADDRESS_CLAMP);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
    g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
    if (g_AnmManager != NULL)
    {
        anm1 = g_AnmManager;
        anm1->currentBlendMode = 0xff;
        anm2 = g_AnmManager;
        anm2->currentColorOp = 0xff;
        anm3 = g_AnmManager;
        anm3->currentVertexShader = 0xff;
        anm4 = g_AnmManager;
        anm4->currentTexture = NULL;
    }
    g_Stage.skyFogNeedsSetup = 1;

    if (g_VerticalMode)
        CreateVerticalSurface();
    return;
}
}; // namespace th06
