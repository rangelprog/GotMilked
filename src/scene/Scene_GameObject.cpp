#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/core/Logger.hpp"
#include "gm/scene/TransformComponent.hpp"

#include <algorithm>
#include <cassert>
#include <iterator>

namespace gm {

namespace {
constexpr std::size_t kCleanupBatchThreshold = 12;
constexpr int kMaxCleanupDelayFrames = 4;
constexpr std::size_t kInitialObjectPoolCapacity = 64;
}

void Scene::GameObjectPool::Reserve(std::size_t capacity) {
    if (objects.capacity() < capacity) {
        objects.reserve(capacity);
    }
}

std::shared_ptr<GameObject> Scene::GameObjectPool::Acquire(Scene& owner, const std::string& name) {
    for (auto it = objects.rbegin(); it != objects.rend(); ++it) {
        if (!*it) {
            continue;
        }
        const auto refCount = it->use_count();
        if (refCount != 1) {
            core::Logger::Warning(
                "[Scene] Skipping pooled GameObject reuse because '{}' still has {} outstanding references",
                (*it)->GetName(), refCount - 1);
            continue;
        }
        std::shared_ptr<GameObject> gameObject = std::move(*it);
        objects.erase(std::next(it).base());
        GameObject* raw = gameObject.get();
        assert(raw && "Pooled GameObject must not be null");
        raw->ResetForReuse();
        raw->SetScene(&owner);
        raw->SetName(name);
        raw->SetActive(true);
        return gameObject;
    }

    auto gameObject = std::make_shared<GameObject>(name);
    gameObject->SetScene(&owner);
    return gameObject;
}

void Scene::GameObjectPool::Release(Scene& owner, std::shared_ptr<GameObject> gameObject) {
    (void)owner;
    if (!gameObject) {
        return;
    }
    const auto refCount = gameObject.use_count();
    if (refCount > 1) {
        core::Logger::Warning(
            "[Scene] GameObject '{}' still has {} outstanding references; skipping pool reuse",
            gameObject->GetName(), refCount - 1);
        return;
    }
    assert(refCount == 1 && "Releasing GameObject that is still referenced elsewhere");

    GameObject* raw = gameObject.get();
    raw->ResetForReuse();
    raw->SetScene(nullptr);

    Reserve(kInitialObjectPoolCapacity);
    objects.push_back(std::move(gameObject));
}

void Scene::GameObjectPool::Clear() {
    objects.clear();
}

void Scene::RemoveFromActiveLists(const std::shared_ptr<GameObject>& gameObject) {
    m_scheduler.RemoveFromActiveLists(gameObject);
}

std::shared_ptr<GameObject> Scene::CreateGameObject(const std::string& name) {
    std::string finalName = name;
    EnsureNameLookup();

    if (finalName.empty()) {
        finalName = GenerateUniqueName();
        core::Logger::Warning("[Scene] Creating GameObject with empty name. Assigned '{}'", finalName);
    } else {
        const std::string baseName = finalName;
        std::size_t suffix = 1;
        while (true) {
            auto existingIt = objectsByName.find(finalName);
            if (existingIt == objectsByName.end() || !existingIt->second || existingIt->second->IsDestroyed()) {
                break;
            }
            finalName = baseName + " (" + std::to_string(suffix++) + ")";
        }
        if (finalName != baseName) {
            core::Logger::Info("[Scene] Renamed duplicated GameObject '{}' to '{}'", baseName, finalName);
        }
    }

    auto gameObject = AcquireGameObject(finalName);
    gameObjects.push_back(gameObject);
    MarkNameLookupDirty();
    MarkActiveListsDirty();
    core::Logger::Debug("[Scene] Created GameObject '{}'", gameObject ? gameObject->GetName().c_str() : "<null>");
    return gameObject;
}

std::shared_ptr<GameObject> Scene::AcquireGameObject(const std::string& name) {
    auto acquired = m_gameObjectPool.Acquire(*this, name);
    if (!acquired) {
        core::Logger::Error("[Scene] AcquireGameObject returned null for '{}'", name);
    } else if (acquired->GetName().empty()) {
        core::Logger::Error("[Scene] AcquireGameObject yielded object with empty name (desired '{}')", name);
    }
    return acquired;
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
        core::Logger::Warning("[Scene] GameObject '{}' is already marked for destruction",
                              gameObject->GetName());
        return;
    }

    // Detach from parent before handling children
    if (auto parent = gameObject->GetParent()) {
        parent->RemoveChildInternal(gameObject);
    }
    gameObject->ClearParentInternal();

    // Recursively destroy children
    auto children = gameObject->GetChildren();
    for (auto& child : children) {
        DestroyGameObject(child);
    }
    gameObject->ClearChildrenInternal();

    std::vector<std::string> existingTags;
    existingTags.reserve(gameObject->GetTags().size());
    for (const auto& tag : gameObject->GetTags()) {
        existingTags.push_back(tag);
    }

    for (const auto& tag : existingTags) {
        UntagGameObject(gameObject, tag);
    }

    gameObject->Destroy();
    MarkActiveListsDirty();
    MarkNameLookupDirty();
    ++m_destroyedSinceLastCleanup;
}

void Scene::DestroyGameObjectByName(const std::string& name) {
    EnsureNameLookup();
    auto it = objectsByName.find(name);
    if (it != objectsByName.end() && it->second && !it->second->IsDestroyed()) {
        DestroyGameObject(it->second);
    }
}

bool Scene::SetParent(const std::shared_ptr<GameObject>& child, GameObject* newParent) {
    if (!child) {
        return false;
    }
    std::shared_ptr<GameObject> parentShared;
    if (newParent) {
        parentShared = FindGameObjectByPointer(newParent);
        if (!parentShared) {
            core::Logger::Warning("[Scene] SetParent could not resolve parent pointer");
            return false;
        }
    }
    return SetParent(child, parentShared);
}

bool Scene::SetParent(const std::shared_ptr<GameObject>& child, const std::shared_ptr<GameObject>& newParent) {
    if (!child) {
        return false;
    }
    if (child->GetSceneInternal() != this) {
        core::Logger::Warning("[Scene] Attempted to reparent GameObject '{}' that does not belong to this scene",
                              child->GetName());
        return false;
    }
    if (newParent && newParent->GetSceneInternal() != this) {
        core::Logger::Warning("[Scene] Attempted to assign parent '{}' from another scene",
                              newParent->GetName());
        return false;
    }
    if (newParent && newParent.get() == child.get()) {
        core::Logger::Warning("[Scene] Cannot parent GameObject '{}' to itself", child->GetName());
        return false;
    }
    if (newParent && newParent->IsDestroyed()) {
        core::Logger::Warning("[Scene] Cannot parent GameObject '{}' to destroyed parent '{}'",
                              child->GetName(), newParent->GetName());
        return false;
    }
    if (child->IsDestroyed()) {
        core::Logger::Warning("[Scene] Cannot parent destroyed GameObject '{}'", child->GetName());
        return false;
    }

    // Prevent cycles by walking up the ancestry chain
    if (newParent) {
        auto ancestor = newParent;
        while (ancestor) {
            if (ancestor == child) {
                core::Logger::Warning("[Scene] Cannot create cyclic parent-child relationship for '{}'",
                                      child->GetName());
                return false;
            }
            ancestor = ancestor->GetParent();
        }
    }

    auto currentParent = child->GetParent();
    if (currentParent == newParent) {
        return true;
    }

    glm::vec3 worldPosition{0.0f};
    glm::vec3 worldRotation{0.0f};
    glm::vec3 worldScale{1.0f};
    if (auto transform = child->GetTransform()) {
        worldPosition = transform->GetPosition();
        worldRotation = transform->GetRotation();
        worldScale = transform->GetScale();
    }

    if (currentParent) {
        currentParent->RemoveChildInternal(child);
    }
    child->ClearParentInternal();

    if (newParent) {
        child->SetParentInternal(newParent);
        newParent->AddChildInternal(child);
        newParent->MarkChildrenTransformsDirty();
    }

    if (auto transform = child->GetTransform()) {
        transform->SetPosition(worldPosition);
        transform->SetRotation(worldRotation);
        transform->SetScale(worldScale);
    } else {
        child->MarkChildrenTransformsDirty();
    }

    return true;
}

std::shared_ptr<GameObject> Scene::FindGameObjectByName(const std::string& name) {
    EnsureNameLookup();
    auto it = objectsByName.find(name);
    if (it != objectsByName.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<GameObject> Scene::FindGameObjectByPointer(const GameObject* ptr) {
    if (!ptr) {
        return nullptr;
    }
    for (const auto& obj : gameObjects) {
        if (obj && obj.get() == ptr) {
            return obj;
        }
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
    if (!gameObject || tag.empty()) {
        return;
    }

    if (!gameObject->HasTag(tag)) {
        gameObject->AddTag(tag);
    }

    auto& bucket = objectsByTag[tag];
    if (bucket.empty() && bucket.capacity() == 0) {
        bucket.reserve(4);
    }
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
    }
}

void Scene::EnsureNameLookup() {
    if (!m_nameLookupDirty) {
        return;
    }

    objectsByName.clear();

    for (const auto& gameObject : gameObjects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }

        const std::string& objName = gameObject->GetName();
        if (objName.empty()) {
            continue;
        }

        auto [it, inserted] = objectsByName.emplace(objName, gameObject);
        if (!inserted) {
            auto existing = it->second;
            if (existing && existing != gameObject) {
                core::Logger::Warning("[Scene] Duplicate GameObject name '{}' detected; keeping first instance",
                                      objName);
            }
        }
    }

    m_nameLookupDirty = false;
}

void Scene::HandleGameObjectRename(GameObject& object, const std::string& oldName, const std::string& newName) {
    if (&object == nullptr || oldName == newName) {
        return;
    }

    if (newName.empty()) {
        std::string generated = GenerateUniqueName();
        core::Logger::Warning("[Scene] Empty GameObject name detected after rename; assigning '{}'", generated);
        object.name = generated;
        MarkNameLookupDirty();
        return;
    }

    MarkNameLookupDirty();

    EnsureNameLookup();

    auto it = objectsByName.find(newName);
    if (it != objectsByName.end()) {
        auto existing = it->second;
        if (existing && existing.get() != &object) {
            core::Logger::Warning("[Scene] GameObject rename conflict: '{}' already exists; keeping first instance",
                                  newName);
        }
    }
}

void Scene::ResetCleanupCounters() {
    m_destroyedSinceLastCleanup = 0;
    m_framesSinceLastCleanup = 0;
}

void Scene::CleanupDestroyedObjects() {
    if (m_destroyedSinceLastCleanup == 0) {
        return;
    }

    ++m_framesSinceLastCleanup;

    if (m_destroyedSinceLastCleanup < kCleanupBatchThreshold &&
        m_framesSinceLastCleanup < kMaxCleanupDelayFrames) {
        return;
    }

    std::vector<std::shared_ptr<GameObject>> recycledObjects;
    bool removedAny = false;

    for (auto it = gameObjects.begin(); it != gameObjects.end();) {
        auto& obj = *it;
        if (obj && obj->IsDestroyed()) {
            obj->ClearChildrenInternal();
            obj->ClearParentInternal();
            for (auto& [tag, objects] : objectsByTag) {
                auto tagIt = std::remove(objects.begin(), objects.end(), obj);
                objects.erase(tagIt, objects.end());
            }

            const std::string& objName = obj->GetName();
            if (!objName.empty()) {
                auto nameIt = objectsByName.find(objName);
                if (nameIt != objectsByName.end() && nameIt->second == obj) {
                    objectsByName.erase(nameIt);
                }
            }

            RemoveFromActiveLists(obj);
            obj->SetScene(nullptr);
            recycledObjects.push_back(std::move(obj));
            it = gameObjects.erase(it);
            removedAny = true;
        } else {
            ++it;
        }
    }

    if (removedAny) {
        MarkActiveListsDirty();
        MarkActiveListsDirty();
        MarkNameLookupDirty();
    }

    for (auto& recycled : recycledObjects) {
        ReleaseGameObject(std::move(recycled));
    }

    ResetCleanupCounters();
}

void Scene::ReleaseGameObject(std::shared_ptr<GameObject> gameObject) {
    m_gameObjectPool.Release(*this, std::move(gameObject));
}

void Scene::ClearObjectPool() {
    m_gameObjectPool.Clear();
}

std::string Scene::GenerateUniqueName() {
    return "GameObject_" + std::to_string(++m_unnamedObjectCounter);
}

std::vector<std::shared_ptr<GameObject>> Scene::GetRootGameObjects() const {
    std::vector<std::shared_ptr<GameObject>> roots;
    roots.reserve(gameObjects.size());
    for (const auto& obj : gameObjects) {
        if (obj && !obj->IsDestroyed() && !obj->HasParent()) {
            roots.push_back(obj);
        }
    }
    return roots;
}

} // namespace gm
