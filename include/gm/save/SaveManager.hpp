#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <nlohmann/json_fwd.hpp>

#include "gm/save/SaveVersion.hpp"

namespace gm::save {

struct SaveMetadata {
    std::string slotName;
    std::filesystem::path filePath;
    std::chrono::system_clock::time_point timestamp{};
    std::size_t fileSizeBytes = 0;
};

struct SaveGameData {
    SaveVersion version = SaveVersion::Current();
    std::string sceneName;
    glm::vec3 cameraPosition{0.0f};
    glm::vec3 cameraForward{0.0f, 0.0f, -1.0f};
    float cameraFov = 60.0f;
    double worldTime = 0.0;

    int terrainResolution = 0;
    float terrainSize = 0.0f;
    float terrainMinHeight = 0.0f;
    float terrainMaxHeight = 0.0f;
    std::vector<float> terrainHeights;
    float terrainTextureTiling = 1.0f;
    std::string terrainBaseTextureGuid;
    int terrainActivePaintLayer = 0;
    struct TerrainPaintLayerData {
        std::string guid;
        bool enabled = true;
        std::vector<float> weights;
    };
    std::vector<TerrainPaintLayerData> terrainPaintLayers;
};

struct SaveLoadResult {
    bool success = false;
    std::string message;
};

using SaveList = std::vector<SaveMetadata>;

class SaveManager {
public:
    explicit SaveManager(std::filesystem::path saveDirectory);

    const std::filesystem::path& GetSaveDirectory() const { return m_saveDirectory; }

    SaveLoadResult QuickSave(const SaveGameData& data);
    SaveLoadResult QuickSaveWithJson(const nlohmann::json& json);
    SaveLoadResult QuickLoad(SaveGameData& outData) const;
    SaveLoadResult QuickLoadWithJson(nlohmann::json& outJson) const;
    SaveLoadResult LoadMostRecentQuickSaveJson(nlohmann::json& outJson) const;

    SaveLoadResult SaveToSlot(const std::string& slotName, const SaveGameData& data);
    SaveLoadResult LoadFromSlot(const std::string& slotName, SaveGameData& outData) const;

    SaveList EnumerateSaves() const;

private:
    std::filesystem::path SlotPath(const std::string& slotName) const;
    std::filesystem::path QuickSavePath() const;
    std::filesystem::path GetMostRecentQuickSave() const;

    std::filesystem::path m_saveDirectory;
};

} // namespace gm::save

