#pragma once

#include "gm/scene/Component.hpp"

#include <string>

namespace gm::gameplay {

class QuestTriggerComponent : public gm::Component {
public:
    QuestTriggerComponent();

    const std::string& GetQuestId() const { return m_questId; }
    void SetQuestId(std::string questId);

    float GetActivationRadius() const { return m_activationRadius; }
    void SetActivationRadius(float radius);

    bool TriggerOnSceneLoad() const { return m_triggerOnSceneLoad; }
    void SetTriggerOnSceneLoad(bool enabled) { m_triggerOnSceneLoad = enabled; }

    bool TriggerOnInteract() const { return m_triggerOnInteract; }
    void SetTriggerOnInteract(bool enabled) { m_triggerOnInteract = enabled; }

    bool IsRepeatable() const { return m_repeatable; }
    void SetRepeatable(bool repeatable) { m_repeatable = repeatable; }

    bool IsTriggered() const { return m_triggered; }
    bool MarkTriggered();
    void ResetTriggerState() { m_triggered = false; }

    bool HasSceneLoadTriggered() const { return m_sceneLoadTriggered; }
    void MarkSceneLoadTriggered() { m_sceneLoadTriggered = true; }

    const std::string& GetActivationAction() const { return m_activationAction; }
    void SetActivationAction(std::string action);

private:
    std::string m_questId;
    float m_activationRadius = 2.5f;
    bool m_triggerOnSceneLoad = false;
    bool m_triggerOnInteract = true;
    bool m_repeatable = false;
    bool m_triggered = false;
    bool m_sceneLoadTriggered = false;
    std::string m_activationAction{"Interact"};
};

} // namespace gm::gameplay


