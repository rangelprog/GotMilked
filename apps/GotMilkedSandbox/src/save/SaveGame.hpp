#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

#include <glm/vec3.hpp>

namespace sandbox::save {

struct SaveMetadata {
    std::string slotName;
    std::filesystem::path filePath;
    std::chrono::system_clock::time_point timestamp {};
    std::size_t fileSizeBytes = 0;
};

struct SaveGameData {
    std::string version = "0.1.0";
    std::string sceneName;
    glm::vec3 cameraPosition {0.0f};
    glm::vec3 cameraForward {0.0f, 0.0f, -1.0f};
    float cameraFov = 60.0f;
    double worldTime = 0.0;
};

struct SaveLoadResult {
    bool success = false;
    std::string message;
};

using SaveList = std::vector<SaveMetadata>;

} // namespace sandbox::save

