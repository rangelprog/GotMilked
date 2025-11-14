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

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

GameRenderer::GameRenderer(Game& game)
    : m_game(game) {}

void GameRenderer::Render() {
    gm::utils::Profiler::ScopedTimer frameTimer("GameRenderer::Render");
    if (!m_game.m_window) return;
    if (!m_game.m_resources.GetShader()) {
        gm::core::Logger::Warning("[Game] Cannot render - shader not loaded");
        return;
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
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect, 0.1f, 200.0f);
    auto* activeCamera = m_game.GetRenderCamera();
    if (!activeCamera) {
        return;
    }
    glm::mat4 view = activeCamera->View();

    if (m_game.m_toolingFacade) {
        gm::utils::Profiler::ScopedTimer toolingTimer("GameRenderer::ToolingBegin");
        m_game.m_toolingFacade->BeginFrame();
        m_game.m_toolingFacade->RenderGrid(view, proj);
    }

    if (m_game.m_gameScene && activeCamera) {
        gm::utils::Profiler::ScopedTimer sceneTimer("GameRenderer::DrawScene");
        m_game.m_gameScene->Draw(*m_game.m_resources.GetShader(), *activeCamera, fbw, fbh, fov);
    }
    if (m_game.m_toolingFacade) {
        gm::utils::Profiler::ScopedTimer toolingUiTimer("GameRenderer::ToolingUI");
        m_game.m_toolingFacade->RenderUI();
    }
}

