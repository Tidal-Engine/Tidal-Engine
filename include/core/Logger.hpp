#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>

namespace engine {

/**
 * @brief Logging system wrapper around spdlog
 *
 * Provides multi-sink logging to both console (with colors) and file.
 * Supports named loggers for subsystems and convenience macros for
 * different log levels (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL).
 */
class Logger {
public:
    /**
     * @brief Initialize the default logger
     * @param name Logger name (default: "TidalEngine")
     */
    static void init(const std::string& name [[maybe_unused]] = "TidalEngine") {
        // Create sinks
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        consoleSink->set_level(spdlog::level::trace);

        // Use basic_file_sink with truncate mode to overwrite on each run
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            "logs/engine.log", true);  // true = truncate (clear file on open)
        fileSink->set_level(spdlog::level::trace);

        // Create logger with both sinks
        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());

        logger->set_level(spdlog::level::trace);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] [thread %t] %v");

        spdlog::register_logger(logger);
        spdlog::set_default_logger(logger);
    }

    /**
     * @brief Create a named logger for a subsystem
     * @param name Subsystem name (e.g., "Vulkan", "Renderer")
     * @return std::shared_ptr<spdlog::logger> Logger instance
     */
    static std::shared_ptr<spdlog::logger> create(const std::string& name) {
        auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            std::string("logs/") + name + ".log", true);  // true = truncate

        std::vector<spdlog::sink_ptr> sinks{consoleSink, fileSink};
        auto logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());

        logger->set_level(spdlog::level::trace);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%^%l%$] %v");

        spdlog::register_logger(logger);
        return logger;
    }

    /**
     * @brief Get a logger by name
     * @param name Logger name (empty for default logger)
     * @return std::shared_ptr<spdlog::logger> Logger instance
     */
    static std::shared_ptr<spdlog::logger> get(const std::string& name = "") {
        if (name.empty()) {
            return spdlog::default_logger();
        }
        return spdlog::get(name);
    }

    /**
     * @brief Shutdown all loggers and flush buffers
     */
    static void shutdown() {
        spdlog::shutdown();
    }
};

// Convenience macros
// NOLINTBEGIN(cppcoreguidelines-macro-usage)
#define LOG_TRACE(...)    spdlog::trace(__VA_ARGS__)
#define LOG_DEBUG(...)    spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...)     spdlog::info(__VA_ARGS__)
#define LOG_WARN(...)     spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...)    spdlog::error(__VA_ARGS__)
#define LOG_CRITICAL(...) spdlog::critical(__VA_ARGS__)
// NOLINTEND(cppcoreguidelines-macro-usage)

} // namespace engine
