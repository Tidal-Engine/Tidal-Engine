#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <filesystem>

namespace engine {

/**
 * @brief Centralized resource management system for asset paths
 *
 * Provides a registry-based approach to managing asset paths, supporting
 * validation, multiple asset types, and flexible configuration. This makes
 * it easy to change asset locations, add mod support, or implement hot-reloading
 * in the future without changing call sites throughout the codebase.
 */
class ResourceManager {
public:
    /**
     * @brief Initialize the resource manager with a base asset directory
     * @param assetRoot Root directory for all assets (default: current directory)
     */
    static void init(const std::string& assetRoot = ".");

    /**
     * @brief Register a shader asset
     * @param name Logical name for the shader (e.g., "cube_vert")
     * @param relativePath Path relative to asset root (e.g., "shaders/cube_vert.spv")
     */
    static void registerShader(const std::string& name, const std::string& relativePath);

    /**
     * @brief Register a texture asset
     * @param name Logical name for the texture
     * @param relativePath Path relative to asset root
     */
    static void registerTexture(const std::string& name, const std::string& relativePath);

    /**
     * @brief Register a model asset
     * @param name Logical name for the model
     * @param relativePath Path relative to asset root
     */
    static void registerModel(const std::string& name, const std::string& relativePath);

    /**
     * @brief Get the absolute path to a shader
     * @param name Logical shader name
     * @return Absolute path to the shader file
     * @throws std::runtime_error if shader not found
     */
    static std::string getShaderPath(const std::string& name);

    /**
     * @brief Get the absolute path to a texture
     * @param name Logical texture name
     * @return Absolute path to the texture file
     * @throws std::runtime_error if texture not found
     */
    static std::string getTexturePath(const std::string& name);

    /**
     * @brief Get the absolute path to a model
     * @param name Logical model name
     * @return Absolute path to the model file
     * @throws std::runtime_error if model not found
     */
    static std::string getModelPath(const std::string& name);

    /**
     * @brief Check if a shader is registered
     * @param name Logical shader name
     * @return true if shader exists in registry
     */
    static bool hasShader(const std::string& name);

    /**
     * @brief Check if a texture is registered
     * @param name Logical texture name
     * @return true if texture exists in registry
     */
    static bool hasTexture(const std::string& name);

    /**
     * @brief Check if a model is registered
     * @param name Logical model name
     * @return true if model exists in registry
     */
    static bool hasModel(const std::string& name);

    /**
     * @brief Clear all registered assets
     */
    static void clear();

private:
    static std::string s_assetRoot;  // NOLINT(readability-identifier-naming)
    static std::unordered_map<std::string, std::string> s_shaders;  // NOLINT(readability-identifier-naming)
    static std::unordered_map<std::string, std::string> s_textures;  // NOLINT(readability-identifier-naming)
    static std::unordered_map<std::string, std::string> s_models;  // NOLINT(readability-identifier-naming)

    static std::string getPath(const std::unordered_map<std::string, std::string>& registry,
                               const std::string& name, const std::string& typeName);
};

} // namespace engine
