#pragma once

#include "gm/scene/Component.hpp"

#include <glm/vec3.hpp>
#include <glm/common.hpp>
#include <string>

namespace gm {

class WeatherEmitterComponent : public Component {
public:
    enum class ParticleType : std::uint32_t {
        Rain = 0,
        Snow = 1,
        Dust = 2
    };

    enum class VolumeShape : std::uint32_t {
        Box = 0,
        Cylinder = 1
    };

    WeatherEmitterComponent() = default;
    ~WeatherEmitterComponent() override = default;

    ParticleType GetType() const { return m_type; }
    void SetType(ParticleType type) { m_type = type; }

    VolumeShape GetVolumeShape() const { return m_shape; }
    void SetVolumeShape(VolumeShape shape) { m_shape = shape; }

    const glm::vec3& GetVolumeExtents() const { return m_volumeExtents; }
    void SetVolumeExtents(const glm::vec3& extents) { m_volumeExtents = glm::max(extents, glm::vec3(0.1f)); }

    const glm::vec3& GetDirection() const { return m_direction; }
    void SetDirection(const glm::vec3& dir) { m_direction = dir; }

    float GetSpawnRate() const { return m_spawnRate; }
    void SetSpawnRate(float rate) { m_spawnRate = rate; }

    float GetParticleLifetime() const { return m_particleLifetime; }
    void SetParticleLifetime(float seconds) { m_particleLifetime = seconds; }

    float GetParticleSpeed() const { return m_particleSpeed; }
    void SetParticleSpeed(float speed) { m_particleSpeed = speed; }

    float GetParticleSize() const { return m_particleSize; }
    void SetParticleSize(float size) { m_particleSize = size; }

    bool GetAlignToWind() const { return m_alignToWind; }
    void SetAlignToWind(bool align) { m_alignToWind = align; }

    float GetMaxParticlesHigh() const { return m_maxParticlesHigh; }
    float GetMaxParticlesMedium() const { return m_maxParticlesMedium; }
    float GetMaxParticlesLow() const { return m_maxParticlesLow; }
    void SetMaxParticlesHigh(float value) { m_maxParticlesHigh = value; }
    void SetMaxParticlesMedium(float value) { m_maxParticlesMedium = value; }
    void SetMaxParticlesLow(float value) { m_maxParticlesLow = value; }

    const std::string& GetProfileTag() const { return m_profileTag; }
    void SetProfileTag(std::string tag) { m_profileTag = std::move(tag); }

    const glm::vec3& GetBaseColor() const { return m_baseColor; }
    void SetBaseColor(const glm::vec3& color) { m_baseColor = color; }

private:
    ParticleType m_type = ParticleType::Rain;
    VolumeShape m_shape = VolumeShape::Box;
    glm::vec3 m_volumeExtents = glm::vec3(6.0f, 8.0f, 6.0f);
    glm::vec3 m_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 m_baseColor = glm::vec3(0.85f, 0.9f, 1.0f);
    float m_spawnRate = 250.0f;
    float m_particleLifetime = 3.5f;
    float m_particleSpeed = 12.0f;
    float m_particleSize = 0.15f;
    float m_maxParticlesHigh = 1500.0f;
    float m_maxParticlesMedium = 800.0f;
    float m_maxParticlesLow = 300.0f;
    bool m_alignToWind = true;
    std::string m_profileTag = "default";
};

} // namespace gm


