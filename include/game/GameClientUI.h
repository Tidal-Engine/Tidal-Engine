/**
 * @file GameClientUI.h
 * @brief ImGui-based user interface system for game client
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include "game/GameClient.h" // For GameState enum and other types
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan.h>

#include <vector>
#include <string>

// Forward declarations
class GameClient;
class VulkanDevice;

/**
 * @brief User interface system managing all ImGui rendering
 *
 * GameClientUI handles all user interface rendering and interaction:
 * - Main menu and navigation system
 * - World selection and creation dialogs
 * - In-game HUD elements (crosshair, hotbar, debug info)
 * - Performance monitoring overlays
 * - Settings and configuration interfaces
 * - ImGui integration with Vulkan rendering pipeline
 *
 * Features:
 * - State-driven UI rendering based on game state
 * - Vulkan descriptor management for textures
 * - Modular UI component system
 * - Debug and performance visualization tools
 *
 * @see GameClient for main game state management
 * @see VulkanDevice for Vulkan integration
 */
class GameClientUI {
public:
    /**
     * @brief Construct UI system with required dependencies
     * @param gameClient Reference to main game client
     * @param window GLFW window handle for UI context
     * @param device Vulkan device for descriptor management
     */
    GameClientUI(GameClient& gameClient, GLFWwindow* window, VulkanDevice& device);
    /**
     * @brief Destructor - cleanup ImGui resources
     */
    ~GameClientUI();

    /// @name Initialization
    /// @{
    /**
     * @brief Initialize ImGui and Vulkan integration
     * @return True if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown and cleanup all UI resources
     */
    void shutdown();
    /// @}

    /// @name Rendering
    /// @{
    /**
     * @brief Render ImGui frame to command buffer
     * @param commandBuffer Vulkan command buffer for recording
     */
    void renderImGui(VkCommandBuffer commandBuffer);

    /**
     * @brief Render main menu interface
     */
    void renderMainMenu();

    /**
     * @brief Render world selection screen
     */
    void renderWorldSelection();

    /**
     * @brief Render pause menu (ESC menu)
     */
    void renderPauseMenu();

    /**
     * @brief Render loading screen with progress
     */
    void renderLoadingScreen();

    /**
     * @brief Render debug information window
     */
    void renderDebugWindow();

    /**
     * @brief Render crosshair overlay
     */
    void renderCrosshair();

    /**
     * @brief Render hotbar with selected slot highlight
     */
    void renderHotbar();

    /**
     * @brief Render performance metrics HUD
     */
    void renderPerformanceHUD();

    /**
     * @brief Render rendering statistics HUD
     */
    void renderRenderingHUD();

    /**
     * @brief Render camera information HUD
     */
    void renderCameraHUD();
    /// @}

    /// @name Debug Window State
    /// @{
    /**
     * @brief Set debug window visibility
     * @param show True to show debug window
     */
    void setShowDebugWindow(bool show) { m_showDebugWindow = show; }

    /**
     * @brief Check if debug window is visible
     * @return True if debug window is showing
     */
    bool isShowingDebugWindow() const { return m_showDebugWindow; }

    /**
     * @brief Toggle debug window visibility
     */
    void toggleDebugWindow() { m_showDebugWindow = !m_showDebugWindow; }
    /// @}

    /// @name Performance HUD State
    /// @{
    /**
     * @brief Set performance HUD visibility
     * @param show True to show performance HUD
     */
    void setShowPerformanceHUD(bool show) { m_showPerformanceHUD = show; }

    /**
     * @brief Check if performance HUD is visible
     * @return True if performance HUD is showing
     */
    bool isShowingPerformanceHUD() const { return m_showPerformanceHUD; }
    /// @}

    /// @name Rendering HUD State
    /// @{
    /**
     * @brief Set rendering HUD visibility
     * @param show True to show rendering HUD
     */
    void setShowRenderingHUD(bool show) { m_showRenderingHUD = show; }

    /**
     * @brief Check if rendering HUD is visible
     * @return True if rendering HUD is showing
     */
    bool isShowingRenderingHUD() const { return m_showRenderingHUD; }
    /// @}

    /// @name Camera HUD State
    /// @{
    /**
     * @brief Set camera HUD visibility
     * @param show True to show camera HUD
     */
    void setShowCameraHUD(bool show) { m_showCameraHUD = show; }

    /**
     * @brief Check if camera HUD is visible
     * @return True if camera HUD is showing
     */
    bool isShowingCameraHUD() const { return m_showCameraHUD; }
    /// @}

    /// @name Chunk Boundaries State
    /// @{
    /**
     * @brief Set chunk boundary visualization
     * @param show True to show chunk boundaries
     */
    void setShowChunkBoundaries(bool show) { m_showChunkBoundaries = show; }

    /**
     * @brief Check if chunk boundaries are visible
     * @return True if chunk boundaries are showing
     */
    bool isShowingChunkBoundaries() const { return m_showChunkBoundaries; }
    /// @}

    /// @name World Creation Dialog State
    /// @{
    /**
     * @brief Set create world dialog visibility
     * @param show True to show create world dialog
     */
    void setShowCreateWorldDialog(bool show) { m_showCreateWorldDialog = show; }

    /**
     * @brief Check if create world dialog is visible
     * @return True if create world dialog is showing
     */
    bool isShowingCreateWorldDialog() const { return m_showCreateWorldDialog; }
    /// @}

    /// @name Texture Management
    /// @{
    /**
     * @brief Create Vulkan descriptor for atlas texture
     */
    void createAtlasTextureDescriptor();

    /**
     * @brief Get atlas texture descriptor for rendering
     * @return Vulkan descriptor set for atlas texture
     */
    VkDescriptorSet getAtlasTextureDescriptor() const { return m_atlasTextureDescriptor; }
    /// @}

    /// @name World Management UI
    /// @{
    /**
     * @brief Get buffer for new world name input
     * @return Pointer to world name input buffer
     */
    char* getNewWorldNameBuffer() { return m_newWorldName; }

    static constexpr size_t NEW_WORLD_NAME_SIZE = 64;  ///< Maximum world name length
    /// @}

private:
    GameClient& m_gameClient;   ///< Reference to main game client
    GLFWwindow* m_window;       ///< GLFW window handle
    VulkanDevice& m_device;     ///< Vulkan device for resource management

    /// @name ImGui Resources
    /// @{
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;    ///< ImGui descriptor pool
    VkDescriptorSet m_atlasTextureDescriptor = VK_NULL_HANDLE;  ///< Atlas texture descriptor
    /// @}

    /// @name UI State Flags
    /// @{
    bool m_showDebugWindow = false;         ///< Debug window visibility
    bool m_showPerformanceHUD = false;      ///< Performance HUD visibility
    bool m_showRenderingHUD = false;        ///< Rendering stats HUD visibility
    bool m_showCameraHUD = false;           ///< Camera info HUD visibility
    bool m_showChunkBoundaries = false;     ///< Chunk boundary visualization
    bool m_showCreateWorldDialog = false;   ///< Create world dialog visibility
    /// @}

    /// @name World Management State
    /// @{
    char m_newWorldName[NEW_WORLD_NAME_SIZE] = {};  ///< Buffer for new world name input
    /// @}

    /// @name Private Implementation
    /// @{
    /**
     * @brief Initialize ImGui system and Vulkan integration
     */
    void initImGui();

    /**
     * @brief Cleanup ImGui resources and shutdown
     */
    void cleanupImGui();
    /// @}
};