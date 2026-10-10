#pragma once

#include "inttypes.hpp"
#include <mmsystem.h>
#include <windows.h>

namespace th06
{

#define MAX_SOUNDFONTS 32
#define MAX_SOUNDFONT_NAME 64

struct SoundFontList
{
    int count;
    int currentIndex;
    char names[MAX_SOUNDFONTS][MAX_SOUNDFONT_NAME];
};

class SoundFontSynth
{
  public:
    static bool Initialize();
    static void Shutdown();

    static bool Open(const char *soundfontName = NULL);
    static void Close();
    static bool IsOpen();

    static bool SendShortMsg(u8 status, u8 b1, u8 b2);
    static bool SendLongMsg(LPMIDIHDR pmh);

    static void SetMasterVolume(float vol);
    static void Reset();
    static void Pause();
    static void Resume();

    static SoundFontList ScanSoundFonts();

    static const char *GetCurrentSoundFontName();
    static bool SetCurrentSoundFont(const char *name);

    static void SetSoundFontFile(const char *path);
    static bool BrowseSoundFontFile(HWND hwndOwner);
};

extern i32 g_MidiSynthType; // 0 = 外部シンセ (WinMM), 1 = 内蔵シンセ (FluidSynth)
extern char g_MidiSoundFont[MAX_PATH];

}; // namespace th06
