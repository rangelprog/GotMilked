#include "CowComponent.hpp"

CowComponent::CowComponent() {
    SetName("CowComponent");
    m_milkLevel = 0.0f;
    m_milkRegenRate = 0.1f;  // Takes 10 seconds to fill up
}

void CowComponent::Update(float deltaTime) {
    // Regenerate milk over time
    m_milkLevel += m_milkRegenRate * deltaTime;
    if (m_milkLevel > 1.0f) {
        m_milkLevel = 1.0f;
    }
}

