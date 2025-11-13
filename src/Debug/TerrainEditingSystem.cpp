#include "gm/debug/TerrainEditingSystem.hpp"

#if GM_DEBUG_TOOLS

#include "gm/debug/ITerrainEditing.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Component.hpp"

#include <utility>

namespace gm::debug {

void TerrainEditingSystem::OnRegister(gm::Scene& scene) {
    m_lastSceneVersion = scene.CurrentReloadVersion();
    CollectTerrains(scene);
}

void TerrainEditingSystem::OnSceneInit(gm::Scene& scene) {
    m_lastSceneVersion = scene.CurrentReloadVersion();
    CollectTerrains(scene);
    RefreshBindings();
}

void TerrainEditingSystem::OnSceneShutdown(gm::Scene&) {
    m_terrains.clear();
}

void TerrainEditingSystem::Update(gm::Scene& scene, float /*deltaTime*/) {
    if (scene.CurrentReloadVersion() != m_lastSceneVersion) {
        m_lastSceneVersion = scene.CurrentReloadVersion();
        CollectTerrains(scene);
        RefreshBindings();
    }
}

void TerrainEditingSystem::SetCamera(gm::Camera* camera) {
    m_camera = camera;
    RefreshBindings();
}

void TerrainEditingSystem::SetWindow(GLFWwindow* window) {
    m_window = window;
    RefreshBindings();
}

void TerrainEditingSystem::SetFovProvider(std::function<float()> provider) {
    m_fovProvider = std::move(provider);
    RefreshBindings();
}

void TerrainEditingSystem::SetSceneContext(const std::shared_ptr<gm::Scene>& scene) {
    m_sceneWeak = scene;
    if (auto shared = m_sceneWeak.lock()) {
        m_lastSceneVersion = shared->CurrentReloadVersion();
        CollectTerrains(*shared);
        RefreshBindings();
    }
}

void TerrainEditingSystem::RefreshBindings() {
    for (auto it = m_terrains.begin(); it != m_terrains.end();) {
        if (auto component = it->component.lock()) {
            ApplyBindingsTo(component);
            ++it;
        } else {
            it = m_terrains.erase(it);
        }
    }
}

void TerrainEditingSystem::CollectTerrains(gm::Scene& scene) {
    m_terrains.clear();
    auto& objects = scene.GetAllGameObjects();
    for (auto& object : objects) {
        if (!object) {
            continue;
        }
        for (auto& component : object->GetComponents()) {
            if (!component) {
                continue;
            }
            if (dynamic_cast<ITerrainEditing*>(component.get())) {
                m_terrains.push_back(TerrainHandle{component});
            }
        }
    }
}

void TerrainEditingSystem::ApplyBindingsTo(const std::shared_ptr<gm::Component>& component) const {
    if (!component) {
        return;
    }
    auto* editor = dynamic_cast<ITerrainEditing*>(component.get());
    if (!editor) {
        return;
    }
    editor->SetCamera(m_camera);
    editor->SetWindow(m_window);
    if (m_fovProvider) {
        editor->SetFovProvider(m_fovProvider);
    }
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


