#pragma once

#include "gm/scene/SceneSystem.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace gm {
class Scene;
class Component;
}

namespace gm::gameplay {

class QuestTriggerComponent;

/**
 * @brief Scene system that evaluates quest trigger components and dispatches callbacks.
 */
class QuestTriggerSystem : public gm::SceneSystem {
public:
    QuestTriggerSystem() = default;
    ~QuestTriggerSystem() override = default;

    std::string_view GetName() const override { return "QuestTriggerSystem"; }

    void OnRegister(gm::Scene& scene) override;
    void OnSceneInit(gm::Scene& scene) override;
    void OnSceneShutdown(gm::Scene& scene) override;
    void Update(gm::Scene& scene, float deltaTime) override;

    void SetPlayerPositionProvider(std::function<glm::vec3()> provider);
    void SetTriggerCallback(std::function<void(const QuestTriggerComponent&)> callback);
    void SetSceneContext(const std::shared_ptr<gm::Scene>& scene);
    void SetInputSuppressed(bool suppressed);

private:
    struct TriggerHandle {
        std::weak_ptr<QuestTriggerComponent> component;
    };

    void CollectTriggers(gm::Scene& scene);
    void RefreshHandles();
    void ProcessSceneLoadTriggers();
    void ProcessInteractionTriggers();
    bool EvaluateInteraction(const std::shared_ptr<QuestTriggerComponent>& trigger) const;
    glm::vec3 GetPlayerPositionSafe() const;

    std::function<glm::vec3()> m_playerPositionProvider;
    std::function<void(const QuestTriggerComponent&)> m_triggerCallback;
    std::weak_ptr<gm::Scene> m_sceneWeak;
    std::vector<TriggerHandle> m_triggers;
    std::uint64_t m_lastSceneVersion = 0;
    bool m_inputSuppressed = false;
};

} // namespace gm::gameplay


