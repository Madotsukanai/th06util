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
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> cpuData;
    bool dirty = false;
    bool valid = false;
};

struct Batch2D {
    TextureHandle texture = INVALID_TEXTURE;
    BlendMode blendMode = BLEND_ALPHA;
    uint32_t indexStart = 0;
    uint32_t indexCount = 0;
};

struct PushConstants2D {
    float projMat[16];
    float colorKey[4];
    float colorKeyEnable;
    float pad[3];
};

struct PushConstants3D {
    float viewProjMat[16];
    float fogColor[4];
    float fogDist[2]; // x=near, y=far
    float fogEnable;
    float pad;
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

    TextureHandle CreateTexture(uint32_t width, uint32_t height) override;
    TextureHandle CreateTextureFromMemory(const void* data, size_t size, uint32_t colorKey = 0) override;
    bool LockTexture(TextureHandle handle, SurfaceDesc* desc) override;
    void UnlockTexture(TextureHandle handle) override;
    void ReleaseTexture(TextureHandle handle) override;
    TextureHandle GetWhiteTexture() const override { return whiteTexture_; }

    void DrawQuad2D(TextureHandle tex, const Vertex2D v[4], BlendMode blend) override;
    void DrawColoredQuad2D(const Vertex2D v[4], BlendMode blend) override;
    void DrawTriangleStrip2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) override;
    void DrawTriangleFan2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) override;

    void SetCamera3D(const float view[16], const float proj[16]) override;
    void SetFog(bool enable, uint32_t color, float nearZ, float farZ) override;
    void DrawQuad3D(const float world[16], TextureHandle tex, const Vertex3D v[4], BlendMode blend) override;

    void CaptureScreen(TextureHandle target) override;

private:
    bool InitVulkan();
    bool CreateSwapchain();
    bool CreateRenderPass();
    bool CreateDepthBuffer();
    bool CreateFramebuffers();
    bool CreateDescriptorPool();
    bool CreatePipelines();
    bool CreateFrameResources();
    void CleanupSwapchain();

    void FlushBatch2D();
    void FlushBatch3D();
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

    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthImageView_ = VK_NULL_HANDLE;
    VkFormat depthFormat_ = VK_FORMAT_D32_SFLOAT;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout descSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;

    // 2D Sprite Pipelines
    VkPipelineLayout spritePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline spritePipelineAlpha_ = VK_NULL_HANDLE;
    VkPipeline spritePipelineAdd_ = VK_NULL_HANDLE;
    VkPipeline spritePipelineSub_ = VK_NULL_HANDLE;

    // 3D Stage Pipeline
    VkPipelineLayout stagePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline stagePipelineAlpha_ = VK_NULL_HANDLE;
    VkPipeline stagePipelineAdd_ = VK_NULL_HANDLE;

    std::array<FrameResource, MAX_FRAMES_IN_FLIGHT> frames_;
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;
    bool inScene_ = false;
    bool frameReady_ = false;
    float clearColor_[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    // Batching state
    uint32_t vertexCount_ = 0;
    uint32_t indexCount_ = 0;
    std::vector<Batch2D> batches2D_;

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
};

} // namespace th06
