#include "gm/save/SaveDiff.hpp"

#include <algorithm>
#include <array>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "gm/core/Logger.hpp"

namespace gm::save {

namespace {

bool TerrainEqual(const nlohmann::json& lhs, const nlohmann::json& rhs) {
    if (!lhs.is_object() || !rhs.is_object()) {
        return false;
    }
    static constexpr std::array<std::string_view, 8> kKeys{
        "resolution", "size", "minHeight", "maxHeight",
        "textureTiling", "baseTextureGuid", "activePaintLayer", "heights"
    };
    for (auto key : kKeys) {
        if (lhs.contains(key) != rhs.contains(key)) {
            return false;
        }
        if (!lhs.contains(key)) {
            continue;
        }
        if (lhs[key] != rhs[key]) {
            return false;
        }
    }

    if (lhs.contains("paintLayers") || rhs.contains("paintLayers")) {
        return lhs.value("paintLayers", nlohmann::json::array())
             == rhs.value("paintLayers", nlohmann::json::array());
    }

    return true;
}

using QuestStateMap = std::unordered_map<std::string, nlohmann::json>;

QuestStateMap ExtractQuestStates(const nlohmann::json& save) {
    QuestStateMap quests;
    if (!save.contains("gameObjects") || !save["gameObjects"].is_array()) {
        return quests;
    }

    for (const auto& object : save["gameObjects"]) {
        if (!object.is_object()) continue;
        const std::string name = object.value("name", std::string());
        if (name.empty()) continue;

        if (!object.contains("components") || !object["components"].is_array()) continue;
        for (const auto& component : object["components"]) {
            if (!component.is_object()) continue;
            const std::string compName = component.value("name", std::string());
            if (compName != "QuestTriggerComponent") continue;
            quests[name] = component;
        }
    }
    return quests;
}

std::vector<std::string> DiffQuestStates(const QuestStateMap& previous, const QuestStateMap& next) {
    std::vector<std::string> changes;
    std::unordered_map<std::string, const nlohmann::json*> combined;
    for (const auto& [name, state] : previous) {
        combined.emplace(name, &state);
    }
    for (const auto& [name, state] : next) {
        const auto it = combined.find(name);
        if (it == combined.end()) {
            changes.push_back("Quest added: " + name);
        } else if (*it->second != state) {
            changes.push_back("Quest updated: " + name);
            combined.erase(it);
        } else {
            combined.erase(it);
        }
    }
    for (const auto& [name, _] : combined) {
        changes.push_back("Quest removed: " + name);
    }
    return changes;
}

} // namespace

SaveDiffSummary ComputeSaveDiff(const nlohmann::json& previous, const nlohmann::json& next) {
    SaveDiffSummary summary;

    if (previous.contains("version") && next.contains("version")) {
        summary.versionChanged = previous["version"] != next["version"];
    }

    const bool prevHasTerrain = previous.contains("terrain");
    const bool nextHasTerrain = next.contains("terrain");

    if (prevHasTerrain || nextHasTerrain) {
        if (!prevHasTerrain || !nextHasTerrain) {
            summary.terrainChanged = true;
        } else if (!TerrainEqual(previous["terrain"], next["terrain"])) {
            summary.terrainChanged = true;
            try {
                summary.terrainDiff = nlohmann::json::diff(previous["terrain"], next["terrain"]);
            } catch (const std::exception& ex) {
                gm::core::Logger::Warning("[SaveDiff] Failed to compute terrain diff: {}", ex.what());
            }
        }
    }

    const auto prevQuests = ExtractQuestStates(previous);
    const auto nextQuests = ExtractQuestStates(next);
    summary.questChanges = DiffQuestStates(prevQuests, nextQuests);
    summary.questStateChanged = !summary.questChanges.empty();

    return summary;
}

void MergeTerrainIfMissing(nlohmann::json& target, const nlohmann::json& fallback) {
    if (target.contains("terrain")) {
        return;
    }
    if (!fallback.contains("terrain")) {
        return;
    }
    target["terrain"] = fallback["terrain"];
}

} // namespace gm::save


