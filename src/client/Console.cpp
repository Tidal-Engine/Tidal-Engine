#include "client/Console.hpp"
#include "client/NetworkClient.hpp"
#include "core/Logger.hpp"

#include <imgui.h>
#include <sstream>
#include <algorithm>

namespace engine {

Console::Console() {
    addMessage("Console initialized. Type /help for available commands.");
}

Console::~Console() = default;

void Console::toggle() {
    visible = !visible;
    if (visible) {
        focusInput = true;
    }
}

void Console::addMessage(const std::string& message) {
    messages.push_back(message);

    // Limit message history
    if (messages.size() > MAX_MESSAGES) {
        messages.erase(messages.begin());
    }

    scrollToBottom = true;
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
void Console::render() {
    if (!visible) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Console", &visible, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Output area
    const float FOOTER_HEIGHT_RESERVE = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -FOOTER_HEIGHT_RESERVE), false, ImGuiWindowFlags_HorizontalScrollbar)) {
        for (const auto& message : messages) {
            ImGui::TextUnformatted(message.c_str());
        }

        if (scrollToBottom) {
            ImGui::SetScrollHereY(1.0f);
            scrollToBottom = false;
        }
    }
    ImGui::EndChild();

    // Input area
    ImGui::Separator();

    ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;

    auto historyCallback = [](ImGuiInputTextCallbackData* data) -> int {
        Console* console = static_cast<Console*>(data->UserData);

        if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
            if (data->EventKey == ImGuiKey_UpArrow) {
                if (console->historyIndex < static_cast<int>(console->commandHistory.size()) - 1) {
                    console->historyIndex++;
                    const std::string& cmd = console->commandHistory[console->commandHistory.size() - 1 - console->historyIndex];
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, cmd.c_str());
                }
            } else if (data->EventKey == ImGuiKey_DownArrow) {
                if (console->historyIndex > 0) {
                    console->historyIndex--;
                    const std::string& cmd = console->commandHistory[console->commandHistory.size() - 1 - console->historyIndex];
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, cmd.c_str());
                } else if (console->historyIndex == 0) {
                    console->historyIndex = -1;
                    data->DeleteChars(0, data->BufTextLen);
                }
            }
        }
        return 0;
    };

    if (focusInput) {
        ImGui::SetKeyboardFocusHere();
        focusInput = false;
    }

    if (ImGui::InputText("##input", inputBuffer, sizeof(inputBuffer), inputFlags, historyCallback, this)) {
        std::string command = inputBuffer;
        if (!command.empty()) {
            addMessage("> " + command);
            commandHistory.push_back(command);
            historyIndex = -1;
            executeCommand(command);
        }
        inputBuffer[0] = '\0';
        focusInput = true;
    }

    ImGui::End();
}

std::vector<std::string> Console::tokenize(const std::string& str) {  // NOLINT(readability-convert-member-functions-to-static)
    std::vector<std::string> tokens;
    std::istringstream stream(str);
    std::string token;

    while (stream >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

bool Console::executeCommand(const std::string& command) {
    auto tokens = tokenize(command);
    if (tokens.empty()) {
        return false;
    }

    std::string cmd = tokens[0];

    // Remove leading slash if present
    if (!cmd.empty() && cmd[0] == '/') {
        cmd = cmd.substr(1);
    }

    // Convert to lowercase for case-insensitive matching
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    // Remove the command from tokens
    tokens.erase(tokens.begin());

    if (cmd == "help") {
        cmdHelp(tokens);
    } else if (cmd == "connect" || cmd == "join") {
        cmdConnect(tokens);
    } else if (cmd == "disconnect") {
        cmdDisconnect(tokens);
    } else if (cmd == "clear") {
        cmdClear(tokens);
    } else {
        addMessage("Unknown command: " + cmd);
        addMessage("Type /help for available commands");
        return false;
    }

    return true;
}

void Console::cmdHelp(const std::vector<std::string>& args) {
    addMessage("=== Available Commands ===");
    addMessage("/connect <ip> [port] - Connect to a server");
    addMessage("  Examples:");
    addMessage("    /connect 127.0.0.1");
    addMessage("    /connect playit.gg-address 12345");
    addMessage("    /connect localhost 25565");
    addMessage("/disconnect - Disconnect from current server");
    addMessage("/clear - Clear console messages");
    addMessage("/help - Show this help message");
    addMessage("=========================");
}

void Console::cmdConnect(const std::vector<std::string>& args) {
    if (!networkClient) {
        addMessage("ERROR: Network client not available");
        return;
    }

    if (args.empty()) {
        addMessage("ERROR: No server address provided");
        addMessage("Usage: /connect <ip> [port]");
        return;
    }

    std::string host = args[0];
    uint16_t port = 25565;  // Default port

    if (args.size() > 1) {
        try {
            port = static_cast<uint16_t>(std::stoi(args[1]));
        } catch (...) {
            addMessage("ERROR: Invalid port number: " + args[1]);
            return;
        }
    }

    if (networkClient->isConnected()) {
        addMessage("Disconnecting from current server...");
        networkClient->disconnect();
    }

    addMessage("Connecting to " + host + ":" + std::to_string(port) + " as '" + username + "'...");

    if (networkClient->connect(host, username, port)) {
        addMessage("Successfully connected to server!");
        LOG_INFO("Console: Connected to {}:{} as '{}'", host, port, username);
    } else {
        addMessage("ERROR: Failed to connect to server");
        addMessage("Make sure the server is running and address is correct");
        LOG_ERROR("Console: Failed to connect to {}:{}", host, port);
    }
}

void Console::cmdDisconnect(const std::vector<std::string>& args) {
    if (!networkClient) {
        addMessage("ERROR: Network client not available");
        return;
    }

    if (!networkClient->isConnected()) {
        addMessage("Not connected to any server");
        return;
    }

    networkClient->disconnect();
    addMessage("Disconnected from server");
    LOG_INFO("Console: Disconnected from server");
}

void Console::cmdClear(const std::vector<std::string>& args) {
    messages.clear();
    addMessage("Console cleared");
}

} // namespace engine
