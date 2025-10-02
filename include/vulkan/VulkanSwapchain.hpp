#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <vector>

namespace engine {

/**
 * @brief Manages Vulkan swapchain and presentation
 *
 * Handles swapchain creation, image views, framebuffers, and recreation
 * when the window is resized or minimized. Chooses optimal surface formats
 * and present modes for smooth rendering.
 */
class VulkanSwapchain {
public:
    /**
     * @brief Construct a new Vulkan Swapchain manager
     * @param device Logical Vulkan device
     * @param physicalDevice Physical device for surface capabilities
     * @param surface Window surface for presentation
     * @param window SDL window for size queries
     */
    VulkanSwapchain(VkDevice device, VkPhysicalDevice physicalDevice,
                    VkSurfaceKHR surface, SDL_Window* window);
    ~VulkanSwapchain() = default;

    // Delete copy operations (Vulkan handles shouldn't be copied)
    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    // Allow move operations
    VulkanSwapchain(VulkanSwapchain&&) noexcept = default;
    VulkanSwapchain& operator=(VulkanSwapchain&&) noexcept = default;

    /**
     * @brief Create the swapchain with optimal settings
     */
    void create();

    /**
     * @brief Create image views for all swapchain images
     */
    void createImageViews();

    /**
     * @brief Create framebuffers for all swapchain images
     * @param renderPass Render pass to attach framebuffers to
     * @param depthImageView Depth buffer image view
     */
    void createFramebuffers(VkRenderPass renderPass, VkImageView depthImageView);

    /**
     * @brief Recreate swapchain after window resize
     */
    void recreate();

    /**
     * @brief Clean up only framebuffers (used before recreation)
     */
    void cleanupFramebuffers();

    /**
     * @brief Clean up all swapchain resources
     */
    void cleanup();

    /**
     * @brief Get the swapchain handle
     * @return VkSwapchainKHR Swapchain
     */
    VkSwapchainKHR getSwapchain() const { return swapChain; }

    /**
     * @brief Get the swapchain image format
     * @return VkFormat Image format (e.g., VK_FORMAT_B8G8R8A8_SRGB)
     */
    VkFormat getImageFormat() const { return swapChainImageFormat; }

    /**
     * @brief Get the swapchain extent (width and height)
     * @return VkExtent2D Swapchain dimensions
     */
    VkExtent2D getExtent() const { return swapChainExtent; }

    /**
     * @brief Get all swapchain images
     * @return const std::vector<VkImage>& Vector of swapchain images
     */
    const std::vector<VkImage>& getImages() const { return swapChainImages; }

    /**
     * @brief Get all swapchain image views
     * @return const std::vector<VkImageView>& Vector of image views
     */
    const std::vector<VkImageView>& getImageViews() const { return swapChainImageViews; }

    /**
     * @brief Get all swapchain framebuffers
     * @return const std::vector<VkFramebuffer>& Vector of framebuffers
     */
    const std::vector<VkFramebuffer>& getFramebuffers() const { return swapChainFramebuffers; }

private:
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkSurfaceKHR surface;
    SDL_Window* window;

    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    SwapChainSupportDetails querySwapChainSupport();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
};

} // namespace engine
