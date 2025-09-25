/**
 * @file VulkanRenderer.h
 * @brief Vulkan renderer with swap chain management and command buffer orchestration
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "vulkan/VulkanDevice.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <memory>
#include <vector>
#include <cassert>
#include <limits>

/**
 * @brief High-level Vulkan renderer managing swap chain and frame rendering
 *
 * The VulkanRenderer class encapsulates the rendering pipeline infrastructure:
 * - Swap chain creation and management (handles window resizing)
 * - Render pass configuration for color and depth rendering
 * - Frame synchronization with semaphores and fences
 * - Command buffer allocation and management
 * - Double/triple buffering for smooth rendering
 * - Depth buffer creation and management
 *
 * The renderer follows a typical Vulkan rendering loop:
 * 1. Begin frame (acquire swap chain image, start command buffer)
 * 2. Begin render pass (clear attachments, set viewport)
 * 3. Record rendering commands (done by client code)
 * 4. End render pass
 * 5. End frame (submit commands, present image)
 *
 * @note This class handles the low-level rendering infrastructure.
 *       Actual rendering commands are recorded by client code between
 *       beginSwapchainRenderPass() and endSwapchainRenderPass()
 *
 * @see VulkanDevice for device management
 * @see VulkanBuffer for vertex/index buffer creation
 */
class VulkanRenderer {
public:
    /**
     * @brief Initialize Vulkan renderer with window and device
     * @param window GLFW window for rendering target
     * @param device Initialized Vulkan device
     *
     * Creates swap chain, render pass, framebuffers, command buffers,
     * and synchronization objects needed for rendering.
     */
    VulkanRenderer(GLFWwindow* window, VulkanDevice& device);

    /**
     * @brief Destructor - cleans up all rendering resources
     * @note Waits for device idle before cleanup to ensure safe destruction
     */
    ~VulkanRenderer();

    // Non-copyable to prevent resource duplication
    VulkanRenderer(const VulkanRenderer&) = delete;  ///< Copy constructor deleted
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;  ///< Copy assignment deleted

    /// @name Accessors
    /// @{
    /**
     * @brief Get swap chain render pass handle
     * @return Render pass compatible with swap chain framebuffers
     *
     * The render pass defines the rendering operations and attachments.
     * Use this when creating graphics pipelines that render to the swap chain.
     */
    VkRenderPass getSwapChainRenderPass() const { return m_swapChainRenderPass; }

    /**
     * @brief Get current swap chain aspect ratio
     * @return Width/height ratio of swap chain images
     *
     * Useful for setting up projection matrices and viewport calculations.
     * Updates automatically when window is resized.
     */
    float getAspectRatio() const {
        return static_cast<float>(m_swapChainExtent.width) / static_cast<float>(m_swapChainExtent.height);
    }

    /**
     * @brief Check if frame rendering is currently in progress
     * @return True if between beginFrame() and endFrame() calls
     *
     * Used for validation to ensure proper rendering call ordering.
     */
    bool isFrameInProgress() const { return m_isFrameStarted; }
    /// @}

    /// @name Frame Context
    /// @{
    /**
     * @brief Get current frame's command buffer
     * @return Active command buffer for current frame
     *
     * Returns the command buffer that should be used for recording
     * rendering commands in the current frame. Only valid between
     * beginFrame() and endFrame() calls.
     *
     * @warning Asserts if called when frame is not in progress
     */
    VkCommandBuffer getCurrentCommandBuffer() const {
        assert(m_isFrameStarted && "Cannot get command buffer when frame not in progress");
        return m_commandBuffers[m_currentFrameIndex];
    }

    /**
     * @brief Get current frame index
     * @return Index of current frame (0 to MAX_FRAMES_IN_FLIGHT-1)
     *
     * Useful for accessing per-frame resources like uniform buffers.
     * The frame index cycles through available frames in flight.
     *
     * @warning Asserts if called when frame is not in progress
     */
    int getFrameIndex() const {
        assert(m_isFrameStarted && "Cannot get frame index when frame not in progress");
        return m_currentFrameIndex;
    }
    /// @}

    /// @name Frame Management
    /// @{
    /**
     * @brief Begin a new frame for rendering
     * @return Command buffer for recording rendering commands
     * @retval VK_NULL_HANDLE if swap chain needs recreation (window resized)
     *
     * Acquires the next swap chain image and begins command buffer recording.
     * Must be paired with endFrame(). If VK_NULL_HANDLE is returned,
     * the frame should be skipped as the swap chain is being recreated.
     *
     * @note Only one frame can be in progress at a time
     */
    VkCommandBuffer beginFrame();

    /**
     * @brief End current frame and submit for presentation
     *
     * Ends command buffer recording, submits to graphics queue,
     * and queues the image for presentation. Handles swap chain
     * recreation if needed (e.g., window resize).
     *
     * @warning Must be called after beginFrame() and only when frame is in progress
     */
    void endFrame();
    /// @}

    /// @name Render Pass Management
    /// @{
    /**
     * @brief Begin swap chain render pass
     * @param commandBuffer Command buffer to record into
     *
     * Begins the render pass for the current swap chain framebuffer.
     * Clears color and depth attachments and sets up viewport/scissor.
     * All rendering commands should be recorded after this call.
     *
     * @note Must be called between beginFrame() and endFrame()
     */
    void beginSwapchainRenderPass(VkCommandBuffer commandBuffer);

    /**
     * @brief End swap chain render pass
     * @param commandBuffer Command buffer that was used for rendering
     *
     * Ends the render pass begun by beginSwapchainRenderPass().
     * Must be called before endFrame().
     */
    void endSwapchainRenderPass(VkCommandBuffer commandBuffer);
    /// @}

    /// @name Swap Chain Management
    /// @{
    /**
     * @brief Recreate swap chain for window changes
     *
     * Destroys and recreates the swap chain, render pass, and framebuffers.
     * Called automatically when window is resized or when swap chain
     * becomes invalid. Waits for device idle before recreation.
     *
     * @note This is an expensive operation - only call when necessary
     */
    void recreateSwapChain();
    /// @}

private:
    /// @name Initialization Functions
    /// @{
    void createSwapChain();       ///< Create swap chain for window surface
    void createImageViews();      ///< Create image views for swap chain images
    void createRenderPass();      ///< Create render pass for color and depth rendering
    void createDepthResources();  ///< Create depth buffer images and views
    void createFramebuffers();    ///< Create framebuffers for each swap chain image
    void createSyncObjects();     ///< Create semaphores and fences for synchronization
    void createCommandBuffers();  ///< Allocate command buffers for each frame
    /// @}

    /// @name Helper Functions
    /// @{
    /**
     * @brief Choose optimal surface format for swap chain
     * @param availableFormats List of supported surface formats
     * @return Best surface format (prefers SRGB color space)
     */
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    /**
     * @brief Choose optimal presentation mode for swap chain
     * @param availablePresentModes List of supported present modes
     * @return Best present mode (prefers mailbox for low latency)
     */
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    /**
     * @brief Choose optimal swap extent based on window size
     * @param capabilities Surface capabilities
     * @return Swap extent matching window size within surface limits
     */
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    /**
     * @brief Find optimal depth buffer format
     * @return Supported depth format (prefers D32_SFLOAT)
     */
    VkFormat findDepthFormat();
    /// @}

    /**
     * @brief Clean up swap chain resources before recreation
     * @note Called automatically during swap chain recreation
     */
    void cleanupSwapChain();

    /// @name Core References
    /// @{
    GLFWwindow* m_window;     ///< GLFW window for rendering target
    VulkanDevice& m_device;   ///< Reference to Vulkan device
    /// @}

    /// @name Swap Chain Objects
    /// @{
    VkSwapchainKHR m_swapChain;  ///< Vulkan swap chain handle

    std::vector<VkFramebuffer> m_swapChainFramebuffers;  ///< Framebuffers for each swap chain image
    VkRenderPass m_swapChainRenderPass;                  ///< Render pass for swap chain rendering
    /// @}

    /// @name Image Resources
    /// @{
    std::vector<VkImage> m_depthImages;           ///< Depth buffer images (one per swap chain image)
    std::vector<VkDeviceMemory> m_depthImageMemorys;  ///< Memory for depth images
    std::vector<VkImageView> m_depthImageViews;   ///< Views for depth images
    std::vector<VkImage> m_swapChainImages;       ///< Swap chain images (managed by swap chain)
    std::vector<VkImageView> m_swapChainImageViews;  ///< Views for swap chain images
    /// @}

    /// @name Format and Extent
    /// @{
    VkFormat m_swapChainImageFormat;   ///< Pixel format of swap chain images
    VkFormat m_swapChainDepthFormat;   ///< Pixel format of depth buffer
    VkExtent2D m_swapChainExtent;      ///< Resolution of swap chain images
    /// @}

    /// @name Command Buffers
    /// @{
    std::vector<VkCommandBuffer> m_commandBuffers;  ///< Command buffers (one per frame in flight)
    /// @}

    /// @name Synchronization
    /// @{
    std::vector<VkSemaphore> m_imageAvailableSemaphores;  ///< Semaphores signaled when swap chain image available
    std::vector<VkSemaphore> m_renderFinishedSemaphores;  ///< Semaphores signaled when rendering complete
    std::vector<VkFence> m_inFlightFences;                ///< Fences for CPU-GPU synchronization
    uint32_t m_currentImageIndex;                         ///< Index of current swap chain image
    int m_currentFrameIndex{0};                           ///< Index of current frame in flight
    bool m_isFrameStarted{false};                         ///< True if frame is currently being recorded
    /// @}

    /// @name Constants
    /// @{
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;  ///< Number of frames that can be processed simultaneously
    /// @}
};