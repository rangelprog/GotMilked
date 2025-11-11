#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <vector>
struct GLFWwindow;

namespace gm {
    class Shader;
    class Texture;
    class Mesh;
    class Camera;
    class Scene;
    class GameObject;
    namespace core { class InputManager; }
    namespace scene { struct Transform; }
}

#include "gm/scene/GameObject.hpp"
#include "gm/utils/Config.hpp"
#include "SandboxResources.hpp"
#include "gameplay/SandboxGameplay.hpp"
#include "ResourceHotReloader.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "tooling/ToolingOverlay.hpp"

namespace sandbox::save {
class SaveManager;
}

class Game {
public:
    Game(const gm::utils::AppConfig& config);
    ~Game();

    bool Init(GLFWwindow* window);
    void Update(float dt);
    void Render();
    void Shutdown();

private:
    gm::utils::AppConfig m_config;
    std::filesystem::path m_assetsDir;
    GLFWwindow* m_window = nullptr;

    // Scene management
    std::shared_ptr<gm::Scene> m_gameScene;
    std::vector<std::shared_ptr<gm::GameObject>> m_spinnerObjects; // Mesh spinner objects for demonstration

    SandboxResources m_resources;
    sandbox::ResourceHotReloader m_hotReloader;

    // camera / state
    std::unique_ptr<gm::Camera> m_camera;
    std::unique_ptr<sandbox::gameplay::SandboxGameplay> m_gameplay;
    std::unique_ptr<sandbox::save::SaveManager> m_saveManager;
    std::unique_ptr<gm::utils::ImGuiManager> m_imgui;
    std::unique_ptr<sandbox::tooling::ToolingOverlay> m_tooling;
    bool m_overlayVisible = false;
    
    void PerformQuickSave();
    void PerformQuickLoad();
    void ForceResourceReload();

    // Helper methods
    void SetupScene();
    void SetupResourceHotReload();
    void ApplyResourcesToScene();
};
