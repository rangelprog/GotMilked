#include "DialogueTriggerSystem.hpp"

#include "DialogueTriggerComponent.hpp"
#include "gm/core/Input.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/scene/TransformComponent.hpp"

#include <algorithm>
#include <glm/glm.hpp>

namespace gm::gameplay {

void DialogueTriggerSystem::OnRegister(gm::Scene& scene) {
    m_lastSceneVersion = scene.CurrentReloadVersion();
    CollectTriggers(scene);
    ProcessSceneLoadTriggers();
}

void DialogueTriggerSystem::OnSceneInit(gm::Scene& scene) {
    m_lastSceneVersion = scene.CurrentReloadVersion();
    CollectTriggers(scene);
    ProcessSceneLoadTriggers();
}

void DialogueTriggerSystem::OnSceneShutdown(gm::Scene&) {
    m_triggers.clear();
}

void DialogueTriggerSystem::Update(gm::Scene& scene, float /*deltaTime*/) {
    if (scene.CurrentReloadVersion() != m_lastSceneVersion) {
        m_lastSceneVersion = scene.CurrentReloadVersion();
        CollectTriggers(scene);
    }

    RefreshHandles();
    ProcessSceneLoadTriggers();
    ProcessInteractionTriggers();
}

void DialogueTriggerSystem::SetPlayerPositionProvider(std::function<glm::vec3()> provider) {
    m_playerPositionProvider = std::move(provider);
}

void DialogueTriggerSystem::SetTriggerCallback(TriggerCallback callback) {
    m_triggerCallback = std::move(callback);
}

void DialogueTriggerSystem::SetSceneContext(const std::shared_ptr<gm::Scene>& scene) {
    m_sceneWeak = scene;
    if (auto shared = m_sceneWeak.lock()) {
        m_lastSceneVersion = shared->CurrentReloadVersion();
        CollectTriggers(*shared);
        ProcessSceneLoadTriggers();
    }
}

void DialogueTriggerSystem::SetInputSuppressed(bool suppressed) {
    m_inputSuppressed = suppressed;
}

void DialogueTriggerSystem::CollectTriggers(gm::Scene& scene) {
    m_triggers.clear();
    auto& objects = scene.GetAllGameObjects();
    for (auto& object : objects) {
        if (!object) {
            continue;
        }
        for (auto& component : object->GetComponents()) {
            if (!component) {
                continue;
            }
            if (auto trigger = std::dynamic_pointer_cast<DialogueTriggerComponent>(component)) {
                m_triggers.push_back(TriggerHandle{trigger});
            }
        }
    }
}

void DialogueTriggerSystem::RefreshHandles() {
    auto it = std::remove_if(m_triggers.begin(), m_triggers.end(),
        [](const TriggerHandle& handle) { return handle.component.expired(); });
    m_triggers.erase(it, m_triggers.end());
}

void DialogueTriggerSystem::ProcessSceneLoadTriggers() {
    for (auto& handle : m_triggers) {
        auto trigger = handle.component.lock();
        if (!trigger) {
            continue;
        }
        if (!trigger->TriggerOnSceneLoad()) {
            continue;
        }
        if (trigger->HasSceneLoadTriggered()) {
            continue;
        }
        trigger->MarkSceneLoadTriggered();
        if (trigger->MarkTriggered() && m_triggerCallback) {
            m_triggerCallback(*trigger, TriggerContext{TriggerSource::SceneLoad});
        }
    }
}

void DialogueTriggerSystem::ProcessInteractionTriggers() {
    if (m_inputSuppressed) {
        return;
    }
    for (auto& handle : m_triggers) {
        auto trigger = handle.component.lock();
        if (!trigger) {
            continue;
        }
        if (!trigger->TriggerOnInteract()) {
            continue;
        }
        const auto& action = trigger->GetActivationAction();
        if (action.empty()) {
            continue;
        }
        auto& input = gm::core::Input::Instance();
        if (!input.IsActionJustPressed(action)) {
            continue;
        }
        if (!EvaluateInteraction(trigger)) {
            continue;
        }
        if (trigger->MarkTriggered() && m_triggerCallback) {
            m_triggerCallback(*trigger, TriggerContext{TriggerSource::Interact});
        }
    }
}

bool DialogueTriggerSystem::EvaluateInteraction(const std::shared_ptr<DialogueTriggerComponent>& trigger) const {
    auto owner = trigger->GetOwner();
    if (!owner) {
        return false;
    }
    auto transform = owner->GetTransform();
    if (!transform) {
        return false;
    }
    glm::vec3 playerPos = GetPlayerPositionSafe();
    glm::vec3 triggerPos = transform->GetPosition();
    float radius = trigger->GetActivationRadius();
    return glm::distance(playerPos, triggerPos) <= radius;
}

glm::vec3 DialogueTriggerSystem::GetPlayerPositionSafe() const {
    if (m_playerPositionProvider) {
        return m_playerPositionProvider();
    }
    return glm::vec3(0.0f);
}

} // namespace gm::gameplay


