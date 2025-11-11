#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/rendering/LightManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace gm {

Scene::Scene(const std::string& name) : sceneName(name) {}

void Scene::Init() {
    if (isInitialized) return;
    
    for (auto& gameObject : gameObjects) {
        if (gameObject && gameObject->IsActive()) {
            gameObject->Init();
        }
    }
    
    isInitialized = true;
}

void Scene::Update(float deltaTime) {
    if (isPaused) return;

    UpdateGameObjects(deltaTime);
    CleanupDestroyedObjects();
}

void Scene::UpdateGameObjects(float deltaTime) {
    for (auto& gameObject : gameObjects) {
        if (gameObject && gameObject->IsActive() && !gameObject->IsDestroyed()) {
            gameObject->Update(deltaTime);
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

void Scene::Cleanup() {
    gameObjects.clear();
    objectsByTag.clear();
    objectsByName.clear();
    isInitialized = false;
}

std::shared_ptr<GameObject> Scene::CreateGameObject(const std::string& name) {
    if (name.empty()) {
        printf("[Scene] Warning: Creating GameObject with empty name\n");
    }
    
    // Check if name already exists
    if (objectsByName.find(name) != objectsByName.end()) {
        printf("[Scene] Warning: GameObject with name '%s' already exists, returning existing object\n", name.c_str());
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
        printf("[Scene] Warning: Attempted to destroy null GameObject\n");
        return;
    }
    
    if (gameObject->IsDestroyed()) {
        printf("[Scene] Warning: GameObject '%s' is already marked for destruction\n", gameObject->GetName().c_str());
        return;
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

std::vector<std::shared_ptr<GameObject>> Scene::FindGameObjectsByTag(const std::string& tag) {
    auto it = objectsByTag.find(tag);
    if (it != objectsByTag.end()) {
        return it->second;
    }
    return {};
}

void Scene::TagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag) {
    if (!gameObject) return;
    gameObject->AddTag(tag);
    objectsByTag[tag].push_back(gameObject);
}

void Scene::UntagGameObject(std::shared_ptr<GameObject> gameObject, const std::string& tag) {
    if (!gameObject) return;
    gameObject->RemoveTag(tag);
    
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
        printf("[Scene] Warning: Invalid framebuffer size (%d x %d)\n", fbw, fbh);
        return;
    }
    
    if (fovDeg <= 0.0f || fovDeg >= 180.0f) {
        printf("[Scene] Warning: Invalid FOV: %.2f degrees (expected 0-180)\n", fovDeg);
        return;
    }

    const float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
    glm::mat4 proj = glm::perspective(glm::radians(fovDeg), aspect, 0.1f, 100.0f);
    glm::mat4 view = cam.View();

    shader.Use();

    // Collect and apply lights
    LightManager lightManager;
    lightManager.CollectLights(gameObjects);
    lightManager.ApplyLights(shader, cam.Position());

    // Draw GameObjects
    for (const auto& gameObject : gameObjects) {
        if (gameObject && gameObject->IsActive() && !gameObject->IsDestroyed()) {
            gameObject->Render();
        }
    }
}

} // namespace gm
