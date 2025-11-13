#include "gm/core/InputBindings.hpp"

#include "gm/core/input/InputManager.hpp"
#include "gm/core/input/InputAction.hpp"
#include "gm/core/input/InputSystem.hpp"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace gm::core::InputBindings {

void SetupDefaultBindings(InputManager& inputManager) {
    // Movement actions
    BindKeyboardAction(inputManager, "MoveForward", GLFW_KEY_W, InputTriggerType::WhilePressed);
    BindKeyboardAction(inputManager, "MoveBackward", GLFW_KEY_S, InputTriggerType::WhilePressed);
    BindKeyboardAction(inputManager, "MoveLeft", GLFW_KEY_A, InputTriggerType::WhilePressed);
    BindKeyboardAction(inputManager, "MoveRight", GLFW_KEY_D, InputTriggerType::WhilePressed);
    BindKeyboardAction(inputManager, "MoveUp", GLFW_KEY_SPACE, InputTriggerType::WhilePressed);
    BindKeyboardAction(inputManager, "MoveDown", GLFW_KEY_LEFT_CONTROL, InputTriggerType::WhilePressed);

    // Camera look (mouse delta)
    BindMouseAxisAction(inputManager, "LookX", true, false, 0.01f);
    BindMouseAxisAction(inputManager, "LookY", false, false, 0.01f);

    // UI actions
    BindKeyboardAction(inputManager, "Exit", GLFW_KEY_ESCAPE, InputTriggerType::OnPress);
    BindKeyboardAction(inputManager, "ToggleWireframe", GLFW_KEY_F, InputTriggerType::OnPress);
    BindMouseButtonAction(inputManager, "MouseCapture", MouseButton::Right, InputTriggerType::OnPress);
    BindMouseButtonAction(inputManager, "MouseRelease", MouseButton::Right, InputTriggerType::OnRelease);

#if GM_DEBUG_TOOLS
    BindKeyboardAction(inputManager, "ToggleGrid", GLFW_KEY_G, InputTriggerType::OnPress);
#endif

    // Modifier actions
    BindKeyboardAction(inputManager, "Sprint", GLFW_KEY_LEFT_SHIFT, InputTriggerType::WhilePressed);
    BindKeyboardAction(inputManager, "QuickSave", GLFW_KEY_F5, InputTriggerType::OnPress);
    BindKeyboardAction(inputManager, "QuickLoad", GLFW_KEY_F9, InputTriggerType::OnPress);
    BindKeyboardAction(inputManager, "ToggleOverlay", GLFW_KEY_F1, InputTriggerType::OnPress);
}

void BindKeyboardAction(InputManager& inputManager,
                        const std::string& actionName,
                        int glfwKey,
                        InputTriggerType trigger) {
    auto* action = inputManager.GetAction(actionName);
    if (!action) {
        action = inputManager.CreateAction(actionName);
    }

    InputBinding binding;
    binding.type = InputType::Keyboard;
    binding.keyOrButton = glfwKey;
    binding.trigger = trigger;
    action->AddBinding(binding);
}

void BindMouseButtonAction(InputManager& inputManager,
                           const std::string& actionName,
                           MouseButton button,
                           InputTriggerType trigger) {
    auto* action = inputManager.GetAction(actionName);
    if (!action) {
        action = inputManager.CreateAction(actionName);
    }

    InputBinding binding;
    binding.type = InputType::MouseButton;
    binding.keyOrButton = static_cast<int>(button);
    binding.trigger = trigger;
    action->AddBinding(binding);
}

void BindMouseAxisAction(InputManager& inputManager,
                         const std::string& actionName,
                         bool isXAxis,
                         bool invert,
                         float threshold) {
    auto* action = inputManager.GetAction(actionName);
    if (!action) {
        action = inputManager.CreateAction(actionName);
    }

    InputBinding binding;
    binding.type = InputType::MouseAxis;
    binding.keyOrButton = isXAxis ? GLFW_MOUSE_BUTTON_LEFT : GLFW_MOUSE_BUTTON_RIGHT;
    binding.trigger = InputTriggerType::WhileValue;
    binding.threshold = threshold;
    binding.isNegative = invert;
    action->AddBinding(binding);
}

} // namespace gm::core::InputBindings

