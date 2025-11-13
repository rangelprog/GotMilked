#pragma once

#if GM_DEBUG_TOOLS

#include "gm/scene/SceneSystem.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

struct GLFWwindow;

namespace gm {
class Camera;
class Component;
class Scene;
}

namespace gm::debug {

class ITerrainEditing;

/**
 * @brief Scene system that binds shared runtime services (camera, window, FOV) to terrain editors.
 */
class TerrainEditingSystem : public gm::SceneSystem {
public:
    TerrainEditingSystem() = default;
    ~TerrainEditingSystem() override = default;

    std::string_view GetName() const override { return "TerrainEditingSystem"; }

    void OnRegister(gm::Scene& scene) override;
    void OnSceneInit(gm::Scene& scene) override;
    void OnSceneShutdown(gm::Scene& scene) override;
    void Update(gm::Scene& scene, float deltaTime) override;

    void SetCamera(gm::Camera* camera);
    void SetWindow(GLFWwindow* window);
    void SetFovProvider(std::function<float()> provider);
    void SetSceneContext(const std::shared_ptr<gm::Scene>& scene);

    void RefreshBindings();

private:
    struct TerrainHandle {
        std::weak_ptr<gm::Component> component;
    };

    void CollectTerrains(gm::Scene& scene);
    void ApplyBindingsTo(const std::shared_ptr<gm::Component>& component) const;

    gm::Camera* m_camera = nullptr;
    GLFWwindow* m_window = nullptr;
    std::function<float()> m_fovProvider;
    std::weak_ptr<gm::Scene> m_sceneWeak;
    std::vector<TerrainHandle> m_terrains;
    std::uint64_t m_lastSceneVersion = 0;
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


