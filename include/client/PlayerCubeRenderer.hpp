#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <unordered_map>
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <chrono>

namespace engine {

// Forward declaration
class NetworkClient;

/**
 * @brief Renders 3D rainbow cubes at other players' positions
 */
class PlayerCubeRenderer {
public:
    PlayerCubeRenderer(VkDevice device, VkPhysicalDevice physicalDevice,
                      VkCommandPool commandPool, VkQueue graphicsQueue);
    ~PlayerCubeRenderer();

    // Delete copy operations
    PlayerCubeRenderer(const PlayerCubeRenderer&) = delete;
    PlayerCubeRenderer& operator=(const PlayerCubeRenderer&) = delete;

    /**
     * @brief Create rendering resources (pipeline, buffers)
     */
    void init(VkRenderPass renderPass, VkExtent2D swapchainExtent,
             VkDescriptorSetLayout descriptorSetLayout);

    /**
     * @brief Update player cube positions and colors
     * @param playerData Map of player ID to player data (position, yaw, pitch)
     */
    template<typename PlayerData>
    void update(const std::unordered_map<uint32_t, PlayerData>& playerData);

    /**
     * @brief Record draw commands for player cubes
     */
    void draw(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet);

    /**
     * @brief Recreate pipeline after swapchain resize
     */
    void recreatePipeline(VkRenderPass renderPass, VkExtent2D swapchainExtent,
                         VkDescriptorSetLayout descriptorSetLayout);

    /**
     * @brief Cleanup Vulkan resources
     */
    void cleanup();

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule = VK_NULL_HANDLE;

    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;

    VkImage faceTextureImage = VK_NULL_HANDLE;
    VkDeviceMemory faceTextureMemory = VK_NULL_HANDLE;
    VkImageView faceTextureImageView = VK_NULL_HANDLE;
    VkSampler faceTextureSampler = VK_NULL_HANDLE;

    VkDescriptorSetLayout textureDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet textureDescriptorSet = VK_NULL_HANDLE;

    struct PlayerCube {
        glm::vec3 position;
        glm::vec3 color;
        float yaw;
        float pitch;

        // For interpolation
        glm::vec3 targetPosition;
        float targetYaw;
        float targetPitch;
    };

    std::vector<PlayerCube> cubes;
    std::unordered_map<uint32_t, PlayerCube> playerStates;  // Track state per player ID

    void createPipeline(VkRenderPass renderPass, VkExtent2D swapchainExtent,
                       VkDescriptorSetLayout uboDescriptorSetLayout);
    void createBuffers();
    void loadFaceTexture();
    void createTextureDescriptors();
    void createTextureImage(const unsigned char* pixels, uint32_t width, uint32_t height);
    void createTextureImageView();
    void createTextureSampler();
    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
    std::vector<char> readFile(const std::string& filename);
    VkShaderModule createShaderModule(const std::vector<char>& code);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    glm::vec3 getRainbowColor(uint32_t playerId, float time);
};

// Template implementation
template<typename PlayerData>
void PlayerCubeRenderer::update(const std::unordered_map<uint32_t, PlayerData>& playerData) {
    cubes.clear();

    // Get current time for rainbow animation
    static auto startTime = std::chrono::steady_clock::now();
    auto currentTime = std::chrono::steady_clock::now();
    float time = std::chrono::duration<float>(currentTime - startTime).count();

    // Interpolation speed (higher = faster interpolation)
    const float interpSpeed = 10.0f;
    static auto lastUpdateTime = currentTime;
    float deltaTime = std::chrono::duration<float>(currentTime - lastUpdateTime).count();
    lastUpdateTime = currentTime;

    for (const auto& [playerId, data] : playerData) {
        // Get or create player state
        if (playerStates.find(playerId) == playerStates.end()) {
            // New player - initialize with no interpolation
            PlayerCube& state = playerStates[playerId];
            state.position = data.position;
            state.yaw = data.yaw;
            state.pitch = data.pitch;
            state.targetPosition = data.position;
            state.targetYaw = data.yaw;
            state.targetPitch = data.pitch;
        }

        PlayerCube& state = playerStates[playerId];

        // Update targets
        state.targetPosition = data.position;
        state.targetYaw = data.yaw;
        state.targetPitch = data.pitch;

        // Interpolate position
        state.position = glm::mix(state.position, state.targetPosition, std::min(interpSpeed * deltaTime, 1.0f));

        // Interpolate rotation (handle wrapping for yaw)
        float yawDiff = state.targetYaw - state.yaw;
        if (yawDiff > 180.0f) yawDiff -= 360.0f;
        if (yawDiff < -180.0f) yawDiff += 360.0f;
        state.yaw += yawDiff * std::min(interpSpeed * deltaTime, 1.0f);

        state.pitch = glm::mix(state.pitch, state.targetPitch, std::min(interpSpeed * deltaTime, 1.0f));

        // Add to render list
        PlayerCube cube;
        cube.position = state.position;
        cube.yaw = state.yaw;
        cube.pitch = state.pitch;
        cube.color = getRainbowColor(playerId, time);
        cubes.push_back(cube);
    }

    // Remove states for players that disconnected
    for (auto it = playerStates.begin(); it != playerStates.end();) {
        if (playerData.find(it->first) == playerData.end()) {
            it = playerStates.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace engine
