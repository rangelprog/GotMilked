#pragma once

#include <string>
#include "gm/core/input/InputAction.hpp"

namespace gm {
namespace core {

class InputManager;

/**
 * @brief Helper functions to set up common input action bindings
 */
namespace InputBindings {

/**
 * @brief Sets up default action bindings for a typical first-person camera/game
 * 
 * Creates actions for:
 * - Movement: MoveForward, MoveBackward, MoveLeft, MoveRight, MoveUp, MoveDown
 * - Camera: LookX, LookY (mouse delta)
 * - UI: Exit, ToggleWireframe, MouseCapture
 * - Misc: Sprint (speed boost)
 */
void SetupDefaultBindings(InputManager& inputManager);

/**
 * @brief Helper to create a keyboard action binding
 */
void BindKeyboardAction(InputManager& inputManager,
                        const std::string& actionName,
                        int glfwKey,
                        InputTriggerType trigger = InputTriggerType::WhilePressed);

/**
 * @brief Helper to create a mouse button action binding
 */
void BindMouseButtonAction(InputManager& inputManager,
                          const std::string& actionName,
                          MouseButton button,
                          InputTriggerType trigger = InputTriggerType::OnPress);

/**
 * @brief Helper to create a mouse axis action binding
 */
void BindMouseAxisAction(InputManager& inputManager,
                         const std::string& actionName,
                         bool isXAxis,
                         bool invert = false,
                         float threshold = 0.01f);

} // namespace InputBindings
} // namespace core
} // namespace gm

