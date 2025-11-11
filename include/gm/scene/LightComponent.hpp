#pragma once
#include "gm/scene/Component.hpp"
#include <glm/glm.hpp>

namespace gm {

/**
 * @brief Component that represents a light source in the scene
 * 
 * Supports three light types:
 * - Directional: Infinite light source (like sun)
 * - Point: Light at a position with attenuation
 * - Spot: Directional light with cone angle
 */
class LightComponent : public Component {
public:
    enum class LightType {
        Directional,
        Point,
        Spot
    };

    LightComponent();
    ~LightComponent() override = default;

    void Init() override {}
    void Update(float deltaTime) override {}
    void Render() override {}

    // Light type
    void SetType(LightType type) { m_type = type; }
    LightType GetType() const { return m_type; }

    // Color and intensity
    void SetColor(const glm::vec3& color) { m_color = color; }
    const glm::vec3& GetColor() const { return m_color; }
    void SetIntensity(float intensity) { m_intensity = intensity; }
    float GetIntensity() const { return m_intensity; }

    // Direction (for Directional and Spot lights)
    void SetDirection(const glm::vec3& direction) { m_direction = glm::normalize(direction); }
    const glm::vec3& GetDirection() const { return m_direction; }

    // Attenuation (for Point and Spot lights)
    void SetAttenuation(float constant, float linear, float quadratic) {
        m_attenuationConstant = constant;
        m_attenuationLinear = linear;
        m_attenuationQuadratic = quadratic;
    }
    glm::vec3 GetAttenuation() const {
        return glm::vec3(m_attenuationConstant, m_attenuationLinear, m_attenuationQuadratic);
    }

    // Spot light properties (stored internally as radians)
    void SetInnerConeAngle(float degrees) { m_innerConeAngle = glm::radians(degrees); }
    void SetOuterConeAngle(float degrees) { m_outerConeAngle = glm::radians(degrees); }
    float GetInnerConeAngle() const { return m_innerConeAngle; } // Returns radians
    float GetOuterConeAngle() const { return m_outerConeAngle; } // Returns radians

    // Enable/disable
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    bool IsEnabled() const { return m_enabled; }

    // Get world position (from TransformComponent)
    glm::vec3 GetWorldPosition() const;
    glm::vec3 GetWorldDirection() const;

private:
    LightType m_type = LightType::Directional;
    glm::vec3 m_color = glm::vec3(1.0f, 1.0f, 1.0f);
    float m_intensity = 1.0f;
    glm::vec3 m_direction = glm::vec3(0.0f, -1.0f, 0.0f); // Down by default

    // Attenuation (Point/Spot lights)
    float m_attenuationConstant = 1.0f;
    float m_attenuationLinear = 0.09f;
    float m_attenuationQuadratic = 0.032f;

    // Spot light
    float m_innerConeAngle = glm::radians(12.5f);
    float m_outerConeAngle = glm::radians(17.5f);

    bool m_enabled = true;
};

} // namespace gm

