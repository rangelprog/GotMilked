#include "gm/rendering/LightManager.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/core/Logger.hpp"
#include <cstdio>
#include <cstring>

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
                    core::Logger::Warning(
                        "[LightManager] Maximum number of lights (%zu) reached, skipping light",
                        MAX_LIGHTS);
                }
            }
        }
    }
}

void LightManager::ApplyLights(Shader& shader, const glm::vec3& viewPos) const {
    // Set number of active lights
    shader.SetInt("uNumLights", static_cast<int>(m_lights.size()));

    // Pre-allocate string buffer to avoid allocations (stack-allocated, no heap allocation)
    char uniformName[64];  // Large enough for "uLights[7].propertyName"

    // Apply each light
    for (size_t i = 0; i < m_lights.size() && i < MAX_LIGHTS; ++i) {
        const auto* light = m_lights[i];
        if (!light) continue;

        // Build base prefix: "uLights[N]."
        int prefixLen = std::snprintf(uniformName, sizeof(uniformName), "uLights[%zu].", i);
        char* suffixPtr = uniformName + prefixLen;

        // Light type (0=Directional, 1=Point, 2=Spot)
        int type = static_cast<int>(light->GetType());
        std::strcpy(suffixPtr, "type");
        shader.SetInt(uniformName, type);

        // Color and intensity
        glm::vec3 color = light->GetColor() * light->GetIntensity();
        std::strcpy(suffixPtr, "color");
        shader.SetVec3(uniformName, color);

        if (light->GetType() == LightComponent::LightType::Directional) {
            // Directional light
            glm::vec3 direction = light->GetWorldDirection();
            std::strcpy(suffixPtr, "direction");
            shader.SetVec3(uniformName, direction);
        } else if (light->GetType() == LightComponent::LightType::Point) {
            // Point light
            glm::vec3 position = light->GetWorldPosition();
            std::strcpy(suffixPtr, "position");
            shader.SetVec3(uniformName, position);
            glm::vec3 attenuation = light->GetAttenuation();
            std::strcpy(suffixPtr, "attenuation");
            shader.SetVec3(uniformName, attenuation);
        } else if (light->GetType() == LightComponent::LightType::Spot) {
            // Spot light
            glm::vec3 position = light->GetWorldPosition();
            std::strcpy(suffixPtr, "position");
            shader.SetVec3(uniformName, position);
            glm::vec3 direction = light->GetWorldDirection();
            std::strcpy(suffixPtr, "direction");
            shader.SetVec3(uniformName, direction);
            glm::vec3 attenuation = light->GetAttenuation();
            std::strcpy(suffixPtr, "attenuation");
            shader.SetVec3(uniformName, attenuation);
            float innerCone = glm::cos(light->GetInnerConeAngle());
            std::strcpy(suffixPtr, "innerCone");
            shader.SetFloat(uniformName, innerCone);
            float outerCone = glm::cos(light->GetOuterConeAngle());
            std::strcpy(suffixPtr, "outerCone");
            shader.SetFloat(uniformName, outerCone);
        }
    }

    // Fill remaining light slots with disabled lights
    for (size_t i = m_lights.size(); i < MAX_LIGHTS; ++i) {
        std::snprintf(uniformName, sizeof(uniformName), "uLights[%zu].type", i);
        shader.SetInt(uniformName, -1); // Disabled
    }
}

} // namespace gm

