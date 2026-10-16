#ifdef USE_VULKAN

#include "dummy_d3d8.hpp"
#include <cstring>

namespace th06 {

class DummyD3DVertexBuffer : public IDirect3DVertexBuffer8 {
    char buf_[65536] = {};
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG   STDMETHODCALLTYPE AddRef()  override { return 1; }
    ULONG   STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, CONST void*, DWORD, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, void*, DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override { return S_OK; }
    DWORD   STDMETHODCALLTYPE SetPriority(DWORD) override { return 0; }
    DWORD   STDMETHODCALLTYPE GetPriority() override { return 0; }
    void    STDMETHODCALLTYPE PreLoad() override {}
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override { return D3DRTYPE_VERTEXBUFFER; }
    HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock, BYTE** ppbData, DWORD) override {
        if (ppbData) *ppbData = reinterpret_cast<BYTE*>(buf_ + OffsetToLock);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE Unlock() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC* pDesc) override {
        if (pDesc) memset(pDesc, 0, sizeof(*pDesc));
        return S_OK;
    }
};

class DummyD3DSurface : public IDirect3DSurface8 {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG   STDMETHODCALLTYPE AddRef()  override { return 1; }
    ULONG   STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID, CONST void*, DWORD, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID, void*, DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID, void**) override { return E_NOINTERFACE; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC* desc) override {
        if (desc) {
            memset(desc, 0, sizeof(*desc));
            desc->Width = 640;
            desc->Height = 480;
            desc->Format = D3DFMT_X8R8G8B8;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLR, CONST RECT*, DWORD) override {
        static char buf[640 * 480 * 4] = {};
        if (pLR) {
            pLR->Pitch = 640 * 4;
            pLR->pBits = buf;
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE UnlockRect() override { return S_OK; }
};

class DummyD3D;
IDirect3D8* GetDummyD3D();

class DummyD3DDevice : public IDirect3DDevice8 {
    DummyD3DSurface dummySurface_;
    DummyD3DVertexBuffer dummyVB_;
public:
    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG   STDMETHODCALLTYPE AddRef()  override { return 1; }
    ULONG   STDMETHODCALLTYPE Release() override { return 1; }

    // IDirect3DDevice8
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override { return D3D_OK; }
    UINT    STDMETHODCALLTYPE GetAvailableTextureMem() override { return 128 * 1024 * 1024; }
    HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8** pp) override { if (pp) *pp = GetDummyD3D(); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8* p) override {
        if (p) {
            memset(p, 0, sizeof(*p));
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
            p->MaxActiveLights = 8;
            p->MaxUserClipPlanes = 6;
            p->MaxVertexBlendMatrices = 4;
            p->MaxStreams = 16;
            p->MaxStreamStride = 256;
            p->VertexShaderVersion = D3DVS_VERSION(1, 1);
            p->PixelShaderVersion = D3DPS_VERSION(1, 4);
        }
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE* m) override {
        if(m){m->Width=640;m->Height=480;m->RefreshRate=60;m->Format=D3DFMT_X8R8G8B8;} return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetCursorProperties(UINT, UINT, IDirect3DSurface8*) override { return S_OK; }
    void    STDMETHODCALLTYPE SetCursorPosition(UINT, UINT, DWORD) override {}
    BOOL    STDMETHODCALLTYPE ShowCursor(BOOL) override { return FALSE; }
    HRESULT STDMETHODCALLTYPE CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS*, IDirect3DSwapChain8**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS*) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE Present(CONST RECT*, CONST RECT*, HWND, CONST RGNDATA*) override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT, D3DBACKBUFFER_TYPE, IDirect3DSurface8** pp) override {
        if (pp) *pp = &dummySurface_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRasterStatus(D3DRASTER_STATUS*) override { return S_OK; }
    void    STDMETHODCALLTYPE SetGammaRamp(DWORD, CONST D3DGAMMARAMP*) override {}
    void    STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP*) override {}
    HRESULT STDMETHODCALLTYPE CreateTexture(UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DTexture8**) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CreateVolumeTexture(UINT, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DVolumeTexture8**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateCubeTexture(UINT, UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DCubeTexture8**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateVertexBuffer(UINT, DWORD, DWORD, D3DPOOL, IDirect3DVertexBuffer8** ppVB) override {
        if (ppVB) *ppVB = &dummyVB_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateIndexBuffer(UINT, DWORD, D3DFORMAT, D3DPOOL, IDirect3DIndexBuffer8**) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CreateRenderTarget(UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, BOOL, IDirect3DSurface8** pp) override {
        if (pp) *pp = &dummySurface_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(UINT, UINT, D3DFORMAT, D3DMULTISAMPLE_TYPE, IDirect3DSurface8** pp) override {
        if (pp) *pp = &dummySurface_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateImageSurface(UINT, UINT, D3DFORMAT, IDirect3DSurface8** pp) override {
        if (pp) *pp = &dummySurface_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8*, CONST RECT*, UINT, IDirect3DSurface8*, CONST POINT*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UpdateTexture(IDirect3DBaseTexture8*, IDirect3DBaseTexture8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetFrontBuffer(IDirect3DSurface8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetRenderTarget(IDirect3DSurface8*, IDirect3DSurface8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetRenderTarget(IDirect3DSurface8** pp) override {
        if (pp) *pp = &dummySurface_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface8** pp) override {
        if (pp) *pp = &dummySurface_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE BeginScene() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EndScene() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE Clear(DWORD, CONST D3DRECT*, DWORD, D3DCOLOR, float, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE, D3DMATRIX*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX*) override { return S_OK; }
    D3DVIEWPORT8 curViewport_ = { 0, 0, 640, 480, 0.0f, 1.0f };
    HRESULT STDMETHODCALLTYPE SetViewport(CONST D3DVIEWPORT8* p) override {
        if (p) curViewport_ = *p;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8* p) override {
        if (p) *p = curViewport_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE SetMaterial(CONST D3DMATERIAL8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetLight(DWORD, CONST D3DLIGHT8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetLight(DWORD, D3DLIGHT8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE LightEnable(DWORD, BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD, BOOL*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD, CONST float*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD, float*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE, DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE BeginStateBlock() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE, DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetClipStatus(CONST D3DCLIPSTATUS8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTexture(DWORD, IDirect3DBaseTexture8**) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetTexture(DWORD, IDirect3DBaseTexture8*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD, D3DTEXTURESTAGESTATETYPE, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetInfo(DWORD, void*, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPaletteEntries(UINT, CONST PALETTEENTRY*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT, PALETTEENTRY*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetCurrentTexturePalette(UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetCurrentTexturePalette(UINT*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE, UINT, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE, UINT, UINT, UINT, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE, UINT, CONST void*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE, UINT, UINT, UINT, CONST void*, D3DFORMAT, CONST void*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ProcessVertices(UINT, UINT, UINT, IDirect3DVertexBuffer8*, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CreateVertexShader(CONST DWORD*, CONST DWORD*, DWORD*, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(DWORD, CONST void*, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(DWORD, void*, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderDeclaration(DWORD, void*, DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetVertexShaderFunction(DWORD, void*, DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetStreamSource(UINT, IDirect3DVertexBuffer8*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetStreamSource(UINT, IDirect3DVertexBuffer8**, UINT*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer8*, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer8**, UINT*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CreatePixelShader(CONST DWORD*, DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DeletePixelShader(DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(DWORD, CONST void*, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(DWORD, void*, DWORD) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetPixelShaderFunction(DWORD, void*, DWORD*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawRectPatch(UINT, CONST float*, CONST D3DRECTPATCH_INFO*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DrawTriPatch(UINT, CONST float*, CONST D3DTRIPATCH_INFO*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE DeletePatch(UINT) override { return S_OK; }
};

class DummyD3D : public IDirect3D8 {
    DummyD3DDevice dummyDevice_;
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { return E_NOINTERFACE; }
    ULONG   STDMETHODCALLTYPE AddRef()  override { return 1; }
    ULONG   STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void*) override { return S_OK; }
    UINT    STDMETHODCALLTYPE GetAdapterCount() override { return 1; }
    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT, DWORD, D3DADAPTER_IDENTIFIER8*) override { return S_OK; }
    UINT    STDMETHODCALLTYPE GetAdapterModeCount(UINT) override { return 1; }
    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT, UINT, D3DDISPLAYMODE* m) override {
        if(m){m->Width=640;m->Height=480;m->RefreshRate=60;m->Format=D3DFMT_X8R8G8B8;} return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT, D3DDISPLAYMODE* m) override {
        if(m){m->Width=640;m->Height=480;m->RefreshRate=60;m->Format=D3DFMT_X8R8G8B8;} return S_OK;
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, BOOL) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT, D3DDEVTYPE, D3DFORMAT, DWORD, D3DRESOURCETYPE, D3DFORMAT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT, D3DDEVTYPE, D3DFORMAT, BOOL, D3DMULTISAMPLE_TYPE) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT, D3DDEVTYPE, D3DFORMAT, D3DFORMAT, D3DFORMAT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT, D3DDEVTYPE, D3DCAPS8* p) override { if(p) memset(p,0,sizeof(*p)); return S_OK; }
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT) override { return NULL; }
    HRESULT STDMETHODCALLTYPE CreateDevice(UINT, D3DDEVTYPE, HWND, DWORD,
                                           D3DPRESENT_PARAMETERS*, IDirect3DDevice8** ppDev) override {
        if (ppDev) *ppDev = &dummyDevice_;
        return S_OK;
    }
};

static DummyD3D s_dummyD3D;
static DummyD3DDevice s_dummyD3DDevice;

IDirect3D8* GetDummyD3D() {
    return &s_dummyD3D;
}

IDirect3DDevice8* GetDummyD3DDevice() {
    return &s_dummyD3DDevice;
}

} // namespace th06

#endif // USE_VULKAN
