#include "gm/scene/GameObjectScheduler.hpp"

#include "gm/scene/GameObject.hpp"
#include "gm/scene/Scene.hpp"

#include <algorithm>
#include <future>
#include <thread>

namespace gm {

GameObjectScheduler::GameObjectScheduler(Scene& owner)
    : m_scene(owner)
    , m_updateThreadPool(std::max<std::size_t>(1, std::thread::hardware_concurrency())) {}

void GameObjectScheduler::BindSource(const std::vector<std::shared_ptr<GameObject>>* objects) {
    m_sourceObjects = objects;
    MarkActiveListsDirty();
}

void GameObjectScheduler::SetParallelUpdatesEnabled(bool enabled) {
    m_parallelUpdatesEnabled = enabled;
}

void GameObjectScheduler::MarkActiveListsDirty() {
    m_activeListsDirty = true;
}

void GameObjectScheduler::RemoveFromActiveLists(const std::shared_ptr<GameObject>& gameObject) {
    auto removeFrom = [&gameObject](auto& list) {
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [&gameObject](const std::shared_ptr<GameObject>& candidate) {
                                      return candidate == gameObject;
                                  }),
                   list.end());
    };
    removeFrom(m_activeRenderables);
    removeFrom(m_activeUpdatables);
}

const std::vector<std::shared_ptr<GameObject>>& GameObjectScheduler::GetActiveRenderables() {
    EnsureActiveLists();
    return m_activeRenderables;
}

const std::vector<std::shared_ptr<GameObject>>& GameObjectScheduler::GetActiveUpdatables() {
    EnsureActiveLists();
    return m_activeUpdatables;
}

void GameObjectScheduler::UpdateGameObjects(float deltaTime) {
    EnsureActiveLists();

    if (m_activeUpdatables.empty()) {
        return;
    }

    if (!m_parallelUpdatesEnabled || m_activeUpdatables.size() <= 1) {
        RunSequentialUpdate(deltaTime);
    } else {
        RunParallelUpdate(deltaTime);
    }
}

void GameObjectScheduler::EnsureActiveLists() {
    if (!m_activeListsDirty || !m_sourceObjects) {
        return;
    }

    const auto& objects = *m_sourceObjects;
    m_activeRenderables.clear();
    m_activeUpdatables.clear();
    m_activeRenderables.reserve(objects.size());
    m_activeUpdatables.reserve(objects.size());

    for (const auto& gameObject : objects) {
        if (!gameObject || gameObject->IsDestroyed()) {
            continue;
        }
        if (gameObject->IsActive()) {
            m_activeRenderables.push_back(gameObject);
            m_activeUpdatables.push_back(gameObject);
        }
    }

    m_activeListsDirty = false;
}

void GameObjectScheduler::RunSequentialUpdate(float deltaTime) {
    for (auto& gameObject : m_activeUpdatables) {
        if (gameObject && !gameObject->IsDestroyed()) {
            gameObject->Update(deltaTime);
        }
    }
}

void GameObjectScheduler::RunParallelUpdate(float deltaTime) {
    const std::size_t count = m_activeUpdatables.size();
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

} // namespace gm

