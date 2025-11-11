#pragma once

#include <memory>
#include <string>
#include <functional>

namespace gm {
class Camera;
class Scene;
}

struct GLFWwindow;

namespace gm::gameplay {

/**
 * Generic fly-camera controller for debug/editor use.
 * Handles mouse capture, camera movement, FOV control, and wireframe toggle.
 */
class FlyCameraController {
public:
    struct Config {
        float baseSpeed = 3.0f;
        float sprintMultiplier = 4.0f;
        float fovMin = 30.0f;
        float fovMax = 100.0f;
        float fovScrollSensitivity = 2.0f;
        float initialFov = 60.0f;
    };

    explicit FlyCameraController(Camera& camera, GLFWwindow* window = nullptr, Config config = {});

    void SetWindow(GLFWwindow* window);
    void SetScene(const std::shared_ptr<Scene>& scene);

    void Update(float dt);

    float GetFovDegrees() const { return m_fovDegrees; }
    void SetFovDegrees(float fov);
    bool IsMouseCaptured() const { return m_mouseCaptured; }
    double GetWorldTimeSeconds() const { return m_worldTimeSeconds; }
    void SetWorldTimeSeconds(double time) { m_worldTimeSeconds = time; }
    std::string GetActiveSceneName() const;
    void SetInputSuppressed(bool suppressed) { m_inputSuppressed = suppressed; }

    // Optional callback for wireframe toggle (defaults to glPolygonMode)
    using WireframeCallback = std::function<void(bool wireframe)>;
    void SetWireframeCallback(WireframeCallback callback) { m_wireframeCallback = std::move(callback); }

private:
    Camera& m_camera;
    std::weak_ptr<Scene> m_scene;
    GLFWwindow* m_window = nullptr;
    Config m_config;

    bool m_mouseCaptured = false;
    bool m_firstCapture = true;
    bool m_wireframe = false;
    float m_fovDegrees = 60.0f;
    double m_worldTimeSeconds = 0.0;
    bool m_inputSuppressed = false;
    WireframeCallback m_wireframeCallback;

    void ApplyCameraMouseLook();
    void ApplyMovement(float dt);
    void HandleWireframeToggle();
    void HandleScroll();
};

} // namespace gm::gameplay

