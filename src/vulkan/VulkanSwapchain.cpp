#include "vulkan/VulkanSwapchain.hpp"
#include "core/Logger.hpp"

#include <stdexcept>
#include <algorithm>
#include <limits>

namespace engine {

VulkanSwapchain::VulkanSwapchain(VkDevice device, VkPhysicalDevice physicalDevice,
                                 VkSurfaceKHR surface, SDL_Window* window)
    : device(device), physicalDevice(physicalDevice), surface(surface), window(window),
      swapChainImageFormat(VK_FORMAT_UNDEFINED), swapChainExtent{0, 0} {
}

void VulkanSwapchain::create() {
    LOG_DEBUG("Creating swapchain");

    SwapChainSupportDetails swapChainSupport = querySwapChainSupport();

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        LOG_ERROR("Failed to create swapchain");
        throw std::runtime_error("Failed to create swapchain");
    }

    if (vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr) != VK_SUCCESS) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        LOG_ERROR("Failed to get swapchain image count");
        throw std::runtime_error("Failed to get swapchain image count");
    }
    swapChainImages.resize(imageCount);
    if (vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data()) != VK_SUCCESS) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
        LOG_ERROR("Failed to get swapchain images");
        throw std::runtime_error("Failed to get swapchain images");
    }

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;

    LOG_INFO("Swapchain created ({}x{}, {} images)", extent.width, extent.height, imageCount);
}

void VulkanSwapchain::createImageViews() {
    LOG_DEBUG("Creating swapchain image views");

    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create image view {}", i);
            throw std::runtime_error("Failed to create image views");
        }
    }

    LOG_DEBUG("Created {} image views", swapChainImageViews.size());
}

void VulkanSwapchain::createFramebuffers(VkRenderPass renderPass, VkImageView depthImageView) {
    LOG_DEBUG("Creating framebuffers");

    swapChainFramebuffers.resize(swapChainImageViews.size());

    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            swapChainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = swapChainExtent.width;
        framebufferInfo.height = swapChainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
            LOG_ERROR("Failed to create framebuffer {}", i);
            throw std::runtime_error("Failed to create framebuffer");
        }
    }

    LOG_DEBUG("Created {} framebuffers", swapChainFramebuffers.size());
}

void VulkanSwapchain::recreate() {
    LOG_INFO("Recreating swapchain");

    // Wait for device to be idle
    if (vkDeviceWaitIdle(device) != VK_SUCCESS) {
        LOG_WARN("Failed to wait for device idle during swapchain recreation");
    }

    // Clean up old swapchain resources
    cleanupFramebuffers();
    for (auto* imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    swapChainImageViews.clear();

    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    // Recreate swapchain
    create();
    createImageViews();

    LOG_INFO("Swapchain recreated successfully");
}

void VulkanSwapchain::cleanupFramebuffers() {
    LOG_DEBUG("Cleaning up framebuffers");

    for (auto* framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    swapChainFramebuffers.clear();
}

void VulkanSwapchain::cleanup() {
    LOG_DEBUG("Cleaning up swapchain");

    cleanupFramebuffers();

    for (auto* imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }

    if (swapChain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
}

VulkanSwapchain::SwapChainSupportDetails VulkanSwapchain::querySwapChainSupport() {
    SwapChainSupportDetails details;

    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities) != VK_SUCCESS) {
        LOG_ERROR("Failed to get surface capabilities");
        throw std::runtime_error("Failed to query swapchain support: surface capabilities");
    }

    uint32_t formatCount = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr) != VK_SUCCESS) {
        LOG_ERROR("Failed to get surface format count");
        throw std::runtime_error("Failed to query swapchain support: format count");
    }
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data()) != VK_SUCCESS) {
            LOG_ERROR("Failed to get surface formats");
            throw std::runtime_error("Failed to query swapchain support: surface formats");
        }
    }

    uint32_t presentModeCount = 0;
    if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr) != VK_SUCCESS) {
        LOG_ERROR("Failed to get present mode count");
        throw std::runtime_error("Failed to query swapchain support: present mode count");
    }
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        if (vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data()) != VK_SUCCESS) {
            LOG_ERROR("Failed to get present modes");
            throw std::runtime_error("Failed to query swapchain support: present modes");
        }
    }

    LOG_DEBUG("Swapchain support queried: {} formats, {} present modes", formatCount, presentModeCount);
    return details;
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {  // NOLINT(readability-convert-member-functions-to-static)
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {  // NOLINT(readability-convert-member-functions-to-static)
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actualExtent.width = std::clamp(actualExtent.width,
                                   capabilities.minImageExtent.width,
                                   capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height,
                                    capabilities.minImageExtent.height,
                                    capabilities.maxImageExtent.height);

    return actualExtent;
}

} // namespace engine
