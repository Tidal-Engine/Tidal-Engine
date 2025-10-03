#pragma once

#include <imgui.h>
#include <vector>
#include <string>
#include <glm/glm.hpp>

namespace engine {

// Forward declarations
class Camera;
class PerformanceMetrics;
class NetworkClient;

/**
 * @brief Debug overlay for displaying runtime information (F3 menu)
 *
 * Provides a comprehensive debug interface showing camera position, rendering stats,
 * network info, and performance metrics. Similar to Minecraft's F3 debug screen.
 */
class DebugOverlay {
public:
    DebugOverlay();
    ~DebugOverlay() = default;

    /**
     * @brief Toggle the debug overlay on/off
     */
    void toggle();

    /**
     * @brief Set visibility state
     */
    void setVisible(bool visible) { isVisible = visible; }

    /**
     * @brief Check if overlay is visible
     */
    bool getVisible() const { return isVisible; }

    /**
     * @brief Render the debug overlay
     * @param camera Camera for position/rotation info
     * @param metrics Performance metrics
     * @param networkClient Network client for connection stats
     * @param chunksVisible Number of chunks currently visible
     * @param chunksTotal Total number of chunks loaded
     * @param verticesRendered Total vertices rendered this frame
     * @param drawCalls Number of draw calls this frame
     */
    void render(const Camera* camera,
                const PerformanceMetrics* metrics,
                const NetworkClient* networkClient,
                uint32_t chunksVisible,
                uint32_t chunksTotal,
                uint32_t verticesRendered,
                uint32_t drawCalls);

private:
    bool isVisible = false;

    // FPS history for graph
    static constexpr size_t FPS_HISTORY_SIZE = 100;
    std::vector<float> fpsHistory;
    size_t fpsHistoryIndex = 0;

    /**
     * @brief Render camera position and orientation section
     * @param camera Camera to display info for
     */
    void renderCameraInfo(const Camera* camera);

    /**
     * @brief Render rendering statistics section
     * @param chunksVisible Number of chunks being rendered
     * @param chunksTotal Total chunks loaded
     * @param verticesRendered Total vertices rendered
     * @param drawCalls Number of draw calls
     */
    void renderRenderingStats(uint32_t chunksVisible, uint32_t chunksTotal,
                              uint32_t verticesRendered, uint32_t drawCalls);

    /**
     * @brief Render performance metrics section
     * @param metrics Performance metrics to display
     */
    void renderPerformanceInfo(const PerformanceMetrics* metrics);

    /**
     * @brief Render network connection info section
     * @param networkClient Network client to display info for
     */
    void renderNetworkInfo(const NetworkClient* networkClient);

    /**
     * @brief Format large numbers with comma separators
     * @param num Number to format
     * @return Formatted string (e.g., "1,234,567")
     */
    std::string formatNumber(uint32_t num);
};

} // namespace engine
