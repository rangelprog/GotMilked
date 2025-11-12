#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/core/Logger.hpp"

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

static void RemoveFromVector(std::vector<std::shared_ptr<GameObject>>& vec,
                              const std::shared_ptr<GameObject>& gameObject) {
    vec.erase(std::remove(vec.begin(), vec.end(), gameObject), vec.end());
}

void Scene::RemoveFromActiveLists(const std::shared_ptr<GameObject>& gameObject) {
    RemoveFromVector(m_activeRenderables, gameObject);
    RemoveFromVector(m_activeUpdatables, gameObject);
}

std::shared_ptr<GameObject> Scene::CreateGameObject(const std::string& name) {
    std::string finalName = name;
    if (finalName.empty()) {
        finalName = GenerateUniqueName();
        core::Logger::Warning("[Scene] Creating GameObject with empty name. Assigned '{}'", finalName);
    }

    EnsureNameLookup();

    if (!finalName.empty()) {
        auto existingIt = objectsByName.find(finalName);
        if (existingIt != objectsByName.end() && existingIt->second) {
            core::Logger::Warning("[Scene] GameObject with name '{}' already exists, returning existing object",
                                  finalName);
            return existingIt->second;
        }
    }
    auto gameObject = AcquireGameObject(finalName);
    gameObjects.push_back(gameObject);
    MarkNameLookupDirty();
    m_activeListsDirty = true;
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

    std::vector<std::string> existingTags;
    existingTags.reserve(gameObject->GetTags().size());
    for (const auto& tag : gameObject->GetTags()) {
        existingTags.push_back(tag);
    }

    for (const auto& tag : existingTags) {
        UntagGameObject(gameObject, tag);
    }

    gameObject->Destroy();
    m_activeListsDirty = true;
    MarkNameLookupDirty();
    ++m_destroyedSinceLastCleanup;
}

void Scene::DestroyGameObjectByName(const std::string& name) {
    EnsureNameLookup();
    auto it = objectsByName.find(name);
    if (it != objectsByName.end() && it->second && !it->second->IsDestroyed()) {
        it->second->Destroy();
        m_activeListsDirty = true;
        MarkNameLookupDirty();
        ++m_destroyedSinceLastCleanup;
    }
}

std::shared_ptr<GameObject> Scene::FindGameObjectByName(const std::string& name) {
    EnsureNameLookup();
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
        m_activeListsDirty = true;
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

} // namespace gm
