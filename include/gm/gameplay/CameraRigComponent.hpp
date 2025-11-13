#pragma once

#include "gm/scene/Component.hpp"
#include "gm/gameplay/FlyCameraController.hpp"

#include <string>

namespace gm::gameplay {

/**
 * @brief Data-driven configuration for camera rigs handled by CameraRigSystem.
 *
 * Stores movement and FOV parameters that can be serialized with the scene.
 * Runtime control and input handling are delegated to CameraRigSystem.
 */
class CameraRigComponent : public gm::Component {
public:
    CameraRigComponent();

    const FlyCameraController::Config& GetConfig() const { return m_config; }
    void SetConfig(const FlyCameraController::Config& config);

    float GetBaseSpeed() const { return m_config.baseSpeed; }
    void SetBaseSpeed(float speed);

    float GetSprintMultiplier() const { return m_config.sprintMultiplier; }
    void SetSprintMultiplier(float multiplier);

    float GetFovMin() const { return m_config.fovMin; }
    void SetFovMin(float minFov);

    float GetFovMax() const { return m_config.fovMax; }
    void SetFovMax(float maxFov);

    float GetFovScrollSensitivity() const { return m_config.fovScrollSensitivity; }
    void SetFovScrollSensitivity(float sensitivity);

    float GetInitialFov() const { return m_config.initialFov; }
    void SetInitialFov(float fovDegrees);

    bool CaptureMouseOnFocus() const { return m_captureMouseOnFocus; }
    void SetCaptureMouseOnFocus(bool capture);

    const std::string& GetRigId() const { return m_rigId; }
    void SetRigId(std::string rigId);

    bool AutoActivate() const { return m_autoActivate; }
    void SetAutoActivate(bool value) { m_autoActivate = value; }

private:
    void NormalizeFovBounds();

    FlyCameraController::Config m_config{};
    std::string m_rigId{"PrimaryCamera"};
    bool m_captureMouseOnFocus = true;
    bool m_autoActivate = true;
};

} // namespace gm::gameplay


