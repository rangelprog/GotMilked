#pragma once

#include "gm/scene/SceneSystem.hpp"
#include "FlyCameraController.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

namespace gm {
class Camera;
class Scene;
}

namespace gm::gameplay {

class CameraRigComponent;

/**
 * @brief Scene system that drives camera rigs defined by CameraRigComponent.
 *
 * Provides an engine-level controller for editor/gameplay cameras so that
 * higher-level games can configure rigs through scene data rather than bespoke code.
 */
class CameraRigSystem : public gm::SceneSystem {
public:
    CameraRigSystem();
    ~CameraRigSystem() override = default;

    std::string_view GetName() const override;

    void OnRegister(gm::Scene& scene) override;
    void OnSceneInit(gm::Scene& scene) override;
    void OnSceneShutdown(gm::Scene& scene) override;
    void Update(gm::Scene& scene, float deltaTime) override;

    void SetActiveCamera(gm::Camera* camera);
    void SetWindow(GLFWwindow* window);
    void SetSceneContext(const std::shared_ptr<gm::Scene>& scene);
    void SetInputSuppressed(bool suppressed);
    void SetWireframeCallback(FlyCameraController::WireframeCallback callback);

    float GetFovDegrees() const;
    void SetFovDegrees(float fov);

    double GetWorldTimeSeconds() const;
    void SetWorldTimeSeconds(double seconds);

    bool IsMouseCaptured() const;
    std::string GetActiveSceneName() const;

private:
    struct RigInstance {
        std::weak_ptr<CameraRigComponent> component;
        std::unique_ptr<FlyCameraController> controller;
    };

    void RefreshRigInstances(gm::Scene& scene);
    void PruneExpiredRigs();
    void EnsureControllers();
    void ApplySharedState(FlyCameraController& controller);
    void InvalidateControllers();

    gm::Camera* m_camera = nullptr;
    GLFWwindow* m_window = nullptr;
    bool m_inputSuppressed = false;
    float m_cachedFov = 60.0f;
    double m_cachedWorldTime = 0.0;
    FlyCameraController::WireframeCallback m_wireframeCallback;
    std::weak_ptr<gm::Scene> m_sceneWeak;
    std::uint64_t m_lastSceneVersion = 0;
    std::vector<RigInstance> m_rigs;
};

} // namespace gm::gameplay


