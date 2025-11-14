#include "gm/scene/SceneLifecycle.hpp"

#include "gm/core/Logger.hpp"
#include "gm/scene/Scene.hpp"

#include <algorithm>
#include <future>

namespace gm {

SceneLifecycle::SceneLifecycle(Scene& owner)
    : m_scene(owner) {}

void SceneLifecycle::RegisterSystem(const SceneSystemPtr& system) {
    if (!system) {
        core::Logger::Warning("[Scene] Attempted to register null SceneSystem");
        return;
    }

    std::string_view nameView = system->GetName();
    if (nameView.empty()) {
        core::Logger::Warning("[Scene] SceneSystem with empty name ignored");
        return;
    }

    auto duplicate = std::find_if(m_systems.begin(), m_systems.end(),
        [nameView](const SceneSystemPtr& existing) {
            return existing && existing->GetName() == nameView;
        });

    if (duplicate != m_systems.end()) {
        core::Logger::Warning("[Scene] SceneSystem '{}' already registered", nameView);
        return;
    }

    m_systems.push_back(system);

    if (m_systemsInitialized) {
        system->OnRegister(m_scene);
        if (m_scene.IsInitialized()) {
            system->OnSceneInit(m_scene);
        }
    }
}

bool SceneLifecycle::UnregisterSystem(std::string_view name) {
    auto it = std::find_if(m_systems.begin(), m_systems.end(),
        [name](const SceneSystemPtr& system) {
            return system && system->GetName() == name;
        });

    if (it == m_systems.end()) {
        return false;
    }

    if (m_scene.IsInitialized() && *it) {
        (*it)->OnSceneShutdown(m_scene);
    }
    if (m_systemsInitialized && *it) {
        (*it)->OnUnregister(m_scene);
    }

    m_systems.erase(it);
    return true;
}

void SceneLifecycle::ClearSystems() {
    if (m_scene.IsInitialized()) {
        for (auto& system : m_systems) {
            if (system) {
                system->OnSceneShutdown(m_scene);
            }
        }
    }
    ShutdownSystems();
    m_systems.clear();
}

void SceneLifecycle::InitializeSystems() {
    if (m_systemsInitialized) {
        return;
    }
    for (auto& system : m_systems) {
        if (system) {
            system->OnRegister(m_scene);
            if (m_scene.IsInitialized()) {
                system->OnSceneInit(m_scene);
            }
        }
    }
    m_systemsInitialized = true;
}

void SceneLifecycle::ShutdownSystems() {
    if (!m_systemsInitialized) {
        return;
    }
    for (auto& system : m_systems) {
        if (system) {
            system->OnUnregister(m_scene);
        }
    }
    m_systemsInitialized = false;
}

void SceneLifecycle::RunSystems(float deltaTime) {
    std::vector<std::future<void>> asyncJobs;
    asyncJobs.reserve(m_systems.size());

    for (auto& system : m_systems) {
        if (!system) {
            continue;
        }

        if (system->RunsAsync()) {
            SceneSystemPtr sys = system;
            asyncJobs.emplace_back(std::async(std::launch::async, [this, sys, deltaTime]() {
                sys->Update(m_scene, deltaTime);
            }));
        } else {
            system->Update(m_scene, deltaTime);
        }
    }

    for (auto& job : asyncJobs) {
        if (job.valid()) {
            job.get();
        }
    }
}

void SceneLifecycle::OnSceneInit() {
    for (auto& system : m_systems) {
        if (system) {
            system->OnSceneInit(m_scene);
        }
    }
}

void SceneLifecycle::OnSceneShutdown() {
    for (auto& system : m_systems) {
        if (system) {
            system->OnSceneShutdown(m_scene);
        }
    }
}

} // namespace gm

