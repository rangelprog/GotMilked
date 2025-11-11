#include "gm/rendering/LightManager.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/scene/GameObject.hpp"
#include <cstdio>

namespace gm {

LightManager::LightManager() {
}

void LightManager::CollectLights(const std::vector<std::shared_ptr<GameObject>>& gameObjects) {
    m_lights.clear();
    
    for (const auto& obj : gameObjects) {
        if (!obj || !obj->IsActive() || obj->IsDestroyed()) continue;
        
        if (auto lightComp = obj->GetComponent<LightComponent>()) {
            if (lightComp->IsEnabled() && lightComp->IsActive()) {
                if (m_lights.size() < MAX_LIGHTS) {
                    m_lights.push_back(lightComp.get());
                } else {
                    printf("[LightManager] Warning: Maximum number of lights (%zu) reached, skipping light\n", MAX_LIGHTS);
                }
            }
        }
    }
}

void LightManager::ApplyLights(Shader& shader, const glm::vec3& viewPos) const {
    // Set number of active lights
    shader.SetInt("uNumLights", static_cast<int>(m_lights.size()));

    // Apply each light
    for (size_t i = 0; i < m_lights.size() && i < MAX_LIGHTS; ++i) {
        const auto* light = m_lights[i];
        if (!light) continue;

        std::string prefix = "uLights[" + std::to_string(i) + "].";

        // Light type (0=Directional, 1=Point, 2=Spot)
        int type = static_cast<int>(light->GetType());
        shader.SetInt((prefix + "type").c_str(), type);

        // Color and intensity
        glm::vec3 color = light->GetColor() * light->GetIntensity();
        shader.SetVec3((prefix + "color").c_str(), color);

        if (light->GetType() == LightComponent::LightType::Directional) {
            // Directional light
            glm::vec3 direction = light->GetWorldDirection();
            shader.SetVec3((prefix + "direction").c_str(), direction);
        } else if (light->GetType() == LightComponent::LightType::Point) {
            // Point light
            glm::vec3 position = light->GetWorldPosition();
            shader.SetVec3((prefix + "position").c_str(), position);
            glm::vec3 attenuation = light->GetAttenuation();
            shader.SetVec3((prefix + "attenuation").c_str(), attenuation);
        } else if (light->GetType() == LightComponent::LightType::Spot) {
            // Spot light
            glm::vec3 position = light->GetWorldPosition();
            glm::vec3 direction = light->GetWorldDirection();
            shader.SetVec3((prefix + "position").c_str(), position);
            shader.SetVec3((prefix + "direction").c_str(), direction);
            glm::vec3 attenuation = light->GetAttenuation();
            shader.SetVec3((prefix + "attenuation").c_str(), attenuation);
            shader.SetFloat((prefix + "innerCone").c_str(), glm::cos(light->GetInnerConeAngle())); // Already in radians
            shader.SetFloat((prefix + "outerCone").c_str(), glm::cos(light->GetOuterConeAngle())); // Already in radians
        }
    }

    // Fill remaining light slots with disabled lights
    for (size_t i = m_lights.size(); i < MAX_LIGHTS; ++i) {
        std::string prefix = "uLights[" + std::to_string(i) + "].";
        shader.SetInt((prefix + "type").c_str(), -1); // Disabled
    }
}

} // namespace gm

