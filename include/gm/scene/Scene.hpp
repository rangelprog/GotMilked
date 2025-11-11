#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include "gm/scene/GameObject.hpp"

namespace gm {

class Scene {
private:
    std::vector<std::shared_ptr<GameObject>> gameObjects;
    std::unordered_map<std::string, std::vector<std::shared_ptr<GameObject>>> objectsByTag;
    std::unordered_map<std::string, std::shared_ptr<GameObject>> objectsByName;
    bool isInitialized = false;
    bool isPaused = false;
    std::string sceneName;

public:
    Scene(const std::string& name = "Unnamed Scene");
    virtual ~Scene() = default;

    // Lifecycle
    virtual void Init();
    virtual void Update(float deltaTime);
    virtual void Cleanup();
    void Draw(class Shader& shader, const class Camera& cam, int fbw, int fbh, float fovDeg);

    // Scene state
    const std::string& GetName() const { return sceneName; }
    bool IsInitialized() const { return isInitialized; }
    bool IsPaused() const { return isPaused; }
    void SetPaused(bool paused) { isPaused = paused; }

    // GameObject management
    std::shared_ptr<GameObject> CreateGameObject(const std::string& name);
    std::shared_ptr<GameObject> SpawnGameObject(const std::string& name);
    void DestroyGameObject(std::shared_ptr<GameObject> gameObject);
    void DestroyGameObjectByName(const std::string& name);

    // Querying
    std::shared_ptr<GameObject> FindGameObjectByName(const std::string& name);
    std::vector<std::shared_ptr<GameObject>> FindGameObjectsByTag(const std::string& tag);
    std::vector<std::shared_ptr<GameObject>>& GetAllGameObjects() { return gameObjects; }

    // Tags
    void TagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag);
    void UntagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag);


private:
    void UpdateGameObjects(float deltaTime);
    void CleanupDestroyedObjects();
};

}