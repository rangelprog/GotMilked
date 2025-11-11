#pragma once

#include <memory>
#include <string>
#include <vector>

namespace gm {
class Camera;
class Scene;
class GameObject;
}

struct GLFWwindow;

struct SandboxResources;

namespace sandbox::gameplay {

class SandboxGameplay {
public:
    SandboxGameplay(gm::Camera& camera,
                    SandboxResources& resources,
                    std::vector<std::shared_ptr<gm::GameObject>>& spinnerObjects,
                    GLFWwindow* window = nullptr);

    void SetWindow(GLFWwindow* window);
    void SetScene(const std::shared_ptr<gm::Scene>& scene);

    void Update(float dt);

    float GetFovDegrees() const { return m_fovDegrees; }
    bool IsMouseCaptured() const { return m_mouseCaptured; }
    double GetWorldTimeSeconds() const { return m_worldTimeSeconds; }
    std::string GetActiveSceneName() const;
    void SetInputSuppressed(bool suppressed) { m_inputSuppressed = suppressed; }

private:
    gm::Camera& m_camera;
    SandboxResources& m_resources;
    std::weak_ptr<gm::Scene> m_scene;
    std::vector<std::shared_ptr<gm::GameObject>>& m_spinnerObjects;
    GLFWwindow* m_window = nullptr;

    bool m_mouseCaptured = false;
    bool m_firstCapture = true;
    bool m_wireframe = false;
    float m_fovDegrees = 60.0f;
    double m_worldTimeSeconds = 0.0;
    bool m_inputSuppressed = false;

    void ApplyCameraMouseLook();
    void ApplyMovement(float dt);
    void HandleWireframeToggle();
    void HandleScroll();
};

} // namespace sandbox::gameplay

