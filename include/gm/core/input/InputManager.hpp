#pragma once
#include <memory>
#include <unordered_map>
#include <string>
#include "gm/core/input/InputSystem.hpp"
#include "gm/core/input/InputAction.hpp"

namespace gm {
namespace core {

class InputManager {
public:
    static InputManager& Instance() {
        static InputManager instance;
        return instance;
    }

    void Init(GLFWwindow* window) {
        m_inputSystem = std::make_unique<InputSystem>();
        m_inputSystem->Init(window);
    }

    void Update() {
        m_inputSystem->Update();
        UpdateActions();
    }

    // Input system direct access
    InputSystem* GetInputSystem() const { return m_inputSystem.get(); }

    // Action management
    InputAction* CreateAction(const std::string& name) {
        auto action = std::make_unique<InputAction>(name);
        auto actionPtr = action.get();
        m_actions[name] = std::move(action);
        return actionPtr;
    }

    InputAction* GetAction(const std::string& name) {
        auto it = m_actions.find(name);
        return it != m_actions.end() ? it->second.get() : nullptr;
    }

    void RemoveAction(const std::string& name) {
        m_actions.erase(name);
    }

private:
    InputManager() = default;
    ~InputManager() = default;
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    void UpdateActions() {
        for (const auto& [name, action] : m_actions) {
            for (const auto& binding : action->GetBindings()) {
                float value = 0.0f;
                bool shouldTrigger = false;

                switch (binding.type) {
                    case InputType::Keyboard:
                        switch (binding.trigger) {
                            case InputTriggerType::OnPress:
                                shouldTrigger = m_inputSystem->IsKeyJustPressed(binding.keyOrButton);
                                break;
                            case InputTriggerType::OnRelease:
                                shouldTrigger = m_inputSystem->IsKeyJustReleased(binding.keyOrButton);
                                break;
                            case InputTriggerType::WhilePressed:
                                shouldTrigger = m_inputSystem->IsKeyPressed(binding.keyOrButton);
                                break;
                            default:
                                break;
                        }
                        break;

                    case InputType::MouseButton:
                        switch (binding.trigger) {
                            case InputTriggerType::OnPress:
                                shouldTrigger = m_inputSystem->IsMouseButtonJustPressed(static_cast<MouseButton>(binding.keyOrButton));
                                break;
                            case InputTriggerType::OnRelease:
                                shouldTrigger = m_inputSystem->IsMouseButtonJustReleased(static_cast<MouseButton>(binding.keyOrButton));
                                break;
                            case InputTriggerType::WhilePressed:
                                shouldTrigger = m_inputSystem->IsMouseButtonPressed(static_cast<MouseButton>(binding.keyOrButton));
                                break;
                            default:
                                break;
                        }
                        break;

                    case InputType::MouseAxis:
                        value = binding.keyOrButton == GLFW_MOUSE_BUTTON_LEFT ? 
                               static_cast<float>(m_inputSystem->GetMouseDeltaX()) :
                               static_cast<float>(m_inputSystem->GetMouseDeltaY());
                        if (binding.isNegative) value = -value;
                        shouldTrigger = std::abs(value) > binding.threshold;
                        break;

                    case InputType::MousePosition:
                        value = binding.keyOrButton == GLFW_MOUSE_BUTTON_LEFT ?
                               static_cast<float>(m_inputSystem->GetMouseX()) :
                               static_cast<float>(m_inputSystem->GetMouseY());
                        shouldTrigger = true;
                        break;

                    // TODO: Add gamepad support
                    default:
                        break;
                }

                if (shouldTrigger) {
                    action->Trigger(value);
                }
            }
        }
    }

    std::unique_ptr<InputSystem> m_inputSystem;
    std::unordered_map<std::string, std::unique_ptr<InputAction>> m_actions;
};

} // namespace core
} // namespace gm