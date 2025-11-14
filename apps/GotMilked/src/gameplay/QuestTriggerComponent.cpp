#include "QuestTriggerComponent.hpp"

#include <algorithm>
#include <utility>

namespace {
constexpr float kMinActivationRadius = 0.1f;
}

namespace gm::gameplay {

QuestTriggerComponent::QuestTriggerComponent() {
    SetName("QuestTriggerComponent");
}

void QuestTriggerComponent::SetQuestId(std::string questId) {
    m_questId = std::move(questId);
}

void QuestTriggerComponent::SetActivationRadius(float radius) {
    m_activationRadius = std::max(radius, kMinActivationRadius);
}

bool QuestTriggerComponent::MarkTriggered() {
    if (!m_repeatable && m_triggered) {
        return false;
    }
    m_triggered = true;
    return true;
}

void QuestTriggerComponent::SetActivationAction(std::string action) {
    if (action.empty()) {
        m_activationAction = "Interact";
    } else {
        m_activationAction = std::move(action);
    }
}

} // namespace gm::gameplay


