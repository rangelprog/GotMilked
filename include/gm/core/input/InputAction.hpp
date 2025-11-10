#pragma once
#include <string>
#include <vector>
#include <functional>
#include <GLFW/glfw3.h>
#include "gm/core/input/InputSystem.hpp"

namespace gm {
namespace core {

// Types of input that can trigger an action
enum class InputType {
    Keyboard,
    MouseButton,
    MouseAxis,
    MousePosition,
    GamepadButton,
    GamepadAxis
};

// How the input should be processed
enum class InputTriggerType {
    OnPress,        // Triggered once when input is first pressed
    OnRelease,      // Triggered once when input is released
    WhilePressed,   // Triggered every frame while input is held
    OnValue,        // Triggered when axis/value changes
    WhileValue     // Triggered every frame while value meets condition
};

// Structure to define a binding between a physical input and an action
struct InputBinding {
    InputType type;
    int keyOrButton;        // GLFW key code or mouse button
    InputTriggerType trigger;
    float threshold = 0.1f;  // For axis inputs
    bool isNegative = false; // For axis inputs
};

// Class representing a game action that can be triggered by one or more inputs
class InputAction {
public:
    InputAction(const std::string& name)
        : m_name(name) {}

    void AddBinding(const InputBinding& binding) {
        m_bindings.push_back(binding);
    }

    void AddCallback(std::function<void(float)> callback) {
        m_callbacks.push_back(callback);
    }

    const std::string& GetName() const { return m_name; }
    const std::vector<InputBinding>& GetBindings() const { return m_bindings; }

    void Trigger(float value = 1.0f) {
        for (const auto& callback : m_callbacks) {
            callback(value);
        }
    }

private:
    std::string m_name;
    std::vector<InputBinding> m_bindings;
    std::vector<std::function<void(float)>> m_callbacks;
};

} // namespace core
} // namespace gm