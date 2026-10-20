#include "Screenshot.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "Controller.hpp"
#include "GameWindow.hpp"
#include "SoundPlayer.hpp"
#include "Supervisor.hpp"
#ifdef USE_VULKAN
#include "render/renderer.hpp"
#endif

namespace th06
{

static bool s_screenshotRequested = false;

void Screenshot_Request()
{
    s_screenshotRequested = true;
}

static bool IsKeyBound(i32 dik)
{
    return (g_KeyConfig.shoot == dik) || (g_KeyConfig.bomb == dik) || (g_KeyConfig.focus == dik) ||
           (g_KeyConfig.menu == dik) || (g_KeyConfig.up == dik) || (g_KeyConfig.down == dik) ||
           (g_KeyConfig.left == dik) || (g_KeyConfig.right == dik) || (g_KeyConfig.skip == dik);
}

void Screenshot_Update()
{
    static bool s_prevP = false;
    bool pDown = false;

    if (GetForegroundWindow() == g_Supervisor.hwndGameWindow)
    {
        if (g_Supervisor.keyboard != NULL)
        {
            u8 state[256];
            if (SUCCEEDED(g_Supervisor.keyboard->GetDeviceState(sizeof(state), state)))
            {
                pDown = (state[DIK_P] & 0x80) != 0;
            }
        }
        else
        {
            pDown = (GetAsyncKeyState('P') & 0x8000) != 0;
        }
    }

    if (pDown && !s_prevP)
    {
        if (!Supervisor::IsQuickSettingsWaitingKey() && !IsKeyBound(DIK_P))
        {
            Screenshot_Request();
        }
    }
    s_prevP = pDown;
}

static u32 s_crcTable[256];
static bool s_crcReady = false;

static void InitCrcTable()
{
    for (u32 i = 0; i < 256; i++)
    {
        u32 c = i;
        for (int j = 0; j < 8; j++)
        {
            c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
        }
        s_crcTable[i] = c;
    }
    s_crcReady = true;
}

static u32 Crc32(const void *buf, size_t len, u32 crc)
{
    if (!s_crcReady)
        InitCrcTable();
    const u8 *p = (const u8 *)buf;
    for (size_t i = 0; i < len; i++)
    {
        crc = s_crcTable[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc;
}

static u32 CalcAdler32(const u8 *data, size_t len)
{
    u32 s1 = 1;
    u32 s2 = 0;
    while (len > 0)
    {
        size_t block = len < 5552 ? len : 5552;
        len -= block;
        for (size_t i = 0; i < block; i++)
        {
            s1 += data[i];
            s2 += s1;
        }
        s1 %= 65521;
        s2 %= 65521;
    }
    return (s2 << 16) | s1;
}

static void WriteU32BE(FILE *f, u32 val)
{
    u8 b[4];
    b[0] = (u8)((val >> 24) & 0xFF);
    b[1] = (u8)((val >> 16) & 0xFF);
    b[2] = (u8)((val >> 8) & 0xFF);
    b[3] = (u8)(val & 0xFF);
    fwrite(b, 1, 4, f);
}

static bool WritePNG(const char *filename, u32 width, u32 height, const u8 *rawRgb)
{
    FILE *f = fopen(filename, "wb");
    if (!f)
        return false;

    const u8 sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(sig, 1, 8, f);

    u8 ihdr[13];
    ihdr[0] = (u8)((width >> 24) & 0xFF);
    ihdr[1] = (u8)((width >> 16) & 0xFF);
    ihdr[2] = (u8)((width >> 8) & 0xFF);
    ihdr[3] = (u8)(width & 0xFF);
    ihdr[4] = (u8)((height >> 24) & 0xFF);
    ihdr[5] = (u8)((height >> 16) & 0xFF);
    ihdr[6] = (u8)((height >> 8) & 0xFF);
    ihdr[7] = (u8)(height & 0xFF);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    WriteU32BE(f, 13);
    fwrite("IHDR", 1, 4, f);
    fwrite(ihdr, 1, 13, f);
    u32 ihdrCrc = Crc32("IHDR", 4, 0xFFFFFFFFL);
    ihdrCrc = Crc32(ihdr, 13, ihdrCrc) ^ 0xFFFFFFFFL;
    WriteU32BE(f, ihdrCrc);

    size_t rawLen = height * (1 + width * 3);
    size_t numBlocks = (rawLen + 65534) / 65535;
    if (numBlocks == 0)
        numBlocks = 1;
    u32 idatDataLen = (u32)(2 + numBlocks * 5 + rawLen + 4);

    WriteU32BE(f, idatDataLen);
    fwrite("IDAT", 1, 4, f);
    u32 idatCrc = Crc32("IDAT", 4, 0xFFFFFFFFL);

    u8 zlibHdr[2] = {0x78, 0x01};
    fwrite(zlibHdr, 1, 2, f);
    idatCrc = Crc32(zlibHdr, 2, idatCrc);

    size_t offset = 0;
    while (offset < rawLen)
    {
        size_t chunk = rawLen - offset;
        if (chunk > 65535)
            chunk = 65535;
        bool isLast = (offset + chunk == rawLen);
        u8 blkHdr[5];
        blkHdr[0] = isLast ? 0x01 : 0x00;
        blkHdr[1] = (u8)(chunk & 0xFF);
        blkHdr[2] = (u8)((chunk >> 8) & 0xFF);
        u16 nlen = (u16)(~chunk & 0xFFFF);
        blkHdr[3] = (u8)(nlen & 0xFF);
        blkHdr[4] = (u8)((nlen >> 8) & 0xFF);

        fwrite(blkHdr, 1, 5, f);
        idatCrc = Crc32(blkHdr, 5, idatCrc);

        fwrite(rawRgb + offset, 1, chunk, f);
        idatCrc = Crc32(rawRgb + offset, chunk, idatCrc);

        offset += chunk;
    }

    u32 adler = CalcAdler32(rawRgb, rawLen);
    u8 adlerBuf[4];
    adlerBuf[0] = (u8)((adler >> 24) & 0xFF);
    adlerBuf[1] = (u8)((adler >> 16) & 0xFF);
    adlerBuf[2] = (u8)((adler >> 8) & 0xFF);
    adlerBuf[3] = (u8)(adler & 0xFF);
    fwrite(adlerBuf, 1, 4, f);
    idatCrc = Crc32(adlerBuf, 4, idatCrc) ^ 0xFFFFFFFFL;
    WriteU32BE(f, idatCrc);

    WriteU32BE(f, 0);
    fwrite("IEND", 1, 4, f);
    u32 iendCrc = Crc32("IEND", 4, 0xFFFFFFFFL) ^ 0xFFFFFFFFL;
    WriteU32BE(f, iendCrc);

    fclose(f);
    return true;
}

void Screenshot_SaveIfRequested()
{
    if (!s_screenshotRequested)
        return;
    s_screenshotRequested = false;

#ifdef USE_VULKAN
    // Read the presented frame back through the renderer (the dummy backbuffer
    // never contains the rendered image, so the D3D path would save black).
    if (g_Renderer == NULL)
        return;

    std::vector<u32> argb;
    if (!g_Renderer->ReadGameImage(argb) || argb.empty())
        return;
    const u32 width = 640;
    const u32 height = 480;

    const size_t rawSize = height * (1 + width * 3);
    u8 *raw = (u8 *)malloc(rawSize);
    if (raw == NULL)
        return;

    for (u32 y = 0; y < height; y++)
    {
        u8 *dstRow = raw + y * (1 + width * 3);
        *dstRow++ = 0;
        const u32 *srcRow = &argb[(size_t)y * width];
        for (u32 x = 0; x < width; x++)
        {
            u32 p = srcRow[x];
            dstRow[x * 3 + 0] = (u8)((p >> 0) & 0xFF);
            dstRow[x * 3 + 1] = (u8)((p >> 8) & 0xFF);
            dstRow[x * 3 + 2] = (u8)((p >> 16) & 0xFF);
        }
    }
#else
    if (g_Supervisor.d3dDevice == NULL)
        return;

    IDirect3DSurface8 *backbuf = NULL;
    if (FAILED(g_Supervisor.d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backbuf)) || backbuf == NULL)
    {
        return;
    }

    D3DSURFACE_DESC desc;
    backbuf->GetDesc(&desc);
    const u32 width = desc.Width;
    const u32 height = desc.Height;

    D3DLOCKED_RECT lr;
    IDirect3DSurface8 *readableSurf = backbuf;
    IDirect3DSurface8 *sysMemSurf = NULL;

    HRESULT hr = backbuf->LockRect(&lr, NULL, D3DLOCK_READONLY);
    if (FAILED(hr))
    {
        hr = g_Supervisor.d3dDevice->CreateImageSurface(width, height, desc.Format, &sysMemSurf);
        if (SUCCEEDED(hr))
        {
            hr = g_Supervisor.d3dDevice->CopyRects(backbuf, NULL, 0, sysMemSurf, NULL);
            if (SUCCEEDED(hr))
            {
                readableSurf = sysMemSurf;
                hr = readableSurf->LockRect(&lr, NULL, D3DLOCK_READONLY);
            }
        }
    }

    if (FAILED(hr))
    {
        if (sysMemSurf != NULL)
            sysMemSurf->Release();
        backbuf->Release();
        return;
    }

    const size_t rawSize = height * (1 + width * 3);
    u8 *raw = (u8 *)malloc(rawSize);
    if (raw == NULL)
    {
        readableSurf->UnlockRect();
        if (sysMemSurf != NULL)
            sysMemSurf->Release();
        backbuf->Release();
        return;
    }

    for (u32 y = 0; y < height; y++)
    {
        u8 *dstRow = raw + y * (1 + width * 3);
        *dstRow++ = 0;

        const u8 *srcRow = ((const u8 *)lr.pBits) + y * lr.Pitch;

        if (desc.Format == D3DFMT_X8R8G8B8 || desc.Format == D3DFMT_A8R8G8B8)
        {
            for (u32 x = 0; x < width; x++)
            {
                dstRow[x * 3 + 0] = srcRow[x * 4 + 2];
                dstRow[x * 3 + 1] = srcRow[x * 4 + 1];
                dstRow[x * 3 + 2] = srcRow[x * 4 + 0];
            }
        }
        else if (desc.Format == D3DFMT_R5G6B5)
        {
            const u16 *src16 = (const u16 *)srcRow;
            for (u32 x = 0; x < width; x++)
            {
                u16 p = src16[x];
                dstRow[x * 3 + 0] = (u8)(((p >> 11) & 0x1F) * 255 / 31);
                dstRow[x * 3 + 1] = (u8)(((p >> 5) & 0x3F) * 255 / 63);
                dstRow[x * 3 + 2] = (u8)((p & 0x1F) * 255 / 31);
            }
        }
        else if (desc.Format == D3DFMT_X1R5G5B5 || desc.Format == D3DFMT_A1R5G5B5)
        {
            const u16 *src16 = (const u16 *)srcRow;
            for (u32 x = 0; x < width; x++)
            {
                u16 p = src16[x];
                dstRow[x * 3 + 0] = (u8)(((p >> 10) & 0x1F) * 255 / 31);
                dstRow[x * 3 + 1] = (u8)(((p >> 5) & 0x1F) * 255 / 31);
                dstRow[x * 3 + 2] = (u8)((p & 0x1F) * 255 / 31);
            }
        }
        else
        {
            memset(dstRow, 0, width * 3);
        }
    }

    readableSurf->UnlockRect();
    if (sysMemSurf != NULL)
        sysMemSurf->Release();
    backbuf->Release();
#endif

    CreateDirectoryA("./snapshot", NULL);

    char filename[MAX_PATH];
    for (int i = 0; i < 10000; i++)
    {
        snprintf(filename, sizeof(filename), "./snapshot/th06_%02d.png", i);
        DWORD attr = GetFileAttributesA(filename);
        if (attr == INVALID_FILE_ATTRIBUTES)
        {
            break;
        }
    }

    if (WritePNG(filename, width, height, raw))
    {
        g_SoundPlayer.PlaySoundByIdx(SOUND_SELECT, 0);
    }

    free(raw);
}

}; // namespace th06
