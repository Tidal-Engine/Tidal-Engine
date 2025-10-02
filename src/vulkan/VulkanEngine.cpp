#include "vulkan/VulkanEngine.hpp"
#include "vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanSwapchain.hpp"
#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/CubeGeometry.hpp"
#include "core/Logger.hpp"

#include <stdexcept>

namespace engine {

VulkanEngine::VulkanEngine() = default;
VulkanEngine::~VulkanEngine() = default;

void VulkanEngine::run() {
    initSDL();
    initVulkan();
    initGeometry();
    initRenderingResources();
    mainLoop();
    cleanup();
}

void VulkanEngine::initSDL() {
    LOG_INFO("Initializing SDL3...");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("Failed to initialize SDL: {}", SDL_GetError());
        throw std::runtime_error("Failed to initialize SDL");
    }

    LOG_INFO("Creating window ({}x{})...", WIDTH, HEIGHT);
    window = SDL_CreateWindow(
        "Vulkan Engine - Rainbow Cube",
        static_cast<int>(WIDTH), static_cast<int>(HEIGHT),
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        LOG_ERROR("Failed to create window: {}", SDL_GetError());
        throw std::runtime_error("Failed to create window");
    }

    LOG_INFO("SDL3 initialized successfully");
}

void VulkanEngine::initVulkan() {
    LOG_INFO("Initializing Vulkan...");
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    LOG_INFO("Vulkan initialized successfully");
}

void VulkanEngine::initGeometry() {
    LOG_INFO("Loading cube geometry...");
    vertices = CubeGeometry::getVertices();
    indices = CubeGeometry::getIndices();
    LOG_INFO("Loaded {} vertices and {} indices", vertices.size(), indices.size());
}

void VulkanEngine::initRenderingResources() {
    LOG_INFO("Initializing rendering resources...");

    // Get queue family indices
    QueueFamilyIndices queueIndices = findQueueFamilies(physicalDevice);

    // Create subsystems
    bufferManager = std::make_unique<VulkanBuffer>(device, physicalDevice);
    swapchain = std::make_unique<VulkanSwapchain>(device, physicalDevice, surface, window);
    renderer = std::make_unique<VulkanRenderer>(device, physicalDevice,
                                                queueIndices.graphicsFamily,
                                                graphicsQueue, presentQueue);

    // Create swapchain
    swapchain->create();
    swapchain->createImageViews();

    // Create pipeline (needs swapchain extent and format)
    pipeline = std::make_unique<VulkanPipeline>(device, swapchain->getExtent(),
                                                swapchain->getImageFormat());
    pipeline->createRenderPass();
    pipeline->createDescriptorSetLayout();
    pipeline->createGraphicsPipeline("shaders/cube_vert.spv", "shaders/cube_frag.spv");

    // Create rendering resources
    renderer->createCommandPool();
    renderer->createDepthResources(swapchain->getExtent());

    // Create framebuffers
    swapchain->createFramebuffers(pipeline->getRenderPass(), renderer->getDepthImageView());

    // Create buffers
    bufferManager->createVertexBuffer(vertices.data(), sizeof(vertices[0]) * vertices.size(),
                                     renderer->getCommandPool(), graphicsQueue);
    bufferManager->createIndexBuffer(indices.data(), sizeof(indices[0]) * indices.size(),
                                    renderer->getCommandPool(), graphicsQueue);
    bufferManager->createUniformBuffers(MAX_FRAMES_IN_FLIGHT, sizeof(UniformBufferObject));

    // Create descriptor sets
    pipeline->createDescriptorPool(MAX_FRAMES_IN_FLIGHT);
    pipeline->createDescriptorSets(bufferManager->getUniformBuffers(), sizeof(UniformBufferObject));

    // Create command buffers and sync objects
    renderer->createCommandBuffers(MAX_FRAMES_IN_FLIGHT);
    renderer->createSyncObjects(MAX_FRAMES_IN_FLIGHT);

    LOG_INFO("Rendering resources initialized successfully");
}

void VulkanEngine::createInstance() {
    LOG_DEBUG("Creating Vulkan instance...");
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Get SDL required extensions
    uint32_t sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan instance");
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    LOG_INFO("Vulkan instance created (API version 1.3)");
}

void VulkanEngine::createSurface() {
    LOG_DEBUG("Creating Vulkan surface...");

    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        LOG_ERROR("Failed to create window surface");
        throw std::runtime_error("Failed to create window surface");
    }

    LOG_DEBUG("Vulkan surface created");
}

void VulkanEngine::pickPhysicalDevice() {
    LOG_DEBUG("Selecting physical device...");
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        LOG_ERROR("No GPUs with Vulkan support found");
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    LOG_DEBUG("Found {} Vulkan-capable device(s)", deviceCount);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // For now, just pick the first device
    physicalDevice = devices[0];

    if (physicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to find suitable GPU");
        throw std::runtime_error("Failed to find suitable GPU");
    }

    // Log device info
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    LOG_INFO("Selected GPU: {}", deviceProperties.deviceName);
}

VulkanEngine::QueueFamilyIndices VulkanEngine::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

void VulkanEngine::recreateSwapchain() {
    LOG_INFO("Recreating swapchain due to window resize or out-of-date swapchain");

    // Handle minimization - wait until window is visible again
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    while (width == 0 || height == 0) {
        SDL_GetWindowSize(window, &width, &height);
        SDL_WaitEvent(nullptr);
    }

    // Wait for device to finish
    vkDeviceWaitIdle(device);

    // Recreate swapchain and dependent resources
    swapchain->recreate();
    renderer->recreateDepthResources(swapchain->getExtent());
    swapchain->createFramebuffers(pipeline->getRenderPass(), renderer->getDepthImageView());

    framebufferResized = false;

    LOG_INFO("Swapchain recreation complete");
}

void VulkanEngine::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    const std::vector<const char*> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    createInfo.enabledLayerCount = 0;

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        LOG_ERROR("Failed to create logical device");
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);

    LOG_DEBUG("Logical device created with graphics and present queues");
}

void VulkanEngine::mainLoop() {
    LOG_INFO("Entering main loop...");

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                LOG_INFO("Quit event received");
                running = false;
            } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                LOG_DEBUG("Window resized");
                framebufferResized = true;
            }
        }

        // Recreate swapchain if needed (after resize or out of date)
        if (framebufferResized) {
            recreateSwapchain();
        }

        renderer->drawFrame(swapchain->getSwapchain(), swapchain->getFramebuffers(),
                          pipeline->getRenderPass(), swapchain->getExtent(),
                          pipeline->getPipeline(), pipeline->getPipelineLayout(),
                          bufferManager->getVertexBuffer(), bufferManager->getIndexBuffer(),
                          static_cast<uint32_t>(indices.size()),
                          pipeline->getDescriptorSets(),
                          bufferManager->getUniformBuffersMapped(),
                          MAX_FRAMES_IN_FLIGHT,
                          framebufferResized);
    }

    renderer->waitIdle();
    LOG_INFO("Exited main loop");
}

void VulkanEngine::cleanup() {
    LOG_INFO("Cleaning up resources...");

    if (renderer) {
        renderer->cleanup();
    }

    if (bufferManager) {
        bufferManager->cleanup();
    }

    if (pipeline) {
        pipeline->cleanup();
    }

    if (swapchain) {
        swapchain->cleanup();
    }

    if (device != VK_NULL_HANDLE) {
        LOG_DEBUG("Destroying logical device");
        vkDestroyDevice(device, nullptr);
    }

    if (surface != VK_NULL_HANDLE) {
        LOG_DEBUG("Destroying surface");
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }

    if (instance != VK_NULL_HANDLE) {
        LOG_DEBUG("Destroying Vulkan instance");
        vkDestroyInstance(instance, nullptr);
    }

    if (window) {
        LOG_DEBUG("Destroying window");
        SDL_DestroyWindow(window);
    }

    LOG_DEBUG("Shutting down SDL");
    SDL_Quit();

    LOG_INFO("Cleanup complete");
}

} // namespace engine
