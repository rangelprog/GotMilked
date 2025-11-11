#pragma once

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace gm::save {

struct SaveMetadata {
    std::string slotName;
    std::filesystem::path filePath;
    std::chrono::system_clock::time_point timestamp{};
    std::size_t fileSizeBytes = 0;
};

struct SaveGameData {
    std::string version = "0.1.0";
    std::string sceneName;
    glm::vec3 cameraPosition{0.0f};
    glm::vec3 cameraForward{0.0f, 0.0f, -1.0f};
    float cameraFov = 60.0f;
    double worldTime = 0.0;
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
    SaveLoadResult QuickLoad(SaveGameData& outData) const;

    SaveLoadResult SaveToSlot(const std::string& slotName, const SaveGameData& data);
    SaveLoadResult LoadFromSlot(const std::string& slotName, SaveGameData& outData) const;

    SaveList EnumerateSaves() const;

private:
    std::filesystem::path SlotPath(const std::string& slotName) const;
    std::filesystem::path QuickSavePath() const;

    std::filesystem::path m_saveDirectory;
};

} // namespace gm::save

