#pragma once

#include <string>
#include <glm/glm.hpp>

namespace gm {
namespace core {

class InputSystem;
class InputManager;

/**
 * @brief High-level input abstraction for querying game actions
 * 
 * Provides a clean API for checking input state using action names
 * instead of raw key codes. Actions are defined with bindings that
 * can be easily reconfigured.
 */
class Input {
public:
    static Input& Instance();

    // Action query methods (polling-based, no callbacks needed)
    bool IsActionPressed(const std::string& actionName) const;
    bool IsActionJustPressed(const std::string& actionName) const;
    bool IsActionJustReleased(const std::string& actionName) const;
    float GetActionValue(const std::string& actionName) const;

    // Mouse state (direct access for camera/look controls)
    glm::vec2 GetMousePosition() const;
    glm::vec2 GetMouseDelta() const;
    float GetMouseScrollY() const;

    // Raw input system access (for advanced use cases)
    InputSystem* GetInputSystem() const;

private:
    Input() = default;
    ~Input() = default;
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    InputManager* GetInputManager() const;
};

} // namespace core
} // namespace gm

