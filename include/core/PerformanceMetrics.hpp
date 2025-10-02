#pragma once

#include <chrono>
#include <deque>
#include <cstdint>

namespace engine {

/**
 * @brief Tracks performance metrics like FPS and frame time
 *
 * Maintains a rolling average for smooth FPS display and
 * tracks min/max frame times for performance analysis.
 */
class PerformanceMetrics {
public:
    PerformanceMetrics() = default;
    ~PerformanceMetrics() = default;

    /**
     * @brief Start timing a new frame
     */
    void beginFrame();

    /**
     * @brief End timing for current frame and update metrics
     */
    void endFrame();

    /**
     * @brief Get current FPS (rolling average)
     * @return Frames per second
     */
    double getFPS() const { return fps; }

    /**
     * @brief Get delta time for last frame in seconds
     * @return Delta time in seconds
     */
    double getDeltaTime() const { return deltaTime; }

    /**
     * @brief Get average frame time in milliseconds
     * @return Average frame time
     */
    double getAverageFrameTime() const { return averageFrameTime; }

    /**
     * @brief Get minimum frame time in milliseconds (best performance)
     * @return Minimum frame time
     */
    double getMinFrameTime() const { return minFrameTime; }

    /**
     * @brief Get maximum frame time in milliseconds (worst performance)
     * @return Maximum frame time
     */
    double getMaxFrameTime() const { return maxFrameTime; }

    /**
     * @brief Get total number of frames rendered
     * @return Frame count
     */
    uint64_t getFrameCount() const { return frameCount; }

    /**
     * @brief Reset all metrics
     */
    void reset();

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;

    TimePoint frameStartTime;
    TimePoint lastFrameTime;

    double deltaTime = 0.0;
    double fps = 0.0;
    double averageFrameTime = 0.0;
    double minFrameTime = 999999.0;
    double maxFrameTime = 0.0;

    uint64_t frameCount = 0;
    static constexpr size_t SAMPLE_COUNT = 60; // Average over 60 frames
    std::deque<double> frameTimes;
};

} // namespace engine
