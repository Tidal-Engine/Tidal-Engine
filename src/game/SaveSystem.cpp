#include "SaveSystem.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <chrono>

#ifdef HEADLESS_SERVER
#define Chunk_SIZE CHUNK_SIZE
#else
#define Chunk_SIZE Chunk::SIZE
#endif

bool ChunkData::isEmpty() const {
    for (int x = 0; x < Chunk_SIZE; x++) {
        for (int y = 0; y < Chunk_SIZE; y++) {
            for (int z = 0; z < Chunk_SIZE; z++) {
                if (voxels[x][y][z] != BlockType::AIR) {
                    return false;
                }
            }
        }
    }
    return true;
}

SaveSystem::SaveSystem(const std::string& savesDirectory)
    : m_savesDirectory(savesDirectory) {
    createDirectory(m_savesDirectory);
}

bool SaveSystem::createWorld(const std::string& worldName) {
    if (worldName.empty()) return false;

    std::string worldPath = m_savesDirectory + "/" + worldName;
    if (!createDirectory(worldPath)) return false;
    if (!createDirectory(worldPath + "/chunks")) return false;

    // Create default world data
    WorldData worldData;
    worldData.name = worldName;
    worldData.creationTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    worldData.lastPlayTime = worldData.creationTime;
    worldData.seed = 12345; // Default seed
    worldData.activeTexturepack = "default";

    m_currentWorld = worldName;
    return saveWorldData(worldData);
}

bool SaveSystem::loadWorld(const std::string& worldName) {
    if (worldName.empty()) return false;

    std::string worldPath = m_savesDirectory + "/" + worldName;
    if (!std::filesystem::exists(worldPath)) return false;

    m_currentWorld = worldName;

    // Update last play time
    WorldData worldData;
    if (loadWorldData(worldData)) {
        worldData.lastPlayTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        saveWorldData(worldData);
    }

    return true;
}

bool SaveSystem::saveWorld() {
    if (m_currentWorld.empty()) return false;

    // Update last play time
    WorldData worldData;
    if (loadWorldData(worldData)) {
        worldData.lastPlayTime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return saveWorldData(worldData);
    }

    return false;
}

bool SaveSystem::deleteWorld(const std::string& worldName) {
    if (worldName.empty()) return false;

    std::string worldPath = m_savesDirectory + "/" + worldName;
    try {
        std::filesystem::remove_all(worldPath);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to delete world: " << e.what() << std::endl;
        return false;
    }
}

std::vector<std::string> SaveSystem::getAvailableWorlds() const {
    std::vector<std::string> worlds;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(m_savesDirectory)) {
            if (entry.is_directory()) {
                std::string worldName = entry.path().filename().string();
                // Check if it has a level.dat file
                if (std::filesystem::exists(entry.path() / "level.dat")) {
                    worlds.push_back(worldName);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to list worlds: " << e.what() << std::endl;
    }

    return worlds;
}

bool SaveSystem::savePlayerData(const PlayerData& playerData) {
    if (m_currentWorld.empty()) return false;

    std::vector<uint8_t> buffer;
    serializePlayerData(playerData, buffer);

    std::string path = getWorldPath() + "/player.dat";
    return writeDataToFile(path, buffer.data(), buffer.size());
}

bool SaveSystem::loadPlayerData(PlayerData& playerData) {
    if (m_currentWorld.empty()) return false;

    std::string path = getWorldPath() + "/player.dat";
    if (!fileExists(path)) return false;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();

    return deserializePlayerData(buffer, playerData);
}

bool SaveSystem::saveChunk(const ChunkData& chunkData) {
    if (m_currentWorld.empty()) return false;

    // Don't save empty chunks
    if (chunkData.isEmpty()) {
        // Delete existing chunk file if it exists
        std::string path = getChunkPath(chunkData.position);
        if (fileExists(path)) {
            std::filesystem::remove(path);
        }
        return true;
    }

    std::vector<uint8_t> buffer;
    serializeChunkData(chunkData, buffer);

    std::string path = getChunkPath(chunkData.position);
    return writeDataToFile(path, buffer.data(), buffer.size());
}

bool SaveSystem::loadChunk(const ChunkPos& position, ChunkData& chunkData) {
    if (m_currentWorld.empty()) return false;

    std::string path = getChunkPath(position);
    if (!fileExists(path)) return false;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();

    return deserializeChunkData(buffer, chunkData);
}

bool SaveSystem::chunkExists(const ChunkPos& position) const {
    if (m_currentWorld.empty()) return false;
    return fileExists(getChunkPath(position));
}

bool SaveSystem::saveWorldData(const WorldData& worldData) {
    if (m_currentWorld.empty()) return false;

    std::vector<uint8_t> buffer;
    serializeWorldData(worldData, buffer);

    std::string path = getWorldPath() + "/level.dat";
    return writeDataToFile(path, buffer.data(), buffer.size());
}

bool SaveSystem::loadWorldData(WorldData& worldData) {
    if (m_currentWorld.empty()) return false;

    std::string path = getWorldPath() + "/level.dat";
    if (!fileExists(path)) return false;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();

    return deserializeWorldData(buffer, worldData);
}

std::string SaveSystem::getWorldPath() const {
    return m_savesDirectory + "/" + m_currentWorld;
}

std::string SaveSystem::getChunkPath(const ChunkPos& position) const {
    return getWorldPath() + "/chunks/chunk_" +
           std::to_string(position.x) + "_" +
           std::to_string(position.y) + "_" +
           std::to_string(position.z) + ".dat";
}

void SaveSystem::savePlayerFromCamera(const Camera& camera, int selectedHotbarSlot) {
#ifndef HEADLESS_SERVER
    PlayerData playerData;
    playerData.position = camera.getPosition();
    playerData.yaw = camera.Yaw;
    playerData.pitch = camera.Pitch;
    playerData.movementSpeed = camera.MovementSpeed;
    playerData.mouseSensitivity = camera.MouseSensitivity;
    playerData.zoom = camera.Zoom;
    playerData.selectedHotbarSlot = selectedHotbarSlot;

    savePlayerData(playerData);
#else
    (void)camera;
    (void)selectedHotbarSlot;
#endif
}

void SaveSystem::loadPlayerToCamera(Camera& camera, int& selectedHotbarSlot) {
#ifndef HEADLESS_SERVER
    PlayerData playerData;
    if (loadPlayerData(playerData)) {
        camera.Position = playerData.position;
        camera.Yaw = playerData.yaw;
        camera.Pitch = playerData.pitch;
        camera.MovementSpeed = playerData.movementSpeed;
        camera.MouseSensitivity = playerData.mouseSensitivity;
        camera.Zoom = playerData.zoom;
        selectedHotbarSlot = playerData.selectedHotbarSlot;

        // Update camera vectors after setting position/rotation
        camera.processMouseMovement(0, 0); // This will call updateCameraVectors internally
    }
#else
    (void)camera;
    (void)selectedHotbarSlot;
#endif
}

void SaveSystem::saveChunkFromChunk(const Chunk& chunk) {
#ifndef HEADLESS_SERVER
    ChunkData chunkData;
    chunkData.position = chunk.getPosition();

    // Copy voxel data from chunk
    for (int x = 0; x < Chunk_SIZE; x++) {
        for (int y = 0; y < Chunk_SIZE; y++) {
            for (int z = 0; z < Chunk_SIZE; z++) {
                chunkData.voxels[x][y][z] = chunk.getVoxelType(x, y, z);
            }
        }
    }

    saveChunk(chunkData);
#else
    (void)chunk;
#endif
}

bool SaveSystem::loadChunkToChunk(Chunk& chunk) {
#ifndef HEADLESS_SERVER
    ChunkData chunkData;
    if (loadChunk(chunk.getPosition(), chunkData)) {
        // Load voxel data into chunk
        for (int x = 0; x < Chunk_SIZE; x++) {
            for (int y = 0; y < Chunk_SIZE; y++) {
                for (int z = 0; z < Chunk_SIZE; z++) {
                    chunk.setVoxel(x, y, z, chunkData.voxels[x][y][z]);
                }
            }
        }
        return true;
    }
    return false;
#else
    (void)chunk;
    return false;
#endif
}

// Private helper methods
bool SaveSystem::createDirectory(const std::string& path) const {
    try {
        std::filesystem::create_directories(path);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to create directory: " << path << " - " << e.what() << std::endl;
        return false;
    }
}

bool SaveSystem::fileExists(const std::string& path) const {
    return std::filesystem::exists(path);
}

bool SaveSystem::writeDataToFile(const std::string& path, const void* data, size_t size) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for writing: " << path << std::endl;
        return false;
    }

    file.write(static_cast<const char*>(data), size);
    file.close();
    return true;
}

bool SaveSystem::readDataFromFile(const std::string& path, void* data, size_t size) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open file for reading: " << path << std::endl;
        return false;
    }

    file.read(static_cast<char*>(data), size);
    file.close();
    return file.gcount() == static_cast<std::streamsize>(size);
}

// Serialization methods
void SaveSystem::serializePlayerData(const PlayerData& data, std::vector<uint8_t>& buffer) {
    buffer.clear();
    buffer.reserve(sizeof(uint32_t) + sizeof(PlayerData));

    // Write version
    uint32_t version = SAVE_VERSION;
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&version),
                  reinterpret_cast<const uint8_t*>(&version) + sizeof(version));

    // Write player data
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&data),
                  reinterpret_cast<const uint8_t*>(&data) + sizeof(data));
}

bool SaveSystem::deserializePlayerData(const std::vector<uint8_t>& buffer, PlayerData& data) {
    if (buffer.size() < sizeof(uint32_t) + sizeof(PlayerData)) return false;

    // Read version
    uint32_t version;
    std::memcpy(&version, buffer.data(), sizeof(version));
    if (version != SAVE_VERSION) return false;

    // Read player data
    std::memcpy(&data, buffer.data() + sizeof(version), sizeof(data));
    return true;
}

void SaveSystem::serializeChunkData(const ChunkData& data, std::vector<uint8_t>& buffer) {
    buffer.clear();
    buffer.reserve(sizeof(uint32_t) + sizeof(ChunkData));

    // Write version
    uint32_t version = SAVE_VERSION;
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&version),
                  reinterpret_cast<const uint8_t*>(&version) + sizeof(version));

    // Write chunk data
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&data),
                  reinterpret_cast<const uint8_t*>(&data) + sizeof(data));
}

bool SaveSystem::deserializeChunkData(const std::vector<uint8_t>& buffer, ChunkData& data) {
    if (buffer.size() < sizeof(uint32_t) + sizeof(ChunkData)) return false;

    // Read version
    uint32_t version;
    std::memcpy(&version, buffer.data(), sizeof(version));
    if (version != SAVE_VERSION) return false;

    // Read chunk data
    std::memcpy(&data, buffer.data() + sizeof(version), sizeof(data));
    return true;
}

void SaveSystem::serializeWorldData(const WorldData& data, std::vector<uint8_t>& buffer) {
    buffer.clear();

    // Write version
    uint32_t version = SAVE_VERSION;
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&version),
                  reinterpret_cast<const uint8_t*>(&version) + sizeof(version));

    // Write string lengths and data
    uint32_t nameLen = data.name.length();
    uint32_t texturepackLen = data.activeTexturepack.length();

    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&nameLen),
                  reinterpret_cast<const uint8_t*>(&nameLen) + sizeof(nameLen));
    buffer.insert(buffer.end(), data.name.begin(), data.name.end());

    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&data.creationTime),
                  reinterpret_cast<const uint8_t*>(&data.creationTime) + sizeof(data.creationTime));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&data.lastPlayTime),
                  reinterpret_cast<const uint8_t*>(&data.lastPlayTime) + sizeof(data.lastPlayTime));
    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&data.seed),
                  reinterpret_cast<const uint8_t*>(&data.seed) + sizeof(data.seed));

    buffer.insert(buffer.end(), reinterpret_cast<const uint8_t*>(&texturepackLen),
                  reinterpret_cast<const uint8_t*>(&texturepackLen) + sizeof(texturepackLen));
    buffer.insert(buffer.end(), data.activeTexturepack.begin(), data.activeTexturepack.end());
}

bool SaveSystem::deserializeWorldData(const std::vector<uint8_t>& buffer, WorldData& data) {
    if (buffer.size() < sizeof(uint32_t)) return false;

    size_t offset = 0;

    // Read version
    uint32_t version;
    std::memcpy(&version, buffer.data() + offset, sizeof(version));
    offset += sizeof(version);
    if (version != SAVE_VERSION) return false;

    // Read name
    if (offset + sizeof(uint32_t) > buffer.size()) return false;
    uint32_t nameLen;
    std::memcpy(&nameLen, buffer.data() + offset, sizeof(nameLen));
    offset += sizeof(nameLen);

    if (offset + nameLen > buffer.size()) return false;
    data.name = std::string(buffer.begin() + offset, buffer.begin() + offset + nameLen);
    offset += nameLen;

    // Read times and seed
    if (offset + sizeof(data.creationTime) + sizeof(data.lastPlayTime) + sizeof(data.seed) > buffer.size()) return false;
    std::memcpy(&data.creationTime, buffer.data() + offset, sizeof(data.creationTime));
    offset += sizeof(data.creationTime);
    std::memcpy(&data.lastPlayTime, buffer.data() + offset, sizeof(data.lastPlayTime));
    offset += sizeof(data.lastPlayTime);
    std::memcpy(&data.seed, buffer.data() + offset, sizeof(data.seed));
    offset += sizeof(data.seed);

    // Read texturepack
    if (offset + sizeof(uint32_t) > buffer.size()) return false;
    uint32_t texturepackLen;
    std::memcpy(&texturepackLen, buffer.data() + offset, sizeof(texturepackLen));
    offset += sizeof(texturepackLen);

    if (offset + texturepackLen > buffer.size()) return false;
    data.activeTexturepack = std::string(buffer.begin() + offset, buffer.begin() + offset + texturepackLen);

    return true;
}