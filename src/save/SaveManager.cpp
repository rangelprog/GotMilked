#include "gm/save/SaveManager.hpp"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <sstream>
#include <iomanip>
#include <ctime>

#include <nlohmann/json.hpp>

#include "gm/core/Logger.hpp"
#include "gm/save/SaveVersion.hpp"

namespace gm::save {

namespace {

constexpr const char* kQuickSaveFilename = "quick_save.json";
constexpr const char* kSaveExtension = ".json";

nlohmann::json ToJson(const SaveGameData& data) {
    nlohmann::json json = {
        {"version", SaveVersionToJson(data.version)},
        {"sceneName", data.sceneName},
        {"camera", {
            {"position", {data.cameraPosition.x, data.cameraPosition.y, data.cameraPosition.z}},
            {"forward", {data.cameraForward.x, data.cameraForward.y, data.cameraForward.z}},
            {"fov", data.cameraFov}
        }},
        {"worldTime", data.worldTime}
    };

    if (data.terrainResolution > 0 && !data.terrainHeights.empty()) {
        nlohmann::json terrainJson = {
            {"resolution", data.terrainResolution},
            {"size", data.terrainSize},
            {"minHeight", data.terrainMinHeight},
            {"maxHeight", data.terrainMaxHeight},
            {"heights", data.terrainHeights},
            {"textureTiling", data.terrainTextureTiling},
            {"baseTextureGuid", data.terrainBaseTextureGuid},
            {"activePaintLayer", data.terrainActivePaintLayer}
        };

        nlohmann::json paintLayers = nlohmann::json::array();
        for (const auto& layer : data.terrainPaintLayers) {
            nlohmann::json layerJson;
            layerJson["guid"] = layer.guid;
            layerJson["enabled"] = layer.enabled;
            layerJson["weights"] = layer.weights;
            paintLayers.push_back(std::move(layerJson));
        }
        terrainJson["paintLayers"] = std::move(paintLayers);

        json["terrain"] = std::move(terrainJson);
    }

    return json;
}

std::optional<SaveGameData> FromJson(const nlohmann::json& json, std::string& outError) {
    SaveGameData data;
    try {
        if (json.contains("version")) {
            data.version = ParseSaveVersion(json["version"]);
        }
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

        if (json.contains("terrain")) {
            const auto& terrain = json["terrain"];
            data.terrainResolution = terrain.value("resolution", data.terrainResolution);
            data.terrainSize = terrain.value("size", data.terrainSize);
            data.terrainMinHeight = terrain.value("minHeight", data.terrainMinHeight);
            data.terrainMaxHeight = terrain.value("maxHeight", data.terrainMaxHeight);
            data.terrainTextureTiling = terrain.value("textureTiling", data.terrainTextureTiling);
            data.terrainBaseTextureGuid = terrain.value("baseTextureGuid", data.terrainBaseTextureGuid);
            data.terrainActivePaintLayer = terrain.value("activePaintLayer", data.terrainActivePaintLayer);

            if (terrain.contains("heights") && terrain["heights"].is_array()) {
                const auto& heights = terrain["heights"];
                data.terrainHeights.clear();
                data.terrainHeights.reserve(heights.size());
                for (const auto& value : heights) {
                    data.terrainHeights.push_back(value.get<float>());
                }
            }

            data.terrainPaintLayers.clear();
            if (terrain.contains("paintLayers") && terrain["paintLayers"].is_array()) {
                const auto& layers = terrain["paintLayers"];
                data.terrainPaintLayers.reserve(layers.size());
                for (const auto& layerJson : layers) {
                    SaveGameData::TerrainPaintLayerData layerData;
                    layerData.guid = layerJson.value("guid", std::string());
                    layerData.enabled = layerJson.value("enabled", true);
                    if (layerJson.contains("weights") && layerJson["weights"].is_array()) {
                        const auto& weights = layerJson["weights"];
                        layerData.weights.reserve(weights.size());
                        for (const auto& weight : weights) {
                            layerData.weights.push_back(weight.get<float>());
                        }
                    }
                    data.terrainPaintLayers.push_back(std::move(layerData));
                }
            }
        }
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
            gm::core::Logger::Error("[SaveManager] Failed to create save directory: {} ({})",
                                    m_saveDirectory.string(),
                                    ec.message());
        }
    }
}

SaveLoadResult SaveManager::QuickSave(const SaveGameData& data) {
    // Generate unique quick save filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmLocal{};
#ifdef _WIN32
    localtime_s(&tmLocal, &timeT);
#else
    localtime_r(&timeT, &tmLocal);
#endif
    
    std::ostringstream oss;
    oss << "quick_save_"
        << std::put_time(&tmLocal, "%Y%m%d_%H%M%S")
        << ".json";
    
    std::string slotName = oss.str();
    return SaveToSlot(slotName, data);
}

SaveLoadResult SaveManager::QuickSaveWithJson(const nlohmann::json& json) {
    // Generate unique quick save filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto timeT = std::chrono::system_clock::to_time_t(now);
    std::tm tmLocal{};
#ifdef _WIN32
    localtime_s(&tmLocal, &timeT);
#else
    localtime_r(&timeT, &tmLocal);
#endif
    
    std::ostringstream oss;
    oss << "quick_save_"
        << std::put_time(&tmLocal, "%Y%m%d_%H%M%S")
        << ".json";
    
    std::string slotName = oss.str();
    std::filesystem::path path = m_saveDirectory / slotName;
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

    nlohmann::json output = json;
    if (!output.contains("version")) {
        output["version"] = SaveVersionToJson(SaveVersion::Current());
    }

    out << output.dump(2);
    if (!out.good()) {
        result.message = "Failed while writing save data";
        return result;
    }

    gm::core::Logger::Info("[SaveManager] Saved quick save '{}' to {}",
                           slotName, path.string());
    result.success = true;
    return result;
}

SaveLoadResult SaveManager::QuickLoad(SaveGameData& outData) const {
    return LoadFromSlot("quick", outData);
}

SaveLoadResult SaveManager::QuickLoadWithJson(nlohmann::json& outJson) const {
    return LoadMostRecentQuickSaveJson(outJson);
}

SaveLoadResult SaveManager::SaveToSlot(const std::string& slotName, const SaveGameData& data) {
    // Check if slotName starts with "quick_save" to use quick save directory
    std::filesystem::path path;
    if (slotName.find("quick_save") == 0) {
        path = m_saveDirectory / slotName;
    } else {
        path = SlotPath(slotName);
    }
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

    gm::core::Logger::Info("[SaveManager] Saved slot '{}' to {}",
                           slotName, path.string());
    result.success = true;
    return result;
}

SaveLoadResult SaveManager::LoadFromSlot(const std::string& slotName, SaveGameData& outData) const {
    // Check if slotName starts with "quick_save" to use quick save directory
    std::filesystem::path path;
    if (slotName == "quick") {
        // For "quick", load the most recent quick save
        path = GetMostRecentQuickSave();
        if (path.empty()) {
            SaveLoadResult result;
            result.message = "No quick save found";
            return result;
        }
    } else if (slotName.find("quick_save") == 0) {
        path = m_saveDirectory / slotName;
    } else {
        path = SlotPath(slotName);
    }
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

std::filesystem::path SaveManager::GetMostRecentQuickSave() const {
    std::filesystem::path mostRecent;
    std::chrono::system_clock::time_point mostRecentTime{};
    
    std::error_code ec;
    if (!std::filesystem::exists(m_saveDirectory, ec)) {
        return mostRecent;
    }
    
    for (const auto& entry : std::filesystem::directory_iterator(m_saveDirectory, ec)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        if (filename.find("quick_save_") == 0 && entry.path().extension() == kSaveExtension) {
            auto fileTime = entry.last_write_time();
            auto sysTime = FromFileTime(fileTime);
            
            if (mostRecent.empty() || sysTime > mostRecentTime) {
                mostRecent = entry.path();
                mostRecentTime = sysTime;
            }
        }
    }
    
    return mostRecent;
}

SaveLoadResult SaveManager::LoadMostRecentQuickSaveJson(nlohmann::json& outJson) const {
    std::filesystem::path path = GetMostRecentQuickSave();
    SaveLoadResult result;

    if (path.empty()) {
        result.message = "No quick save found";
        return result;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        result.message = "Save file not found: " + path.string();
        return result;
    }

    try {
        in >> outJson;
    } catch (const nlohmann::json::exception& ex) {
        result.message = std::string("Failed to parse save: ") + ex.what();
        return result;
    }

    if (!outJson.contains("version")) {
        gm::core::Logger::Warning("[SaveManager] Quick save '{}' missing version; assuming current", path.string());
        outJson["version"] = SaveVersionToJson(SaveVersion::Current());
    }

    result.success = true;
    return result;
}

} // namespace gm::save

