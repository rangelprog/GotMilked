#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"

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

}