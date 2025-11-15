#include "CowAnimationController.hpp"

#include "gm/scene/AnimatorComponent.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/core/Logger.hpp"
#include "WeatherService.hpp"

#include <glm/glm.hpp>
#include <algorithm>

namespace gotmilked {

CowAnimationController::CowAnimationController() {
    SetName("CowAnimationController");
}

void CowAnimationController::Init() {
    auto ownerObj = GetOwner();
    if (!ownerObj) {
        return;
    }

    m_transform = ownerObj->GetTransform();
    if (auto transform = m_transform.lock()) {
        m_lastPosition = transform->GetPosition();
        m_hasLastPosition = true;
    }

    m_animator = ownerObj->GetComponent<gm::scene::AnimatorComponent>();
    if (auto animator = m_animator.lock()) {
        animator->SetWeight(m_idleSlot, 1.0f);
        animator->SetWeight(m_walkSlot, 0.0f);
        animator->Play(m_idleSlot, true);
        animator->Play(m_walkSlot, true);
        m_currentWalkWeight = 0.0f;
    } else {
        gm::core::Logger::Warning(
            "[CowAnimationController] GameObject '{}' is missing AnimatorComponent",
            ownerObj->GetName().c_str());
    }
}

void CowAnimationController::Update(float deltaTime) {
    if (deltaTime <= 0.0f) {
        return;
    }

    auto transform = m_transform.lock();
    auto animator = m_animator.lock();
    if (!transform || !animator) {
        return;
    }

    const glm::vec3 position = transform->GetPosition();
    if (!m_hasLastPosition) {
        m_lastPosition = position;
        m_hasLastPosition = true;
        return;
    }

    const glm::vec3 delta = position - m_lastPosition;
    m_lastPosition = position;

    const float speed = glm::length(delta) / std::max(deltaTime, 1e-4f);
    const float moodFactor = ComputeMoodFactor();
    const float baseTarget = (speed >= m_speedThreshold) ? 1.0f : 0.0f;
    const float targetWalkWeight = baseTarget * moodFactor + (1.0f - moodFactor) * 0.15f;
    const float lerpFactor = std::clamp(m_blendRate * deltaTime, 0.0f, 1.0f);
    m_currentWalkWeight = glm::mix(m_currentWalkWeight, targetWalkWeight, lerpFactor);

    ApplyWeights(m_currentWalkWeight);
}

void CowAnimationController::ApplyWeights(float walkWeight) {
    auto animator = m_animator.lock();
    if (!animator) {
        return;
    }

    const float idleWeight = std::clamp(1.0f - walkWeight, 0.0f, 1.0f);
    animator->SetWeight(m_walkSlot, std::clamp(walkWeight, 0.0f, 1.0f));
    animator->SetWeight(m_idleSlot, idleWeight);
}

float CowAnimationController::ComputeMoodFactor() const {
    auto weatherService = WeatherService::GlobalInstance();
    if (!weatherService) {
        return 1.0f;
    }
    const auto env = weatherService->GetEnvironment();
    const float temp = env.ambientTemperatureC;
    const float precip = env.precipitationRate;

    float tempFactor = 1.0f;
    if (temp < m_comfortMinC) {
        tempFactor = std::clamp(1.0f - (m_comfortMinC - temp) / 15.0f, 0.2f, 1.0f);
    } else if (temp > m_comfortMaxC) {
        tempFactor = std::clamp(1.0f - (temp - m_comfortMaxC) / 15.0f, 0.2f, 1.0f);
    }

    float rainFactor = 1.0f;
    if (precip > m_rainTolerance) {
        rainFactor = std::clamp(1.0f - (precip - m_rainTolerance) / (m_rainTolerance * 3.0f), 0.15f, 1.0f);
    }

    return std::clamp((tempFactor + rainFactor) * 0.5f, 0.1f, 1.0f);
}

} // namespace gotmilked

