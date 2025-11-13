#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/SceneSystem.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/LightManager.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/core/Logger.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <future>
#include <thread>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <cassert>
#include <iterator>

namespace gm {

namespace {
constexpr std::string_view kGameObjectSystemName = "GameObjectUpdate";
constexpr std::size_t kCleanupBatchThreshold = 12;
constexpr int kMaxCleanupDelayFrames = 4;
constexpr std::size_t kInitialObjectPoolCapacity = 64;
} // namespace

class GameObjectUpdateSystem final : public SceneSystem {
public:
    std::string_view GetName() const override { return kGameObjectSystemName; }
    void Update(Scene& scene, float deltaTime) override {
        scene.UpdateGameObjects(deltaTime);
    }
};

Scene::Scene(const std::string& name)
    : sceneName(name)
    , m_updateThreadPool(std::max<std::size_t>(1, std::thread::hardware_concurrency())) {
    m_gameObjectPool.Reserve(kInitialObjectPoolCapacity);
    RegisterSystem(std::make_shared<GameObjectUpdateSystem>());
}

void Scene::Init() {
    if (isInitialized) return;
    
    InitializeSystems();
    
    for (auto& gameObject : gameObjects) {
        if (gameObject && gameObject->IsActive()) {
            gameObject->Init();
        }
    }
    
    for (auto& system : systems) {
        if (system) {
            system->OnSceneInit(*this);
        }
    }
    
    isInitialized = true;
}

void Scene::Update(float deltaTime) {
    if (isPaused) return;

    InitializeSystems();

    RunSystems(deltaTime);
    CleanupDestroyedObjects();
}

void Scene::UpdateGameObjects(float deltaTime) {
    UpdateActiveLists();  // Ensure lists are up to date
    
    if (!parallelGameObjectUpdatesEnabled) {
        for (auto& gameObject : m_activeUpdatables) {
            if (gameObject && !gameObject->IsDestroyed()) {
                gameObject->Update(deltaTime);
            }
        }
        return;
    }

    const std::size_t count = m_activeUpdatables.size();
    if (count <= 1) {
        for (auto& gameObject : m_activeUpdatables) {
            if (gameObject && !gameObject->IsDestroyed()) {
                gameObject->Update(deltaTime);
            }
        }
        return;
    }

    const std::size_t poolThreads = std::max<std::size_t>(1, m_updateThreadPool.ThreadCount());
    const std::size_t workerCount = std::max<std::size_t>(1, std::min<std::size_t>(poolThreads, count));

    const std::size_t chunkSize = (count + workerCount - 1) / workerCount;

    auto processRange = [this, deltaTime](std::size_t begin, std::size_t end) {
        for (std::size_t idx = begin; idx < end; ++idx) {
            auto& gameObject = m_activeUpdatables[idx];
            if (gameObject && !gameObject->IsDestroyed()) {
                gameObject->Update(deltaTime);
            }
        }
    };

    std::vector<std::future<void>> tasks;
    if (workerCount > 1) {
        tasks.reserve(workerCount - 1);
        std::size_t start = chunkSize;
        for (std::size_t worker = 1; worker < workerCount; ++worker, start += chunkSize) {
            const std::size_t begin = std::min(start, count);
            const std::size_t end = std::min(begin + chunkSize, count);
            if (begin >= end) {
                break;
            }
            tasks.emplace_back(m_updateThreadPool.Submit(processRange, begin, end));
        }
    }

    const std::size_t primaryEnd = std::min(chunkSize, count);
    processRange(0, primaryEnd);

    for (auto& task : tasks) {
        task.get();
    }
}

void Scene::UpdateActiveLists() {
    // Only rebuild if dirty (objects added/removed or active state changed)
    if (!m_activeListsDirty) {
        return;
    }
    m_instancedGroupsDirty = true;
    
    m_activeRenderables.clear();
    m_activeUpdatables.clear();
    
    // Reserve capacity to avoid reallocations
    m_activeRenderables.reserve(gameObjects.size());
    m_activeUpdatables.reserve(gameObjects.size());
    
    for (const auto& gameObject : gameObjects) {
        if (gameObject && !gameObject->IsDestroyed()) {
            if (gameObject->IsActive()) {
                m_activeRenderables.push_back(gameObject);
                m_activeUpdatables.push_back(gameObject);
            }
        }
    }
    
    m_activeListsDirty = false;
}

const std::vector<std::shared_ptr<GameObject>>& Scene::GetActiveRenderables() {
    UpdateActiveLists();
    return m_activeRenderables;
}

const std::vector<Scene::InstancedGroup>& Scene::GetInstancedGroups() const {
    EnsureInstancedGroups();
    return m_instancedGroups;
}

void Scene::EnsureInstancedGroups() const {
    if (!m_instancedGroupsDirty && m_instancedGroupsVersion == m_reloadVersion) {
        return;
    }

    const_cast<Scene*>(this)->UpdateActiveLists();

    m_instancedGroups.clear();

    struct BatchKey {
        Mesh* mesh = nullptr;
        Shader* shader = nullptr;
        const Material* material = nullptr;

        bool operator==(const BatchKey& other) const {
            return mesh == other.mesh && shader == other.shader && material == other.material;
        }
    };

    struct BatchKeyHash {
        std::size_t operator()(const BatchKey& key) const {
            std::size_t h1 = std::hash<Mesh*>{}(key.mesh);
            std::size_t h2 = std::hash<Shader*>{}(key.shader);
            std::size_t h3 = std::hash<const Material*>{}(key.material);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    std::unordered_map<BatchKey, std::size_t, BatchKeyHash> lookup;
    lookup.reserve(m_activeRenderables.size());

    for (const auto& gameObject : m_activeRenderables) {
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

        BatchKey key{ mesh, shader, material.get() };
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
    m_instancedGroupsVersion = m_reloadVersion;
}

void Scene::InitializeSystems() {
    if (systemsInitialized) {
        return;
    }
    for (auto& system : systems) {
        if (system) {
            system->OnRegister(*this);
            if (isInitialized) {
                system->OnSceneInit(*this);
            }
        }
    }
    systemsInitialized = true;
}

void Scene::ShutdownSystems() {
    if (!systemsInitialized) {
        return;
    }
    for (auto& system : systems) {
        if (system) {
            system->OnUnregister(*this);
        }
    }
    systemsInitialized = false;
}

void Scene::RunSystems(float deltaTime) {
    std::vector<std::future<void>> asyncJobs;
    asyncJobs.reserve(systems.size());

    for (auto& system : systems) {
        if (!system) {
            continue;
        }

        if (system->RunsAsync()) {
            SceneSystemPtr sys = system;
            asyncJobs.emplace_back(std::async(std::launch::async, [this, sys, deltaTime]() {
                sys->Update(*this, deltaTime);
            }));
        } else {
            system->Update(*this, deltaTime);
        }
    }

    for (auto& job : asyncJobs) {
        if (job.valid()) {
            job.get();
        }
    }
}

void Scene::Cleanup() {
    if (isInitialized) {
        for (auto& system : systems) {
            if (system) {
                system->OnSceneShutdown(*this);
            }
        }
        isInitialized = false;
    }

    ShutdownSystems();

    m_activeRenderables.clear();
    m_activeUpdatables.clear();
    objectsByTag.clear();
    objectsByName.clear();
    m_activeListsDirty = true;
    m_nameLookupDirty = true;

    for (auto& obj : gameObjects) {
        if (obj) {
            ReleaseGameObject(std::move(obj));
        }
    }
    gameObjects.clear();
    ResetCleanupCounters();
    isInitialized = false;
    ClearObjectPool();
}

void Scene::RegisterSystem(const SceneSystemPtr& system) {
    if (!system) {
        core::Logger::Warning("[Scene] Attempted to register null SceneSystem");
        return;
    }

    std::string_view nameView = system->GetName();
    if (nameView.empty()) {
        core::Logger::Warning("[Scene] SceneSystem with empty name ignored");
        return;
    }

    // Compare string_view directly to avoid string copies
    auto duplicate = std::find_if(systems.begin(), systems.end(),
        [nameView](const SceneSystemPtr& existing) {
            return existing && existing->GetName() == nameView;
        });

    if (duplicate != systems.end()) {
        core::Logger::Warning("[Scene] SceneSystem '{}' already registered", nameView);
        return;
    }

    systems.push_back(system);

    if (systemsInitialized) {
        system->OnRegister(*this);
        if (isInitialized) {
            system->OnSceneInit(*this);
        }
    }
}

bool Scene::UnregisterSystem(std::string_view name) {
    if (name == kGameObjectSystemName) {
        core::Logger::Warning("[Scene] '{}' is a built-in system and cannot be unregistered",
                              name);
        return false;
    }

    auto it = std::find_if(systems.begin(), systems.end(),
        [name](const SceneSystemPtr& system) {
            return system && system->GetName() == name;
        });

    if (it == systems.end()) {
        return false;
    }

    if (isInitialized && *it) {
        (*it)->OnSceneShutdown(*this);
    }
    if (systemsInitialized && *it) {
        (*it)->OnUnregister(*this);
    }

    systems.erase(it);
    return true;
}

void Scene::ClearSystems() {
    if (isInitialized) {
        for (auto& system : systems) {
            if (system) {
                system->OnSceneShutdown(*this);
            }
        }
    }
    ShutdownSystems();

    systems.clear();
    RegisterSystem(std::make_shared<GameObjectUpdateSystem>());
}

void Scene::Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg) {
    if (fbw <= 0 || fbh <= 0) {
        core::Logger::Warning("[Scene] Invalid framebuffer size ({} x {})", fbw, fbh);
        return;
    }
    
    if (fovDeg <= 0.0f || fovDeg >= 180.0f) {
        core::Logger::Warning("[Scene] Invalid FOV: {:.2f} degrees (expected 0-180)", fovDeg);
        return;
    }

    const float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
    glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 100.0f);
    glm::mat4 view = cam.View();

    shader.Use();
    shader.SetMat4("uView", view);
    shader.SetMat4("uProj", proj);
    shader.SetVec3("uViewPos", cam.Position());

    // Collect and apply lights (using cached LightManager)
    m_lightManager.CollectLights(gameObjects);
    m_lightManager.ApplyLights(shader, cam.Position());

    // Draw GameObjects (only iterate over active renderables)
    UpdateActiveLists();  // Ensure lists are up to date
    
    // Calculate frustum for culling if enabled
    Frustum frustum;
    const Frustum* frustumPtr = nullptr;
    if (m_frustumCullingEnabled) {
        glm::mat4 viewProj = proj * view;
        frustum = CalculateFrustum(viewProj);
        frustumPtr = &frustum;
    }
    
    // Try instanced rendering first if enabled
    if (m_instancedRenderingEnabled) {
        std::vector<InstancedBatch> batches;
        CollectInstancedBatches(batches, frustumPtr);

        std::unordered_set<const GameObject*> instancedObjects;
        instancedObjects.reserve(batches.size() * 4);
        
        // Render instanced batches
        for (const auto& batch : batches) {
            if (batch.modelMatrices.size() > 1) {
                // Use instanced rendering for batches with multiple instances
                RenderInstancedBatch(batch, shader, cam);
                for (const auto& obj : batch.gameObjects) {
                    if (obj) {
                        instancedObjects.insert(obj.get());
                    }
                }
            } else if (batch.modelMatrices.size() == 1) {
                // Single instance, render normally
                if (!batch.gameObjects.empty() && batch.gameObjects[0]) {
                    batch.gameObjects[0]->Render();
                    instancedObjects.insert(batch.gameObjects[0].get());
                }
            }
        }
        
        // Render non-instanced objects (those without StaticMeshComponent or unique combinations)
        for (const auto& gameObject : m_activeRenderables) {
            if (gameObject && !gameObject->IsDestroyed()) {
                if (instancedObjects.find(gameObject.get()) != instancedObjects.end()) {
                    continue;
                }
                // Skip frustum culling if disabled, or render if in frustum
                if (!m_frustumCullingEnabled || !frustumPtr || IsInFrustum(*gameObject, *frustumPtr)) {
                    gameObject->Render();
                }
            }
        }
    } else {
        // Fallback to individual rendering
        for (const auto& gameObject : m_activeRenderables) {
            if (gameObject && !gameObject->IsDestroyed()) {
                // Skip frustum culling if disabled, or render if in frustum
                if (!m_frustumCullingEnabled || !frustumPtr || IsInFrustum(*gameObject, *frustumPtr)) {
                    gameObject->Render();
                }
            }
        }
    }
}

bool Scene::SaveToFile(const std::string& filepath) {
    return SceneSerializer::SaveToFile(*this, filepath);
}

bool Scene::LoadFromFile(const std::string& filepath) {
    bool result = SceneSerializer::LoadFromFile(*this, filepath);
    if (result) {
        // Mark active lists as dirty after loading (objects may have different active states)
        m_activeListsDirty = true;
        MarkNameLookupDirty();
        ResetCleanupCounters();
    }
    return result;
}

Scene::Frustum Scene::CalculateFrustum(const glm::mat4& viewProj) const {
    Frustum frustum;
    
    // Extract frustum planes from view-projection matrix
    // Each plane is stored as (a, b, c, d) where ax + by + cz + d = 0
    const glm::mat4& m = viewProj;
    
    // Left plane
    frustum.planes[0].plane.x = m[0][3] + m[0][0];
    frustum.planes[0].plane.y = m[1][3] + m[1][0];
    frustum.planes[0].plane.z = m[2][3] + m[2][0];
    frustum.planes[0].plane.w = m[3][3] + m[3][0];
    
    // Right plane
    frustum.planes[1].plane.x = m[0][3] - m[0][0];
    frustum.planes[1].plane.y = m[1][3] - m[1][0];
    frustum.planes[1].plane.z = m[2][3] - m[2][0];
    frustum.planes[1].plane.w = m[3][3] - m[3][0];
    
    // Bottom plane
    frustum.planes[2].plane.x = m[0][3] + m[0][1];
    frustum.planes[2].plane.y = m[1][3] + m[1][1];
    frustum.planes[2].plane.z = m[2][3] + m[2][1];
    frustum.planes[2].plane.w = m[3][3] + m[3][1];
    
    // Top plane
    frustum.planes[3].plane.x = m[0][3] - m[0][1];
    frustum.planes[3].plane.y = m[1][3] - m[1][1];
    frustum.planes[3].plane.z = m[2][3] - m[2][1];
    frustum.planes[3].plane.w = m[3][3] - m[3][1];
    
    // Near plane
    frustum.planes[4].plane.x = m[0][3] + m[0][2];
    frustum.planes[4].plane.y = m[1][3] + m[1][2];
    frustum.planes[4].plane.z = m[2][3] + m[2][2];
    frustum.planes[4].plane.w = m[3][3] + m[3][2];
    
    // Far plane
    frustum.planes[5].plane.x = m[0][3] - m[0][2];
    frustum.planes[5].plane.y = m[1][3] - m[1][2];
    frustum.planes[5].plane.z = m[2][3] - m[2][2];
    frustum.planes[5].plane.w = m[3][3] - m[3][2];
    
    // Normalize all planes
    for (int i = 0; i < 6; ++i) {
        float length = std::sqrt(
            frustum.planes[i].plane.x * frustum.planes[i].plane.x +
            frustum.planes[i].plane.y * frustum.planes[i].plane.y +
            frustum.planes[i].plane.z * frustum.planes[i].plane.z
        );
        
        if (length > 0.0f) {
            frustum.planes[i].plane.x /= length;
            frustum.planes[i].plane.y /= length;
            frustum.planes[i].plane.z /= length;
            frustum.planes[i].plane.w /= length;
        }
    }
    
    return frustum;
}

bool Scene::IsInFrustum(const GameObject& obj, const Frustum& frustum) const {
    // Get transform to get position
    auto transform = obj.GetTransform();
    if (!transform) {
        // No transform means no position, skip culling (render it)
        return true;
    }
    
    // Skip frustum culling for terrain objects (they're typically large and always visible)
    if (obj.HasTag("terrain")) {
        return true;
    }
    
    glm::vec3 position = transform->GetPosition();
    glm::vec3 scale = transform->GetScale();
    
    // Estimate bounding sphere radius from scale
    // Use the maximum scale component as a simple radius estimate
    float radius = std::max({std::abs(scale.x), std::abs(scale.y), std::abs(scale.z)});
    
    // Default radius if scale is too small (for point-like objects)
    if (radius < 0.1f) {
        radius = 0.5f;  // Default 0.5 unit radius
    }
    
    // Test sphere against all 6 frustum planes
    for (int i = 0; i < 6; ++i) {
        const glm::vec4& plane = frustum.planes[i].plane;
        
        // Distance from sphere center to plane
        float distance = plane.x * position.x + 
                        plane.y * position.y + 
                        plane.z * position.z + 
                        plane.w;
        
        // If sphere is completely outside plane (behind it), object is culled
        if (distance < -radius) {
            return false;
        }
    }
    
    // Sphere intersects or is inside all planes
    return true;
}

void Scene::CollectInstancedBatches(std::vector<InstancedBatch>& batches, const Frustum* frustum) const {
    EnsureInstancedGroups();

    batches.reserve(m_instancedGroups.size());

    for (const auto& group : m_instancedGroups) {
        if (group.objects.empty()) {
            continue;
        }

        InstancedBatch batch;
        batch.mesh = group.mesh;
        batch.shader = group.shader;
        batch.material = group.material;

        const size_t estimatedInstances = group.objects.size();
        batch.modelMatrices.reserve(estimatedInstances);
        batch.normalMatrices.reserve(estimatedInstances);
        batch.gameObjects.reserve(estimatedInstances);

        for (const auto& gameObject : group.objects) {
            if (!gameObject || gameObject->IsDestroyed()) {
                continue;
            }

            if (frustum && !IsInFrustum(*gameObject, *frustum)) {
                continue;
            }

            auto meshComp = gameObject->GetComponent<gm::scene::StaticMeshComponent>();
            if (!meshComp || !meshComp->IsActive()) {
                continue;
            }

            if (!meshComp->GetMesh() || !meshComp->GetShader()) {
                continue;
            }

            auto transform = gameObject->GetTransform();
            if (!transform) {
                continue;
            }

            glm::mat4 model = transform->GetMatrix();
            glm::mat3 normalMat = glm::mat3(glm::transpose(glm::inverse(model)));
            batch.modelMatrices.push_back(model);
            batch.normalMatrices.push_back(normalMat);
            batch.gameObjects.push_back(gameObject);
        }

        if (!batch.modelMatrices.empty()) {
            batches.push_back(std::move(batch));
        }
    }
}

void Scene::RenderInstancedBatch(const InstancedBatch& batch, Shader& shader, const Camera& cam) const {
    if (!batch.mesh || !batch.shader || batch.modelMatrices.empty()) {
        return;
    }
    
    // Use the batch's shader
    batch.shader->Use();
    
    // Set camera position
    batch.shader->SetVec3("uViewPos", cam.Position());
    
    // Apply material if present
    if (batch.material) {
        batch.material->Apply(*batch.shader);
    }
    
    // For now, render each instance individually but with shared state setup
    // This provides some batching benefit (shared shader/material setup)
    // TODO: Implement proper GPU instanced rendering with VAO modification
    const size_t instanceCount = batch.modelMatrices.size();
    for (size_t i = 0; i < instanceCount; ++i) {
        batch.shader->SetMat4("uModel", batch.modelMatrices[i]);
        batch.shader->SetMat3("uNormalMat", batch.normalMatrices[i]);
        
        // Get camera from gameObject if available (for per-object view position)
        if (i < batch.gameObjects.size() && batch.gameObjects[i]) {
            auto meshComp = batch.gameObjects[i]->GetComponent<gm::scene::StaticMeshComponent>();
            if (meshComp && meshComp->GetCamera()) {
                batch.shader->SetVec3("uViewPos", meshComp->GetCamera()->Position());
            }
        }
        
        batch.mesh->Draw();
    }
}

} // namespace gm
