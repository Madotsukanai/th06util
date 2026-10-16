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

namespace th06 {

using TextureHandle = size_t;
constexpr TextureHandle INVALID_TEXTURE = static_cast<size_t>(-1);

enum BlendMode {
    BLEND_NONE = 0,
    BLEND_ALPHA = 1,       // SrcAlpha, InvSrcAlpha
    BLEND_ADD = 2,         // SrcAlpha, One (or One, One)
    BLEND_SUB = 3,         // Zero, InvSrcColor
};

// 2D vertex in 640x480 screen space
struct Vertex2D {
    float x, y;
    float u, v;
    uint32_t color; // 0xAARRGGBB
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

    // ── Texture management ──
    virtual TextureHandle CreateTexture(uint32_t width, uint32_t height) = 0;
    virtual TextureHandle CreateTextureFromMemory(const void* data, size_t size, uint32_t colorKey = 0) = 0;
    virtual bool LockTexture(TextureHandle handle, SurfaceDesc* desc) = 0;
    virtual void UnlockTexture(TextureHandle handle) = 0;
    virtual void ReleaseTexture(TextureHandle handle) = 0;
    virtual TextureHandle GetWhiteTexture() const = 0;

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

    // ── Screen Capture / Effects ──
    virtual void CaptureScreen(TextureHandle target) = 0;
};

extern IRenderer* g_Renderer;

} // namespace th06
