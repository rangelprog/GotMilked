#pragma once

#include <functional>
#include <string>

#include "gm/scene/SceneManager.hpp"

struct GLFWwindow;

namespace gm::core {

struct GameAppConfig {
    int width = 1280;
    int height = 720;
    std::string title = "GotMilked";
    bool enableVsync = true;
    bool enableDepthTest = true;
    bool showFpsInTitle = true;
    double fpsTitleUpdateIntervalSeconds = 0.5;
};

struct GameAppContext {
    GLFWwindow* window = nullptr;
    SceneManager* sceneManager = nullptr;
    std::function<void()> requestExit;
    std::function<void(bool)> setVSyncEnabled;
    std::function<bool()> isVSyncEnabled;
};

struct GameAppCallbacks {
    std::function<bool(GameAppContext&)> onInit;
    std::function<void(GameAppContext&, float)> onUpdate;
    std::function<void(GameAppContext&)> onRender;
    std::function<void(GameAppContext&)> onShutdown;
};

class GameApp {
public:
    explicit GameApp(GameAppConfig config = {});

    int Run(const GameAppCallbacks& callbacks);

    void RequestExit();
    void SetVSyncEnabled(bool enabled);
    bool IsVSyncEnabled() const { return m_vsyncEnabled; }

private:
    static void ErrorCallback(int code, const char* desc);

    bool InitializeWindow();
    void ShutdownWindow();

    GameAppConfig m_config;
    GLFWwindow* m_window = nullptr;
    bool m_exitRequested = false;
    bool m_vsyncEnabled = true;
    SceneManager m_sceneManager;
};

} // namespace gm::core

