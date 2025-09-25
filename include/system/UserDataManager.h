/**
 * @file UserDataManager.h
 * @brief Cross-platform user data management system with settings and asset organization
 * @author Tidal Engine Team
 * @version 1.0
 */

#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <unordered_map>

/**
 * @brief Cross-platform user data management system
 *
 * The UserDataManager class handles all user-specific data operations including:
 * - Platform-specific user data directory resolution
 * - Directory structure creation and management
 * - Settings persistence and retrieval
 * - Texturepack management and switching
 * - Default asset copying and initialization
 *
 * Supports standard platform conventions:
 * - Windows: %APPDATA%/TidalEngine
 * - Linux: ~/.local/share/TidalEngine
 * - macOS: ~/Library/Application Support/TidalEngine
 *
 * @note All file I/O operations are performed synchronously
 * @see std::filesystem for underlying file operations
 */
class UserDataManager {
public:
    /**
     * @brief Default constructor - initializes platform-specific user data path
     */
    UserDataManager();

    /**
     * @brief Default destructor
     */
    ~UserDataManager() = default;

    /// @name Initialization
    /// @{
    /**
     * @brief Initialize user data directory structure and copy default assets
     *
     * Creates the user data directory structure if it doesn't exist and copies
     * default assets (texturepacks, settings) from the installation directory.
     * Must be called before using other methods.
     *
     * @return True if initialization succeeded, false on filesystem errors
     * @note This operation may involve significant file I/O for first-time setup
     */
    bool initialize();
    /// @}

    /// @name Directory Path Accessors
    /// @{
    /**
     * @brief Get the root user data directory path
     * @return Absolute path to user data directory (platform-specific)
     */
    std::string getUserDataPath() const { return m_userDataPath; }

    /**
     * @brief Get the world saves directory path
     * @return Absolute path to saves subdirectory
     */
    std::string getSavesPath() const { return m_userDataPath + "/saves"; }

    /**
     * @brief Get the texturepacks directory path
     * @return Absolute path to texturepacks subdirectory
     */
    std::string getTexturepacksPath() const { return m_userDataPath + "/texturepacks"; }

    /**
     * @brief Get the settings directory path
     * @return Absolute path to settings subdirectory
     */
    std::string getSettingsPath() const { return m_userDataPath + "/settings"; }

    /**
     * @brief Get the main configuration file path
     * @return Absolute path to config.json file
     */
    std::string getConfigPath() const { return m_userDataPath + "/config.json"; }
    /// @}

    /// @name Texturepack Management
    /// @{
    /**
     * @brief Get list of all available texturepack names
     *
     * Scans the texturepacks directory for valid texturepack folders.
     * A valid texturepack must contain the required asset structure.
     *
     * @return Vector of texturepack names (directory names)
     */
    std::vector<std::string> getAvailableTexturepacks() const;

    /**
     * @brief Get the full path to the currently active texturepack
     * @return Absolute path to active texturepack directory
     */
    std::string getActiveTexturepackPath() const;

    /**
     * @brief Set the active texturepack by name
     *
     * Changes the active texturepack if the specified pack exists.
     * The change persists across application restarts.
     *
     * @param name Name of the texturepack to activate
     * @return True if texturepack was found and activated successfully
     */
    bool setActiveTexturepack(const std::string& name);

    /**
     * @brief Check if a texturepack exists by name
     * @param name Name of texturepack to check
     * @return True if texturepack directory exists
     */
    bool texturepackExists(const std::string& name) const;
    /// @}

    /// @name Settings Management
    /// @{
    /**
     * @brief Load settings from configuration file
     *
     * Reads the config.json file and populates the internal settings map.
     * If the file doesn't exist, creates it with default values.
     *
     * @return True if settings were loaded successfully
     * @note Invalid JSON will cause the method to fail
     */
    bool loadSettings();

    /**
     * @brief Save current settings to configuration file
     *
     * Writes the internal settings map to config.json in JSON format.
     * Creates the file if it doesn't exist.
     *
     * @return True if settings were saved successfully
     */
    bool saveSettings();

    /**
     * @brief Get a setting value by key
     * @param key Setting key to retrieve
     * @param defaultValue Default value if key doesn't exist
     * @return Setting value or default if not found
     */
    std::string getSetting(const std::string& key, const std::string& defaultValue = "") const;

    /**
     * @brief Set a setting value by key
     *
     * Updates the setting in memory. Call saveSettings() to persist changes.
     *
     * @param key Setting key to set
     * @param value New setting value
     */
    void setSetting(const std::string& key, const std::string& value);
    /// @}

private:
    /// @name Internal State
    /// @{
    std::string m_userDataPath;                                 ///< Platform-specific user data directory path
    std::string m_activeTexturepack = "default";               ///< Name of currently active texturepack
    std::unordered_map<std::string, std::string> m_settings;   ///< In-memory settings cache (key-value pairs)
    /// @}

    /// @name Platform Integration
    /// @{
    /**
     * @brief Get platform-specific user data directory path
     *
     * Determines the appropriate user data directory based on the operating system:
     * - Windows: Uses %APPDATA%/TidalEngine
     * - Linux: Uses ~/.local/share/TidalEngine (XDG Base Directory Specification)
     * - macOS: Uses ~/Library/Application Support/TidalEngine
     *
     * @return Platform-appropriate user data directory path
     * @note Falls back to current directory if platform detection fails
     */
    std::string getPlatformUserDataPath() const;
    /// @}

    /// @name Directory Management
    /// @{
    /**
     * @brief Create the complete user data directory structure
     *
     * Creates all required subdirectories including saves, texturepacks,
     * and settings folders if they don't exist.
     *
     * @return True if directory structure was created successfully
     */
    bool createDirectoryStructure();

    /**
     * @brief Copy default assets from installation to user directory
     *
     * Copies default configuration files, settings, and other required
     * assets from the application installation directory to the user data directory.
     *
     * @return True if default assets were copied successfully
     */
    bool copyDefaultAssets();

    /**
     * @brief Copy default texturepack to user directory
     *
     * Ensures the default texturepack is available in the user's texturepacks
     * directory by copying it from the installation directory.
     *
     * @return True if default texturepack was copied successfully
     */
    bool copyDefaultTexturepack();
    /// @}

    /// @name File System Utilities
    /// @{
    /**
     * @brief Check if a directory exists
     * @param path Directory path to check
     * @return True if directory exists and is accessible
     */
    bool directoryExists(const std::string& path) const;

    /**
     * @brief Create a directory with parent directories as needed
     * @param path Directory path to create
     * @return True if directory was created or already exists
     */
    bool createDirectory(const std::string& path) const;

    /**
     * @brief Copy a single file from source to destination
     * @param src Source file path
     * @param dst Destination file path
     * @return True if file was copied successfully
     * @note Creates destination directory if it doesn't exist
     */
    bool copyFile(const std::string& src, const std::string& dst) const;

    /**
     * @brief Recursively copy a directory and all its contents
     * @param src Source directory path
     * @param dst Destination directory path
     * @return True if directory was copied successfully
     * @note Preserves directory structure and file attributes where possible
     */
    bool copyDirectoryRecursive(const std::string& src, const std::string& dst) const;
    /// @}
};