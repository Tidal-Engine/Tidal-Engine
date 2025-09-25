/**
 * @file VulkanDevice.h
 * @brief Vulkan device abstraction with queue management and resource creation
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vector>
#include <optional>
#include <set>

/**
 * @brief Queue family indices for graphics and presentation
 *
 * Stores the indices of queue families that support graphics operations
 * and surface presentation. In Vulkan, different operations may require
 * different queue families, though they often overlap on modern GPUs.
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;  ///< Queue family index for graphics operations
    std::optional<uint32_t> presentFamily;   ///< Queue family index for presentation to surface

    /**
     * @brief Check if both required queue families are available
     * @return True if both graphics and present queue families are found
     */
    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/**
 * @brief Swap chain support information for surface presentation
 *
 * Contains all the information needed to configure a swap chain for
 * presenting rendered images to the window surface. This includes
 * capabilities, supported formats, and presentation modes.
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;      ///< Surface capabilities (min/max images, extents, etc.)
    std::vector<VkSurfaceFormatKHR> formats;    ///< Supported surface formats (color space, format)
    std::vector<VkPresentModeKHR> presentModes; ///< Available presentation modes (vsync, immediate, etc.)
};

/**
 * @brief Vulkan device abstraction and resource factory
 *
 * The VulkanDevice class encapsulates Vulkan instance, device, and queue management.
 * It provides a high-level interface for:
 * - Vulkan instance and device initialization
 * - Physical device selection and capability querying
 * - Queue family discovery and queue creation
 * - Buffer and image creation with memory management
 * - Command buffer allocation and execution
 * - Debug layer setup and validation
 *
 * This class serves as the foundation for all Vulkan operations in the engine,
 * providing centralized device management and resource creation utilities.
 *
 * @note This class follows RAII principles for automatic cleanup
 * @see VulkanBuffer for buffer management
 * @see VulkanRenderer for rendering operations
 */
class VulkanDevice {
public:
    /**
     * @brief Initialize Vulkan device with window surface
     * @param window GLFW window for surface creation
     *
     * Creates Vulkan instance, selects suitable physical device,
     * creates logical device, and sets up command pools and queues.
     * Also initializes debug layers in debug builds.
     */
    VulkanDevice(GLFWwindow* window);

    /**
     * @brief Destructor - cleans up all Vulkan resources
     * @note Automatically destroys device, instance, and associated objects
     */
    ~VulkanDevice();

    // Non-copyable to prevent resource duplication
    VulkanDevice(const VulkanDevice&) = delete;  ///< Copy constructor deleted
    VulkanDevice& operator=(const VulkanDevice&) = delete;  ///< Copy assignment deleted

    /// @name Vulkan Object Accessors
    /// @{
    VkCommandPool getCommandPool() const { return m_commandPool; }  ///< Get command pool for command buffer allocation
    VkDevice getDevice() const { return m_device; }  ///< Get logical device handle
    VkSurfaceKHR getSurface() const { return m_surface; }  ///< Get window surface handle
    VkQueue getGraphicsQueue() const { return m_graphicsQueue; }  ///< Get graphics queue for rendering commands
    VkQueue getPresentQueue() const { return m_presentQueue; }  ///< Get present queue for surface presentation
    VkInstance getInstance() const { return m_instance; }  ///< Get Vulkan instance handle
    VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }  ///< Get selected physical device
    /// @}

    /// @name Device Capability Queries
    /// @{
    /**
     * @brief Query swap chain support details for current physical device
     * @return Structure containing surface capabilities, formats, and present modes
     */
    SwapChainSupportDetails getSwapChainSupport() const { return querySwapChainSupport(m_physicalDevice); }

    /**
     * @brief Find suitable memory type for allocation requirements
     * @param typeFilter Bitmask of acceptable memory types
     * @param properties Required memory property flags
     * @return Index of suitable memory type
     * @throws std::runtime_error if no suitable memory type found
     *
     * Searches device memory types to find one that matches both the type filter
     * and required properties (e.g., host visible, device local, etc.)
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;

    /**
     * @brief Find queue family indices for current physical device
     * @return QueueFamilyIndices with graphics and present family indices
     */
    QueueFamilyIndices findPhysicalQueueFamilies() const { return findQueueFamilies(m_physicalDevice); }

    /**
     * @brief Find supported format from candidates list
     * @param candidates List of preferred formats in priority order
     * @param tiling Required image tiling mode
     * @param features Required format feature flags
     * @return First supported format from candidates
     * @throws std::runtime_error if no format from candidates is supported
     *
     * Used to find optimal formats for depth buffers, color attachments, etc.
     */
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;
    /// @}

    /// @name Buffer Helper Functions
    /// @{
    /**
     * @brief Create Vulkan buffer with associated memory
     * @param size Buffer size in bytes
     * @param usage Buffer usage flags (vertex, index, uniform, etc.)
     * @param properties Memory property flags (device local, host visible, etc.)
     * @param buffer Output parameter for created buffer handle
     * @param bufferMemory Output parameter for allocated memory handle
     *
     * Creates a Vulkan buffer and allocates suitable device memory for it.
     * This is a lower-level function - consider using VulkanBuffer class instead.
     */
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory) const;

    /**
     * @brief Begin single-time command buffer for immediate operations
     * @return Command buffer ready for recording
     *
     * Allocates and begins a command buffer for one-time operations like
     * buffer copying or image layout transitions.
     */
    VkCommandBuffer beginSingleTimeCommands() const;

    /**
     * @brief End and submit single-time command buffer
     * @param commandBuffer Command buffer to end and submit
     *
     * Ends recording, submits to graphics queue, and waits for completion.
     * Automatically frees the command buffer when done.
     */
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

    /**
     * @brief Copy data between buffers using GPU
     * @param srcBuffer Source buffer handle
     * @param dstBuffer Destination buffer handle
     * @param size Number of bytes to copy
     *
     * Performs GPU-side buffer copy using a single-time command buffer.
     * More efficient than CPU copy for large amounts of data.
     */
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;

    /**
     * @brief Copy buffer data to image
     * @param buffer Source buffer containing image data
     * @param image Destination image handle
     * @param width Image width in pixels
     * @param height Image height in pixels
     *
     * Copies linear buffer data to 2D image, performing any necessary
     * format conversions and layout transitions.
     */
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) const;
    /// @}

    /// @name Image Helper Functions
    /// @{
    /**
     * @brief Create Vulkan image with associated memory
     * @param imageInfo Complete image creation parameters
     * @param properties Memory property flags for image memory
     * @param image Output parameter for created image handle
     * @param imageMemory Output parameter for allocated image memory
     *
     * Creates a Vulkan image and allocates suitable device memory for it.
     * Handles memory requirement querying and binding automatically.
     */
    void createImageWithInfo(
        const VkImageCreateInfo& imageInfo,
        VkMemoryPropertyFlags properties,
        VkImage& image,
        VkDeviceMemory& imageMemory) const;
    /// @}

    VkPhysicalDeviceProperties m_properties;  ///< Physical device properties and limits

private:
    /// @name Initialization Functions
    /// @{
    void createInstance();        ///< Create Vulkan instance with extensions and layers
    void setupDebugMessenger();   ///< Set up debug callback for validation layers
    void createSurface();         ///< Create window surface for presentation
    void pickPhysicalDevice();    ///< Select suitable physical device (GPU)
    void createLogicalDevice();   ///< Create logical device with required queues
    void createCommandPool();     ///< Create command pool for command buffer allocation
    /// @}

    /// @name Helper Functions
    /// @{
    /**
     * @brief Check if physical device meets requirements
     * @param device Physical device to evaluate
     * @return True if device supports required features and extensions
     */
    bool isDeviceSuitable(VkPhysicalDevice device) const;

    /**
     * @brief Get list of required Vulkan instance extensions
     * @return Vector of required extension names (including GLFW and debug)
     */
    std::vector<const char*> getRequiredExtensions() const;

    /**
     * @brief Check if validation layers are available
     * @return True if all required validation layers are supported
     */
    bool checkValidationLayerSupport() const;

    /**
     * @brief Find queue family indices for a physical device
     * @param device Physical device to query
     * @return QueueFamilyIndices with graphics and present family indices
     */
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

    /**
     * @brief Configure debug messenger creation info
     * @param createInfo Output parameter for debug messenger configuration
     */
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) const;

    /**
     * @brief Verify GLFW required extensions are available
     * @throws std::runtime_error if required extensions are missing
     */
    void hasGlfwRequiredInstanceExtensions() const;

    /**
     * @brief Check if physical device supports required extensions
     * @param device Physical device to check
     * @return True if all required device extensions are supported
     */
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;

    /**
     * @brief Query swap chain support details for physical device
     * @param device Physical device to query
     * @return SwapChainSupportDetails with capabilities and supported modes
     */
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;
    /// @}

    /// @name Vulkan Objects
    /// @{
    VkInstance m_instance;                              ///< Vulkan instance handle
    VkDebugUtilsMessengerEXT m_debugMessenger;          ///< Debug messenger for validation layers
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE; ///< Selected physical device (GPU)
    GLFWwindow* m_window;                               ///< GLFW window for surface creation
    VkCommandPool m_commandPool;                        ///< Command pool for command buffer allocation

    VkDevice m_device;                                  ///< Logical device handle
    VkSurfaceKHR m_surface;                            ///< Window surface for presentation
    VkQueue m_graphicsQueue;                           ///< Graphics queue for rendering commands
    VkQueue m_presentQueue;                            ///< Present queue for surface presentation
    /// @}

    /// @name Configuration Constants
    /// @{
    const std::vector<const char*> m_validationLayers = {"VK_LAYER_KHRONOS_validation"};  ///< Validation layers for debug builds
    const std::vector<const char*> m_deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; ///< Required device extensions
    /// @}
};