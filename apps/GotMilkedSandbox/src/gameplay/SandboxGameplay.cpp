#include "SandboxGameplay.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "gm/core/Input.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/GameObject.hpp"

#include "../SandboxResources.hpp"

namespace sandbox::gameplay {

SandboxGameplay::SandboxGameplay(gm::Camera& camera,
                                 SandboxResources& resources,
                                 std::vector<std::shared_ptr<gm::GameObject>>& spinnerObjects,
                                 GLFWwindow* window)
    : m_camera(camera),
      m_resources(resources),
      m_spinnerObjects(spinnerObjects),
      m_window(window) {}

void SandboxGameplay::SetWindow(GLFWwindow* window) {
    m_window = window;
}

void SandboxGameplay::SetScene(const std::shared_ptr<gm::Scene>& scene) {
    m_scene = scene;
}

void SandboxGameplay::Update(float dt) {
    auto& input = gm::core::Input::Instance();

    m_worldTimeSeconds += dt;

    if (m_inputSuppressed) {
        if (m_mouseCaptured) {
            if (m_window) {
                glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }
            m_mouseCaptured = false;
            m_firstCapture = true;
        }
        return;
    }

    if (!m_mouseCaptured && input.IsActionJustPressed("MouseCapture")) {
        if (m_window) {
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        m_mouseCaptured = true;
        m_firstCapture = true;
    } else if (m_mouseCaptured && input.IsActionJustReleased("MouseRelease")) {
        if (m_window) {
            glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        m_mouseCaptured = false;
    }

    ApplyCameraMouseLook();
    ApplyMovement(dt);
    HandleWireframeToggle();
    HandleScroll();
}

void SandboxGameplay::ApplyCameraMouseLook() {
    if (!m_mouseCaptured) {
        return;
    }

    auto& input = gm::core::Input::Instance();
    if (m_firstCapture) {
        m_firstCapture = false;
        return;
    }

    glm::vec2 delta = input.GetMouseDelta();
    m_camera.ProcessMouseMovement(delta.x, delta.y);
}

void SandboxGameplay::ApplyMovement(float dt) {
    const float baseSpeed = 3.0f;
    auto& input = gm::core::Input::Instance();
    const float speedMultiplier = input.IsActionPressed("Sprint") ? 4.0f : 1.0f;
    const float speed = baseSpeed * speedMultiplier * dt;

    if (input.IsActionPressed("MoveForward")) m_camera.MoveForward(speed);
    if (input.IsActionPressed("MoveBackward")) m_camera.MoveBackward(speed);
    if (input.IsActionPressed("MoveLeft")) m_camera.MoveLeft(speed);
    if (input.IsActionPressed("MoveRight")) m_camera.MoveRight(speed);
    if (input.IsActionPressed("MoveUp")) m_camera.MoveUp(speed);
    if (input.IsActionPressed("MoveDown")) m_camera.MoveDown(speed);
}

void SandboxGameplay::HandleWireframeToggle() {
    auto& input = gm::core::Input::Instance();
    if (input.IsActionJustPressed("ToggleWireframe")) {
        m_wireframe = !m_wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, m_wireframe ? GL_LINE : GL_FILL);
    }
}

void SandboxGameplay::HandleScroll() {
    auto& input = gm::core::Input::Instance();
    float scrollY = input.GetMouseScrollY();
    if (scrollY == 0.0f) {
        return;
    }

    m_fovDegrees -= scrollY * 2.0f;
    if (m_fovDegrees < 30.0f) m_fovDegrees = 30.0f;
    if (m_fovDegrees > 100.0f) m_fovDegrees = 100.0f;
}

std::string SandboxGameplay::GetActiveSceneName() const {
    if (auto scene = m_scene.lock()) {
        return scene->GetName();
    }
    return {};
}

} // namespace sandbox::gameplay
