#include "gm/scene/LightComponent.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/scene/TransformComponent.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace gm {

LightComponent::LightComponent() {
    SetName("LightComponent");
}

glm::vec3 LightComponent::GetWorldPosition() const {
    if (!owner) return glm::vec3(0.0f);
    
    if (auto transform = owner->GetTransform()) {
        return transform->GetPosition();
    }
    return glm::vec3(0.0f);
}

glm::vec3 LightComponent::GetWorldDirection() const {
    if (!owner) return m_direction;
    
    if (auto transform = owner->GetTransform()) {
        // Use forward vector from transform rotation
        return transform->GetForward();
    }
    return m_direction;
}

} // namespace gm

