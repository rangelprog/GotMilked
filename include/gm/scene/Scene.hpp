#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <glm/glm.hpp>
#include "SceneSystem.hpp"
#include "gm/rendering/LightManager.hpp"

namespace gm {

class GameObject;
class GameObjectUpdateSystem;

class Shader;
class Camera;

class Scene {
private:
    std::vector<std::shared_ptr<GameObject>> gameObjects;
    std::unordered_map<std::string, std::vector<std::shared_ptr<GameObject>>> objectsByTag;
    std::unordered_map<std::string, std::shared_ptr<GameObject>> objectsByName;
    bool isInitialized = false;
    bool isPaused = false;
    std::string sceneName;
    bool systemsInitialized = false;
    bool parallelGameObjectUpdatesEnabled = false;
    std::vector<SceneSystemPtr> systems;
    LightManager m_lightManager;  // Cached LightManager to avoid per-frame allocation
    
    // Optimized lists for active GameObjects (avoid iterating inactive ones)
    std::vector<std::shared_ptr<GameObject>> m_activeRenderables;
    std::vector<std::shared_ptr<GameObject>> m_activeUpdatables;
    bool m_activeListsDirty = true;  // Flag to track if lists need rebuilding
    
    // Frustum culling
    struct FrustumPlane {
        glm::vec4 plane;  // (a, b, c, d) where ax + by + cz + d = 0
    };
    
    struct Frustum {
        FrustumPlane planes[6];  // left, right, bottom, top, near, far
    };
    
    Frustum CalculateFrustum(const glm::mat4& viewProj) const;
    bool IsInFrustum(const GameObject& obj, const Frustum& frustum) const;
    bool m_frustumCullingEnabled = true;  // Enable/disable frustum culling
    
    // Instanced rendering
    struct InstancedBatch {
        class Mesh* mesh = nullptr;
        class Shader* shader = nullptr;
        std::shared_ptr<class Material> material;
        std::vector<glm::mat4> modelMatrices;
        std::vector<glm::mat3> normalMatrices;
        std::vector<std::shared_ptr<GameObject>> gameObjects;  // Keep reference for camera access
    };
    
    void CollectInstancedBatches(std::vector<InstancedBatch>& batches, const Frustum* frustum) const;
    void RenderInstancedBatch(const InstancedBatch& batch, Shader& shader, const Camera& cam) const;
    bool m_instancedRenderingEnabled = true;  // Enable/disable instanced rendering

public:
    Scene(const std::string& name = "Unnamed Scene");
    virtual ~Scene() = default;

    // Lifecycle
    virtual void Init();
    virtual void Update(float deltaTime);
    virtual void Cleanup();
    void Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg);

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

    // Systems
    void RegisterSystem(const SceneSystemPtr& system);
    bool UnregisterSystem(std::string_view name);
    void ClearSystems();
    const std::vector<SceneSystemPtr>& GetSystems() const { return systems; }
    void SetParallelGameObjectUpdates(bool enabled) { parallelGameObjectUpdatesEnabled = enabled; }
    bool GetParallelGameObjectUpdates() const { return parallelGameObjectUpdatesEnabled; }

    // Querying
    std::shared_ptr<GameObject> FindGameObjectByName(const std::string& name);
    std::vector<std::shared_ptr<GameObject>> FindGameObjectsByTag(const std::string& tag);
    std::vector<std::shared_ptr<GameObject>>& GetAllGameObjects() { return gameObjects; }

    // Tags
    void TagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag);
    void UntagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag);

    // Serialization
    bool SaveToFile(const std::string& filepath);
    bool LoadFromFile(const std::string& filepath);
    
    // Optimization: Mark active lists as dirty (call when GameObject active state changes)
    void MarkActiveListsDirty() { m_activeListsDirty = true; }
    
    // Frustum culling
    void SetFrustumCullingEnabled(bool enabled) { m_frustumCullingEnabled = enabled; }
    bool IsFrustumCullingEnabled() const { return m_frustumCullingEnabled; }
    
    // Instanced rendering
    void SetInstancedRenderingEnabled(bool enabled) { m_instancedRenderingEnabled = enabled; }
    bool IsInstancedRenderingEnabled() const { return m_instancedRenderingEnabled; }

private:
    void UpdateGameObjects(float deltaTime);
    void CleanupDestroyedObjects();
    void InitializeSystems();
    void ShutdownSystems();
    void RunSystems(float deltaTime);
    void UpdateActiveLists();  // Rebuild active renderable/updatable lists

    friend class GameObjectUpdateSystem;
};

}