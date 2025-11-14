#pragma once

#include <memory>
#include <vector>

#include "gm/utils/ThreadPool.hpp"

namespace gm {

class Scene;
class GameObject;

class GameObjectScheduler {
public:
    explicit GameObjectScheduler(Scene& owner);

    void BindSource(const std::vector<std::shared_ptr<GameObject>>* objects);

    void SetParallelUpdatesEnabled(bool enabled);
    bool GetParallelUpdatesEnabled() const { return m_parallelUpdatesEnabled; }

    void MarkActiveListsDirty();
    void RemoveFromActiveLists(const std::shared_ptr<GameObject>& gameObject);

    const std::vector<std::shared_ptr<GameObject>>& GetActiveRenderables();
    const std::vector<std::shared_ptr<GameObject>>& GetActiveUpdatables();

    void UpdateGameObjects(float deltaTime);

private:
    void EnsureActiveLists();
    void RunSequentialUpdate(float deltaTime);
    void RunParallelUpdate(float deltaTime);

    Scene& m_scene;
    gm::utils::ThreadPool m_updateThreadPool;
    const std::vector<std::shared_ptr<GameObject>>* m_sourceObjects = nullptr;

    std::vector<std::shared_ptr<GameObject>> m_activeRenderables;
    std::vector<std::shared_ptr<GameObject>> m_activeUpdatables;
    bool m_activeListsDirty = true;
    bool m_parallelUpdatesEnabled = false;
};

} // namespace gm

