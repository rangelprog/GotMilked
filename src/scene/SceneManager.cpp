#include "gm/scene/SceneManager.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/core/Logger.hpp"
#include <algorithm>

namespace gm {

std::shared_ptr<Scene> SceneManager::CreateScene(const std::string& name) {
    if (name.empty()) {
        core::Logger::Error("[SceneManager] Cannot create scene with empty name");
        return nullptr;
    }
    
    if (scenes.find(name) != scenes.end()) {
        core::Logger::Warning("[SceneManager] Scene '%s' already exists, returning existing scene",
                              name.c_str());
        return scenes[name];
    }

    auto scene = std::make_shared<Scene>(name);
    scenes[name] = scene;
    
    core::Logger::Info("[SceneManager] Created scene '%s'", name.c_str());
    return scene;
}

std::shared_ptr<Scene> SceneManager::LoadScene(const std::string& name) {
    auto scene = CreateScene(name);
    SetActiveScene(name);
    return scene;
}

void SceneManager::UnloadScene(const std::string& name) {
    auto it = scenes.find(name);
    if (it != scenes.end()) {
        it->second->Cleanup();
        scenes.erase(it);
        
        // If this was the active scene, clear it
        if (activeScene && activeScene->GetName() == name) {
            activeScene = nullptr;
        }
        
        core::Logger::Info("[SceneManager] Unloaded scene '%s'", name.c_str());
    }
}

void SceneManager::UnloadAllScenes() {
    for (auto& [name, scene] : scenes) {
        scene->Cleanup();
    }
    scenes.clear();
    activeScene = nullptr;
    core::Logger::Info("[SceneManager] Unloaded all scenes");
}

std::shared_ptr<Scene> SceneManager::GetScene(const std::string& name) {
    auto it = scenes.find(name);
    if (it != scenes.end()) {
        return it->second;
    }
    return nullptr;
}

bool SceneManager::HasScene(const std::string& name) const {
    return scenes.find(name) != scenes.end();
}

void SceneManager::SetActiveScene(const std::string& name) {
    if (name.empty()) {
        core::Logger::Error("[SceneManager] Cannot set active scene with empty name");
        return;
    }
    
    auto scene = GetScene(name);
    if (scene) {
        activeScene = scene;
        core::Logger::Info("[SceneManager] Set active scene to '%s'", name.c_str());
    } else {
        core::Logger::Error("[SceneManager] Scene '%s' not found", name.c_str());
    }
}

void SceneManager::InitActiveScene() {
    if (!activeScene) {
        core::Logger::Warning("[SceneManager] No active scene to initialize");
        return;
    }
    
    if (activeScene->IsInitialized()) {
        core::Logger::Warning("[SceneManager] Active scene '%s' is already initialized",
                              activeScene->GetName().c_str());
        return;
    }
    
    activeScene->Init();
}

void SceneManager::UpdateActiveScene(float deltaTime) {
    if (!activeScene) {
        return; // No active scene is not an error, just skip update
    }
    
    if (deltaTime < 0.0f) {
        core::Logger::Warning("[SceneManager] Negative deltaTime (%.6f), clamping to 0",
                              deltaTime);
        deltaTime = 0.0f;
    }
    
    activeScene->Update(deltaTime);
}

void SceneManager::Shutdown() {
    UnloadAllScenes();
    // Static local instance is automatically destroyed on program exit
    // No manual cleanup needed
}

} // namespace gm
