#include "AnmManager.hpp"
#include "AsciiManager.hpp"
#include "FileSystem.hpp"
#include "GameErrorContext.hpp"
#include "GameWindow.hpp"
#include "Rng.hpp"
#include "Supervisor.hpp"
#include "TextHelper.hpp"
#include "ZunMath.hpp"
#include "i18n.hpp"
#include "utils.hpp"

#include <stdio.h>

#ifdef USE_VULKAN
#include "renderer.hpp"
#endif

namespace th06
{
VertexTex1Xyzrwh g_PrimitivesToDrawVertexBuf[4];
VertexTex1DiffuseXyzrwh g_PrimitivesToDrawNoVertexBuf[4];
VertexTex1DiffuseXyz g_PrimitivesToDrawUnknown[4];
AnmManager *g_AnmManager;

#ifndef DIFFBUILD
D3DFORMAT g_TextureFormatD3D8Mapping[6] = {
    D3DFMT_UNKNOWN, D3DFMT_A8R8G8B8, D3DFMT_A1R5G5B5, D3DFMT_R5G6B5, D3DFMT_R8G8B8, D3DFMT_A4R4G4B4,
};
#endif

#define TEX_FMT_UNKNOWN 0
#define TEX_FMT_A8R8G8B8 1
#define TEX_FMT_A1R5G5B5 2
#define TEX_FMT_R5G6B5 3
#define TEX_FMT_R8G8B8 4
#define TEX_FMT_A4R4G4B4 5

void AnmManager::ReleaseSurfaces(void)
{
    for (i32 idx = 0; idx < ARRAY_SIZE_SIGNED(this->surfaces); idx++)
    {
        if (this->surfaces[idx] != NULL)
        {
            this->surfaces[idx]->Release();
            this->surfaces[idx] = NULL;
        }
    }
}

void AnmManager::TakeScreenshotIfRequested()
{
    if (this->screenshotTextureId >= 0)
    {
        this->TakeScreenshot(this->screenshotTextureId, this->screenshotLeft, this->screenshotTop,
                             this->screenshotWidth, this->screenshotHeight);
        this->screenshotTextureId = -1;
    }
    return;
}

AnmManager::AnmManager()
{
    this->maybeLoadedSpriteCount = 0;

    memset(this, 0, sizeof(AnmManager));

    for (i32 spriteIndex = 0; spriteIndex < ARRAY_SIZE_SIGNED(this->sprites); spriteIndex++)
    {
        this->sprites[spriteIndex].sourceFileIndex = -1;
    }

    g_PrimitivesToDrawVertexBuf[3].position.w = 1.0;
    g_PrimitivesToDrawVertexBuf[2].position.w = g_PrimitivesToDrawVertexBuf[3].position.w;
    g_PrimitivesToDrawVertexBuf[1].position.w = g_PrimitivesToDrawVertexBuf[2].position.w;
    g_PrimitivesToDrawVertexBuf[0].position.w = g_PrimitivesToDrawVertexBuf[1].position.w;
    g_PrimitivesToDrawVertexBuf[0].textureUV.x = 0.0;
    g_PrimitivesToDrawVertexBuf[0].textureUV.y = 0.0;
    g_PrimitivesToDrawVertexBuf[1].textureUV.x = 1.0;
    g_PrimitivesToDrawVertexBuf[1].textureUV.y = 0.0;
    g_PrimitivesToDrawVertexBuf[2].textureUV.x = 0.0;
    g_PrimitivesToDrawVertexBuf[2].textureUV.y = 1.0;
    g_PrimitivesToDrawVertexBuf[3].textureUV.x = 1.0;
    g_PrimitivesToDrawVertexBuf[3].textureUV.y = 1.0;

    g_PrimitivesToDrawNoVertexBuf[3].position.w = 1.0;
    g_PrimitivesToDrawNoVertexBuf[2].position.w = g_PrimitivesToDrawNoVertexBuf[3].position.w;
    g_PrimitivesToDrawNoVertexBuf[1].position.w = g_PrimitivesToDrawNoVertexBuf[2].position.w;
    g_PrimitivesToDrawNoVertexBuf[0].position.w = g_PrimitivesToDrawNoVertexBuf[1].position.w;
    g_PrimitivesToDrawNoVertexBuf[0].textureUV.x = 0.0;
    g_PrimitivesToDrawNoVertexBuf[0].textureUV.y = 0.0;
    g_PrimitivesToDrawNoVertexBuf[1].textureUV.x = 1.0;
    g_PrimitivesToDrawNoVertexBuf[1].textureUV.y = 0.0;
    g_PrimitivesToDrawNoVertexBuf[2].textureUV.x = 0.0;
    g_PrimitivesToDrawNoVertexBuf[2].textureUV.y = 1.0;
    g_PrimitivesToDrawNoVertexBuf[3].textureUV.x = 1.0;
    g_PrimitivesToDrawNoVertexBuf[3].textureUV.y = 1.0;

    this->vertexBuffer = NULL;
    this->currentTexture = NULL;
    this->currentBlendMode = 0;
    this->currentColorOp = 0;
    this->currentTextureFactor = 1;
    this->currentVertexShader = 0;
    this->currentZWriteDisable = 0;
    this->screenshotTextureId = -1;
}

void AnmManager::SetupVertexBuffer()
{
    this->vertexBufferContents[2].position.x = -128;
    this->vertexBufferContents[0].position.x = -128;
    this->vertexBufferContents[3].position.x = 128;
    this->vertexBufferContents[1].position.x = 128;

    this->vertexBufferContents[1].position.y = -128;
    this->vertexBufferContents[0].position.y = -128;
    this->vertexBufferContents[3].position.y = 128;
    this->vertexBufferContents[2].position.y = 128;

    this->vertexBufferContents[3].position.z = 0;
    this->vertexBufferContents[2].position.z = 0;
    this->vertexBufferContents[1].position.z = 0;
    this->vertexBufferContents[0].position.z = 0;

    this->vertexBufferContents[2].textureUV.x = 0;
    this->vertexBufferContents[0].textureUV.x = 0;
    this->vertexBufferContents[3].textureUV.x = 1;
    this->vertexBufferContents[1].textureUV.x = 1;
    this->vertexBufferContents[1].textureUV.y = 0;
    this->vertexBufferContents[0].textureUV.y = 0;
    this->vertexBufferContents[3].textureUV.y = 1;
    this->vertexBufferContents[2].textureUV.y = 1;

    g_PrimitivesToDrawUnknown[0].position = this->vertexBufferContents[0].position;
    g_PrimitivesToDrawUnknown[1].position = this->vertexBufferContents[1].position;
    g_PrimitivesToDrawUnknown[2].position = this->vertexBufferContents[2].position;
    g_PrimitivesToDrawUnknown[3].position = this->vertexBufferContents[3].position;

    g_PrimitivesToDrawUnknown[0].textureUV.x = this->vertexBufferContents[0].textureUV.x;
    g_PrimitivesToDrawUnknown[0].textureUV.y = this->vertexBufferContents[0].textureUV.y;
    g_PrimitivesToDrawUnknown[1].textureUV.x = this->vertexBufferContents[1].textureUV.x;
    g_PrimitivesToDrawUnknown[1].textureUV.y = this->vertexBufferContents[1].textureUV.y;
    g_PrimitivesToDrawUnknown[2].textureUV.x = this->vertexBufferContents[2].textureUV.x;
    g_PrimitivesToDrawUnknown[2].textureUV.y = this->vertexBufferContents[2].textureUV.y;
    g_PrimitivesToDrawUnknown[3].textureUV.x = this->vertexBufferContents[3].textureUV.x;
    g_PrimitivesToDrawUnknown[3].textureUV.y = this->vertexBufferContents[3].textureUV.y;

    RenderVertexInfo *buffer;

    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        g_Supervisor.d3dDevice->CreateVertexBuffer(sizeof(this->vertexBufferContents), 0, D3DFVF_TEX1 | D3DFVF_XYZ,
                                                   D3DPOOL_MANAGED, &this->vertexBuffer);

        this->vertexBuffer->Lock(0, 0, (BYTE **)&buffer, 0);
        memcpy(buffer, this->vertexBufferContents, sizeof(this->vertexBufferContents));
        this->vertexBuffer->Unlock();

        g_Supervisor.d3dDevice->SetStreamSource(0, g_AnmManager->vertexBuffer, sizeof(RenderVertexInfo));
    }
}

ZunResult AnmManager::LoadTexture(i32 textureIdx, char *textureName, i32 textureFormat, D3DCOLOR colorKey)
{
    ReleaseTexture(textureIdx);
    this->imageDataArray[textureIdx] = FileSystem::OpenPath(textureName, 0);

    if (this->imageDataArray[textureIdx] == NULL)
    {
        return ZUN_ERROR;
    }

    // (USE_VULKAN) loadTextureClampToEdge is set by LoadAnm before calling this
    // for textures whose sprites reach the texture border.
#ifdef USE_VULKAN
    {
        TextureHandle handle =
            g_Renderer->CreateTextureFromMemory(this->imageDataArray[textureIdx], g_LastFileSize, colorKey, this->loadTextureClampToEdge);
        this->textures[textureIdx] = (IDirect3DTexture8 *)(uintptr_t)handle;
        if (handle == INVALID_TEXTURE)
        {
            return ZUN_ERROR;
        }
    }
    return ZUN_SUCCESS;
#else
    if (D3DXCreateTextureFromFileInMemoryEx(g_Supervisor.d3dDevice, this->imageDataArray[textureIdx], g_LastFileSize, 0,
                                            0, 0, 0, g_TextureFormatD3D8Mapping[textureFormat], D3DPOOL_MANAGED,
                                            D3DX_FILTER_NONE | D3DX_FILTER_POINT, D3DX_DEFAULT, colorKey, NULL, NULL,
                                            &this->textures[textureIdx]) != D3D_OK)
    {
        return ZUN_ERROR;
    }

    return ZUN_SUCCESS;
#endif
}

ZunResult AnmManager::LoadTextureAlphaChannel(i32 textureIdx, char *textureName, i32 textureFormat, D3DCOLOR colorKey)
{
    struct Argb1555Pixel
    {
        u16 b : 5;
        u16 g : 5;
        u16 r : 5;
        u16 a : 1;
    };

    struct Argb4444Pixel
    {
        u16 b : 4;
        u16 g : 4;
        u16 r : 4;
        u16 a : 4;
    };

    IDirect3DTexture8 *textureSrc;
    D3DSURFACE_DESC surfaceDesc;
    D3DLOCKED_RECT lockedRectDst;
    D3DLOCKED_RECT lockedRectSrc;
    u8 *data;

    u8 *dstData0;
    u8 *srcData0;
    i32 x0;
    i32 y0;
    Argb1555Pixel *dstData1;
    Argb1555Pixel *srcData1;
    i32 y1;
    i32 x1;
    Argb4444Pixel *dstData2;
    Argb4444Pixel *srcData2;
    i32 y2;
    i32 x2;

    textureSrc = NULL;
    data = FileSystem::OpenPath(textureName, 0);

    if (data == NULL)
    {
        return ZUN_ERROR;
    }

#ifdef USE_VULKAN
    {
        TextureHandle srcTex = g_Renderer->CreateTextureFromMemory(data, g_LastFileSize, colorKey);
        TextureHandle dstTex = (TextureHandle)(uintptr_t)this->textures[textureIdx];
        free(data);
        data = NULL;

        if (srcTex == INVALID_TEXTURE || dstTex == INVALID_TEXTURE)
        {
            if (srcTex != INVALID_TEXTURE)
            {
                g_Renderer->ReleaseTexture(srcTex);
            }
            return ZUN_ERROR;
        }

        SurfaceDesc srcDesc;
        SurfaceDesc dstDesc;
        if (g_Renderer->LockTexture(srcTex, &srcDesc) && g_Renderer->LockTexture(dstTex, &dstDesc))
        {
            u8 *srcPixels = (u8 *)srcDesc.pixels;
            u8 *dstPixels = (u8 *)dstDesc.pixels;
            u32 pixelCount = dstDesc.width * dstDesc.height;
            for (u32 i = 0; i < pixelCount; i++)
            {
                // The original copies the source blue channel into the
                // destination alpha channel (see the D3D path below).
                dstPixels[i * 4 + 3] = srcPixels[i * 4 + 0];
            }

            // Alpha bleed: transparent texels keep whatever RGB happened to be
            // stored in the colour texture (front.png stores red/white/black in
            // the transparent margins around each sprite). Bilinear filtering
            // across a sprite edge interpolates that garbage RGB into the
            // visible edge pixels, producing coloured seams. Dilate the opaque
            // RGB one texel into the surrounding transparent texels so edge
            // filtering can only ever pull in the sprite's own colours. The
            // alpha channel is left alone here; sprite edges that reach a
            // texture border get CLAMP_TO_EDGE sampling in the renderer, and
            // interior sprite edges are handled by BuildSpriteDecoys once the
            // sprite rectangles are known, so that transparent padding inside
            // a sprite is not turned opaque.
            {
                u32 w = dstDesc.width;
                u32 h = dstDesc.height;
                for (u32 y = 0; y < h; y++)
                {
                    for (u32 x = 0; x < w; x++)
                    {
                        u32 i = (y * w + x) * 4;
                        if (dstPixels[i + 3] != 0)
                        {
                            continue;
                        }
                        u32 r = 0;
                        u32 g = 0;
                        u32 b = 0;
                        u32 cnt = 0;
                        for (i32 dy = -1; dy <= 1; dy++)
                        {
                            for (i32 dx = -1; dx <= 1; dx++)
                            {
                                if (dx == 0 && dy == 0)
                                {
                                    continue;
                                }
                                u32 nx = (u32)(((i32)x + dx + (i32)w) % (i32)w);
                                u32 ny = (u32)(((i32)y + dy + (i32)h) % (i32)h);
                                u32 ni = (ny * w + nx) * 4;
                                if (dstPixels[ni + 3] != 0)
                                {
                                    r += dstPixels[ni + 0];
                                    g += dstPixels[ni + 1];
                                    b += dstPixels[ni + 2];
                                    cnt++;
                                }
                            }
                        }
                        if (cnt != 0)
                        {
                            dstPixels[i + 0] = (u8)(r / cnt);
                            dstPixels[i + 1] = (u8)(g / cnt);
                            dstPixels[i + 2] = (u8)(b / cnt);
                        }
                    }
                }
            }
        }
        g_Renderer->UnlockTexture(srcTex);
        g_Renderer->UnlockTexture(dstTex);
        g_Renderer->ReleaseTexture(srcTex);
        return ZUN_SUCCESS;
    }
#else
    this->textures[textureIdx]->GetLevelDesc(0, &surfaceDesc);

    if (surfaceDesc.Format != D3DFMT_A8R8G8B8 && surfaceDesc.Format != D3DFMT_A4R4G4B4 &&
        surfaceDesc.Format != D3DFMT_A1R5G5B5)
    {
        g_GameErrorContext.Fatal(TH_ERR_ANMMANAGER_UNK_TEX_FORMAT);
        goto err;
    }

    if (D3DXCreateTextureFromFileInMemoryEx(g_Supervisor.d3dDevice, data, g_LastFileSize, 0, 0, 0, 0,
                                            surfaceDesc.Format, D3DPOOL_SYSTEMMEM, D3DX_FILTER_NONE | D3DX_FILTER_POINT,
                                            D3DX_DEFAULT, colorKey, NULL, NULL, &textureSrc) != D3D_OK)
    {
        goto err;
    }

    if (this->textures[textureIdx]->LockRect(0, &lockedRectDst, NULL, 0) != 0)
        goto err;

    if (textureSrc->LockRect(0, &lockedRectSrc, NULL, D3DLOCK_NO_DIRTY_UPDATE) != 0)
        goto err;

    switch (surfaceDesc.Format)
    {
    case D3DFMT_A8R8G8B8:
        for (y0 = 0; y0 < surfaceDesc.Height; y0++)
        {
            dstData0 = (u8 *)lockedRectDst.pBits + y0 * lockedRectDst.Pitch;
            srcData0 = (u8 *)lockedRectSrc.pBits + y0 * lockedRectSrc.Pitch;

            for (x0 = 0; x0 < surfaceDesc.Width; x0++, srcData0 += 4, dstData0 += 4)
            {
                dstData0[3] = srcData0[0];
            }
        }
        break;

    case D3DFMT_A1R5G5B5:
        for (y1 = 0; y1 < surfaceDesc.Height; y1++)
        {

            dstData1 = (Argb1555Pixel *)((u8 *)lockedRectDst.pBits + y1 * lockedRectDst.Pitch);
            srcData1 = (Argb1555Pixel *)((u8 *)lockedRectSrc.pBits + y1 * lockedRectSrc.Pitch);

            for (x1 = 0; x1 < surfaceDesc.Width; x1++, srcData1++, dstData1++)
            {
                dstData1->a = srcData1->b >> 4;
            }
        }
        break;

    case D3DFMT_A4R4G4B4:
        for (y2 = 0; y2 < surfaceDesc.Height; y2++)
        {
            dstData2 = (Argb4444Pixel *)((u8 *)lockedRectDst.pBits + y2 * lockedRectDst.Pitch);
            srcData2 = (Argb4444Pixel *)((u8 *)lockedRectSrc.pBits + y2 * lockedRectSrc.Pitch);

            for (x2 = 0; x2 < surfaceDesc.Width; x2++, srcData2++, dstData2++)
            {
                dstData2->a = srcData2->b;
            }
        }
        break;
    }

    textureSrc->UnlockRect(0);
    this->textures[textureIdx]->UnlockRect(0);

    if (textureSrc != NULL)
    {
        textureSrc->Release();
        textureSrc = NULL;
    }

    free(data);
    return ZUN_SUCCESS;

err:
    if (textureSrc != NULL)
    {
        textureSrc->Release();
        textureSrc = NULL;
    }

    free(data);
    return ZUN_ERROR;
#endif
}

ZunResult AnmManager::CreateEmptyTexture(i32 textureIdx, u32 width, u32 height, i32 textureFormat)
{
#ifdef USE_VULKAN
    this->textures[textureIdx] = (IDirect3DTexture8 *)(uintptr_t)g_Renderer->CreateTexture(width, height);
#else
    D3DXCreateTexture(g_Supervisor.d3dDevice, width, height, 1, 0, g_TextureFormatD3D8Mapping[textureFormat],
                      D3DPOOL_MANAGED, this->textures + textureIdx);
#endif

    return ZUN_SUCCESS;
}

#ifdef USE_VULKAN
// Returns true when any script in the anm drifts UV coordinates over the
// texture (opcode 27/28). Such textures must keep the REPEAT sampler;
// clamp-sampling a scrolling sprite would break the seamless wrap.
bool AnmManager::AnmUsesUvScroll(AnmRawEntry *anm)
{
    u32 *scriptTable = anm->spriteOffsets + anm->numSprites;
    // FileSystem::OpenPath stored the exact size of this file - never scan
    // past the end of the loaded buffer.
    u32 fileSize = g_LastFileSize;
    for (i32 s = 0; s < anm->numScripts; s++)
    {
        if (fileSize == 0)
        {
            break;
        }
        AnmRawInstr *instr = (AnmRawInstr *)((u8 *)anm + scriptTable[2 * s + 1]);
        // The instruction stride is 8 + size bytes, always positive, so the
        // file bound guarantees termination (jump targets stay in-stream).
        while ((u8 *)instr + 8 <= (u8 *)anm + fileSize && (u8 *)instr >= (u8 *)anm)
        {
            if (instr->opcode == AnmOpcode_UVScrollX || instr->opcode == AnmOpcode_UVScrollY)
            {
                return true;
            }
            instr = (AnmRawInstr *)((u8 *)instr->args + instr->argsCount);
        }
    }
    return false;
}
#endif

ZunResult AnmManager::LoadAnm(i32 anmIdx, const char *path, i32 spriteIdxOffset)
{
    this->ReleaseAnm(anmIdx);
    this->anmFiles[anmIdx] = (AnmRawEntry *)FileSystem::OpenPath(path, 0);

    AnmRawEntry *anm = this->anmFiles[anmIdx];


    if (anm == NULL)
    {
        g_GameErrorContext.Fatal(TH_ERR_ANMMANAGER_SPRITE_CORRUPTED, path);
        return ZUN_ERROR;
    }

    anm->textureIdx = anmIdx;

    char *anmName = (char *)((u8 *)anm + anm->nameOffset);


#ifdef USE_VULKAN
    // Decide the sampler wrap mode for this texture now: it is needed by
    // LoadTexture below, which runs before the sprite decoy / script loops.
    // Sprites that span a full texture edge-to-edge wrap around with the
    // REPEAT sampler when bilinear fine sampling reaches past the opposite
    // edge (visible as dark grid lines around full-texture sprites like the
    // character select portrait), so such textures use CLAMP_TO_EDGE instead
    // - unless any script scrolls UVs, which needs REPEAT.
    {
        bool anySpriteTouchesBorder = false;
        u32 *sprOff = anm->spriteOffsets;
        for (i32 s = 0; s < anm->numSprites; s++, sprOff++)
        {
            AnmRawSprite *rawSprite = (AnmRawSprite *)((u8 *)anm + *sprOff);
            f32 x0 = rawSprite->offset.x;
            f32 y0 = rawSprite->offset.y;
            f32 x1 = x0 + rawSprite->size.x;
            f32 y1 = y0 + rawSprite->size.y;
            if (x0 <= 0.0f || x1 >= (f32)anm->width || y0 <= 0.0f || y1 >= (f32)anm->height)
            {
                anySpriteTouchesBorder = true;
            }
        }
        this->loadTextureClampToEdge = false;
        if (anySpriteTouchesBorder && !this->AnmUsesUvScroll(anm))
        {
            this->loadTextureClampToEdge = true;
        }
    }
#endif

    if (*anmName == '@')
    {
        this->CreateEmptyTexture(anm->textureIdx, anm->width, anm->height, anm->format);
    }
    else if (this->LoadTexture(anm->textureIdx, anmName, anm->format, anm->colorKey) != ZUN_SUCCESS)
    {
        g_GameErrorContext.Fatal(TH_ERR_ANMMANAGER_TEXTURE_CORRUPTED, anmName);
        return ZUN_ERROR;
    }

    if (anm->mipmapNameOffset != 0)
    {
        anmName = (char *)((u8 *)anm + anm->mipmapNameOffset);
        if (this->LoadTextureAlphaChannel(anm->textureIdx, anmName, anm->format, anm->colorKey) != ZUN_SUCCESS)
        {
            g_GameErrorContext.Fatal(TH_ERR_ANMMANAGER_TEXTURE_CORRUPTED, anmName);
            return ZUN_ERROR;
        }
    }

    anm->spriteIdxOffset = spriteIdxOffset;

    u32 *curSpriteOffset = anm->spriteOffsets;

    i32 index;
    AnmRawSprite *rawSprite;

    for (index = 0; index < this->anmFiles[anmIdx]->numSprites; index++, curSpriteOffset++)
    {
        rawSprite = (AnmRawSprite *)((u8 *)anm + *curSpriteOffset);

        AnmLoadedSprite loadedSprite;
        loadedSprite.sourceFileIndex = this->anmFiles[anmIdx]->textureIdx;
        loadedSprite.startPixelInclusive.x = rawSprite->offset.x;
        loadedSprite.startPixelInclusive.y = rawSprite->offset.y;
        loadedSprite.endPixelInclusive.x = rawSprite->offset.x + rawSprite->size.x;
        loadedSprite.endPixelInclusive.y = rawSprite->offset.y + rawSprite->size.y;
        loadedSprite.textureWidth = (float)anm->width;
        loadedSprite.textureHeight = (float)anm->height;
        this->LoadSprite(rawSprite->id + spriteIdxOffset, &loadedSprite);
    }

#ifdef USE_VULKAN
    // NOTE: widthPx/heightPx are the same as before (the rectangle size is
    // unchanged), so the in-game scale of every sprite is preserved.
    BuildSpriteDecoys(anmIdx);
#endif

    for (index = 0; index < anm->numScripts; index++, curSpriteOffset += 2)
    {
        this->scripts[curSpriteOffset[0] + spriteIdxOffset] = (AnmRawInstr *)((u8 *)anm + curSpriteOffset[1]);
        this->spriteIndices[curSpriteOffset[0] + spriteIdxOffset] = spriteIdxOffset;
    }

    this->anmFilesSpriteIndexOffsets[anmIdx] = spriteIdxOffset;

    return ZUN_SUCCESS;
}

#ifdef USE_VULKAN
void AnmManager::BuildSpriteDecoys(i32 anmIdx)
{
    static const bool noDecoyDbg = getenv("VKDBG_NO_DECOY") != nullptr;
    if (noDecoyDbg)
    {
        return;
    }

    // Redirect each sprite's UV rectangle to a copy of its own artwork placed in
    // a free area of the texture, surrounded by a ring of replicated edge
    // pixels. At non-integer scales (e.g. 2.25x) the bilinear sampler near a
    // sprite edge averages in up to one texel beyond the sprite rectangle; with
    // the ring those texels are copies of the sprite's own edge pixels instead
    // of unrelated (usually transparent or wrong-coloured) neighbours, so edge
    // rows/columns no longer darken or pick up red/white spill. Sprites whose
    // padded copy does not fit in the free texture area keep their original
    // rectangle so they behave exactly as before.

    AnmRawEntry *anm = this->anmFiles[anmIdx];
    if (anm == NULL || this->textures[anm->textureIdx] == NULL)
    {
        return;
    }

    TextureHandle tex = (TextureHandle)(uintptr_t)this->textures[anm->textureIdx];
    if (tex == INVALID_TEXTURE)
    {
        return;
    }

    SurfaceDesc desc;
    if (!g_Renderer->LockTexture(tex, &desc))
    {
        return;
    }

    u8 *px = (u8 *)desc.pixels;
    u32 w = desc.width;
    u32 h = desc.height;
    u32 pixelCount = w * h;

    u8 *used = new u8[pixelCount];
    u8 *inRect = new u8[pixelCount];
    for (u32 i = 0; i < pixelCount; i++)
    {
        used[i] = (px[i * 4 + 3] != 0) ? 1 : 0;
    }
    memset(inRect, 0, pixelCount);

    // The text system (TextHelper::RenderTextToTexture) blits a full-width
    // 16-pixel-tall strip into rows [yPos, yPos+16) of the sprite's texture on
    // every string render, where yPos is the text sprite's rectangle top. The
    // text sprites' rects all start at row 0, so rows 0..15 of every texture
    // are rewritten at runtime and must not hold decoys. Sprites whose rects
    // intersect that band keep their original rects too: redirecting them to a
    // decoy copy would sample a stale snapshot that text renders acidly
    // overwrite (garbled glyphs in dialogue/HUD strings).
    const i32 dynamicTextY1 = 16;
    for (i32 y = 0; y < dynamicTextY1 && y < (i32)h; y++)
    {
        for (i32 x = 0; x < (i32)w; x++)
        {
            inRect[y * w + x] = 1;
        }
    }

    struct DecoyJob
    {
        i32 spriteIdx;
        i32 sx;
        i32 sy;
        i32 sw;
        i32 sh;
    };

    DecoyJob *jobs = new DecoyJob[anm->numSprites];
    i32 numJobs = 0;
    u32 *curSpriteOffset = anm->spriteOffsets;
    for (i32 s = 0; s < anm->numSprites; s++, curSpriteOffset++)
    {
        AnmRawSprite *rawSprite = (AnmRawSprite *)((u8 *)anm + *curSpriteOffset);
        f32 fx0 = rawSprite->offset.x;
        f32 fy0 = rawSprite->offset.y;
        f32 fw = rawSprite->size.x;
        f32 fh = rawSprite->size.y;
        if (fx0 != (f32)(i32)fx0 || fy0 != (f32)(i32)fy0 || fw != (f32)(i32)fw || fh != (f32)(i32)fh || fw <= 0.0f ||
            fh <= 0.0f)
        {
            continue;
        }
        i32 sx0 = (i32)fx0;
        i32 sy0 = (i32)fy0;
        i32 sw = (i32)fw;
        i32 sh = (i32)fh;
        if (sx0 < 0 || sy0 < 0 || sx0 + sw > (i32)w || sy0 + sh > (i32)h)
        {
            continue;
        }
        if (sy0 < dynamicTextY1)
        {
            continue;
        }
        for (i32 y = sy0; y < sy0 + sh; y++)
        {
            for (i32 x = sx0; x < sx0 + sw; x++)
            {
                inRect[y * w + x] = 1;
            }
        }
        jobs[numJobs].spriteIdx = (i32)(rawSprite->id + anm->spriteIdxOffset);
        jobs[numJobs].sx = sx0;
        jobs[numJobs].sy = sy0;
        jobs[numJobs].sw = sw;
        jobs[numJobs].sh = sh;
        numJobs++;
    }

    // Greedy allocation order: tallest / largest sprites first, so compact
    // requirements claim the limited tall free windows before the space
    // fragments into short bands.
    for (i32 i = 1; i < numJobs; i++)
    {
        DecoyJob key = jobs[i];
        i32 j = i - 1;
        while (j >= 0 && (jobs[j].sw * jobs[j].sh < key.sw * key.sh ||
                          (jobs[j].sw * jobs[j].sh == key.sw * key.sh && jobs[j].sh < key.sh)))
        {
            jobs[j + 1] = jobs[j];
            j--;
        }
        jobs[j + 1] = key;
    }

    // Per-column running counts of `used` texels inside the candidate window
    // rows, rebuilt as the window slides down the texture one row at a time.
    i32 iw = (i32)w;
    i32 ih = (i32)h;
    u32 *usedCol = new u32[iw];
    u32 *rectCol = new u32[iw];
    u32 *rectPrefix = new u32[iw + 1];

    for (i32 jobIdx = 0; jobIdx < numJobs; jobIdx++)
    {
        DecoyJob *job = &jobs[jobIdx];
        i32 bw = job->sw + 2;
        i32 bh = job->sh + 2;
        if (bw > iw || bh > ih)
        {
            continue;
        }

        i32 findX = -1;
        i32 findY = -1;
        for (i32 y = 0; y + bh <= ih && findX < 0; y++)
        {
            // used counts over rows y .. y+bh-1; rect counts over the expanded
            // span y-1 .. y+bh (for the neighbouring-rect check)
            if (y == 0)
            {
                for (i32 x = 0; x < iw; x++)
                {
                    u32 c = 0;
                    for (i32 r = 0; r < bh; r++)
                    {
                        c += used[r * iw + x];
                    }
                    usedCol[x] = c;
                    u32 rc = 0;
                    for (i32 r = 0; r <= bh && r < ih; r++)
                    {
                        rc += inRect[r * iw + x];
                    }
                    rectCol[x] = rc;
                }
            }
            else
            {
                for (i32 x = 0; x < iw; x++)
                {
                    usedCol[x] += used[(y + bh - 1) * iw + x];
                    usedCol[x] -= used[(y - 1) * iw + x];
                    // rect window for candidate y is rows y-1 .. y+bh; the
                    // window for y-1 was rows y-2 .. y-1+bh
                    i32 r0 = y - 2;
                    i32 r1 = y + bh;
                    if (r0 >= 0)
                    {
                        rectCol[x] -= inRect[r0 * iw + x];
                    }
                    if (r1 < ih)
                    {
                        rectCol[x] += inRect[r1 * iw + x];
                    }
                }
            }

            // prefix sums over inRect counts for the expanded (ring+1) block
            rectPrefix[0] = 0;
            for (i32 x = 0; x < iw; x++)
            {
                rectPrefix[x + 1] = rectPrefix[x] + rectCol[x];
            }

            i32 run = 0;
            for (i32 x = 0; x + bw <= iw && findX < 0; x++)
            {
                if (usedCol[x] == 0)
                {
                    run++;
                }
                else
                {
                    run = 0;
                    continue;
                }
                if (run < bw)
                {
                    continue;
                }
                // candidate left edge; the strict check requires the block and
                // one texel around it to contain no other sprite's rectangle
                i32 x0 = x - bw + 1;
                u32 rsum = rectPrefix[x0 + bw + 1 > iw ? iw : x0 + bw + 1];
                u32 rsub = rectPrefix[x0 - 1 < 0 ? 0 : x0 - 1];
                if (rsum - rsub == 0)
                {
                    findX = x0;
                    findY = y;
                }
            }
        }

        if (findX < 0)
        {
            continue;
        }

        i32 dx = findX;
        i32 dy = findY;

        // interior copy
        for (i32 r = 0; r < job->sh; r++)
        {
            u8 *d = px + ((dy + 1 + r) * iw + (dx + 1)) * 4;
            u8 *s = px + ((job->sy + r) * iw + job->sx) * 4;
            for (i32 c = 0; c < job->sw; c++)
            {
                d[c * 4 + 0] = s[c * 4 + 0];
                d[c * 4 + 1] = s[c * 4 + 1];
                d[c * 4 + 2] = s[c * 4 + 2];
                d[c * 4 + 3] = s[c * 4 + 3];
            }
        }

        // horizontal rings: copy of the top/bottom interior rows
        for (i32 c = 0; c < job->sw; c++)
        {
            u8 *top = px + (dy * iw + dx + 1 + c) * 4;
            u8 *firstRow = px + ((dy + 1) * iw + dx + 1 + c) * 4;
            u8 *lastRow = px + ((dy + job->sh) * iw + dx + 1 + c) * 4;
            top[0] = firstRow[0];
            top[1] = firstRow[1];
            top[2] = firstRow[2];
            top[3] = firstRow[3];

            u8 *bot = px + ((dy + bh - 1) * iw + dx + 1 + c) * 4;
            bot[0] = lastRow[0];
            bot[1] = lastRow[1];
            bot[2] = lastRow[2];
            bot[3] = lastRow[3];
        }

        // vertical rings: copy of the left/right interior columns
        for (i32 r = 0; r < job->sh; r++)
        {
            u8 *left = px + ((dy + 1 + r) * iw + dx) * 4;
            u8 *firstCol = px + ((dy + 1 + r) * iw + dx + 1) * 4;
            u8 *lastCol = px + ((dy + 1 + r) * iw + dx + job->sw) * 4;
            left[0] = firstCol[0];
            left[1] = firstCol[1];
            left[2] = firstCol[2];
            left[3] = firstCol[3];

            u8 *right = px + ((dy + 1 + r) * iw + dx + bw - 1) * 4;
            right[0] = lastCol[0];
            right[1] = lastCol[1];
            right[2] = lastCol[2];
            right[3] = lastCol[3];
        }

        // corners
        u8 *cTL = px + (dy * iw + dx) * 4;
        u8 *cTLsrc = px + ((dy + 1) * iw + dx + 1) * 4;
        u8 *cTR = px + (dy * iw + dx + bw - 1) * 4;
        u8 *cTRsrc = px + ((dy + 1) * iw + dx + job->sw) * 4;
        u8 *cBL = px + ((dy + bh - 1) * iw + dx) * 4;
        u8 *cBLsrc = px + ((dy + job->sh) * iw + dx + 1) * 4;
        u8 *cBR = px + ((dy + bh - 1) * iw + dx + bw - 1) * 4;
        u8 *cBRsrc = px + ((dy + job->sh) * iw + dx + job->sw) * 4;
        for (i32 c = 0; c < 4; c++)
        {
            cTL[c] = cTLsrc[c];
            cTR[c] = cTRsrc[c];
            cBL[c] = cBLsrc[c];
            cBR[c] = cBRsrc[c];
        }

        // mark the decoy (including its ring) as used for subsequent placements
        for (i32 r = -1; r <= job->sh; r++)
        {
            for (i32 c = -1; c <= job->sw; c++)
            {
                used[(dy + 1 + r) * iw + dx + 1 + c] = 1;
            }
        }

        // redirect the sprite rectangle to the decoy interior
        AnmLoadedSprite *sp = &this->sprites[job->spriteIdx];
        sp->startPixelInclusive.x = (f32)(dx + 1);
        sp->startPixelInclusive.y = (f32)(dy + 1);
        sp->endPixelInclusive.x = (f32)(dx + 1 + job->sw);
        sp->endPixelInclusive.y = (f32)(dy + 1 + job->sh);
        sp->uvStart.x = sp->startPixelInclusive.x / sp->textureWidth;
        sp->uvEnd.x = sp->endPixelInclusive.x / sp->textureWidth;
        sp->uvStart.y = sp->startPixelInclusive.y / sp->textureHeight;
        sp->uvEnd.y = sp->endPixelInclusive.y / sp->textureHeight;
    }

    delete[] used;
    delete[] inRect;
    delete[] usedCol;
    delete[] rectCol;
    delete[] rectPrefix;
    delete[] jobs;

    g_Renderer->UnlockTexture(tex);
}

#endif

void AnmManager::ReleaseAnm(i32 anmIdx)
{
    if (this->anmFiles[anmIdx] != NULL)
    {
        i32 *spriteIdx;
        i32 i;
        i32 spriteIdxOffset = this->anmFilesSpriteIndexOffsets[anmIdx];
        u32 *byteOffset = this->anmFiles[anmIdx]->spriteOffsets;
        for (i = 0; i < this->anmFiles[anmIdx]->numSprites; i++, byteOffset++)
        {
            spriteIdx = (i32 *)((u8 *)this->anmFiles[anmIdx] + *byteOffset);
            memset(&this->sprites[*spriteIdx + spriteIdxOffset], 0,
                   sizeof(this->sprites[*spriteIdx + spriteIdxOffset]));
            this->sprites[*spriteIdx + spriteIdxOffset].sourceFileIndex = -1;
        }

        for (i = 0; i < this->anmFiles[anmIdx]->numScripts; i++, byteOffset += 2)
        {
            this->scripts[*byteOffset + spriteIdxOffset] = NULL;
            this->spriteIndices[*byteOffset + spriteIdxOffset] = NULL;
        }
        this->anmFilesSpriteIndexOffsets[anmIdx] = NULL;
        AnmRawEntry *entry = this->anmFiles[anmIdx];
        this->ReleaseTexture(entry->textureIdx);
        AnmRawEntry *anmFilePtr = this->anmFiles[anmIdx];
        free(anmFilePtr);
        this->anmFiles[anmIdx] = 0;
        this->currentBlendMode = 0xff;
        this->currentColorOp = 0xff;
        this->currentVertexShader = 0xff;
        this->currentTexture = NULL;
    }
}

void AnmManager::ReleaseTexture(i32 textureIdx)
{
    if (this->textures[textureIdx] != NULL)
    {
#ifdef USE_VULKAN
        g_Renderer->ReleaseTexture((TextureHandle)(uintptr_t)this->textures[textureIdx]);
#else
        this->textures[textureIdx]->Release();
#endif
        this->textures[textureIdx] = NULL;
    }

    void *imageDataArray = this->imageDataArray[textureIdx];
    free(imageDataArray);

    this->imageDataArray[textureIdx] = NULL;
}

void AnmManager::LoadSprite(u32 spriteIdx, AnmLoadedSprite *sprite)
{
    this->sprites[spriteIdx] = *sprite;
    this->sprites[spriteIdx].spriteId = this->maybeLoadedSpriteCount++;

    this->sprites[spriteIdx].uvStart.x =
        this->sprites[spriteIdx].startPixelInclusive.x / (this->sprites[spriteIdx].textureWidth);
    this->sprites[spriteIdx].uvEnd.x =
        this->sprites[spriteIdx].endPixelInclusive.x / (this->sprites[spriteIdx].textureWidth);
    this->sprites[spriteIdx].uvStart.y =
        this->sprites[spriteIdx].startPixelInclusive.y / (this->sprites[spriteIdx].textureHeight);
    this->sprites[spriteIdx].uvEnd.y =
        this->sprites[spriteIdx].endPixelInclusive.y / (this->sprites[spriteIdx].textureHeight);

    this->sprites[spriteIdx].widthPx =
        this->sprites[spriteIdx].endPixelInclusive.x - this->sprites[spriteIdx].startPixelInclusive.x;
    this->sprites[spriteIdx].heightPx =
        this->sprites[spriteIdx].endPixelInclusive.y - this->sprites[spriteIdx].startPixelInclusive.y;
}

ZunResult AnmManager::SetActiveSprite(AnmVm *vm, u32 sprite_index)
{
    if (this->sprites[sprite_index].sourceFileIndex < 0)
    {
        return ZUN_ERROR;
    }

    vm->activeSpriteIndex = (i16)sprite_index;
    vm->sprite = this->sprites + sprite_index;
    D3DXMatrixIdentity(&vm->matrix);
    vm->matrix.m[0][0] = vm->sprite->widthPx / vm->sprite->textureWidth;
    vm->matrix.m[1][1] = vm->sprite->heightPx / vm->sprite->textureHeight;

    return ZUN_SUCCESS;
}

void AnmManager::SetAndExecuteScript(AnmVm *vm, AnmRawInstr *beginingOfScript)
{
    ZunTimer *timer;

    vm->flags.flip = 0;
    vm->Initialize();
    vm->beginingOfScript = beginingOfScript;
    vm->currentInstruction = vm->beginingOfScript;

    timer = &(vm->currentTimeInScript);
    timer->current = 0;
    timer->subFrame = 0.0;
    timer->previous = -999;

    vm->flags.isVisible = 0;
    if (beginingOfScript)
    {
        this->ExecuteScript(vm);
    }
}

void AnmManager::SetRenderStateForVm(AnmVm *vm)
{
    if (this->currentBlendMode != vm->flags.blendMode)
    {
        this->currentBlendMode = vm->flags.blendMode;
        if (this->currentBlendMode == AnmVmBlendMode_InvSrcAlpha)
        {
            g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        }
        else
        {
            g_Supervisor.d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
        }
    }
    if ((((g_Supervisor.cfg.opts >> GCOS_USE_D3D_HW_TEXTURE_BLENDING) & 1) == 0) &&
        (((g_Supervisor.cfg.opts >> GCOS_NO_COLOR_COMP) & 1) == 0) && (this->currentColorOp != vm->flags.colorOp))
    {
        this->currentColorOp = vm->flags.colorOp;
        if (this->currentColorOp == AnmVmColorOp_Modulate)
        {
            g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        }
        else
        {
            g_Supervisor.d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_ADD);
        }
    }
    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        if (this->currentTextureFactor != vm->color)
        {
            this->currentTextureFactor = vm->color;
            g_Supervisor.d3dDevice->SetRenderState(D3DRS_TEXTUREFACTOR, this->currentTextureFactor);
        }
    }
    else
    {
        g_PrimitivesToDrawNoVertexBuf[0].diffuse = vm->color;
        g_PrimitivesToDrawNoVertexBuf[1].diffuse = vm->color;
        g_PrimitivesToDrawNoVertexBuf[2].diffuse = vm->color;
        g_PrimitivesToDrawNoVertexBuf[3].diffuse = vm->color;
        g_PrimitivesToDrawUnknown[0].diffuse = vm->color;
        g_PrimitivesToDrawUnknown[1].diffuse = vm->color;
        g_PrimitivesToDrawUnknown[2].diffuse = vm->color;
        g_PrimitivesToDrawUnknown[3].diffuse = vm->color;
    }
    if ((((g_Supervisor.cfg.opts >> GCOS_TURN_OFF_DEPTH_TEST) & 1) == 0) &&
        (this->currentZWriteDisable != vm->flags.zWriteDisable))
    {
        this->currentZWriteDisable = vm->flags.zWriteDisable;
        if (this->currentZWriteDisable == 0)
        {
            g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, 1);
        }
        else
        {
            g_Supervisor.d3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, 0);
        }
    }
    return;
}

static f32 g_ZeroPointFive = 0.5;

ZunResult AnmManager::DrawInner(AnmVm *vm, i32 param_3)
{
    if (param_3 != 0)
    {

#ifdef __GNUC__

        f32 x0 = rintf(g_PrimitivesToDrawVertexBuf[0].position.x) - g_ZeroPointFive;
        f32 x1 = rintf(g_PrimitivesToDrawVertexBuf[1].position.x) - g_ZeroPointFive;
        f32 y0 = rintf(g_PrimitivesToDrawVertexBuf[0].position.y) - g_ZeroPointFive;
        f32 y2 = rintf(g_PrimitivesToDrawVertexBuf[2].position.y) - g_ZeroPointFive;
        g_PrimitivesToDrawVertexBuf[2].position.y = y2;
        g_PrimitivesToDrawVertexBuf[3].position.y = y2;
        g_PrimitivesToDrawVertexBuf[0].position.y = y0;
        g_PrimitivesToDrawVertexBuf[1].position.y = y0;
        g_PrimitivesToDrawVertexBuf[1].position.x = x1;
        g_PrimitivesToDrawVertexBuf[3].position.x = x1;
        g_PrimitivesToDrawVertexBuf[0].position.x = x0;
        g_PrimitivesToDrawVertexBuf[2].position.x = x0;
#else
        __asm {
            fld g_PrimitivesToDrawVertexBuf[0 * TYPE g_PrimitivesToDrawVertexBuf].position.x
            frndint
            fsub g_ZeroPointFive
            fld g_PrimitivesToDrawVertexBuf[1 * TYPE g_PrimitivesToDrawVertexBuf].position.x
            frndint
            fsub g_ZeroPointFive
            fld g_PrimitivesToDrawVertexBuf[0 * TYPE g_PrimitivesToDrawVertexBuf].position.y
            frndint
            fsub g_ZeroPointFive
            fld g_PrimitivesToDrawVertexBuf[2 * TYPE g_PrimitivesToDrawVertexBuf].position.y
            frndint
            fsub g_ZeroPointFive
            fst g_PrimitivesToDrawVertexBuf[2 * TYPE g_PrimitivesToDrawVertexBuf].position.y
            fstp g_PrimitivesToDrawVertexBuf[3 * TYPE g_PrimitivesToDrawVertexBuf].position.y
            fst g_PrimitivesToDrawVertexBuf[0 * TYPE g_PrimitivesToDrawVertexBuf].position.y
            fstp g_PrimitivesToDrawVertexBuf[1 * TYPE g_PrimitivesToDrawVertexBuf].position.y
            fst g_PrimitivesToDrawVertexBuf[1 * TYPE g_PrimitivesToDrawVertexBuf].position.x
            fstp g_PrimitivesToDrawVertexBuf[3 * TYPE g_PrimitivesToDrawVertexBuf].position.x
            fst g_PrimitivesToDrawVertexBuf[0 * TYPE g_PrimitivesToDrawVertexBuf].position.x
            fstp g_PrimitivesToDrawVertexBuf[2 * TYPE g_PrimitivesToDrawVertexBuf].position.x
        }
#endif
    }
    g_PrimitivesToDrawVertexBuf[0].position.z = g_PrimitivesToDrawVertexBuf[1].position.z =
        g_PrimitivesToDrawVertexBuf[2].position.z = g_PrimitivesToDrawVertexBuf[3].position.z = vm->pos.z;
    if (this->currentSprite != vm->sprite)
    {
        this->currentSprite = vm->sprite;
        g_PrimitivesToDrawVertexBuf[0].textureUV.x = g_PrimitivesToDrawVertexBuf[2].textureUV.x =
            vm->sprite->uvStart.x + vm->uvScrollPos.x;
        g_PrimitivesToDrawVertexBuf[1].textureUV.x = g_PrimitivesToDrawVertexBuf[3].textureUV.x =
            vm->sprite->uvEnd.x + vm->uvScrollPos.x;
        g_PrimitivesToDrawVertexBuf[0].textureUV.y = g_PrimitivesToDrawVertexBuf[1].textureUV.y =
            vm->sprite->uvStart.y + vm->uvScrollPos.y;
        g_PrimitivesToDrawVertexBuf[2].textureUV.y = g_PrimitivesToDrawVertexBuf[3].textureUV.y =
            vm->sprite->uvEnd.y + vm->uvScrollPos.y;
        if (this->currentTexture != this->textures[vm->sprite->sourceFileIndex])
        {
            this->currentTexture = this->textures[vm->sprite->sourceFileIndex];
            g_Supervisor.d3dDevice->SetTexture(0, this->currentTexture);
        }
    }
    if (this->currentVertexShader != 2)
    {
        if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
        {
            g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_TEX1 | D3DFVF_XYZRHW);
        }
        else
        {
            g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_XYZRHW);
        }
        this->currentVertexShader = 2;
    }
#ifdef USE_VULKAN
    Vertex2D v2d[4];
    for (int i = 0; i < 4; i++) {
        v2d[i].x = g_PrimitivesToDrawVertexBuf[i].position.x;
        v2d[i].y = g_PrimitivesToDrawVertexBuf[i].position.y;
        v2d[i].u = g_PrimitivesToDrawVertexBuf[i].textureUV.x;
        v2d[i].v = g_PrimitivesToDrawVertexBuf[i].textureUV.y;
        v2d[i].color = vm->color;
    }
    BlendMode blend = (vm->flags.blendMode == AnmVmBlendMode_InvSrcAlpha) ? BLEND_ALPHA : BLEND_ADD;
    TextureHandle tex = (TextureHandle)(uintptr_t)this->textures[vm->sprite->sourceFileIndex];
    {
        ptrdiff_t spi = vm->sprite - this->sprites;
        if (spi >= 108 && spi <= 118)
        {
        }
    }
    g_Renderer->DrawQuad2D(tex, v2d, blend);
    return ZUN_SUCCESS;
#else
    this->SetRenderStateForVm(vm);
    if (((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF) & 1) == 0)
    {
        g_Supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, g_PrimitivesToDrawVertexBuf, 0x18);
    }
    else
    {
        g_PrimitivesToDrawNoVertexBuf[0].position.x = g_PrimitivesToDrawVertexBuf[0].position.x;
        g_PrimitivesToDrawNoVertexBuf[0].position.y = g_PrimitivesToDrawVertexBuf[0].position.y;
        g_PrimitivesToDrawNoVertexBuf[0].position.z = g_PrimitivesToDrawVertexBuf[0].position.z;
        g_PrimitivesToDrawNoVertexBuf[1].position.x = g_PrimitivesToDrawVertexBuf[1].position.x;
        g_PrimitivesToDrawNoVertexBuf[1].position.y = g_PrimitivesToDrawVertexBuf[1].position.y;
        g_PrimitivesToDrawNoVertexBuf[1].position.z = g_PrimitivesToDrawVertexBuf[1].position.z;
        g_PrimitivesToDrawNoVertexBuf[2].position.x = g_PrimitivesToDrawVertexBuf[2].position.x;
        g_PrimitivesToDrawNoVertexBuf[2].position.y = g_PrimitivesToDrawVertexBuf[2].position.y;
        g_PrimitivesToDrawNoVertexBuf[2].position.z = g_PrimitivesToDrawVertexBuf[2].position.z;
        g_PrimitivesToDrawNoVertexBuf[3].position.x = g_PrimitivesToDrawVertexBuf[3].position.x;
        g_PrimitivesToDrawNoVertexBuf[3].position.y = g_PrimitivesToDrawVertexBuf[3].position.y;
        g_PrimitivesToDrawNoVertexBuf[3].position.z = g_PrimitivesToDrawVertexBuf[3].position.z;
        g_PrimitivesToDrawNoVertexBuf[0].textureUV.x = g_PrimitivesToDrawNoVertexBuf[2].textureUV.x =
            vm->sprite->uvStart.x + vm->uvScrollPos.x;
        g_PrimitivesToDrawNoVertexBuf[1].textureUV.x = g_PrimitivesToDrawNoVertexBuf[3].textureUV.x =
            vm->sprite->uvEnd.x + vm->uvScrollPos.x;
        g_PrimitivesToDrawNoVertexBuf[0].textureUV.y = g_PrimitivesToDrawNoVertexBuf[1].textureUV.y =
            vm->sprite->uvStart.y + vm->uvScrollPos.y;
        g_PrimitivesToDrawNoVertexBuf[2].textureUV.y = g_PrimitivesToDrawNoVertexBuf[3].textureUV.y =
            vm->sprite->uvEnd.y + vm->uvScrollPos.y;
        g_Supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, g_PrimitivesToDrawNoVertexBuf, 0x1c);
    }
    return ZUN_SUCCESS;
#endif
}

ZunResult AnmManager::DrawNoRotation(AnmVm *vm)
{
    float fVar2;
    float fVar3;

    if (vm->flags.isVisible == 0)
    {
        return ZUN_ERROR;
    }
    if (vm->flags.flag1 == 0)
    {
        return ZUN_ERROR;
    }
    if (vm->color == 0)
    {
        return ZUN_ERROR;
    }
    fVar2 = (vm->sprite->widthPx * vm->scaleX) / 2.0f;
    fVar3 = (vm->sprite->heightPx * vm->scaleY) / 2.0f;
    if ((vm->flags.anchor & AnmVmAnchor_Left) == 0)
    {
        g_PrimitivesToDrawVertexBuf[0].position.x = g_PrimitivesToDrawVertexBuf[2].position.x = vm->pos.x - fVar2;
        g_PrimitivesToDrawVertexBuf[1].position.x = g_PrimitivesToDrawVertexBuf[3].position.x = fVar2 + vm->pos.x;
    }
    else
    {
        g_PrimitivesToDrawVertexBuf[0].position.x = g_PrimitivesToDrawVertexBuf[2].position.x = vm->pos.x;
        g_PrimitivesToDrawVertexBuf[1].position.x = g_PrimitivesToDrawVertexBuf[3].position.x =
            fVar2 + vm->pos.x + fVar2;
    }
    if ((vm->flags.anchor & AnmVmAnchor_Top) == 0)
    {
        g_PrimitivesToDrawVertexBuf[0].position.y = g_PrimitivesToDrawVertexBuf[1].position.y = vm->pos.y - fVar3;
        g_PrimitivesToDrawVertexBuf[2].position.y = g_PrimitivesToDrawVertexBuf[3].position.y = fVar3 + vm->pos.y;
    }
    else
    {
        g_PrimitivesToDrawVertexBuf[0].position.y = g_PrimitivesToDrawVertexBuf[1].position.y = vm->pos.y;
        g_PrimitivesToDrawVertexBuf[2].position.y = g_PrimitivesToDrawVertexBuf[3].position.y =
            fVar3 + vm->pos.y + fVar3;
    }
    return this->DrawInner(vm, 1);
}

void AnmManager::TranslateRotation(VertexTex1Xyzrwh *param_1, f32 x, f32 y, f32 sine, f32 cosine, f32 xOffset,
                                   f32 yOffset)
{
    param_1->position.x = x * cosine + y * sine + xOffset;
    param_1->position.y = -x * sine + y * cosine + yOffset;
    return;
}

ZunResult AnmManager::Draw(AnmVm *vm)
{
    f32 zSine;
    f32 zCosine;
    f32 spriteXCenter;
    f32 spriteYCenter;
    f32 xOffset;
    f32 yOffset;
    f32 z;

    if (vm->rotation.z == 0.0f)
    {
        return this->DrawNoRotation(vm);
    }
    if (vm->flags.isVisible == 0)
    {
        return ZUN_ERROR;
    }
    if (vm->flags.flag1 == 0)
    {
        return ZUN_ERROR;
    }
    if (vm->color == 0)
    {
        return ZUN_ERROR;
    }
    z = vm->rotation.z;
    sincos(z, zSine, zCosine);
    xOffset = rintf(vm->pos.x);
    yOffset = rintf(vm->pos.y);
    spriteXCenter = rintf((vm->sprite->widthPx * vm->scaleX) / 2.0f);
    spriteYCenter = rintf((vm->sprite->heightPx * vm->scaleY) / 2.0f);
    this->TranslateRotation(&g_PrimitivesToDrawVertexBuf[0], -spriteXCenter - 0.5f, -spriteYCenter - 0.5f, zSine,
                            zCosine, xOffset, yOffset);
    this->TranslateRotation(&g_PrimitivesToDrawVertexBuf[1], spriteXCenter - 0.5f, -spriteYCenter - 0.5f, zSine,
                            zCosine, xOffset, yOffset);
    this->TranslateRotation(&g_PrimitivesToDrawVertexBuf[2], -spriteXCenter - 0.5f, spriteYCenter - 0.5f, zSine,
                            zCosine, xOffset, yOffset);
    this->TranslateRotation(&g_PrimitivesToDrawVertexBuf[3], spriteXCenter - 0.5f, spriteYCenter - 0.5f, zSine, zCosine,
                            xOffset, yOffset);
    g_PrimitivesToDrawVertexBuf[0].position.z = g_PrimitivesToDrawVertexBuf[1].position.z =
        g_PrimitivesToDrawVertexBuf[2].position.z = g_PrimitivesToDrawVertexBuf[3].position.z = vm->pos.z;
    if ((vm->flags.anchor & AnmVmAnchor_Left) != 0)
    {
        g_PrimitivesToDrawVertexBuf[0].position.x += spriteXCenter;
        g_PrimitivesToDrawVertexBuf[1].position.x += spriteXCenter;
        g_PrimitivesToDrawVertexBuf[2].position.x += spriteXCenter;
        g_PrimitivesToDrawVertexBuf[3].position.x += spriteXCenter;
    }
    if ((vm->flags.anchor & AnmVmAnchor_Top) != 0)
    {
        g_PrimitivesToDrawVertexBuf[0].position.y += spriteYCenter;
        g_PrimitivesToDrawVertexBuf[1].position.y += spriteYCenter;
        g_PrimitivesToDrawVertexBuf[2].position.y += spriteYCenter;
        g_PrimitivesToDrawVertexBuf[3].position.y += spriteYCenter;
    }
    return this->DrawInner(vm, 0);
}

ZunResult AnmManager::DrawFacingCamera(AnmVm *vm)
{
    f32 centerX;
    f32 centerY;

    if (!vm->flags.isVisible)
    {
        return ZUN_ERROR;
    }
    if (!vm->flags.flag1)
    {
        return ZUN_ERROR;
    }
    if (vm->color == 0)
    {
        return ZUN_ERROR;
    }

    centerX = vm->sprite->widthPx * vm->scaleX / 2.0f;
    centerY = vm->sprite->heightPx * vm->scaleY / 2.0f;
    if ((vm->flags.anchor & AnmVmAnchor_Left) == 0)
    {
        g_PrimitivesToDrawVertexBuf[0].position.x = g_PrimitivesToDrawVertexBuf[2].position.x = vm->pos.x - centerX;
        g_PrimitivesToDrawVertexBuf[1].position.x = g_PrimitivesToDrawVertexBuf[3].position.x = vm->pos.x + centerX;
    }
    else
    {
        g_PrimitivesToDrawVertexBuf[0].position.x = g_PrimitivesToDrawVertexBuf[2].position.x = vm->pos.x;
        g_PrimitivesToDrawVertexBuf[1].position.x = g_PrimitivesToDrawVertexBuf[3].position.x =
            vm->pos.x + centerX + centerX;
    }
    if ((vm->flags.anchor & AnmVmAnchor_Top) == 0)
    {
        g_PrimitivesToDrawVertexBuf[0].position.y = g_PrimitivesToDrawVertexBuf[1].position.y = vm->pos.y - centerY;
        g_PrimitivesToDrawVertexBuf[2].position.y = g_PrimitivesToDrawVertexBuf[3].position.y = vm->pos.y + centerY;
    }
    else
    {
        g_PrimitivesToDrawVertexBuf[0].position.y = g_PrimitivesToDrawVertexBuf[1].position.y = vm->pos.y;
        g_PrimitivesToDrawVertexBuf[2].position.y = g_PrimitivesToDrawVertexBuf[3].position.y =
            vm->pos.y + centerY + centerY;
    }
    return this->DrawInner(vm, 0);
}

ZunResult AnmManager::Draw3(AnmVm *vm)
{
    D3DXMATRIX worldTransformMatrix;
    D3DXMATRIX rotationMatrix;
    D3DXMATRIX textureMatrix;
    f32 scaledXCenter;
    f32 scaledYCenter;

    if (!vm->flags.isVisible)
    {
        return ZUN_ERROR;
    }
    if (!vm->flags.flag1)
    {
        return ZUN_ERROR;
    }
    if (vm->color == 0)
    {
        return ZUN_ERROR;
    }

    worldTransformMatrix = vm->matrix;
    worldTransformMatrix.m[0][0] *= vm->scaleX;
    worldTransformMatrix.m[1][1] *= -vm->scaleY;

    if (vm->rotation.x != 0.0)
    {
        D3DXMatrixRotationX(&rotationMatrix, vm->rotation.x);
        D3DXMatrixMultiply(&worldTransformMatrix, &worldTransformMatrix, &rotationMatrix);
    }

    if (vm->rotation.y != 0.0)
    {
        D3DXMatrixRotationY(&rotationMatrix, vm->rotation.y);
        D3DXMatrixMultiply(&worldTransformMatrix, &worldTransformMatrix, &rotationMatrix);
    }

    if (vm->rotation.z != 0.0)
    {
        D3DXMatrixRotationZ(&rotationMatrix, vm->rotation.z);
        D3DXMatrixMultiply(&worldTransformMatrix, &worldTransformMatrix, &rotationMatrix);
    }

    if ((vm->flags.anchor & AnmVmAnchor_Left) == 0)
    {
        worldTransformMatrix.m[3][0] = vm->pos.x;
    }
    else
    {
        scaledXCenter = vm->sprite->widthPx * vm->scaleX / 2.0f;
        worldTransformMatrix.m[3][0] = fabsf(scaledXCenter) + vm->pos.x;
    }

    if ((vm->flags.anchor & AnmVmAnchor_Top) == 0)
    {
        worldTransformMatrix.m[3][1] = -vm->pos.y;
    }
    else
    {
        scaledYCenter = vm->sprite->heightPx * vm->scaleY / 2.0f;
        worldTransformMatrix.m[3][1] = -vm->pos.y - fabsf(scaledYCenter);
    }

    worldTransformMatrix.m[3][2] = vm->pos.z;

    g_Supervisor.d3dDevice->SetTransform(D3DTS_WORLD, &worldTransformMatrix);

    if (this->currentSprite != vm->sprite)
    {
        this->currentSprite = vm->sprite;
        textureMatrix = vm->matrix;
        textureMatrix.m[2][0] = vm->sprite->uvStart.x + vm->uvScrollPos.x;
        textureMatrix.m[2][1] = vm->sprite->uvStart.y + vm->uvScrollPos.y;
        g_Supervisor.d3dDevice->SetTransform(D3DTS_TEXTURE0, &textureMatrix);
        if (this->currentTexture != this->textures[vm->sprite->sourceFileIndex])
        {
            this->currentTexture = this->textures[vm->sprite->sourceFileIndex];
            g_Supervisor.d3dDevice->SetTexture(0, this->currentTexture);
        }
    }

    if (this->currentVertexShader != 3)
    {
        if ((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF & 1) == 0)
        {
            g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_TEX1 | D3DFVF_XYZ);
            g_Supervisor.d3dDevice->SetStreamSource(0, this->vertexBuffer, 0x14);
        }
        else
        {
            g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_XYZ);
        }
        this->currentVertexShader = 3;
    }

#ifdef USE_VULKAN
    Vertex3D v3d[4];
    v3d[0] = { -128.0f, -128.0f, 0.0f, vm->sprite->uvStart.x + vm->uvScrollPos.x, vm->sprite->uvStart.y + vm->uvScrollPos.y, vm->color };
    v3d[1] = {  128.0f, -128.0f, 0.0f, vm->sprite->uvEnd.x   + vm->uvScrollPos.x, vm->sprite->uvStart.y + vm->uvScrollPos.y, vm->color };
    v3d[2] = { -128.0f,  128.0f, 0.0f, vm->sprite->uvStart.x + vm->uvScrollPos.x, vm->sprite->uvEnd.y   + vm->uvScrollPos.y, vm->color };
    v3d[3] = {  128.0f,  128.0f, 0.0f, vm->sprite->uvEnd.x   + vm->uvScrollPos.x, vm->sprite->uvEnd.y   + vm->uvScrollPos.y, vm->color };

    BlendMode blend = (vm->flags.blendMode == AnmVmBlendMode_InvSrcAlpha) ? BLEND_ALPHA : BLEND_ADD;
    TextureHandle tex = (TextureHandle)(uintptr_t)this->textures[vm->sprite->sourceFileIndex];
    g_Renderer->DrawQuad3D((const float*)&worldTransformMatrix, tex, v3d, blend);
    return ZUN_SUCCESS;
#else
    this->SetRenderStateForVm(vm);

    if ((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF & 1) == 0)
    {
        g_Supervisor.d3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
    }
    else
    {
        g_Supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, g_PrimitivesToDrawUnknown, 0x18);
    }
    return ZUN_SUCCESS;
#endif
}

ZunResult AnmManager::Draw2(AnmVm *vm)
{
    D3DXMATRIX worldTransformMatrix;
    D3DXMATRIX unusedMatrix;
    D3DXMATRIX textureMatrix;

    if (!vm->flags.isVisible)
    {
        return ZUN_ERROR;
    }
    if (!vm->flags.flag1)
    {
        return ZUN_ERROR;
    }

    if (vm->rotation.x != 0 || vm->rotation.y != 0 || vm->rotation.z != 0)
    {
        return this->Draw3(vm);
    }

    if (vm->color == 0)
    {
        return ZUN_ERROR;
    }

    worldTransformMatrix = vm->matrix;
    worldTransformMatrix.m[3][0] = rintf(vm->pos.x) - 0.5f;
    worldTransformMatrix.m[3][1] = -rintf(vm->pos.y) + 0.5f;
    if ((vm->flags.anchor & AnmVmAnchor_Left) != 0)
    {
        worldTransformMatrix.m[3][0] += (vm->sprite->widthPx * vm->scaleX) / 2.0f;
    }
    if ((vm->flags.anchor & AnmVmAnchor_Top) != 0)
    {
        worldTransformMatrix.m[3][1] -= (vm->sprite->heightPx * vm->scaleY) / 2.0f;
    }
    worldTransformMatrix.m[3][2] = vm->pos.z;
    worldTransformMatrix.m[0][0] *= vm->scaleX;
    worldTransformMatrix.m[1][1] *= -vm->scaleY;
    g_Supervisor.d3dDevice->SetTransform(D3DTS_WORLD, &worldTransformMatrix);

    if (this->currentSprite != vm->sprite)
    {
        this->currentSprite = vm->sprite;
        textureMatrix = vm->matrix;
        textureMatrix.m[2][0] = vm->sprite->uvStart.x + vm->uvScrollPos.x;
        textureMatrix.m[2][1] = vm->sprite->uvStart.y + vm->uvScrollPos.y;
        g_Supervisor.d3dDevice->SetTransform(D3DTS_TEXTURE0, &textureMatrix);
        if (this->currentTexture != this->textures[vm->sprite->sourceFileIndex])
        {
            this->currentTexture = this->textures[vm->sprite->sourceFileIndex];
            g_Supervisor.d3dDevice->SetTexture(0, this->currentTexture);
        }
        if (this->currentVertexShader != 3)
        {
            if ((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF & 1) == 0)
            {
                g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_TEX1 | D3DFVF_XYZ);
                g_Supervisor.d3dDevice->SetStreamSource(0, this->vertexBuffer, 0x14);
            }
            else
            {
                g_Supervisor.d3dDevice->SetVertexShader(D3DFVF_TEX1 | D3DFVF_DIFFUSE | D3DFVF_XYZ);
            }
            this->currentVertexShader = 3;
        }
    }
#ifdef USE_VULKAN
    Vertex3D v3d[4];
    v3d[0] = { -128.0f, -128.0f, 0.0f, vm->sprite->uvStart.x + vm->uvScrollPos.x, vm->sprite->uvStart.y + vm->uvScrollPos.y, vm->color };
    v3d[1] = {  128.0f, -128.0f, 0.0f, vm->sprite->uvEnd.x   + vm->uvScrollPos.x, vm->sprite->uvStart.y + vm->uvScrollPos.y, vm->color };
    v3d[2] = { -128.0f,  128.0f, 0.0f, vm->sprite->uvStart.x + vm->uvScrollPos.x, vm->sprite->uvEnd.y   + vm->uvScrollPos.y, vm->color };
    v3d[3] = {  128.0f,  128.0f, 0.0f, vm->sprite->uvEnd.x   + vm->uvScrollPos.x, vm->sprite->uvEnd.y   + vm->uvScrollPos.y, vm->color };

    BlendMode blend = (vm->flags.blendMode == AnmVmBlendMode_InvSrcAlpha) ? BLEND_ALPHA : BLEND_ADD;
    TextureHandle tex = (TextureHandle)(uintptr_t)this->textures[vm->sprite->sourceFileIndex];
    g_Renderer->DrawQuad3D((const float*)&worldTransformMatrix, tex, v3d, blend);
    return ZUN_SUCCESS;
#else
    this->SetRenderStateForVm(vm);
    if ((g_Supervisor.cfg.opts >> GCOS_DONT_USE_VERTEX_BUF & 1) == 0)
    {
        g_Supervisor.d3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
    }
    else
    {
        g_Supervisor.d3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, g_PrimitivesToDrawUnknown, 0x18);
    }
    return ZUN_SUCCESS;
#endif
}

i32 AnmManager::ExecuteScript(AnmVm *vm)
{
    AnmRawInstr *curInstr;
    u32 *local_c;
    f32 *local_10;
    f32 *local_14;
    f32 *local_18;
    f32 *local_1c;
    u32 *local_20;
    AnmRawInstr *nextInstr;
    ZunColor local_28;
    ZunColor local_2c;
    f32 local_30;
    i32 local_34;
    i32 local_38;
    f32 local_3c;

    if (vm->currentInstruction == NULL)
    {
        return 1;
    }

    if (vm->pendingInterrupt != 0)
    {
        goto handle_interrupt;
    }

    while (curInstr = vm->currentInstruction, curInstr->time <= vm->currentTimeInScript.AsFrames())
    {
        switch (curInstr->opcode)
        {
        case AnmOpcode_Exit:
            vm->flags.isVisible = 0;
        case AnmOpcode_ExitHide:
            vm->currentInstruction = NULL;
            return 1;
        case AnmOpcode_SetActiveSprite:
            vm->flags.isVisible = 1;
            this->SetActiveSprite(vm, curInstr->args[0] + this->spriteIndices[vm->anmFileIndex]);
            vm->timeOfLastSpriteSet = vm->currentTimeInScript.AsFrames();
            break;
        case AnmOpcode_SetRandomSprite:
            vm->flags.isVisible = 1;
            local_c = &curInstr->args[0];
            this->SetActiveSprite(vm, local_c[0] + g_Rng.GetRandomU16InRange(local_c[1]) +
                                          this->spriteIndices[vm->anmFileIndex]);
            vm->timeOfLastSpriteSet = vm->currentTimeInScript.AsFrames();
            break;
        case AnmOpcode_SetScale:
            vm->scaleX = *(f32 *)&curInstr->args[0];
            vm->scaleY = *(f32 *)&curInstr->args[1];
            break;
        case AnmOpcode_SetAlpha:
            COLOR_SET_COMPONENT(vm->color, COLOR_ALPHA_BYTE_IDX, curInstr->args[0] & 0xff);
            break;
        case AnmOpcode_SetColor:
            vm->color = COLOR_COMBINE_ALPHA(curInstr->args[0], vm->color);
            break;
        case AnmOpcode_Jump:
            vm->currentInstruction = (AnmRawInstr *)((i32)vm->beginingOfScript->args + curInstr->args[0] - 4);
            vm->currentTimeInScript.current = vm->currentInstruction->time;
            continue;
        case AnmOpcode_FlipX:
            vm->flags.flip ^= 1;
            vm->scaleX *= -1.f;
            break;
        case AnmOpcode_UsePosOffset:
            vm->flags.usePosOffset = curInstr->args[0];
            break;
        case AnmOpcode_FlipY:
            vm->flags.flip ^= 2;
            vm->scaleY *= -1.f;
            break;
        case AnmOpcode_SetRotation:
            local_10 = (f32 *)&curInstr->args[0];
            vm->rotation.x = *local_10++;
            vm->rotation.y = *local_10++;
            vm->rotation.z = *local_10;
            break;
        case AnmOpcode_SetAngleVel:
            local_14 = (f32 *)&curInstr->args[0];
            vm->angleVel.x = *local_14++;
            vm->angleVel.y = *local_14++;
            vm->angleVel.z = *local_14;
            break;
        case AnmOpcode_SetScaleSpeed:
            local_18 = (f32 *)&curInstr->args[0];
            vm->scaleInterpFinalX = *local_18++;
            vm->scaleInterpFinalY = *local_18;
            vm->scaleInterpEndTime = 0;
            break;
        case AnmOpcode_ScaleTime:
            local_1c = (f32 *)&curInstr->args[0];
            vm->scaleInterpFinalX = *local_1c++;
            vm->scaleInterpFinalY = *local_1c++;
            vm->scaleInterpEndTime = *(u16 *)local_1c;
            vm->scaleInterpTime.InitializeForPopup();
            vm->scaleInterpInitialX = vm->scaleX;
            vm->scaleInterpInitialY = vm->scaleY;
            break;
        case AnmOpcode_Fade:
            local_20 = (u32 *)&curInstr->args[0];
            vm->alphaInterpInitial = vm->color;
            vm->alphaInterpFinal = COLOR_SET_ALPHA2(vm->color, local_20[0]);
            vm->alphaInterpEndTime = local_20[1];
            vm->alphaInterpTime.InitializeForPopup();
            break;
        case AnmOpcode_SetBlendAdditive:
            vm->flags.blendMode = AnmVmBlendMode_One;
            break;
        case AnmOpcode_SetBlendDefault:
            vm->flags.blendMode = AnmVmBlendMode_InvSrcAlpha;
            break;
        case AnmOpcode_SetPosition:
            if (vm->flags.usePosOffset == 0)
            {
                vm->pos =
                    D3DXVECTOR3(*(f32 *)&curInstr->args[0], *(f32 *)&curInstr->args[1], *(f32 *)&curInstr->args[2]);
            }
            else
            {
                vm->posOffset =
                    D3DXVECTOR3(*(f32 *)&curInstr->args[0], *(f32 *)&curInstr->args[1], *(f32 *)&curInstr->args[2]);
            }
            break;
        case AnmOpcode_PosTimeAccel:
            vm->flags.posTime = 2;
            goto PosTimeDoStuff;
        case AnmOpcode_PosTimeDecel:
            vm->flags.posTime = 1;
            goto PosTimeDoStuff;
        case AnmOpcode_PosTimeLinear:
            vm->flags.posTime = 0;
        PosTimeDoStuff:
            if (vm->flags.usePosOffset == 0)
            {
                memcpy(vm->posInterpInitial, vm->pos, sizeof(D3DXVECTOR3));
            }
            else
            {
                memcpy(vm->posInterpInitial, vm->posOffset, sizeof(D3DXVECTOR3));
            }
            vm->posInterpFinal =
                D3DXVECTOR3(*(f32 *)&curInstr->args[0], *(f32 *)&curInstr->args[1], *(f32 *)&curInstr->args[2]);
            vm->posInterpEndTime = curInstr->args[3];
            vm->posInterpTime.InitializeForPopup();
            break;
        case AnmOpcode_StopHide:
            vm->flags.isVisible = 0;
        case AnmOpcode_Stop:
            if (vm->pendingInterrupt == 0)
            {
                vm->flags.isStopped = 1;
                vm->currentTimeInScript.Decrement(1);
                goto stop;
            }
        handle_interrupt:
            nextInstr = NULL;
            curInstr = vm->beginingOfScript;
            while ((curInstr->opcode != AnmOpcode_InterruptLabel || vm->pendingInterrupt != curInstr->args[0]) &&
                   curInstr->opcode != AnmOpcode_Exit && curInstr->opcode != AnmOpcode_ExitHide)
            {
                if (curInstr->opcode == AnmOpcode_InterruptLabel && curInstr->args[0] == 0xffffffff)
                {
                    nextInstr = curInstr;
                }
                curInstr = (AnmRawInstr *)((i32)curInstr->args + curInstr->argsCount);
            }

            vm->pendingInterrupt = 0;
            vm->flags.isStopped = 0;
            if (curInstr->opcode != AnmOpcode_InterruptLabel)
            {
                if (nextInstr == NULL)
                {
                    vm->currentTimeInScript.Decrement(1);
                    goto stop;
                }
                curInstr = nextInstr;
            }

            curInstr = (AnmRawInstr *)((i32)curInstr->args + curInstr->argsCount);
            vm->currentInstruction = curInstr;
            vm->currentTimeInScript.SetCurrent(vm->currentInstruction->time);
            vm->flags.isVisible = 1;
            continue;
        case AnmOpcode_SetVisibility:
            vm->flags.isVisible = curInstr->args[0];
            break;
        case AnmOpcode_AnchorTopLeft:
            vm->flags.anchor = AnmVmAnchor_TopLeft;
            break;
        case AnmOpcode_SetAutoRotate:
            vm->autoRotate = curInstr->args[0];
            break;
        case AnmOpcode_UVScrollX:
            vm->uvScrollPos.x += *(f32 *)&curInstr->args[0];
            if (vm->uvScrollPos.x >= 1.0f)
            {
                vm->uvScrollPos.x -= 1.0f;
            }
            else if (vm->uvScrollPos.x < 0.0f)
            {
                vm->uvScrollPos.x += 1.0f;
            }
            break;
        case AnmOpcode_UVScrollY:
            vm->uvScrollPos.y += *(f32 *)&curInstr->args[0];
            if (vm->uvScrollPos.y >= 1.0f)
            {
                vm->uvScrollPos.y -= 1.0f;
            }
            else if (vm->uvScrollPos.y < 0.0f)
            {
                vm->uvScrollPos.y += 1.0f;
            }
            break;
        case AnmOpcode_SetZWriteDisable:
            vm->flags.zWriteDisable = curInstr->args[0];
            break;
        case AnmOpcode_Nop:
        case AnmOpcode_InterruptLabel:
        default:
            break;
        }
        vm->currentInstruction = (AnmRawInstr *)((u32)curInstr->args + curInstr->argsCount);
    }

stop:
    if (vm->angleVel.x != 0.0f)
    {
        vm->rotation.x =
            utils::AddNormalizeAngle(vm->rotation.x, g_Supervisor.effectiveFramerateMultiplier * vm->angleVel.x);
    }
    if (vm->angleVel.y != 0.0f)
    {
        vm->rotation.y =
            utils::AddNormalizeAngle(vm->rotation.y, g_Supervisor.effectiveFramerateMultiplier * vm->angleVel.y);
    }
    if (vm->angleVel.z != 0.0f)
    {
        vm->rotation.z =
            utils::AddNormalizeAngle(vm->rotation.z, g_Supervisor.effectiveFramerateMultiplier * vm->angleVel.z);
    }
    if (vm->scaleInterpEndTime > 0)
    {
        vm->scaleInterpTime.Tick();
        if (vm->scaleInterpTime.AsFrames() >= vm->scaleInterpEndTime)
        {
            vm->scaleY = vm->scaleInterpFinalY;
            vm->scaleX = vm->scaleInterpFinalX;
            vm->scaleInterpEndTime = 0;
            vm->scaleInterpFinalY = 0.0;
            vm->scaleInterpFinalX = 0.0;
        }
        else
        {
            vm->scaleX = (vm->scaleInterpFinalX - vm->scaleInterpInitialX) * vm->scaleInterpTime.AsFramesFloat() /
                             vm->scaleInterpEndTime +
                         vm->scaleInterpInitialX;
            vm->scaleY = (vm->scaleInterpFinalY - vm->scaleInterpInitialY) * vm->scaleInterpTime.AsFramesFloat() /
                             vm->scaleInterpEndTime +
                         vm->scaleInterpInitialY;
        }
        if ((vm->flags.flip & 1) != 0)
        {
            vm->scaleX = vm->scaleX * -1.f;
        }
        if ((vm->flags.flip & 2) != 0)
        {
            vm->scaleY = vm->scaleY * -1.f;
        }
    }
    else
    {
        vm->scaleY = g_Supervisor.effectiveFramerateMultiplier * vm->scaleInterpFinalY + vm->scaleY;
        vm->scaleX = g_Supervisor.effectiveFramerateMultiplier * vm->scaleInterpFinalX + vm->scaleX;
    }
    if (vm->alphaInterpEndTime > 0)
    {
        vm->alphaInterpTime.Tick();
        local_2c = vm->alphaInterpInitial;
        local_28 = vm->alphaInterpFinal;
        local_30 = vm->alphaInterpTime.AsFramesFloat() / (f32)vm->alphaInterpEndTime;
        if (local_30 >= 1.0f)
        {
            local_30 = 1.0;
        }
        for (local_38 = 0; local_38 < 4; local_38++)
        {
            local_34 = ((f32)COLOR_GET_COMPONENT(local_28, local_38) - (f32)COLOR_GET_COMPONENT(local_2c, local_38)) *
                           local_30 +
                       COLOR_GET_COMPONENT(local_2c, local_38);
            if (local_34 < 0)
            {
                local_34 = 0;
            }
            COLOR_SET_COMPONENT(local_2c, local_38, local_34 >= 256 ? 255 : local_34);
        }
        vm->color = local_2c;
        if (vm->alphaInterpTime.AsFrames() >= vm->alphaInterpEndTime)
        {
            vm->alphaInterpEndTime = 0;
        }
    }
    if (vm->posInterpEndTime != 0)
    {
        local_3c = vm->posInterpTime.AsFramesFloat() / (f32)vm->posInterpEndTime;
        if (local_3c >= 1.0f)
        {
            local_3c = 1.0;
        }
        switch (vm->flags.posTime)
        {
        case 1:
            local_3c = 1.0f - local_3c;
            local_3c *= local_3c;
            local_3c = 1.0f - local_3c;
            break;
        case 2:
            local_3c = 1.0f - local_3c;
            local_3c = local_3c * local_3c * local_3c * local_3c;
            local_3c = 1.0f - local_3c;
            break;
        }
        if (vm->flags.usePosOffset == 0)
        {
            vm->pos.x = local_3c * vm->posInterpFinal.x + (1.0f - local_3c) * vm->posInterpInitial.x;
            vm->pos.y = local_3c * vm->posInterpFinal.y + (1.0f - local_3c) * vm->posInterpInitial.y;
            vm->pos.z = local_3c * vm->posInterpFinal.z + (1.0f - local_3c) * vm->posInterpInitial.z;
        }
        else
        {
            vm->posOffset.x = local_3c * vm->posInterpFinal.x + (1.0f - local_3c) * vm->posInterpInitial.x;
            vm->posOffset.y = local_3c * vm->posInterpFinal.y + (1.0f - local_3c) * vm->posInterpInitial.y;
            vm->posOffset.z = local_3c * vm->posInterpFinal.z + (1.0f - local_3c) * vm->posInterpInitial.z;
        }

        if (vm->posInterpTime.AsFrames() >= vm->posInterpEndTime)
        {
            vm->posInterpEndTime = 0;
        }
        vm->posInterpTime.Tick();
    }
    vm->currentTimeInScript.Tick();
    return 0;
}

void AnmManager::DrawTextToSprite(u32 textureDstIdx, i32 xPos, i32 yPos, i32 spriteWidth, i32 spriteHeight,
                                  i32 fontWidth, i32 fontHeight, ZunColor textColor, ZunColor shadowColor,
                                  char *strToPrint)
{
    if (fontWidth <= 0)
    {
        fontWidth = 15;
    }
    if (fontHeight <= 0)
    {
        fontHeight = 15;
    }
    TextHelper::RenderTextToTexture(xPos, yPos, spriteWidth, spriteHeight, fontWidth, fontHeight, textColor,
                                    shadowColor, strToPrint, this->textures[textureDstIdx]);
    return;
}

void AnmManager::DrawVmTextFmt(AnmManager *anmMgr, AnmVm *vm, ZunColor textColor, ZunColor shadowColor, char *fmt, ...)
{
    u32 fontWidth;
    char buffer[64];
    va_list argptr;

    fontWidth = vm->fontWidth;
    va_start(argptr, fmt);
    vsprintf(buffer, fmt, argptr);
    va_end(argptr);
    anmMgr->DrawTextToSprite(vm->sprite->sourceFileIndex, vm->sprite->startPixelInclusive.x,
                             vm->sprite->startPixelInclusive.y, vm->sprite->textureWidth, vm->sprite->textureHeight,
                             fontWidth, vm->fontHeight, textColor, shadowColor, buffer);
    vm->flags.isVisible = true;
    return;
}

void AnmManager::DrawStringFormat(AnmManager *mgr, AnmVm *vm, ZunColor textColor, ZunColor shadowColor, char *fmt, ...)
{
    char buf[64];
    va_list args;
    i32 fontWidth;
    i32 secondPartStartX;

    fontWidth = vm->fontWidth <= 0 ? 15 : vm->fontWidth;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    mgr->DrawTextToSprite(vm->sprite->sourceFileIndex, vm->sprite->startPixelInclusive.x,
                          vm->sprite->startPixelInclusive.y, vm->sprite->textureWidth, vm->sprite->textureHeight,
                          fontWidth, vm->fontHeight, textColor, shadowColor, " ");
    secondPartStartX =
        vm->sprite->startPixelInclusive.x + vm->sprite->textureWidth - ((f32)strlen(buf) * (f32)(fontWidth + 1) / 2.0f);
    mgr->DrawTextToSprite(vm->sprite->sourceFileIndex, secondPartStartX, vm->sprite->startPixelInclusive.y,
                          vm->sprite->textureWidth, vm->sprite->textureHeight, fontWidth, vm->fontHeight, textColor,
                          shadowColor, buf);
    vm->flags.isVisible = true;
    return;
}

void AnmManager::DrawStringFormat2(AnmManager *mgr, AnmVm *vm, ZunColor textColor, ZunColor shadowColor, char *fmt, ...)
{
    char buf[64];
    va_list args;
    i32 fontWidth;
    i32 secondPartStartX;

    fontWidth = vm->fontWidth <= 0 ? 15 : vm->fontWidth;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    mgr->DrawTextToSprite(vm->sprite->sourceFileIndex, vm->sprite->startPixelInclusive.x,
                          vm->sprite->startPixelInclusive.y, vm->sprite->textureWidth, vm->sprite->textureHeight,
                          fontWidth, vm->fontHeight, textColor, shadowColor, " ");
    secondPartStartX = vm->sprite->startPixelInclusive.x + vm->sprite->textureWidth / 2.0f -
                       ((f32)strlen(buf) * (f32)(fontWidth + 1) / 4.0f);
    mgr->DrawTextToSprite(vm->sprite->sourceFileIndex, secondPartStartX, vm->sprite->startPixelInclusive.y,
                          vm->sprite->textureWidth, vm->sprite->textureHeight, fontWidth, vm->fontHeight, textColor,
                          shadowColor, buf);
    vm->flags.isVisible = true;
    return;
}

ZunResult AnmManager::LoadSurface(i32 surfaceIdx, char *path)
{
#ifdef USE_VULKAN
    u8 *data = FileSystem::OpenPath(path, 0);
    if (data == NULL)
    {
        g_GameErrorContext.Fatal(TH_ERR_CANNOT_BE_LOADED, path);
        return ZUN_ERROR;
    }
    uint32_t width = 0;
    uint32_t height = 0;
    bool ok = g_Renderer->LoadSurfaceImage(surfaceIdx, data, g_LastFileSize, &width, &height);
    free(data);
    if (!ok)
    {
        return ZUN_ERROR;
    }
    this->surfaceSourceInfo[surfaceIdx].Width = width;
    this->surfaceSourceInfo[surfaceIdx].Height = height;
    return ZUN_SUCCESS;
#else
    if (this->surfaces[surfaceIdx] != NULL)
    {
        this->ReleaseSurface(surfaceIdx);
    }
    u8 *data = FileSystem::OpenPath(path, 0);
    if (data == NULL)
    {
        g_GameErrorContext.Fatal(TH_ERR_CANNOT_BE_LOADED, path);
        return ZUN_ERROR;
    }

    LPDIRECT3DSURFACE8 surface;
    if (g_Supervisor.d3dDevice->CreateImageSurface(0x280, 0x400, g_Supervisor.presentParameters.BackBufferFormat,
                                                   &surface) != D3D_OK)
    {
        return ZUN_ERROR;
    }

    if (D3DXLoadSurfaceFromFileInMemory(surface, NULL, NULL, data, g_LastFileSize, NULL, D3DX_FILTER_NONE, 0,
                                        &this->surfaceSourceInfo[surfaceIdx]) != D3D_OK)
    {
        goto fail;
    }
    if (g_Supervisor.d3dDevice->CreateRenderTarget(this->surfaceSourceInfo[surfaceIdx].Width,
                                                   this->surfaceSourceInfo[surfaceIdx].Height,
                                                   g_Supervisor.presentParameters.BackBufferFormat, D3DMULTISAMPLE_NONE,
                                                   TRUE, &this->surfaces[surfaceIdx]) != D3D_OK &&
        g_Supervisor.d3dDevice->CreateImageSurface(
            this->surfaceSourceInfo[surfaceIdx].Width, this->surfaceSourceInfo[surfaceIdx].Height,
            g_Supervisor.presentParameters.BackBufferFormat, &this->surfaces[surfaceIdx]) != D3D_OK)
    {
        goto fail;
    }
    if (g_Supervisor.d3dDevice->CreateImageSurface(
            this->surfaceSourceInfo[surfaceIdx].Width, this->surfaceSourceInfo[surfaceIdx].Height,
            g_Supervisor.presentParameters.BackBufferFormat, &this->surfacesBis[surfaceIdx]) != D3D_OK)
    {
        goto fail;
    }

    if (D3DXLoadSurfaceFromSurface(this->surfaces[surfaceIdx], NULL, NULL, surface, NULL, NULL, D3DX_FILTER_NONE, 0) !=
        D3D_OK)
    {
        goto fail;
    }

    if (D3DXLoadSurfaceFromSurface(this->surfacesBis[surfaceIdx], NULL, NULL, surface, NULL, NULL, D3DX_FILTER_NONE,
                                   0) != D3D_OK)
    {
        goto fail;
    }

    if (surface != NULL)
    {
        surface->Release();
        surface = NULL;
    }
    free(data);
    return ZUN_SUCCESS;

fail:
    if (surface != NULL)
    {
        surface->Release();
        surface = NULL;
    }
    free(data);
    return ZUN_ERROR;
#endif
}

void AnmManager::ReleaseSurface(i32 surfaceIdx)
{
#ifdef USE_VULKAN
    g_Renderer->ReleaseSurfaceImage(surfaceIdx);
    this->surfaces[surfaceIdx] = NULL;
    this->surfacesBis[surfaceIdx] = NULL;
    return;
#else
    if (this->surfaces[surfaceIdx] != NULL)
    {
        this->surfaces[surfaceIdx]->Release();
        this->surfaces[surfaceIdx] = NULL;
    }
    if (this->surfacesBis[surfaceIdx] != NULL)
    {
        this->surfacesBis[surfaceIdx]->Release();
        this->surfacesBis[surfaceIdx] = NULL;
    }
#endif
}

void AnmManager::CopySurfaceToBackBuffer(i32 surfaceIdx, i32 left, i32 top, i32 x, i32 y)
{
#ifdef USE_VULKAN
    g_Renderer->DrawSurfaceRegion(surfaceIdx, x, y, left, top,
                                  (i32)this->surfaceSourceInfo[surfaceIdx].Width - left,
                                  (i32)this->surfaceSourceInfo[surfaceIdx].Height - top);
    return;
#else
    if (this->surfacesBis[surfaceIdx] == NULL)
    {
        return;
    }

    IDirect3DSurface8 *destSurface;
    if (g_VerticalMode && GameWindow::verticalSurface != NULL)
    {
        destSurface = GameWindow::verticalSurface;
        destSurface->AddRef();
    }
    else if (g_Supervisor.d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &destSurface) != D3D_OK)
    {
        return;
    }
    if (this->surfaces[surfaceIdx] == NULL)
    {
        if (g_Supervisor.d3dDevice->CreateRenderTarget(
                this->surfaceSourceInfo[surfaceIdx].Width, this->surfaceSourceInfo[surfaceIdx].Height,
                g_Supervisor.presentParameters.BackBufferFormat, D3DMULTISAMPLE_NONE, TRUE,
                &this->surfaces[surfaceIdx]) != D3D_OK)
        {
            if (g_Supervisor.d3dDevice->CreateImageSurface(
                    this->surfaceSourceInfo[surfaceIdx].Width, this->surfaceSourceInfo[surfaceIdx].Height,
                    g_Supervisor.presentParameters.BackBufferFormat, &this->surfaces[surfaceIdx]) != D3D_OK)
            {
                destSurface->Release();
                return;
            }
        }
        if (D3DXLoadSurfaceFromSurface(this->surfaces[surfaceIdx], NULL, NULL, this->surfacesBis[surfaceIdx], NULL,
                                       NULL, D3DX_FILTER_NONE, 0) != D3D_OK)
        {
            destSurface->Release();
            return;
        }
    }

    RECT sourceRect;
    POINT destPoint;
    sourceRect.left = left;
    sourceRect.top = top;
    sourceRect.right = this->surfaceSourceInfo[surfaceIdx].Width;
    sourceRect.bottom = this->surfaceSourceInfo[surfaceIdx].Height;
    destPoint.x = x;
    destPoint.y = y;
    g_Supervisor.d3dDevice->CopyRects(this->surfaces[surfaceIdx], &sourceRect, 1, destSurface, &destPoint);
    destSurface->Release();
#endif
}

void AnmManager::DrawEndingRect(i32 surfaceIdx, i32 rectX, i32 rectY, i32 rectLeft, i32 rectTop, i32 width, i32 height)
{
#ifdef USE_VULKAN
    g_Renderer->DrawSurfaceRegion(surfaceIdx, rectX, rectY, rectLeft, rectTop, width, height);
    return;
#else
    if (this->surfacesBis[surfaceIdx] == NULL)
    {
        return;
    }

    IDirect3DSurface8 *D3D_Surface;
    if (g_Supervisor.d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &D3D_Surface) != D3D_OK)
    {
        return;
    }

    if (this->surfaces[surfaceIdx] == NULL)
    {
        if (g_Supervisor.d3dDevice->CreateRenderTarget(
                this->surfaceSourceInfo[surfaceIdx].Width, this->surfaceSourceInfo[surfaceIdx].Height,
                g_Supervisor.presentParameters.BackBufferFormat, D3DMULTISAMPLE_NONE, TRUE,
                &this->surfaces[surfaceIdx]) != D3D_OK)
        {
            if (g_Supervisor.d3dDevice->CreateImageSurface(
                    this->surfaceSourceInfo[surfaceIdx].Width, this->surfaceSourceInfo[surfaceIdx].Height,
                    g_Supervisor.presentParameters.BackBufferFormat, &this->surfaces[surfaceIdx]) != D3D_OK)
            {
                D3D_Surface->Release();
                return;
            }
        }
        if (D3DXLoadSurfaceFromSurface(this->surfaces[surfaceIdx], NULL, NULL, this->surfacesBis[surfaceIdx], NULL,
                                       NULL, D3DX_FILTER_NONE, 0) != D3D_OK)
        {
            D3D_Surface->Release();
            return;
        }
    }

    RECT rect;
    POINT point;
    rect.left = rectLeft;
    rect.top = rectTop;
    rect.right = rectLeft + width;
    rect.bottom = rectTop + height;
    point.x = rectX;
    point.y = rectY;
    g_Supervisor.d3dDevice->CopyRects(this->surfaces[surfaceIdx], &rect, 1, D3D_Surface, &point);
    D3D_Surface->Release();
#endif
}

void AnmManager::TakeScreenshot(i32 textureId, i32 left, i32 top, i32 width, i32 height)
{
#ifdef USE_VULKAN
    TextureHandle tex = (TextureHandle)(uintptr_t)this->textures[textureId];
    if (tex == INVALID_TEXTURE)
    {
        return;
    }
    g_Renderer->CaptureScreen(tex, left, top, width, height);
    return;
#else
    LPDIRECT3DSURFACE8 sourceSurface;
    LPDIRECT3DSURFACE8 destSurface;
    RECT rect;

    if (this->textures[textureId] == NULL)
    {
        return;
    }

    if (g_VerticalMode && GameWindow::verticalSurface != NULL)
    {
        sourceSurface = GameWindow::verticalSurface;
        sourceSurface->AddRef();
    }
    else if (g_Supervisor.d3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &sourceSurface) != D3D_OK)
    {
        return;
    }
    if (this->textures[textureId]->GetSurfaceLevel(0, &destSurface) != D3D_OK)
    {
        sourceSurface->Release();
        return;
    }

    rect.left = left;
    rect.top = top;
    rect.right = left + width;
    rect.bottom = top + height;
    if (D3DXLoadSurfaceFromSurface(destSurface, NULL, NULL, sourceSurface, NULL, &rect, D3DX_DEFAULT, 0) != D3D_OK)
    {
        destSurface->Release();
        sourceSurface->Release();
        return;
    }
    destSurface->Release();
    sourceSurface->Release();
    return;
#endif
}
}; // namespace th06
