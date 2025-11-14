#include "gm/scene/RenderBatcher.hpp"

#include "gm/core/Logger.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/GameObjectScheduler.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/TransformComponent.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include <glad/glad.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace gm {

namespace {
constexpr GLuint kInstanceModelBufferBinding = 4;
constexpr GLuint kInstanceNormalBufferBinding = 5;
}

RenderBatcher::RenderBatcher(Scene& owner, GameObjectScheduler& scheduler)
    : m_scene(owner)
    , m_scheduler(scheduler) {}

RenderBatcher::~RenderBatcher() {
    for (auto& entry : m_batchGpuCache) {
        DeleteResources(entry.second);
    }
}

void RenderBatcher::Draw(const Camera& cam,
                         const glm::mat4& view,
                         const glm::mat4& proj,
                         std::uint64_t sceneVersion) {
    const auto& activeRenderables = m_scheduler.GetActiveRenderables();

    Frustum frustum;
    const Frustum* frustumPtr = nullptr;
    if (m_frustumCullingEnabled) {
        glm::mat4 viewProj = proj * view;
        frustum = CalculateFrustum(viewProj);
        frustumPtr = &frustum;
    }

    std::vector<std::shared_ptr<GameObject>> visibleRenderables;
    visibleRenderables.reserve(activeRenderables.size());
    for (const auto& gameObject : activeRenderables) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }
        if (frustumPtr && !IsInFrustum(*gameObject, *frustumPtr)) {
            continue;
        }
        visibleRenderables.push_back(gameObject);
    }

    if (!m_instancedRenderingEnabled) {
        for (const auto& gameObject : visibleRenderables) {
            if (gameObject && !gameObject->IsDestroyed()) {
                gameObject->Render();
            }
        }
        return;
    }

    std::unordered_set<const GameObject*> visibleSet;
    visibleSet.reserve(visibleRenderables.size());
    for (const auto& obj : visibleRenderables) {
        if (obj) {
            visibleSet.insert(obj.get());
        }
    }

    std::unordered_set<const GameObject*> instancedObjects;
    instancedObjects.reserve(visibleRenderables.size());

    const auto& instancedGroups = GetInstancedGroups(sceneVersion);
    for (const auto& group : instancedGroups) {
        if (!group.mesh || !group.shader) {
            continue;
        }

        InstanceBatchData batchData = BuildInstanceBatchData(group, visibleSet);
        const std::size_t instanceCount = batchData.modelMatrices.size();
        if (instanceCount == 0) {
            continue;
        }

        BatchKey key{group.mesh, group.shader, group.material ? group.material.get() : nullptr};
        auto& resources = GetOrCreateResources(key);
        UploadInstanceData(resources, batchData.modelMatrices, batchData.normalMatrices);

        group.shader->Use();
        group.shader->SetMat4("uView", view);
        group.shader->SetMat4("uProj", proj);
        group.shader->SetVec3("uViewPos", cam.Position());
        group.shader->SetInt("uUseInstanceBuffers", 1);

        if (group.material) {
            group.material->Apply(*group.shader);
        }

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, kInstanceModelBufferBinding, resources.modelBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, kInstanceNormalBufferBinding, resources.normalBuffer);
        group.mesh->DrawInstanced(static_cast<unsigned int>(instanceCount));
        group.shader->SetInt("uUseInstanceBuffers", 0);

        for (const auto& obj : batchData.gameObjects) {
            instancedObjects.insert(obj.get());
        }
    }

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, kInstanceModelBufferBinding, 0);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, kInstanceNormalBufferBinding, 0);

    for (const auto& gameObject : visibleRenderables) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }
        if (instancedObjects.find(gameObject.get()) != instancedObjects.end()) {
            continue;
        }
        gameObject->Render();
    }
}

const std::vector<RenderBatcher::InstancedGroup>& RenderBatcher::GetInstancedGroups(std::uint64_t sceneVersion) const {
    EnsureInstancedGroups(sceneVersion);
    return m_instancedGroups;
}

void RenderBatcher::MarkDirty() {
    m_instancedGroupsDirty = true;
}

RenderBatcher::Frustum RenderBatcher::CalculateFrustum(const glm::mat4& viewProj) const {
    Frustum frustum;
    const glm::mat4& m = viewProj;

    frustum.planes[0].plane.x = m[0][3] + m[0][0];
    frustum.planes[0].plane.y = m[1][3] + m[1][0];
    frustum.planes[0].plane.z = m[2][3] + m[2][0];
    frustum.planes[0].plane.w = m[3][3] + m[3][0];

    frustum.planes[1].plane.x = m[0][3] - m[0][0];
    frustum.planes[1].plane.y = m[1][3] - m[1][0];
    frustum.planes[1].plane.z = m[2][3] - m[2][0];
    frustum.planes[1].plane.w = m[3][3] - m[3][0];

    frustum.planes[2].plane.x = m[0][3] + m[0][1];
    frustum.planes[2].plane.y = m[1][3] + m[1][1];
    frustum.planes[2].plane.z = m[2][3] + m[2][1];
    frustum.planes[2].plane.w = m[3][3] + m[3][1];

    frustum.planes[3].plane.x = m[0][3] - m[0][1];
    frustum.planes[3].plane.y = m[1][3] - m[1][1];
    frustum.planes[3].plane.z = m[2][3] - m[2][1];
    frustum.planes[3].plane.w = m[3][3] - m[3][1];

    frustum.planes[4].plane.x = m[0][3] + m[0][2];
    frustum.planes[4].plane.y = m[1][3] + m[1][2];
    frustum.planes[4].plane.z = m[2][3] + m[2][2];
    frustum.planes[4].plane.w = m[3][3] + m[3][2];

    frustum.planes[5].plane.x = m[0][3] - m[0][2];
    frustum.planes[5].plane.y = m[1][3] - m[1][2];
    frustum.planes[5].plane.z = m[2][3] - m[2][2];
    frustum.planes[5].plane.w = m[3][3] - m[3][2];

    for (auto& plane : frustum.planes) {
        float length = std::sqrt(
            plane.plane.x * plane.plane.x +
            plane.plane.y * plane.plane.y +
            plane.plane.z * plane.plane.z);
        if (length > 0.0f) {
            plane.plane.x /= length;
            plane.plane.y /= length;
            plane.plane.z /= length;
            plane.plane.w /= length;
        }
    }

    return frustum;
}

bool RenderBatcher::IsInFrustum(const GameObject& obj, const Frustum& frustum) const {
    auto transform = obj.GetTransform();
    if (!transform) {
        return true;
    }

    if (obj.HasTag("terrain")) {
        return true;
    }

    glm::vec3 position = transform->GetPosition();
    glm::vec3 scale = transform->GetScale();

    float radius = std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)});
    if (radius < 0.1f) {
        radius = 0.5f;
    }

    for (int i = 0; i < 6; ++i) {
        const glm::vec4& plane = frustum.planes[i].plane;
        float distance = plane.x * position.x +
                         plane.y * position.y +
                         plane.z * position.z +
                         plane.w;
        if (distance < -radius) {
            return false;
        }
    }

    return true;
}

void RenderBatcher::EnsureInstancedGroups(std::uint64_t sceneVersion) const {
    if (!m_instancedGroupsDirty && m_instancedGroupsVersion == sceneVersion) {
        return;
    }

    m_instancedGroups.clear();

    std::unordered_map<BatchKey, std::size_t, BatchKeyHash> lookup;
    const auto& activeRenderables = m_scheduler.GetActiveRenderables();
    lookup.reserve(activeRenderables.size());

    for (const auto& gameObject : activeRenderables) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }

        auto meshComp = gameObject->GetComponent<gm::scene::StaticMeshComponent>();
        if (!meshComp || !meshComp->IsActive()) {
            continue;
        }

        Mesh* mesh = meshComp->GetMesh();
        Shader* shader = meshComp->GetShader();
        auto material = meshComp->GetMaterial();

        if (!mesh || !shader) {
            continue;
        }

        BatchKey key{mesh, shader, material.get()};
        auto [it, inserted] = lookup.try_emplace(key, m_instancedGroups.size());
        if (inserted) {
            InstancedGroup group;
            group.mesh = mesh;
            group.shader = shader;
            group.material = material;
            m_instancedGroups.push_back(std::move(group));
        }

        m_instancedGroups[it->second].objects.push_back(gameObject);
    }

    m_instancedGroupsDirty = false;
    m_instancedGroupsVersion = sceneVersion;
}

RenderBatcher::InstanceBatchData RenderBatcher::BuildInstanceBatchData(
    const InstancedGroup& group,
    const std::unordered_set<const GameObject*>& visibleSet) const {
    InstanceBatchData data;
    data.gameObjects.reserve(group.objects.size());
    data.modelMatrices.reserve(group.objects.size());
    data.normalMatrices.reserve(group.objects.size());

    for (const auto& gameObject : group.objects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }

        if (!visibleSet.empty() && visibleSet.find(gameObject.get()) == visibleSet.end()) {
            continue;
        }

        auto meshComp = gameObject->GetComponent<gm::scene::StaticMeshComponent>();
        if (!meshComp || !meshComp->IsActive()) {
            continue;
        }

        auto transform = gameObject->GetTransform();
        if (!transform) {
            continue;
        }

        glm::mat4 model = transform->GetMatrix();
        glm::mat3 normalMat3 = glm::mat3(glm::transpose(glm::inverse(model)));
        glm::mat4 normalMat4(1.0f);
        normalMat4[0] = glm::vec4(normalMat3[0], 0.0f);
        normalMat4[1] = glm::vec4(normalMat3[1], 0.0f);
        normalMat4[2] = glm::vec4(normalMat3[2], 0.0f);

        data.modelMatrices.push_back(model);
        data.normalMatrices.push_back(normalMat4);
        data.gameObjects.push_back(gameObject);
    }

    return data;
}

RenderBatcher::BatchGpuResources& RenderBatcher::GetOrCreateResources(const BatchKey& key) const {
    auto [it, inserted] = m_batchGpuCache.try_emplace(key);
    if (inserted) {
        glGenBuffers(1, &it->second.modelBuffer);
        glGenBuffers(1, &it->second.normalBuffer);
        it->second.capacity = 0;
    }
    return it->second;
}

void RenderBatcher::UploadInstanceData(BatchGpuResources& resources,
                                       const std::vector<glm::mat4>& models,
                                       const std::vector<glm::mat4>& normals) const {
    const std::size_t requiredCount = models.size();
    if (requiredCount == 0) {
        return;
    }

    if (resources.capacity < requiredCount) {
        resources.capacity = std::max<std::size_t>(requiredCount, resources.capacity == 0 ? requiredCount : resources.capacity * 2);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, resources.modelBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, resources.capacity * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, resources.normalBuffer);
        glBufferData(GL_SHADER_STORAGE_BUFFER, resources.capacity * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, resources.modelBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, requiredCount * sizeof(glm::mat4), models.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, resources.normalBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, requiredCount * sizeof(glm::mat4), normals.data());

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void RenderBatcher::DeleteResources(BatchGpuResources& resources) const {
    if (resources.modelBuffer) {
        glDeleteBuffers(1, &resources.modelBuffer);
        resources.modelBuffer = 0;
    }
    if (resources.normalBuffer) {
        glDeleteBuffers(1, &resources.normalBuffer);
        resources.normalBuffer = 0;
    }
    resources.capacity = 0;
}

} // namespace gm

