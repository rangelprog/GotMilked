#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <string_view>
#include <cstdint>
#include "SceneSystem.hpp"
#include "gm/rendering/LightManager.hpp"
#include "gm/scene/GameObjectScheduler.hpp"
#include "gm/scene/RenderBatcher.hpp"
#include "gm/scene/SceneLifecycle.hpp"

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
    LightManager m_lightManager;  // Cached LightManager to avoid per-frame allocation
    bool m_nameLookupDirty = true;
    std::size_t m_destroyedSinceLastCleanup = 0;
    int m_framesSinceLastCleanup = 0;
    struct GameObjectPool {
        std::vector<std::shared_ptr<GameObject>> objects;
        void Reserve(std::size_t capacity);
        std::shared_ptr<GameObject> Acquire(Scene& owner, const std::string& name);
        void Release(Scene& owner, std::shared_ptr<GameObject> gameObject);
        void Clear();
        std::size_t Size() const { return objects.size(); }
    };
    GameObjectPool m_gameObjectPool;
    
    GameObjectScheduler m_scheduler;
    RenderBatcher m_renderBatcher;
    SceneLifecycle m_lifecycle;

public:
    using InstancedGroup = RenderBatcher::InstancedGroup;
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
    bool SetParent(const std::shared_ptr<GameObject>& child, const std::shared_ptr<GameObject>& newParent);
    bool SetParent(const std::shared_ptr<GameObject>& child, GameObject* newParent);

    // Systems
    void RegisterSystem(const SceneSystemPtr& system);
    bool UnregisterSystem(std::string_view name);
    void ClearSystems();
    const std::vector<SceneSystemPtr>& GetSystems() const { return m_lifecycle.GetSystems(); }
    void SetParallelGameObjectUpdates(bool enabled) { m_scheduler.SetParallelUpdatesEnabled(enabled); }
    bool GetParallelGameObjectUpdates() const { return m_scheduler.GetParallelUpdatesEnabled(); }

    // Querying
    std::shared_ptr<GameObject> FindGameObjectByName(const std::string& name);
    std::shared_ptr<GameObject> FindGameObjectByPointer(const GameObject* ptr);
    std::vector<std::shared_ptr<GameObject>> FindGameObjectsByTag(const std::string& tag);
    std::vector<std::shared_ptr<GameObject>>& GetAllGameObjects() { return gameObjects; }
    std::vector<std::shared_ptr<GameObject>> GetRootGameObjects() const;
    const std::vector<std::shared_ptr<GameObject>>& GetActiveRenderables();
    const std::vector<InstancedGroup>& GetInstancedGroups() const;
    void InvalidateInstancedGroups() { m_renderBatcher.MarkDirty(); }

    // Tags
    void TagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag);
    void UntagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag);

    // Serialization
    bool SaveToFile(const std::string& filepath);
    bool LoadFromFile(const std::string& filepath);
    
    // Optimization: Mark active lists as dirty (call when GameObject active state changes)
    void MarkActiveListsDirty();
    void BumpReloadVersion();
    std::uint64_t CurrentReloadVersion() const { return m_reloadVersion; }
    
    // Frustum culling
    void SetFrustumCullingEnabled(bool enabled) { m_renderBatcher.SetFrustumCullingEnabled(enabled); }
    bool IsFrustumCullingEnabled() const { return m_renderBatcher.IsFrustumCullingEnabled(); }
    
    // Instanced rendering
    void SetInstancedRenderingEnabled(bool enabled) { m_renderBatcher.SetInstancedRenderingEnabled(enabled); }
    bool IsInstancedRenderingEnabled() const { return m_renderBatcher.IsInstancedRenderingEnabled(); }

private:
    void UpdateGameObjects(float deltaTime);
    void CleanupDestroyedObjects();
    void InitializeSystems();
    void ShutdownSystems();
    void RunSystems(float deltaTime);
    void EnsureNameLookup();
    void HandleGameObjectRename(GameObject& object, const std::string& oldName, const std::string& newName);
    void MarkNameLookupDirty() { m_nameLookupDirty = true; }
    void ResetCleanupCounters();
    std::shared_ptr<GameObject> AcquireGameObject(const std::string& name);
    void ReleaseGameObject(std::shared_ptr<GameObject> gameObject);
    void ClearObjectPool();
    void RemoveFromActiveLists(const std::shared_ptr<GameObject>& gameObject);
    std::string GenerateUniqueName();

    friend class GameObjectUpdateSystem;
    friend class GameObject;

    std::uint64_t m_unnamedObjectCounter = 0;
    std::uint64_t m_reloadVersion = 0;
};

}