#include "CameraRigComponent.hpp"

#include <algorithm>
#include <utility>

namespace {
constexpr float kDefaultBaseSpeed = 3.0f;
constexpr float kDefaultSprintMultiplier = 4.0f;
constexpr float kDefaultFovMin = 30.0f;
constexpr float kDefaultFovMax = 100.0f;
constexpr float kDefaultFovScrollSensitivity = 2.0f;
constexpr float kDefaultInitialFov = 60.0f;
}

namespace gm::gameplay {

CameraRigComponent::CameraRigComponent() {
    m_config.baseSpeed = kDefaultBaseSpeed;
    m_config.sprintMultiplier = kDefaultSprintMultiplier;
    m_config.fovMin = kDefaultFovMin;
    m_config.fovMax = kDefaultFovMax;
    m_config.fovScrollSensitivity = kDefaultFovScrollSensitivity;
    m_config.initialFov = kDefaultInitialFov;
}

void CameraRigComponent::SetConfig(const FlyCameraController::Config& config) {
    m_config = config;
    NormalizeFovBounds();
}

void CameraRigComponent::SetBaseSpeed(float speed) {
    m_config.baseSpeed = std::max(0.0f, speed);
}

void CameraRigComponent::SetSprintMultiplier(float multiplier) {
    m_config.sprintMultiplier = std::max(1.0f, multiplier);
}

void CameraRigComponent::SetFovMin(float minFov) {
    m_config.fovMin = minFov;
    NormalizeFovBounds();
}

void CameraRigComponent::SetFovMax(float maxFov) {
    m_config.fovMax = maxFov;
    NormalizeFovBounds();
}

void CameraRigComponent::SetFovScrollSensitivity(float sensitivity) {
    m_config.fovScrollSensitivity = std::max(0.0f, sensitivity);
}

void CameraRigComponent::SetInitialFov(float fovDegrees) {
    m_config.initialFov = fovDegrees;
    NormalizeFovBounds();
}

void CameraRigComponent::SetCaptureMouseOnFocus(bool capture) {
    m_captureMouseOnFocus = capture;
}

void CameraRigComponent::SetRigId(std::string rigId) {
    if (rigId.empty()) {
        m_rigId = "PrimaryCamera";
    } else {
        m_rigId = std::move(rigId);
    }
}

void CameraRigComponent::NormalizeFovBounds() {
    if (m_config.fovMax < m_config.fovMin) {
        std::swap(m_config.fovMax, m_config.fovMin);
    }
    if (m_config.initialFov < m_config.fovMin) {
        m_config.initialFov = m_config.fovMin;
    } else if (m_config.initialFov > m_config.fovMax) {
        m_config.initialFov = m_config.fovMax;
    }
}

} // namespace gm::gameplay


