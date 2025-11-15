#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace gm::save {

struct SaveDiffSummary {
    bool versionChanged = false;
    bool terrainChanged = false;
    bool questStateChanged = false;
    bool dialogueStateChanged = false;
    std::vector<std::string> questChanges;
    std::vector<std::string> dialogueChanges;
    nlohmann::json terrainDiff;
};

SaveDiffSummary ComputeSaveDiff(const nlohmann::json& previous, const nlohmann::json& next);
void MergeTerrainIfMissing(nlohmann::json& target, const nlohmann::json& fallback);

} // namespace gm::save


