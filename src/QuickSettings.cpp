#include "QuickSettings.hpp"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "AnmManager.hpp"
#include "AsciiManager.hpp"
#include "Controller.hpp"
#include "GameManager.hpp"
#include "Network.hpp"
#include "ScreenEffect.hpp"
#include "SoundFontSynth.hpp"
#include "SoundPlayer.hpp"
#include "Supervisor.hpp"
#include "ZunColor.hpp"
#include "inttypes.hpp"

namespace th06
{
namespace QuickSettings
{
enum ItemKind
{
    KIND_TOGGLE,
    KIND_PERCENT,
    KIND_ROTATION,
    KIND_SUBMENU,
    KIND_SUBMENU_BGM,
    KIND_SUBMENU_MIDI,
    KIND_STEP, // 0..100, step 1, 0 = unlimited ("無制限")
};

struct Item
{
    const wchar_t *label;
    const char *section;
    const char *key;
    i32 *live;
    ItemKind kind;
    bool needsRestart;
    i32 pending;
    bool noRecord;
    char labelSjis[40];
};

static void ToSjis(const wchar_t *src, char *dst, i32 dstSize)
{
    dst[0] = '\0';

    // CP932に存在しないUnicode文字を近似値に置換してから変換する。
    // U+301C WAVE DASH (〜) は CP932 に無く ? になるため、
    // CP932 0x8160 に対応する U+FF5E FULLWIDTH TILDE (～) に置き換える。
    // 変換前にスタック上の固定バッファでコピーして置換する。
    // 呼び出し元は全て短いリテラルなので 256 wchar_t で十分。
    wchar_t tmp[256];
    int i = 0;
    for (; i < 255 && src[i]; i++)
        tmp[i] = (src[i] == L'\u301C') ? L'\uFF5E' : src[i];
    tmp[i] = L'\0';

    WideCharToMultiByte(932, 0, tmp, -1, dst, dstSize, NULL, NULL);
    dst[dstSize - 1] = '\0';
}

static i32 s_submenuDummy = 0;

static Item *GetDisplayItems(i32 *count)
{
    static Item items[] = {
        {L"入力表示", "display", "show_input", &g_ShowInputDisplay, KIND_TOGGLE, false, 0, false},
        {L"入力配置切替", "display", "move_shift", &g_MoveShift, KIND_TOGGLE, false, 0, false},
        {L"自機当たり判定表示", "display", "show_collision", &g_ShowCollision, KIND_TOGGLE, false, 0, true},
        {L"弾当たり判定表示", "display", "hitdisp", &g_HitDisp, KIND_TOGGLE, false, 0, true},
        {L"ボスHPの円形表示", "display", "boss_hp_circle", &g_BossHpCircle, KIND_TOGGLE, false, 0, false},
        {L"エネミーマーカー", "display", "enemy_marker", &g_ShowEnemyMarker, KIND_TOGGLE, false, 0, true},
        {L"難易度表示", "display", "show_difficulty", &g_ShowDifficultyLabel, KIND_TOGGLE, false, 0, false},
        {L"経過フレーム数表示", "display", "show_frame_counter", &g_ShowFrameCounter, KIND_TOGGLE, false, 0, false},
        {L"垂直同期*", "display", "vsync", &g_Vsync, KIND_TOGGLE, true, 0, false},
        {L"縦画面モード*", "display", "vertical_mode", &g_VerticalMode, KIND_ROTATION, true, 0, false},
    };
    static bool initialized = false;
    const i32 n = (i32)(sizeof(items) / sizeof(items[0]));
    if (!initialized)
    {
        for (i32 i = 0; i < n; i++)
        {
            items[i].pending = *items[i].live;
            ToSjis(items[i].label, items[i].labelSjis, (i32)sizeof(items[i].labelSjis));
        }
        initialized = true;
    }
    *count = n;
    return items;
}

static Item *GetAudioItems(i32 *count)
{
    static Item items[] = {
        {L"BGM音量(WAV)", "audio", "bgm_volume", &g_CustomBgmVolume, KIND_PERCENT, false, 0, false},
        {L"BGM音量(MIDI)", "audio", "bgm_volume_midi", &g_CustomBgmVolumeMidi, KIND_PERCENT, false, 0, false},
        {L"SE音量", "audio", "se_volume", &g_CustomSeVolume, KIND_PERCENT, false, 0, false},
        {L"BGM設定", NULL, NULL, &s_submenuDummy, KIND_SUBMENU_BGM, false, 0, false},
        {L"MIDI設定", NULL, NULL, &s_submenuDummy, KIND_SUBMENU_MIDI, false, 0, false},
    };
    static bool initialized = false;
    const i32 n = (i32)(sizeof(items) / sizeof(items[0]));
    if (!initialized)
    {
        for (i32 i = 0; i < n; i++)
        {
            items[i].pending = *items[i].live;
            ToSjis(items[i].label, items[i].labelSjis, (i32)sizeof(items[i].labelSjis));
        }
        initialized = true;
    }
    *count = n;
    return items;
}

static Item *GetSystemItems(i32 *count)
{
    static Item items[] = {
        {L"バックアップ", "system", "backup", &g_Backup, KIND_TOGGLE, false, 0, false},
        {L"バックアップ保持数", "system", "backup_keep_count", &g_BackupKeepCount, KIND_STEP, false, 0, false},
    };
    static bool initialized = false;
    const i32 n = (i32)(sizeof(items) / sizeof(items[0]));
    if (!initialized)
    {
        for (i32 i = 0; i < n; i++)
        {
            items[i].pending = *items[i].live;
            ToSjis(items[i].label, items[i].labelSjis, (i32)sizeof(items[i].labelSjis));
        }
        initialized = true;
    }
    *count = n;
    return items;
}

static bool s_open = false;

bool IsOpen()
{
    return s_open;
}
static f32 s_slide = 0.0f;

bool IsVisible()
{
    return s_open || s_slide > 0.0f;
}
static i32 s_cursor = 0;
static u16 s_prevInput = 0;
static bool s_prevF1 = false;

static bool s_pendingOpen = false;
static bool s_pendingReady = false;
static i32 s_pendingFrames = 0;

static bool IsGameplay()
{
    return g_Supervisor.curState == SUPERVISOR_STATE_GAMEMANAGER;
}

static bool s_runActive = false;

static bool IsLocked(const Item &it)
{
    return it.noRecord && s_runActive;
}

static bool IsGameplayOverlayAllowed()
{

    return g_GameManager.isInGameMenu != 0 || g_GameManager.isInRetryMenu != 0 ||
           (g_GameManager.demoMode != 0 && g_GameManager.isInReplay != 0);
}

static bool IsAllowedState(const Supervisor *s)
{
    if (s->wantedState != s->curState)
    {
        return false;
    }
    switch (s->curState)
    {
    case SUPERVISOR_STATE_GAMEMANAGER:
        return IsGameplayOverlayAllowed();
    case SUPERVISOR_STATE_MAINMENU:
    case SUPERVISOR_STATE_RESULTSCREEN:
    case SUPERVISOR_STATE_RESULTSCREEN_FROMGAME:
    case SUPERVISOR_STATE_MUSICROOM:
    case SUPERVISOR_STATE_ENDING:
        return true;
    default:
        return false;
    }
}

enum AtlasConsts
{
    QS_TEX_SLOT = 250,

    QS_SPRITE_BASE = 2000,
    QS_TEX_W = 256,
    QS_TEX_H = 1024,
    QS_ROW_H = 16,
    QS_MAX_ITEMS = 22,
    QS_ROW_TITLE = 0,
    QS_ROW_MARKER = 1,
    QS_ROW_HINT1 = 2,
    QS_ROW_HINT2 = 3,
    QS_ROW_LABEL = 4,
    QS_ROW_VALUE = QS_ROW_LABEL + QS_MAX_ITEMS,
    QS_ROW_REDSTAR = QS_ROW_VALUE + QS_MAX_ITEMS,
    QS_ROW_HINT3,
    QS_ROWS,
};

struct TextRow
{
    char str[64];
    ZunColor text;
    ZunColor shadow;
    bool valid;
};

static IDirect3DTexture8 *s_atlasTex = NULL;
static PDIRECT3DDEVICE8 s_atlasDevice = NULL;
static TextRow s_rows[QS_ROWS];

static bool EnsureAtlas()
{
    if (s_atlasTex != NULL && s_atlasDevice == g_Supervisor.d3dDevice)
    {
        g_AnmManager->textures[QS_TEX_SLOT] = s_atlasTex;
        return true;
    }

    s_atlasTex = NULL;
    g_AnmManager->textures[QS_TEX_SLOT] = NULL;

    D3DFORMAT fmt = D3DFMT_A4R4G4B4;
    IDirect3DTexture8 *textTex = g_AnmManager->textures[ANM_FILE_TEXT];
    if (textTex != NULL)
    {
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(textTex->GetLevelDesc(0, &desc)))
        {
            fmt = desc.Format;
        }
    }

    IDirect3DTexture8 *tex = NULL;
    if (FAILED(D3DXCreateTexture(g_Supervisor.d3dDevice, QS_TEX_W, QS_TEX_H, 1, 0, fmt, D3DPOOL_MANAGED, &tex)) ||
        tex == NULL)
    {
        return false;
    }
    s_atlasTex = tex;
    s_atlasDevice = g_Supervisor.d3dDevice;
    g_AnmManager->textures[QS_TEX_SLOT] = tex;

    for (i32 row = 0; row < QS_ROWS; row++)
    {
        s_rows[row].valid = false;

        AnmLoadedSprite sprite;
        memset(&sprite, 0, sizeof(sprite));
        sprite.sourceFileIndex = QS_TEX_SLOT;
        sprite.startPixelInclusive.x = 0.0f;
        sprite.startPixelInclusive.y = (f32)(row * QS_ROW_H);
        sprite.endPixelInclusive.x = (f32)QS_TEX_W;
        sprite.endPixelInclusive.y = (f32)(row * QS_ROW_H + QS_ROW_H);
        sprite.textureWidth = (f32)QS_TEX_W;
        sprite.textureHeight = (f32)QS_TEX_H;
        g_AnmManager->LoadSprite(QS_SPRITE_BASE + row, &sprite);
    }
    return true;
}

static bool IsF1Down()
{
    if (g_Supervisor.keyboard != NULL)
    {
        u8 state[256];
        memset(state, 0, sizeof(state));
        if (SUCCEEDED(g_Supervisor.keyboard->GetDeviceState(sizeof(state), state)))
        {
            return (state[DIK_F1] & 0x80) != 0;
        }
        return false;
    }

    return GetForegroundWindow() == g_Supervisor.hwndGameWindow && (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
}

static bool RepeatFire(i32 slot, bool down)
{
    static i32 held[4] = {0, 0, 0, 0};
    if (!down)
    {
        held[slot] = 0;
        return false;
    }
    held[slot]++;
    if (held[slot] == 1)
    {
        return true;
    }
    return held[slot] > 20 && ((held[slot] - 20) % 3) == 0;
}

static void WriteIni(const Item &it, i32 value)
{
    char ini[MAX_PATH];
    char buf[16];
    GetFullPathNameA("./custom_new.ini", MAX_PATH, ini, NULL);
    snprintf(buf, sizeof(buf), "%d", value);
    WritePrivateProfileStringA(it.section, it.key, buf, ini);
}

static void RestartLiveMidi()
{
    if (g_MidiSynthType == 1)
    {
        // FluidSynth が未初期化（WAVモードで起動した場合など）でも Open する。
        // 既に同一 SF2 で開いている場合は SoundFontSynth::Open が即 Reset して返る。
        SoundFontSynth::Open(g_MidiSoundFont[0] ? g_MidiSoundFont : NULL);
    }

    if (g_Supervisor.midiOutput != NULL && g_Supervisor.midiOutput->midiOutDev.handle != 0)
    {
        g_Supervisor.midiOutput->midiOutDev.Close();
        g_Supervisor.midiOutput->midiOutDev.OpenDevice(0xFFFFFFFF);
        g_Supervisor.midiOutput->ResendChannelState();
    }
}

static void Adjust(Item &it, i32 dir)
{
    if (IsLocked(it))
    {
        g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
        return;
    }
    const i32 cur = it.needsRestart ? it.pending : *it.live;
    i32 next = cur;

    switch (it.kind)
    {
    case KIND_TOGGLE:
        next = cur ? 0 : 1;
        break;
    case KIND_PERCENT:
        next = cur + dir;
        if (next < 0)
            next = 0;
        if (next > 100)
            next = 100;
        break;
    case KIND_ROTATION:
        next = (cur + dir + 3) % 3;
        break;
    case KIND_STEP:
        next = cur + dir;
        if (next < 0)
            next = 0;
        if (dir > 0 && next > 100)
            next = 100;
        break;
    }

    if (next == cur)
    {
        return;
    }

    if (it.needsRestart)
    {
        it.pending = next;
    }
    else
    {
        *it.live = next;
    }
    WriteIni(it, next);

    g_Supervisor.unk198 = 3;

    if (it.kind == KIND_PERCENT)
    {
        g_SoundPlayer.ApplyVolumeSettings();
    }
    if (it.live == &g_MidiSynthType)
    {
        RestartLiveMidi();
    }
    g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
}

enum Screen
{
    SCREEN_MAIN,
    SCREEN_DISPLAY,
    SCREEN_AUDIO,
    SCREEN_KEYCONFIG,
    SCREEN_SYSTEM,
    SCREEN_BGM,
    SCREEN_MIDI,
};

enum MainMenuIndex
{
    MAIN_MENU_DISPLAY = 0,
    MAIN_MENU_AUDIO,
    MAIN_MENU_INPUT,
    MAIN_MENU_SYSTEM,
    MAIN_MENU_COUNT
};

static Screen s_screen = SCREEN_MAIN;
static i32 s_mainCursor = 0;
static i32 s_displayCursor = 0;
static i32 s_audioCursor = 0;
static i32 s_systemCursor = 0;

static Item *GetCurrentItems(i32 *count)
{
    switch (s_screen)
    {
    case SCREEN_DISPLAY:
        return GetDisplayItems(count);
    case SCREEN_AUDIO:
        return GetAudioItems(count);
    case SCREEN_SYSTEM:
        return GetSystemItems(count);
    default:
        *count = 0;
        return NULL;
    }
}

struct KeyAction
{
    const wchar_t *label;
    const char *iniKey;
    i32 *slot;
    i32 defaultDik;
    char labelSjis[24];
};

static KeyAction *GetKeyActions(i32 *count)
{
    // ショット ボム 低速移動 上 下 左 右 ポーズ スキップ
    static KeyAction actions[] = {
        {L"ショット", "shoot", &g_KeyConfig.shoot, DIK_Z},      {L"ボム", "bomb", &g_KeyConfig.bomb, DIK_X},
        {L"低速移動", "focus", &g_KeyConfig.focus, DIK_LSHIFT}, {L"上", "up", &g_KeyConfig.up, DIK_UP},
        {L"下", "down", &g_KeyConfig.down, DIK_DOWN},           {L"左", "left", &g_KeyConfig.left, DIK_LEFT},
        {L"右", "right", &g_KeyConfig.right, DIK_RIGHT},        {L"ポーズ", "menu", &g_KeyConfig.menu, DIK_ESCAPE},
        {L"スキップ", "skip", &g_KeyConfig.skip, DIK_LCONTROL},
    };
    static bool initialized = false;
    const i32 n = (i32)(sizeof(actions) / sizeof(actions[0]));
    if (!initialized)
    {
        for (i32 i = 0; i < n; i++)
        {
            ToSjis(actions[i].label, actions[i].labelSjis, (i32)sizeof(actions[i].labelSjis));
        }
        initialized = true;
    }
    *count = n;
    return actions;
}

static i32 s_keyCursor = 0;
static bool s_waitingKey = false;
static bool s_captureArmed = false;
bool IsWaitingKey()
{
    return s_waitingKey;
}

static bool IsKeyConfigCustom()
{
    i32 n = 0;
    KeyAction *a = GetKeyActions(&n);
    for (i32 i = 0; i < n; i++)
    {
        if (*a[i].slot != 0)
        {
            return true;
        }
    }
    return false;
}

static i32 DisplayKey(i32 idx)
{
    i32 n = 0;
    KeyAction *a = GetKeyActions(&n);
    return IsKeyConfigCustom() ? *a[idx].slot : a[idx].defaultDik;
}

static void WriteIniInt(const char *section, const char *key, i32 value)
{
    char ini[MAX_PATH];
    char buf[16];
    GetFullPathNameA("./custom_new.ini", MAX_PATH, ini, NULL);
    snprintf(buf, sizeof(buf), "%d", value);
    WritePrivateProfileStringA(section, key, buf, ini);
}

static void SaveKeyConfig()
{
    i32 n = 0;
    KeyAction *a = GetKeyActions(&n);
    for (i32 i = 0; i < n; i++)
    {
        WriteIniInt("input", a[i].iniKey, *a[i].slot);
    }
}

static void FormatKeyName(i32 dik, char *out, i32 outSize)
{
    if (dik == 0)
    {
        // 未設定
        ToSjis(L"未設定", out, outSize);
        return;
    }

    const LONG lParam = (LONG)(((dik & 0x7f) << 16) | ((dik & 0x80) ? (1 << 24) : 0));
    wchar_t wname[32];
    if (GetKeyNameTextW(lParam, wname, 32) > 0)
    {
        ToSjis(wname, out, outSize);
    }
    else
    {
        snprintf(out, outSize, "0x%02X", dik);
    }
}

static void ResetKeyConfig()
{
    i32 n = 0;
    KeyAction *a = GetKeyActions(&n);
    for (i32 i = 0; i < n; i++)
    {
        *a[i].slot = 0;
    }
    SaveKeyConfig();
}

static void BindKey(i32 idx, i32 dik)
{
    i32 n = 0;
    KeyAction *a = GetKeyActions(&n);
    if (!IsKeyConfigCustom())
    {
        for (i32 i = 0; i < n; i++)
        {
            *a[i].slot = a[i].defaultDik;
        }
    }
    const i32 old = *a[idx].slot;
    for (i32 i = 0; i < n; i++)
    {
        if (i != idx && *a[i].slot == dik)
        {
            *a[i].slot = old;
        }
    }
    *a[idx].slot = dik;
    SaveKeyConfig();
}

static void EnterKeyConfig()
{

    if (g_Supervisor.keyboard == NULL)
    {
        g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
        return;
    }
    s_screen = SCREEN_KEYCONFIG;
    s_keyCursor = 0;
    s_waitingKey = false;
    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
}

static void LeaveKeyConfig()
{
    s_screen = SCREEN_MAIN;
    s_waitingKey = false;
    g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
}

static void BeginKeyCapture()
{
    s_waitingKey = true;
    s_captureArmed = false;
    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
}

static i32 PollKeyCapture()
{
    u8 state[256];
    memset(state, 0, sizeof(state));
    if (g_Supervisor.keyboard == NULL || FAILED(g_Supervisor.keyboard->GetDeviceState(sizeof(state), state)))
    {
        return 0;
    }

    bool anyDown = false;
    for (i32 i = 0; i < 256; i++)
    {
        if (state[i] & 0x80)
        {
            anyDown = true;
            break;
        }
    }
    if (!s_captureArmed)
    {
        if (!anyDown)
        {
            s_captureArmed = true;
        }
        return 0;
    }

    if (state[DIK_ESCAPE] & 0x80)
    {
        return -1;
    }
    for (i32 i = 1; i < 256; i++)
    {
        if (!(state[i] & 0x80))
        {
            continue;
        }

        if (i == DIK_F1 || i == DIK_LWIN || i == DIK_RWIN || i == DIK_APPS)
        {
            continue;
        }
        return i;
    }
    return 0;
}

enum BgmScreenConsts
{
    BGM_ROWS = BGM_TRACK_COUNT + 1,
    BGM_VISIBLE = 15,
};

static i32 s_bgmCursor = 0;
static i32 s_bgmOffset = 0;

// Shift-JIS row label of a track (1 .. BGM_TRACK_COUNT): "NN 曲名".

static const char *GetBgmLabel(i32 track)
{

    // 赤より紅い夢 / ほおずきみたいに紅い魂 / 妖魔夜行 / ルーネイトエルフ / おてんば恋娘
    // 上海紅茶館〜Chinese Tea / 明治十七年の上海アリス / ヴワル魔法図書館
    // ラクトガール〜少女密室 / メイドと血の懐中時計 / 月時計〜ルナ・ダイアル
    // ツェペシュの幼き末裔 / 亡き王女の為のセプテット / 魔法少女達の百年祭
    // U.N.オーエンは彼女なのか？ / 紅より儚い永遠 / 紅楼〜Eastern Dream...
    static const wchar_t *labels[BGM_TRACK_COUNT] = {
        // 赤より紅い夢
        L"01 赤より紅い夢",
        // ほおずきみたいに紅い魂
        L"02 ほおずきみたいに紅い魂",
        // 妖魔夜行
        L"03 妖魔夜行",
        // ルーネイトエルフ
        L"04 ルーネイトエルフ",
        // おてんば恋娘
        L"05 おてんば恋娘",
        // 上海紅茶館　〜 Chinese Tea
        L"06 上海紅茶館 〜 Chinese Tea",
        // 明治十七年の上海アリス
        L"07 明治十七年の上海アリス",
        // ヴワル魔法図書館
        L"08 ヴワル魔法図書館",
        // ラクトガール　〜 少女密室
        L"09 ラクトガール 〜 少女密室",
        // メイドと血の懐中時計
        L"10 メイドと血の懐中時計",
        // 月時計　〜 ルナ・ダイアル
        L"11 月時計 〜 ルナ・ダイアル",
        // ツェペシュの幼き末裔
        L"12 ツェペシュの幼き末裔",
        // 亡き王女の為のセプテット
        L"13 亡き王女の為のセプテット",
        // 魔法少女達の百年祭
        L"14 魔法少女達の百年祭",
        // U.N.オーエンは彼女なのか？
        L"15 U.N.オーエンは彼女なのか？",
        // 紅より儚い永遠
        L"16 紅より儚い永遠",
        // 紅楼　〜 Eastern Dream...
        L"17 紅楼 〜 Eastern Dream...",
    };
    static char sjis[BGM_TRACK_COUNT][40];
    static bool ready = false;
    if (!ready)
    {
        for (i32 i = 0; i < BGM_TRACK_COUNT; i++)
        {
            ToSjis(labels[i], sjis[i], (i32)sizeof(sjis[i]));
        }
        ready = true;
    }
    return sjis[track - 1];
}

// Shift-JIS value text of a track setting: WAV / MIDI, or "共通(WAV)" -- common, with what that currently means.
static void FormatBgmValue(i32 mode, char *out, i32 outSize)
{
    static char commonText[16];
    static char offText[8];
    static bool ready = false;
    if (!ready)
    {
        // 共通 / オフ
        ToSjis(L"共通", commonText, (i32)sizeof(commonText));
        ToSjis(L"オフ", offText, (i32)sizeof(offText));
        ready = true;
    }
    if (mode == WAV)
    {
        snprintf(out, outSize, "WAV");
    }
    else if (mode == MIDI)
    {
        snprintf(out, outSize, "MIDI");
    }
    else
    {
        const i32 global = g_Supervisor.cfg.musicMode;
        snprintf(out, outSize, "%s(%s)", commonText, global == WAV ? "WAV" : (global == MIDI ? "MIDI" : offText));
    }
}

static void ClampBgmScroll()
{
    if (s_bgmCursor < s_bgmOffset)
    {
        s_bgmOffset = s_bgmCursor;
    }
    if (s_bgmCursor >= s_bgmOffset + BGM_VISIBLE)
    {
        s_bgmOffset = s_bgmCursor - BGM_VISIBLE + 1;
    }
    if (s_bgmOffset > BGM_ROWS - BGM_VISIBLE)
    {
        s_bgmOffset = BGM_ROWS - BGM_VISIBLE;
    }
    if (s_bgmOffset < 0)
    {
        s_bgmOffset = 0;
    }
}

static void WriteBgmIni(i32 track)
{
    char key[16];
    snprintf(key, sizeof(key), "track%02d", track);
    WriteIniInt("bgm", key, g_BgmTrackMode[track]);
}

static void AdjustBgm(i32 track, i32 dir)
{
    g_BgmTrackMode[track] = (g_BgmTrackMode[track] + dir + 3) % 3;
    WriteBgmIni(track);
    g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
}

static void ResetBgmModes()
{
    for (i32 track = 1; track <= BGM_TRACK_COUNT; track++)
    {
        g_BgmTrackMode[track] = 0;
        WriteBgmIni(track);
    }
    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
}

static void EnterBgmScreen()
{
    s_screen = SCREEN_BGM;
    s_bgmCursor = 0;
    s_bgmOffset = 0;
    s_waitingKey = false;
    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
}

static void LeaveBgmScreen()
{
    s_screen = SCREEN_AUDIO;
    g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
}

enum MidiScreenConsts
{
    MIDI_ROWS = 3, // 0 = 出力方式, 1 = SF2ファイル選択(GUI), 2 = リセット
};

static i32 s_midiCursor = 0;

static void EnterMidiScreen()
{
    s_screen = SCREEN_MIDI;
    s_midiCursor = 0;
    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
}

static void LeaveMidiScreen()
{
    s_screen = SCREEN_AUDIO;
    g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
}

static void AdjustMidi(i32 dir, bool isShoot = false)
{
    if (s_midiCursor == 0)
    {
        // 0 = オフ, 1 = オン
        g_MidiSynthType = (g_MidiSynthType == 1) ? 0 : 1;
        WriteIniInt("audio", "internal_synth", g_MidiSynthType);
        RestartLiveMidi();
        g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
    }
    else if (s_midiCursor == 1)
    {
        // SF2ファイル選択 (GUI file browse)
        if (g_MidiSynthType == 1)
        {
            if (SoundFontSynth::BrowseSoundFontFile(g_Supervisor.hwndGameWindow))
            {
                RestartLiveMidi();
                g_SoundPlayer.ApplyVolumeSettings();
                g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
            }
            else
            {
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
        }
        else
        {
            g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
        }
    }
    else if (s_midiCursor == 2)
    {

        if (g_MidiSynthType == 1)
        {
            SoundFontSynth::Reset();
        }
        RestartLiveMidi();
        g_SoundPlayer.ApplyVolumeSettings();
        g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
    }
}

static void Open()
{
    s_open = true;
    s_screen = SCREEN_MAIN;
    s_waitingKey = false;
    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);

    ScreenEffect::FinishFadeIns();
}

static void Close()
{
    s_open = false;
    s_screen = SCREEN_MAIN;
    s_waitingKey = false;
    g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);

    Supervisor::SuppressInputFrames(15);
}

bool Update(Supervisor *s)
{
    const u16 cur = g_RealCurFrameInput;
    const u16 pressed = cur & ~s_prevInput;
    s_prevInput = cur;

    const bool f1Down = IsF1Down();
    const bool f1Edge = f1Down && !s_prevF1;
    s_prevF1 = f1Down;

    if (s->curState == SUPERVISOR_STATE_GAMEMANAGER)
    {
        if (g_GameManager.isInReplay == 0 && g_GameManager.demoMode == 0)
        {
            s_runActive = true;
        }
    }
    else if (s->curState == SUPERVISOR_STATE_MAINMENU)
    {
        s_runActive = false;
    }

    const bool netIdle = !g_NetworkManager.IsConnected() && !g_IsOnline;
    if (f1Edge && !s_open && !s_pendingOpen && netIdle && s->curState == SUPERVISOR_STATE_GAMEMANAGER &&
        s->wantedState == s->curState && !IsGameplayOverlayAllowed())
    {
        GameManager::OpenPauseMenu();
        s_pendingOpen = true;
        s_pendingReady = false;
        s_pendingFrames = 0;
    }

    const bool allowed = IsAllowedState(s) && netIdle;

    if (!allowed)
    {
        s_open = false;
        s_slide = 0.0f;
        s_screen = SCREEN_MAIN;
        s_waitingKey = false;
        s_pendingOpen = false;
        return false;
    }

    if (!s_open)
    {
        if (s_pendingOpen)
        {

            const bool pauseMenuStarted =
                g_GameManager.isInGameMenu != 0 && g_AsciiManager.gameMenu.curState != GAME_MENU_PAUSE_OPENING;
            if (pauseMenuStarted)
            {
                if (s_pendingReady)
                {
                    s_pendingOpen = false;
                    Open();
                }
                else
                {
                    s_pendingReady = true;
                }
            }
            else if (++s_pendingFrames > 60)
            {
                s_pendingOpen = false;
            }
        }
        else if (f1Edge)
        {
            Open();
        }
    }
    else
    {
        const bool keyScreen = (s_screen == SCREEN_KEYCONFIG);
        const bool bgmScreen = (s_screen == SCREEN_BGM);
        const bool midiScreen = (s_screen == SCREEN_MIDI);
        i32 numKeyActions = 0;
        GetKeyActions(&numKeyActions);

        if (keyScreen && s_waitingKey)
        {

            if (f1Edge)
            {
                s_waitingKey = false;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
            else
            {
                const i32 key = PollKeyCapture();
                if (key < 0)
                {
                    s_waitingKey = false;
                    g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
                }
                else if (key > 0)
                {
                    BindKey(s_keyCursor, key);
                    s_waitingKey = false;
                    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);

                    Supervisor::SuppressInputFrames(15);
                }
            }
        }
        else if (f1Edge)
        {
            Close();
        }
        else if (pressed & (TH_BUTTON_MENU | TH_BUTTON_BOMB))
        {
            if (s_screen == SCREEN_MAIN)
            {
                Close();
            }
            else if (s_screen == SCREEN_KEYCONFIG)
            {
                LeaveKeyConfig();
            }
            else if (s_screen == SCREEN_BGM)
            {
                LeaveBgmScreen();
            }
            else if (s_screen == SCREEN_MIDI)
            {
                LeaveMidiScreen();
            }
            else
            {
                s_screen = SCREEN_MAIN;
                g_SoundPlayer.PlaySoundByIdx(SOUND_BACK, 0);
            }
        }
        else
        {
            const bool up = RepeatFire(0, (cur & TH_BUTTON_UP) != 0);
            const bool down = RepeatFire(1, (cur & TH_BUTTON_DOWN) != 0);
            const bool left = RepeatFire(2, (cur & TH_BUTTON_LEFT) != 0);
            const bool right = RepeatFire(3, (cur & TH_BUTTON_RIGHT) != 0);

            i32 *cursor = NULL;
            i32 rows = 0;
            switch (s_screen)
            {
            case SCREEN_MAIN:
                cursor = &s_mainCursor;
                rows = MAIN_MENU_COUNT;
                break;
            case SCREEN_DISPLAY:
                cursor = &s_displayCursor;
                GetDisplayItems(&rows);
                break;
            case SCREEN_AUDIO:
                cursor = &s_audioCursor;
                GetAudioItems(&rows);
                break;
            case SCREEN_KEYCONFIG:
                cursor = &s_keyCursor;
                rows = numKeyActions + 1;
                break;
            case SCREEN_SYSTEM:
                cursor = &s_systemCursor;
                GetSystemItems(&rows);
                break;
            case SCREEN_BGM:
                cursor = &s_bgmCursor;
                rows = (i32)BGM_ROWS;
                break;
            case SCREEN_MIDI:
                cursor = &s_midiCursor;
                rows = (i32)MIDI_ROWS;
                break;
            }

            if (up)
            {
                if (cursor && rows > 0)
                {
                    *cursor = (*cursor + rows - 1) % rows;
                    if (s_screen == SCREEN_BGM)
                    {
                        ClampBgmScroll();
                    }
                    g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                }
            }
            else if (down)
            {
                if (cursor && rows > 0)
                {
                    *cursor = (*cursor + 1) % rows;
                    if (s_screen == SCREEN_BGM)
                    {
                        ClampBgmScroll();
                    }
                    g_SoundPlayer.PlaySoundByIdx(SOUND_MOVE_MENU, 0);
                }
            }
            else if (left)
            {
                if (s_screen == SCREEN_BGM)
                {
                    if (s_bgmCursor < BGM_TRACK_COUNT)
                    {
                        AdjustBgm(s_bgmCursor + 1, -1);
                    }
                }
                else if (s_screen == SCREEN_MIDI)
                {
                    AdjustMidi(-1);
                }
                else if (s_screen == SCREEN_DISPLAY || s_screen == SCREEN_AUDIO || s_screen == SCREEN_SYSTEM)
                {
                    i32 itemCount = 0;
                    Item *currentItems = GetCurrentItems(&itemCount);
                    if (currentItems != NULL && cursor && *cursor >= 0 && *cursor < itemCount)
                    {
                        if (currentItems[*cursor].kind != KIND_SUBMENU_BGM &&
                            currentItems[*cursor].kind != KIND_SUBMENU_MIDI)
                        {
                            Adjust(currentItems[*cursor], -1);
                        }
                    }
                }
            }
            else if (right || (pressed & TH_BUTTON_SHOOT))
            {
                if (s_screen == SCREEN_MAIN)
                {
                    switch (s_mainCursor)
                    {
                    case MAIN_MENU_DISPLAY:
                        s_screen = SCREEN_DISPLAY;
                        s_displayCursor = 0;
                        break;
                    case MAIN_MENU_AUDIO:
                        s_screen = SCREEN_AUDIO;
                        s_audioCursor = 0;
                        break;
                    case MAIN_MENU_INPUT:
                        EnterKeyConfig();
                        return true;
                    case MAIN_MENU_SYSTEM:
                        s_screen = SCREEN_SYSTEM;
                        s_systemCursor = 0;
                        break;
                    }
                    g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                }
                else if (s_screen == SCREEN_KEYCONFIG)
                {
                    if (s_keyCursor < numKeyActions)
                    {
                        BeginKeyCapture();
                    }
                    else
                    {
                        ResetKeyConfig();
                        g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
                    }
                }
                else if (s_screen == SCREEN_BGM)
                {
                    if (s_bgmCursor < BGM_TRACK_COUNT)
                    {
                        AdjustBgm(s_bgmCursor + 1, +1);
                    }
                    else
                    {
                        ResetBgmModes();
                    }
                }
                else if (s_screen == SCREEN_MIDI)
                {
                    AdjustMidi(+1, (pressed & TH_BUTTON_SHOOT) != 0);
                }
                else if (s_screen == SCREEN_DISPLAY || s_screen == SCREEN_AUDIO || s_screen == SCREEN_SYSTEM)
                {
                    i32 itemCount = 0;
                    Item *currentItems = GetCurrentItems(&itemCount);
                    if (currentItems != NULL && cursor && *cursor >= 0 && *cursor < itemCount)
                    {
                        if (currentItems[*cursor].kind == KIND_SUBMENU_BGM)
                        {
                            EnterBgmScreen();
                        }
                        else if (currentItems[*cursor].kind == KIND_SUBMENU_MIDI)
                        {
                            EnterMidiScreen();
                        }
                        else
                        {
                            Adjust(currentItems[*cursor], +1);
                        }
                    }
                }
            }
        }
    }

    const f32 target = s_open ? 1.0f : 0.0f;
    const f32 step = 0.14f;
    if (s_slide < target)
    {
        s_slide += step;
        if (s_slide > target)
            s_slide = target;
    }
    else if (s_slide > target)
    {
        s_slide -= step;
        if (s_slide < target)
            s_slide = target;
    }

    return s_open;
}

static void FillRectClipped(f32 left, f32 top, f32 right, f32 bottom, f32 clipTop, D3DCOLOR color)
{
    if (top < clipTop)
        top = clipTop;
    if (bottom <= top)
        return;
    ZunRect rect;
    rect.left = left;
    rect.top = top;
    rect.right = right;
    rect.bottom = bottom;
    ScreenEffect::DrawSquare(&rect, color);
}

static void DrawText(i32 row, f32 x, f32 y, const char *sjis, ZunColor textColor, ZunColor shadowColor)
{
    static AnmVm vm;
    char line[64];

    if (row < 0 || row >= QS_ROWS || !EnsureAtlas())
    {
        return;
    }

    strncpy(line, sjis, sizeof(line) - 1);
    line[sizeof(line) - 1] = '\0';

    TextRow &r = s_rows[row];
    if (!r.valid || r.text != textColor || r.shadow != shadowColor || strcmp(r.str, line) != 0)
    {
        g_AnmManager->DrawTextToSprite(QS_TEX_SLOT, 0, row * QS_ROW_H, QS_TEX_W, QS_TEX_H, 12, 12, textColor,
                                       shadowColor, line);
        strcpy(r.str, line);
        r.text = textColor;
        r.shadow = shadowColor;
        r.valid = true;
    }

    g_AnmManager->InitializeAndSetSprite(&vm, QS_SPRITE_BASE + row);
    vm.flags.anchor = AnmVmAnchor_TopLeft;
    vm.flags.zWriteDisable = 1;
    vm.pos = D3DXVECTOR3(x, y, 0.0f);
    g_AnmManager->DrawNoRotation(&vm);
}

static f32 EstimateTextWidth(const char *sjis)
{
    f32 w = 0.0f;
    for (const unsigned char *p = (const unsigned char *)sjis; *p != '\0'; p++)
    {
        if ((*p >= 0x81 && *p <= 0x9f) || (*p >= 0xe0 && *p <= 0xfc))
        {
            w += 12.0f;
            if (p[1] != '\0')
            {
                p++;
            }
        }
        else
        {
            w += 6.0f;
        }
    }
    return w;
}

void Draw()
{
    if (s_slide <= 0.0f)
    {
        return;
    }

    const bool gameplay = IsGameplay();
    const f32 regionL = gameplay ? 32.0f : 128.0f;
    const f32 regionT = gameplay ? 16.0f : 0.0f;
    const f32 regionR = gameplay ? 416.0f : 512.0f;
    const f32 regionB = gameplay ? 464.0f : 480.0f;

    const f32 inv = 1.0f - s_slide;
    const f32 ease = 1.0f - inv * inv * inv;

    const bool keyScreen = (s_screen == SCREEN_KEYCONFIG);
    const bool bgmScreen = (s_screen == SCREEN_BGM);
    const bool midiScreen = (s_screen == SCREEN_MIDI);
    i32 count = 0;
    i32 numKeyActions = 0;
    KeyAction *keyActions = GetKeyActions(&numKeyActions);

    i32 cursorRow = 0;
    switch (s_screen)
    {
    case SCREEN_MAIN:
        count = MAIN_MENU_COUNT;
        cursorRow = s_mainCursor;
        break;
    case SCREEN_DISPLAY:
        GetDisplayItems(&count);
        cursorRow = s_displayCursor;
        break;
    case SCREEN_AUDIO:
        GetAudioItems(&count);
        cursorRow = s_audioCursor;
        break;
    case SCREEN_KEYCONFIG:
        count = numKeyActions + 1;
        cursorRow = s_keyCursor;
        break;
    case SCREEN_SYSTEM:
        GetSystemItems(&count);
        cursorRow = s_systemCursor;
        break;
    case SCREEN_BGM:
        count = BGM_VISIBLE;
        cursorRow = s_bgmCursor - s_bgmOffset;
        break;
    case SCREEN_MIDI:
        count = MIDI_ROWS;
        cursorRow = s_midiCursor;
        break;
    }
    if (count > QS_MAX_ITEMS)
    {
        count = QS_MAX_ITEMS;
    }

    const f32 rowH = 18.0f;
    const f32 pad = 8.0f;
    const f32 titleH = rowH + 4.0f;
    const bool hasHint3 = (s_screen == SCREEN_DISPLAY);
    const f32 footerH = rowH * (hasHint3 ? 3.0f : 2.0f) + 6.0f;
    const f32 panelH = pad + titleH + rowH * (f32)count + footerH + pad;
    const f32 panelTop = regionT - panelH * (1.0f - ease);
    const f32 panelBottom = panelTop + panelH;

    if (panelBottom <= regionT)
    {
        return;
    }

    const D3DVIEWPORT8 savedViewport = g_Supervisor.viewport;
    g_Supervisor.viewport.X = 0;
    g_Supervisor.viewport.Y = 0;
    g_Supervisor.viewport.Width = 640;
    g_Supervisor.viewport.Height = 480;
    g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);

    const f32 itemsTop = panelTop + pad + titleH;

    const u32 dimAlpha = (u32)(88.0f * ease);
    if (gameplay)
    {
        FillRectClipped(regionL, regionT, regionR, regionB, regionT, D3DCOLOR_ARGB(dimAlpha, 0, 0, 0));
    }
    else
    {
        FillRectClipped(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, D3DCOLOR_ARGB(dimAlpha, 0, 0, 0));
    }

    FillRectClipped(regionL, panelTop, regionR, panelBottom, regionT, D3DCOLOR_ARGB(0xDC, 0x10, 0x16, 0x2A));
    FillRectClipped(regionL, panelBottom - 2.0f, regionR, panelBottom, regionT, D3DCOLOR_ARGB(0xFF, 0x70, 0x98, 0xFF));

    if (s_open)
    {
        const f32 rowTop = itemsTop + rowH * (f32)cursorRow;
        FillRectClipped(regionL + 4.0f, rowTop - 1.0f, regionR - 4.0f, rowTop + rowH - 2.0f, regionT,
                        D3DCOLOR_ARGB(0x90, 0x38, 0x50, 0xA0));
    }

    const f32 markerX = regionL + 6.0f;
    const f32 labelX = regionL + 22.0f;
    const f32 valueX = regionL + 236.0f;
    const f32 visibleFrom = regionT - 1.0f;

    static char titleSjis[40];
    static char displayTitleSjis[40];
    static char audioTitleSjis[40];
    static char keyTitleSjis[40];
    static char systemTitleSjis[40];
    static char bgmTitleSjis[40];
    static char midiTitleSjis[40];

    static char mainDisplayLabel[32];
    static char mainAudioLabel[32];
    static char mainInputLabel[32];
    static char mainSystemLabel[32];

    static char mainHint1Sjis[64];
    static char mainHint2Sjis[64];
    static char hint1Sjis[64];
    static char hint2BackSjis[64];
    static char displayHint2Sjis[64];
    static char keyHint1Sjis[64];
    static char keyHint2Sjis[64];
    static char keyWaitHint2Sjis[64];
    static char keyWaitSjis[40];
    static char keyResetSjis[40];
    static char noRecordHintSjis[64];
    static char bgmResetSjis[40];
    static char midiModeLabelSjis[32];
    static char midiSfLabelSjis[32];
    static char midiDisabledSjis[32];
    static char midiResetLabelSjis[32];
    static char midiNoSfSjis[64];
    static char midiExecSjis[16];
    static char midiHint1Sjis[64];
    static char midiHint2Sjis[64];
    static char s_on[8], s_off[8], s_cw[12], s_ccw[16], s_unlimited[16];
    static bool textReady = false;
    if (!textReady)
    {
        ToSjis(L"クイック設定", titleSjis, (i32)sizeof(titleSjis));
        ToSjis(L"描画設定", displayTitleSjis, (i32)sizeof(displayTitleSjis));
        ToSjis(L"音声設定", audioTitleSjis, (i32)sizeof(audioTitleSjis));
        ToSjis(L"操作設定", keyTitleSjis, (i32)sizeof(keyTitleSjis));
        ToSjis(L"システム設定", systemTitleSjis, (i32)sizeof(systemTitleSjis));
        ToSjis(L"BGM設定", bgmTitleSjis, (i32)sizeof(bgmTitleSjis));
        ToSjis(L"MIDI設定", midiTitleSjis, (i32)sizeof(midiTitleSjis));

        ToSjis(L"描画", mainDisplayLabel, (i32)sizeof(mainDisplayLabel));
        ToSjis(L"音声", mainAudioLabel, (i32)sizeof(mainAudioLabel));
        ToSjis(L"操作", mainInputLabel, (i32)sizeof(mainInputLabel));
        ToSjis(L"システム", mainSystemLabel, (i32)sizeof(mainSystemLabel));

        ToSjis(L"↑↓:選択 →:決定", mainHint1Sjis, (i32)sizeof(mainHint1Sjis));
        ToSjis(L"F1/Esc:閉じる", mainHint2Sjis, (i32)sizeof(mainHint2Sjis));
        ToSjis(L"↑↓:選択 ←→:変更", hint1Sjis, (i32)sizeof(hint1Sjis));
        ToSjis(L"Esc:戻る F1:閉じる", hint2BackSjis, (i32)sizeof(hint2BackSjis));
        ToSjis(L"Esc:戻る F1:閉じる  *:再起動後に反映", displayHint2Sjis, (i32)sizeof(displayHint2Sjis));
        ToSjis(L"↑↓:選択 →:変更", keyHint1Sjis, (i32)sizeof(keyHint1Sjis));
        ToSjis(L"Esc:キャンセル", keyWaitHint2Sjis, (i32)sizeof(keyWaitHint2Sjis));
        ToSjis(L"キーを押してください", keyWaitSjis, (i32)sizeof(keyWaitSjis));
        ToSjis(L"初期設定に戻す", keyResetSjis, (i32)sizeof(keyResetSjis));
        ToSjis(L"有効中はスコアとリプレイを記録しません", noRecordHintSjis, (i32)sizeof(noRecordHintSjis));
        ToSjis(L"全て共通に戻す", bgmResetSjis, (i32)sizeof(bgmResetSjis));
        ToSjis(L"内蔵シンセ", midiModeLabelSjis, (i32)sizeof(midiModeLabelSjis));
        ToSjis(L"SF2ファイル", midiSfLabelSjis, (i32)sizeof(midiSfLabelSjis));
        ToSjis(L"無効(外部シンセ使用)", midiDisabledSjis, (i32)sizeof(midiDisabledSjis));
        ToSjis(L"MIDIリセット", midiResetLabelSjis, (i32)sizeof(midiResetLabelSjis));
        ToSjis(L"(sf2ファイルなし)", midiNoSfSjis, (i32)sizeof(midiNoSfSjis));
        ToSjis(L"実行", midiExecSjis, (i32)sizeof(midiExecSjis));
        ToSjis(L"↑↓:選択 ←→/Z:変更・参照", midiHint1Sjis, (i32)sizeof(midiHint1Sjis));
        ToSjis(L"Esc:戻る F1:閉じる", midiHint2Sjis, (i32)sizeof(midiHint2Sjis));
        ToSjis(L"オン", s_on, (i32)sizeof(s_on));
        ToSjis(L"オフ", s_off, (i32)sizeof(s_off));
        ToSjis(L"時計回り", s_cw, (i32)sizeof(s_cw));
        ToSjis(L"反時計回り", s_ccw, (i32)sizeof(s_ccw));
        ToSjis(L"無制限", s_unlimited, (i32)sizeof(s_unlimited));
        textReady = true;
    }

    const ZunColor shadow = RGB(0x08, 0x08, 0x18);
    const ZunColor titleColor = RGB(0x90, 0xC8, 0xFF);
    const ZunColor normalColor = RGB(0xE0, 0xE0, 0xE0);
    const ZunColor restartColor = RGB(0xB0, 0xB8, 0xC8);
    const ZunColor selectedColor = RGB(0xFF, 0xE0, 0x60);
    const ZunColor hintColor = RGB(0xA0, 0xA8, 0xB8);
    const ZunColor redColor = RGB(0xFF, 0x30, 0x30);
    const ZunColor lockedColor = RGB(0x70, 0x74, 0x80);

    f32 y = panelTop + pad;
    if (y >= visibleFrom)
    {
        char bgmTitleBuf[64];
        const char *titleText = titleSjis;
        if (s_screen == SCREEN_DISPLAY)
            titleText = displayTitleSjis;
        else if (s_screen == SCREEN_AUDIO)
            titleText = audioTitleSjis;
        else if (keyScreen)
            titleText = keyTitleSjis;
        else if (s_screen == SCREEN_SYSTEM)
            titleText = systemTitleSjis;
        else if (bgmScreen)
        {
            snprintf(bgmTitleBuf, sizeof(bgmTitleBuf), "%s  %d/%d", bgmTitleSjis, s_bgmCursor + 1, (i32)BGM_ROWS);
            titleText = bgmTitleBuf;
        }
        else if (midiScreen)
            titleText = midiTitleSjis;

        DrawText(QS_ROW_TITLE, labelX - 16.0f, y, titleText, titleColor, shadow);
    }

    y = itemsTop;
    for (i32 i = 0; i < count; i++, y += rowH)
    {
        if (y < visibleFrom)
        {
            continue;
        }
        if (s_screen == SCREEN_MAIN)
        {
            const bool selected = s_open && (i == s_mainCursor);
            const ZunColor color = selected ? selectedColor : normalColor;
            const char *labelText = "";
            switch (i)
            {
            case MAIN_MENU_DISPLAY:
                labelText = mainDisplayLabel;
                break;
            case MAIN_MENU_AUDIO:
                labelText = mainAudioLabel;
                break;
            case MAIN_MENU_INPUT:
                labelText = mainInputLabel;
                break;
            case MAIN_MENU_SYSTEM:
                labelText = mainSystemLabel;
                break;
            }
            if (selected)
            {
                DrawText(QS_ROW_MARKER, markerX, y, ">", selectedColor, shadow);
            }
            DrawText(QS_ROW_LABEL + i, labelX, y, labelText, color, shadow);
            DrawText(QS_ROW_VALUE + i, valueX, y, ">>", color, shadow);
            continue;
        }
        if (keyScreen)
        {
            const bool selected = s_open && (i == s_keyCursor);
            const ZunColor color = selected ? selectedColor : normalColor;
            const char *labelText = keyResetSjis;
            const char *valueText = NULL;
            char keyBuf[40];
            if (i < numKeyActions)
            {
                labelText = keyActions[i].labelSjis;
                if (selected && s_waitingKey)
                {
                    valueText = keyWaitSjis;
                }
                else
                {
                    FormatKeyName(DisplayKey(i), keyBuf, (i32)sizeof(keyBuf));
                    valueText = keyBuf;
                }
            }
            if (selected)
            {
                DrawText(QS_ROW_MARKER, markerX, y, ">", selectedColor, shadow);
            }
            DrawText(QS_ROW_LABEL + i, labelX, y, labelText, color, shadow);
            if (valueText != NULL)
            {
                DrawText(QS_ROW_VALUE + i, valueX, y, valueText, color, shadow);
            }
            continue;
        }
        if (bgmScreen)
        {
            const i32 entry = s_bgmOffset + i;
            const bool selected = s_open && (entry == s_bgmCursor);
            const ZunColor color = selected ? selectedColor : normalColor;
            const char *labelText = bgmResetSjis;
            const char *valueText = NULL;
            char bgmValueBuf[24];
            if (entry < BGM_TRACK_COUNT)
            {
                labelText = GetBgmLabel(entry + 1);
                FormatBgmValue(g_BgmTrackMode[entry + 1], bgmValueBuf, (i32)sizeof(bgmValueBuf));
                valueText = bgmValueBuf;
            }
            if (selected)
            {
                DrawText(QS_ROW_MARKER, markerX, y, ">", selectedColor, shadow);
            }
            DrawText(QS_ROW_LABEL + i, labelX, y, labelText, color, shadow);
            if (valueText != NULL)
            {
                DrawText(QS_ROW_VALUE + i, valueX, y, valueText, color, shadow);
            }
            continue;
        }
        if (midiScreen)
        {
            const bool selected = s_open && (i == s_midiCursor);
            const ZunColor color = selected ? selectedColor : normalColor;
            const char *labelText = "";
            const char *valueText = "";

            if (i == 0)
            {
                labelText = midiModeLabelSjis;
                valueText = (g_MidiSynthType == 1) ? s_on : s_off;
            }
            else if (i == 1)
            {
                labelText = midiSfLabelSjis;
                if (g_MidiSynthType == 1)
                {
                    const char *curSf = SoundFontSynth::GetCurrentSoundFontName();
                    const char *sf = (curSf && curSf[0] != '\0')    ? curSf
                                     : (g_MidiSoundFont[0] != '\0') ? g_MidiSoundFont
                                                                    : NULL;
                    if (sf)
                    {

                        const char *base = strrchr(sf, '\\');
                        const char *base2 = strrchr(sf, '/');
                        const char *last = (base2 > base) ? base2 : base;
                        valueText = last ? last + 1 : sf;
                    }
                    else
                    {
                        valueText = midiNoSfSjis;
                    }
                }
                else
                {
                    valueText = midiDisabledSjis;
                }
            }
            else if (i == 2)
            {
                labelText = midiResetLabelSjis;
                valueText = midiExecSjis;
            }

            if (selected)
            {
                DrawText(QS_ROW_MARKER, markerX, y, ">", selectedColor, shadow);
            }
            DrawText(QS_ROW_LABEL + i, labelX, y, labelText, color, shadow);
            if (valueText != NULL)
            {
                DrawText(QS_ROW_VALUE + i, valueX, y, valueText, color, shadow);
            }
            continue;
        }

        i32 itemCount = 0;
        Item *currentItems = GetCurrentItems(&itemCount);
        if (!currentItems || i >= itemCount)
        {
            continue;
        }

        const Item &it = currentItems[i];
        const i32 value = it.needsRestart ? it.pending : *it.live;

        char valueBuf[16];
        const char *valueText = valueBuf;
        switch (it.kind)
        {
        case KIND_TOGGLE:
            valueText = value ? s_on : s_off;
            break;
        case KIND_ROTATION:
            valueText = value == 1 ? s_cw : (value == 2 ? s_ccw : s_off);
            break;
        case KIND_STEP:
            if (value <= 0)
            {
                valueText = s_unlimited;
            }
            else
            {
                snprintf(valueBuf, sizeof(valueBuf), "%d", value);
            }
            break;
        case KIND_SUBMENU_BGM:
        case KIND_SUBMENU_MIDI:
            valueText = ">>";
            break;
        default:
            snprintf(valueBuf, sizeof(valueBuf), "%d", value);
            break;
        }

        const i32 curIdx = (s_screen == SCREEN_DISPLAY) ? s_displayCursor
                                                        : ((s_screen == SCREEN_AUDIO) ? s_audioCursor : s_systemCursor);
        const bool selected = s_open && (i == curIdx);
        ZunColor color = normalColor;
        if (selected)
        {
            color = selectedColor;
        }
        else if (it.needsRestart)
        {
            color = restartColor;
        }
        if (IsLocked(it))
        {
            color = lockedColor;
        }

        if (selected)
        {
            DrawText(QS_ROW_MARKER, markerX, y, ">", selectedColor, shadow);
        }
        DrawText(QS_ROW_LABEL + i, labelX, y, it.labelSjis, color, shadow);
        if (it.noRecord)
        {
            DrawText(QS_ROW_REDSTAR, labelX + EstimateTextWidth(it.labelSjis) + 2.0f, y, "*", redColor, shadow);
        }
        DrawText(QS_ROW_VALUE + i, valueX, y, valueText, color, shadow);
    }

    y = itemsTop + rowH * (f32)count + 6.0f;
    if (y >= visibleFrom)
    {
        const char *h1 = hint1Sjis;
        if (s_screen == SCREEN_MAIN)
            h1 = mainHint1Sjis;
        else if (keyScreen)
            h1 = keyHint1Sjis;
        else if (midiScreen)
            h1 = midiHint1Sjis;
        DrawText(QS_ROW_HINT1, labelX - 10.0f, y, h1, hintColor, shadow);
    }
    y += rowH;
    if (y >= visibleFrom)
    {
        const char *h2 = hint2BackSjis;
        if (s_screen == SCREEN_MAIN)
            h2 = mainHint2Sjis;
        else if (s_screen == SCREEN_DISPLAY)
            h2 = displayHint2Sjis;
        else if (keyScreen)
            h2 = (s_waitingKey ? keyWaitHint2Sjis : keyHint2Sjis);
        else if (bgmScreen)
            h2 = keyHint2Sjis;
        else if (midiScreen)
            h2 = midiHint2Sjis;
        DrawText(QS_ROW_HINT2, labelX - 10.0f, y, h2, hintColor, shadow);
    }
    if (hasHint3)
    {
        y += rowH;
        if (y >= visibleFrom)
        {
            DrawText(QS_ROW_REDSTAR, labelX - 10.0f, y, "*", redColor, shadow);
            DrawText(QS_ROW_HINT3, labelX - 10.0f + 8.0f, y, noRecordHintSjis, hintColor, shadow);
        }
    }

    g_Supervisor.viewport = savedViewport;
    g_Supervisor.d3dDevice->SetViewport(&g_Supervisor.viewport);
}
}; // namespace QuickSettings
}; // namespace th06
