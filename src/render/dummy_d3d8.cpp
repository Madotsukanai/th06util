#ifdef USE_VULKAN

#include "dummy_d3d8.hpp"
#include "renderer.hpp"
#include "../utils.hpp"
#include <cstring>
#include <map>
#include <vector>

// Missing from the vendored d3d8types.h.
#ifndef D3DPOOL_SCRATCH
#define D3DPOOL_SCRATCH 3
#endif

// Dummy Direct3D 8 layer for the Vulkan backend.
//
// Actual drawing never goes through this layer: game code is patched to call
// g_Renderer directly under USE_VULKAN. What remains here is the part of the
// D3D8 API that game code still touches (device setup, render/texture-stage
// state, viewport, surfaces, vertex buffers ...). It therefore has to behave
// like a well-formed D3D8 device: output parameters must be filled in, Get*
// must return what Set* stored, and distinct resources must be distinct.
//
// A subset of the state is forwarded to g_Renderer (see kForward* below) so
// that code which still uses the D3D8 calls ends up with the right Vulkan
// state. Flip a bit in kForwardMask to turn a group of forwarding off.

namespace th06 {

namespace {

enum : unsigned {
    kForwardCamera = 1u << 0,    // SetTransform(VIEW / PROJECTION) -> SetCamera3D
    kForwardFog = 1u << 1,       // D3DRS_FOG* -> SetFog
    kForwardZWrite = 1u << 2,    // D3DRS_ZWRITEENABLE -> SetDepthWrite
    kForwardColorOp = 1u << 3,   // D3DTSS_COLOROP (stage 0) -> SetColorOpAdd
    kForwardViewport = 1u << 4,  // SetViewport -> SetGameViewport
};
constexpr unsigned kForwardMask = kForwardCamera | kForwardFog | kForwardZWrite | kForwardColorOp | kForwardViewport;

constexpr UINT kDefaultWidth = 640;
constexpr UINT kDefaultHeight = 480;

UINT BytesPerPixel(D3DFORMAT fmt) {
    switch (fmt) {
    case D3DFMT_R5G6B5:
    case D3DFMT_X1R5G5B5:
    case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4:
    case D3DFMT_D16:
    case D3DFMT_D16_LOCKABLE:
        return 2;
    case D3DFMT_R3G3B2:
    case D3DFMT_A8:
    case D3DFMT_L8:
    case D3DFMT_P8:
        return 1;
    default:
        return 4;
    }
}

void FillDisplayMode(D3DDISPLAYMODE* m) {
    if (m) {
        m->Width = kDefaultWidth;
        m->Height = kDefaultHeight;
        m->RefreshRate = 60;
        m->Format = D3DFMT_X8R8G8B8;
    }
}

void FillCaps(D3DCAPS8* p) {
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->DeviceType = D3DDEVTYPE_HAL;
    p->AdapterOrdinal = 0;
    p->TextureOpCaps = 0xFFFFFFFF;
    p->PresentationIntervals = 0xFFFFFFFF;
    p->DevCaps = 0xFFFFFFFF;
    p->PrimitiveMiscCaps = 0xFFFFFFFF;
    p->RasterCaps = 0xFFFFFFFF;
    p->ZCmpCaps = 0xFFFFFFFF;
    p->SrcBlendCaps = 0xFFFFFFFF;
    p->DestBlendCaps = 0xFFFFFFFF;
    p->AlphaCmpCaps = 0xFFFFFFFF;
    p->ShadeCaps = 0xFFFFFFFF;
    p->TextureCaps = 0xFFFFFFFF;
    p->TextureFilterCaps = 0xFFFFFFFF;
    p->TextureAddressCaps = 0xFFFFFFFF;
    p->MaxTextureWidth = 4096;
    p->MaxTextureHeight = 4096;
    p->MaxTextureRepeat = 8192;
    p->MaxTextureBlendStages = 8;
    p->MaxSimultaneousTextures = 8;
    p->MaxPrimitiveCount = 0xFFFF;
    p->MaxVertexIndex = 0xFFFF;
    p->MaxPointSize = 64.0f;
    p->MaxActiveLights = 8;
    p->MaxUserClipPlanes = 6;
    p->MaxVertexBlendMatrices = 4;
    p->MaxStreams = 16;
    p->MaxStreamStride = 256;
    p->VertexShaderVersion = D3DVS_VERSION(1, 1);
    p->PixelShaderVersion = D3DPS_VERSION(1, 4);
}

D3DMATRIX IdentityMatrix() {
    D3DMATRIX m;
    memset(&m, 0, sizeof(m));
    float* f = reinterpret_cast<float*>(&m);
    f[0] = f[5] = f[10] = f[15] = 1.0f;
    return m;
}

} // namespace

class DummyD3DDevice;
IDirect3D8* GetDummyD3D();

// ---------------------------------------------------------------------------
// Resources
// ---------------------------------------------------------------------------

// Common IDirect3DResource8 boilerplate. Resources created through the device
// are reference counted and freed on the last Release(); "persistent" ones
// (back buffer, default depth buffer) are owned by the device and never freed.
#define DUMMY_RESOURCE_COMMON(Base)                                                                          \
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }            \
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }                                           \
    ULONG STDMETHODCALLTYPE Release() override {                                                            \
        if (persistent_) return 1;                                                                          \
        ULONG r = refs_ > 0 ? --refs_ : 0;                                                                  \
        if (r == 0) delete this;                                                                            \
        return r;                                                                                           \
    }                                                                                                       \
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** pp) override;                                    \
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, CONST void*, DWORD, DWORD) override { return S_OK; } \
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, void*, DWORD*) override { return E_NOTIMPL; }        \
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override { return S_OK; }

class DummyD3DVertexBuffer final : public IDirect3DVertexBuffer8 {
    ULONG refs_ = 1;
    bool persistent_ = false;
    std::vector<BYTE> data_;
    D3DVERTEXBUFFER_DESC desc_{};

public:
    DummyD3DVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool) {
        data_.assign(length ? length : 16, 0);
        desc_.Format = D3DFMT_VERTEXDATA;
        desc_.Type = D3DRTYPE_VERTEXBUFFER;
        desc_.Usage = usage;
        desc_.Pool = pool;
        desc_.Size = length;
        desc_.FVF = fvf;
    }
    DUMMY_RESOURCE_COMMON(IDirect3DVertexBuffer8)
    DWORD STDMETHODCALLTYPE SetPriority(DWORD) override { return 0; }
    DWORD STDMETHODCALLTYPE GetPriority() override { return 0; }
    void STDMETHODCALLTYPE PreLoad() override {}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return D3DRTYPE_VERTEXBUFFER; }
    HRESULT STDMETHODCALLTYPE Lock(UINT offset, UINT size, BYTE** ppbData, DWORD) override {
        if (!ppbData) return D3DERR_INVALIDCALL;
        if (offset > data_.size() || (size != 0 && offset + size > data_.size())) {
            *ppbData = nullptr;
            return D3DERR_INVALIDCALL;
        }
        *ppbData = data_.data() + offset;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC* pDesc) override {
        if (!pDesc) return D3DERR_INVALIDCALL;
        *pDesc = desc_;
        return S_OK;
    }
};

class DummyD3DIndexBuffer final : public IDirect3DIndexBuffer8 {
    ULONG refs_ = 1;
    bool persistent_ = false;
    std::vector<BYTE> data_;
    D3DINDEXBUFFER_DESC desc_{};

public:
    DummyD3DIndexBuffer(UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool) {
        data_.assign(length ? length : 16, 0);
        desc_.Format = fmt;
        desc_.Type = D3DRTYPE_INDEXBUFFER;
        desc_.Usage = usage;
        desc_.Pool = pool;
        desc_.Size = length;
    }
    DUMMY_RESOURCE_COMMON(IDirect3DIndexBuffer8)
    DWORD STDMETHODCALLTYPE SetPriority(DWORD) override { return 0; }
    DWORD STDMETHODCALLTYPE GetPriority() override { return 0; }
    void STDMETHODCALLTYPE PreLoad() override {}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return D3DRTYPE_INDEXBUFFER; }
    HRESULT STDMETHODCALLTYPE Lock(UINT offset, UINT size, BYTE** ppbData, DWORD) override {
        if (!ppbData) return D3DERR_INVALIDCALL;
        if (offset > data_.size() || (size != 0 && offset + size > data_.size())) {
            *ppbData = nullptr;
            return D3DERR_INVALIDCALL;
        }
        *ppbData = data_.data() + offset;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC* pDesc) override {
        if (!pDesc) return D3DERR_INVALIDCALL;
        *pDesc = desc_;
        return S_OK;
    }
};

class DummyD3DSurface final : public IDirect3DSurface8 {
    ULONG refs_ = 1;
    bool persistent_ = false;
    D3DSURFACE_DESC desc_{};
    UINT pitch_ = 0;
    std::vector<BYTE> pixels_; // allocated lazily on first LockRect

public:
    DummyD3DSurface(UINT w, UINT h, D3DFORMAT fmt, D3DRESOURCETYPE type, DWORD usage, D3DPOOL pool, bool persistent) {
        persistent_ = persistent;
        Resize(w, h, fmt);
        desc_.Type = type;
        desc_.Usage = usage;
        desc_.Pool = pool;
        desc_.MultiSampleType = D3DMULTISAMPLE_NONE;
    }
    UINT Width() const { return desc_.Width; }
    UINT Height() const { return desc_.Height; }
    void Resize(UINT w, UINT h, D3DFORMAT fmt) {
        desc_.Width = w ? w : kDefaultWidth;
        desc_.Height = h ? h : kDefaultHeight;
        desc_.Format = fmt;
        pitch_ = desc_.Width * BytesPerPixel(fmt);
        desc_.Size = pitch_ * desc_.Height;
        pixels_.clear();
    }
    DUMMY_RESOURCE_COMMON(IDirect3DSurface8)
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID, void** pp) override {
        if (pp) *pp = nullptr;
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* d) override {
        if (!d) return D3DERR_INVALIDCALL;
        *d = desc_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLR, CONST RECT* rect, DWORD) override {
        if (!pLR) return D3DERR_INVALIDCALL;
        if (pixels_.empty()) pixels_.assign(static_cast<size_t>(desc_.Size), 0);
        UINT left = 0, top = 0;
        if (rect) {
            if (rect->left < 0 || rect->top < 0 || rect->right > static_cast<LONG>(desc_.Width) ||
                rect->bottom > static_cast<LONG>(desc_.Height) || rect->left >= rect->right ||
                rect->top >= rect->bottom) {
                pLR->pBits = nullptr;
                pLR->Pitch = 0;
                return D3DERR_INVALIDCALL;
            }
            left = static_cast<UINT>(rect->left);
            top = static_cast<UINT>(rect->top);
        }
        pLR->Pitch = static_cast<INT>(pitch_);
        pLR->pBits = pixels_.data() + static_cast<size_t>(top) * pitch_ + static_cast<size_t>(left) * BytesPerPixel(desc_.Format);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UnlockRect() override { return S_OK; }
};

// ---------------------------------------------------------------------------
// Device
// ---------------------------------------------------------------------------

class DummyD3DDevice : public IDirect3DDevice8 {
    static constexpr int kMaxStages = 8;
    static constexpr int kMaxRenderStates = 256;
    static constexpr int kMaxStageStates = 33;

    DummyD3DSurface backBuffer_{kDefaultWidth, kDefaultHeight, D3DFMT_X8R8G8B8, D3DRTYPE_SURFACE,
                                D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, true};
    DummyD3DSurface depthBuffer_{kDefaultWidth, kDefaultHeight, D3DFMT_D16, D3DRTYPE_SURFACE,
                                 D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, true};
    IDirect3DSurface8* renderTarget_ = &backBuffer_;
    IDirect3DSurface8* depthTarget_ = &depthBuffer_;

    D3DVIEWPORT8 curViewport_ = {0, 0, kDefaultWidth, kDefaultHeight, 0.0f, 1.0f};

    DWORD renderStates_[kMaxRenderStates] = {};
    DWORD stageStates_[kMaxStages][kMaxStageStates] = {};
    IDirect3DBaseTexture8* textures_[kMaxStages] = {};
    std::map<DWORD, D3DMATRIX> transforms_;
    bool haveView_ = false, haveProj_ = false;

    D3DMATERIAL8 material_{};
    std::map<DWORD, D3DLIGHT8> lights_;
    std::map<DWORD, BOOL> lightEnable_;
    float clipPlanes_[6][4] = {};
    D3DCLIPSTATUS8 clipStatus_{};

    DWORD vertexShader_ = 0;
    DWORD pixelShader_ = 0;
    float vsConst_[96][4] = {};
    float psConst_[8][4] = {};
    DWORD nextShaderHandle_ = 1;
    DWORD nextBlockToken_ = 1;
    bool recordingBlock_ = false;

    IDirect3DVertexBuffer8* streamSource_[16] = {};
    UINT streamStride_[16] = {};
    IDirect3DIndexBuffer8* indices_ = nullptr;
    UINT baseVertexIndex_ = 0;

    D3DDEVICE_CREATION_PARAMETERS creation_{};

    // Fog state mirrored so that D3DRS_FOG* can be forwarded as one SetFog().
    static float AsFloat(DWORD v) {
        float f;
        memcpy(&f, &v, sizeof(f));
        return f;
    }

    void ResetState() {
        memset(renderStates_, 0, sizeof(renderStates_));
        memset(stageStates_, 0, sizeof(stageStates_));
        // D3D8 defaults for the states we track.
        renderStates_[D3DRS_ZENABLE] = D3DZB_TRUE;
        renderStates_[D3DRS_FILLMODE] = D3DFILL_SOLID;
        renderStates_[D3DRS_SHADEMODE] = D3DSHADE_GOURAUD;
        renderStates_[D3DRS_ZWRITEENABLE] = TRUE;
        renderStates_[D3DRS_SRCBLEND] = D3DBLEND_ONE;
        renderStates_[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
        renderStates_[D3DRS_CULLMODE] = D3DCULL_CCW;
        renderStates_[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
        renderStates_[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
        renderStates_[D3DRS_LIGHTING] = TRUE;
        renderStates_[D3DRS_COLORWRITEENABLE] = 0xF;
        float one = 1.0f, zero = 0.0f;
        memcpy(&renderStates_[D3DRS_FOGEND], &one, sizeof(DWORD));
        memcpy(&renderStates_[D3DRS_FOGSTART], &zero, sizeof(DWORD));
        memcpy(&renderStates_[D3DRS_FOGDENSITY], &one, sizeof(DWORD));
        renderStates_[D3DRS_TEXTUREFACTOR] = 0xFFFFFFFF;
        for (int i = 0; i < kMaxStages; i++) {
            stageStates_[i][D3DTSS_COLOROP] = (i == 0) ? D3DTOP_MODULATE : D3DTOP_DISABLE;
            stageStates_[i][D3DTSS_ALPHAOP] = (i == 0) ? D3DTOP_SELECTARG1 : D3DTOP_DISABLE;
            stageStates_[i][D3DTSS_COLORARG1] = D3DTA_TEXTURE;
            stageStates_[i][D3DTSS_COLORARG2] = D3DTA_CURRENT;
            stageStates_[i][D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
            stageStates_[i][D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
            stageStates_[i][D3DTSS_ADDRESSU] = D3DTADDRESS_WRAP;
            stageStates_[i][D3DTSS_ADDRESSV] = D3DTADDRESS_WRAP;
            stageStates_[i][D3DTSS_MAGFILTER] = D3DTEXF_POINT;
            stageStates_[i][D3DTSS_MINFILTER] = D3DTEXF_POINT;
            stageStates_[i][D3DTSS_MIPFILTER] = D3DTEXF_NONE;
        }
        transforms_.clear();
        haveView_ = haveProj_ = false;
        memset(textures_, 0, sizeof(textures_));
        memset(streamSource_, 0, sizeof(streamSource_));
        memset(streamStride_, 0, sizeof(streamStride_));
        indices_ = nullptr;
        baseVertexIndex_ = 0;
        vertexShader_ = pixelShader_ = 0;
        renderTarget_ = &backBuffer_;
        depthTarget_ = &depthBuffer_;
        curViewport_ = {0, 0, backBuffer_.Width(), backBuffer_.Height(), 0.0f, 1.0f};
    }

    void ForwardFog() {
        if (!(kForwardMask & kForwardFog) || !g_Renderer) return;
        g_Renderer->SetFog(renderStates_[D3DRS_FOGENABLE] != 0, renderStates_[D3DRS_FOGCOLOR],
                           AsFloat(renderStates_[D3DRS_FOGSTART]), AsFloat(renderStates_[D3DRS_FOGEND]));
    }

    void ForwardCamera() {
        if (!(kForwardMask & kForwardCamera) || !g_Renderer || !haveView_ || !haveProj_) return;
        g_Renderer->SetCamera3D(reinterpret_cast<const float*>(&transforms_[D3DTS_VIEW]),
                                reinterpret_cast<const float*>(&transforms_[D3DTS_PROJECTION]));
    }

    void ApplyPresentParameters(const D3DPRESENT_PARAMETERS* pp) {
        if (!pp) return;
        D3DFORMAT fmt = pp->BackBufferFormat == D3DFMT_UNKNOWN ? D3DFMT_X8R8G8B8 : pp->BackBufferFormat;
        backBuffer_.Resize(pp->BackBufferWidth, pp->BackBufferHeight, fmt);
        depthBuffer_.Resize(pp->BackBufferWidth, pp->BackBufferHeight,
                            pp->EnableAutoDepthStencil ? pp->AutoDepthStencilFormat : D3DFMT_D16);
    }

public:
    DummyD3DDevice() { ResetState(); }

    void OnCreateDevice(UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags, const D3DPRESENT_PARAMETERS* pp) {
        creation_.AdapterOrdinal = adapter;
        creation_.DeviceType = type;
        creation_.hFocusWindow = hwnd;
        creation_.BehaviorFlags = flags;
        ApplyPresentParameters(pp);
        ResetState();
    }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    // IDirect3DDevice8
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override { return D3D_OK; }
    UINT STDMETHODCALLTYPE GetAvailableTextureMem() override { return 128 * 1024 * 1024; }
    HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = GetDummyD3D();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8* p) override {
        if (!p) return D3DERR_INVALIDCALL;
        FillCaps(p);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* m) override {
        if (!m) return D3DERR_INVALIDCALL;
        FillDisplayMode(m);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* p) override {
        if (!p) return D3DERR_INVALIDCALL;
        *p = creation_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT, UINT, IDirect3DSurface8*) override { return S_OK; }
    void STDMETHODCALLTYPE SetCursorPosition(UINT, UINT, DWORD) override {}
    BOOL STDMETHODCALLTYPE ShowCursor(BOOL) override { return FALSE; }
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain8** pp) override {
        if (pp) *pp = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* pp) override {
        ApplyPresentParameters(pp);
        ResetState();
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE Present(CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT index, D3DBACKBUFFER_TYPE, IDirect3DSurface8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        if (index != 0) {
            *pp = nullptr;
            return D3DERR_INVALIDCALL;
        }
        *pp = &backBuffer_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS* p) override {
        if (p) memset(p, 0, sizeof(*p));
        return S_OK;
    }
    void STDMETHODCALLTYPE SetGammaRamp(DWORD, CONST D3DGAMMARAMP*) override {}
    void STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP* p) override {
        if (!p) return;
        for (int i = 0; i < 256; i++) p->red[i] = p->green[i] = p->blue[i] = static_cast<WORD>(i * 257);
    }

    // Textures are handled by g_Renderer (TextureHandle); the D3D8 texture
    // interfaces are intentionally not emulated. Fail cleanly, with a NULL out
    // pointer, instead of returning S_OK with an uninitialised pointer.
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                            IDirect3DTexture8** pp) override {
        if (pp) *pp = nullptr;
        return D3DERR_NOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                                  IDirect3DVolumeTexture8** pp) override {
        if (pp) *pp = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT, UINT, DWORD, D3DFORMAT, D3DPOOL,
                                                IDirect3DCubeTexture8** pp) override {
        if (pp) *pp = nullptr;
        return E_NOTIMPL;
    }
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT length, DWORD usage, DWORD fvf, D3DPOOL pool,
                                                 IDirect3DVertexBuffer8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = new DummyD3DVertexBuffer(length, usage, fvf, pool);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT length, DWORD usage, D3DFORMAT fmt, D3DPOOL pool,
                                                IDirect3DIndexBuffer8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = new DummyD3DIndexBuffer(length, usage, fmt, pool);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE, BOOL,
                                                 IDirect3DSurface8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = new DummyD3DSurface(w, h, fmt, D3DRTYPE_SURFACE, D3DUSAGE_RENDERTARGET, D3DPOOL_DEFAULT, false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT w, UINT h, D3DFORMAT fmt, D3DMULTISAMPLE_TYPE,
                                                        IDirect3DSurface8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = new DummyD3DSurface(w, h, fmt, D3DRTYPE_SURFACE, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT w, UINT h, D3DFORMAT fmt, IDirect3DSurface8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = new DummyD3DSurface(w, h, fmt, D3DRTYPE_SURFACE, 0, D3DPOOL_SCRATCH, false);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8* src, CONST RECT* srcRects, UINT numRects,
                                        IDirect3DSurface8* dst, CONST POINT* dstPoints) override {
        if (!src || !dst) return D3DERR_INVALIDCALL;
        D3DSURFACE_DESC sd, dd;
        src->GetDesc(&sd);
        dst->GetDesc(&dd);
        // Real D3D8 rejects mismatched formats; stay lenient (as the old stub was) and skip the copy.
        if (sd.Format != dd.Format) return S_OK;
        const UINT bpp = BytesPerPixel(sd.Format);

        RECT whole = {0, 0, static_cast<LONG>(sd.Width), static_cast<LONG>(sd.Height)};
        const UINT n = (srcRects && numRects) ? numRects : 1;
        for (UINT i = 0; i < n; i++) {
            RECT r = (srcRects && numRects) ? srcRects[i] : whole;
            LONG dx = (dstPoints && numRects) ? dstPoints[i].x : r.left;
            LONG dy = (dstPoints && numRects) ? dstPoints[i].y : r.top;
            // Clip like D3D8: never read or write outside either surface.
            if (r.left < 0) { dx -= r.left; r.left = 0; }
            if (r.top < 0) { dy -= r.top; r.top = 0; }
            if (r.right > static_cast<LONG>(sd.Width)) r.right = sd.Width;
            if (r.bottom > static_cast<LONG>(sd.Height)) r.bottom = sd.Height;
            if (dx < 0) { r.left -= dx; dx = 0; }
            if (dy < 0) { r.top -= dy; dy = 0; }
            LONG w = r.right - r.left, h = r.bottom - r.top;
            if (dx + w > static_cast<LONG>(dd.Width)) w = dd.Width - dx;
            if (dy + h > static_cast<LONG>(dd.Height)) h = dd.Height - dy;
            if (w <= 0 || h <= 0) continue;

            D3DLOCKED_RECT sl, dl;
            RECT sr = {r.left, r.top, r.left + w, r.top + h};
            RECT dr = {dx, dy, dx + w, dy + h};
            if (FAILED(src->LockRect(&sl, &sr, D3DLOCK_READONLY))) continue;
            if (FAILED(dst->LockRect(&dl, &dr, 0))) {
                src->UnlockRect();
                continue;
            }
            for (LONG y = 0; y < h; y++) {
                memmove(static_cast<BYTE*>(dl.pBits) + y * dl.Pitch, static_cast<BYTE*>(sl.pBits) + y * sl.Pitch,
                        static_cast<size_t>(w) * bpp);
            }
            dst->UnlockRect();
            src->UnlockRect();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture8*, IDirect3DBaseTexture8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetFrontBuffer(IDirect3DSurface8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirect3DSurface8* rt, IDirect3DSurface8* ds) override {
        if (rt) renderTarget_ = rt;
        if (ds) depthTarget_ = ds;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirect3DSurface8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = renderTarget_;
        if (*pp) (*pp)->AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface8** pp) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = depthTarget_;
        if (*pp) (*pp)->AddRef();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE BeginScene() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EndScene() override { return S_OK; }
    // Clear is intentionally a no-op: the frame is cleared once per frame via
    // g_Renderer->Clear() in GameWindow::Render.
    HRESULT STDMETHODCALLTYPE Clear(DWORD, CONST D3DRECT*, DWORD, D3DCOLOR, float, DWORD) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE type, CONST D3DMATRIX* m) override {
        if (!m) return D3DERR_INVALIDCALL;
        transforms_[static_cast<DWORD>(type)] = *m;
        if (type == D3DTS_VIEW) {
            haveView_ = true;
            ForwardCamera();
        } else if (type == D3DTS_PROJECTION) {
            haveProj_ = true;
            ForwardCamera();
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE type, D3DMATRIX* m) override {
        if (!m) return D3DERR_INVALIDCALL;
        auto it = transforms_.find(static_cast<DWORD>(type));
        *m = (it != transforms_.end()) ? it->second : IdentityMatrix();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE SetViewport(CONST D3DVIEWPORT8* p) override {
        if (!p) return D3DERR_INVALIDCALL;
        curViewport_ = *p;
        if ((kForwardMask & kForwardViewport) && g_Renderer) {
            g_Renderer->SetGameViewport(static_cast<int>(p->X), static_cast<int>(p->Y), static_cast<int>(p->Width),
                                        static_cast<int>(p->Height), p->MinZ, p->MaxZ);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8* p) override {
        if (!p) return D3DERR_INVALIDCALL;
        *p = curViewport_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetMaterial(CONST D3DMATERIAL8* m) override {
        if (!m) return D3DERR_INVALIDCALL;
        material_ = *m;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8* m) override {
        if (!m) return D3DERR_INVALIDCALL;
        *m = material_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD i, CONST D3DLIGHT8* l) override {
        if (!l) return D3DERR_INVALIDCALL;
        lights_[i] = *l;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetLight(DWORD i, D3DLIGHT8* l) override {
        auto it = lights_.find(i);
        if (!l || it == lights_.end()) return D3DERR_INVALIDCALL;
        *l = it->second;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD i, BOOL e) override {
        lightEnable_[i] = e;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD i, BOOL* e) override {
        if (!e) return D3DERR_INVALIDCALL;
        auto it = lightEnable_.find(i);
        *e = (it != lightEnable_.end()) ? it->second : FALSE;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD i, CONST float* p) override {
        if (i >= 6 || !p) return D3DERR_INVALIDCALL;
        memcpy(clipPlanes_[i], p, sizeof(float) * 4);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD i, float* p) override {
        if (i >= 6 || !p) return D3DERR_INVALIDCALL;
        memcpy(p, clipPlanes_[i], sizeof(float) * 4);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override {
        if (static_cast<DWORD>(state) >= kMaxRenderStates) return D3DERR_INVALIDCALL;
        renderStates_[state] = value;
        switch (state) {
        case D3DRS_FOGENABLE:
        case D3DRS_FOGCOLOR:
        case D3DRS_FOGSTART:
        case D3DRS_FOGEND:
            ForwardFog();
            break;
        case D3DRS_ZWRITEENABLE:
            if ((kForwardMask & kForwardZWrite) && g_Renderer) g_Renderer->SetDepthWrite(value != 0);
            break;
        default:
            break;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE state, DWORD* value) override {
        if (!value || static_cast<DWORD>(state) >= kMaxRenderStates) return D3DERR_INVALIDCALL;
        *value = renderStates_[state];
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE BeginStateBlock() override {
        recordingBlock_ = true;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD* token) override {
        if (!token) return D3DERR_INVALIDCALL;
        recordingBlock_ = false;
        *token = nextBlockToken_++;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE, DWORD* token) override {
        if (!token) return D3DERR_INVALIDCALL;
        *token = nextBlockToken_++;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetClipStatus(CONST D3DCLIPSTATUS8* s) override {
        if (!s) return D3DERR_INVALIDCALL;
        clipStatus_ = *s;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS8* s) override {
        if (!s) return D3DERR_INVALIDCALL;
        *s = clipStatus_;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE GetTexture(DWORD stage, IDirect3DBaseTexture8** pp) override {
        if (!pp || stage >= kMaxStages) return D3DERR_INVALIDCALL;
        // Note: in the Vulkan build the "texture pointers" held by game code are
        // really TextureHandle values, so no AddRef is attempted here.
        *pp = textures_[stage];
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD stage, IDirect3DBaseTexture8* tex) override {
        if (stage >= kMaxStages) return D3DERR_INVALIDCALL;
        textures_[stage] = tex;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD* v) override {
        if (!v || stage >= kMaxStages || static_cast<DWORD>(type) >= kMaxStageStates) return D3DERR_INVALIDCALL;
        *v = stageStates_[stage][type];
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD stage, D3DTEXTURESTAGESTATETYPE type, DWORD v) override {
        if (stage >= kMaxStages || static_cast<DWORD>(type) >= kMaxStageStates) return D3DERR_INVALIDCALL;
        stageStates_[stage][type] = v;
        if (stage == 0 && type == D3DTSS_COLOROP && (kForwardMask & kForwardColorOp) && g_Renderer) {
            // DIAG(colorop): log every stage-0 COLOROP transition to verify when
            // D3DTOP_ADD actually reaches the renderer during boss-hit flashes.
            {
                static DWORD s_dbgPrevOp = 0xffffffff;
                static int s_dbgOpCount = 0;
                if (s_dbgPrevOp != v && s_dbgOpCount < 400)
                {
                    s_dbgPrevOp = v;
                    s_dbgOpCount++;
                    const char *opName = "OTHER";
                    if (v == D3DTOP_ADD) opName = "ADD";
                    else if (v == D3DTOP_MODULATE) opName = "MODULATE";
                    else if (v == D3DTOP_SELECTARG1) opName = "SELECTARG1";
                    th06::utils::LogDebug("COLOROP stage0 -> %s (count=%d)", opName, s_dbgOpCount);
                }
            }
            g_Renderer->SetColorOpAdd(v == D3DTOP_ADD);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD* passes) override {
        if (passes) *passes = 1;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetInfo(DWORD, void*, DWORD) override { return S_FALSE; }
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT, CONST PALETTEENTRY*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT, PALETTEENTRY* p) override {
        if (p) memset(p, 0, sizeof(PALETTEENTRY) * 256);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT* p) override {
        if (p) *p = 0;
        return S_OK;
    }

    // Drawing goes through g_Renderer; D3D8 draw calls are accepted and dropped.
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE, UINT, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE, UINT, CONST void*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE, UINT, UINT, UINT, CONST void*, D3DFORMAT,
                                                     CONST void*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD) override { return S_OK; }

    HRESULT STDMETHODCALLTYPE CreateVertexShader(CONST DWORD*, CONST DWORD*, DWORD* handle, DWORD) override {
        if (!handle) return D3DERR_INVALIDCALL;
        *handle = nextShaderHandle_++ | 0x80000000u; // top bit set = shader handle, not an FVF
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD h) override {
        vertexShader_ = h;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD* h) override {
        if (!h) return D3DERR_INVALIDCALL;
        *h = vertexShader_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(DWORD reg, CONST void* data, DWORD count) override {
        if (!data || reg >= 96 || reg + count > 96) return D3DERR_INVALIDCALL;
        memcpy(vsConst_[reg], data, sizeof(float) * 4 * count);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(DWORD reg, void* data, DWORD count) override {
        if (!data || reg >= 96 || reg + count > 96) return D3DERR_INVALIDCALL;
        memcpy(data, vsConst_[reg], sizeof(float) * 4 * count);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetVertexShaderDeclaration(DWORD, void*, DWORD* size) override {
        if (size) *size = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetVertexShaderFunction(DWORD, void*, DWORD* size) override {
        if (size) *size = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT n, IDirect3DVertexBuffer8* vb, UINT stride) override {
        if (n >= 16) return D3DERR_INVALIDCALL;
        streamSource_[n] = vb;
        streamStride_[n] = stride;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT n, IDirect3DVertexBuffer8** pp, UINT* stride) override {
        if (n >= 16 || !pp) return D3DERR_INVALIDCALL;
        *pp = streamSource_[n];
        if (*pp) (*pp)->AddRef();
        if (stride) *stride = streamStride_[n];
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer8* ib, UINT base) override {
        indices_ = ib;
        baseVertexIndex_ = base;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer8** pp, UINT* base) override {
        if (!pp) return D3DERR_INVALIDCALL;
        *pp = indices_;
        if (*pp) (*pp)->AddRef();
        if (base) *base = baseVertexIndex_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreatePixelShader(CONST DWORD*, DWORD* handle) override {
        if (!handle) return D3DERR_INVALIDCALL;
        *handle = nextShaderHandle_++;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD h) override {
        pixelShader_ = h;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD* h) override {
        if (!h) return D3DERR_INVALIDCALL;
        *h = pixelShader_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DeletePixelShader(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(DWORD reg, CONST void* data, DWORD count) override {
        if (!data || reg >= 8 || reg + count > 8) return D3DERR_INVALIDCALL;
        memcpy(psConst_[reg], data, sizeof(float) * 4 * count);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(DWORD reg, void* data, DWORD count) override {
        if (!data || reg >= 8 || reg + count > 8) return D3DERR_INVALIDCALL;
        memcpy(data, psConst_[reg], sizeof(float) * 4 * count);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPixelShaderFunction(DWORD, void*, DWORD* size) override {
        if (size) *size = 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT, CONST float*, CONST D3DRECTPATCH_INFO*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT, CONST float*, CONST D3DTRIPATCH_INFO*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT) override { return S_OK; }
};

// One device for the whole process: CreateDevice() and GetDummyD3DDevice()
// must hand out the same object, otherwise state set through one is invisible
// to the other.
static DummyD3DDevice& TheDevice() {
    static DummyD3DDevice device;
    return device;
}

#define DEFINE_RESOURCE_GETDEVICE(Class)                           \
    HRESULT STDMETHODCALLTYPE Class::GetDevice(IDirect3DDevice8** pp) { \
        if (!pp) return D3DERR_INVALIDCALL;                        \
        *pp = &TheDevice();                                        \
        return S_OK;                                               \
    }
DEFINE_RESOURCE_GETDEVICE(DummyD3DVertexBuffer)
DEFINE_RESOURCE_GETDEVICE(DummyD3DIndexBuffer)
DEFINE_RESOURCE_GETDEVICE(DummyD3DSurface)

// ---------------------------------------------------------------------------
// IDirect3D8
// ---------------------------------------------------------------------------

class DummyD3D : public IDirect3D8 {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void*) override { return S_OK; }
    UINT STDMETHODCALLTYPE GetAdapterCount() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT adapter, DWORD, D3DADAPTER_IDENTIFIER8* id) override {
        if (adapter != 0 || !id) return D3DERR_INVALIDCALL;
        memset(id, 0, sizeof(*id));
        strncpy(id->Driver, "vulkan_compat", sizeof(id->Driver) - 1);
        strncpy(id->Description, "Vulkan D3D8 compatibility layer", sizeof(id->Description) - 1);
        return S_OK;
    }
    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT adapter) override { return adapter == 0 ? 1 : 0; }
    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT adapter, UINT mode, D3DDISPLAYMODE* m) override {
        if (adapter != 0 || mode != 0 || !m) return D3DERR_INVALIDCALL;
        FillDisplayMode(m);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT adapter, D3DDISPLAYMODE* m) override {
        if (adapter != 0 || !m) return D3DERR_INVALIDCALL;
        FillDisplayMode(m);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) override {
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT, BOOL,
                                                         D3DMULTISAMPLE_TYPE type) override {
        return type == D3DMULTISAMPLE_NONE ? S_OK : D3DERR_NOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) override {
        return S_OK;
    }
    // Same caps as the device reports (this used to return an all-zero struct).
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT, D3DDEVTYPE, D3DCAPS8* p) override {
        if (!p) return D3DERR_INVALIDCALL;
        FillCaps(p);
        return S_OK;
    }
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT) override { return NULL; }
    HRESULT STDMETHODCALLTYPE CreateDevice(UINT adapter, D3DDEVTYPE type, HWND hwnd, DWORD flags,
                                           D3DPRESENT_PARAMETERS* pp, IDirect3DDevice8** ppDev) override {
        if (!ppDev) return D3DERR_INVALIDCALL;
        TheDevice().OnCreateDevice(adapter, type, hwnd, flags, pp);
        *ppDev = &TheDevice();
        return S_OK;
    }
};

IDirect3D8* GetDummyD3D() {
    static DummyD3D d3d;
    return &d3d;
}

IDirect3DDevice8* GetDummyD3DDevice() {
    return &TheDevice();
}

} // namespace th06

#endif // USE_VULKAN
