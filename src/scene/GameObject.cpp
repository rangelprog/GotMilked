#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/core/Logger.hpp"

#include <algorithm>
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
    m_parent.reset();
    m_children.clear();
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

void GameObject::SetParentInternal(const std::shared_ptr<GameObject>& parent) {
    m_parent = parent;
}

void GameObject::ClearParentInternal() {
    m_parent.reset();
}

void GameObject::AddChildInternal(const std::shared_ptr<GameObject>& child) {
    if (!child) {
        return;
    }
    for (const auto& existing : m_children) {
        if (auto locked = existing.lock()) {
            if (locked == child) {
                return;
            }
        }
    }
    m_children.push_back(child);
}

void GameObject::RemoveChildInternal(const std::shared_ptr<GameObject>& child) {
    if (!child) {
        return;
    }
    m_children.erase(
        std::remove_if(m_children.begin(), m_children.end(),
                       [&child](const std::weak_ptr<GameObject>& candidate) {
                           auto locked = candidate.lock();
                           return !locked || locked == child;
                       }),
        m_children.end());
}

void GameObject::ClearChildrenInternal() {
    m_children.clear();
}

std::vector<std::shared_ptr<GameObject>> GameObject::GetChildren() const {
    std::vector<std::shared_ptr<GameObject>> result;
    result.reserve(m_children.size());
    for (const auto& weakChild : m_children) {
        if (auto child = weakChild.lock()) {
            result.push_back(child);
        }
    }
    return result;
}

bool GameObject::HasChildren() const {
    for (const auto& weakChild : m_children) {
        if (weakChild.lock()) {
            return true;
        }
    }
    return false;
}

void GameObject::PropagateTransformDirty() const {
    for (const auto& weakChild : m_children) {
        if (auto child = weakChild.lock()) {
            if (auto transform = child->GetTransform()) {
                transform->MarkWorldDirty();
            }
        }
    }
}

void GameObject::MarkChildrenTransformsDirty() const {
    PropagateTransformDirty();
}

} // namespace gm