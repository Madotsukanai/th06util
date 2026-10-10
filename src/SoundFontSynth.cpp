#include "SoundFontSynth.hpp"
#include "Supervisor.hpp"
#include "utils.hpp"
#include <commdlg.h>
#include <windows.h>
#ifndef SHANDLE_PTR
typedef LONG_PTR SHANDLE_PTR;
#endif
#include <fluidsynth.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

namespace th06
{

#define BUFFER_SAMPLES 512
#define NUM_BUFFERS 4
#define BUFFER_BYTES (BUFFER_SAMPLES * 2 * sizeof(short))

#define MIDI_QUEUE_SIZE 2048

struct QueuedMidiMsg
{
    u8 status;
    u8 b1;
    u8 b2;
};

i32 g_MidiSynthType = 1; // 0 = 外部シンセ (WinMM), 1 = 内蔵シンセ (FluidSynth)
char g_MidiSoundFont[MAX_PATH] = {0};

static fluid_settings_t *s_settings = NULL;
static fluid_synth_t *s_synth = NULL;
static int s_sfontId = -1;

static HWAVEOUT s_hWaveOut = NULL;
static HANDLE s_hThread = NULL;
static HANDLE s_hEvent = NULL;
static HANDLE s_hStopEvent = NULL;
static CRITICAL_SECTION s_cs;
static bool s_csInitialized = false;
static volatile bool s_running = false;
static bool s_isOpen = false;
static char s_currentSoundFont[MAX_SOUNDFONT_NAME] = {0};
static volatile float s_masterVolume = 1.0f;

static WAVEHDR s_headers[NUM_BUFFERS];
static short s_bufferData[NUM_BUFFERS][BUFFER_SAMPLES * 2];

static QueuedMidiMsg s_midiQueue[MIDI_QUEUE_SIZE];
static volatile u32 s_queueHead = 0;
static volatile u32 s_queueTail = 0;

static DWORD WINAPI AudioThreadProc(LPVOID param);

static inline void QueueMidiMsg(u8 status, u8 b1, u8 b2)
{
    u32 curHead = s_queueHead;
    u32 nextHead = (curHead + 1) & (MIDI_QUEUE_SIZE - 1);
    if (nextHead != s_queueTail)
    {
        s_midiQueue[curHead].status = status;
        s_midiQueue[curHead].b1 = b1;
        s_midiQueue[curHead].b2 = b2;
        MemoryBarrier();
        s_queueHead = nextHead;
    }
}

static void DispatchMidiMsg(const QueuedMidiMsg &msg)
{
    if (!s_synth)
        return;

    if (msg.status == 0xFF)
    {
        fluid_synth_system_reset(s_synth);
        return;
    }

    u8 cmd = msg.status & 0xF0;
    u8 ch = msg.status & 0x0F;

    switch (cmd)
    {
    case 0x80:
        fluid_synth_noteoff(s_synth, ch, msg.b1);
        break;
    case 0x90:
        if (msg.b2 > 0)
        {
            fluid_synth_noteon(s_synth, ch, msg.b1, msg.b2);
        }
        else
        {
            fluid_synth_noteoff(s_synth, ch, msg.b1);
        }
        break;
    case 0xA0:
        break;
    case 0xB0:
        fluid_synth_cc(s_synth, ch, msg.b1, msg.b2);
        break;
    case 0xC0:
        fluid_synth_program_change(s_synth, ch, msg.b1);
        break;
    case 0xD0:
        fluid_synth_channel_pressure(s_synth, ch, msg.b1);
        break;
    case 0xE0:
        fluid_synth_pitch_bend(s_synth, ch, ((int)msg.b2 << 7) | (int)msg.b1);
        break;
    default:
        break;
    }
}

static void ProcessPendingMidiEvents()
{
    while (s_queueTail != s_queueHead)
    {
        QueuedMidiMsg msg = s_midiQueue[s_queueTail];
        MemoryBarrier();
        s_queueTail = (s_queueTail + 1) & (MIDI_QUEUE_SIZE - 1);
        DispatchMidiMsg(msg);
    }
}

bool SoundFontSynth::Initialize()
{
    if (!s_csInitialized)
    {
        InitializeCriticalSection(&s_cs);
        s_csInitialized = true;
    }
    return true;
}

void SoundFontSynth::Shutdown()
{
    Close();
    if (s_csInitialized)
    {
        DeleteCriticalSection(&s_cs);
        s_csInitialized = false;
    }
}

#define EMBEDDED_SOUNDFONT_NAME "(内蔵) GeneralUser-GS.sf2"
#define EMBEDDED_SOUNDFONT_PATH ":embedded:"

struct SoundFontStreamHandle
{
    int isMem;
    const unsigned char *memData;
    fluid_long_long_t memSize;
    fluid_long_long_t memPos;
    FILE *fp;
};

static bool HasEmbeddedSoundFont()
{
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(2), RT_RCDATA);
    return (hRes != NULL);
}

static void *SoundFontStreamOpen(const char *filename)
{
    struct SoundFontStreamHandle *h = (struct SoundFontStreamHandle *)calloc(1, sizeof(struct SoundFontStreamHandle));
    if (!h)
        return NULL;

    if (strcmp(filename, EMBEDDED_SOUNDFONT_PATH) == 0)
    {
        HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(2), RT_RCDATA);
        if (!hRes)
        {
            free(h);
            return NULL;
        }
        HGLOBAL hGlobal = LoadResource(NULL, hRes);
        if (!hGlobal)
        {
            free(h);
            return NULL;
        }
        DWORD size = SizeofResource(NULL, hRes);
        const unsigned char *data = (const unsigned char *)LockResource(hGlobal);
        if (!data || size == 0)
        {
            free(h);
            return NULL;
        }
        h->isMem = 1;
        h->memData = data;
        h->memSize = size;
        h->memPos = 0;
        return h;
    }

    h->fp = fopen(filename, "rb");
    if (!h->fp)
    {
        free(h);
        return NULL;
    }
    h->isMem = 0;
    return h;
}

static int SoundFontStreamRead(void *buf, fluid_long_long_t count, void *handle)
{
    struct SoundFontStreamHandle *h = (struct SoundFontStreamHandle *)handle;
    if (!h || count < 0)
        return FLUID_FAILED;
    if (h->isMem)
    {
        if (h->memPos + count > h->memSize)
            return FLUID_FAILED;
        memcpy(buf, h->memData + h->memPos, (size_t)count);
        h->memPos += count;
        return FLUID_OK;
    }
    size_t r = fread(buf, 1, (size_t)count, h->fp);
    return (r == (size_t)count) ? FLUID_OK : FLUID_FAILED;
}

static int SoundFontStreamSeek(void *handle, fluid_long_long_t offset, int origin)
{
    struct SoundFontStreamHandle *h = (struct SoundFontStreamHandle *)handle;
    if (!h)
        return FLUID_FAILED;
    if (h->isMem)
    {
        fluid_long_long_t newPos = 0;
        switch (origin)
        {
        case SEEK_SET:
            newPos = offset;
            break;
        case SEEK_CUR:
            newPos = h->memPos + offset;
            break;
        case SEEK_END:
            newPos = h->memSize + offset;
            break;
        default:
            return FLUID_FAILED;
        }
        if (newPos < 0 || newPos > h->memSize)
            return FLUID_FAILED;
        h->memPos = newPos;
        return FLUID_OK;
    }
    return (fseek(h->fp, (long)offset, origin) == 0) ? FLUID_OK : FLUID_FAILED;
}

static fluid_long_long_t SoundFontStreamTell(void *handle)
{
    struct SoundFontStreamHandle *h = (struct SoundFontStreamHandle *)handle;
    if (!h)
        return FLUID_FAILED;
    if (h->isMem)
        return h->memPos;
    return ftell(h->fp);
}

static int SoundFontStreamClose(void *handle)
{
    struct SoundFontStreamHandle *h = (struct SoundFontStreamHandle *)handle;
    if (!h)
        return FLUID_FAILED;
    if (!h->isMem && h->fp)
        fclose(h->fp);
    free(h);
    return FLUID_OK;
}

SoundFontList SoundFontSynth::ScanSoundFonts()
{
    SoundFontList list;
    memset(&list, 0, sizeof(list));
    list.currentIndex = -1;

    if (g_MidiSoundFont[0] != '\0' && _stricmp(g_MidiSoundFont, EMBEDDED_SOUNDFONT_NAME) != 0 &&
        strcmp(g_MidiSoundFont, EMBEDDED_SOUNDFONT_PATH) != 0)
    {
        DWORD attr = GetFileAttributesA(g_MidiSoundFont);
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
        {
            strncpy(list.names[list.count], g_MidiSoundFont, MAX_SOUNDFONT_NAME - 1);
            list.currentIndex = list.count;
            list.count++;
        }
    }

    if (HasEmbeddedSoundFont() && list.count < MAX_SOUNDFONTS)
    {
        strncpy(list.names[list.count], EMBEDDED_SOUNDFONT_NAME, MAX_SOUNDFONT_NAME - 1);
        if (g_MidiSoundFont[0] == '\0' || _stricmp(g_MidiSoundFont, EMBEDDED_SOUNDFONT_NAME) == 0)
        {
            list.currentIndex = list.count;
        }
        list.count++;
    }

    return list;
}

bool SoundFontSynth::Open(const char *soundfontName)
{
    Initialize();

    char targetFile[MAX_PATH] = {0};
    char chosenDisplayName[MAX_PATH] = {0};
    bool useEmbedded = false;

    auto CheckFileExists = [](const char *path) -> bool {
        DWORD attr = GetFileAttributesA(path);
        return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
    };

    const char *sf = (soundfontName && soundfontName[0] != '\0') ? soundfontName : g_MidiSoundFont;

    if (sf[0] != '\0')
    {
        if (_stricmp(sf, EMBEDDED_SOUNDFONT_NAME) == 0 || strcmp(sf, EMBEDDED_SOUNDFONT_PATH) == 0)
        {
            useEmbedded = true;
            strncpy(chosenDisplayName, EMBEDDED_SOUNDFONT_NAME, sizeof(chosenDisplayName) - 1);
        }
        else
        {

            if (!CheckFileExists(sf))
            {
                useEmbedded = true;
                strncpy(chosenDisplayName, EMBEDDED_SOUNDFONT_NAME, sizeof(chosenDisplayName) - 1);
            }
            else
            {
                strncpy(targetFile, sf, sizeof(targetFile) - 1);
                strncpy(chosenDisplayName, sf, sizeof(chosenDisplayName) - 1);
            }
        }
    }
    else if (HasEmbeddedSoundFont())
    {
        useEmbedded = true;
        strncpy(chosenDisplayName, EMBEDDED_SOUNDFONT_NAME, sizeof(chosenDisplayName) - 1);
    }

    if (useEmbedded)
    {
        if (!HasEmbeddedSoundFont())
        {
            utils::DebugPrint2("SoundFontSynth: No soundfont found and embedded soundfont not available\n");
            return false;
        }
        strncpy(targetFile, EMBEDDED_SOUNDFONT_PATH, sizeof(targetFile) - 1);
    }
    else if (targetFile[0] == '\0')
    {
        utils::DebugPrint2("SoundFontSynth: No soundfont found\n");
        return false;
    }

    if (s_isOpen && s_synth != NULL)
    {
        if (chosenDisplayName[0] != '\0' && _stricmp(s_currentSoundFont, chosenDisplayName) == 0)
        {
            Reset();
            return true;
        }
        Close();
    }

    EnterCriticalSection(&s_cs);
    s_settings = new_fluid_settings();
    fluid_settings_setnum(s_settings, "synth.sample-rate", 44100.0);
    fluid_settings_setint(s_settings, "synth.polyphony", 256);
    fluid_settings_setstr(s_settings, "synth.midi-bank-select", "gs");
    fluid_settings_setint(s_settings, "synth.reverb.active", 1);
    fluid_settings_setint(s_settings, "synth.chorus.active", 1);
    fluid_settings_setnum(s_settings, "synth.gain", s_masterVolume);

    s_synth = new_fluid_synth(s_settings);
    if (!s_synth)
    {
        delete_fluid_settings(s_settings);
        s_settings = NULL;
        LeaveCriticalSection(&s_cs);
        return false;
    }

    fluid_sfloader_t *loader = new_fluid_defsfloader(s_settings);
    if (loader)
    {
        fluid_sfloader_set_callbacks(loader, SoundFontStreamOpen, SoundFontStreamRead, SoundFontStreamSeek,
                                     SoundFontStreamTell, SoundFontStreamClose);
        fluid_synth_add_sfloader(s_synth, loader);
    }

    s_sfontId = fluid_synth_sfload(s_synth, targetFile, 1);
    LeaveCriticalSection(&s_cs);

    if (s_sfontId == FLUID_FAILED)
    {
        utils::DebugPrint2("SoundFontSynth: Failed to load soundfont '%s'\n", targetFile);
        Close();
        return false;
    }

    strncpy(s_currentSoundFont, chosenDisplayName, sizeof(s_currentSoundFont) - 1);

    s_queueTail = s_queueHead = 0;

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = 2;
    wfx.nSamplesPerSec = 44100;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    wfx.cbSize = 0;

    s_hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    s_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    MMRESULT mr = waveOutOpen(&s_hWaveOut, WAVE_MAPPER, &wfx, (DWORD_PTR)s_hEvent, 0, CALLBACK_EVENT);
    if (mr != MMSYSERR_NOERROR)
    {
        utils::DebugPrint2("SoundFontSynth: waveOutOpen failed (%u)\n", mr);
        Close();
        return false;
    }

    memset(s_bufferData, 0, sizeof(s_bufferData));
    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        memset(&s_headers[i], 0, sizeof(WAVEHDR));
        s_headers[i].lpData = (LPSTR)s_bufferData[i];
        s_headers[i].dwBufferLength = BUFFER_BYTES;
        waveOutPrepareHeader(s_hWaveOut, &s_headers[i], sizeof(WAVEHDR));
    }

    s_running = true;
    s_isOpen = true;
    s_hThread = CreateThread(NULL, 0, AudioThreadProc, NULL, 0, NULL);
    SetThreadPriority(s_hThread, THREAD_PRIORITY_TIME_CRITICAL);

    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        waveOutWrite(s_hWaveOut, &s_headers[i], sizeof(WAVEHDR));
    }

    utils::DebugPrint2("SoundFontSynth: FluidSynth loaded '%s' (512-sample low-latency stream started)\n",
                       s_currentSoundFont);
    return true;
}

void SoundFontSynth::Close()
{
    if (!s_isOpen && !s_synth)
    {
        return;
    }

    s_running = false;
    if (s_hStopEvent)
    {
        SetEvent(s_hStopEvent);
    }
    if (s_hEvent)
    {
        SetEvent(s_hEvent);
    }

    if (s_hThread)
    {
        WaitForSingleObject(s_hThread, 500);
        CloseHandle(s_hThread);
        s_hThread = NULL;
    }

    if (s_hWaveOut)
    {
        waveOutReset(s_hWaveOut);
        for (int i = 0; i < NUM_BUFFERS; i++)
        {
            if (s_headers[i].dwFlags & WHDR_PREPARED)
            {
                waveOutUnprepareHeader(s_hWaveOut, &s_headers[i], sizeof(WAVEHDR));
            }
        }
        waveOutClose(s_hWaveOut);
        s_hWaveOut = NULL;
    }

    if (s_hEvent)
    {
        CloseHandle(s_hEvent);
        s_hEvent = NULL;
    }
    if (s_hStopEvent)
    {
        CloseHandle(s_hStopEvent);
        s_hStopEvent = NULL;
    }

    EnterCriticalSection(&s_cs);
    if (s_synth)
    {
        delete_fluid_synth(s_synth);
        s_synth = NULL;
    }
    if (s_settings)
    {
        delete_fluid_settings(s_settings);
        s_settings = NULL;
    }
    s_sfontId = -1;
    LeaveCriticalSection(&s_cs);

    s_isOpen = false;
    s_currentSoundFont[0] = '\0';
}

bool SoundFontSynth::IsOpen()
{
    return s_isOpen;
}

static DWORD WINAPI AudioThreadProc(LPVOID param)
{
    HANDLE waitHandles[2] = {s_hStopEvent, s_hEvent};

    while (s_running)
    {
        DWORD wr = WaitForMultipleObjects(2, waitHandles, FALSE, 50);
        if (wr == WAIT_OBJECT_0)
        {
            break;
        }

        if (!s_running)
            break;

        for (int i = 0; i < NUM_BUFFERS; i++)
        {
            if (s_headers[i].dwFlags & WHDR_DONE)
            {

                ProcessPendingMidiEvents();

                if (s_synth)
                {
                    fluid_synth_write_s16(s_synth, BUFFER_SAMPLES, s_headers[i].lpData, 0, 2, s_headers[i].lpData, 1,
                                          2);
                }
                else
                {
                    memset(s_headers[i].lpData, 0, BUFFER_BYTES);
                }

                waveOutWrite(s_hWaveOut, &s_headers[i], sizeof(WAVEHDR));
            }
        }
    }

    return 0;
}

bool SoundFontSynth::SendShortMsg(u8 status, u8 b1, u8 b2)
{
    if (!s_isOpen)
    {
        return false;
    }

    QueueMidiMsg(status, b1, b2);
    return true;
}

bool SoundFontSynth::SendLongMsg(LPMIDIHDR pmh)
{
    if (!s_isOpen || !s_synth || !pmh || !pmh->lpData)
    {
        return false;
    }

    fluid_synth_sysex(s_synth, (const char *)pmh->lpData, (int)pmh->dwBufferLength, NULL, NULL, NULL, 0);

    return true;
}

void SoundFontSynth::SetMasterVolume(float vol)
{
    if (vol < 0.0f)
        vol = 0.0f;
    if (vol > 1.0f)
        vol = 1.0f;
    s_masterVolume = vol;

    if (s_synth)
    {
        fluid_synth_set_gain(s_synth, vol);
    }
}

void SoundFontSynth::Reset()
{
    if (!s_isOpen || !s_synth)
    {
        return;
    }

    s_queueTail = s_queueHead;

    QueueMidiMsg(0xFF, 0, 0);
}

void SoundFontSynth::Pause()
{
    if (s_hWaveOut)
    {
        waveOutPause(s_hWaveOut);
    }
}

void SoundFontSynth::Resume()
{
    if (s_hWaveOut)
    {
        waveOutRestart(s_hWaveOut);
    }
}

const char *SoundFontSynth::GetCurrentSoundFontName()
{
    return s_currentSoundFont;
}

bool SoundFontSynth::SetCurrentSoundFont(const char *name)
{
    if (!name || name[0] == '\0')
        return false;
    SetSoundFontFile(name);
    if (s_isOpen)
    {
        return Open(name);
    }
    return true;
}

void SoundFontSynth::SetSoundFontFile(const char *path)
{
    if (!path || path[0] == '\0')
        return;

    strncpy(g_MidiSoundFont, path, sizeof(g_MidiSoundFont) - 1);
    g_MidiSoundFont[sizeof(g_MidiSoundFont) - 1] = '\0';

    char ini[MAX_PATH];
    GetFullPathNameA("./custom_new.ini", MAX_PATH, ini, NULL);
    WritePrivateProfileStringA("audio", "sf2", g_MidiSoundFont, ini);
}

bool SoundFontSynth::BrowseSoundFontFile(HWND hwndOwner)
{

    char initDir[MAX_PATH] = {0};
    if (g_MidiSoundFont[0] != '\0' && _stricmp(g_MidiSoundFont, EMBEDDED_SOUNDFONT_NAME) != 0)
    {
        strncpy(initDir, g_MidiSoundFont, sizeof(initDir) - 1);
        char *sep = strrchr(initDir, '\\');
        char *sep2 = strrchr(initDir, '/');
        char *last = (sep2 > sep) ? sep2 : sep;
        if (last)
            *last = '\0';
        else
            initDir[0] = '\0';
    }

    char fileBuf[MAX_PATH] = {0};
    OPENFILENAMEA ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndOwner;
    ofn.lpstrFilter = "SoundFont Files (*.sf2)\0*.sf2\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = sizeof(fileBuf);
    ofn.lpstrInitialDir = initDir[0] ? initDir : NULL;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn))
    {
        SetSoundFontFile(fileBuf);
        return true;
    }
    return false;
}

}; // namespace th06
