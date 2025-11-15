#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/SceneSystem.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/StaticMeshComponent.hpp"
#include "gm/scene/AnimationSystem.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/LightManager.hpp"
#include "gm/rendering/Mesh.hpp"
#include "gm/rendering/Material.hpp"
#include "gm/core/Logger.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

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
    , m_scheduler(*this)
    , m_renderBatcher(*this, m_scheduler)
    , m_lifecycle(*this) {
    m_gameObjectPool.Reserve(kInitialObjectPoolCapacity);
    m_scheduler.BindSource(&gameObjects);
    RegisterSystem(std::make_shared<GameObjectUpdateSystem>());
    RegisterSystem(std::make_shared<scene::AnimationSystem>());
}

void Scene::Init() {
    if (isInitialized) return;
    
    m_lifecycle.InitializeSystems();
    
    for (auto& gameObject : gameObjects) {
        if (gameObject && gameObject->IsActive()) {
            gameObject->Init();
        }
    }
    
    m_lifecycle.OnSceneInit();
    
    isInitialized = true;
}

void Scene::Update(float deltaTime) {
    if (isPaused) return;

    m_lifecycle.InitializeSystems();

    RunSystems(deltaTime);
    CleanupDestroyedObjects();
}

void Scene::UpdateGameObjects(float deltaTime) {
    m_scheduler.UpdateGameObjects(deltaTime);
}

const std::vector<std::shared_ptr<GameObject>>& Scene::GetActiveRenderables() {
    return m_scheduler.GetActiveRenderables();
}

const std::vector<Scene::InstancedGroup>& Scene::GetInstancedGroups() const {
    return m_renderBatcher.GetInstancedGroups(m_reloadVersion);
}

void Scene::MarkActiveListsDirty() {
    m_scheduler.MarkActiveListsDirty();
    m_renderBatcher.MarkDirty();
}

void Scene::BumpReloadVersion() {
    ++m_reloadVersion;
    MarkActiveListsDirty();
}

void Scene::InitializeSystems() {
    m_lifecycle.InitializeSystems();
}

void Scene::ShutdownSystems() {
    m_lifecycle.ShutdownSystems();
}

void Scene::RunSystems(float deltaTime) {
    m_lifecycle.RunSystems(deltaTime);
}

void Scene::Cleanup() {
    if (isInitialized) {
        m_lifecycle.OnSceneShutdown();
        isInitialized = false;
    }

    ShutdownSystems();

    objectsByTag.clear();
    objectsByName.clear();
    m_nameLookupDirty = true;
    MarkActiveListsDirty();

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
    m_lifecycle.RegisterSystem(system);
}

bool Scene::UnregisterSystem(std::string_view name) {
    if (name == kGameObjectSystemName) {
        core::Logger::Warning("[Scene] '{}' is a built-in system and cannot be unregistered",
                              name);
        return false;
    }
    return m_lifecycle.UnregisterSystem(name);
}

void Scene::ClearSystems() {
    if (isInitialized) {
        m_lifecycle.OnSceneShutdown();
    }
    ShutdownSystems();

    m_lifecycle.ClearSystems();
    RegisterSystem(std::make_shared<GameObjectUpdateSystem>());
    RegisterSystem(std::make_shared<scene::AnimationSystem>());
}

void Scene::Draw(Shader& shader,
                 const Camera& cam,
                 int fbw,
                 int fbh,
                 float fovDeg,
                 float nearPlane,
                 float farPlane) {
    if (fbw <= 0 || fbh <= 0) {
        core::Logger::Warning("[Scene] Invalid framebuffer size ({} x {})", fbw, fbh);
        return;
    }

    if (fovDeg <= 0.0f || fovDeg >= 180.0f) {
        core::Logger::Warning("[Scene] Invalid FOV: {:.2f} degrees (expected 0-180)", fovDeg);
        return;
    }

    if (nearPlane <= 0.0f || nearPlane >= farPlane) {
        core::Logger::Warning("[Scene] Invalid clip distances: near={}, far={}", nearPlane, farPlane);
        nearPlane = 0.1f;
        farPlane = 100.0f;
    }

    const float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
    glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, nearPlane, farPlane);
    glm::mat4 view = cam.View();

    SetRenderContext(view, proj, cam.Position());

    shader.Use();
    shader.SetMat4("uView", view);
    shader.SetMat4("uProj", proj);
    shader.SetVec3("uViewPos", cam.Position());

    m_lightManager.CollectLights(gameObjects);
    m_lightManager.ApplyLights(shader, cam.Position());

    m_renderBatcher.Draw(cam, view, proj, m_reloadVersion);
    ClearRenderContext();
}

void Scene::SetRenderContext(const glm::mat4& view,
                             const glm::mat4& proj,
                             const glm::vec3& camPos) {
    m_renderView = view;
    m_renderProj = proj;
    m_renderCameraPos = camPos;
    m_renderContextValid = true;
}

void Scene::ClearRenderContext() {
    m_renderContextValid = false;
    m_renderView = glm::mat4(1.0f);
    m_renderProj = glm::mat4(1.0f);
    m_renderCameraPos = glm::vec3(0.0f);
}

bool Scene::SaveToFile(const std::string& filepath) {
    return SceneSerializer::SaveToFile(*this, filepath);
}

bool Scene::LoadFromFile(const std::string& filepath) {
    bool result = SceneSerializer::LoadFromFile(*this, filepath);
    if (result) {
        // Mark active lists as dirty after loading (objects may have different active states)
        MarkActiveListsDirty();
        MarkNameLookupDirty();
        ResetCleanupCounters();
    }
    return result;
}

} // namespace gm
