#define XDELTA3_MAIN 0
#define XD3_POSIX 0
#define XD3_WIN32 1
#define SECONDARY_DJW 0
#define SECONDARY_FGK 0
#define SECONDARY_LZMA 0

extern "C"
{
#include "xdelta3.h"
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

// Constants
constexpr DWORD EXPECTED_ORIGINAL_SIZE = 512000;
constexpr size_t MAX_DECODE_BUFFER_SIZE = 128 * 1024 * 1024; // 128MB max for patched executable
constexpr const char *DEFAULT_GAME_EXE_NAME = "東方紅魔郷.exe";
constexpr const char *PATCHED_EXE_NAME = "th06_patched.exe";

static void ShowError(const char *message)
{
    MessageBoxA(NULL, message, "th06UtilityTool Launcher", MB_OK | MB_ICONERROR);
}

// Decode xdelta3/VCDIFF patch in memory
// Returns a pointer to VirtualAlloc'd memory containing patched executable (must be freed with VirtualFree).
static BYTE *DecodePatch(const BYTE *srcData, size_t srcSize, const BYTE *patchData, size_t patchSize, DWORD *outSize)
{
    BYTE *outBuffer = (BYTE *)VirtualAlloc(NULL, MAX_DECODE_BUFFER_SIZE, MEM_COMMIT, PAGE_READWRITE);
    if (!outBuffer)
    {
        return NULL;
    }

    xd3_stream stream;
    xd3_config config;
    xd3_source source;
    memset(&stream, 0, sizeof(stream));
    memset(&config, 0, sizeof(config));
    memset(&source, 0, sizeof(source));

    xd3_init_config(&config, XD3_ADLER32_NOVER);
    xd3_config_stream(&stream, &config);

    source.blksize = (usize_t)srcSize;
    source.onblk = (usize_t)srcSize;
    source.curblk = srcData;
    source.curblkno = 0;
    source.max_blkno = 0;
    source.frontier_blkno = 0;
    source.onlastblk = (usize_t)srcSize;
    source.eof_known = 1;
    source.max_winsize = (xoff_t)srcSize;
    xd3_set_source(&stream, &source);

    usize_t decodedSize = 0;
    int ret = xd3_decode_stream(&stream, patchData, (usize_t)patchSize, outBuffer, &decodedSize,
                                (usize_t)MAX_DECODE_BUFFER_SIZE);

    if (ret != 0)
    {
        char msg[256];
        snprintf(msg, sizeof(msg), "Patch decode failed (code=%d): %s", ret, stream.msg ? stream.msg : "(unknown)");
        ShowError(msg);
        xd3_close_stream(&stream);
        xd3_free_stream(&stream);
        VirtualFree(outBuffer, 0, MEM_RELEASE);
        return NULL;
    }

    xd3_close_stream(&stream);
    xd3_free_stream(&stream);
    *outSize = (DWORD)decodedSize;
    return outBuffer;
}

static BYTE *ReadEntireFile(const char *path, DWORD *outSize)
{
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }

    DWORD size = GetFileSize(hFile, NULL);
    if (size == INVALID_FILE_SIZE)
    {
        CloseHandle(hFile);
        return NULL;
    }

    BYTE *data = (BYTE *)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
    if (!data)
    {
        CloseHandle(hFile);
        return NULL;
    }

    DWORD bytesRead = 0;
    if (!ReadFile(hFile, data, size, &bytesRead, NULL) || bytesRead != size)
    {
        VirtualFree(data, 0, MEM_RELEASE);
        CloseHandle(hFile);
        return NULL;
    }

    CloseHandle(hFile);
    *outSize = size;
    return data;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    char moduleDir[MAX_PATH];
    GetModuleFileNameA(NULL, moduleDir, MAX_PATH);
    char *lastSlash = strrchr(moduleDir, '\\');
    if (lastSlash)
    {
        lastSlash[1] = '\0';
    }

    // Clean up any leftover patched executable from a previous run
    char leftoverPatchedExe[MAX_PATH];
    snprintf(leftoverPatchedExe, MAX_PATH, "%s%s", moduleDir, PATCHED_EXE_NAME);
    DeleteFileA(leftoverPatchedExe);

    char originalExePath[MAX_PATH];
    char customPatchPath[MAX_PATH] = "";

    if (__argc >= 3)
    {
        snprintf(originalExePath, MAX_PATH, "%s", __argv[1]);
        snprintf(customPatchPath, MAX_PATH, "%s", __argv[2]);
    }
    else if (__argc == 2)
    {
        snprintf(originalExePath, MAX_PATH, "%s", __argv[1]);
    }
    else
    {
        snprintf(originalExePath, MAX_PATH, "%s%s", moduleDir, DEFAULT_GAME_EXE_NAME);
    }

    if (GetFileAttributesA(originalExePath) == INVALID_FILE_ATTRIBUTES)
    {
        char msg[MAX_PATH + 64];
        snprintf(msg, sizeof(msg), "%s が見つかりません。\n同じフォルダに配置してください。", originalExePath);
        ShowError(msg);
        return 1;
    }

    // Read original game executable
    DWORD origSize = 0;
    BYTE *origData = ReadEntireFile(originalExePath, &origSize);
    if (!origData)
    {
        ShowError("元ゲーム実行ファイルの読み込みに失敗しました。");
        return 1;
    }

    // Verify file size for Touhou Koumakyou ver 1.02h
    if (__argc < 3 && origSize != EXPECTED_ORIGINAL_SIZE)
    {
        VirtualFree(origData, 0, MEM_RELEASE);
        ShowError("実行ファイルのサイズが一致しません。\n東方紅魔郷 ver1.02h を使用してください。");
        return 1;
    }

    // Load patch data (either from custom path or embedded resource 101)
    DWORD patchSize = 0;
    BYTE *patchData = NULL;
    bool shouldFreePatchData = false;

    if (customPatchPath[0] != '\0')
    {
        patchData = ReadEntireFile(customPatchPath, &patchSize);
        if (!patchData)
        {
            VirtualFree(origData, 0, MEM_RELEASE);
            ShowError("指定されたパッチファイルの読み込みに失敗しました。");
            return 1;
        }
        shouldFreePatchData = true;
    }
    else
    {
        HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(101), RT_RCDATA);
        if (hRes)
        {
            HGLOBAL hData = LoadResource(NULL, hRes);
            patchSize = SizeofResource(NULL, hRes);
            patchData = (BYTE *)LockResource(hData);
        }
    }

    if (!patchData || patchSize == 0)
    {
        VirtualFree(origData, 0, MEM_RELEASE);
        ShowError("パッチデータの取得に失敗しました。");
        return 1;
    }

    // Apply delta patch in memory
    DWORD patchedSize = 0;
    BYTE *patchedExe = DecodePatch(origData, origSize, patchData, patchSize, &patchedSize);
    VirtualFree(origData, 0, MEM_RELEASE);
    if (shouldFreePatchData)
    {
        VirtualFree(patchData, 0, MEM_RELEASE);
    }

    if (!patchedExe || patchedSize == 0)
    {
        ShowError("パッチの適用に失敗しました。");
        return 1;
    }

    // Write patched executable to temporary file
    char patchedExePath[MAX_PATH];
    snprintf(patchedExePath, MAX_PATH, "%s%s", moduleDir, PATCHED_EXE_NAME);

    HANDLE hPatched = CreateFileA(patchedExePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hPatched == INVALID_HANDLE_VALUE)
    {
        ShowError("一時実行ファイルの作成に失敗しました。");
        VirtualFree(patchedExe, 0, MEM_RELEASE);
        return 1;
    }

    DWORD bytesWritten = 0;
    if (!WriteFile(hPatched, patchedExe, patchedSize, &bytesWritten, NULL) || bytesWritten != patchedSize)
    {
        CloseHandle(hPatched);
        DeleteFileA(patchedExePath);
        ShowError("一時実行ファイルの書き込みに失敗しました。");
        VirtualFree(patchedExe, 0, MEM_RELEASE);
        return 1;
    }
    CloseHandle(hPatched);
    VirtualFree(patchedExe, 0, MEM_RELEASE);

    // Launch the patched game
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    char workDir[MAX_PATH];
    strncpy(workDir, moduleDir, sizeof(workDir) - 1);
    workDir[sizeof(workDir) - 1] = '\0';
    size_t workDirLen = strlen(workDir);
    if (workDirLen > 0 && workDir[workDirLen - 1] == '\\')
    {
        workDir[workDirLen - 1] = '\0';
    }

    LPSTR cmdLine = GetCommandLineA();
    if (!CreateProcessA(patchedExePath, cmdLine, NULL, NULL, FALSE, 0, NULL, workDir, &si, &pi))
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "ゲームプロセスの起動に失敗しました (Error: %lu)", GetLastError());
        ShowError(msg);
        DeleteFileA(patchedExePath);
        return 1;
    }

    // Wait for the game process to exit
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Clean up temporary patched executable
    for (int retry = 0; retry < 5; retry++)
    {
        if (DeleteFileA(patchedExePath))
        {
            break;
        }
        Sleep(100);
    }

    return 0;
}
