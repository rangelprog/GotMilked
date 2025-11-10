#include "gm/GameObject.hpp"

namespace gm {

void GameObject::Init() {
    for (auto& component : components) {
        if (component->IsActive()) {
            component->Init();
        }
    }
}

void GameObject::Update(float deltaTime) {
    for (auto& component : components) {
        if (component->IsActive()) {
            component->Update(deltaTime);
        }
    }
}

void GameObject::Render() {
    for (auto& component : components) {
        if (component->IsActive()) {
            component->Render();
        }
    }
}

}