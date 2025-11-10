#include "gm/scene/SceneManager.hpp"
#include <algorithm>
#include <cstdio>

namespace gm {

std::shared_ptr<Scene> SceneManager::CreateScene(const std::string& name) {
    if (name.empty()) {
        printf("[SceneManager] Error: Cannot create scene with empty name\n");
        return nullptr;
    }
    
    if (scenes.find(name) != scenes.end()) {
        printf("[SceneManager] Warning: Scene '%s' already exists, returning existing scene\n", name.c_str());
        return scenes[name];
    }

    auto scene = std::make_shared<Scene>(name);
    scenes[name] = scene;
    
    printf("[SceneManager] Created scene '%s'\n", name.c_str());
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
        
        printf("[SceneManager] Unloaded scene '%s'\n", name.c_str());
    }
}

void SceneManager::UnloadAllScenes() {
    for (auto& [name, scene] : scenes) {
        scene->Cleanup();
    }
    scenes.clear();
    activeScene = nullptr;
    printf("[SceneManager] Unloaded all scenes\n");
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
        printf("[SceneManager] Error: Cannot set active scene with empty name\n");
        return;
    }
    
    auto scene = GetScene(name);
    if (scene) {
        activeScene = scene;
        printf("[SceneManager] Set active scene to '%s'\n", name.c_str());
    } else {
        printf("[SceneManager] Error: Scene '%s' not found\n", name.c_str());
    }
}

void SceneManager::InitActiveScene() {
    if (!activeScene) {
        printf("[SceneManager] Warning: No active scene to initialize\n");
        return;
    }
    
    if (activeScene->IsInitialized()) {
        printf("[SceneManager] Warning: Active scene '%s' is already initialized\n", activeScene->GetName().c_str());
        return;
    }
    
    activeScene->Init();
}

void SceneManager::UpdateActiveScene(float deltaTime) {
    if (!activeScene) {
        return; // No active scene is not an error, just skip update
    }
    
    if (deltaTime < 0.0f) {
        printf("[SceneManager] Warning: Negative deltaTime (%.6f), clamping to 0\n", deltaTime);
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
