#pragma once

#include <cpptrace/cpptrace.hpp>
#include <spdlog/spdlog.h>
#include <csignal>
#include <sstream>

namespace engine {

/**
 * @brief Crash handling and stack trace utilities
 *
 * Wraps cpptrace to provide detailed stack traces on crashes.
 * Signal handlers are automatically installed by cpptrace.
 * Provides methods to manually capture and log stack traces.
 */
class CrashHandler {
public:
    /**
     * @brief Initialize crash handling (cpptrace auto-installs signal handlers)
     */
    static void init() {
        // cpptrace automatically installs signal handlers
        // No explicit initialization needed
    }

    /**
     * @brief Print stack trace to stdout
     */
    static void printStackTrace() {
        auto trace = cpptrace::generate_trace();
        trace.print();
    }

    /**
     * @brief Log stack trace using spdlog
     */
    static void logStackTrace() {
        auto trace = cpptrace::generate_trace();
        std::ostringstream oss;
        trace.print(oss);
        spdlog::error("Stack trace:\n{}", oss.str());
    }

    /**
     * @brief Get stack trace as a string
     * @return std::string Stack trace formatted as a string
     */
    static std::string getStackTraceString() {
        auto trace = cpptrace::generate_trace();
        std::ostringstream oss;
        trace.print(oss);
        return oss.str();
    }
};

} // namespace engine
