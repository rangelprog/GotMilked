#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/SceneSerializer.hpp"
#include "gm/scene/SceneSystem.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/LightManager.hpp"
#include "gm/core/Logger.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <future>
#include <thread>

namespace gm {

namespace {
constexpr std::string_view kGameObjectSystemName = "GameObjectUpdate";
} // namespace

class GameObjectUpdateSystem final : public SceneSystem {
public:
    std::string_view GetName() const override { return kGameObjectSystemName; }
    void Update(Scene& scene, float deltaTime) override {
        scene.UpdateGameObjects(deltaTime);
    }
};

Scene::Scene(const std::string& name)
    : sceneName(name) {
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
    if (!parallelGameObjectUpdatesEnabled) {
        for (auto& gameObject : gameObjects) {
            if (gameObject && gameObject->IsActive() && !gameObject->IsDestroyed()) {
                gameObject->Update(deltaTime);
            }
        }
        return;
    }

    const std::size_t count = gameObjects.size();
    if (count <= 1) {
        for (auto& gameObject : gameObjects) {
            if (gameObject && gameObject->IsActive() && !gameObject->IsDestroyed()) {
                gameObject->Update(deltaTime);
            }
        }
        return;
    }

    const unsigned int hwThreads = std::max(1u, std::thread::hardware_concurrency());
    const std::size_t workerCount = std::min<std::size_t>(hwThreads, count);

    const std::size_t chunkSize = (count + workerCount - 1) / workerCount;

    auto processRange = [this, deltaTime](std::size_t begin, std::size_t end) {
        for (std::size_t idx = begin; idx < end; ++idx) {
            auto& gameObject = gameObjects[idx];
            if (gameObject && gameObject->IsActive() && !gameObject->IsDestroyed()) {
                gameObject->Update(deltaTime);
            }
        }
    };

    std::vector<std::future<void>> workers;
    workers.reserve(workerCount > 0 ? workerCount - 1 : 0);

    std::size_t start = chunkSize;
    for (std::size_t worker = 1; worker < workerCount; ++worker, start += chunkSize) {
        const std::size_t begin = std::min(start, count);
        const std::size_t end = std::min(begin + chunkSize, count);
        if (begin >= end) {
            break;
        }

        workers.emplace_back(std::async(std::launch::async, processRange, begin, end));
    }

    const std::size_t primaryEnd = std::min(chunkSize, count);
    processRange(0, primaryEnd);

    for (auto& worker : workers) {
        if (worker.valid()) {
            worker.get();
        }
    }
}

void Scene::CleanupDestroyedObjects() {
    // Remove destroyed GameObjects
    auto it = std::remove_if(gameObjects.begin(), gameObjects.end(),
        [this](const std::shared_ptr<GameObject>& obj) {
            if (obj && obj->IsDestroyed()) {
                // Remove from tags map
                for (auto& [tag, objects] : objectsByTag) {
                    auto tagIt = std::remove(objects.begin(), objects.end(), obj);
                    objects.erase(tagIt, objects.end());
                }
                // Remove from name map
                auto nameIt = objectsByName.find(obj->GetName());
                if (nameIt != objectsByName.end() && nameIt->second == obj) {
                    objectsByName.erase(nameIt);
                }
                return true;
            }
            return false;
        });
    gameObjects.erase(it, gameObjects.end());
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

    gameObjects.clear();
    objectsByTag.clear();
    objectsByName.clear();
    isInitialized = false;
}

std::shared_ptr<GameObject> Scene::CreateGameObject(const std::string& name) {
    if (name.empty()) {
        core::Logger::Warning("[Scene] Creating GameObject with empty name");
    }
    
    // Check if name already exists
    if (objectsByName.find(name) != objectsByName.end()) {
        core::Logger::Warning("[Scene] GameObject with name '%s' already exists, returning existing object",
                              name.c_str());
        return objectsByName[name];
    }
    
    auto gameObject = std::make_shared<GameObject>(name);
    gameObjects.push_back(gameObject);
    objectsByName[name] = gameObject;
    return gameObject;
}

std::shared_ptr<GameObject> Scene::SpawnGameObject(const std::string& name) {
    auto gameObject = CreateGameObject(name);
    if (isInitialized) {
        gameObject->Init();
    }
    return gameObject;
}

void Scene::DestroyGameObject(std::shared_ptr<GameObject> gameObject) {
    if (!gameObject) {
        core::Logger::Warning("[Scene] Attempted to destroy null GameObject");
        return;
    }
    
    if (gameObject->IsDestroyed()) {
        core::Logger::Warning("[Scene] GameObject '%s' is already marked for destruction",
                              gameObject->GetName().c_str());
        return;
    }
    
    std::vector<std::string> existingTags;
    existingTags.reserve(gameObject->GetTags().size());
    for (const auto& tag : gameObject->GetTags()) {
        existingTags.push_back(tag);
    }

    for (const auto& tag : existingTags) {
        UntagGameObject(gameObject, tag);
    }

    const std::string& name = gameObject->GetName();
    if (!name.empty()) {
        auto nameIt = objectsByName.find(name);
        if (nameIt != objectsByName.end() && nameIt->second == gameObject) {
            objectsByName.erase(nameIt);
        }
    }

    gameObject->Destroy();
}

void Scene::DestroyGameObjectByName(const std::string& name) {
    auto it = objectsByName.find(name);
    if (it != objectsByName.end()) {
        it->second->Destroy();
    }
}

std::shared_ptr<GameObject> Scene::FindGameObjectByName(const std::string& name) {
    auto it = objectsByName.find(name);
    if (it != objectsByName.end()) {
        return it->second;
    }
    return nullptr;
}

void Scene::RegisterSystem(const SceneSystemPtr& system) {
    if (!system) {
        core::Logger::Warning("[Scene] Attempted to register null SceneSystem");
        return;
    }

    const std::string name{system->GetName()};
    if (name.empty()) {
        core::Logger::Warning("[Scene] SceneSystem with empty name ignored");
        return;
    }

    auto duplicate = std::find_if(systems.begin(), systems.end(),
        [&name](const SceneSystemPtr& existing) {
            return existing && existing->GetName() == name;
        });

    if (duplicate != systems.end()) {
        core::Logger::Warning("[Scene] SceneSystem '%s' already registered", name.c_str());
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
        core::Logger::Warning("[Scene] '%s' is a built-in system and cannot be unregistered",
                              name.data());
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

std::vector<std::shared_ptr<GameObject>> Scene::FindGameObjectsByTag(const std::string& tag) {
    auto it = objectsByTag.find(tag);
    if (it != objectsByTag.end()) {
        return it->second;
    }
    return {};
}

void Scene::TagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag) {
    if (!gameObject || tag.empty()) {
        return;
    }

    if (!gameObject->HasTag(tag)) {
        gameObject->AddTag(tag);
    }

    auto& bucket = objectsByTag[tag];
    auto exists = std::find(bucket.begin(), bucket.end(), gameObject);
    if (exists == bucket.end()) {
        bucket.push_back(gameObject);
    }
}

void Scene::UntagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag) {
    if (!gameObject || tag.empty()) return;

    if (gameObject->HasTag(tag)) {
        gameObject->RemoveTag(tag);
    }

    auto it = objectsByTag.find(tag);
    if (it != objectsByTag.end()) {
        auto& objects = it->second;
        auto objIt = std::find(objects.begin(), objects.end(), gameObject);
        if (objIt != objects.end()) {
            objects.erase(objIt);
        }
        if (objects.empty()) {
            objectsByTag.erase(it);
        }
    }
}

void Scene::Draw(Shader& shader, const Camera& cam, int fbw, int fbh, float fovDeg) {
    if (fbw <= 0 || fbh <= 0) {
        core::Logger::Warning("[Scene] Invalid framebuffer size (%d x %d)", fbw, fbh);
        return;
    }
    
    if (fovDeg <= 0.0f || fovDeg >= 180.0f) {
        core::Logger::Warning("[Scene] Invalid FOV: %.2f degrees (expected 0-180)", fovDeg);
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

    // Draw GameObjects
    for (const auto& gameObject : gameObjects) {
        if (gameObject && gameObject->IsActive() && !gameObject->IsDestroyed()) {
            gameObject->Render();
        }
    }
}

bool Scene::SaveToFile(const std::string& filepath) {
    return SceneSerializer::SaveToFile(*this, filepath);
}

bool Scene::LoadFromFile(const std::string& filepath) {
    return SceneSerializer::LoadFromFile(*this, filepath);
}

} // namespace gm
