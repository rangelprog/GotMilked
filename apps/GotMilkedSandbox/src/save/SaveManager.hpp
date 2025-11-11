#pragma once

#include "SaveGame.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace gm {
class Scene;
class Camera;
}

namespace sandbox {
class SandboxResources;
namespace gameplay {
class SandboxGameplay;
}
}

namespace sandbox::save {

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
    std::filesystem::path m_saveDirectory;

    std::filesystem::path SlotPath(const std::string& slotName) const;
    std::filesystem::path QuickSavePath() const;
};

SaveGameData CaptureSnapshot(const gm::Scene& scene,
                             const gm::Camera& camera,
                             const gameplay::SandboxGameplay& gameplay);

void ApplySnapshot(const SaveGameData& data,
                   gm::Scene& scene,
                   gm::Camera& camera,
                   gameplay::SandboxGameplay& gameplay);

} // namespace sandbox::save

