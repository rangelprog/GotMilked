#pragma once
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace gm {
class LightComponent;
class Shader;
}

namespace gm {

/**
 * @brief Manages lights in a scene and applies them to shaders
 * 
 * Collects all active lights from GameObjects and provides
 * methods to apply lighting data to shaders.
 */
class LightManager {
public:
    LightManager();
    ~LightManager() = default;

    // Collect lights from scene (call before rendering)
    void CollectLights(const std::vector<std::shared_ptr<class GameObject>>& gameObjects);

    // Apply lighting to shader
    void ApplyLights(Shader& shader, const glm::vec3& viewPos) const;

    // Celestial overrides
    void SetCelestialLights(const glm::vec3& sunDirection,
                            const glm::vec3& sunColor,
                            float sunIntensity,
                            const glm::vec3& moonDirection,
                            const glm::vec3& moonColor,
                            float moonIntensity);
    void ClearCelestialLights();

    // Clear collected lights
    void Clear() { m_lights.clear(); }

    // Get collected lights
    const std::vector<LightComponent*>& GetLights() const { return m_lights; }
    size_t GetLightCount() const { return m_lights.size(); }

    // Maximum number of lights supported by shader
    static constexpr size_t MAX_LIGHTS = 8;

private:
    struct DirectionalOverride {
        bool enabled = false;
        glm::vec3 direction{0.0f, -1.0f, 0.0f};
        glm::vec3 color{1.0f};
        float intensity = 0.0f;
    };

    std::vector<LightComponent*> m_lights; // Non-owning pointers
    DirectionalOverride m_sunOverride;
    DirectionalOverride m_moonOverride;

    static glm::vec3 NormalizeFallback(const glm::vec3& dir);
    static DirectionalOverride BuildOverride(const glm::vec3& direction,
                                             const glm::vec3& color,
                                             float intensity);
};

} // namespace gm

