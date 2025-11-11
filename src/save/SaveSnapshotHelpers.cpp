#include "gm/save/SaveSnapshotHelpers.hpp"

#include "gm/rendering/Camera.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/core/Logger.hpp"

namespace gm::save {

SaveGameData SaveSnapshotHelpers::CaptureSnapshot(
    const Camera* camera,
    const std::shared_ptr<Scene>& scene,
    WorldTimeProvider worldTimeProvider) {
    SaveGameData data;
    
    if (scene) {
        data.sceneName = scene->GetName();
    }
    
    if (camera) {
        data.cameraPosition = camera->Position();
        data.cameraForward = camera->Front();
    }
    
    if (worldTimeProvider) {
        data.worldTime = worldTimeProvider();
    }
    
    return data;
}

bool SaveSnapshotHelpers::ApplySnapshot(
    const SaveGameData& data,
    Camera* camera,
    const std::shared_ptr<Scene>& scene,
    std::function<void(double)> worldTimeSetter) {
    
    if (!camera) {
        gm::core::Logger::Warning("[SaveSnapshotHelpers] Cannot apply snapshot: camera is null");
        return false;
    }
    
    // Check scene name match (warn if different, but don't fail)
    if (scene && !data.sceneName.empty() && data.sceneName != scene->GetName()) {
        gm::core::Logger::Info(
            "[SaveSnapshotHelpers] Snapshot references scene '%s' (current '%s') -- scene switching not yet implemented",
            data.sceneName.c_str(), scene->GetName().c_str());
    }
    
    // Apply camera position and orientation
    camera->SetPosition(data.cameraPosition);
    camera->SetForward(data.cameraForward);
    if (data.cameraFov > 0.0f) {
        camera->SetFov(data.cameraFov);
    }
    
    // Apply world time if setter provided
    if (worldTimeSetter) {
        worldTimeSetter(data.worldTime);
    }
    
    return true;
}

} // namespace gm::save

