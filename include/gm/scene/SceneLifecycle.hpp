#pragma once

#include <string_view>
#include <vector>

#include "SceneSystem.hpp"

namespace gm {

class Scene;

class SceneLifecycle {
public:
    explicit SceneLifecycle(Scene& owner);

    void RegisterSystem(const SceneSystemPtr& system);
    bool UnregisterSystem(std::string_view name);
    void ClearSystems();

    void InitializeSystems();
    void ShutdownSystems();
    void RunSystems(float deltaTime);

    void OnSceneInit();
    void OnSceneShutdown();

    const std::vector<SceneSystemPtr>& GetSystems() const { return m_systems; }

private:
    Scene& m_scene;
    std::vector<SceneSystemPtr> m_systems;
    bool m_systemsInitialized = false;
};

} // namespace gm

