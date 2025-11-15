#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace gm {
class Shader;
}

namespace gm::scene {
struct SunMoonState;
struct CelestialConfig;
}

class GameResources;

class SkyRenderer {
public:
    SkyRenderer();
    ~SkyRenderer();

    bool Initialize(GameResources& resources);
    void Render(const gm::scene::SunMoonState& state,
                const gm::scene::CelestialConfig& config,
                const glm::mat4& view,
                const glm::mat4& proj,
                bool highQuality);

    gm::Shader* ActiveSkyShader() const { return m_shader; }
    gm::Shader* ActiveGradientShader() const { return m_gradientShader; }
    void Shutdown();

private:
    void EnsureBuffers();
    void UploadPhysicallyBased(const gm::scene::SunMoonState& state,
                               const gm::scene::CelestialConfig& config,
                               const glm::mat4& view,
                               const glm::mat4& proj);
    void UploadGradient(const gm::scene::SunMoonState& state,
                        const gm::scene::CelestialConfig& config);

    gm::Shader* m_shader = nullptr;
    gm::Shader* m_gradientShader = nullptr;
    unsigned int m_vao = 0;
    unsigned int m_vbo = 0;
};


