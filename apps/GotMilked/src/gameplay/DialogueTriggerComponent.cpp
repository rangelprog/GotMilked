#include "DialogueTriggerComponent.hpp"

#include <algorithm>
#include <utility>

namespace {
constexpr float kMinActivationRadius = 0.1f;
}

namespace gm::gameplay {

DialogueTriggerComponent::DialogueTriggerComponent() {
    SetName("DialogueTriggerComponent");
}

void DialogueTriggerComponent::SetDialogueId(std::string dialogueId) {
    m_dialogueId = std::move(dialogueId);
}

void DialogueTriggerComponent::SetActivationRadius(float radius) {
    m_activationRadius = std::max(radius, kMinActivationRadius);
}

bool DialogueTriggerComponent::MarkTriggered() {
    if (!m_repeatable && m_triggered) {
        return false;
    }
    m_triggered = true;
    return true;
}

void DialogueTriggerComponent::SetActivationAction(std::string action) {
    if (action.empty()) {
        m_activationAction = "Interact";
    } else {
        m_activationAction = std::move(action);
    }
}

} // namespace gm::gameplay


