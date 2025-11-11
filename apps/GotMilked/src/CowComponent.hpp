#pragma once

#include "gm/scene/Component.hpp"

class CowComponent : public gm::Component {
public:
    CowComponent();

    // Milk management
    float GetMilkLevel() const { return m_milkLevel; }
    float Milk() { float milk = m_milkLevel; m_milkLevel = 0.0f; return milk; }
    bool CanBeMilked() const { return m_milkLevel >= 0.5f; }

    // Update - regenerates milk over time
    void Update(float deltaTime) override;

private:
    float m_milkLevel = 0.0f;  // 0.0 to 1.0
    float m_milkRegenRate = 0.1f;  // milk per second
};

