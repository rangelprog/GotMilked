#include "gm/core/GameApp.hpp"

#include <cstdio>
#include <cstdlib>
#include <utility>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

#include "gm/core/Logger.hpp"
#include "gm/core/input/InputManager.hpp"
#include "gm/scene/SceneManager.hpp"

namespace gm::core {

GameApp::GameApp(GameAppConfig config)
    : m_config(std::move(config)),
      m_vsyncEnabled(m_config.enableVsync) {}

void GameApp::RequestExit() {
    m_exitRequested = true;
    if (m_window) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }
}

void GameApp::SetVSyncEnabled(bool enabled) {
    m_vsyncEnabled = enabled;
    if (m_window) {
        glfwSwapInterval(enabled ? 1 : 0);
    }
}

int GameApp::Run(const GameAppCallbacks& callbacks) {
    if (!InitializeWindow()) {
        return EXIT_FAILURE;
    }

    GameAppContext context{
        m_window,
        &m_sceneManager,
        [this]() { RequestExit(); },
        [this](bool enabled) { SetVSyncEnabled(enabled); },
        [this]() -> bool { return IsVSyncEnabled(); }
    };

    auto& inputManager = InputManager::Instance();
    inputManager.Init(m_window);

    bool initOk = true;
    if (callbacks.onInit) {
        initOk = callbacks.onInit(context);
    }

    if (!initOk) {
        if (callbacks.onShutdown) {
            callbacks.onShutdown(context);
        }
        m_sceneManager.Shutdown();
        ShutdownWindow();
        return EXIT_FAILURE;
    }

    m_sceneManager.InitActiveScene();

    double lastTime = glfwGetTime();
    double lastTitleUpdate = lastTime;
    int frameCount = 0;

    while (!m_exitRequested && !glfwWindowShouldClose(m_window)) {
        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;

        inputManager.Update();
        glfwPollEvents();

        m_sceneManager.UpdateActiveScene(dt);

        if (callbacks.onUpdate) {
            callbacks.onUpdate(context, dt);
        }

        if (callbacks.onRender) {
            callbacks.onRender(context);
        }

        if (m_config.showFpsInTitle && m_config.fpsTitleUpdateIntervalSeconds > 0.0) {
            frameCount++;
            if (now - lastTitleUpdate >= m_config.fpsTitleUpdateIntervalSeconds) {
                double fps = frameCount / (now - lastTitleUpdate);
                lastTitleUpdate = now;
                frameCount = 0;

                char buf[256];
                std::snprintf(buf, sizeof(buf), "%s | FPS: %.1f", m_config.title.c_str(), fps);
                glfwSetWindowTitle(m_window, buf);
            }
        }

        glfwSwapBuffers(m_window);
    }

    if (callbacks.onShutdown) {
        callbacks.onShutdown(context);
    }

    m_sceneManager.Shutdown();
    ShutdownWindow();
    return EXIT_SUCCESS;
}

void GameApp::ErrorCallback(int code, const char* desc) {
    core::Logger::Error("[GameApp] GLFW error {}: {}", code, desc ? desc : "unknown");
}

bool GameApp::InitializeWindow() {
    glfwSetErrorCallback(ErrorCallback);

    if (!glfwInit()) {
        core::Logger::Error("[GameApp] Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(
        m_config.width,
        m_config.height,
        m_config.title.c_str(),
        nullptr,
        nullptr);

    if (!m_window) {
        core::Logger::Error("[GameApp] Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        core::Logger::Error("[GameApp] Failed to initialize GLAD");
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
        return false;
    }

    if (m_config.enableDepthTest) {
        glEnable(GL_DEPTH_TEST);
    }

    SetVSyncEnabled(m_config.enableVsync);
    return true;
}

void GameApp::ShutdownWindow() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

} // namespace gm::core

