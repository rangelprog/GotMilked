#include "gm/rendering/LightManager.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/scene/GameObject.hpp"
#include "gm/core/Logger.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace gm {

LightManager::LightManager() {
}

void LightManager::CollectLights(const std::vector<std::shared_ptr<GameObject>>& gameObjects) {
    m_lights.clear();
    // Reserve capacity to avoid reallocations (most scenes have few lights)
    m_lights.reserve(std::min(MAX_LIGHTS, gameObjects.size()));
    
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

glm::vec3 LightManager::NormalizeFallback(const glm::vec3& dir) {
    const float lenSq = glm::dot(dir, dir);
    if (lenSq < 1e-4f) {
        return glm::vec3(0.0f, -1.0f, 0.0f);
    }
    return glm::normalize(dir);
}

LightManager::DirectionalOverride LightManager::BuildOverride(const glm::vec3& direction,
                                                              const glm::vec3& color,
                                                              float intensity) {
    DirectionalOverride data;
    data.enabled = intensity > 0.0001f;
    data.direction = NormalizeFallback(direction);
    data.color = color;
    data.intensity = intensity;
    return data;
}

void LightManager::SetCelestialLights(const glm::vec3& sunDirection,
                                      const glm::vec3& sunColor,
                                      float sunIntensity,
                                      const glm::vec3& moonDirection,
                                      const glm::vec3& moonColor,
                                      float moonIntensity) {
    m_sunOverride = BuildOverride(sunDirection, sunColor, sunIntensity);
    m_moonOverride = BuildOverride(moonDirection, moonColor, moonIntensity);
}

void LightManager::ClearCelestialLights() {
    m_sunOverride.enabled = false;
    m_moonOverride.enabled = false;
}

void LightManager::ApplyLights(Shader& shader, const glm::vec3& viewPos) const {
    static_cast<void>(viewPos);

    char uniformName[64];
    auto makePrefix = [&](size_t slotIndex, char*& suffixPtr, size_t& suffixCapacity) {
        int prefixLen = std::snprintf(uniformName, sizeof(uniformName), "uLights[%zu].", slotIndex);
        suffixPtr = uniformName + prefixLen;
        suffixCapacity = sizeof(uniformName) > static_cast<size_t>(prefixLen)
            ? sizeof(uniformName) - static_cast<size_t>(prefixLen)
            : 0;
    };
    auto setSuffix = [&](char* suffixPtr, size_t suffixCapacity, const char* suffix) {
        if (suffixCapacity == 0) {
            return;
        }
        std::snprintf(suffixPtr, suffixCapacity, "%s", suffix);
    };

    size_t slot = 0;
    auto applyOverride = [&](const DirectionalOverride& light) {
        if (!light.enabled || slot >= MAX_LIGHTS) {
            return;
        }
        char* suffixPtr = nullptr;
        size_t suffixCapacity = 0;
        makePrefix(slot, suffixPtr, suffixCapacity);

        setSuffix(suffixPtr, suffixCapacity, "type");
        shader.SetInt(uniformName, 0);

        setSuffix(suffixPtr, suffixCapacity, "color");
        shader.SetVec3(uniformName, light.color * light.intensity);

        setSuffix(suffixPtr, suffixCapacity, "direction");
        shader.SetVec3(uniformName, light.direction);

        ++slot;
    };

    applyOverride(m_sunOverride);
    applyOverride(m_moonOverride);

    for (size_t i = 0; i < m_lights.size() && slot < MAX_LIGHTS; ++i) {
        const auto* light = m_lights[i];
        if (!light) {
            continue;
        }

        char* suffixPtr = nullptr;
        size_t suffixCapacity = 0;
        makePrefix(slot, suffixPtr, suffixCapacity);
        ++slot;

        auto safeSet = [&](const char* suffix) {
            setSuffix(suffixPtr, suffixCapacity, suffix);
        };

        int type = static_cast<int>(light->GetType());
        safeSet("type");
        shader.SetInt(uniformName, type);

        glm::vec3 color = light->GetColor() * light->GetIntensity();
        safeSet("color");
        shader.SetVec3(uniformName, color);

        if (light->GetType() == LightComponent::LightType::Directional) {
            glm::vec3 direction = light->GetWorldDirection();
            safeSet("direction");
            shader.SetVec3(uniformName, direction);
        } else if (light->GetType() == LightComponent::LightType::Point) {
            glm::vec3 position = light->GetWorldPosition();
            safeSet("position");
            shader.SetVec3(uniformName, position);
            glm::vec3 attenuation = light->GetAttenuation();
            safeSet("attenuation");
            shader.SetVec3(uniformName, attenuation);
        } else if (light->GetType() == LightComponent::LightType::Spot) {
            glm::vec3 position = light->GetWorldPosition();
            safeSet("position");
            shader.SetVec3(uniformName, position);
            glm::vec3 direction = light->GetWorldDirection();
            safeSet("direction");
            shader.SetVec3(uniformName, direction);
            glm::vec3 attenuation = light->GetAttenuation();
            safeSet("attenuation");
            shader.SetVec3(uniformName, attenuation);
            float innerCone = glm::cos(light->GetInnerConeAngle());
            safeSet("innerCone");
            shader.SetFloat(uniformName, innerCone);
            float outerCone = glm::cos(light->GetOuterConeAngle());
            safeSet("outerCone");
            shader.SetFloat(uniformName, outerCone);
        }
    }

    shader.SetInt("uNumLights", static_cast<int>(slot));

    for (size_t i = slot; i < MAX_LIGHTS; ++i) {
        std::snprintf(uniformName, sizeof(uniformName), "uLights[%zu].type", i);
        shader.SetInt(uniformName, -1);
    }
}

} // namespace gm

