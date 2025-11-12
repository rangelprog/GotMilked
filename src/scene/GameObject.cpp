#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/core/Logger.hpp"

#include <cctype>

namespace gm {

void GameObject::Init() {
    for (auto& component : components) {
        if (component && component->IsActive()) {
            component->Init();
        }
    }
}

void GameObject::Update(float deltaTime) {
    for (auto& component : components) {
        if (component && component->IsActive()) {
            component->Update(deltaTime);
        }
    }
}

void GameObject::Render() {
    for (auto& component : components) {
        if (component && component->IsActive()) {
            component->Render();
        }
    }
}

void GameObject::Destroy() {
    if (isDestroyed) return;
    
    // Call OnDestroy on all components before marking as destroyed
    for (auto& component : components) {
        if (component) {
            component->OnDestroy();
        }
    }
    
    isDestroyed = true;
}

std::shared_ptr<TransformComponent> GameObject::GetTransform() const {
    return GetComponent<TransformComponent>();
}

std::shared_ptr<TransformComponent> GameObject::EnsureTransform() {
    auto transform = GetComponent<TransformComponent>();
    if (!transform) {
        transform = AddComponent<TransformComponent>();
    }
    return transform;
}

void GameObject::UpdateComponentMap() {
    m_componentMap.clear();
    
    // Build map from type_index to first component of each type
    // This enables O(1) lookup instead of O(n) linear search
    for (const auto& component : components) {
        if (component) {
            std::type_index typeId = std::type_index(typeid(*component));
            // Only store first component of each type (GetComponent returns first match)
            if (m_componentMap.find(typeId) == m_componentMap.end()) {
                m_componentMap[typeId] = component;
            }
        }
    }
}

void GameObject::SetName(const std::string& newName) {
    if (name == newName) {
        return;
    }

    if (!newName.empty()) {
        bool leadingWhitespace = false;
        bool secondCharWhitespace = false;
        if (!newName.empty()) {
            leadingWhitespace = std::isspace(static_cast<unsigned char>(newName.front())) != 0;
        }
        if (newName.size() > 1) {
            secondCharWhitespace = std::isspace(static_cast<unsigned char>(newName[1])) != 0;
        }
        if (leadingWhitespace || secondCharWhitespace) {
            gm::core::Logger::Warning(
                "[GameObject] SetName detected leading whitespace: ptr={}, old='{}', new='{}'",
                static_cast<const void*>(this),
                name,
                newName);
        }
    }

    const std::string oldName = name;
    name = newName;
    m_lastKnownName = name;
    m_hasNameSnapshot = true;

    if (m_scene) {
        m_scene->HandleGameObjectRename(*this, oldName, newName);
    }
}

void GameObject::ResetForReuse() {
    for (auto& component : components) {
        if (component) {
            component->OnReset();
            component->OnDestroy();
        }
    }
    components.clear();
    m_componentMap.clear();
    tags.clear();
    layer = 0;
    isActive = true;
    isDestroyed = false;
    name.clear();
    m_lastKnownName.clear();
    m_hasNameSnapshot = false;
}

void GameObject::ValidateNameIntegrity() const {
    if (!m_hasNameSnapshot) {
        m_lastKnownName = name;
        m_hasNameSnapshot = true;
        return;
    }

    if (name != m_lastKnownName) {
        gm::core::Logger::Error("[GameObject] Detected unexpected name mutation: ptr={}, previous='{}', current='{}'",
            static_cast<const void*>(this),
            m_lastKnownName,
            name);
        m_lastKnownName = name;
    }
}

} // namespace gm