#include "core/PerformanceMetrics.hpp"
#include "core/Logger.hpp"

#include <algorithm>

namespace engine {

PerformanceMetrics::PerformanceMetrics() {
    lastLogTime = Clock::now();  // Initialize to avoid immediate logging on startup
}

void PerformanceMetrics::beginFrame() {
    frameStartTime = Clock::now();
}

void PerformanceMetrics::endFrame() {
    auto frameEndTime = Clock::now();
    std::chrono::duration<double> frameDuration = frameEndTime - frameStartTime;
    deltaTime = frameDuration.count();

    double frameTimeMs = deltaTime * 1000.0;
    frameTimes.push_back(frameTimeMs);

    // Keep only the last SAMPLE_COUNT frames
    if (frameTimes.size() > SAMPLE_COUNT) {
        frameTimes.pop_front();
    }

    // Calculate average frame time
    double sum = 0.0;
    for (double time : frameTimes) {
        sum += time;
    }
    averageFrameTime = sum / static_cast<double>(frameTimes.size());

    // Calculate FPS from average frame time
    if (averageFrameTime > 0.0) {
        fps = 1000.0 / averageFrameTime;
    }

    // Update min/max
    minFrameTime = std::min(minFrameTime, frameTimeMs);
    maxFrameTime = std::max(maxFrameTime, frameTimeMs);

    frameCount++;

    // Log performance every 10 seconds (time-based, not frame-based)
    auto now = Clock::now();
    std::chrono::duration<double> timeSinceLastLog = now - lastLogTime;
    if (timeSinceLastLog.count() >= 10.0) {
        LOG_DEBUG("Performance: {:.1f} FPS | Frame time: {:.2f}ms (avg), {:.2f}ms (min), {:.2f}ms (max)",
                  fps, averageFrameTime, minFrameTime, maxFrameTime);
        lastLogTime = now;
    }

    lastFrameTime = frameEndTime;
}

void PerformanceMetrics::reset() {
    frameTimes.clear();
    deltaTime = 0.0;
    fps = 0.0;
    averageFrameTime = 0.0;
    minFrameTime = 999999.0;
    maxFrameTime = 0.0;
    frameCount = 0;
    lastLogTime = Clock::now();  // Reset log timer
}

} // namespace engine
