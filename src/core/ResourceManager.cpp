#include "core/ResourceManager.hpp"
#include "core/Logger.hpp"

#include <stdexcept>

namespace engine {

// Static member initialization
std::string ResourceManager::s_assetRoot = ".";
std::unordered_map<std::string, std::string> ResourceManager::s_shaders;
std::unordered_map<std::string, std::string> ResourceManager::s_textures;
std::unordered_map<std::string, std::string> ResourceManager::s_models;

void ResourceManager::init(const std::string& assetRoot) {
    s_assetRoot = assetRoot;
    LOG_INFO("ResourceManager initialized with asset root: {}", s_assetRoot);
}

void ResourceManager::registerShader(const std::string& name, const std::string& relativePath) {
    std::filesystem::path fullPath = std::filesystem::path(s_assetRoot) / relativePath;
    s_shaders[name] = fullPath.string();
    LOG_DEBUG("Registered shader '{}' -> {}", name, fullPath.string());
}

void ResourceManager::registerTexture(const std::string& name, const std::string& relativePath) {
    std::filesystem::path fullPath = std::filesystem::path(s_assetRoot) / relativePath;
    s_textures[name] = fullPath.string();
    LOG_DEBUG("Registered texture '{}' -> {}", name, fullPath.string());
}

void ResourceManager::registerModel(const std::string& name, const std::string& relativePath) {
    std::filesystem::path fullPath = std::filesystem::path(s_assetRoot) / relativePath;
    s_models[name] = fullPath.string();
    LOG_DEBUG("Registered model '{}' -> {}", name, fullPath.string());
}

std::string ResourceManager::getShaderPath(const std::string& name) {
    return getPath(s_shaders, name, "shader");
}

std::string ResourceManager::getTexturePath(const std::string& name) {
    return getPath(s_textures, name, "texture");
}

std::string ResourceManager::getModelPath(const std::string& name) {
    return getPath(s_models, name, "model");
}

bool ResourceManager::hasShader(const std::string& name) {
    return s_shaders.contains(name);
}

bool ResourceManager::hasTexture(const std::string& name) {
    return s_textures.contains(name);
}

bool ResourceManager::hasModel(const std::string& name) {
    return s_models.contains(name);
}

void ResourceManager::clear() {
    s_shaders.clear();
    s_textures.clear();
    s_models.clear();
    LOG_DEBUG("ResourceManager cleared all assets");
}

std::string ResourceManager::getPath(const std::unordered_map<std::string, std::string>& registry,
                                     const std::string& name, const std::string& typeName) {
    auto iter = registry.find(name);
    if (iter == registry.end()) {
        LOG_ERROR("Failed to find {} '{}' in resource registry", typeName, name);
        throw std::runtime_error("Resource not found: " + typeName + " '" + name + "'");
    }

    const std::string& path = iter->second;

    // Validate file exists
    if (!std::filesystem::exists(path)) {
        LOG_WARN("{} '{}' registered but file not found at: {}", typeName, name, path);
    }

    return path;
}

} // namespace engine
