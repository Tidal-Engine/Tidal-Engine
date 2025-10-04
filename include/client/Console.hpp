#pragma once

#include <string>
#include <vector>
#include <functional>
#include <vulkan/vulkan.h>

namespace engine {

// Forward declarations
class VulkanEngine;
class NetworkClient;

/**
 * @brief In-game console for commands and messages
 *
 * Provides a developer console UI that can be toggled with the ~ key.
 * Supports commands like /connect, /disconnect, etc.
 */
class Console {
public:
    Console();
    ~Console();

    /**
     * @brief Toggle console visibility
     */
    void toggle();

    /**
     * @brief Check if console is open
     */
    bool isOpen() const { return visible; }

    /**
     * @brief Add a message to console log
     */
    void addMessage(const std::string& message);

    /**
     * @brief Render the console UI using ImGui
     */
    void render();

    /**
     * @brief Execute a console command
     * @return true if command was valid
     */
    bool executeCommand(const std::string& command);

    /**
     * @brief Set the network client for connection commands
     */
    void setNetworkClient(NetworkClient* client) { networkClient = client; }

    /**
     * @brief Set the username for server connections
     */
    void setUsername(const std::string& name) { username = name; }

private:
    bool visible = false;
    char inputBuffer[256] = {0};
    std::vector<std::string> messages;
    std::vector<std::string> commandHistory;
    int historyIndex = -1;
    bool scrollToBottom = false;
    bool focusInput = false;

    NetworkClient* networkClient = nullptr;
    std::string username = "Player";  // Default username

    static constexpr size_t MAX_MESSAGES = 100;

    // Command handlers
    void cmdConnect(const std::vector<std::string>& args);
    void cmdDisconnect(const std::vector<std::string>& args);
    void cmdHelp(const std::vector<std::string>& args);
    void cmdClear(const std::vector<std::string>& args);

    // Helper to split command into tokens
    std::vector<std::string> tokenize(const std::string& str);
};

} // namespace engine
