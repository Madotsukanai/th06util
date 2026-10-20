#pragma once

// mingw_compat.hpp (force-included) defines empty SAL macros such as __out,
// which collide with libstdc++ parameter names (e.g. in bits/c++locale.h).
// Undefine them before pulling in any C++ standard library headers.
#ifdef __in
#undef __in
#endif
#ifdef __out
#undef __out
#endif
#ifdef __inout
#undef __inout
#endif

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

namespace th06 {

using TextureHandle = size_t;
constexpr TextureHandle INVALID_TEXTURE = static_cast<size_t>(-1);

enum BlendMode {
    BLEND_NONE = 0,
    BLEND_ALPHA = 1,       // SrcAlpha, InvSrcAlpha
    BLEND_ADD = 2,         // SrcAlpha, One (or One, One)
    BLEND_SUB = 3,         // Zero, InvSrcColor
};

// 2D vertex in 640x480 screen space.
// NOTE: Vertex2D MUST have the same size as Vertex3D. Both 2D and 3D quads are
// written into the same per-frame vertex buffer and share the same running
// vertex index, so a stride mismatch would make the 2D and 3D vertex ranges
// overlap and corrupt each other (3D stage geometry is written first, then
// overwritten by the later 2D HUD/frame quads).
struct Vertex2D {
    float x, y;
    float u, v;
    uint32_t color; // 0xAARRGGBB
    float _pad;     // keep sizeof(Vertex2D) == sizeof(Vertex3D)
};

// 3D vertex for stage background geometry
struct Vertex3D {
    float x, y, z;
    float u, v;
    uint32_t color; // 0xAARRGGBB
};

struct SurfaceDesc {
    uint32_t width;
    uint32_t height;
    void* pixels;
    int pitch;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // ── Lifecycle ──
    virtual bool Initialize(void* windowHandle, int width, int height, bool fullscreen) = 0;
    virtual void Shutdown() = 0;
    virtual void OnWindowResize(int width, int height) = 0;

    // ── Frame control ──
    virtual void BeginScene() = 0;
    virtual void EndScene() = 0;
    virtual void Present() = 0;
    virtual void Clear(uint32_t argb, bool clearDepth = true) = 0;
    virtual void SetViewport(int x, int y, int width, int height) = 0;
    // Game viewport (D3D SetViewport forwarding): confines 3D stage rendering to
    // the arcade region. 2D quads are drawn in absolute screen space regardless.
    virtual void SetGameViewport(int x, int y, int width, int height, float minZ, float maxZ) = 0;

    // ── Texture management ──
    virtual TextureHandle CreateTexture(uint32_t width, uint32_t height, bool clampToEdge = false) = 0;
    virtual TextureHandle CreateTextureFromMemory(const void* data, size_t size, uint32_t colorKey = 0,
                                                  bool clampToEdge = false) = 0;
    virtual bool LockTexture(TextureHandle handle, SurfaceDesc* desc) = 0;
    virtual void UnlockTexture(TextureHandle handle) = 0;
    virtual void ReleaseTexture(TextureHandle handle) = 0;
    virtual TextureHandle GetWhiteTexture() const = 0;

    // ── Offscreen surfaces (title/select/result/ending backgrounds) ──
    // LoadSurfaceImage decodes an image (png/jpg) and stores it as an
    // offscreen surface under a caller-chosen index.
    virtual bool LoadSurfaceImage(int index, const void* data, size_t size, uint32_t* outWidth,
                                  uint32_t* outHeight) = 0;
    virtual void DrawSurfaceRegion(int index, int dstX, int dstY, int srcX, int srcY, int srcW,
                                   int srcH) = 0;
    virtual void ReleaseSurfaceImage(int index) = 0;

    // ── Vertical (portrait) mode ──
    // The offscreen render target mirrors GameWindow::verticalSurface
    // (640 x (480 + 168)): the whole draw chain renders into it, and
    // Present() composes the rotated portrait image onto the swapchain.
    virtual void CreateVerticalComposition(int width, int height)
    {
    }
    virtual void ReleaseVerticalComposition()
    {
    }
    // Called by the game loop when the vertical surface is active for this
    // scene; Present() only composes when this was enabled.
    virtual void SetVerticalComposition(bool enabled)
    {
    }
    virtual void SetVerticalInGame(bool inGame)
    {
    }
    virtual void SetVerticalFlip180(bool flip)
    {
    }
    virtual void SetVerticalTransitionBlack(bool black)
    {
    }

    // ── Text rendering (TextHelper) ──
    // Blit an ARGB8888 source image into a texture, scaling it to dstW x dstH.
    virtual void BlitPixelsToTexture(TextureHandle tex, int dstX, int dstY, int dstW, int dstH,
                                     const uint32_t* argbSrc, int srcW, int srcH) = 0;

    // ── 2D Drawing (AnmManager, Gui, Player, ScreenEffect) ──
    // Quad vertices in strip order: 0=TL, 1=TR, 2=BL, 3=BR
    virtual void DrawQuad2D(TextureHandle tex, const Vertex2D v[4], BlendMode blend) = 0;
    virtual void DrawColoredQuad2D(const Vertex2D v[4], BlendMode blend) = 0;
    virtual void DrawTriangleStrip2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) = 0;
    virtual void DrawTriangleFan2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) = 0;

    // ── 3D Stage Drawing (Stage, AnmManager::Draw3, Draw2) ──
    virtual void SetCamera3D(const float view[16], const float proj[16]) = 0;
    virtual void SetFog(bool enable, uint32_t color, float nearZ, float farZ) = 0;
    virtual void DrawQuad3D(const float world[16], TextureHandle tex, const Vertex3D v[4], BlendMode blend) = 0;

    // ── Fixed-function state emulation ──
    // D3DTOP_ADD instead of D3DTOP_MODULATE for the colour stage (AnmVm colorOp).
    // Applies to all following 2D/3D draws until changed.
    virtual void SetColorOpAdd(bool enable) = 0;
    // D3DRS_ZWRITEENABLE for 3D quads (AnmVm zWriteDisable).
    virtual void SetDepthWrite(bool enable) = 0;

    // ── Screen Capture / Effects ──
    // Copies the source rect (in 640x480 game space) from the presented frame
    // into `target`, clipped to the texture size like D3D8 CopyRects (no scaling).
    virtual void CaptureScreen(TextureHandle target, int left, int top, int width, int height) = 0;
    // Reads back the presented frame's letter-boxed game region (what a D3D8
    // backbuffer of the same size would contain) as row-major 0xAARRGGBB.
    virtual bool ReadGameImage(std::vector<uint32_t>& argbOut) = 0;
};

extern IRenderer* g_Renderer;

} // namespace th06
