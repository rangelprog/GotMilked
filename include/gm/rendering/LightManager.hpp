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

    // Clear collected lights
    void Clear() { m_lights.clear(); }

    // Get collected lights
    const std::vector<LightComponent*>& GetLights() const { return m_lights; }
    size_t GetLightCount() const { return m_lights.size(); }

    // Maximum number of lights supported by shader
    static constexpr size_t MAX_LIGHTS = 8;

private:
    std::vector<LightComponent*> m_lights; // Non-owning pointers
};

} // namespace gm

