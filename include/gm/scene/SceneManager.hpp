#pragma once
#include <memory>
#include <unordered_map>
#include <string>

namespace gm {

class Scene;

/**
 * SceneManager - Singleton for managing scene loading, unloading, and switching
 * Supports multiple active scenes and efficient scene transitions
 */
class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;

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

private:
    std::unordered_map<std::string, std::shared_ptr<Scene>> scenes;
    std::shared_ptr<Scene> activeScene;
};

} // namespace gm
