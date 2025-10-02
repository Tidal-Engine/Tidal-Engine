#include "vulkan/VulkanEngine.hpp"
#include "vulkan/VulkanBuffer.hpp"
#include "vulkan/VulkanSwapchain.hpp"
#include "vulkan/VulkanPipeline.hpp"
#include "vulkan/VulkanRenderer.hpp"
#include "vulkan/CubeGeometry.hpp"
#include "core/Logger.hpp"
#include "core/ResourceManager.hpp"

#include <stdexcept>
#include <cstring>

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

    LOG_INFO("Creating window ({}x{})...", config.windowWidth, config.windowHeight);
    window = SDL_CreateWindow(
        config.windowTitle.c_str(),
        static_cast<int>(config.windowWidth), static_cast<int>(config.windowHeight),
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

    // Initialize resource manager and register assets
    ResourceManager::init(".");
    ResourceManager::registerShader("cube_vert", "shaders/cube_vert.spv");
    ResourceManager::registerShader("cube_frag", "shaders/cube_frag.spv");

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
    pipeline->createGraphicsPipeline(
        ResourceManager::getShaderPath("cube_vert"),
        ResourceManager::getShaderPath("cube_frag"));

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
    bufferManager->createUniformBuffers(EngineConfig::MAX_FRAMES_IN_FLIGHT, sizeof(UniformBufferObject));

    // Create descriptor sets
    pipeline->createDescriptorPool(EngineConfig::MAX_FRAMES_IN_FLIGHT);
    pipeline->createDescriptorSets(bufferManager->getUniformBuffers(), sizeof(UniformBufferObject));

    // Create command buffers and sync objects
    renderer->createCommandBuffers(EngineConfig::MAX_FRAMES_IN_FLIGHT);
    renderer->createSyncObjects(EngineConfig::MAX_FRAMES_IN_FLIGHT);

    LOG_INFO("Rendering resources initialized successfully");
}

void VulkanEngine::createInstance() {
    LOG_DEBUG("Creating Vulkan instance...");
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = config.windowTitle.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = EngineConfig::ENGINE_NAME;
    appInfo.engineVersion = VK_MAKE_VERSION(EngineConfig::ENGINE_VERSION_MAJOR,
                                             EngineConfig::ENGINE_VERSION_MINOR,
                                             EngineConfig::ENGINE_VERSION_PATCH);
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

#ifdef ENABLE_VALIDATION_LAYERS
    const std::vector<const char*> VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
    };

    // Check if validation layers are available
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool layersAvailable = true;
    for (const char* layerName : VALIDATION_LAYERS) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            LOG_WARN("Validation layer not available: {}", layerName);
            layersAvailable = false;
        }
    }

    if (layersAvailable) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
        LOG_INFO("Validation layers enabled");
    } else {
        createInfo.enabledLayerCount = 0;
        LOG_WARN("Validation layers requested but not available - continuing without them");
    }
#else
    createInfo.enabledLayerCount = 0;
    LOG_DEBUG("Validation layers disabled");
#endif

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
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr) != VK_SUCCESS) {
        LOG_ERROR("Failed to enumerate physical devices");
        throw std::runtime_error("Failed to enumerate physical devices");
    }

    if (deviceCount == 0) {
        LOG_ERROR("No GPUs with Vulkan support found");
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    LOG_DEBUG("Found {} Vulkan-capable device(s)", deviceCount);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    if (vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()) != VK_SUCCESS) {
        LOG_ERROR("Failed to get physical device list");
        throw std::runtime_error("Failed to get physical device list");
    }

    // Score and select the best device
    int bestScore = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

    for (const auto& device : devices) {
        int score = rateDeviceSuitability(device);
        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
        }
    }

    if (bestDevice == VK_NULL_HANDLE || bestScore < 0) {
        LOG_ERROR("Failed to find suitable GPU");
        throw std::runtime_error("Failed to find suitable GPU");
    }

    physicalDevice = bestDevice;

    // Log device info
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    LOG_INFO("Selected GPU: {} (score: {})", deviceProperties.deviceName, bestScore);
}

VulkanEngine::QueueFamilyIndices VulkanEngine::findQueueFamilies(VkPhysicalDevice device) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    if (queueFamilyCount == 0) {
        LOG_ERROR("No queue families found for device");
        throw std::runtime_error("No queue families found");
    }

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport) != VK_SUCCESS) {
            LOG_WARN("Failed to check present support for queue family {}", i);
            continue;
        }

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    if (!indices.isComplete()) {
        LOG_ERROR("Failed to find all required queue families");
        throw std::runtime_error("Failed to find required queue families");
    }

    LOG_DEBUG("Found queue families - Graphics: {}, Present: {}",
              indices.graphicsFamily, indices.presentFamily);

    return indices;
}

int VulkanEngine::rateDeviceSuitability(VkPhysicalDevice device) {
    if (!isDeviceSuitable(device)) {
        return -1;
    }

    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

    int score = 0;

    // Discrete GPUs have a significant performance advantage
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }

    // Maximum possible size of textures affects graphics quality
    score += static_cast<int>(deviceProperties.limits.maxImageDimension2D);

    // Prefer devices with geometry shader support
    if (deviceFeatures.geometryShader) {
        score += 100;
    }

    // Prefer devices with anisotropic filtering
    if (deviceFeatures.samplerAnisotropy) {
        score += 50;
    }

    LOG_DEBUG("GPU '{}' rated with score: {}", deviceProperties.deviceName, score);
    return score;
}

bool VulkanEngine::isDeviceSuitable(VkPhysicalDevice device) {
    // Check queue families
    QueueFamilyIndices indices = findQueueFamilies(device);
    if (!indices.isComplete()) {
        LOG_DEBUG("Device missing required queue families");
        return false;
    }

    // Check for required features
    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    // We need geometry shader support for future features
    if (!supportedFeatures.geometryShader) {
        LOG_DEBUG("Device missing geometry shader support");
        return false;
    }

    return true;
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
        performanceMetrics.beginFrame();

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
            framebufferResized = false;
        }

        bool needsRecreation = renderer->drawFrame(swapchain->getSwapchain(), swapchain->getFramebuffers(),
                          pipeline->getRenderPass(), swapchain->getExtent(),
                          pipeline->getPipeline(), pipeline->getPipelineLayout(),
                          bufferManager->getVertexBuffer(), bufferManager->getIndexBuffer(),
                          static_cast<uint32_t>(indices.size()),
                          pipeline->getDescriptorSets(),
                          bufferManager->getUniformBuffersMapped(),
                          EngineConfig::MAX_FRAMES_IN_FLIGHT);

        if (needsRecreation) {
            framebufferResized = true;
        }

        performanceMetrics.endFrame();
    }

    renderer->waitIdle();
    LOG_INFO("Exited main loop - Total frames: {}, Average FPS: {:.1f}",
             performanceMetrics.getFrameCount(), performanceMetrics.getFPS());
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
