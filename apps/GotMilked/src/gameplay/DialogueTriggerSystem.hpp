#pragma once

#include "gm/scene/SceneSystem.hpp"

#include <functional>
#include <memory>
#include <string_view>
#include <vector>

#include <glm/vec3.hpp>

namespace gm {
class Scene;
}

namespace gm::gameplay {

class DialogueTriggerComponent;

class DialogueTriggerSystem : public gm::SceneSystem {
public:
    enum class TriggerSource {
        SceneLoad,
        Interact
    };

    struct TriggerContext {
        TriggerSource source;
    };

    using TriggerCallback = std::function<void(const DialogueTriggerComponent&, const TriggerContext&)>;

    DialogueTriggerSystem() = default;
    ~DialogueTriggerSystem() override = default;

    std::string_view GetName() const override { return "DialogueTriggerSystem"; }

    void OnRegister(gm::Scene& scene) override;
    void OnSceneInit(gm::Scene& scene) override;
    void OnSceneShutdown(gm::Scene& scene) override;
    void Update(gm::Scene& scene, float deltaTime) override;

    void SetPlayerPositionProvider(std::function<glm::vec3()> provider);
    void SetTriggerCallback(TriggerCallback callback);
    void SetSceneContext(const std::shared_ptr<gm::Scene>& scene);
    void SetInputSuppressed(bool suppressed);

private:
    struct TriggerHandle {
        std::weak_ptr<DialogueTriggerComponent> component;
    };

    void CollectTriggers(gm::Scene& scene);
    void RefreshHandles();
    void ProcessSceneLoadTriggers();
    void ProcessInteractionTriggers();
    bool EvaluateInteraction(const std::shared_ptr<DialogueTriggerComponent>& trigger) const;
    glm::vec3 GetPlayerPositionSafe() const;

    std::function<glm::vec3()> m_playerPositionProvider;
    TriggerCallback m_triggerCallback;
    std::weak_ptr<gm::Scene> m_sceneWeak;
    std::vector<TriggerHandle> m_triggers;
    std::uint64_t m_lastSceneVersion = 0;
    bool m_inputSuppressed = false;
};

} // namespace gm::gameplay


