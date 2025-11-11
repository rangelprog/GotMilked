#include "gm/save/SaveManager.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

#include "gm/core/Logger.hpp"

namespace gm::save {

namespace {

constexpr const char* kQuickSaveFilename = "quick_save.json";
constexpr const char* kSaveExtension = ".json";

nlohmann::json ToJson(const SaveGameData& data) {
    return {
        {"version", data.version},
        {"sceneName", data.sceneName},
        {"camera", {
            {"position", {data.cameraPosition.x, data.cameraPosition.y, data.cameraPosition.z}},
            {"forward", {data.cameraForward.x, data.cameraForward.y, data.cameraForward.z}},
            {"fov", data.cameraFov}
        }},
        {"worldTime", data.worldTime}
    };
}

std::optional<SaveGameData> FromJson(const nlohmann::json& json, std::string& outError) {
    SaveGameData data;
    try {
        data.version = json.value("version", data.version);
        data.sceneName = json.value("sceneName", data.sceneName);

        if (json.contains("camera")) {
            const auto& camera = json["camera"];
            if (camera.contains("position")) {
                auto pos = camera["position"];
                if (pos.is_array() && pos.size() == 3) {
                    data.cameraPosition = glm::vec3(
                        pos[0].get<float>(),
                        pos[1].get<float>(),
                        pos[2].get<float>());
                }
            }
            if (camera.contains("forward")) {
                auto fwd = camera["forward"];
                if (fwd.is_array() && fwd.size() == 3) {
                    data.cameraForward = glm::vec3(
                        fwd[0].get<float>(),
                        fwd[1].get<float>(),
                        fwd[2].get<float>());
                }
            }
            data.cameraFov = camera.value("fov", data.cameraFov);
        }

        data.worldTime = json.value("worldTime", data.worldTime);
    } catch (const nlohmann::json::exception& ex) {
        outError = ex.what();
        return std::nullopt;
    }
    return data;
}

std::chrono::system_clock::time_point FromFileTime(std::filesystem::file_time_type ft) {
    return std::chrono::clock_cast<std::chrono::system_clock>(ft);
}

} // namespace

SaveManager::SaveManager(std::filesystem::path saveDirectory)
    : m_saveDirectory(std::move(saveDirectory)) {
    std::error_code ec;
    if (!std::filesystem::exists(m_saveDirectory, ec)) {
        if (!std::filesystem::create_directories(m_saveDirectory, ec)) {
            gm::core::Logger::Error("[SaveManager] Failed to create save directory: %s (%s)",
                                    m_saveDirectory.string().c_str(),
                                    ec.message().c_str());
        }
    }
}

SaveLoadResult SaveManager::QuickSave(const SaveGameData& data) {
    return SaveToSlot("quick", data);
}

SaveLoadResult SaveManager::QuickLoad(SaveGameData& outData) const {
    return LoadFromSlot("quick", outData);
}

SaveLoadResult SaveManager::SaveToSlot(const std::string& slotName, const SaveGameData& data) {
    const auto path = slotName == "quick" ? QuickSavePath() : SlotPath(slotName);
    SaveLoadResult result;

    std::error_code ec;
    if (!std::filesystem::exists(m_saveDirectory, ec)) {
        if (!std::filesystem::create_directories(m_saveDirectory, ec)) {
            result.message = "Unable to create save directory: " + ec.message();
            return result;
        }
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        result.message = "Failed to open save file for writing";
        return result;
    }

    nlohmann::json json = ToJson(data);
    out << json.dump(2);
    if (!out.good()) {
        result.message = "Failed while writing save data";
        return result;
    }

    gm::core::Logger::Info("[SaveManager] Saved slot '%s' to %s",
                           slotName.c_str(), path.string().c_str());
    result.success = true;
    return result;
}

SaveLoadResult SaveManager::LoadFromSlot(const std::string& slotName, SaveGameData& outData) const {
    const auto path = slotName == "quick" ? QuickSavePath() : SlotPath(slotName);
    SaveLoadResult result;

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.message = "Save file not found: " + path.string();
        return result;
    }

    nlohmann::json json;
    try {
        in >> json;
    } catch (const nlohmann::json::exception& ex) {
        result.message = std::string("Failed to parse save: ") + ex.what();
        return result;
    }

    std::string error;
    auto maybeData = FromJson(json, error);
    if (!maybeData) {
        result.message = "Invalid save data: " + error;
        return result;
    }

    outData = *maybeData;
    result.success = true;
    return result;
}

SaveList SaveManager::EnumerateSaves() const {
    SaveList saves;
    std::error_code ec;
    if (!std::filesystem::exists(m_saveDirectory, ec)) {
        return saves;
    }

    for (const auto& entry : std::filesystem::directory_iterator(m_saveDirectory, ec)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != kSaveExtension) continue;

        SaveMetadata meta;
        meta.filePath = entry.path();
        meta.slotName = meta.filePath.stem().string();
        meta.fileSizeBytes = entry.file_size();
        meta.timestamp = FromFileTime(entry.last_write_time());
        saves.push_back(std::move(meta));
    }

    std::sort(saves.begin(), saves.end(), [](const SaveMetadata& a, const SaveMetadata& b) {
        return a.timestamp > b.timestamp;
    });
    return saves;
}

std::filesystem::path SaveManager::SlotPath(const std::string& slotName) const {
    return m_saveDirectory / (slotName + kSaveExtension);
}

std::filesystem::path SaveManager::QuickSavePath() const {
    return m_saveDirectory / kQuickSaveFilename;
}

} // namespace gm::save

