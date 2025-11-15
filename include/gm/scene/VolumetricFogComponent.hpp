#pragma once

#include "gm/scene/Component.hpp"
#include <glm/vec3.hpp>

namespace gm {

class VolumetricFogComponent : public Component {
public:
    VolumetricFogComponent() = default;

    float GetDensity() const { return m_density; }
    void SetDensity(float density) { m_density = density; }

    float GetHeightFalloff() const { return m_heightFalloff; }
    void SetHeightFalloff(float falloff) { m_heightFalloff = falloff; }

    float GetMaxDistance() const { return m_maxDistance; }
    void SetMaxDistance(float distance) { m_maxDistance = distance; }

    glm::vec3 GetColor() const { return m_color; }
    void SetColor(const glm::vec3& color) { m_color = color; }

    float GetNoiseScale() const { return m_noiseScale; }
    void SetNoiseScale(float scale) { m_noiseScale = scale; }

    float GetNoiseSpeed() const { return m_noiseSpeed; }
    void SetNoiseSpeed(float speed) { m_noiseSpeed = speed; }

    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }

private:
    float m_density = 0.02f;
    float m_heightFalloff = 1.0f;
    float m_maxDistance = 80.0f;
    glm::vec3 m_color{0.85f, 0.93f, 1.0f};
    float m_noiseScale = 0.5f;
    float m_noiseSpeed = 0.1f;
    bool m_enabled = true;
};

} // namespace gm


