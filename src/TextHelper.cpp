#include "TextHelper.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
#include "i18n.hpp"
#include "render/renderer.hpp"
#include <stdlib.h>
#include <cstdint>
#include <vector>

namespace th06
{

FormatInfo g_FormatInfoArray[7] = {
    {D3DFMT_X8R8G8B8, 32, 0x00000000, 0x00FF0000, 0x0000FF00, 0x000000FF},
    {D3DFMT_A8R8G8B8, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF},
    {D3DFMT_X1R5G5B5, 16, 0x00000000, 0x00007C00, 0x000003E0, 0x0000001F},
    {D3DFMT_R5G6B5, 16, 0x00000000, 0x0000F800, 0x000007E0, 0x0000001F},
    {D3DFMT_A1R5G5B5, 16, 0x0000F000, 0x00007C00, 0x000003E0, 0x0000001F},
    {D3DFMT_A4R4G4B4, 16, 0x0000F000, 0x00000F00, 0x000000F0, 0x0000000F},
    {(D3DFORMAT)-1, 0, 0, 0, 0, 0},
};

TextHelper::TextHelper()
{
    this->format = (D3DFORMAT)-1;
    this->width = 0;
    this->height = 0;
    this->hdc = 0;
    this->gdiObj2 = 0;
    this->gdiObj = 0;
    this->buffer = NULL;
}

TextHelper::~TextHelper()
{
    this->ReleaseBuffer();
}

bool TextHelper::ReleaseBuffer()
{
    if (this->hdc)
    {
        SelectObject(this->hdc, this->gdiObj);
        DeleteDC(this->hdc);
        DeleteObject(this->gdiObj2);
        this->format = (D3DFORMAT)-1;
        this->width = 0;
        this->height = 0;
        this->hdc = 0;
        this->gdiObj2 = 0;
        this->gdiObj = 0;
        this->buffer = NULL;
        return true;
    }
    else
    {
        return false;
    }
}

#define TEXT_BUFFER_HEIGHT 64
void TextHelper::CreateTextBuffer()
{
    g_Supervisor.d3dDevice->CreateImageSurface(GAME_WINDOW_WIDTH, TEXT_BUFFER_HEIGHT, D3DFMT_A1R5G5B5,
                                               &g_TextBufferSurface);
}

bool TextHelper::AllocateBufferWithFallback(i32 width, i32 height, D3DFORMAT format)
{
    if (this->TryAllocateBuffer(width, height, format))
    {
        return true;
    }

    if (format == D3DFMT_A1R5G5B5 || format == D3DFMT_A4R4G4B4)
    {
        return this->TryAllocateBuffer(width, height, D3DFMT_A8R8G8B8);
    }
    if (format == D3DFMT_R5G6B5)
    {
        return this->TryAllocateBuffer(width, height, D3DFMT_X8R8G8B8);
    }
    return false;
}

struct THBITMAPINFO
{
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[17];
};

bool TextHelper::TryAllocateBuffer(i32 width, i32 height, D3DFORMAT format)
{
    HGDIOBJ originalBitmapObj;
    u8 *bitmapData;
    HBITMAP bitmapObj;
    FormatInfo *formatInfo;
    THBITMAPINFO bitmapInfo;
    u32 padding;
    HDC deviceContext;
    i32 imageWidthInBytes;

    this->ReleaseBuffer();
    memset(&bitmapInfo, 0, sizeof(THBITMAPINFO));
    formatInfo = this->GetFormatInfo(format);
    if (formatInfo == NULL)
    {
        return false;
    }
    imageWidthInBytes = ((((width * formatInfo->bitCount) / 8) + 3) / 4) * 4;
    bitmapInfo.bmiHeader.biSize = sizeof(THBITMAPINFO);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -(height + 1);
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = formatInfo->bitCount;
    bitmapInfo.bmiHeader.biSizeImage = height * imageWidthInBytes;
    if (format != D3DFMT_X1R5G5B5 && format != D3DFMT_X8R8G8B8)
    {
        bitmapInfo.bmiHeader.biCompression = 3;
        ((u32 *)bitmapInfo.bmiColors)[0] = formatInfo->redMask;
        ((u32 *)bitmapInfo.bmiColors)[1] = formatInfo->greenMask;
        ((u32 *)bitmapInfo.bmiColors)[2] = formatInfo->blueMask;
        ((u32 *)bitmapInfo.bmiColors)[3] = formatInfo->alphaMask;
    }
    bitmapObj = CreateDIBSection(NULL, (BITMAPINFO *)&bitmapInfo, 0, (void **)&bitmapData, NULL, 0);
    if (bitmapObj == NULL)
    {
        return false;
    }
    memset(bitmapData, 0, bitmapInfo.bmiHeader.biSizeImage);
    deviceContext = CreateCompatibleDC(NULL);
    originalBitmapObj = SelectObject(deviceContext, bitmapObj);
    this->hdc = deviceContext;
    this->gdiObj2 = bitmapObj;
    this->buffer = bitmapData;
    this->imageSizeInBytes = bitmapInfo.bmiHeader.biSizeImage;
    this->gdiObj = originalBitmapObj;
    this->width = width;
    this->height = height;
    this->format = format;
    this->imageWidthInBytes = imageWidthInBytes;
    return true;
}

FormatInfo *TextHelper::GetFormatInfo(D3DFORMAT format)
{
    i32 local_8;

    for (local_8 = 0; g_FormatInfoArray[local_8].format != -1 && g_FormatInfoArray[local_8].format != format; local_8++)
    {
    }
    if (format == -1)
    {
        return NULL;
    }
    return &g_FormatInfoArray[local_8];
}

struct A1R5G5B5
{
    u16 blue : 5;
    u16 green : 5;
    u16 red : 5;
    u16 alpha : 1;
};

bool TextHelper::InvertAlpha(i32 x, i32 y, i32 spriteWidth, i32 fontHeight)
{
    i32 doubleArea;
    u8 *bufferRegion;
    i32 idx;
    u8 *bufferStart;
    A1R5G5B5 *bufferCursor;

    doubleArea = spriteWidth * fontHeight * 2;
    bufferStart = &this->buffer[0];
    bufferRegion = &bufferStart[y * spriteWidth * 2];
    switch (this->format)
    {
    case D3DFMT_A8R8G8B8:
        for (idx = 3; idx < doubleArea; idx += 4)
        {
            bufferRegion[idx] = bufferRegion[idx] ^ 0xff;
        }
        break;
    case D3DFMT_A1R5G5B5:
        for (bufferCursor = (A1R5G5B5 *)bufferRegion, idx = 0; idx < doubleArea; idx += 2, bufferCursor += 1)
        {
            bufferCursor->alpha ^= 1;
            if (bufferCursor->alpha)
            {
                bufferCursor->red = bufferCursor->red - bufferCursor->red * idx / doubleArea / 2;
                bufferCursor->green = bufferCursor->green - bufferCursor->green * idx / doubleArea / 2;
                bufferCursor->blue = bufferCursor->blue - bufferCursor->blue * idx / doubleArea / 4;
            }
            else
            {
                bufferCursor->red = 31 - idx * 31 / doubleArea / 2;
                bufferCursor->green = 31 - idx * 31 / doubleArea / 2;
                bufferCursor->blue = 31 - idx * 31 / doubleArea / 4;
            }
        }
        break;
    case D3DFMT_A4R4G4B4:
        for (idx = 1; idx < doubleArea; idx = idx + 2)
        {
            bufferRegion[idx] = bufferRegion[idx] ^ 0xf0;
        }
        break;
    default:
        return false;
    }
    return true;
}

bool TextHelper::CopyTextToSurface(IDirect3DSurface8 *outSurface)
{
    D3DLOCKED_RECT lockedRect;
    u8 *srcBuf;
    D3DSURFACE_DESC outSurfaceDesc;
    size_t srcWidthBytes;
    int curHeight;
    RECT rectToLock;
    int dstWidthBytes;
    u8 *dstBuf;
    i32 width;
    i32 height;
    D3DFORMAT thisFormat;
    i32 thisHeight;

    if (!(bool)(u32)(this->gdiObj2 != NULL))
    {
        return false;
    }
    outSurface->GetDesc(&outSurfaceDesc);
    rectToLock.left = 0;
    rectToLock.top = 0;
    rectToLock.right = width = this->width;
    rectToLock.bottom = height = this->height;
    if (outSurface->LockRect(&lockedRect, &rectToLock, 0))
    {
        return false;
    }
    dstWidthBytes = lockedRect.Pitch;
    srcWidthBytes = this->imageWidthInBytes;
    srcBuf = this->buffer;
    dstBuf = (u8 *)lockedRect.pBits;
    thisFormat = this->format;
    if (outSurfaceDesc.Format == thisFormat)
    {
        for (curHeight = 0; thisHeight = this->height, curHeight < thisHeight; curHeight++)
        {
            memcpy(dstBuf, srcBuf, srcWidthBytes);
            srcBuf += srcWidthBytes;
            dstBuf += dstWidthBytes;
        }
    }
    outSurface->UnlockRect();
    return true;
}

void TextHelper::RenderTextToTexture(i32 xPos, i32 yPos, i32 spriteWidth, i32 spriteHeight, i32 fontHeight,
                                     i32 fontWidth, ZunColor textColor, ZunColor shadowColor, char *string,
                                     IDirect3DTexture8 *outTexture)
{
    HGDIOBJ h;
    LPDIRECT3DSURFACE8 destSurface;
    RECT destRect;
    RECT srcRect;
    D3DSURFACE_DESC textSurfaceDesc;
    HFONT font;
    HDC hdc;
    wchar_t faceName[32];
    wchar_t *wideBuf;
    const wchar_t *wideString;
    int wideLen;

    MultiByteToWideChar(932, 0, TH_FONT_NAME, -1, faceName, 32);
    faceName[31] = 0;
    wideBuf = NULL;
    wideString = L"";
    wideLen = MultiByteToWideChar(932, 0, string, (int)strlen(string), NULL, 0);
    if (wideLen > 0)
    {
        wideBuf = (wchar_t *)malloc((wideLen + 1) * sizeof(wchar_t));
        if (wideBuf != NULL)
        {
            wideLen = MultiByteToWideChar(932, 0, string, (int)strlen(string), wideBuf, wideLen);
            wideBuf[wideLen] = 0;
            wideString = wideBuf;
        }
        else
        {
            wideLen = 0;
        }
    }

    font = CreateFontW(fontHeight * 2, 0, 0, 0, FW_BOLD, false, false, false, SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, FF_ROMAN | FIXED_PITCH, faceName);
    TextHelper textHelper;
#ifdef USE_VULKAN
    textHelper.AllocateBufferWithFallback(GAME_WINDOW_WIDTH, TEXT_BUFFER_HEIGHT, D3DFMT_A1R5G5B5);
#else
    g_TextBufferSurface->GetDesc(&textSurfaceDesc);
    textHelper.AllocateBufferWithFallback(textSurfaceDesc.Width, textSurfaceDesc.Height, textSurfaceDesc.Format);
#endif
    hdc = textHelper.hdc;
    h = SelectObject(hdc, font);
    textHelper.InvertAlpha(0, 0, spriteWidth * 2, fontHeight * 2 + 6);
    SetBkMode(hdc, TRANSPARENT);

    if (shadowColor != COLOR_WHITE)
    {

        SetTextColor(hdc, shadowColor);
        TextOutW(hdc, xPos * 2 + 3, 2, wideString, wideLen);
    }

    SetTextColor(hdc, textColor);
    TextOutW(hdc, xPos * 2, 0, wideString, wideLen);
    if (wideBuf != NULL)
    {
        free(wideBuf);
    }

    SelectObject(hdc, h);
    textHelper.InvertAlpha(0, 0, spriteWidth * 2, fontHeight * 2 + 6);
#ifdef USE_VULKAN
    SelectObject(hdc, h);
    DeleteObject(font);
    {
        TextureHandle tex = (TextureHandle)(uintptr_t)outTexture;
        i32 srcW = spriteWidth * 2 - 2;
        i32 srcH = fontHeight * 2 - 2;
        if (tex != INVALID_TEXTURE && srcW > 0 && srcH > 0 && textHelper.buffer != NULL)
        {
            std::vector<uint32_t> argb((size_t)srcW * (size_t)srcH, 0);
            for (i32 yy = 0; yy < srcH; yy++)
            {
                const uint16_t *row =
                    (const uint16_t *)(textHelper.buffer + (size_t)yy * textHelper.imageWidthInBytes);
                for (i32 xx = 0; xx < srcW; xx++)
                {
                    uint16_t p = row[xx];
                    uint32_t a = ((p >> 15) & 1) ? 0xFFu : 0x00u;
                    uint32_t r = (uint32_t)(((p >> 10) & 0x1F) * 255 / 31);
                    uint32_t g = (uint32_t)(((p >> 5) & 0x1F) * 255 / 31);
                    uint32_t b = (uint32_t)((p & 0x1F) * 255 / 31);
                    argb[(size_t)yy * (size_t)srcW + (size_t)xx] = (a << 24) | (r << 16) | (g << 8) | b;
                }
            }
            g_Renderer->BlitPixelsToTexture(tex, 0, yPos, spriteWidth, 16, argb.data(), srcW, srcH);
        }
    }
    return;
#else
    textHelper.CopyTextToSurface(g_TextBufferSurface);
    SelectObject(hdc, h);
    DeleteObject(font);
    destRect.left = 0;
    destRect.top = yPos;
    destRect.right = spriteWidth;
    destRect.bottom = yPos + 16;
    srcRect.left = 0;
    srcRect.top = 0;
    srcRect.right = spriteWidth * 2 - 2;
    srcRect.bottom = fontHeight * 2 - 2;
    outTexture->GetSurfaceLevel(0, &destSurface);
    D3DXLoadSurfaceFromSurface(destSurface, NULL, &destRect, g_TextBufferSurface, NULL, &srcRect, 4, 0);
    if (destSurface != NULL)
    {
        destSurface->Release();
        destSurface = NULL;
    }
    return;
#endif
}

void th06::TextHelper::ReleaseTextBuffer()
{
    if (g_TextBufferSurface != NULL)
    {
        g_TextBufferSurface->Release();
        g_TextBufferSurface = NULL;
    }
    return;
}
}; // namespace th06
