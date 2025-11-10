#pragma once
#include <unordered_map>
#include <array>
#include <functional>

#define GLFW_INCLUDE_NONE  // Prevent GLFW from including OpenGL headers
#include <GLFW/glfw3.h>

namespace gm {
namespace core {

enum class KeyState {
    Released,
    Pressed,
    Held,
    JustPressed,
    JustReleased
};

enum class MouseButton {
    Left = GLFW_MOUSE_BUTTON_LEFT,
    Right = GLFW_MOUSE_BUTTON_RIGHT,
    Middle = GLFW_MOUSE_BUTTON_MIDDLE,
    Button4 = GLFW_MOUSE_BUTTON_4,
    Button5 = GLFW_MOUSE_BUTTON_5,
    Button6 = GLFW_MOUSE_BUTTON_6,
    Button7 = GLFW_MOUSE_BUTTON_7,
    Button8 = GLFW_MOUSE_BUTTON_8,
    Count
};

struct MouseState {
    double x = 0.0;
    double y = 0.0;
    double scrollX = 0.0;
    double scrollY = 0.0;
    std::array<KeyState, static_cast<size_t>(MouseButton::Count)> buttons{};
};

class InputSystem {
public:
    InputSystem() = default;
    ~InputSystem() = default;

    // Initialization
    void Init(GLFWwindow* window);

    // Update - processes pending input state changes
    // Call this at the START of your frame, before glfwPollEvents()
    void Update();

    // Keyboard methods
    bool IsKeyPressed(int key) const;
    bool IsKeyJustPressed(int key) const;
    bool IsKeyJustReleased(int key) const;
    bool IsKeyHeld(int key) const;
    KeyState GetKeyState(int key) const;

    // Mouse methods
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonJustPressed(MouseButton button) const;
    bool IsMouseButtonJustReleased(MouseButton button) const;
    bool IsMouseButtonHeld(MouseButton button) const;
    KeyState GetMouseButtonState(MouseButton button) const;

    // Mouse position and scroll
    double GetMouseX() const { return m_mouseState.x; }
    double GetMouseY() const { return m_mouseState.y; }
    double GetMouseScrollX() const { return m_mouseState.scrollX; }
    double GetMouseScrollY() const { return m_mouseState.scrollY; }

    // Mouse delta (movement since last frame)
    double GetMouseDeltaX() const { return m_mouseState.x - m_lastMouseState.x; }
    double GetMouseDeltaY() const { return m_mouseState.y - m_lastMouseState.y; }

private:
    // GLFW callbacks
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Internal state
    std::unordered_map<int, KeyState> m_keyStates;
    MouseState m_mouseState;
    MouseState m_lastMouseState;
    GLFWwindow* m_window = nullptr;

    // Static instance for callbacks
    static InputSystem* s_instance;
};

} // namespace core
} // namespace gm