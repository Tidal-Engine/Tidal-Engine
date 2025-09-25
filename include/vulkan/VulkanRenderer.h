#pragma once

#include "vulkan/VulkanDevice.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <memory>
#include <vector>
#include <cassert>
#include <limits>

class VulkanRenderer {
public:
    VulkanRenderer(GLFWwindow* window, VulkanDevice& device);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    VkRenderPass getSwapChainRenderPass() const { return m_swapChainRenderPass; }
    float getAspectRatio() const {
        return static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
    }
    bool isFrameInProgress() const { return m_isFrameStarted; }

    VkCommandBuffer getCurrentCommandBuffer() const {
        assert(m_isFrameStarted && "Cannot get command buffer when frame not in progress");
        return m_commandBuffers[m_currentFrameIndex];
    }

    int getFrameIndex() const {
        assert(m_isFrameStarted && "Cannot get frame index when frame not in progress");
        return m_currentFrameIndex;
    }

    VkCommandBuffer beginFrame();
    void endFrame();
    void beginSwapchainRenderPass(VkCommandBuffer commandBuffer);
    void endSwapchainRenderPass(VkCommandBuffer commandBuffer);
    void recreateSwapChain();

private:
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDepthResources();
    void createFramebuffers();
    void createSyncObjects();
    void createCommandBuffers();

    // helper functions
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    VkFormat findDepthFormat();

    void cleanupSwapChain();

    GLFWwindow* m_window;
    VulkanDevice& m_device;

    VkSwapchainKHR m_swapChain;

    std::vector<VkFramebuffer> m_swapChainFramebuffers;
    VkRenderPass m_swapChainRenderPass;

    std::vector<VkImage> m_depthImages;
    std::vector<VkDeviceMemory> m_depthImageMemorys;
    std::vector<VkImageView> m_depthImageViews;
    std::vector<VkImage> m_swapChainImages;
    std::vector<VkImageView> m_swapChainImageViews;

    VkFormat m_swapChainImageFormat;
    VkFormat m_swapChainDepthFormat;
    VkExtent2D m_swapChainExtent;

    std::vector<VkCommandBuffer> m_commandBuffers;

    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentImageIndex;
    int m_currentFrameIndex{0};
    bool m_isFrameStarted{false};

    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
};