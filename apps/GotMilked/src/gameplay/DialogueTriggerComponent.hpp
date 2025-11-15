#pragma once

#include "gm/scene/Component.hpp"

#include <string>

namespace gm::gameplay {

class DialogueTriggerComponent : public gm::Component {
public:
    DialogueTriggerComponent();

    const std::string& GetDialogueId() const { return m_dialogueId; }
    void SetDialogueId(std::string dialogueId);

    float GetActivationRadius() const { return m_activationRadius; }
    void SetActivationRadius(float radius);

    bool TriggerOnSceneLoad() const { return m_triggerOnSceneLoad; }
    void SetTriggerOnSceneLoad(bool enabled) { m_triggerOnSceneLoad = enabled; }

    bool TriggerOnInteract() const { return m_triggerOnInteract; }
    void SetTriggerOnInteract(bool enabled) { m_triggerOnInteract = enabled; }

    bool IsRepeatable() const { return m_repeatable; }
    void SetRepeatable(bool repeatable) { m_repeatable = repeatable; }

    bool AutoStart() const { return m_autoStart; }
    void SetAutoStart(bool autoStart) { m_autoStart = autoStart; }

    bool IsTriggered() const { return m_triggered; }
    bool MarkTriggered();
    void ResetTriggerState() { m_triggered = false; m_sceneLoadTriggered = false; }

    bool HasSceneLoadTriggered() const { return m_sceneLoadTriggered; }
    void MarkSceneLoadTriggered() { m_sceneLoadTriggered = true; }

    const std::string& GetActivationAction() const { return m_activationAction; }
    void SetActivationAction(std::string action);

private:
    std::string m_dialogueId;
    float m_activationRadius = 2.5f;
    bool m_triggerOnSceneLoad = false;
    bool m_triggerOnInteract = true;
    bool m_repeatable = false;
    bool m_autoStart = true;
    bool m_triggered = false;
    bool m_sceneLoadTriggered = false;
    std::string m_activationAction{"Interact"};
};

} // namespace gm::gameplay


