#include "vulkan_renderer.hpp"
#include "shaders_embedded.hpp"
#include <SDL2/SDL_vulkan.h>
#define STB_IMAGE_IMPLEMENTATION
#include "../../3rdparty/stb_image.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <optional>

namespace th06 {

IRenderer* g_Renderer = nullptr;

// ── Global Vulkan Function Pointers ──────────────────────────
#define VK_FUNC(name) static PFN_##name p##name = nullptr;

VK_FUNC(vkGetInstanceProcAddr)
VK_FUNC(vkCreateInstance)
VK_FUNC(vkDestroyInstance)
VK_FUNC(vkEnumeratePhysicalDevices)
VK_FUNC(vkGetPhysicalDeviceProperties)
VK_FUNC(vkGetPhysicalDeviceFeatures)
VK_FUNC(vkGetPhysicalDeviceQueueFamilyProperties)
VK_FUNC(vkGetPhysicalDeviceMemoryProperties)
VK_FUNC(vkGetPhysicalDeviceSurfaceSupportKHR)
VK_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
VK_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR)
VK_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR)
VK_FUNC(vkCreateDevice)
VK_FUNC(vkDestroyDevice)
VK_FUNC(vkGetDeviceProcAddr)
VK_FUNC(vkGetDeviceQueue)
VK_FUNC(vkCreateSwapchainKHR)
VK_FUNC(vkDestroySwapchainKHR)
VK_FUNC(vkGetSwapchainImagesKHR)
VK_FUNC(vkAcquireNextImageKHR)
VK_FUNC(vkQueuePresentKHR)
VK_FUNC(vkCreateImageView)
VK_FUNC(vkDestroyImageView)
VK_FUNC(vkCreateRenderPass)
VK_FUNC(vkDestroyRenderPass)
VK_FUNC(vkCreateFramebuffer)
VK_FUNC(vkDestroyFramebuffer)
VK_FUNC(vkCreateCommandPool)
VK_FUNC(vkDestroyCommandPool)
VK_FUNC(vkAllocateCommandBuffers)
VK_FUNC(vkFreeCommandBuffers)
VK_FUNC(vkBeginCommandBuffer)
VK_FUNC(vkEndCommandBuffer)
VK_FUNC(vkResetCommandBuffer)
VK_FUNC(vkCmdBeginRenderPass)
VK_FUNC(vkCmdEndRenderPass)
VK_FUNC(vkCmdBindPipeline)
VK_FUNC(vkCmdBindDescriptorSets)
VK_FUNC(vkCmdBindVertexBuffers)
VK_FUNC(vkCmdBindIndexBuffer)
VK_FUNC(vkCmdSetViewport)
VK_FUNC(vkCmdSetScissor)
VK_FUNC(vkCmdDrawIndexed)
VK_FUNC(vkCmdPushConstants)
VK_FUNC(vkCmdCopyBufferToImage)
VK_FUNC(vkCmdPipelineBarrier)
VK_FUNC(vkCreateBuffer)
VK_FUNC(vkDestroyBuffer)
VK_FUNC(vkGetBufferMemoryRequirements)
VK_FUNC(vkAllocateMemory)
VK_FUNC(vkFreeMemory)
VK_FUNC(vkBindBufferMemory)
VK_FUNC(vkMapMemory)
VK_FUNC(vkUnmapMemory)
VK_FUNC(vkFlushMappedMemoryRanges)
VK_FUNC(vkCreateImage)
VK_FUNC(vkDestroyImage)
VK_FUNC(vkGetImageMemoryRequirements)
VK_FUNC(vkBindImageMemory)
VK_FUNC(vkCreateShaderModule)
VK_FUNC(vkDestroyShaderModule)
VK_FUNC(vkCreateDescriptorSetLayout)
VK_FUNC(vkDestroyDescriptorSetLayout)
VK_FUNC(vkCreatePipelineLayout)
VK_FUNC(vkDestroyPipelineLayout)
VK_FUNC(vkCreateGraphicsPipelines)
VK_FUNC(vkDestroyPipeline)
VK_FUNC(vkCreateDescriptorPool)
VK_FUNC(vkDestroyDescriptorPool)
VK_FUNC(vkAllocateDescriptorSets)
VK_FUNC(vkFreeDescriptorSets)
VK_FUNC(vkUpdateDescriptorSets)
VK_FUNC(vkCreateSampler)
VK_FUNC(vkDestroySampler)
VK_FUNC(vkCreateSemaphore)
VK_FUNC(vkDestroySemaphore)
VK_FUNC(vkCreateFence)
VK_FUNC(vkDestroyFence)
VK_FUNC(vkWaitForFences)
VK_FUNC(vkResetFences)
VK_FUNC(vkQueueSubmit)
VK_FUNC(vkQueueWaitIdle)
VK_FUNC(vkDeviceWaitIdle)

#define vkCreateInstance pvkCreateInstance
#define vkDestroyInstance pvkDestroyInstance
#define vkEnumeratePhysicalDevices pvkEnumeratePhysicalDevices
#define vkGetPhysicalDeviceProperties pvkGetPhysicalDeviceProperties
#define vkGetPhysicalDeviceFeatures pvkGetPhysicalDeviceFeatures
#define vkGetPhysicalDeviceQueueFamilyProperties pvkGetPhysicalDeviceQueueFamilyProperties
#define vkGetPhysicalDeviceMemoryProperties pvkGetPhysicalDeviceMemoryProperties
#define vkGetPhysicalDeviceSurfaceSupportKHR pvkGetPhysicalDeviceSurfaceSupportKHR
#define vkGetPhysicalDeviceSurfaceCapabilitiesKHR pvkGetPhysicalDeviceSurfaceCapabilitiesKHR
#define vkGetPhysicalDeviceSurfaceFormatsKHR pvkGetPhysicalDeviceSurfaceFormatsKHR
#define vkGetPhysicalDeviceSurfacePresentModesKHR pvkGetPhysicalDeviceSurfacePresentModesKHR
#define vkCreateDevice pvkCreateDevice
#define vkDestroyDevice pvkDestroyDevice
#define vkGetDeviceProcAddr pvkGetDeviceProcAddr
#define vkGetDeviceQueue pvkGetDeviceQueue
#define vkCreateSwapchainKHR pvkCreateSwapchainKHR
#define vkDestroySwapchainKHR pvkDestroySwapchainKHR
#define vkGetSwapchainImagesKHR pvkGetSwapchainImagesKHR
#define vkAcquireNextImageKHR pvkAcquireNextImageKHR
#define vkQueuePresentKHR pvkQueuePresentKHR
#define vkCreateImageView pvkCreateImageView
#define vkDestroyImageView pvkDestroyImageView
#define vkCreateRenderPass pvkCreateRenderPass
#define vkDestroyRenderPass pvkDestroyRenderPass
#define vkCreateFramebuffer pvkCreateFramebuffer
#define vkDestroyFramebuffer pvkDestroyFramebuffer
#define vkCreateCommandPool pvkCreateCommandPool
#define vkDestroyCommandPool pvkDestroyCommandPool
#define vkAllocateCommandBuffers pvkAllocateCommandBuffers
#define vkFreeCommandBuffers pvkFreeCommandBuffers
#define vkBeginCommandBuffer pvkBeginCommandBuffer
#define vkEndCommandBuffer pvkEndCommandBuffer
#define vkResetCommandBuffer pvkResetCommandBuffer
#define vkCmdBeginRenderPass pvkCmdBeginRenderPass
#define vkCmdEndRenderPass pvkCmdEndRenderPass
#define vkCmdBindPipeline pvkCmdBindPipeline
#define vkCmdBindDescriptorSets pvkCmdBindDescriptorSets
#define vkCmdBindVertexBuffers pvkCmdBindVertexBuffers
#define vkCmdBindIndexBuffer pvkCmdBindIndexBuffer
#define vkCmdSetViewport pvkCmdSetViewport
#define vkCmdSetScissor pvkCmdSetScissor
#define vkCmdDrawIndexed pvkCmdDrawIndexed
#define vkCmdPushConstants pvkCmdPushConstants
#define vkCmdCopyBufferToImage pvkCmdCopyBufferToImage
#define vkCmdPipelineBarrier pvkCmdPipelineBarrier
#define vkCreateBuffer pvkCreateBuffer
#define vkDestroyBuffer pvkDestroyBuffer
#define vkGetBufferMemoryRequirements pvkGetBufferMemoryRequirements
#define vkAllocateMemory pvkAllocateMemory
#define vkFreeMemory pvkFreeMemory
#define vkBindBufferMemory pvkBindBufferMemory
#define vkMapMemory pvkMapMemory
#define vkUnmapMemory pvkUnmapMemory
#define vkFlushMappedMemoryRanges pvkFlushMappedMemoryRanges
#define vkCreateImage pvkCreateImage
#define vkDestroyImage pvkDestroyImage
#define vkGetImageMemoryRequirements pvkGetImageMemoryRequirements
#define vkBindImageMemory pvkBindImageMemory
#define vkCreateShaderModule pvkCreateShaderModule
#define vkDestroyShaderModule pvkDestroyShaderModule
#define vkCreateDescriptorSetLayout pvkCreateDescriptorSetLayout
#define vkDestroyDescriptorSetLayout pvkDestroyDescriptorSetLayout
#define vkCreatePipelineLayout pvkCreatePipelineLayout
#define vkDestroyPipelineLayout pvkDestroyPipelineLayout
#define vkCreateGraphicsPipelines pvkCreateGraphicsPipelines
#define vkDestroyPipeline pvkDestroyPipeline
#define vkCreateDescriptorPool pvkCreateDescriptorPool
#define vkDestroyDescriptorPool pvkDestroyDescriptorPool
#define vkAllocateDescriptorSets pvkAllocateDescriptorSets
#define vkFreeDescriptorSets pvkFreeDescriptorSets
#define vkUpdateDescriptorSets pvkUpdateDescriptorSets
#define vkCreateSampler pvkCreateSampler
#define vkDestroySampler pvkDestroySampler
#define vkCreateSemaphore pvkCreateSemaphore
#define vkDestroySemaphore pvkDestroySemaphore
#define vkCreateFence pvkCreateFence
#define vkDestroyFence pvkDestroyFence
#define vkWaitForFences pvkWaitForFences
#define vkResetFences pvkResetFences
#define vkQueueSubmit pvkQueueSubmit
#define vkQueueWaitIdle pvkQueueWaitIdle
#define vkDeviceWaitIdle pvkDeviceWaitIdle

static bool LoadVulkanSymbols(VkInstance instance = VK_NULL_HANDLE, VkDevice device = VK_NULL_HANDLE) {
    if (!pvkGetInstanceProcAddr) {
        pvkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr());
        if (!pvkGetInstanceProcAddr) {
            fprintf(stderr, "[Vulkan] SDL_Vulkan_GetVkGetInstanceProcAddr failed!\n");
            return false;
        }
    }

    if (instance == VK_NULL_HANDLE) {
        pvkCreateInstance = (PFN_vkCreateInstance)pvkGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
        return pvkCreateInstance != nullptr;
    }

    if (device == VK_NULL_HANDLE) {
        #define LOAD_I(name) p##name = (PFN_##name)pvkGetInstanceProcAddr(instance, #name)
        LOAD_I(vkDestroyInstance);
        LOAD_I(vkEnumeratePhysicalDevices);
        LOAD_I(vkGetPhysicalDeviceProperties);
        LOAD_I(vkGetPhysicalDeviceFeatures);
        LOAD_I(vkGetPhysicalDeviceQueueFamilyProperties);
        LOAD_I(vkGetPhysicalDeviceMemoryProperties);
        LOAD_I(vkGetPhysicalDeviceSurfaceSupportKHR);
        LOAD_I(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
        LOAD_I(vkGetPhysicalDeviceSurfaceFormatsKHR);
        LOAD_I(vkGetPhysicalDeviceSurfacePresentModesKHR);
        LOAD_I(vkCreateDevice);
        LOAD_I(vkGetDeviceProcAddr);
        #undef LOAD_I
        return true;
    }

    #define LOAD_D(name) p##name = (PFN_##name)pvkGetDeviceProcAddr(device, #name)
    LOAD_D(vkDestroyDevice);
    LOAD_D(vkGetDeviceQueue);
    LOAD_D(vkCreateSwapchainKHR);
    LOAD_D(vkDestroySwapchainKHR);
    LOAD_D(vkGetSwapchainImagesKHR);
    LOAD_D(vkAcquireNextImageKHR);
    LOAD_D(vkQueuePresentKHR);
    LOAD_D(vkCreateImageView);
    LOAD_D(vkDestroyImageView);
    LOAD_D(vkCreateRenderPass);
    LOAD_D(vkDestroyRenderPass);
    LOAD_D(vkCreateFramebuffer);
    LOAD_D(vkDestroyFramebuffer);
    LOAD_D(vkCreateCommandPool);
    LOAD_D(vkDestroyCommandPool);
    LOAD_D(vkAllocateCommandBuffers);
    LOAD_D(vkFreeCommandBuffers);
    LOAD_D(vkBeginCommandBuffer);
    LOAD_D(vkEndCommandBuffer);
    LOAD_D(vkResetCommandBuffer);
    LOAD_D(vkCmdBeginRenderPass);
    LOAD_D(vkCmdEndRenderPass);
    LOAD_D(vkCmdBindPipeline);
    LOAD_D(vkCmdBindDescriptorSets);
    LOAD_D(vkCmdBindVertexBuffers);
    LOAD_D(vkCmdBindIndexBuffer);
    LOAD_D(vkCmdSetViewport);
    LOAD_D(vkCmdSetScissor);
    LOAD_D(vkCmdDrawIndexed);
    LOAD_D(vkCmdPushConstants);
    LOAD_D(vkCmdCopyBufferToImage);
    LOAD_D(vkCmdPipelineBarrier);
    LOAD_D(vkCreateBuffer);
    LOAD_D(vkDestroyBuffer);
    LOAD_D(vkGetBufferMemoryRequirements);
    LOAD_D(vkAllocateMemory);
    LOAD_D(vkFreeMemory);
    LOAD_D(vkBindBufferMemory);
    LOAD_D(vkMapMemory);
    LOAD_D(vkUnmapMemory);
    LOAD_D(vkFlushMappedMemoryRanges);
    LOAD_D(vkCreateImage);
    LOAD_D(vkDestroyImage);
    LOAD_D(vkGetImageMemoryRequirements);
    LOAD_D(vkBindImageMemory);
    LOAD_D(vkCreateShaderModule);
    LOAD_D(vkDestroyShaderModule);
    LOAD_D(vkCreateDescriptorSetLayout);
    LOAD_D(vkDestroyDescriptorSetLayout);
    LOAD_D(vkCreatePipelineLayout);
    LOAD_D(vkDestroyPipelineLayout);
    LOAD_D(vkCreateGraphicsPipelines);
    LOAD_D(vkDestroyPipeline);
    LOAD_D(vkCreateDescriptorPool);
    LOAD_D(vkDestroyDescriptorPool);
    LOAD_D(vkAllocateDescriptorSets);
    LOAD_D(vkFreeDescriptorSets);
    LOAD_D(vkUpdateDescriptorSets);
    LOAD_D(vkCreateSampler);
    LOAD_D(vkDestroySampler);
    LOAD_D(vkCreateSemaphore);
    LOAD_D(vkDestroySemaphore);
    LOAD_D(vkCreateFence);
    LOAD_D(vkDestroyFence);
    LOAD_D(vkWaitForFences);
    LOAD_D(vkResetFences);
    LOAD_D(vkQueueSubmit);
    LOAD_D(vkQueueWaitIdle);
    LOAD_D(vkDeviceWaitIdle);
    #undef LOAD_D
    return true;
}

// ── Constructor / Destructor ─────────────────────────────────

VulkanRenderer::VulkanRenderer() {
    g_Renderer = this;
}

VulkanRenderer::~VulkanRenderer() {
    Shutdown();
    g_Renderer = nullptr;
}

// ── Initialization ───────────────────────────────────────────

bool VulkanRenderer::Initialize(void* windowHandle, int width, int height, bool /*fullscreen*/) {
    window_ = static_cast<SDL_Window*>(windowHandle);
    windowWidth_ = width;
    windowHeight_ = height;

    if (!InitVulkan()) return false;
    if (!CreateSwapchain()) return false;
    if (!CreateDepthBuffer()) return false;
    if (!CreateRenderPass()) return false;
    if (!CreateFramebuffers()) return false;
    if (!CreateDescriptorPool()) return false;
    if (!CreatePipelines()) return false;
    if (!CreateFrameResources()) return false;

    // Create default 1x1 white texture
    whiteTexture_ = CreateTexture(1, 1);
    SurfaceDesc desc;
    if (LockTexture(whiteTexture_, &desc)) {
        *static_cast<uint32_t*>(desc.pixels) = 0xFFFFFFFF;
        UnlockTexture(whiteTexture_);
    }

    fprintf(stdout, "[Vulkan] Renderer initialized successfully (%dx%d)\n", width, height);
    return true;
}

bool VulkanRenderer::InitVulkan() {
    if (!LoadVulkanSymbols()) return false;

    // Instance Extensions from SDL
    unsigned int extCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window_, &extCount, nullptr)) {
        fprintf(stderr, "[Vulkan] Failed to get instance extension count\n");
        return false;
    }
    std::vector<const char*> extensions(extCount);
    SDL_Vulkan_GetInstanceExtensions(window_, &extCount, extensions.data());

    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "Touhou 6 Vulkan";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "TH06 Vulkan Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo instInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&instInfo, nullptr, &instance_) != VK_SUCCESS) {
        fprintf(stderr, "[Vulkan] vkCreateInstance failed!\n");
        return false;
    }
    LoadVulkanSymbols(instance_);

    // Surface
    if (!SDL_Vulkan_CreateSurface(window_, instance_, &surface_)) {
        fprintf(stderr, "[Vulkan] SDL_Vulkan_CreateSurface failed: %s\n", SDL_GetError());
        return false;
    }

    // Physical Device selection
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) return false;

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    physicalDevice_ = VK_NULL_HANDLE;
    for (auto dev : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physicalDevice_ = dev;
            break;
        }
    }
    if (physicalDevice_ == VK_NULL_HANDLE) {
        physicalDevice_ = devices[0];
    }

    // Queue Families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice_, &queueFamilyCount, queueFamilies.data());

    std::optional<uint32_t> gFam, pFam;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) gFam = i;
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, i, surface_, &presentSupport);
        if (presentSupport) pFam = i;
        if (gFam.has_value() && pFam.has_value()) break;
    }
    if (!gFam.has_value() || !pFam.has_value()) return false;
    graphicsQueueFamily_ = gFam.value();
    presentQueueFamily_ = pFam.value();

    // Logical Device
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::vector<uint32_t> uniqueQueueFamilies = { graphicsQueueFamily_ };
    if (graphicsQueueFamily_ != presentQueueFamily_) {
        uniqueQueueFamilies.push_back(presentQueueFamily_);
    }
    for (uint32_t fam : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo qci{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
        qci.queueFamilyIndex = fam;
        qci.queueCount = 1;
        qci.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(qci);
    }

    std::vector<const char*> devExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceFeatures devFeatures{};
    devFeatures.samplerAnisotropy = VK_FALSE;

    VkDeviceCreateInfo dci{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    dci.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    dci.pQueueCreateInfos = queueCreateInfos.data();
    dci.pEnabledFeatures = &devFeatures;
    dci.enabledExtensionCount = static_cast<uint32_t>(devExtensions.size());
    dci.ppEnabledExtensionNames = devExtensions.data();

    if (vkCreateDevice(physicalDevice_, &dci, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }
    LoadVulkanSymbols(instance_, device_);

    vkGetDeviceQueue(device_, graphicsQueueFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, presentQueueFamily_, 0, &presentQueue_);

    // Command Pool
    VkCommandPoolCreateInfo cpci{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    cpci.queueFamilyIndex = graphicsQueueFamily_;
    cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(device_, &cpci, nullptr, &cmdPool_) != VK_SUCCESS) {
        return false;
    }

    return true;
}

uint32_t VulkanRenderer::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

bool VulkanRenderer::CreateSwapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &caps);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());

    swapchainFormat_ = formats[0].format;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            swapchainFormat_ = f.format;
            break;
        }
    }

    int dw, dh;
    SDL_Vulkan_GetDrawableSize(window_, &dw, &dh);
    swapchainExtent_.width = std::clamp(static_cast<uint32_t>(dw), caps.minImageExtent.width, caps.maxImageExtent.width);
    swapchainExtent_.height = std::clamp(static_cast<uint32_t>(dh), caps.minImageExtent.height, caps.maxImageExtent.height);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    VkSwapchainCreateInfoKHR sci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    sci.surface = surface_;
    sci.minImageCount = imageCount;
    sci.imageFormat = swapchainFormat_;
    sci.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    sci.imageExtent = swapchainExtent_;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    uint32_t queueFamilyIndices[] = { graphicsQueueFamily_, presentQueueFamily_ };
    if (graphicsQueueFamily_ != presentQueueFamily_) {
        sci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        sci.queueFamilyIndexCount = 2;
        sci.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR; // VSync 60fps
    sci.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_) != VK_SUCCESS) {
        return false;
    }

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());

    swapchainImageViews_.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = swapchainImages_[i];
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = swapchainFormat_;
        ivci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &ivci, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::CreateDepthBuffer() {
    VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent.width = swapchainExtent_.width;
    ici.extent.height = swapchainExtent_.height;
    ici.extent.depth = 1;
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = depthFormat_;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device_, &ici, nullptr, &depthImage_) != VK_SUCCESS) return false;

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(device_, depthImage_, &memReq);

    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = memReq.size;
    mai.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device_, &mai, nullptr, &depthMemory_) != VK_SUCCESS) return false;
    vkBindImageMemory(device_, depthImage_, depthMemory_, 0);

    VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    ivci.image = depthImage_;
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = depthFormat_;
    ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    ivci.subresourceRange.baseMipLevel = 0;
    ivci.subresourceRange.levelCount = 1;
    ivci.subresourceRange.baseArrayLayer = 0;
    ivci.subresourceRange.layerCount = 1;
    return vkCreateImageView(device_, &ivci, nullptr, &depthImageView_) == VK_SUCCESS;
}

bool VulkanRenderer::CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat_;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo rpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpci.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpci.pAttachments = attachments.data();
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.dependencyCount = 1;
    rpci.pDependencies = &dependency;

    return vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_) == VK_SUCCESS;
}

bool VulkanRenderer::CreateFramebuffers() {
    framebuffers_.resize(swapchainImageViews_.size());
    for (size_t i = 0; i < swapchainImageViews_.size(); i++) {
        std::array<VkImageView, 2> attachments = { swapchainImageViews_[i], depthImageView_ };
        VkFramebufferCreateInfo fbci{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fbci.renderPass = renderPass_;
        fbci.attachmentCount = static_cast<uint32_t>(attachments.size());
        fbci.pAttachments = attachments.data();
        fbci.width = swapchainExtent_.width;
        fbci.height = swapchainExtent_.height;
        fbci.layers = 1;
        if (vkCreateFramebuffer(device_, &fbci, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            return false;
        }
    }
    return true;
}

bool VulkanRenderer::CreateDescriptorPool() {
    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerLayoutBinding;
    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &descSetLayout_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 512;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 512;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descPool_) != VK_SUCCESS) {
        return false;
    }

    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    return vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) == VK_SUCCESS;
}

static VkShaderModule CreateShaderModuleFromSpv(VkDevice device, const uint32_t* code, size_t size) {
    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = size;
    createInfo.pCode = code;
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

bool VulkanRenderer::CreatePipelines() {
    // ── 2D Sprite Pipelines ──
    VkShaderModule spriteVertMod = CreateShaderModuleFromSpv(device_, kSpriteVertSpv, kSpriteVertSpvSize);
    VkShaderModule spriteFragMod = CreateShaderModuleFromSpv(device_, kSpriteFragSpv, kSpriteFragSpvSize);

    VkPipelineShaderStageCreateInfo spriteStages[2]{};
    spriteStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, spriteVertMod, "main" };
    spriteStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, spriteFragMod, "main" };

    VkVertexInputBindingDescription binding2D{ 0, sizeof(Vertex2D), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attribs2D[3]{};
    attribs2D[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, x) };
    attribs2D[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, u) };
    attribs2D[2] = { 2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Vertex2D, color) };

    VkPipelineVertexInputStateCreateInfo vi2D{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vi2D.vertexBindingDescriptionCount = 1;
    vi2D.pVertexBindingDescriptions = &binding2D;
    vi2D.vertexAttributeDescriptionCount = 3;
    vi2D.pVertexAttributeDescriptions = attribs2D;

    VkPipelineInputAssemblyStateCreateInfo ia2D{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    ia2D.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport vp{ 0.0f, 0.0f, (float)swapchainExtent_.width, (float)swapchainExtent_.height, 0.0f, 1.0f };
    VkRect2D scissor{ {0, 0}, swapchainExtent_ };
    VkPipelineViewportStateCreateInfo vpState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    vpState.viewportCount = 1; vpState.pViewports = &vp;
    vpState.scissorCount = 1;  vpState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rsState{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rsState.polygonMode = VK_POLYGON_MODE_FILL;
    rsState.cullMode = VK_CULL_MODE_NONE;
    rsState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rsState.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo msState{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    msState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds2D{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds2D.depthTestEnable = VK_FALSE;
    ds2D.depthWriteEnable = VK_FALSE;

    VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynState.dynamicStateCount = 2;
    dynState.pDynamicStates = dynStates;

    VkPushConstantRange pc2D{};
    pc2D.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc2D.offset = 0;
    pc2D.size = sizeof(PushConstants2D);

    VkPipelineLayoutCreateInfo pl2D{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pl2D.setLayoutCount = 1;
    pl2D.pSetLayouts = &descSetLayout_;
    pl2D.pushConstantRangeCount = 1;
    pl2D.pPushConstantRanges = &pc2D;
    if (vkCreatePipelineLayout(device_, &pl2D, nullptr, &spritePipelineLayout_) != VK_SUCCESS) return false;

    // Normal Alpha Blend
    VkPipelineColorBlendAttachmentState cbAlpha{};
    cbAlpha.blendEnable = VK_TRUE;
    cbAlpha.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbAlpha.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAlpha.colorBlendOp = VK_BLEND_OP_ADD;
    cbAlpha.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cbAlpha.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cbAlpha.alphaBlendOp = VK_BLEND_OP_ADD;
    cbAlpha.colorWriteMask = 0xF;

    VkPipelineColorBlendStateCreateInfo cbsAlpha{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbsAlpha.attachmentCount = 1;
    cbsAlpha.pAttachments = &cbAlpha;

    VkGraphicsPipelineCreateInfo gp2D{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gp2D.stageCount = 2;
    gp2D.pStages = spriteStages;
    gp2D.pVertexInputState = &vi2D;
    gp2D.pInputAssemblyState = &ia2D;
    gp2D.pViewportState = &vpState;
    gp2D.pRasterizationState = &rsState;
    gp2D.pMultisampleState = &msState;
    gp2D.pDepthStencilState = &ds2D;
    gp2D.pColorBlendState = &cbsAlpha;
    gp2D.pDynamicState = &dynState;
    gp2D.layout = spritePipelineLayout_;
    gp2D.renderPass = renderPass_;
    gp2D.subpass = 0;

    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp2D, nullptr, &spritePipelineAlpha_);

    // Additive Blend
    VkPipelineColorBlendAttachmentState cbAdd = cbAlpha;
    cbAdd.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
    cbAdd.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    VkPipelineColorBlendStateCreateInfo cbsAdd{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbsAdd.attachmentCount = 1; cbsAdd.pAttachments = &cbAdd;
    gp2D.pColorBlendState = &cbsAdd;
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp2D, nullptr, &spritePipelineAdd_);

    // Subtractive Blend
    VkPipelineColorBlendAttachmentState cbSub = cbAlpha;
    cbSub.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    cbSub.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    VkPipelineColorBlendStateCreateInfo cbsSub{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    cbsSub.attachmentCount = 1; cbsSub.pAttachments = &cbSub;
    gp2D.pColorBlendState = &cbsSub;
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp2D, nullptr, &spritePipelineSub_);

    vkDestroyShaderModule(device_, spriteVertMod, nullptr);
    vkDestroyShaderModule(device_, spriteFragMod, nullptr);

    // ── 3D Stage Pipeline ──
    VkShaderModule stageVertMod = CreateShaderModuleFromSpv(device_, kStage3dVertSpv, kStage3dVertSpvSize);
    VkShaderModule stageFragMod = CreateShaderModuleFromSpv(device_, kStage3dFragSpv, kStage3dFragSpvSize);

    VkPipelineShaderStageCreateInfo stageStages[2]{};
    stageStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, stageVertMod, "main" };
    stageStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, stageFragMod, "main" };

    VkVertexInputBindingDescription binding3D{ 0, sizeof(Vertex3D), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attribs3D[3]{};
    attribs3D[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, x) };
    attribs3D[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex3D, u) };
    attribs3D[2] = { 2, 0, VK_FORMAT_R8G8B8A8_UNORM,   offsetof(Vertex3D, color) };

    VkPipelineVertexInputStateCreateInfo vi3D{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vi3D.vertexBindingDescriptionCount = 1;
    vi3D.pVertexBindingDescriptions = &binding3D;
    vi3D.vertexAttributeDescriptionCount = 3;
    vi3D.pVertexAttributeDescriptions = attribs3D;

    VkPipelineDepthStencilStateCreateInfo ds3D{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    ds3D.depthTestEnable = VK_TRUE;
    ds3D.depthWriteEnable = VK_TRUE;
    ds3D.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPushConstantRange pc3D{};
    pc3D.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pc3D.offset = 0;
    pc3D.size = sizeof(PushConstants3D);

    VkPipelineLayoutCreateInfo pl3D{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pl3D.setLayoutCount = 1;
    pl3D.pSetLayouts = &descSetLayout_;
    pl3D.pushConstantRangeCount = 1;
    pl3D.pPushConstantRanges = &pc3D;
    if (vkCreatePipelineLayout(device_, &pl3D, nullptr, &stagePipelineLayout_) != VK_SUCCESS) return false;

    VkGraphicsPipelineCreateInfo gp3D{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    gp3D.stageCount = 2;
    gp3D.pStages = stageStages;
    gp3D.pVertexInputState = &vi3D;
    gp3D.pInputAssemblyState = &ia2D;
    gp3D.pViewportState = &vpState;
    gp3D.pRasterizationState = &rsState;
    gp3D.pMultisampleState = &msState;
    gp3D.pDepthStencilState = &ds3D;
    gp3D.pColorBlendState = &cbsAlpha;
    gp3D.pDynamicState = &dynState;
    gp3D.layout = stagePipelineLayout_;
    gp3D.renderPass = renderPass_;
    gp3D.subpass = 0;

    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp3D, nullptr, &stagePipelineAlpha_);
    gp3D.pColorBlendState = &cbsAdd;
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp3D, nullptr, &stagePipelineAdd_);

    vkDestroyShaderModule(device_, stageVertMod, nullptr);
    vkDestroyShaderModule(device_, stageFragMod, nullptr);

    return true;
}

bool VulkanRenderer::CreateFrameResources() {
    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = cmdPool_;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkAllocateCommandBuffers(device_, &cbai, &frames_[i].cmdBuffer);
        vkCreateSemaphore(device_, &sci, nullptr, &frames_[i].imageAvailable);
        vkCreateSemaphore(device_, &sci, nullptr, &frames_[i].renderFinished);
        vkCreateFence(device_, &fci, nullptr, &frames_[i].inFlight);

        // Vertex buffer
        VkBufferCreateInfo vbci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        vbci.size = MAX_VERTICES_PER_FRAME * sizeof(Vertex3D); // Max size covers both 2D and 3D
        vbci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        vbci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device_, &vbci, nullptr, &frames_[i].vertexBuffer);

        VkMemoryRequirements vbReq;
        vkGetBufferMemoryRequirements(device_, frames_[i].vertexBuffer, &vbReq);
        VkMemoryAllocateInfo vbMai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        vbMai.allocationSize = vbReq.size;
        vbMai.memoryTypeIndex = FindMemoryType(vbReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &vbMai, nullptr, &frames_[i].vertexMemory);
        vkBindBufferMemory(device_, frames_[i].vertexBuffer, frames_[i].vertexMemory, 0);
        vkMapMemory(device_, frames_[i].vertexMemory, 0, vbReq.size, 0, &frames_[i].mappedVertices);

        // Index buffer
        VkBufferCreateInfo ibci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
        ibci.size = MAX_INDICES_PER_FRAME * sizeof(uint16_t);
        ibci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        ibci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateBuffer(device_, &ibci, nullptr, &frames_[i].indexBuffer);

        VkMemoryRequirements ibReq;
        vkGetBufferMemoryRequirements(device_, frames_[i].indexBuffer, &ibReq);
        VkMemoryAllocateInfo ibMai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        ibMai.allocationSize = ibReq.size;
        ibMai.memoryTypeIndex = FindMemoryType(ibReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkAllocateMemory(device_, &ibMai, nullptr, &frames_[i].indexMemory);
        vkBindBufferMemory(device_, frames_[i].indexBuffer, frames_[i].indexMemory, 0);
        vkMapMemory(device_, frames_[i].indexMemory, 0, ibReq.size, 0, &frames_[i].mappedIndices);
    }
    return true;
}

void VulkanRenderer::Shutdown() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);

        for (auto& t : textures_) {
            if (t.valid) {
                vkDestroyImageView(device_, t.view, nullptr);
                vkDestroyImage(device_, t.image, nullptr);
                vkFreeMemory(device_, t.memory, nullptr);
            }
        }
        textures_.clear();

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkUnmapMemory(device_, frames_[i].vertexMemory);
            vkDestroyBuffer(device_, frames_[i].vertexBuffer, nullptr);
            vkFreeMemory(device_, frames_[i].vertexMemory, nullptr);

            vkUnmapMemory(device_, frames_[i].indexMemory);
            vkDestroyBuffer(device_, frames_[i].indexBuffer, nullptr);
            vkFreeMemory(device_, frames_[i].indexMemory, nullptr);

            vkDestroySemaphore(device_, frames_[i].imageAvailable, nullptr);
            vkDestroySemaphore(device_, frames_[i].renderFinished, nullptr);
            vkDestroyFence(device_, frames_[i].inFlight, nullptr);
        }

        vkDestroyPipeline(device_, spritePipelineAlpha_, nullptr);
        vkDestroyPipeline(device_, spritePipelineAdd_, nullptr);
        vkDestroyPipeline(device_, spritePipelineSub_, nullptr);
        vkDestroyPipelineLayout(device_, spritePipelineLayout_, nullptr);

        vkDestroyPipeline(device_, stagePipelineAlpha_, nullptr);
        vkDestroyPipeline(device_, stagePipelineAdd_, nullptr);
        vkDestroyPipelineLayout(device_, stagePipelineLayout_, nullptr);

        vkDestroySampler(device_, sampler_, nullptr);
        vkDestroyDescriptorPool(device_, descPool_, nullptr);
        vkDestroyDescriptorSetLayout(device_, descSetLayout_, nullptr);

        for (auto fb : framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
        framebuffers_.clear();

        vkDestroyImageView(device_, depthImageView_, nullptr);
        vkDestroyImage(device_, depthImage_, nullptr);
        vkFreeMemory(device_, depthMemory_, nullptr);

        vkDestroyRenderPass(device_, renderPass_, nullptr);

        for (auto iv : swapchainImageViews_) vkDestroyImageView(device_, iv, nullptr);
        swapchainImageViews_.clear();
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);

        vkDestroyCommandPool(device_, cmdPool_, nullptr);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::OnWindowResize(int /*width*/, int /*height*/) {
    // Handled in Next frame acquire
}

// ── Frame Lifecycle ──────────────────────────────────────────

void VulkanRenderer::BeginScene() {
    if (inScene_) return;

    auto& frame = frames_[currentFrame_];
    vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &currentImageIndex_);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        // Recreate swapchain
        return;
    }

    vkResetFences(device_, 1, &frame.inFlight);
    vkResetCommandBuffer(frame.cmdBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(frame.cmdBuffer, &beginInfo);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { { clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3] } };
    clearValues[1].depthStencil = { 1.0f, 0 };
    clearColor_[0] = clearColor_[1] = clearColor_[2] = 0.0f; clearColor_[3] = 1.0f;

    VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    rpbi.renderPass = renderPass_;
    rpbi.framebuffer = framebuffers_[currentImageIndex_];
    rpbi.renderArea.offset = { 0, 0 };
    rpbi.renderArea.extent = swapchainExtent_;
    rpbi.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpbi.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(frame.cmdBuffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{ 0.0f, 0.0f, (float)swapchainExtent_.width, (float)swapchainExtent_.height, 0.0f, 1.0f };
    VkRect2D scissor{ {0, 0}, swapchainExtent_ };
    vkCmdSetViewport(frame.cmdBuffer, 0, 1, &vp);
    vkCmdSetScissor(frame.cmdBuffer, 0, 1, &scissor);

    vertexCount_ = 0;
    indexCount_ = 0;
    batches2D_.clear();
    inScene_ = true;
    frameReady_ = false;
}

void VulkanRenderer::EndScene() {
    if (!inScene_) return;

    FlushBatch2D();

    auto& frame = frames_[currentFrame_];
    vkCmdEndRenderPass(frame.cmdBuffer);
    vkEndCommandBuffer(frame.cmdBuffer);

    inScene_ = false;
    frameReady_ = true;
}

void VulkanRenderer::Present() {
    if (!frameReady_)
    {
        BeginScene();
        EndScene();
        if (!frameReady_) return;
    }
    auto& frame = frames_[currentFrame_];

    VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    VkSemaphore waitSemaphores[] = { frame.imageAvailable };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &frame.cmdBuffer;

    VkSemaphore signalSemaphores[] = { frame.renderFinished };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(graphicsQueue_, 1, &submitInfo, frame.inFlight);

    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &currentImageIndex_;

    vkQueuePresentKHR(presentQueue_, &presentInfo);

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    frameReady_ = false;
}

void VulkanRenderer::Clear(uint32_t argb, bool /*clearDepth*/) {
    clearColor_[0] = ((argb >> 16) & 0xFF) / 255.0f;
    clearColor_[1] = ((argb >>  8) & 0xFF) / 255.0f;
    clearColor_[2] = ((argb >>  0) & 0xFF) / 255.0f;
    clearColor_[3] = ((argb >> 24) & 0xFF) / 255.0f;
}

void VulkanRenderer::SetViewport(int x, int y, int width, int height) {
    if (!inScene_) return;
    FlushBatch2D();

    // Scale from 640x480 coordinate space to actual swapchain extent
    float scaleX = (float)swapchainExtent_.width / 640.0f;
    float scaleY = (float)swapchainExtent_.height / 480.0f;

    VkViewport vp{
        (float)x * scaleX,
        (float)y * scaleY,
        (float)width * scaleX,
        (float)height * scaleY,
        0.0f, 1.0f
    };
    VkRect2D scissor{
        { (int32_t)(x * scaleX), (int32_t)(y * scaleY) },
        { (uint32_t)(width * scaleX), (uint32_t)(height * scaleY) }
    };
    vkCmdSetViewport(frames_[currentFrame_].cmdBuffer, 0, 1, &vp);
    vkCmdSetScissor(frames_[currentFrame_].cmdBuffer, 0, 1, &scissor);
}

// ── 2D Drawing ───────────────────────────────────────────────

void VulkanRenderer::DrawQuad2D(TextureHandle tex, const Vertex2D v[4], BlendMode blend) {
    { static int n=0; if (n++ < 100) fprintf(stderr, "[VkD] q2d tex=%zu valid=%d wh=%ux%u blend=%d p0=(%.0f,%.0f) uv0=(%.2f,%.2f) col=%08x\n", (size_t)tex, tex<textures_.size()?(int)textures_[tex].valid:-1, tex<textures_.size()?textures_[tex].width:0, tex<textures_.size()?textures_[tex].height:0, (int)blend, v[0].x, v[0].y, v[0].u, v[0].v, v[0].color); }
    if (!inScene_) return;
    if (tex >= textures_.size() || !textures_[tex].valid) {
        tex = whiteTexture_;
    }
    if (tex == INVALID_TEXTURE) return;

    if (textures_[tex].dirty) {
        UploadTextureGPU(tex);
    }

    if (!batches2D_.empty()) {
        auto& last = batches2D_.back();
        if (last.texture != tex || last.blendMode != blend) {
            FlushBatch2D();
        }
    }
    if (batches2D_.empty()) {
        batches2D_.push_back({ tex, blend, indexCount_, 0 });
    }

    auto& frame = frames_[currentFrame_];
    if (vertexCount_ + 4 > MAX_VERTICES_PER_FRAME || indexCount_ + 6 > MAX_INDICES_PER_FRAME) {
        FlushBatch2D();
        batches2D_.push_back({ tex, blend, indexCount_, 0 });
    }

    Vertex2D* vDst = static_cast<Vertex2D*>(frame.mappedVertices) + vertexCount_;
    uint16_t* iDst = static_cast<uint16_t*>(frame.mappedIndices) + indexCount_;
    uint16_t base = static_cast<uint16_t>(vertexCount_);

    vDst[0] = v[0];
    vDst[1] = v[1];
    vDst[2] = v[2];
    vDst[3] = v[3];

    // Strip order (0=TL, 1=TR, 2=BL, 3=BR) -> 2 triangles
    iDst[0] = base + 0;
    iDst[1] = base + 1;
    iDst[2] = base + 2;
    iDst[3] = base + 2;
    iDst[4] = base + 1;
    iDst[5] = base + 3;

    vertexCount_ += 4;
    indexCount_ += 6;
    batches2D_.back().indexCount += 6;
}

void VulkanRenderer::DrawColoredQuad2D(const Vertex2D v[4], BlendMode blend) {
    { static int n=0; if (n++ < 40) fprintf(stderr, "[VkD] cq2d blend=%d p0=(%.0f,%.0f) p3=(%.0f,%.0f) col=%08x\n", (int)blend, v[0].x, v[0].y, v[3].x, v[3].y, v[0].color); }
    DrawQuad2D(whiteTexture_, v, blend);
}

void VulkanRenderer::DrawTriangleStrip2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) {
    if (count < 3) return;
    if (tex >= textures_.size() || !textures_[tex].valid) tex = whiteTexture_;
    if (tex == INVALID_TEXTURE) return;
    if (textures_[tex].dirty) UploadTextureGPU(tex);

    size_t numTriangles = count - 2;
    size_t numIndices = numTriangles * 3;

    if (!batches2D_.empty()) {
        auto& last = batches2D_.back();
        if (last.texture != tex || last.blendMode != blend) {
            FlushBatch2D();
        }
    }
    if (batches2D_.empty()) {
        batches2D_.push_back({ tex, blend, indexCount_, 0 });
    }

    auto& frame = frames_[currentFrame_];
    if (vertexCount_ + count > MAX_VERTICES_PER_FRAME || indexCount_ + numIndices > MAX_INDICES_PER_FRAME) {
        FlushBatch2D();
        batches2D_.push_back({ tex, blend, indexCount_, 0 });
    }

    Vertex2D* vDst = static_cast<Vertex2D*>(frame.mappedVertices) + vertexCount_;
    uint16_t* iDst = static_cast<uint16_t*>(frame.mappedIndices) + indexCount_;
    uint16_t base = static_cast<uint16_t>(vertexCount_);

    memcpy(vDst, verts, count * sizeof(Vertex2D));

    size_t outIdx = 0;
    for (size_t i = 0; i < numTriangles; i++) {
        if (i % 2 == 0) {
            iDst[outIdx++] = base + i;
            iDst[outIdx++] = base + i + 1;
            iDst[outIdx++] = base + i + 2;
        } else {
            iDst[outIdx++] = base + i + 1;
            iDst[outIdx++] = base + i;
            iDst[outIdx++] = base + i + 2;
        }
    }

    vertexCount_ += count;
    indexCount_ += numIndices;
    batches2D_.back().indexCount += numIndices;
}

void VulkanRenderer::DrawTriangleFan2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) {
    if (count < 3) return;
    if (tex >= textures_.size() || !textures_[tex].valid) tex = whiteTexture_;
    if (tex == INVALID_TEXTURE) return;
    if (textures_[tex].dirty) UploadTextureGPU(tex);

    size_t numTriangles = count - 2;
    size_t numIndices = numTriangles * 3;

    if (!batches2D_.empty()) {
        auto& last = batches2D_.back();
        if (last.texture != tex || last.blendMode != blend) {
            FlushBatch2D();
        }
    }
    if (batches2D_.empty()) {
        batches2D_.push_back({ tex, blend, indexCount_, 0 });
    }

    auto& frame = frames_[currentFrame_];
    if (vertexCount_ + count > MAX_VERTICES_PER_FRAME || indexCount_ + numIndices > MAX_INDICES_PER_FRAME) {
        FlushBatch2D();
        batches2D_.push_back({ tex, blend, indexCount_, 0 });
    }

    Vertex2D* vDst = static_cast<Vertex2D*>(frame.mappedVertices) + vertexCount_;
    uint16_t* iDst = static_cast<uint16_t*>(frame.mappedIndices) + indexCount_;
    uint16_t base = static_cast<uint16_t>(vertexCount_);

    memcpy(vDst, verts, count * sizeof(Vertex2D));

    size_t outIdx = 0;
    for (size_t i = 1; i + 1 < count; i++) {
        iDst[outIdx++] = base;
        iDst[outIdx++] = base + i;
        iDst[outIdx++] = base + i + 1;
    }

    vertexCount_ += count;
    indexCount_ += numIndices;
    batches2D_.back().indexCount += numIndices;
}

void VulkanRenderer::FlushBatch2D() {
    if (batches2D_.empty() || indexCount_ == 0) return;

    auto& frame = frames_[currentFrame_];
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(frame.cmdBuffer, 0, 1, &frame.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(frame.cmdBuffer, frame.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    PushConstants2D pc{};
    // Orthographic projection: [0, 640] x [0, 480] -> [-1, 1] x [-1, 1]
    // Vulkan Y is downwards (0 at top, 1 at bottom in NDC if using standard viewport)
    pc.projMat[0]  =  2.0f / 640.0f;
    pc.projMat[5]  =  2.0f / 480.0f;
    pc.projMat[10] =  1.0f;
    pc.projMat[12] = -1.0f;
    pc.projMat[13] = -1.0f;
    pc.projMat[15] =  1.0f;
    pc.colorKeyEnable = 0.0f;

    vkCmdPushConstants(frame.cmdBuffer, spritePipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants2D), &pc);

    for (const auto& batch : batches2D_) {
        if (batch.indexCount == 0) continue;

        VkPipeline pipeline = spritePipelineAlpha_;
        if (batch.blendMode == BLEND_ADD) pipeline = spritePipelineAdd_;
        else if (batch.blendMode == BLEND_SUB) pipeline = spritePipelineSub_;

        vkCmdBindPipeline(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        if (batch.texture < textures_.size() && textures_[batch.texture].valid) {
            vkCmdBindDescriptorSets(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    spritePipelineLayout_, 0, 1,
                                    &textures_[batch.texture].descriptorSet, 0, nullptr);
        }

        vkCmdDrawIndexed(frame.cmdBuffer, batch.indexCount, 1, batch.indexStart, 0, 0);
    }

    batches2D_.clear();
}

// ── 3D Stage Drawing ─────────────────────────────────────────

void VulkanRenderer::SetCamera3D(const float view[16], const float proj[16]) {
    memcpy(viewMatrix_, view, sizeof(float) * 16);
    memcpy(projMatrix_, proj, sizeof(float) * 16);

    // Vulkan NDC Y points downwards, opposite to Direct3D.
    // Negate the Y scale (proj[5]) so D3D view*proj maps correctly into Vulkan NDC.
    float vulkanProj[16];
    memcpy(vulkanProj, proj, sizeof(float) * 16);
    vulkanProj[5] = -vulkanProj[5];

    // Multiply view * vulkanProj -> viewProjMatrix_ (row-major: v * (view * proj))
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            viewProjMatrix_[i * 4 + j] = 0.0f;
            for (int k = 0; k < 4; k++) {
                viewProjMatrix_[i * 4 + j] += view[i * 4 + k] * vulkanProj[k * 4 + j];
            }
        }
    }
}

void VulkanRenderer::SetFog(bool enable, uint32_t color, float nearZ, float farZ) {
    fogEnable_ = enable;
    fogColor_ = color;
    fogNear_ = nearZ;
    fogFar_ = farZ;
}

void VulkanRenderer::DrawQuad3D(const float world[16], TextureHandle tex, const Vertex3D v[4], BlendMode blend) {
    { static int n=0; if (n++ < 40) fprintf(stderr, "[VkD] q3d tex=%zu valid=%d blend=%d v0=(%.0f,%.0f,%.0f) col=%08x\n", (size_t)tex, tex<textures_.size()?(int)textures_[tex].valid:-1, (int)blend, v[0].x, v[0].y, v[0].z, v[0].color); }
    if (!inScene_) return;
    FlushBatch2D();

    if (tex >= textures_.size() || !textures_[tex].valid) tex = whiteTexture_;
    if (tex == INVALID_TEXTURE) return;

    if (textures_[tex].dirty) UploadTextureGPU(tex);

    auto& frame = frames_[currentFrame_];
    if (vertexCount_ + 4 > MAX_VERTICES_PER_FRAME || indexCount_ + 6 > MAX_INDICES_PER_FRAME) {
        return;
    }

    // Transform v with world matrix on CPU
    Vertex3D* vDst = static_cast<Vertex3D*>(frame.mappedVertices) + vertexCount_;
    uint16_t* iDst = static_cast<uint16_t*>(frame.mappedIndices) + indexCount_;
    uint16_t base = static_cast<uint16_t>(vertexCount_);

    for (int i = 0; i < 4; i++) {
        float x = v[i].x * world[0] + v[i].y * world[4] + v[i].z * world[8]  + world[12];
        float y = v[i].x * world[1] + v[i].y * world[5] + v[i].z * world[9]  + world[13];
        float z = v[i].x * world[2] + v[i].y * world[6] + v[i].z * world[10] + world[14];
        vDst[i] = { x, y, z, v[i].u, v[i].v, v[i].color };
    }

    iDst[0] = base + 0;
    iDst[1] = base + 1;
    iDst[2] = base + 2;
    iDst[3] = base + 2;
    iDst[4] = base + 1;
    iDst[5] = base + 3;

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(frame.cmdBuffer, 0, 1, &frame.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(frame.cmdBuffer, frame.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    PushConstants3D pc3D{};
    memcpy(pc3D.viewProjMat, viewProjMatrix_, sizeof(float) * 16);
    pc3D.fogColor[0] = ((fogColor_ >> 16) & 0xFF) / 255.0f;
    pc3D.fogColor[1] = ((fogColor_ >>  8) & 0xFF) / 255.0f;
    pc3D.fogColor[2] = ((fogColor_ >>  0) & 0xFF) / 255.0f;
    pc3D.fogColor[3] = ((fogColor_ >> 24) & 0xFF) / 255.0f;
    pc3D.fogDist[0] = fogNear_;
    pc3D.fogDist[1] = fogFar_;
    pc3D.fogEnable = fogEnable_ ? 1.0f : 0.0f;

    vkCmdPushConstants(frame.cmdBuffer, stagePipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants3D), &pc3D);

    VkPipeline pipeline = (blend == BLEND_ADD) ? stagePipelineAdd_ : stagePipelineAlpha_;
    vkCmdBindPipeline(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    vkCmdBindDescriptorSets(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            stagePipelineLayout_, 0, 1,
                            &textures_[tex].descriptorSet, 0, nullptr);

    vkCmdDrawIndexed(frame.cmdBuffer, 6, 1, indexCount_, 0, 0);

    vertexCount_ += 4;
    indexCount_ += 6;
}

// ── Texture Management ───────────────────────────────────────

TextureHandle VulkanRenderer::CreateTexture(uint32_t width, uint32_t height) {
    TextureResource res;
    res.width = width;
    res.height = height;
    res.cpuData.resize(width * height * 4, 0);
    res.dirty = true;
    res.valid = false;

    // Find free slot. Slot 0 is reserved for the white texture so that a
    // valid texture handle is never 0 (the game casts handles to pointers and
    // treats a null pointer as a failed texture creation).
    for (size_t i = 1; i < textures_.size(); i++) {
        if (!textures_[i].valid && textures_[i].image == VK_NULL_HANDLE) {
            textures_[i] = std::move(res);
            return i;
        }
    }
    textures_.push_back(std::move(res));
    return textures_.size() - 1;
}

TextureHandle VulkanRenderer::CreateTextureFromMemory(const void* data, size_t size, uint32_t colorKey) {
    int w, h, ch;
    unsigned char* pixels = stbi_load_from_memory(static_cast<const unsigned char*>(data),
                                                  static_cast<int>(size), &w, &h, &ch, 4);
    if (!pixels) return INVALID_TEXTURE;

    // Color key processing
    if (colorKey != 0) {
        uint8_t ckR = (colorKey >> 16) & 0xFF;
        uint8_t ckG = (colorKey >>  8) & 0xFF;
        uint8_t ckB = (colorKey >>  0) & 0xFF;
        for (int i = 0; i < w * h; i++) {
            uint8_t* p = pixels + i * 4;
            if (abs((int)p[0] - (int)ckR) <= 3 &&
                abs((int)p[1] - (int)ckG) <= 3 &&
                abs((int)p[2] - (int)ckB) <= 3) {
                p[3] = 0; // Transparent
            }
        }
    }

    TextureHandle handle = CreateTexture(w, h);
    SurfaceDesc desc;
    if (LockTexture(handle, &desc)) {
        // Convert RGBA -> BGRA
        uint8_t* dst = static_cast<uint8_t*>(desc.pixels);
        for (int i = 0; i < w * h; i++) {
            dst[i * 4 + 0] = pixels[i * 4 + 2]; // B
            dst[i * 4 + 1] = pixels[i * 4 + 1]; // G
            dst[i * 4 + 2] = pixels[i * 4 + 0]; // R
            dst[i * 4 + 3] = pixels[i * 4 + 3]; // A
        }
        UnlockTexture(handle);
    }
    stbi_image_free(pixels);
    UploadTextureGPU(handle);
    return handle;
}

bool VulkanRenderer::LockTexture(TextureHandle handle, SurfaceDesc* desc) {
    if (handle >= textures_.size()) return false;
    desc->width = textures_[handle].width;
    desc->height = textures_[handle].height;
    desc->pixels = textures_[handle].cpuData.data();
    desc->pitch = textures_[handle].width * 4;
    return true;
}

void VulkanRenderer::UnlockTexture(TextureHandle handle) {
    if (handle < textures_.size()) {
        textures_[handle].dirty = true;
    }
}

void VulkanRenderer::ReleaseTexture(TextureHandle handle) {
    if (handle < textures_.size() && textures_[handle].valid) {
        vkDeviceWaitIdle(device_);
        if (textures_[handle].descriptorSet != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(device_, descPool_, 1, &textures_[handle].descriptorSet);
            textures_[handle].descriptorSet = VK_NULL_HANDLE;
        }
        vkDestroyImageView(device_, textures_[handle].view, nullptr);
        vkDestroyImage(device_, textures_[handle].image, nullptr);
        vkFreeMemory(device_, textures_[handle].memory, nullptr);
        textures_[handle].image = VK_NULL_HANDLE;
        textures_[handle].valid = false;
    }
}

void VulkanRenderer::UploadTextureGPU(TextureHandle handle) {
    if (handle >= textures_.size()) return;
    auto& tex = textures_[handle];

    if (!tex.valid) {
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent.width = tex.width;
        ici.extent.height = tex.height;
        ici.extent.depth = 1;
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.format = VK_FORMAT_B8G8R8A8_UNORM;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateImage(device_, &ici, nullptr, &tex.image);

        VkMemoryRequirements memReq;
        vkGetImageMemoryRequirements(device_, tex.image, &memReq);
        VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        mai.allocationSize = memReq.size;
        mai.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device_, &mai, nullptr, &tex.memory);
        vkBindImageMemory(device_, tex.image, tex.memory, 0);

        VkImageViewCreateInfo ivci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        ivci.image = tex.image;
        ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivci.format = VK_FORMAT_B8G8R8A8_UNORM;
        ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ivci.subresourceRange.baseMipLevel = 0;
        ivci.subresourceRange.levelCount = 1;
        ivci.subresourceRange.baseArrayLayer = 0;
        ivci.subresourceRange.layerCount = 1;
        vkCreateImageView(device_, &ivci, nullptr, &tex.view);

        VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        dsai.descriptorPool = descPool_;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &descSetLayout_;
        vkAllocateDescriptorSets(device_, &dsai, &tex.descriptorSet);

        VkDescriptorImageInfo dii{};
        dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        dii.imageView = tex.view;
        dii.sampler = sampler_;

        VkWriteDescriptorSet wds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        wds.dstSet = tex.descriptorSet;
        wds.dstBinding = 0;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wds.descriptorCount = 1;
        wds.pImageInfo = &dii;
        vkUpdateDescriptorSets(device_, 1, &wds, 0, nullptr);

        tex.valid = true;
    }

    // Staging upload
    VkDeviceSize imageSize = tex.width * tex.height * 4;
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = imageSize;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCreateBuffer(device_, &bci, nullptr, &stagingBuffer);

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &req);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(device_, &mai, nullptr, &stagingMemory);
    vkBindBufferMemory(device_, stagingBuffer, stagingMemory, 0);

    void* data;
    vkMapMemory(device_, stagingMemory, 0, imageSize, 0, &data);
    memcpy(data, tex.cpuData.data(), imageSize);
    vkUnmapMemory(device_, stagingMemory);

    // Command buffer for transfer
    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = cmdPool_;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device_, &cbai, &cmd);

    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);

    // Transition Undefined -> Transfer Dst
    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { tex.width, tex.height, 1 };
    vkCmdCopyBufferToImage(cmd, stagingBuffer, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition Transfer Dst -> Shader Read Only
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);

    vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingMemory, nullptr);

    tex.dirty = false;
}

void VulkanRenderer::CaptureScreen(TextureHandle /*target*/) {
    // Post-process screen capture placeholder
}

} // namespace th06
