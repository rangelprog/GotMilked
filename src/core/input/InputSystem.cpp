#include "gm/core/input/InputSystem.hpp"

namespace gm {
namespace core {

InputSystem* InputSystem::s_instance = nullptr;

void InputSystem::Init(GLFWwindow* window) {
    m_window = window;
    s_instance = this;

    // Set GLFW callbacks
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetScrollCallback(window, ScrollCallback);
}

void InputSystem::Update() {
    // Store last mouse state
    m_lastMouseState = m_mouseState;

    // Update key states
    for (auto& [key, state] : m_keyStates) {
        if (state == KeyState::JustPressed) {
            state = KeyState::Held;
        }
        else if (state == KeyState::JustReleased) {
            state = KeyState::Released;
        }
    }

    // Update mouse button states
    for (size_t i = 0; i < static_cast<size_t>(MouseButton::Count); ++i) {
        if (m_mouseState.buttons[i] == KeyState::JustPressed) {
            m_mouseState.buttons[i] = KeyState::Held;
        }
        else if (m_mouseState.buttons[i] == KeyState::JustReleased) {
            m_mouseState.buttons[i] = KeyState::Released;
        }
    }

    // Reset scroll values
    m_mouseState.scrollX = 0.0;
    m_mouseState.scrollY = 0.0;
}

bool InputSystem::IsKeyPressed(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && 
           (it->second == KeyState::Pressed || it->second == KeyState::Held);
}

bool InputSystem::IsKeyJustPressed(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && it->second == KeyState::JustPressed;
}

bool InputSystem::IsKeyJustReleased(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && it->second == KeyState::JustReleased;
}

bool InputSystem::IsKeyHeld(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() && it->second == KeyState::Held;
}

KeyState InputSystem::GetKeyState(int key) const {
    auto it = m_keyStates.find(key);
    return it != m_keyStates.end() ? it->second : KeyState::Released;
}

bool InputSystem::IsMouseButtonPressed(MouseButton button) const {
    size_t idx = static_cast<size_t>(button);
    return m_mouseState.buttons[idx] == KeyState::Pressed || 
           m_mouseState.buttons[idx] == KeyState::Held;
}

bool InputSystem::IsMouseButtonJustPressed(MouseButton button) const {
    return m_mouseState.buttons[static_cast<size_t>(button)] == KeyState::JustPressed;
}

bool InputSystem::IsMouseButtonJustReleased(MouseButton button) const {
    return m_mouseState.buttons[static_cast<size_t>(button)] == KeyState::JustReleased;
}

bool InputSystem::IsMouseButtonHeld(MouseButton button) const {
    return m_mouseState.buttons[static_cast<size_t>(button)] == KeyState::Held;
}

KeyState InputSystem::GetMouseButtonState(MouseButton button) const {
    return m_mouseState.buttons[static_cast<size_t>(button)];
}

// Static GLFW callbacks
void InputSystem::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (!s_instance) return;

    switch (action) {
        case GLFW_PRESS:
            s_instance->m_keyStates[key] = KeyState::JustPressed;
            break;
        case GLFW_RELEASE:
            s_instance->m_keyStates[key] = KeyState::JustReleased;
            break;
        case GLFW_REPEAT:
            s_instance->m_keyStates[key] = KeyState::Held;
            break;
    }
}

void InputSystem::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (!s_instance || button >= static_cast<int>(MouseButton::Count)) return;

    switch (action) {
        case GLFW_PRESS:
            s_instance->m_mouseState.buttons[button] = KeyState::JustPressed;
            break;
        case GLFW_RELEASE:
            s_instance->m_mouseState.buttons[button] = KeyState::JustReleased;
            break;
    }
}

void InputSystem::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (!s_instance) return;
    s_instance->m_mouseState.x = xpos;
    s_instance->m_mouseState.y = ypos;
}

void InputSystem::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (!s_instance) return;
    s_instance->m_mouseState.scrollX += xoffset;
    s_instance->m_mouseState.scrollY += yoffset;
}

} // namespace core
} // namespace gm