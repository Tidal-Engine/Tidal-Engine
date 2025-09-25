#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

class UserDataManager {
public:
    UserDataManager();
    ~UserDataManager() = default;

    // Initialize user data directory and copy defaults if needed
    bool initialize();

    // Directory paths
    std::string getUserDataPath() const { return m_userDataPath; }
    std::string getSavesPath() const { return m_userDataPath + "/saves"; }
    std::string getTexturepacksPath() const { return m_userDataPath + "/texturepacks"; }
    std::string getSettingsPath() const { return m_userDataPath + "/settings"; }
    std::string getConfigPath() const { return m_userDataPath + "/config.json"; }

    // Texturepack management
    std::vector<std::string> getAvailableTexturepacks() const;
    std::string getActiveTexturepackPath() const;
    bool setActiveTexturepack(const std::string& name);
    bool texturepackExists(const std::string& name) const;

    // Settings management
    bool loadSettings();
    bool saveSettings();
    std::string getSetting(const std::string& key, const std::string& defaultValue = "") const;
    void setSetting(const std::string& key, const std::string& value);

private:
    std::string m_userDataPath;
    std::string m_activeTexturepack = "default";
    std::unordered_map<std::string, std::string> m_settings;

    // Platform-specific paths
    std::string getPlatformUserDataPath() const;

    // Directory creation and setup
    bool createDirectoryStructure();
    bool copyDefaultAssets();
    bool copyDefaultTexturepack();

    // Helper functions
    bool directoryExists(const std::string& path) const;
    bool createDirectory(const std::string& path) const;
    bool copyFile(const std::string& src, const std::string& dst) const;
    bool copyDirectoryRecursive(const std::string& src, const std::string& dst) const;
};