#include "gm/core/Input.hpp"

#include <glm/glm.hpp>
#include "gm/core/input/InputManager.hpp"
#include "gm/core/input/InputSystem.hpp"
#include "gm/core/input/InputAction.hpp"

namespace gm::core {

Input& Input::Instance() {
    static Input instance;
    return instance;
}

bool Input::IsActionPressed(const std::string& actionName) const {
    auto* manager = GetInputManager();
    if (!manager) return false;

    auto* action = manager->GetAction(actionName);
    if (!action) return false;

    auto* inputSys = manager->GetInputSystem();
    if (!inputSys) return false;

    // Check all bindings for this action
    for (const auto& binding : action->GetBindings()) {
        bool pressed = false;
        switch (binding.type) {
            case InputType::Keyboard:
                if (binding.trigger == InputTriggerType::WhilePressed ||
                    binding.trigger == InputTriggerType::OnPress) {
                    pressed = inputSys->IsKeyPressed(binding.keyOrButton);
                }
                break;
            case InputType::MouseButton:
                if (binding.trigger == InputTriggerType::WhilePressed ||
                    binding.trigger == InputTriggerType::OnPress) {
                    pressed = inputSys->IsMouseButtonPressed(
                        static_cast<MouseButton>(binding.keyOrButton));
                }
                break;
            default:
                break;
        }
        if (pressed) return true;
    }
    return false;
}

bool Input::IsActionJustPressed(const std::string& actionName) const {
    auto* manager = GetInputManager();
    if (!manager) return false;

    auto* action = manager->GetAction(actionName);
    if (!action) return false;

    auto* inputSys = manager->GetInputSystem();
    if (!inputSys) return false;

    for (const auto& binding : action->GetBindings()) {
        bool justPressed = false;
        switch (binding.type) {
            case InputType::Keyboard:
                if (binding.trigger == InputTriggerType::OnPress) {
                    justPressed = inputSys->IsKeyJustPressed(binding.keyOrButton);
                }
                break;
            case InputType::MouseButton:
                if (binding.trigger == InputTriggerType::OnPress) {
                    justPressed = inputSys->IsMouseButtonJustPressed(
                        static_cast<MouseButton>(binding.keyOrButton));
                }
                break;
            default:
                break;
        }
        if (justPressed) return true;
    }
    return false;
}

bool Input::IsActionJustReleased(const std::string& actionName) const {
    auto* manager = GetInputManager();
    if (!manager) return false;

    auto* action = manager->GetAction(actionName);
    if (!action) return false;

    auto* inputSys = manager->GetInputSystem();
    if (!inputSys) return false;

    for (const auto& binding : action->GetBindings()) {
        bool justReleased = false;
        switch (binding.type) {
            case InputType::Keyboard:
                if (binding.trigger == InputTriggerType::OnRelease) {
                    justReleased = inputSys->IsKeyJustReleased(binding.keyOrButton);
                }
                break;
            case InputType::MouseButton:
                if (binding.trigger == InputTriggerType::OnRelease) {
                    justReleased = inputSys->IsMouseButtonJustReleased(
                        static_cast<MouseButton>(binding.keyOrButton));
                }
                break;
            default:
                break;
        }
        if (justReleased) return true;
    }
    return false;
}

float Input::GetActionValue(const std::string& actionName) const {
    auto* manager = GetInputManager();
    if (!manager) return 0.0f;

    auto* action = manager->GetAction(actionName);
    if (!action) return 0.0f;

    auto* inputSys = manager->GetInputSystem();
    if (!inputSys) return 0.0f;

    // For axis-based actions, return the current value
    for (const auto& binding : action->GetBindings()) {
        if (binding.type == InputType::MouseAxis) {
            float value = binding.keyOrButton == GLFW_MOUSE_BUTTON_LEFT ?
                         static_cast<float>(inputSys->GetMouseDeltaX()) :
                         static_cast<float>(inputSys->GetMouseDeltaY());
            if (binding.isNegative) value = -value;
            if (std::abs(value) > binding.threshold) {
                return value;
            }
        }
    }

    // For button-based actions, return 1.0 if pressed, 0.0 otherwise
    return IsActionPressed(actionName) ? 1.0f : 0.0f;
}

glm::vec2 Input::GetMousePosition() const {
    auto* manager = GetInputManager();
    if (!manager) return glm::vec2(0.0f);

    auto* inputSys = manager->GetInputSystem();
    if (!inputSys) return glm::vec2(0.0f);

    return glm::vec2(
        static_cast<float>(inputSys->GetMouseX()),
        static_cast<float>(inputSys->GetMouseY())
    );
}

glm::vec2 Input::GetMouseDelta() const {
    auto* manager = GetInputManager();
    if (!manager) return glm::vec2(0.0f);

    auto* inputSys = manager->GetInputSystem();
    if (!inputSys) return glm::vec2(0.0f);

    return glm::vec2(
        static_cast<float>(inputSys->GetMouseDeltaX()),
        static_cast<float>(inputSys->GetMouseDeltaY())
    );
}

float Input::GetMouseScrollY() const {
    auto* manager = GetInputManager();
    if (!manager) return 0.0f;

    auto* inputSys = manager->GetInputSystem();
    if (!inputSys) return 0.0f;

    return static_cast<float>(inputSys->GetMouseScrollY());
}

InputSystem* Input::GetInputSystem() const {
    auto* manager = GetInputManager();
    return manager ? manager->GetInputSystem() : nullptr;
}

InputManager* Input::GetInputManager() const {
    return &InputManager::Instance();
}

} // namespace gm::core

