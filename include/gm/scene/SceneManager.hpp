#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include "Scene.hpp"

namespace gm {

/**
 * SceneManager - Singleton for managing scene loading, unloading, and switching
 * Supports multiple active scenes and efficient scene transitions
 */
class SceneManager {
private:
    std::unordered_map<std::string, std::shared_ptr<Scene>> scenes;
    std::shared_ptr<Scene> activeScene;
    
    SceneManager() = default;
    ~SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

public:
    // Singleton access - uses static local for thread-safe initialization (C++11)
    static SceneManager& Instance() {
        static SceneManager instance;
        return instance;
    }

    // Scene management
    std::shared_ptr<Scene> CreateScene(const std::string& name);
    std::shared_ptr<Scene> LoadScene(const std::string& name);
    void UnloadScene(const std::string& name);
    void UnloadAllScenes();

    // Scene queries
    std::shared_ptr<Scene> GetScene(const std::string& name);
    std::shared_ptr<Scene> GetActiveScene() const { return activeScene; }
    bool HasScene(const std::string& name) const;

    // Scene lifecycle
    void SetActiveScene(const std::string& name);
    void InitActiveScene();
    void UpdateActiveScene(float deltaTime);

    // Cleanup
    void Shutdown();
};

} // namespace gm
