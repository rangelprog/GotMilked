#include "SkyRenderer.hpp"

#include "GameResources.hpp"
#include "gm/rendering/Shader.hpp"
#include "gm/scene/TimeOfDayController.hpp"
#include <glad/glad.h>
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
constexpr const char* kSkyShaderGuid = "shader::sky";
}

SkyRenderer::SkyRenderer() = default;
SkyRenderer::~SkyRenderer() {
    Shutdown();
}

bool SkyRenderer::Initialize(GameResources& resources) {
    Shutdown();
    m_shader = resources.GetSkyShader();
    m_gradientShader = resources.GetSkyGradientShader();
    if (!m_shader && !m_gradientShader) {
        return false;
    }
    EnsureBuffers();
    return m_vao != 0;
}

void SkyRenderer::Shutdown() {
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao != 0) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    m_shader = nullptr;
    m_gradientShader = nullptr;
}

void SkyRenderer::EnsureBuffers() {
    if (m_vao != 0) {
        return;
    }

    const float vertices[] = {
        -1.0f, -1.0f,
         3.0f, -1.0f,
        -1.0f,  3.0f,
    };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, reinterpret_cast<void*>(0));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void SkyRenderer::UploadPhysicallyBased(const gm::scene::SunMoonState& state,
                                        const gm::scene::CelestialConfig& config,
                                        const glm::mat4& view,
                                        const glm::mat4& proj) {
    const glm::vec3 sunDir = glm::normalize(state.sunDirection);
    const float elevation = glm::clamp(state.sunElevationDeg / 90.0f, -1.0f, 1.0f);
    const glm::vec3 horizonColor(0.9f, 0.55f, 0.35f);
    const glm::vec3 zenithDay(0.12f, 0.35f, 0.65f);
    const glm::vec3 zenithNight(0.02f, 0.02f, 0.08f);
    const glm::vec3 zenithColor = glm::mix(zenithNight, zenithDay, glm::clamp(elevation * 0.5f + 0.5f, 0.0f, 1.0f));

    m_shader->SetVec3("uSunDirection", sunDir);
    m_shader->SetVec3("uZenithColor", zenithColor);
    m_shader->SetVec3("uHorizonColor", horizonColor);
    m_shader->SetVec3("uGroundAlbedo", config.groundAlbedo);
    m_shader->SetFloat("uTurbidity", config.turbidity);
    m_shader->SetFloat("uExposure", state.exposureCompensation * config.exposure);
    m_shader->SetFloat("uAirDensity", config.airDensity);
    m_shader->SetMat4("uView", view);
    m_shader->SetMat4("uProj", proj);
}

void SkyRenderer::UploadGradient(const gm::scene::SunMoonState& state,
                                 const gm::scene::CelestialConfig& config) {
    const float elevation = glm::clamp(state.sunElevationDeg / 90.0f, -1.0f, 1.0f);
    const glm::vec3 topColor = glm::mix(glm::vec3(0.02f, 0.02f, 0.08f),
                                        glm::vec3(0.12f, 0.35f, 0.65f),
                                        elevation * 0.5f + 0.5f);
    const glm::vec3 bottomColor = glm::mix(config.groundAlbedo,
                                           glm::vec3(0.9f, 0.55f, 0.35f),
                                           glm::clamp(elevation + 0.2f, 0.0f, 1.0f));
    if (m_gradientShader) {
        m_gradientShader->SetVec3("uTopColor", topColor);
        m_gradientShader->SetVec3("uBottomColor", bottomColor);
    }
}

void SkyRenderer::Render(const gm::scene::SunMoonState& state,
                         const gm::scene::CelestialConfig& config,
                         const glm::mat4& view,
                         const glm::mat4& proj,
                         bool highQuality) {
    if (m_vao == 0) {
        return;
    }

    gm::Shader* shader = nullptr;
    bool usePhysical = highQuality && m_shader;
    if (usePhysical) {
        shader = m_shader;
    } else if (m_gradientShader) {
        shader = m_gradientShader;
    } else if (m_shader) {
        shader = m_shader;
        usePhysical = true;
    }

    if (!shader) {
        return;
    }

    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(m_vao);
    shader->Use();
    if (usePhysical) {
        UploadPhysicallyBased(state, config, view, proj);
    } else {
        UploadGradient(state, config);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}


