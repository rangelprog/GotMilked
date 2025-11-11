#include "gm/gameplay/FlyCameraController.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "gm/core/Input.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/Scene.hpp"

namespace gm::gameplay {

FlyCameraController::FlyCameraController(Camera& camera, GLFWwindow* window, Config config)
    : m_camera(camera),
      m_window(window),
      m_config(std::move(config)),
      m_fovDegrees(m_config.initialFov) {
    // Default wireframe callback
    m_wireframeCallback = [](bool wireframe) {
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    };
}

void FlyCameraController::SetWindow(GLFWwindow* window) {
    m_window = window;
}

void FlyCameraController::SetScene(const std::shared_ptr<Scene>& scene) {
    m_scene = scene;
}

void FlyCameraController::Update(float dt) {
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

void FlyCameraController::ApplyCameraMouseLook() {
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

void FlyCameraController::ApplyMovement(float dt) {
    auto& input = gm::core::Input::Instance();
    const float speedMultiplier = input.IsActionPressed("Sprint") ? m_config.sprintMultiplier : 1.0f;
    const float speed = m_config.baseSpeed * speedMultiplier * dt;

    if (input.IsActionPressed("MoveForward")) m_camera.MoveForward(speed);
    if (input.IsActionPressed("MoveBackward")) m_camera.MoveBackward(speed);
    if (input.IsActionPressed("MoveLeft")) m_camera.MoveLeft(speed);
    if (input.IsActionPressed("MoveRight")) m_camera.MoveRight(speed);
    if (input.IsActionPressed("MoveUp")) m_camera.MoveUp(speed);
    if (input.IsActionPressed("MoveDown")) m_camera.MoveDown(speed);
}

void FlyCameraController::HandleWireframeToggle() {
    auto& input = gm::core::Input::Instance();
    if (input.IsActionJustPressed("ToggleWireframe")) {
        m_wireframe = !m_wireframe;
        if (m_wireframeCallback) {
            m_wireframeCallback(m_wireframe);
        }
    }
}

void FlyCameraController::HandleScroll() {
    auto& input = gm::core::Input::Instance();
    float scrollY = input.GetMouseScrollY();
    if (scrollY == 0.0f) {
        return;
    }

    m_fovDegrees -= scrollY * m_config.fovScrollSensitivity;
    if (m_fovDegrees < m_config.fovMin) m_fovDegrees = m_config.fovMin;
    if (m_fovDegrees > m_config.fovMax) m_fovDegrees = m_config.fovMax;
}

void FlyCameraController::SetFovDegrees(float fov) {
    m_fovDegrees = fov;
    if (m_fovDegrees < m_config.fovMin) m_fovDegrees = m_config.fovMin;
    if (m_fovDegrees > m_config.fovMax) m_fovDegrees = m_config.fovMax;
}

std::string FlyCameraController::GetActiveSceneName() const {
    if (auto scene = m_scene.lock()) {
        return scene->GetName();
    }
    return "No Active Scene";
}

} // namespace gm::gameplay

