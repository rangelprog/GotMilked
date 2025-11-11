#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../save/SaveGame.hpp"

namespace gm {
class Camera;
class Scene;
}

namespace sandbox {
class ResourceHotReloader;
namespace gameplay {
class SandboxGameplay;
}
namespace save {
class SaveManager;
struct SaveMetadata;
}
} // namespace sandbox

namespace sandbox::tooling {

class ToolingOverlay {
public:
    struct Callbacks {
        std::function<void()> quickSave;
        std::function<void()> quickLoad;
        std::function<void()> reloadResources;
    };

    void SetCallbacks(Callbacks callbacks) { m_callbacks = std::move(callbacks); }
    void SetSaveManager(save::SaveManager* manager) { m_saveManager = manager; }
    void SetHotReloader(ResourceHotReloader* reloader) { m_hotReloader = reloader; }
    void SetGameplay(gameplay::SandboxGameplay* gameplay) { m_gameplay = gameplay; }
    void SetCamera(gm::Camera* camera) { m_camera = camera; }
    void SetScene(const std::shared_ptr<gm::Scene>& scene) { m_scene = scene; }

    void AddNotification(const std::string& message);

    void Render(bool& overlayOpen);

private:
    void RenderSaveTable();
    void RenderNotifications();
    void RefreshSaveList();
    void PruneNotifications();

    save::SaveManager* m_saveManager = nullptr;
    ResourceHotReloader* m_hotReloader = nullptr;
    gameplay::SandboxGameplay* m_gameplay = nullptr;
    gm::Camera* m_camera = nullptr;
    std::weak_ptr<gm::Scene> m_scene;

    Callbacks m_callbacks;

    std::vector<std::pair<std::chrono::system_clock::time_point, std::string>> m_notifications;
    std::vector<save::SaveMetadata> m_cachedSaves;
    std::chrono::system_clock::time_point m_lastSaveRefresh{};
};

} // namespace sandbox::tooling

