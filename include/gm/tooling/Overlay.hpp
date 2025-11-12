#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <deque>

#include <glm/vec3.hpp>

namespace gm {
class Camera;
class Scene;
}

namespace gm::save {
struct SaveMetadata;
class SaveManager;
}

namespace gm::utils {
class HotReloader;
}

namespace gm::physics {
class PhysicsWorld;
}

namespace gm::tooling {

class Overlay {
public:
    struct Callbacks {
        std::function<void()> quickSave;
        std::function<void()> quickLoad;
        std::function<void()> reloadResources;
    };

    struct WorldInfo {
        std::string sceneName;
        double worldTimeSeconds = 0.0;
        glm::vec3 cameraPosition{0.0f};
        glm::vec3 cameraDirection{0.0f, 0.0f, -1.0f};
    };

    using WorldInfoProvider = std::function<std::optional<WorldInfo>()>;

    void SetCallbacks(Callbacks callbacks) { m_callbacks = std::move(callbacks); }
    void SetSaveManager(gm::save::SaveManager* manager);
    void SetHotReloader(gm::utils::HotReloader* reloader) { m_hotReloader = reloader; }
    void SetCamera(gm::Camera* camera) { m_camera = camera; }
    void SetScene(const std::shared_ptr<gm::Scene>& scene);
    void SetWorldInfoProvider(WorldInfoProvider provider) { m_worldInfoProvider = std::move(provider); }
    void SetPhysicsWorld(gm::physics::PhysicsWorld* physics) { m_physicsWorld = physics; }

    void AddNotification(const std::string& message);

    void Render(bool& overlayOpen);

private:
    void RenderActionsSection();
    void RenderHotReloadSection();
    void RenderSaveSection();
    void RenderWorldSection();
    void RenderPhysicsSection();
    void RenderNotifications();
    void RefreshSaveList();
    void PruneNotifications();

    gm::save::SaveManager* m_saveManager = nullptr;
    gm::utils::HotReloader* m_hotReloader = nullptr;
    gm::Camera* m_camera = nullptr;
    gm::physics::PhysicsWorld* m_physicsWorld = nullptr;
    std::weak_ptr<gm::Scene> m_scene;

    Callbacks m_callbacks;
    WorldInfoProvider m_worldInfoProvider;

    std::deque<std::pair<std::chrono::system_clock::time_point, std::string>> m_notifications;
    std::vector<gm::save::SaveMetadata> m_cachedSaves;
    std::chrono::system_clock::time_point m_lastSaveRefresh{};
};

} // namespace gm::tooling

