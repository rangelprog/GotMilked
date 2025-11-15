#include "GameRenderer.hpp"

#include "Game.hpp"
#include "GameResources.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/core/Logger.hpp"
#include "ToolingFacade.hpp"
#include "gm/utils/Profiler.hpp"
#include "GameConstants.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fmt/format.h>
#include <vector>

GameRenderer::GameRenderer(Game& game)
    : m_game(game) {
    gm::rendering::CascadeShadowSettings settings;
    settings.cascadeCount = 4;
    settings.baseResolution = 1024;
    m_shadowCascades.SetSettings(settings);
}

void GameRenderer::Render() {
    gm::utils::Profiler::ScopedTimer frameTimer("GameRenderer::Render");
    if (!m_game.m_window) return;
    if (!m_game.m_resources.GetShader()) {
        gm::core::Logger::Warning("[Game] Cannot render - shader not loaded");
        return;
    }

    ProcessEnvironmentCaptureRequests();

    if (!m_skyInitialized) {
        m_skyInitialized = m_skyRenderer.Initialize(m_game.m_resources);
    } else {
        gm::Shader* currentSky = m_skyRenderer.ActiveSkyShader();
        gm::Shader* desiredSky = m_game.m_resources.GetSkyShader();
        gm::Shader* currentGradient = m_skyRenderer.ActiveGradientShader();
        gm::Shader* desiredGradient = m_game.m_resources.GetSkyGradientShader();
        if (desiredSky != currentSky || desiredGradient != currentGradient) {
            m_skyInitialized = m_skyRenderer.Initialize(m_game.m_resources);
        }
    }
    if (!m_fogInitialized) {
        m_fogInitialized = m_fogRenderer.Initialize(m_game.m_resources);
    }
    if (!m_weatherInitialized) {
        m_weatherInitialized = m_weatherParticles.Initialize(m_game.m_resources);
        m_weatherParticles.SetQuality(m_game.GetWeatherQuality());
    }

    int fbw, fbh;
    glfwGetFramebufferSize(m_game.m_window, &fbw, &fbh);
    if (fbw <= 0 || fbh <= 0) {
        return;
    }
    glViewport(0, 0, fbw, fbh);
    glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = static_cast<float>(fbw) / static_cast<float>(fbh);
    float fov = m_game.GetRenderCameraFov();
    const float nearPlane = gotmilked::GameConstants::Camera::NearPlane;
    const float farPlane = gotmilked::GameConstants::Camera::FarPlane;
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
    auto* activeCamera = m_game.GetRenderCamera();
    if (!activeCamera) {
        return;
    }
    glm::mat4 view = activeCamera->View();

    const auto sunState = m_game.GetSunMoonState();
    const auto& celestialConfig = m_game.GetCelestialConfig();
    m_shadowCascades.Update(view,
                            proj,
                            nearPlane,
                            farPlane,
                            sunState.sunDirection,
                            sunState.sunElevationDeg);
    m_game.UpdateShadowCascades(m_shadowCascades);

    if (m_skyInitialized) {
        m_skyRenderer.Render(sunState,
                             celestialConfig,
                             view,
                             proj,
                             !celestialConfig.useGradientSky);
    }

    if (m_game.m_toolingFacade) {
        gm::utils::Profiler::ScopedTimer toolingTimer("GameRenderer::ToolingBegin");
        m_game.m_toolingFacade->BeginFrame();
        m_game.m_toolingFacade->RenderGrid(view, proj);
        if (m_game.m_debugMenu) {
            m_game.m_debugMenu->SetWeatherDiagnosticsSource(&m_weatherParticles);
        }
    }

    if (m_game.m_gameScene && activeCamera) {
        if (auto* shader = m_game.m_resources.GetShader()) {
            const auto& weather = m_game.GetWeatherState();
            shader->Use();
            shader->SetVec4("uWeatherSurface",
                            glm::vec4(weather.surfaceWetness,
                                      weather.puddleAmount,
                                      weather.surfaceDarkening,
                                      0.0f));
            shader->SetVec3("uWeatherTint", weather.surfaceTint);
        }
        gm::utils::Profiler::ScopedTimer sceneTimer("GameRenderer::DrawScene");
        m_game.m_gameScene->Draw(*m_game.m_resources.GetShader(),
                                 *activeCamera,
                                 fbw,
                                 fbh,
                                 fov,
                                 nearPlane,
                                 farPlane);
    }
    if (m_weatherInitialized && m_game.m_gameScene) {
        m_weatherParticles.Update(*m_game.m_gameScene,
                                  m_game.GetWeatherState(),
                                  m_game.GetWeatherProfiles(),
                                  m_game.GetLastDeltaTime());
        glm::vec3 cameraRight = glm::normalize(glm::vec3(view[0][0], view[1][0], view[2][0]));
        glm::vec3 cameraUp = glm::normalize(glm::vec3(view[0][1], view[1][1], view[2][1]));
        m_weatherParticles.Render(view, proj, cameraRight, cameraUp);
    }
    if (m_fogInitialized && m_game.m_gameScene && activeCamera) {
        gm::utils::Profiler::ScopedTimer fogTimer("GameRenderer::VolumetricFog");
        const float timeSeconds = static_cast<float>(glfwGetTime());
        m_fogRenderer.Render(m_game.m_gameScene.get(),
                             *activeCamera,
                             view,
                             proj,
                             fbw,
                             fbh,
                             nearPlane,
                             farPlane,
                             sunState,
                             timeSeconds);
    }
    if (m_game.m_toolingFacade) {
        gm::utils::Profiler::ScopedTimer toolingUiTimer("GameRenderer::ToolingUI");
        m_game.m_toolingFacade->RenderUI();
    }
}

void GameRenderer::ProcessEnvironmentCaptureRequests() {
    auto flags = m_game.ConsumeEnvironmentCaptureFlags();
    if (flags == EnvironmentCaptureFlags::None) {
        return;
    }

    std::vector<const char*> tasks;
    if (HasEnvironmentCaptureFlag(flags, EnvironmentCaptureFlags::Reflection)) {
        tasks.push_back("reflection captures");
    }
    if (HasEnvironmentCaptureFlag(flags, EnvironmentCaptureFlags::LightProbe)) {
        tasks.push_back("light probes");
    }

    if (!tasks.empty()) {
        std::string summary = fmt::format("Refreshing {}", tasks[0]);
        for (size_t i = 1; i < tasks.size(); ++i) {
            summary += fmt::format(" & {}", tasks[i]);
        }
        gm::core::Logger::Info("[Renderer] {}", summary);
        if (auto* tooling = m_game.GetToolingFacade()) {
            tooling->AddNotification(summary);
        }
    }

    // TODO: Integrate with actual probe/reflection capture subsystems when available.
    m_game.NotifyEnvironmentCapturePerformed(flags);
}

