#pragma once

#include "gm/save/SaveManager.hpp"
#include <memory>
#include <functional>

namespace gm {
class Camera;
class Scene;
}

namespace gm::gameplay {
class FlyCameraController;
}

namespace gm::save {

/**
 * Helper functions for capturing and applying save game snapshots.
 * These provide default implementations for common game state (camera, scene, world time).
 */
struct SaveSnapshotHelpers {
    // World time provider function type
    using WorldTimeProvider = std::function<double()>;

    // Capture snapshot from camera, scene, and optional world time provider
    static SaveGameData CaptureSnapshot(
        const Camera* camera,
        const std::shared_ptr<Scene>& scene,
        WorldTimeProvider worldTimeProvider = nullptr);

    // Apply snapshot to camera, scene, and optional world time setter
    // Returns true if snapshot was applied successfully
    static bool ApplySnapshot(
        const SaveGameData& data,
        Camera* camera,
        const std::shared_ptr<Scene>& scene,
        std::function<void(double)> worldTimeSetter = nullptr);
};

} // namespace gm::save

