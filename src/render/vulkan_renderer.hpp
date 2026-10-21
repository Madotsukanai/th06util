#pragma once

#include "renderer.hpp"
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>
#include <vector>
#include <array>
#include <string>

namespace th06 {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
constexpr uint32_t MAX_VERTICES_PER_FRAME = 65536;
constexpr uint32_t MAX_INDICES_PER_FRAME = 98304;

struct TextureResource {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    // REPEAT sampler set, also used by the 3D stage path (its quads tile).
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    // Sampler set for the 2D path; selects CLAMP_TO_EDGE for textures whose
    // sprites span a texture edge (repeat would teleport oversampling digs).
    VkDescriptorSet descriptorSet2D = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> cpuData;
    bool dirty = false;
    bool valid = false;  // GPU resources exist
    bool inUse = false;  // handle is allocated (even if not uploaded yet)
    bool clampToEdge = false;  // CLAMP_TO_EDGE sampler (full-coverage sprites)
};

struct Batch2D {
    TextureHandle texture = INVALID_TEXTURE;
    BlendMode blendMode = BLEND_ALPHA;
    bool colorOpAdd = false;
    // Vertices are bump-allocated by byte offset (2D and 3D vertices have different
    // strides, so a shared vertex index would make them overwrite each other).
    // Indices are local to the batch.
    uint32_t vertexByteOffset = 0;
    uint32_t vertexCount = 0;
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
};

struct PushConstants2D {
    float projMat[16];
    float colorKey[4];
    float colorKeyEnable;
    float colorOpAdd;
    // D3D8 texel-center origin convention: see sprite.frag. The renderer sets
    // these to (0.5/width, 0.5/height) of the texture bound to the batch.
    float halfTexelU;
    float halfTexelV;
};

struct PushConstants3D {
    float viewProjMat[16];
    float fogColor[4];
    float fogDist[2]; // x=near, y=far
    float fogEnable;
    float colorOpAdd;
    // D3D8 texel-center origin convention: identical to sprite.frag. The 3D
    // stage quads (Draw3) sample the same anm textures and need the same
    // (0.5/width, 0.5/height) correction; without it every Draw3 quad samples
    // halfway between two texels and blends in the neighbouring texel row
    // (visible as dark-edge/blur corruption on the hit sparks, stage sprites).
    float halfTexelU;
    float halfTexelV;
};

struct FrameResource {
    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    void* mappedVertices = nullptr;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    void* mappedIndices = nullptr;
};

class VulkanRenderer : public IRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer() override;

    bool Initialize(void* windowHandle, int width, int height, bool fullscreen) override;
    void Shutdown() override;
    void OnWindowResize(int width, int height) override;

    void BeginScene() override;
    void EndScene() override;
    void Present() override;
    void Clear(uint32_t argb, bool clearDepth = true) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetGameViewport(int x, int y, int width, int height, float minZ, float maxZ) override;

    TextureHandle CreateTexture(uint32_t width, uint32_t height, bool clampToEdge = false) override;
    TextureHandle CreateTextureFromMemory(const void* data, size_t size, uint32_t colorKey = 0,
                                          bool clampToEdge = false) override;
    bool LockTexture(TextureHandle handle, SurfaceDesc* desc) override;
    void UnlockTexture(TextureHandle handle) override;
    void ReleaseTexture(TextureHandle handle) override;
    TextureHandle GetWhiteTexture() const override { return whiteTexture_; }

    bool LoadSurfaceImage(int index, const void* data, size_t size, uint32_t* outWidth,
                          uint32_t* outHeight) override;
    void DrawSurfaceRegion(int index, int dstX, int dstY, int srcX, int srcY, int srcW,
                           int srcH) override;
    void ReleaseSurfaceImage(int index) override;
    void BlitPixelsToTexture(TextureHandle tex, int dstX, int dstY, int dstW, int dstH,
                             const uint32_t* argbSrc, int srcW, int srcH) override;

    void DrawQuad2D(TextureHandle tex, const Vertex2D v[4], BlendMode blend) override;
    void DrawColoredQuad2D(const Vertex2D v[4], BlendMode blend) override;
    void DrawTriangleStrip2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) override;
    void DrawTriangleFan2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) override;

    void SetCamera3D(const float view[16], const float proj[16]) override;
    void SetFog(bool enable, uint32_t color, float nearZ, float farZ) override;
    void DrawQuad3D(const float world[16], TextureHandle tex, const Vertex3D v[4], BlendMode blend) override;

    void SetColorOpAdd(bool enable) override { colorOpAdd_ = enable; }
    void SetDepthWrite(bool enable) override { depthWrite_ = enable; }

    void CaptureScreen(TextureHandle target, int left, int top, int width, int height) override;
    bool ReadGameImage(std::vector<uint32_t>& argbOut) override;

    void CreateVerticalComposition(int width, int height) override;
    void ReleaseVerticalComposition() override;
    void SetVerticalComposition(bool enabled) override;
    void SetVerticalInGame(bool inGame) override;
    void SetVerticalFlip180(bool flip) override;
    void SetVerticalTransitionBlack(bool black) override;

private:
    bool EnsureVerticalObjects(int width, int height);
    bool InitVulkan();
    bool CreateSwapchain();
    bool CreateRenderPass();
    bool CreateDepthBuffer();
    bool CreateFramebuffers();
    bool CreateDescriptorPool();
    bool CreatePipelines();
    bool CreateFrameResources();
    void CleanupSwapchain();
    bool RecreateSwapchain();
    void UpdateDestRect();
    void ReleaseStaging();
    bool EnsureStaging(VkDeviceSize bytes);
    // Copies a rect of the current swapchain image into the staging buffer
    // (mapped as ARGB in stagingMapped_) and returns true on success.
    bool CopySwapchainRegion(int ix0, int iy0, int iw, int ih, VkDeviceSize* outSize);

    void FlushBatch2D();
    // Applies the letter-boxed game viewport (or the current game viewport
    // clipped to it) as the dynamic viewport+scissor.
    void ApplyDrawViewport(bool useGameViewport, float minZ, float maxZ);
    void ApplyGameViewport();
    void UploadTextureGPU(TextureHandle handle);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    SDL_Window* window_ = nullptr;
    int windowWidth_ = 640;
    int windowHeight_ = 480;

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily_ = 0;
    uint32_t presentQueueFamily_ = 0;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D swapchainExtent_{ 640, 480 };
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    bool swapchainDirty_ = false;

    // The 640x480 game image is letter-boxed (aspect preserved) into the swapchain.
    float destX_ = 0.0f;
    float destY_ = 0.0f;
    float destW_ = 640.0f;
    float destH_ = 480.0f;

    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_D32_SFLOAT;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    // Variant used when Clear(false) restarts a scene: the colour attachment
    // is cleared but the depth buffer keeps its contents (LOAD_OP_LOAD).
    VkRenderPass renderPassResume_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkSampler samplerClamp_ = VK_NULL_HANDLE;
    // NEAREST + clamp: used by the vertical composition so the rotated portrait
    // blits sample whole source pixels (matches the fork's CPU LockRect blit).
    VkSampler samplerVerticalNearest_ = VK_NULL_HANDLE;
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;

    // 2D Sprite Pipelines
    VkPipelineLayout spritePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline spritePipelineAlpha_ = VK_NULL_HANDLE;
    VkPipeline spritePipelineAdd_ = VK_NULL_HANDLE;
    VkPipeline spritePipelineSub_ = VK_NULL_HANDLE;

    // 3D Stage Pipeline (alpha / add / subtract, depth write on / off)
    VkPipelineLayout stagePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline stagePipelineAlpha_ = VK_NULL_HANDLE;
    VkPipeline stagePipelineAdd_ = VK_NULL_HANDLE;
    VkPipeline stagePipelineSub_ = VK_NULL_HANDLE;
    VkPipeline stagePipelineAlphaNoZ_ = VK_NULL_HANDLE;
    VkPipeline stagePipelineAddNoZ_ = VK_NULL_HANDLE;

    // ── Vertical (portrait) mode ──
    // Offscreen (640 x 648) target the whole scene renders into while the
    // vertical mode draws; Present() then composes the rotated image onto the
    // swapchain. See shaders/vertical.frag for the exact mapping.
    int verticalWidth_ = 0;
    int verticalHeight_ = 0;
    VkRenderPass renderPassVertical_ = VK_NULL_HANDLE;
    VkRenderPass renderPassVerticalResume_ = VK_NULL_HANDLE;
    VkImage verticalColorImage_ = VK_NULL_HANDLE;
    VkDeviceMemory verticalColorMemory_ = VK_NULL_HANDLE;
    VkImageView verticalColorView_ = VK_NULL_HANDLE;
    VkImage verticalDepthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory verticalDepthMemory_ = VK_NULL_HANDLE;
    VkImageView verticalDepthView_ = VK_NULL_HANDLE;
    VkFramebuffer verticalFramebuffer_ = VK_NULL_HANDLE;
    VkPipelineLayout composePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline composePipeline_ = VK_NULL_HANDLE;
    VkDescriptorSet verticalDescSet_ = VK_NULL_HANDLE;
    bool verticalCompositionBuilt_ = false;
    bool verticalCompositionActive_ = false;
    bool verticalCompositionRequested_ = false;
    bool verticalCompositionInGame_ = false;
    bool verticalCompositionFlip180_ = false;
    bool verticalCompositionTransitionBlack_ = false;

    std::array<FrameResource, MAX_FRAMES_IN_FLIGHT> frames_;
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;
    bool inScene_ = false;
    bool frameReady_ = false;
    // True while a render pass is open on the current command buffer.
    bool passActive_ = false;
    float clearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    // Whether the last Clear() asked for a depth clear too.
    bool clearDepth_ = true;

    // Persistent staging buffer for texture uploads.
    VkBuffer stagingBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory_ = VK_NULL_HANDLE;
    void* stagingMapped_ = nullptr;
    VkDeviceSize stagingCapacity_ = 0;
    VkFence uploadFence_ = VK_NULL_HANDLE;

    // Batching state
    // 2D and 3D vertices are both 24 bytes and share one bump-allocated
    // buffer per frame, addressed by vertex index.
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
    std::vector<Batch2D> batches2D_;

    // Fixed-function state emulation
    bool colorOpAdd_ = false;
    bool depthWrite_ = true;

    // Game viewport (arcade region) for 3D stage rendering, in 640x480 space
    int gameViewportX_ = 0;
    int gameViewportY_ = 0;
    int gameViewportW_ = 640;
    int gameViewportH_ = 480;
    float gameViewportMinZ_ = 0.0f;
    float gameViewportMaxZ_ = 1.0f;

    // 3D state
    float viewMatrix_[16]{};
    float projMatrix_[16]{};
    float viewProjMatrix_[16]{};
    bool fogEnable_ = false;
    uint32_t fogColor_ = 0;
    float fogNear_ = 0.0f;
    float fogFar_ = 0.0f;

    // Textures
    std::vector<TextureResource> textures_;
    TextureHandle whiteTexture_ = INVALID_TEXTURE;

    // Offscreen surfaces (title/select/result/ending backgrounds)
    struct SurfaceImage {
        TextureHandle tex = INVALID_TEXTURE;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    std::vector<SurfaceImage> surfaceImages_;
};

} // namespace th06
