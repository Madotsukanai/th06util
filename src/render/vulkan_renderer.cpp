#include "vulkan_renderer.hpp"
#include "shaders_embedded.hpp"
#include "shaders_embedded_frag.hpp"
#include "shaders_embedded_fullscreen.hpp"
#include "shaders_embedded_vertical.hpp"
#include <SDL2/SDL_vulkan.h>

#include "../GameErrorContext.hpp"
#include "../utils.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "../../3rdparty/stb_image.h"

#include <cstdio>
#include <cmath>
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
VK_FUNC(vkCmdDraw)
VK_FUNC(vkCmdPushConstants)
VK_FUNC(vkCmdCopyBufferToImage)
VK_FUNC(vkCmdCopyImageToBuffer)
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
#define vkCmdDraw pvkCmdDraw
#define vkCmdPushConstants pvkCmdPushConstants
#define vkCmdCopyBufferToImage pvkCmdCopyBufferToImage
#define vkCmdCopyImageToBuffer pvkCmdCopyImageToBuffer
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
    LOAD_D(vkCmdCopyImageToBuffer);
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

    // Fence guarding one-time texture uploads.
    VkFenceCreateInfo fci{ VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    if (vkCreateFence(device_, &fci, nullptr, &uploadFence_) != VK_SUCCESS) {
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
    UpdateDestRect();
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

    if (vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_) != VK_SUCCESS) return false;

    // Clear(false) inside a scene restarts the pass but must keep the depth
    // contents written so far. Same subpass structure (pipeline compatible),
    // but the depth attachment LOADs instead of clearing.
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments = { colorAttachment, depthAttachment };
    rpci.pAttachments = attachments.data();
    if (vkCreateRenderPass(device_, &rpci, nullptr, &renderPassResume_) != VK_SUCCESS) return false;

    return true;
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

static VkShaderModule CreateShaderModuleFromSpv(VkDevice device, const uint32_t* code, size_t size);

bool VulkanRenderer::EnsureVerticalObjects(int width, int height) {
    // Vertical (portrait) composition ── mirrors the fork's D3D8 CPU blit in
    // GameWindow::Present. The scene renders into an offscreen 640x648 surface
    // (GameWindow::verticalSurface) and this pass rotates it onto the swapchain
    // via shaders/vertical.frag.
    verticalWidth_ = width;
    verticalHeight_ = height;
    {

        // Offscreen colour image (same format as the swapchain so the scene
        // pipeline stays compatible).
        VkImageCreateInfo ici{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent.width = (uint32_t)width;
        ici.extent.height = (uint32_t)height;
        ici.extent.depth = 1;
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.format = swapchainFormat_;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &ici, nullptr, &verticalColorImage_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical composition image creation failed");
            return false;
        }
        verticalWidth_ = width;
        verticalHeight_ = height;

        VkMemoryRequirements vcmReq;
        vkGetImageMemoryRequirements(device_, verticalColorImage_, &vcmReq);
        VkMemoryAllocateInfo vcm{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        vcm.allocationSize = vcmReq.size;
        vcm.memoryTypeIndex = FindMemoryType(vcmReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device_, &vcm, nullptr, &verticalColorMemory_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical composition memory allocation failed");
            return false;
        }
        vkBindImageMemory(device_, verticalColorImage_, verticalColorMemory_, 0);

        VkImageViewCreateInfo vci{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        vci.image = verticalColorImage_;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = swapchainFormat_;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &vci, nullptr, &verticalColorView_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical composition view creation failed");
            return false;
        }

        // Depth attachment of the offscreen target.
        VkImageCreateInfo di{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        di.imageType = VK_IMAGE_TYPE_2D;
        di.extent.width = (uint32_t)width;
        di.extent.height = (uint32_t)height;
        di.extent.depth = 1;
        di.mipLevels = 1;
        di.arrayLayers = 1;
        di.format = depthFormat_;
        di.tiling = VK_IMAGE_TILING_OPTIMAL;
        di.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        di.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        di.samples = VK_SAMPLE_COUNT_1_BIT;
        di.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &di, nullptr, &verticalDepthImage_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical composition depth image creation failed");
            return false;
        }
        VkMemoryRequirements vdmReq;
        vkGetImageMemoryRequirements(device_, verticalDepthImage_, &vdmReq);
        VkMemoryAllocateInfo vdm{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        vdm.allocationSize = vdmReq.size;
        vdm.memoryTypeIndex = FindMemoryType(vdmReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device_, &vdm, nullptr, &verticalDepthMemory_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical composition depth memory allocation failed");
            return false;
        }
        vkBindImageMemory(device_, verticalDepthImage_, verticalDepthMemory_, 0);

        VkImageViewCreateInfo dvi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        dvi.image = verticalDepthImage_;
        dvi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        dvi.format = depthFormat_;
        dvi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        dvi.subresourceRange.baseMipLevel = 0;
        dvi.subresourceRange.levelCount = 1;
        dvi.subresourceRange.baseArrayLayer = 0;
        dvi.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &dvi, nullptr, &verticalDepthView_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical composition depth view creation failed");
            return false;
        }

        // Renderpass: colour ends up shader-readable for the composition's
        // sampler; the depth clears each frame.
        VkAttachmentDescription vcAtt{};
        vcAtt.format = swapchainFormat_;
        vcAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        vcAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        vcAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        vcAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        vcAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        vcAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vcAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription vdAtt{};
        vdAtt.format = depthFormat_;
        vdAtt.samples = VK_SAMPLE_COUNT_1_BIT;
        vdAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        vdAtt.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        vdAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        vdAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        vdAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vdAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference vcRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
        VkAttachmentReference vdRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
        VkSubpassDescription vSub{};
        vSub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        vSub.colorAttachmentCount = 1;
        vSub.pColorAttachments = &vcRef;
        vSub.pDepthStencilAttachment = &vdRef;

        VkSubpassDependency vDep{};
        vDep.srcSubpass = VK_SUBPASS_EXTERNAL;
        vDep.dstSubpass = 0;
        vDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        vDep.srcAccessMask = 0;
        vDep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                          | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        vDep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> vAtts = { vcAtt, vdAtt };
        VkRenderPassCreateInfo vRpci{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
        vRpci.attachmentCount = static_cast<uint32_t>(vAtts.size());
        vRpci.pAttachments = vAtts.data();
        vRpci.subpassCount = 1;
        vRpci.pSubpasses = &vSub;
        vRpci.dependencyCount = 1;
        vRpci.pDependencies = &vDep;
        if (vkCreateRenderPass(device_, &vRpci, nullptr, &renderPassVertical_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical render pass creation failed");
            return false;
        }

        // Resume variant for the mid-scene Clear(false) restart: the colour is
        // still cleared, the depth keeps its content. The colour image starts
        // in SHADER_READ because the previous pass left it there.
        vcAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        vcAtt.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vcAtt.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vdAtt.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        vdAtt.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        vdAtt.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        vAtts = { vcAtt, vdAtt };
        vRpci.pAttachments = vAtts.data();
        if (vkCreateRenderPass(device_, &vRpci, nullptr, &renderPassVerticalResume_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical resume render pass creation failed");
            return false;
        }

        std::array<VkImageView, 2> vFbAtt = { verticalColorView_, verticalDepthView_ };
        VkFramebufferCreateInfo vFb{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        vFb.renderPass = renderPassVertical_;
        vFb.attachmentCount = static_cast<uint32_t>(vFbAtt.size());
        vFb.pAttachments = vFbAtt.data();
        vFb.width = (uint32_t)width;
        vFb.height = (uint32_t)height;
        vFb.layers = 1;
        if (vkCreateFramebuffer(device_, &vFb, nullptr, &verticalFramebuffer_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical framebuffer creation failed");
            return false;
        }

        verticalCompositionBuilt_ = true;
    }

    // Composition descriptor set + pipeline.
    {
        VkDescriptorSetAllocateInfo vDsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        vDsai.descriptorPool = descPool_;
        vDsai.descriptorSetCount = 1;
        vDsai.pSetLayouts = &descSetLayout_;
        if (vkAllocateDescriptorSets(device_, &vDsai, &verticalDescSet_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical descriptor set allocation failed");
            return false;
        }
        VkDescriptorImageInfo vDii{};
        vDii.sampler = samplerVerticalNearest_;
        vDii.imageView = verticalColorView_;
        vDii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet vWds{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        vWds.dstSet = verticalDescSet_;
        vWds.dstBinding = 0;
        vWds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        vWds.descriptorCount = 1;
        vWds.pImageInfo = &vDii;
        vkUpdateDescriptorSets(device_, 1, &vWds, 0, nullptr);

        struct PushConstantsCompose {
            float rotRect[4];
            float flags[4];
            float dest[4];
        };

        VkShaderModule fullscreenVertMod = CreateShaderModuleFromSpv(device_, kFullscreenVertSpv, kFullscreenVertSpvSize);
        VkShaderModule verticalFragMod = CreateShaderModuleFromSpv(device_, kVerticalFragSpv, kVerticalFragSpvSize);

        VkPipelineShaderStageCreateInfo composeStages[2]{};
        composeStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, fullscreenVertMod, "main" };
        composeStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, verticalFragMod, "main" };

        VkPipelineVertexInputStateCreateInfo composeVi{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

        VkPipelineInputAssemblyStateCreateInfo composeIa{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
        composeIa.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo composeVp{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
        composeVp.viewportCount = 1;
        composeVp.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo composeRs{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
        composeRs.polygonMode = VK_POLYGON_MODE_FILL;
        composeRs.cullMode = VK_CULL_MODE_NONE;
        composeRs.lineWidth = 1.0f;

        VkPipelineMultisampleStateCreateInfo composeMs{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
        composeMs.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineDepthStencilStateCreateInfo composeDs{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
        composeDs.depthTestEnable = VK_FALSE;
        composeDs.depthWriteEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState composeCb{};
        composeCb.blendEnable = VK_FALSE;
        composeCb.colorWriteMask = 0xF;
        VkPipelineColorBlendStateCreateInfo composeCbs{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
        composeCbs.attachmentCount = 1;
        composeCbs.pAttachments = &composeCb;

        VkDynamicState composeDynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo composeDyn{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
        composeDyn.dynamicStateCount = 2;
        composeDyn.pDynamicStates = composeDynStates;

        VkPushConstantRange composePc{};
        composePc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        composePc.offset = 0;
        composePc.size = sizeof(PushConstantsCompose);
        VkPipelineLayoutCreateInfo composePl{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        composePl.setLayoutCount = 1;
        composePl.pSetLayouts = &descSetLayout_;
        composePl.pushConstantRangeCount = 1;
        composePl.pPushConstantRanges = &composePc;
        if (vkCreatePipelineLayout(device_, &composePl, nullptr, &composePipelineLayout_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical pipeline layout creation failed");
            return false;
        }

        VkGraphicsPipelineCreateInfo composeGp{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
        composeGp.stageCount = 2;
        composeGp.pStages = composeStages;
        composeGp.pVertexInputState = &composeVi;
        composeGp.pInputAssemblyState = &composeIa;
        composeGp.pViewportState = &composeVp;
        composeGp.pRasterizationState = &composeRs;
        composeGp.pMultisampleState = &composeMs;
        composeGp.pDepthStencilState = &composeDs;
        composeGp.pColorBlendState = &composeCbs;
        composeGp.pDynamicState = &composeDyn;
        composeGp.layout = composePipelineLayout_;
        composeGp.renderPass = renderPass_;
        composeGp.subpass = 0;
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &composeGp, nullptr, &composePipeline_) != VK_SUCCESS) {
            g_GameErrorContext.Fatal("[Vulkan] vertical pipeline creation failed");
            return false;
        }

        vkDestroyShaderModule(device_, fullscreenVertMod, nullptr);
        vkDestroyShaderModule(device_, verticalFragMod, nullptr);
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
    poolSize.descriptorCount = 2048;

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 2048;
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
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) != VK_SUCCESS) {
        return false;
    }

    // Same as above except wrap mode: textures whose sprites reach the edge of
    // the texture must clamp, because bilinear fine sampling at 2.25x can reach
    // slightly past the edge and REPEAT would teleport the sample to the far
    // side of the texture (visible as dark/crossing lines around full-texture
    // sprites).
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &samplerClamp_) != VK_SUCCESS) {
        return false;
    }

    // NEAREST variant for the vertical composition pass (pixel-exact blit).
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    return vkCreateSampler(device_, &samplerInfo, nullptr, &samplerVerticalNearest_) == VK_SUCCESS;
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
    VkShaderModule spriteFragMod = CreateShaderModuleFromSpv(device_, kSpriteFrag2Spv, kSpriteFrag2SpvSize);

    VkPipelineShaderStageCreateInfo spriteStages[2]{};
    spriteStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, spriteVertMod, "main" };
    spriteStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, spriteFragMod, "main" };

    VkVertexInputBindingDescription binding2D{ 0, sizeof(Vertex2D), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attribs2D[3]{};
    attribs2D[0] = { 0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, x) };
    attribs2D[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex2D, u) };
    attribs2D[2] = { 2, 0, VK_FORMAT_B8G8R8A8_UNORM, offsetof(Vertex2D, color) };

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
    VkShaderModule stageFragMod = CreateShaderModuleFromSpv(device_, kStage3dFrag2Spv, kStage3dFrag2SpvSize);

    VkPipelineShaderStageCreateInfo stageStages[2]{};
    stageStages[0] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, stageVertMod, "main" };
    stageStages[1] = { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, stageFragMod, "main" };

    VkVertexInputBindingDescription binding3D{ 0, sizeof(Vertex3D), VK_VERTEX_INPUT_RATE_VERTEX };
    VkVertexInputAttributeDescription attribs3D[3]{};
    attribs3D[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex3D, x) };
    attribs3D[1] = { 1, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex3D, u) };
    attribs3D[2] = { 2, 0, VK_FORMAT_B8G8R8A8_UNORM,   offsetof(Vertex3D, color) };

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
    gp3D.pColorBlendState = &cbsSub;
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp3D, nullptr, &stagePipelineSub_);

    // Same pipelines with depth writes disabled (AnmVm zWriteDisable).
    ds3D.depthWriteEnable = VK_FALSE;
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp3D, nullptr, &stagePipelineAddNoZ_);
    gp3D.pColorBlendState = &cbsAlpha;
    vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &gp3D, nullptr, &stagePipelineAlphaNoZ_);

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
        vkDestroyPipeline(device_, stagePipelineSub_, nullptr);
        vkDestroyPipeline(device_, stagePipelineAlphaNoZ_, nullptr);
        vkDestroyPipeline(device_, stagePipelineAddNoZ_, nullptr);
        vkDestroyPipelineLayout(device_, stagePipelineLayout_, nullptr);

        ReleaseVerticalComposition();
        ReleaseStaging();
        if (uploadFence_ != VK_NULL_HANDLE) {
            vkDestroyFence(device_, uploadFence_, nullptr);
            uploadFence_ = VK_NULL_HANDLE;
        }

        vkDestroySampler(device_, sampler_, nullptr);
        vkDestroySampler(device_, samplerClamp_, nullptr);
        vkDestroySampler(device_, samplerVerticalNearest_, nullptr);
        vkDestroyDescriptorPool(device_, descPool_, nullptr);
        vkDestroyDescriptorSetLayout(device_, descSetLayout_, nullptr);

        for (auto fb : framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
        framebuffers_.clear();

        vkDestroyImageView(device_, depthImageView_, nullptr);
        vkDestroyImage(device_, depthImage_, nullptr);
        vkFreeMemory(device_, depthMemory_, nullptr);

        vkDestroyRenderPass(device_, renderPass_, nullptr);
        vkDestroyRenderPass(device_, renderPassResume_, nullptr);

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
    // The swapchain is rebuilt at the start of the next frame.
    swapchainDirty_ = true;
}

void VulkanRenderer::UpdateDestRect() {
    // Letter-box the 640x480 game image into the swapchain, preserving the aspect ratio.
    const float ew = static_cast<float>(swapchainExtent_.width);
    const float eh = static_cast<float>(swapchainExtent_.height);
    const float scale = std::min(ew / 640.0f, eh / 480.0f);
    destW_ = 640.0f * scale;
    destH_ = 480.0f * scale;
    destX_ = std::floor((ew - destW_) * 0.5f);
    destY_ = std::floor((eh - destH_) * 0.5f);
}

void VulkanRenderer::CleanupSwapchain() {
    for (auto fb : framebuffers_) vkDestroyFramebuffer(device_, fb, nullptr);
    framebuffers_.clear();

    if (depthImageView_ != VK_NULL_HANDLE) vkDestroyImageView(device_, depthImageView_, nullptr);
    if (depthImage_ != VK_NULL_HANDLE) vkDestroyImage(device_, depthImage_, nullptr);
    if (depthMemory_ != VK_NULL_HANDLE) vkFreeMemory(device_, depthMemory_, nullptr);
    depthImageView_ = VK_NULL_HANDLE;
    depthImage_ = VK_NULL_HANDLE;
    depthMemory_ = VK_NULL_HANDLE;

    for (auto iv : swapchainImageViews_) vkDestroyImageView(device_, iv, nullptr);
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_ != VK_NULL_HANDLE) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

bool VulkanRenderer::RecreateSwapchain() {
    int dw = 0, dh = 0;
    SDL_Vulkan_GetDrawableSize(window_, &dw, &dh);
    if (dw <= 0 || dh <= 0) {
        return false; // minimized: keep the dirty flag and retry next frame
    }

    vkDeviceWaitIdle(device_);
    CleanupSwapchain();
    if (!CreateSwapchain() || !CreateDepthBuffer() || !CreateFramebuffers()) {
        fprintf(stderr, "[Vulkan] Failed to recreate swapchain\n");
        return false;
    }
    swapchainDirty_ = false;
    return true;
}


static constexpr uint32_t VERTEX_BUFFER_BYTES = MAX_VERTICES_PER_FRAME * sizeof(Vertex3D);



// ── Frame Lifecycle ──────────────────────────────────────────

void VulkanRenderer::BeginScene() {
    if (inScene_) return;

    if (swapchainDirty_ && !RecreateSwapchain()) return;
    if (swapchain_ == VK_NULL_HANDLE) return;

    auto& frame = frames_[currentFrame_];
    vkWaitForFences(device_, 1, &frame.inFlight, VK_TRUE, UINT64_MAX);

    VkResult res = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &currentImageIndex_);
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        RecreateSwapchain();
        return;
    }
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        return;
    }
    if (res == VK_SUBOPTIMAL_KHR) {
        swapchainDirty_ = true; // rebuild before the next frame
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
    rpbi.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpbi.pClearValues = clearValues.data();
    if (verticalCompositionActive_ && verticalCompositionBuilt_) {
        // Route the whole scene into the offscreen portrait surface; Present()
        // composes it onto the swapchain.
        rpbi.renderPass = renderPassVertical_;
        rpbi.framebuffer = verticalFramebuffer_;
        rpbi.renderArea.offset = { 0, 0 };
        rpbi.renderArea.extent = { static_cast<uint32_t>(verticalWidth_), static_cast<uint32_t>(verticalHeight_) };
    } else {
        rpbi.renderPass = renderPass_;
        rpbi.framebuffer = framebuffers_[currentImageIndex_];
        rpbi.renderArea.offset = { 0, 0 };
        rpbi.renderArea.extent = swapchainExtent_;
    }

    vkCmdBeginRenderPass(frame.cmdBuffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    passActive_ = true;

    ApplyDrawViewport(false, 0.0f, 1.0f);

    vertexCount_ = 0;
    indexCount_ = 0;
    batches2D_.clear();
    inScene_ = true;
    frameReady_ = false;
}

void VulkanRenderer::EndScene() {
    if (!inScene_) return;
    extern int g_snifferFlashArmed; // DIAG(sniff), defined below near SniffSuspectQuad

    FlushBatch2D();

    auto& frame = frames_[currentFrame_];
    if (passActive_) {
        vkCmdEndRenderPass(frame.cmdBuffer);
        passActive_ = false;
    }
    vkEndCommandBuffer(frame.cmdBuffer);

    inScene_ = false;
    frameReady_ = true;
    if (g_snifferFlashArmed > 0) {
        g_snifferFlashArmed--;
    }
}

void VulkanRenderer::Present() {
    if (!frameReady_)
    {
        BeginScene();
        EndScene();
        if (!frameReady_) return;
    }
    auto& frame = frames_[currentFrame_];

    if (verticalCompositionActive_ && verticalCompositionBuilt_) {
        // ── Vertical composition: rotate the offscreen portrait render onto
        // the swapchain image, matching the fork's CPU blit in
        // GameWindow::Present (see shaders/vertical.frag). ──
        struct PushConstantsCompose {
            float rotRect[4];
            float flags[4];
            float dest[4];
        };

        // destRect = the scaled fit of the rotated region inside the client.
        float rotW = (verticalCompositionInGame_ ? (float)(632u) : 640.0f);
        float rotH = (verticalCompositionInGame_ ? 384.0f : 480.0f);
        float destW = (float)swapchainExtent_.width;
        float destH = destW * rotH / rotW;
        if (destH > (float)swapchainExtent_.height) {
            destH = (float)swapchainExtent_.height;
            destW = destH * rotW / rotH;
        }
        float destX = ((float)swapchainExtent_.width - destW) * 0.5f;
        float destY = ((float)swapchainExtent_.height - destH) * 0.5f;

        std::array<VkClearValue, 2> composeClear{};
        composeClear[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
        composeClear[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo cRp{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
        cRp.renderPass = renderPass_;
        cRp.framebuffer = framebuffers_[currentImageIndex_];
        cRp.renderArea.offset = { 0, 0 };
        cRp.renderArea.extent = swapchainExtent_;
        cRp.clearValueCount = static_cast<uint32_t>(composeClear.size());
        cRp.pClearValues = composeClear.data();
        vkCmdBeginRenderPass(frame.cmdBuffer, &cRp, VK_SUBPASS_CONTENTS_INLINE);

        // inScene_ is false by now, so set the full-swapchain viewport directly.
        VkViewport cvp{ 0.0f, 0.0f, (float)swapchainExtent_.width, (float)swapchainExtent_.height, 0.0f, 1.0f };
        VkRect2D csc{ { 0, 0 }, swapchainExtent_ };
        vkCmdSetViewport(frame.cmdBuffer, 0, 1, &cvp);
        vkCmdSetScissor(frame.cmdBuffer, 0, 1, &csc);

        vkCmdBindPipeline(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, composePipeline_);
        vkCmdBindDescriptorSets(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, composePipelineLayout_, 0, 1,
                                &verticalDescSet_, 0, nullptr);

        PushConstantsCompose pc{};
        pc.rotRect[0] = rotW;
        pc.rotRect[1] = rotH;
        pc.flags[0] = verticalCompositionInGame_ ? 1.0f : 0.0f;
        pc.flags[1] = verticalCompositionFlip180_ ? 1.0f : 0.0f;
        pc.flags[2] = verticalCompositionTransitionBlack_ ? 1.0f : 0.0f;
        pc.dest[0] = destX;
        pc.dest[1] = destY;
        pc.dest[2] = destW;
        pc.dest[3] = destH;
        vkCmdPushConstants(frame.cmdBuffer, composePipelineLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
        vkCmdDraw(frame.cmdBuffer, 3, 1, 0, 0);

        vkCmdEndRenderPass(frame.cmdBuffer);
        verticalCompositionTransitionBlack_ = false;
    }

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

    VkResult presentRes = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
        swapchainDirty_ = true;
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    frameReady_ = false;
}

void VulkanRenderer::Clear(uint32_t argb, bool clearDepth) {
    clearColor_[0] = ((argb >> 16) & 0xFF) / 255.0f;
    clearColor_[1] = ((argb >>  8) & 0xFF) / 255.0f;
    clearColor_[2] = ((argb >>  0) & 0xFF) / 255.0f;
    clearColor_[3] = ((argb >> 24) & 0xFF) / 255.0f;
    clearDepth_ = clearDepth;

    if (!inScene_ || !passActive_)
    {
        // Usual case: the colour is consumed by the next BeginScene. D3D8 keeps
        // the clear colour until the next target clear, so store and return.
        return;
    }

    // Mid-scene Clear: D3D8 clears the targets immediately. Restart the render
    // pass around the clear so pending batches stay ordered after it.
    auto& frame = frames_[currentFrame_];
    FlushBatch2D();
    vkCmdEndRenderPass(frame.cmdBuffer);
    passActive_ = false;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = { { clearColor_[0], clearColor_[1], clearColor_[2], clearColor_[3] } };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo rpbi{ VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    if (verticalCompositionActive_ && verticalCompositionBuilt_) {
        rpbi.renderPass = clearDepth ? renderPassVertical_ : renderPassVerticalResume_;
        rpbi.framebuffer = verticalFramebuffer_;
        rpbi.renderArea.offset = { 0, 0 };
        rpbi.renderArea.extent = { static_cast<uint32_t>(verticalWidth_), static_cast<uint32_t>(verticalHeight_) };
    } else {
        rpbi.renderPass = clearDepth ? renderPass_ : renderPassResume_;
        rpbi.framebuffer = framebuffers_[currentImageIndex_];
        rpbi.renderArea.offset = { 0, 0 };
        rpbi.renderArea.extent = swapchainExtent_;
    }
    rpbi.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpbi.pClearValues = clearValues.data();
    vkCmdBeginRenderPass(frame.cmdBuffer, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    passActive_ = true;
    ApplyDrawViewport(false, 0.0f, 1.0f);
}

void VulkanRenderer::SetViewport(int x, int y, int width, int height) {
    SetGameViewport(x, y, width, height, 0.0f, 1.0f);
}

void VulkanRenderer::SetGameViewport(int x, int y, int width, int height, float minZ, float maxZ) {
    gameViewportX_ = x;
    gameViewportY_ = y;
    gameViewportW_ = width;
    gameViewportH_ = height;
    gameViewportMinZ_ = minZ;
    gameViewportMaxZ_ = maxZ;
}

// Maps a rectangle in 640x480 backbuffer coordinates onto the letter-boxed
// game region of the swapchain and installs it as the dynamic viewport/scissor.
// With useGameViewport the current game viewport (SetViewport forwarding) is
// used instead, clipped to the backbuffer like D3D8 does.
void VulkanRenderer::ApplyDrawViewport(bool useGameViewport, float minZ, float maxZ) {
    if (!inScene_) return;

    const bool verticalActive = verticalCompositionActive_ && verticalCompositionBuilt_;
    const int baseW = verticalActive ? verticalWidth_ : 640;
    const int baseH = verticalActive ? verticalHeight_ : 480;

    int gx0 = 0, gy0 = 0, gx1 = baseW, gy1 = baseH;
    if (useGameViewport) {
        gx0 = std::max(0, gameViewportX_);
        gy0 = std::max(0, gameViewportY_);
        gx1 = std::min(baseW, gameViewportX_ + gameViewportW_);
        gy1 = std::min(baseH, gameViewportY_ + gameViewportH_);
        if (gx1 <= gx0 || gy1 <= gy0) {
            gx0 = 0; gy0 = 0; gx1 = baseW; gy1 = baseH;
        }
    }

    float sx, sy, offX, offY;
    uint32_t fbW, fbH;
    if (verticalActive) {
        // Game coordinates map 1:1 onto the offscreen portrait surface.
        sx = 1.0f;
        sy = 1.0f;
        offX = 0.0f;
        offY = 0.0f;
        fbW = static_cast<uint32_t>(verticalWidth_);
        fbH = static_cast<uint32_t>(verticalHeight_);
    } else {
        sx = destW_ / 640.0f;
        sy = destH_ / 480.0f;
        offX = destX_;
        offY = destY_;
        fbW = swapchainExtent_.width;
        fbH = swapchainExtent_.height;
    }
    float vx = offX + static_cast<float>(gx0) * sx;
    float vy = offY + static_cast<float>(gy0) * sy;
    float vw = static_cast<float>(gx1 - gx0) * sx;
    float vh = static_cast<float>(gy1 - gy0) * sy;
    float tz = useGameViewport ? minZ : 0.0f;
    float bz = useGameViewport ? maxZ : 1.0f;

    int32_t scx = static_cast<int32_t>(vx);
    int32_t scy = static_cast<int32_t>(vy);
    uint32_t scw = static_cast<uint32_t>(std::max(1.0f, vw));
    uint32_t sch = static_cast<uint32_t>(std::max(1.0f, vh));
    if (scx < 0) scx = 0;
    if (scy < 0) scy = 0;
    if (scx + scw > static_cast<int32_t>(fbW)) scw = fbW - scx;
    if (scy + sch > static_cast<int32_t>(fbH)) sch = fbH - scy;

    VkViewport vp{ vx, vy, std::max(1.0f, vw), std::max(1.0f, vh), tz, bz };
    VkRect2D scissor{ { scx, scy }, { scw, sch } };
    vkCmdSetViewport(frames_[currentFrame_].cmdBuffer, 0, 1, &vp);
    vkCmdSetScissor(frames_[currentFrame_].cmdBuffer, 0, 1, &scissor);
}

void VulkanRenderer::ApplyGameViewport() {
    ApplyDrawViewport(true, gameViewportMinZ_, gameViewportMaxZ_);
}

// ── 2D Drawing ───────────────────────────────────────────────

// DIAG(sniff): catch quads whose vertex colour is green-dominant (emerald) or
// near-black while alpha stays opaque — the suspected missing pieces behind the
// stage-3/emerald & stage-6/black boss-hit flashes. Caps keep the log bounded.
// While g_snifferFlashArmed > 0 (set by EnemyManager on boss damage frames),
// every quad is logged unfiltered for a short window so whatever is actually
// drawn on the hit frame can be identified directly.
int g_snifferFlashArmed = 0;

static void SniffSuspectQuad(const char *tag, uint32_t tex, int blend, uint32_t c0, uint32_t c2, float px, float py,
                             float pz, float u0, float v0, float u2, float v2)
{
    static int sGreen = 0;
    static int sBlack = 0;
    static int sFlash = 0;
    uint32_t r0 = (c0 >> 16) & 255, g0 = (c0 >> 8) & 255, b0 = c0 & 255, a0 = (c0 >> 24) & 255;
    uint32_t r2 = (c2 >> 16) & 255, g2 = (c2 >> 8) & 255, b2 = c2 & 255;
    bool green = (g0 >= 80 && r0 * 2 <= g0 && b0 * 2 <= g0);
    bool green2 = (g2 >= 80 && r2 * 2 <= g2 && b2 * 2 <= g2);
    bool black = (r0 <= 14 && g0 <= 14 && b0 <= 14 && a0 >= 16);
    // Teal/green-blue blobs from the hit screenshot: g and b both bright
    // while red lags far behind (screenshots show ~ (32..64, 112, 96)).
    bool teal = (g0 >= 0x60 && b0 >= 0x60 && r0 * 3 <= g0 && r0 * 3 <= b0);
    bool teal2 = (g2 >= 0x60 && b2 >= 0x60 && r2 * 3 <= g2 && r2 * 3 <= b2);
    static int sTeal = 0;
    if ((teal || teal2) && sTeal < 400)
    {
        sTeal++;
        if ((sTeal & 7) == 1)
            th06::utils::LogDebug(
                "SNIFFT %s tex=%u blend=%d c0=0x%08x c2=0x%08x pos=(%.0f,%.0f,%.0f) uv=(%.3f,%.3f)-(%.3f,%.3f)", tag,
                tex, blend, c0, c2, px, py, pz, u0, v0, u2, v2);
    }
    if (g_snifferFlashArmed > 0 && sFlash < 800)
    {
        sFlash++;
        if ((sFlash & 7) == 1)
            th06::utils::LogDebug(
                "FLASH %s tex=%u blend=%d c0=0x%08x c2=0x%08x pos=(%.0f,%.0f,%.0f) uv=(%.3f,%.3f)-(%.3f,%.3f) armed=%d",
                tag, tex, blend, c0, c2, px, py, pz, u0, v0, u2, v2, (int)g_snifferFlashArmed);
    }
    if ((green || green2) && sGreen < 600)
    {
        sGreen++;
        if ((sGreen & 15) == 1)
            th06::utils::LogDebug("SNIFFG %s tex=%u blend=%d c0=0x%08x c2=0x%08x pos=(%.0f,%.0f,%.0f)", tag, tex,
                                  blend, c0, c2, px, py, pz);
    }
    if (black && sBlack < 600)
    {
        sBlack++;
        if ((sBlack & 63) == 1)
            th06::utils::LogDebug("SNIFFK %s tex=%u blend=%d c0=0x%08x pos=(%.0f,%.0f,%.0f)", tag, tex, blend, c0, px,
                                  py, pz);
    }
}

void VulkanRenderer::DrawQuad2D(TextureHandle tex, const Vertex2D v[4], BlendMode blend) {
    if (!inScene_) return;
    if (tex >= textures_.size() || !textures_[tex].valid) {
        tex = whiteTexture_;
    }
    if (tex == INVALID_TEXTURE) return;
    SniffSuspectQuad("2D", tex, (int)blend, v[0].color, v[2].color, v[0].x, v[0].y, 0.0f, v[0].u, v[0].v, v[2].u, v[2].v);

    if (textures_[tex].dirty) {
        UploadTextureGPU(tex);
    }

    if (!batches2D_.empty()) {
        auto& last = batches2D_.back();
        if (last.texture != tex || last.blendMode != blend || last.colorOpAdd != colorOpAdd_) {
            FlushBatch2D();
        }
    }
    if (batches2D_.empty()) {
        Batch2D nb{};
        nb.texture = tex;
        nb.blendMode = blend;
        nb.colorOpAdd = colorOpAdd_;
        nb.indexStart = indexCount_;
        nb.indexCount = 0;
        batches2D_.push_back(nb);
    }

    auto& frame = frames_[currentFrame_];
    if (vertexCount_ + 4 > MAX_VERTICES_PER_FRAME || indexCount_ + 6 > MAX_INDICES_PER_FRAME) {
        FlushBatch2D();
        Batch2D nb{};
        nb.texture = tex;
        nb.blendMode = blend;
        nb.colorOpAdd = colorOpAdd_;
        nb.indexStart = indexCount_;
        nb.indexCount = 0;
        batches2D_.push_back(nb);
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
    SniffSuspectQuad("CF", whiteTexture_, (int)blend, v[0].color, v[2].color, v[0].x, v[0].y, 0.0f, v[0].u, v[0].v, v[2].u, v[2].v);
    DrawQuad2D(whiteTexture_, v, blend);
}

void VulkanRenderer::DrawTriangleStrip2D(TextureHandle tex, const Vertex2D* verts, size_t count, BlendMode blend) {
    if (count < 3) return;
    if (tex >= textures_.size() || !textures_[tex].valid) tex = whiteTexture_;
    if (tex == INVALID_TEXTURE) return;
    SniffSuspectQuad("TS", tex, (int)blend, verts[0].color, verts[1].color, verts[0].x, verts[0].y, 0.0f, verts[0].u, verts[0].v, verts[1].u, verts[1].v);
    if (textures_[tex].dirty) UploadTextureGPU(tex);

    size_t numTriangles = count - 2;
    size_t numIndices = numTriangles * 3;

    if (!batches2D_.empty()) {
        auto& last = batches2D_.back();
        if (last.texture != tex || last.blendMode != blend || last.colorOpAdd != colorOpAdd_) {
            FlushBatch2D();
        }
    }
    if (batches2D_.empty()) {
        Batch2D nb{};
        nb.texture = tex;
        nb.blendMode = blend;
        nb.colorOpAdd = colorOpAdd_;
        nb.indexStart = indexCount_;
        nb.indexCount = 0;
        batches2D_.push_back(nb);
    }

    auto& frame = frames_[currentFrame_];
    if (vertexCount_ + count > MAX_VERTICES_PER_FRAME || indexCount_ + numIndices > MAX_INDICES_PER_FRAME) {
        FlushBatch2D();
        Batch2D nb{};
        nb.texture = tex;
        nb.blendMode = blend;
        nb.colorOpAdd = colorOpAdd_;
        nb.indexStart = indexCount_;
        nb.indexCount = 0;
        batches2D_.push_back(nb);
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
        if (last.texture != tex || last.blendMode != blend || last.colorOpAdd != colorOpAdd_) {
            FlushBatch2D();
        }
    }
    if (batches2D_.empty()) {
        Batch2D nb{};
        nb.texture = tex;
        nb.blendMode = blend;
        nb.colorOpAdd = colorOpAdd_;
        nb.indexStart = indexCount_;
        nb.indexCount = 0;
        batches2D_.push_back(nb);
    }

    auto& frame = frames_[currentFrame_];
    if (vertexCount_ + count > MAX_VERTICES_PER_FRAME || indexCount_ + numIndices > MAX_INDICES_PER_FRAME) {
        FlushBatch2D();
        Batch2D nb{};
        nb.texture = tex;
        nb.blendMode = blend;
        nb.colorOpAdd = colorOpAdd_;
        nb.indexStart = indexCount_;
        nb.indexCount = 0;
        batches2D_.push_back(nb);
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

    // 2D quads are transformed in the 640x480 backbuffer space, so draw them
    // into the letter-boxed game region (the current game viewport clipped to
    // the backbuffer already being part of that region).
    ApplyDrawViewport(false, 0.0f, 1.0f);

    auto& frame = frames_[currentFrame_];
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(frame.cmdBuffer, 0, 1, &frame.vertexBuffer, &offset);
    vkCmdBindIndexBuffer(frame.cmdBuffer, frame.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

    PushConstants2D pcBase{};
    // Orthographic projection: [0, W] x [0, H] -> [-1, 1] x [-1, 1]
    // Vulkan Y is downwards (0 at top, 1 at bottom in NDC if using standard viewport)
    // In vertical (portrait) mode the drawing space is the offscreen surface
    // (640x648), so the clip maps those heights.
    const float projH = (verticalCompositionActive_ && verticalCompositionBuilt_)
                            ? static_cast<float>(verticalHeight_)
                            : 480.0f;
    pcBase.projMat[0]  =  2.0f / 640.0f;
    pcBase.projMat[5]  =  2.0f / projH;
    pcBase.projMat[10] =  1.0f;
    pcBase.projMat[12] = -1.0f;
    pcBase.projMat[13] = -1.0f;
    pcBase.projMat[15] =  1.0f;
    pcBase.colorKeyEnable = 0.0f;

    for (const auto& batch : batches2D_) {
        if (batch.indexCount == 0) continue;

        VkPipeline pipeline = spritePipelineAlpha_;
        if (batch.blendMode == BLEND_ADD) pipeline = spritePipelineAdd_;
        else if (batch.blendMode == BLEND_SUB) pipeline = spritePipelineSub_;

        PushConstants2D pc = pcBase;
        pc.colorOpAdd = batch.colorOpAdd ? 1.0f : 0.0f;
        // D3D8 texel-center origin correction (see sprite.frag). Quads whose
        // UVs address texel boundaries (e.g. v = 0.5 for the music room list
        // rows) must sample the texel centre instead of straddling two rows.
        if (batch.texture < textures_.size() && textures_[batch.texture].valid) {
            pc.halfTexelU = 0.5f / static_cast<float>(textures_[batch.texture].width);
            pc.halfTexelV = 0.5f / static_cast<float>(textures_[batch.texture].height);
        } else {
            pc.halfTexelU = 0.0f;
            pc.halfTexelV = 0.0f;
        }
        vkCmdPushConstants(frame.cmdBuffer, spritePipelineLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants2D), &pc);

        vkCmdBindPipeline(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        if (batch.texture < textures_.size() && textures_[batch.texture].valid) {
            vkCmdBindDescriptorSets(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    spritePipelineLayout_, 0, 1,
                                    &textures_[batch.texture].descriptorSet2D, 0, nullptr);
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
    if (!inScene_) return;
    FlushBatch2D();

    // 3D stage geometry is projected through the arcade-region camera, so it
    // must be drawn into the game viewport (with matching depth range/scissor).
    ApplyGameViewport();

    if (tex >= textures_.size() || !textures_[tex].valid) tex = whiteTexture_;
    if (tex == INVALID_TEXTURE) return;
    SniffSuspectQuad("3D", tex, (int)blend, v[0].color, v[2].color, v[0].x, v[0].y, v[0].z, v[0].u, v[0].v, v[2].u, v[2].v);

    // DIAG(fogw): compute clip-space w per 3D quad on the CPU and log the
    // fog outcome for every draw whose w makes it fully fogged (w >= far) or
    // unfogged, so the boss-damage black/green flash can be pinned on the
    // fog-mix path if applicable.
    {
        float w0 = v[0].x * viewProjMatrix_[3] + v[0].y * viewProjMatrix_[7] + v[0].z * viewProjMatrix_[11] + viewProjMatrix_[15];
        static int sFogWCount = 0;
        bool fullyFogged = fogEnable_ && fogFar_ > fogNear_ && w0 >= fogFar_;
        bool fullyClear = fogEnable_ && fogFar_ > fogNear_ && w0 <= fogNear_;
        if ((fullyFogged || fullyClear) && sFogWCount < 160)
        {
            sFogWCount++;
            if ((sFogWCount & 15) == 1)
            {
                th06::utils::LogDebug(
                    "FOGW tex=%u blend=%d w=%.1f near=%.0f far=%.0f %s fog=0x%08x col=0x%08x pos=(%.0f,%.0f,%.0f)",
                    (int)tex, (int)blend, (double)w0, (double)fogNear_, (double)fogFar_,
                    fullyFogged ? "FULLFOG" : (fullyClear ? "CLEAR" : "PARTIAL"), (u32)fogColor_, (u32)v[0].color,
                    (double)v[0].x, (double)v[0].y, (double)v[0].z);
            }
        }
    }

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
    pc3D.colorOpAdd = colorOpAdd_ ? 1.0f : 0.0f;
    // D3D8 texel-centre correction (see sprite.frag / PushConstants3D): matches
    // the 2D path so Draw3 quads sample texel centres exactly like vanilla D3D8.
    if (tex < textures_.size() && textures_[tex].valid) {
        pc3D.halfTexelU = 0.5f / static_cast<float>(textures_[tex].width);
        pc3D.halfTexelV = 0.5f / static_cast<float>(textures_[tex].height);
    } else {
        pc3D.halfTexelU = 0.0f;
        pc3D.halfTexelV = 0.0f;
    }

    vkCmdPushConstants(frame.cmdBuffer, stagePipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants3D), &pc3D);

    // Pipeline selection mirrors D3D8: blend mode picks the colour-blend
    // state, the current D3DRS_ZWRITEENABLE picks the depth-write variant.
    VkPipeline pipeline;
    if (blend == BLEND_SUB) pipeline = stagePipelineSub_;
    else if (blend == BLEND_ADD) pipeline = depthWrite_ ? stagePipelineAdd_ : stagePipelineAddNoZ_;
    else pipeline = depthWrite_ ? stagePipelineAlpha_ : stagePipelineAlphaNoZ_;
    vkCmdBindPipeline(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // TEMP diagnostics: periodically log the fog state actually pushed for 3D
    // draws, so fog misconfiguration (e.g. stale black fog) can be correlated
    // with the stage tiles that render black.
    {
        static u32 s_stageStateDiagCount = 0;
        if (s_stageStateDiagCount < 50)
        {
            static u32 s_stageStateDiagTick = 0;
            if (++s_stageStateDiagTick >= 400)
            {
                s_stageStateDiagTick = 0;
                s_stageStateDiagCount++;
                th06::utils::LogDebug("STAGE3D fogEn=%d fog=0x%08x near=%.0f far=%.0f blend=%d add=%d tex=%d",
                                      (int)fogEnable_, (u32)fogColor_, (double)fogNear_, (double)fogFar_, (int)blend,
                                      (int)colorOpAdd_, (int)tex);
            }
        }
    }

    vkCmdBindDescriptorSets(frame.cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            stagePipelineLayout_, 0, 1,
                            &textures_[tex].descriptorSet, 0, nullptr);

    vkCmdDrawIndexed(frame.cmdBuffer, 6, 1, indexCount_, 0, 0);

    vertexCount_ += 4;
    indexCount_ += 6;
}

// ── Texture Management ───────────────────────────────────────

TextureHandle VulkanRenderer::CreateTexture(uint32_t width, uint32_t height, bool clampToEdge) {
    TextureResource res;
    res.width = width;
    res.height = height;
    res.cpuData.resize(width * height * 4, 0);
    res.dirty = true;
    res.valid = false;
    res.clampToEdge = clampToEdge;

    // Find free slot. Slot 0 is reserved for the white texture so that a
    // valid texture handle is never 0 (the game casts handles to pointers and
    // treats a null pointer as a failed texture creation).
    //
    // NOTE: only a slot never handed out (or explicitly released) may be
    // reused. Testing "not uploaded" instead would steal slots from textures
    // the engine created but has not uploaded yet (e.g. the '@' placeholder
    // texture of text.anm), which aliasing-merges two game textures into one
    // resource.
    for (size_t i = 1; i < textures_.size(); i++) {
        if (!textures_[i].inUse) {
            res.inUse = true;
            textures_[i] = std::move(res);
            return i;
        }
    }
    res.inUse = true;
    textures_.push_back(std::move(res));
    return textures_.size() - 1;
}

TextureHandle VulkanRenderer::CreateTextureFromMemory(const void* data, size_t size, uint32_t colorKey,
                                                      bool clampToEdge) {
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

    TextureHandle handle = CreateTexture(w, h, clampToEdge);
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
    // DIAG(texload): average colour per texture upload to verify channel order.
    {
        static int sCount = 0;
        if (sCount < 160)
        {
            sCount++;
            u32 ar = 0, ag = 0, ab = 0;
            for (int i = 0; i < w * h; i++)
            {
                ar += pixels[i * 4 + 0];
                ag += pixels[i * 4 + 1];
                ab += pixels[i * 4 + 2];
            }
            u32 n = (u32)(w * h);
            int cx = w / 2, cy = h / 2;
            u8 *cp = pixels + (cy * w + cx) * 4;
            th06::utils::LogDebug("TEXLOAD %ux%u avg=(%u,%u,%u) center=(%u,%u,%u)", (unsigned)w, (unsigned)h,
                                  ar / n, ag / n, ab / n, cp[0], cp[1], cp[2]);
        }
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
    if (handle < textures_.size() && textures_[handle].inUse) {
        vkDeviceWaitIdle(device_);
        if (textures_[handle].descriptorSet != VK_NULL_HANDLE) {
            VkDescriptorSet sets[2] = { textures_[handle].descriptorSet, textures_[handle].descriptorSet2D };
            vkFreeDescriptorSets(device_, descPool_, 2, sets);
        }
        if (textures_[handle].view != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, textures_[handle].view, nullptr);
        }
        if (textures_[handle].image != VK_NULL_HANDLE) {
            vkDestroyImage(device_, textures_[handle].image, nullptr);
            vkFreeMemory(device_, textures_[handle].memory, nullptr);
        }
        // Reset the whole record so the slot can be handed out again without
        // carrying stale descriptors/cpu data from the previous occupant.
        textures_[handle] = TextureResource{};
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

        // Known-good path: two separate count-1 allocations (a combined count-2
        // allocation crashed in the loader/Unix bridge on some systems).
        VkDescriptorSetAllocateInfo dsai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        dsai.descriptorPool = descPool_;
        dsai.descriptorSetCount = 1;
        dsai.pSetLayouts = &descSetLayout_;
        vkAllocateDescriptorSets(device_, &dsai, &tex.descriptorSet);
        vkAllocateDescriptorSets(device_, &dsai, &tex.descriptorSet2D);

        // Set 0: REPEAT sampler (3D stage path always, 2D for repeat textures).
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

        // Set 1: 2D sampler; textures whose sprites span a texture edge use
        // CLAMP_TO_EDGE so that bilinear samples past the edge latch on the
        // edge texel instead of wrapping around to the far side of the
        // texture (dark grid lines around full-coverage sprites).
        dii.sampler = tex.clampToEdge ? samplerClamp_ : sampler_;
        wds.dstSet = tex.descriptorSet2D;
        vkUpdateDescriptorSets(device_, 1, &wds, 0, nullptr);

        tex.valid = true;
    }

    // Staging upload through the persistent host-visible staging buffer
    VkDeviceSize imageSize = tex.width * tex.height * 4;
    if (!EnsureStaging(imageSize)) return;
    memcpy(stagingMapped_, tex.cpuData.data(), imageSize);

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
    vkCmdCopyBufferToImage(cmd, stagingBuffer_, tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

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
    vkQueueSubmit(graphicsQueue_, 1, &si, uploadFence_);
    if (vkWaitForFences(device_, 1, &uploadFence_, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        tex.dirty = false;
        vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
        return;
    }
    vkResetFences(device_, 1, &uploadFence_);

    vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);

    tex.dirty = false;
}

bool VulkanRenderer::EnsureStaging(VkDeviceSize bytes) {
    if (stagingBuffer_ != VK_NULL_HANDLE && stagingCapacity_ >= bytes) return true;
    ReleaseStaging();

    // Grow in 1 MiB steps; textures are small, readbacks use this too.
    VkDeviceSize want = std::max<VkDeviceSize>(bytes, 1u << 20);
    VkBufferCreateInfo bci{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bci.size = want;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bci, nullptr, &stagingBuffer_) != VK_SUCCESS) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(device_, stagingBuffer_, &req);
    VkMemoryAllocateInfo mai{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device_, &mai, nullptr, &stagingMemory_) != VK_SUCCESS) {
        vkDestroyBuffer(device_, stagingBuffer_, nullptr);
        stagingBuffer_ = VK_NULL_HANDLE;
        return false;
    }
    if (vkBindBufferMemory(device_, stagingBuffer_, stagingMemory_, 0) != VK_SUCCESS) {
        vkFreeMemory(device_, stagingMemory_, nullptr);
        vkDestroyBuffer(device_, stagingBuffer_, nullptr);
        stagingMemory_ = VK_NULL_HANDLE;
        stagingBuffer_ = VK_NULL_HANDLE;
        return false;
    }
    if (vkMapMemory(device_, stagingMemory_, 0, want, 0, &stagingMapped_) != VK_SUCCESS) {
        vkFreeMemory(device_, stagingMemory_, nullptr);
        vkDestroyBuffer(device_, stagingBuffer_, nullptr);
        stagingMemory_ = VK_NULL_HANDLE;
        stagingBuffer_ = VK_NULL_HANDLE;
        return false;
    }
    stagingCapacity_ = want;
    return true;
}

void VulkanRenderer::ReleaseStaging() {
    if (stagingBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, stagingBuffer_, nullptr);
        stagingBuffer_ = VK_NULL_HANDLE;
    }
    if (stagingMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, stagingMemory_, nullptr);
        stagingMemory_ = VK_NULL_HANDLE;
    }
    stagingMapped_ = nullptr;
    stagingCapacity_ = 0;
}

void VulkanRenderer::CaptureScreen(TextureHandle target, int left, int top, int width, int height) {
    // Runs between frames (right after Present), when the swapchain image that
    // was just presented is idle and in the PRESENT_SRC layout.
    if (inScene_ || swapchain_ == VK_NULL_HANDLE) return;
    if (target >= textures_.size() || currentImageIndex_ >= swapchainImages_.size()) return;
    if (width <= 0 || height <= 0) return;
    auto& dst = textures_[target];
    if (dst.width == 0 || dst.height == 0) return;

    // The 640x480 game image is letter-boxed into the swapchain; map the
    // source rect (given in game space) into swapchain pixels. The rendered
    // image occupies exactly that region, so what is read back is the same
    // content a D3D8 backbuffer would hold.
    int ix0 = static_cast<int>(std::lround(destX_ + static_cast<float>(left) / 640.0f * destW_));
    int iy0 = static_cast<int>(std::lround(destY_ + static_cast<float>(top) / 480.0f * destH_));
    int iw = static_cast<int>(std::lround(static_cast<float>(width) / 640.0f * destW_));
    int ih = static_cast<int>(std::lround(static_cast<float>(height) / 480.0f * destH_));
    if (ix0 < 0) { iw += ix0; ix0 = 0; }
    if (iy0 < 0) { ih += iy0; iy0 = 0; }
    if (ix0 + iw > static_cast<int>(swapchainExtent_.width)) iw = static_cast<int>(swapchainExtent_.width) - ix0;
    if (iy0 + ih > static_cast<int>(swapchainExtent_.height)) ih = static_cast<int>(swapchainExtent_.height) - iy0;
    if (iw <= 0 || ih <= 0) return;

    VkDeviceSize stagingSize = 0;
    if (!CopySwapchainRegion(ix0, iy0, iw, ih, &stagingSize)) return;

    // Swapchain pixels are BGRA (preferred format) or RGBA; the source buffer
    // for BlitPixelsToTexture is 0xAARRGGBB.
    bool rgbaOrder = (swapchainFormat_ == VK_FORMAT_R8G8B8A8_UNORM);
    std::vector<uint32_t> argb(static_cast<size_t>(iw) * ih);
    const uint8_t* src = static_cast<const uint8_t*>(stagingMapped_);
    for (size_t p = 0; p < argb.size(); p++) {
        uint8_t b0 = src[p * 4 + 0], b1 = src[p * 4 + 1];
        uint8_t b2 = src[p * 4 + 2], b3 = src[p * 4 + 3];
        uint8_t r = rgbaOrder ? b0 : b2;
        uint8_t b = rgbaOrder ? b2 : b0;
        argb[p] = (static_cast<uint32_t>(b3) << 24) | (static_cast<uint32_t>(r) << 16)
                | (static_cast<uint32_t>(b1) << 8) | b;
    }

    // D3DXLoadSurfaceFromSurface scales the source rect to FILL the destination
    // texture (the original game uses this for its pause bg), so blit the full
    // requested rect into the whole target texture.
    BlitPixelsToTexture(target, 0, 0, static_cast<int>(dst.width), static_cast<int>(dst.height),
                        argb.data(), iw, ih);
}

bool VulkanRenderer::ReadGameImage(std::vector<uint32_t>& argbOut) {
    argbOut.clear();
    if (inScene_ || swapchain_ == VK_NULL_HANDLE) return false;
    if (currentImageIndex_ >= swapchainImages_.size()) return false;

    int iw = static_cast<int>(std::lround(destW_));
    int ih = static_cast<int>(std::lround(destH_));
    int ix0 = static_cast<int>(std::lround(destX_));
    int iy0 = static_cast<int>(std::lround(destY_));
    if (iw <= 0 || ih <= 0) return false;

    VkDeviceSize stagingSize = 0;
    if (!CopySwapchainRegion(ix0, iy0, iw, ih, &stagingSize)) return false;

    bool rgbaOrder = (swapchainFormat_ == VK_FORMAT_R8G8B8A8_UNORM);
    argbOut.resize(static_cast<size_t>(iw) * ih);
    const uint8_t* src = static_cast<const uint8_t*>(stagingMapped_);
    for (size_t p = 0; p < argbOut.size(); p++) {
        uint8_t b0 = src[p * 4 + 0], b1 = src[p * 4 + 1];
        uint8_t b2 = src[p * 4 + 2], b3 = src[p * 4 + 3];
        uint8_t r = rgbaOrder ? b0 : b2;
        uint8_t b = rgbaOrder ? b2 : b0;
        argbOut[p] = (static_cast<uint32_t>(b3) << 24) | (static_cast<uint32_t>(r) << 16)
                   | (static_cast<uint32_t>(b1) << 8) | b;
    }
    return true;
}

// ── Vertical (portrait) mode ─────────────────────────────────

void VulkanRenderer::CreateVerticalComposition(int width, int height) {
    ReleaseVerticalComposition();
    if (device_ == VK_NULL_HANDLE) return;
    if (!EnsureVerticalObjects(width, height)) {
        ReleaseVerticalComposition();
        return;
    }
    verticalCompositionBuilt_ = true;
    verticalCompositionActive_ = verticalCompositionRequested_;
}

void VulkanRenderer::ReleaseVerticalComposition() {
    if (device_ != VK_NULL_HANDLE) {
        if (verticalCompositionBuilt_) vkDeviceWaitIdle(device_);
        if (composePipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, composePipeline_, nullptr);
            composePipeline_ = VK_NULL_HANDLE;
        }
        if (composePipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device_, composePipelineLayout_, nullptr);
            composePipelineLayout_ = VK_NULL_HANDLE;
        }
        if (verticalFramebuffer_ != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, verticalFramebuffer_, nullptr);
            verticalFramebuffer_ = VK_NULL_HANDLE;
        }
        if (verticalColorView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, verticalColorView_, nullptr);
            verticalColorView_ = VK_NULL_HANDLE;
        }
        if (verticalDepthView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(device_, verticalDepthView_, nullptr);
            verticalDepthView_ = VK_NULL_HANDLE;
        }
        if (verticalColorImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, verticalColorImage_, nullptr);
            vkFreeMemory(device_, verticalColorMemory_, nullptr);
            verticalColorImage_ = VK_NULL_HANDLE;
            verticalColorMemory_ = VK_NULL_HANDLE;
        }
        if (verticalDepthImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(device_, verticalDepthImage_, nullptr);
            vkFreeMemory(device_, verticalDepthMemory_, nullptr);
            verticalDepthImage_ = VK_NULL_HANDLE;
            verticalDepthMemory_ = VK_NULL_HANDLE;
        }
        if (renderPassVertical_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, renderPassVertical_, nullptr);
            renderPassVertical_ = VK_NULL_HANDLE;
        }
        if (renderPassVerticalResume_ != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, renderPassVerticalResume_, nullptr);
            renderPassVerticalResume_ = VK_NULL_HANDLE;
        }
    }
    verticalCompositionBuilt_ = false;
    verticalCompositionActive_ = false;
    verticalCompositionRequested_ = false;
    verticalCompositionInGame_ = false;
    verticalWidth_ = 0;
    verticalHeight_ = 0;
    if (verticalDescSet_ != VK_NULL_HANDLE) {
        // The pool owns the set; nothing to free individually.
        verticalDescSet_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::SetVerticalComposition(bool enabled) {
    verticalCompositionRequested_ = enabled;
    verticalCompositionActive_ = enabled && verticalCompositionBuilt_;
}

void VulkanRenderer::SetVerticalInGame(bool inGame) {
    verticalCompositionInGame_ = inGame;
}

void VulkanRenderer::SetVerticalFlip180(bool flip) {
    verticalCompositionFlip180_ = flip;
}

void VulkanRenderer::SetVerticalTransitionBlack(bool black) {
    verticalCompositionTransitionBlack_ = black;
}

bool VulkanRenderer::CopySwapchainRegion(int ix0, int iy0, int iw, int ih, VkDeviceSize* outSize) {
    if (!EnsureStaging(static_cast<VkDeviceSize>(iw) * ih * 4)) return false;

    vkQueueWaitIdle(graphicsQueue_);
    if (presentQueue_ != VK_NULL_HANDLE) vkQueueWaitIdle(presentQueue_);

    VkCommandBufferAllocateInfo cbai{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool = cmdPool_;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(device_, &cbai, &cmd) != VK_SUCCESS) return false;
    VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &bi) != VK_SUCCESS) {
        vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
        return false;
    }

    VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = swapchainImages_[currentImageIndex_];
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { ix0, iy0, 0 };
    region.imageExtent = { static_cast<uint32_t>(iw), static_cast<uint32_t>(ih), 1 };
    vkCmdCopyImageToBuffer(cmd, swapchainImages_[currentImageIndex_], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer_, 1, &region);

    // Restore the present layout so the image stays presentable.
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = 0;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(graphicsQueue_, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue_);
    vkFreeCommandBuffers(device_, cmdPool_, 1, &cmd);
    if (outSize) *outSize = static_cast<VkDeviceSize>(iw) * ih * 4;
    return true;
}

bool VulkanRenderer::LoadSurfaceImage(int index, const void* data, size_t size, uint32_t* outWidth,
                                      uint32_t* outHeight) {
    if (index < 0) return false;
    if (static_cast<size_t>(index) >= surfaceImages_.size()) surfaceImages_.resize(index + 1);
    ReleaseSurfaceImage(index);

    TextureHandle handle = CreateTextureFromMemory(data, size, 0);
    if (handle == INVALID_TEXTURE) return false;

    SurfaceDesc desc;
    if (!LockTexture(handle, &desc)) {
        ReleaseTexture(handle);
        return false;
    }
    surfaceImages_[index].tex = handle;
    surfaceImages_[index].width = desc.width;
    surfaceImages_[index].height = desc.height;
    if (outWidth) *outWidth = desc.width;
    if (outHeight) *outHeight = desc.height;
    return true;
}

void VulkanRenderer::DrawSurfaceRegion(int index, int dstX, int dstY, int srcX, int srcY, int srcW,
                                       int srcH) {
    if (index < 0 || static_cast<size_t>(index) >= surfaceImages_.size()) return;
    const SurfaceImage& s = surfaceImages_[index];
    if (s.tex == INVALID_TEXTURE || s.width == 0 || s.height == 0) return;
    if (srcW <= 0 || srcH <= 0) return;

    float u0 = static_cast<float>(srcX) / static_cast<float>(s.width);
    float v0 = static_cast<float>(srcY) / static_cast<float>(s.height);
    float u1 = static_cast<float>(srcX + srcW) / static_cast<float>(s.width);
    float v1 = static_cast<float>(srcY + srcH) / static_cast<float>(s.height);
    float x0 = static_cast<float>(dstX);
    float y0 = static_cast<float>(dstY);
    float x1 = static_cast<float>(dstX + srcW);
    float y1 = static_cast<float>(dstY + srcH);

    Vertex2D v[4] = {
        { x0, y0, u0, v0, 0xFFFFFFFFu },
        { x1, y0, u1, v0, 0xFFFFFFFFu },
        { x0, y1, u0, v1, 0xFFFFFFFFu },
        { x1, y1, u1, v1, 0xFFFFFFFFu },
    };
    DrawQuad2D(s.tex, v, BLEND_NONE);
}

void VulkanRenderer::ReleaseSurfaceImage(int index) {
    if (index < 0 || static_cast<size_t>(index) >= surfaceImages_.size()) return;
    if (surfaceImages_[index].tex != INVALID_TEXTURE) {
        ReleaseTexture(surfaceImages_[index].tex);
        surfaceImages_[index].tex = INVALID_TEXTURE;
    }
    surfaceImages_[index].width = 0;
    surfaceImages_[index].height = 0;
}

void VulkanRenderer::BlitPixelsToTexture(TextureHandle tex, int dstX, int dstY, int dstW, int dstH,
                                         const uint32_t* argbSrc, int srcW, int srcH) {
    if (tex >= textures_.size()) return;
    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0) return;
    auto& t = textures_[tex];
    if (t.width == 0 || t.height == 0) return;

    for (int y = 0; y < dstH; y++) {
        int ty = dstY + y;
        if (ty < 0 || ty >= static_cast<int>(t.height)) continue;
        for (int x = 0; x < dstW; x++) {
            int tx = dstX + x;
            if (tx < 0 || tx >= static_cast<int>(t.width)) continue;

            int sx0 = x * srcW / dstW;
            int sx1 = (x + 1) * srcW / dstW;
            int sy0 = y * srcH / dstH;
            int sy1 = (y + 1) * srcH / dstH;
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sy1 <= sy0) sy1 = sy0 + 1;

            uint32_t a = 0, r = 0, g = 0, b = 0, n = 0;
            for (int sy = sy0; sy < sy1 && sy < srcH; sy++) {
                for (int sx = sx0; sx < sx1 && sx < srcW; sx++) {
                    uint32_t p = argbSrc[static_cast<size_t>(sy) * srcW + sx];
                    a += (p >> 24) & 0xFF;
                    r += (p >> 16) & 0xFF;
                    g += (p >> 8) & 0xFF;
                    b += p & 0xFF;
                    n++;
                }
            }
            if (n == 0) continue;
            uint8_t* px = &t.cpuData[(static_cast<size_t>(ty) * t.width + tx) * 4];
            px[0] = static_cast<uint8_t>(b / n);
            px[1] = static_cast<uint8_t>(g / n);
            px[2] = static_cast<uint8_t>(r / n);
            px[3] = static_cast<uint8_t>(a / n);
        }
    }
    t.dirty = true;
    UploadTextureGPU(tex);
}

} // namespace th06