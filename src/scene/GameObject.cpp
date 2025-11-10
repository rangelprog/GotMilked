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

void GameObject::InvalidateCache() const {
    componentCache.clear();
}

}