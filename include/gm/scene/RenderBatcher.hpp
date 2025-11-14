#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glad/glad.h>
#include <glm/glm.hpp>

namespace gm {

class Camera;
class GameObject;
class Shader;
class Scene;
class LightManager;
class GameObjectScheduler;
class Material;

class RenderBatcher {
public:
    struct InstancedGroup {
        class Mesh* mesh = nullptr;
        class Shader* shader = nullptr;
        std::shared_ptr<class Material> material;
        std::vector<std::shared_ptr<GameObject>> objects;
    };

    explicit RenderBatcher(Scene& owner, GameObjectScheduler& scheduler);
    ~RenderBatcher();

    void SetFrustumCullingEnabled(bool enabled) { m_frustumCullingEnabled = enabled; }
    bool IsFrustumCullingEnabled() const { return m_frustumCullingEnabled; }

    void SetInstancedRenderingEnabled(bool enabled) { m_instancedRenderingEnabled = enabled; }
    bool IsInstancedRenderingEnabled() const { return m_instancedRenderingEnabled; }

    void Draw(const Camera& cam,
              const glm::mat4& view,
              const glm::mat4& proj,
              std::uint64_t sceneVersion);

    const std::vector<InstancedGroup>& GetInstancedGroups(std::uint64_t sceneVersion) const;

    void MarkDirty();

private:
    struct FrustumPlane {
        glm::vec4 plane;
    };

    struct Frustum {
        FrustumPlane planes[6];
    };

    struct InstanceBatchData {
        std::vector<std::shared_ptr<GameObject>> gameObjects;
        std::vector<glm::mat4> modelMatrices;
        std::vector<glm::mat4> normalMatrices;
    };

    struct BatchKey {
        class Mesh* mesh = nullptr;
        class Shader* shader = nullptr;
        const Material* material = nullptr;

        bool operator==(const BatchKey& other) const {
            return mesh == other.mesh && shader == other.shader && material == other.material;
        }
    };

    struct BatchKeyHash {
        std::size_t operator()(const BatchKey& key) const {
            std::size_t h1 = std::hash<class Mesh*>{}(key.mesh);
            std::size_t h2 = std::hash<class Shader*>{}(key.shader);
            std::size_t h3 = std::hash<const Material*>{}(key.material);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    struct BatchGpuResources {
        GLuint modelBuffer = 0;
        GLuint normalBuffer = 0;
        std::size_t capacity = 0;
    };

    Frustum CalculateFrustum(const glm::mat4& viewProj) const;
    bool IsInFrustum(const GameObject& obj, const Frustum& frustum) const;

    void EnsureInstancedGroups(std::uint64_t sceneVersion) const;
    InstanceBatchData BuildInstanceBatchData(
        const InstancedGroup& group,
        const std::unordered_set<const GameObject*>& visibleSet) const;
    BatchGpuResources& GetOrCreateResources(const BatchKey& key) const;
    void UploadInstanceData(BatchGpuResources& resources,
                            const std::vector<glm::mat4>& models,
                            const std::vector<glm::mat4>& normals) const;
    void DeleteResources(BatchGpuResources& resources) const;

    Scene& m_scene;
    GameObjectScheduler& m_scheduler;
    bool m_frustumCullingEnabled = true;
    bool m_instancedRenderingEnabled = true;
    mutable bool m_instancedGroupsDirty = true;
    mutable std::vector<InstancedGroup> m_instancedGroups;
    mutable std::uint64_t m_instancedGroupsVersion = 0;
    mutable std::unordered_map<BatchKey, BatchGpuResources, BatchKeyHash> m_batchGpuCache;
};

} // namespace gm

