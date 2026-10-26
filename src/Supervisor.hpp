#pragma once

#include <d3d8.h>
#include <d3dx8math.h>
#include <dinput.h>

#include "Chain.hpp"
#include "Controller.hpp"
#include "MidiOutput.hpp"
#include "ZunBool.hpp"
#include "ZunResult.hpp"
#include <cstddef>
#include "diffbuild.hpp"
#include "inttypes.hpp"
#include "pbg3/Pbg3Archive.hpp"

namespace th06
{
#define GAME_VERSION 0x102

enum GameConfigOptsShifts
{
    GCOS_USE_D3D_HW_TEXTURE_BLENDING = 0x0,
    GCOS_DONT_USE_VERTEX_BUF = 0x1,
    GCOS_FORCE_16BIT_COLOR_MODE = 0x2,
    GCOS_CLEAR_BACKBUFFER_ON_REFRESH = 0x3,
    GCOS_DISPLAY_MINIMUM_GRAPHICS = 0x4,
    GCOS_SUPPRESS_USE_OF_GOROUD_SHADING = 0x5,
    GCOS_TURN_OFF_DEPTH_TEST = 0x6,
    GCOS_FORCE_60FPS = 0x7,
    GCOS_NO_COLOR_COMP = 0x8,
    GCOS_REFERENCE_RASTERIZER_MODE = 0x9,
    GCOS_DONT_USE_FOG = 0xa,
    GCOS_NO_DIRECTINPUT_PAD = 0xb,
};

struct ControllerMapping
{
    i16 shootButton;
    i16 bombButton;
    i16 focusButton;
    i16 menuButton;
    i16 upButton;
    i16 downButton;
    i16 leftButton;
    i16 rightButton;
    i16 skipButton;
};

enum MusicMode
{
    OFF = 0,
    WAV = 1,
    MIDI = 2
};

#define BGM_TRACK_COUNT 17

struct GameConfiguration
{
    ControllerMapping controllerMapping;

    u16 configLayoutPad; // 0xDDDD filler in the original file layout; version
                         // dword lives at offset 0x14, matching Supervisor::LoadConfig.

    i32 version;
    u8 lifeCount;
    u8 bombCount;
    u8 colorMode16bit;
    u8 musicMode;
    u8 playSounds;
    u8 defaultDifficulty;
    u8 windowed;

    u8 frameskipConfig;
    i16 padXAxis;
    i16 padYAxis;
    u8 unk[16];

    u32 opts;

    u32 IsSoftwareTexturing()
    {
        return ((this->opts >> GCOS_NO_COLOR_COMP) & 1) |
               ((this->opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1);
    }
};

static_assert(sizeof(GameConfiguration) == 0x38, "config layout drift");
static_assert(offsetof(GameConfiguration, version) == 0x14, "config layout drift");
static_assert(offsetof(GameConfiguration, opts) == 0x34, "config layout drift");

#define IN_PBG3_INDEX 0
#define MD_PBG3_INDEX 1
#define ST_PBG3_INDEX 2
#define CM_PBG3_INDEX 4
#define ED_PBG3_INDEX 5

typedef char Pbg3ArchiveName[32];

enum SupervisorState
{
    SUPERVISOR_STATE_INIT,
    SUPERVISOR_STATE_MAINMENU,
    SUPERVISOR_STATE_GAMEMANAGER,
    SUPERVISOR_STATE_GAMEMANAGER_REINIT,
    SUPERVISOR_STATE_EXITSUCCESS,
    SUPERVISOR_STATE_EXITERROR,
    SUPERVISOR_STATE_RESULTSCREEN,
    SUPERVISOR_STATE_RESULTSCREEN_FROMGAME,
    SUPERVISOR_STATE_MAINMENU_REPLAY,
    SUPERVISOR_STATE_MUSICROOM,
    SUPERVISOR_STATE_ENDING,
};

struct Supervisor
{
    static ZunResult RegisterChain();
    static ChainCallbackResult OnUpdate(Supervisor *s);
    static ChainCallbackResult OnDraw(Supervisor *s);
    static ZunResult AddedCallback(Supervisor *s);
    static ZunResult DeletedCallback(Supervisor *s);
    static void DrawFpsCounter();
    static void ResetInputState();
    static void SuppressInputFrames(u16 frames);

    static bool IsQuickSettingsOpen();

    static bool IsQuickSettingsVisible();

    static bool IsQuickSettingsWaitingKey();

    static i32 ResolveMusicMode(const char *path);

    static i32 ActiveMusicMode();

    static void SetMusicModeOverride(i32 mode);
    static i32 GetMusicModeOverride();

    ZunBool ReadMidiFile(u32 midiFileIdx, char *path);
    i32 PlayMidiFile(i32 midiFileIdx);
    ZunResult PlayAudio(char *path);
    ZunResult PauseAudio();
    ZunResult ResumeAudio();
    ZunResult StopAudio();
    ZunResult SetupMidiPlayback(char *path);
    ZunResult FadeOutMusic(f32 fadeOutSeconds);

    static ZunResult SetupDInput(Supervisor *s);
    static BOOL CALLBACK ControllerCallback(LPCDIDEVICEOBJECTINSTANCEA lpddoi, LPVOID pvRef);
    static BOOL CALLBACK EnumGameControllersCb(LPCDIDEVICEINSTANCEA pdidInstance, LPVOID pContext);

    i32 LoadPbg3(i32 pbg3FileIdx, char *filename);
    void ReleasePbg3(i32 pbg3FileIdx);

    ZunResult LoadConfig(char *path);
    static void SaveWindowSettings();

    void TickTimer(i32 *frames, f32 *subframes);

    f32 FramerateMultiplier()
    {
        return this->effectiveFramerateMultiplier;
    }

    u32 IsUnknown()
    {
        return ((this->cfg.opts >> GCOS_CLEAR_BACKBUFFER_ON_REFRESH) & 1) |
               ((this->cfg.opts >> GCOS_DISPLAY_MINIMUM_GRAPHICS) & 1);
    }

    u32 ShouldRunAt60Fps()
    {
        return ((this->cfg.opts >> GCOS_FORCE_60FPS) & 1) && this->vsyncEnabled;
    }

    HINSTANCE hInstance;
    PDIRECT3D8 d3dIface;
    PDIRECT3DDEVICE8 d3dDevice;
    LPDIRECTINPUT8 dinputIface;
    LPDIRECTINPUTDEVICE8A keyboard;
    LPDIRECTINPUTDEVICE8A controller;
    DIDEVCAPS controllerCaps;
    HWND hwndGameWindow;
    D3DXMATRIX viewMatrix;
    D3DXMATRIX projectionMatrix;
    D3DVIEWPORT8 viewport;
    D3DPRESENT_PARAMETERS presentParameters;
    GameConfiguration cfg;
    GameConfiguration defaultConfig;
    i32 calcCount;
    i32 wantedState;
    i32 curState;
    i32 wantedState2;

    i32 unk194;
    i32 unk198;
    ZunBool isInEnding;

    i32 vsyncEnabled;
    i32 lastFrameTime;
    f32 effectiveFramerateMultiplier;
    f32 framerateMultiplier;

    MidiOutput *midiOutput;

    f32 unk1b4;
    f32 unk1b8;

    Pbg3Archive *pbg3Archives[16];
    Pbg3ArchiveName pbg3ArchiveNames[16];

    u8 hasD3dHardwareVertexProcessing;
    u8 lockableBackbuffer;
    u8 colorMode16Bits;

    u32 startupTimeBeforeMenuMusic;
    D3DCAPS8 d3dCaps;
};
ZUN_ASSERT_SIZE(Supervisor, 0x4d8);

extern ControllerMapping g_ControllerMapping;
extern Supervisor g_Supervisor;
extern u16 g_LastFrameInput;
extern u16 g_CurFrameInput;
extern u16 g_SuppressedInputFrames;
extern u16 g_IsEigthFrameOfHeldInput;
extern u8 g_SkipNextFrameClear;
extern IDirect3DSurface8 *g_TextBufferSurface;
extern i32 g_ShowInputDisplay;
extern i32 g_IsSpellPracticeGame;
extern i32 g_SpellPracticeMode;
extern i32 g_SpellPracticeSection;
extern i32 g_SpellPracticeWarpTime;
extern i32 g_ThpracSection;
extern i32 g_ThpracDlg;
extern i32 g_ThpracSectionReplay;
extern i32 g_SpellPracticeBossSubId;
extern i32 g_SpellPracticeResultDelay;
extern i32 g_ReturnToSpellPractice;
extern i32 g_ReplaySpeed;
extern i32 g_SavedReplaySpeed;
extern i32 g_ReplayFastForwardActive;
extern i32 g_ReplayTotalFrames;
extern u16 g_RealCurFrameInput;
extern i32 g_LastSpellPracticeIndex;
extern i32 g_SpellPracticeRank;
extern i32 g_ReturnToPractice;
extern i32 g_SpellPracticeLastSelectedSpellIndex;
extern i32 g_MoveShift;
extern i32 g_CustomWindowWidth;
extern i32 g_CustomWindowHeight;
extern i32 g_CustomWindowPosX;
extern i32 g_CustomWindowPosY;
extern i32 g_CustomBgmVolume;

extern i32 g_CustomBgmVolumeMidi;
extern i32 g_CustomSeVolume;
extern i32 g_ShowCollision;
extern i32 g_HitDisp;
extern i32 g_Backup;
extern i32 g_BackupKeepCount;
extern i32 g_IsOnline;
extern i32 g_Vsync;
extern i32 g_VerticalMode;
extern i32 g_P2BombsRemaining;
extern i32 g_P2GrazeInStage;
extern i32 g_P2LivesRemaining;
extern u32 g_P2Score;
extern u32 g_P2HighScore;
extern u32 g_P2PointItems;
extern u16 g_P2Power;
extern i32 g_BossHpCircle;
extern i32 g_ShowEnemyMarker;
extern i32 g_ShowDifficultyLabel;
extern i32 g_ShowFrameCounter;
extern i32 g_ShowActualDeaths;
extern u16 g_NetRandomSeed;
extern u16 g_NumOfFramesInputsWereHeld;
extern u32 g_InputDelay;

extern i32 g_BgmTrackMode[BGM_TRACK_COUNT + 1];
}; // namespace th06
