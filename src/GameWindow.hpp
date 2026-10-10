#pragma once

#include "diffbuild.hpp"
#include "inttypes.hpp"
#include <d3d8.h>
#include <windows.h>

#define GAME_WINDOW_WIDTH 640
#define GAME_WINDOW_HEIGHT 480

namespace th06
{
enum RenderResult
{
    RENDER_RESULT_KEEP_RUNNING,
    RENDER_RESULT_EXIT_SUCCESS,
    RENDER_RESULT_EXIT_ERROR,
};

struct GameWindow
{
    RenderResult Render();
    static void Present();

    static i32 InitD3dInterface();
    static void CreateGameWindow(HINSTANCE hInstance);
    static i32 InitD3dRendering();
    static void InitD3dDevice();
    static void ApplyWindowMode();
    static RECT GetAspectRatioDestRect(HWND window);
    static LRESULT __stdcall WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    static void CreateVerticalSurface();
    static void ReleaseVerticalSurface();
    static IDirect3DSurface8 *verticalSurface;
    static IDirect3DSurface8 *verticalDepth;

    HWND window;
    i32 isAppClosing;
    i32 lastActiveAppValue;
    i32 isAppActive;
    u8 curFrame;
    i32 screenSaveActive;
    i32 lowPowerActive;
    i32 powerOffActive;
};

extern GameWindow g_GameWindow;
extern i32 g_TickCountToEffectiveFramerate;
extern double g_LastFrameTime;
}; // namespace th06
