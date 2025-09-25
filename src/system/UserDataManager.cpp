#include "UserDataManager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif

namespace fs = std::filesystem;

UserDataManager::UserDataManager() {
    m_userDataPath = getPlatformUserDataPath();
}

bool UserDataManager::initialize() {
    std::cout << "Initializing user data directory: " << m_userDataPath << std::endl;

    // Create directory structure
    if (!createDirectoryStructure()) {
        std::cerr << "Failed to create user data directory structure" << std::endl;
        return false;
    }

    // Copy default assets if they don't exist
    if (!copyDefaultAssets()) {
        std::cerr << "Failed to copy default assets" << std::endl;
        return false;
    }

    // Load settings
    if (!loadSettings()) {
        std::cout << "No existing settings found, using defaults" << std::endl;
    }

    return true;
}

std::string UserDataManager::getPlatformUserDataPath() const {
#ifdef _WIN32
    char* appDataPath = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appDataPath, &len, "APPDATA") == 0 && appDataPath != nullptr) {
        std::string path = std::string(appDataPath) + "\\TidalEngine";
        free(appDataPath);
        return path;
    }
    return ".\\TidalEngine"; // Fallback to current directory
#else
    const char* homeDir = getenv("HOME");
    if (homeDir == nullptr) {
        homeDir = getpwuid(getuid())->pw_dir;
    }
    return std::string(homeDir) + "/.tidal-engine";
#endif
}

bool UserDataManager::createDirectoryStructure() {
    std::vector<std::string> directories = {
        m_userDataPath,
        getSavesPath(),
        getTexturepacksPath(),
        getSettingsPath()
    };

    for (const auto& dir : directories) {
        if (!createDirectory(dir)) {
            std::cerr << "Failed to create directory: " << dir << std::endl;
            return false;
        }
    }

    return true;
}

bool UserDataManager::copyDefaultAssets() {
    // Copy default texturepack if it doesn't exist
    std::string defaultTexturePack = getTexturepacksPath() + "/default";
    if (!directoryExists(defaultTexturePack)) {
        return copyDefaultTexturepack();
    }
    return true;
}

bool UserDataManager::copyDefaultTexturepack() {
    std::string srcPath = "./assets/texturepacks/default";
    std::string dstPath = getTexturepacksPath() + "/default";

    // If source doesn't exist, create a minimal default texturepack
    if (!directoryExists(srcPath)) {
        std::cout << "Creating minimal default texturepack..." << std::endl;

        if (!createDirectory(dstPath)) {
            return false;
        }

        std::string blocksPath = dstPath + "/blocks";
        if (!createDirectory(blocksPath)) {
            return false;
        }

        // Create a simple pack.json
        std::ofstream packFile(dstPath + "/pack.json");
        if (packFile.is_open()) {
            packFile << "{\n";
            packFile << "  \"name\": \"Default\",\n";
            packFile << "  \"description\": \"Default Tidal Engine texturepack\",\n";
            packFile << "  \"version\": \"1.0.0\",\n";
            packFile << "  \"texture_size\": 16\n";
            packFile << "}\n";
            packFile.close();
        }

        std::cout << "Created minimal default texturepack structure" << std::endl;
        return true;
    }

    // Copy from assets
    return copyDirectoryRecursive(srcPath, dstPath);
}

std::vector<std::string> UserDataManager::getAvailableTexturepacks() const {
    std::vector<std::string> texturepacks;

    try {
        for (const auto& entry : fs::directory_iterator(getTexturepacksPath())) {
            if (entry.is_directory()) {
                texturepacks.push_back(entry.path().filename().string());
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error reading texturepacks directory: " << e.what() << std::endl;
    }

    return texturepacks;
}

std::string UserDataManager::getActiveTexturepackPath() const {
    return getTexturepacksPath() + "/" + m_activeTexturepack;
}

bool UserDataManager::setActiveTexturepack(const std::string& name) {
    if (texturepackExists(name)) {
        m_activeTexturepack = name;
        setSetting("active_texturepack", name);
        return saveSettings();
    }
    return false;
}

bool UserDataManager::texturepackExists(const std::string& name) const {
    std::string path = getTexturepacksPath() + "/" + name;
    return directoryExists(path);
}

bool UserDataManager::loadSettings() {
    std::ifstream file(getConfigPath());
    if (!file.is_open()) {
        return false;
    }

    m_settings.clear();
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            m_settings[key] = value;
        }
    }

    // Load active texturepack
    m_activeTexturepack = getSetting("active_texturepack", "default");

    return true;
}

bool UserDataManager::saveSettings() {
    std::ofstream file(getConfigPath());
    if (!file.is_open()) {
        return false;
    }

    // Save active texturepack
    setSetting("active_texturepack", m_activeTexturepack);

    // Write all settings
    for (const auto& setting : m_settings) {
        file << setting.first << "=" << setting.second << "\n";
    }

    return true;
}

std::string UserDataManager::getSetting(const std::string& key, const std::string& defaultValue) const {
    auto it = m_settings.find(key);
    return (it != m_settings.end()) ? it->second : defaultValue;
}

void UserDataManager::setSetting(const std::string& key, const std::string& value) {
    m_settings[key] = value;
}

bool UserDataManager::directoryExists(const std::string& path) const {
    return fs::exists(path) && fs::is_directory(path);
}

bool UserDataManager::createDirectory(const std::string& path) const {
    try {
        // create_directories returns false if directory already exists, but that's OK
        fs::create_directories(path);
        return fs::exists(path) && fs::is_directory(path);
    } catch (const std::exception& e) {
        std::cerr << "Error creating directory " << path << ": " << e.what() << std::endl;
        return false;
    }
}

bool UserDataManager::copyFile(const std::string& src, const std::string& dst) const {
    try {
        return fs::copy_file(src, dst, fs::copy_options::overwrite_existing);
    } catch (const std::exception& e) {
        std::cerr << "Error copying file " << src << " to " << dst << ": " << e.what() << std::endl;
        return false;
    }
}

bool UserDataManager::copyDirectoryRecursive(const std::string& src, const std::string& dst) const {
    try {
        fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error copying directory " << src << " to " << dst << ": " << e.what() << std::endl;
        return false;
    }
}