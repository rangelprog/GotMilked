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
}

#include "gm/scene/GameObject.hpp"
#include "gm/utils/Config.hpp"
#include "GameResources.hpp"
#include "gm/gameplay/FlyCameraController.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#ifdef _DEBUG
#include "DebugMenu.hpp"
#endif

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

    GameResources m_resources;
    gm::utils::HotReloader m_hotReloader;

    // camera / state
    std::unique_ptr<gm::Camera> m_camera;
    std::unique_ptr<gm::gameplay::FlyCameraController> m_gameplay;
    std::unique_ptr<gm::save::SaveManager> m_saveManager;
    std::unique_ptr<gm::utils::ImGuiManager> m_imgui;
    std::unique_ptr<gm::tooling::Overlay> m_tooling;
#ifdef _DEBUG
    std::unique_ptr<DebugMenu> m_debugMenu;
    bool m_debugMenuVisible = false;
#endif
    bool m_overlayVisible = false;
    
    void PerformQuickSave();
    void PerformQuickLoad();
    void ForceResourceReload();

    // Initialization helpers
    bool SetupPhysics();
    bool SetupRendering();
    void SetupInput();
    void SetupGameplay();
    void SetupSaveSystem();
    bool SetupDebugTools();
#ifdef _DEBUG
    void SetupDebugMenu();
#endif
    
    // Helper methods
    void SetupScene();
    void SetupResourceHotReload();
    void ApplyResourcesToScene();
    void ApplyResourcesToTerrain();
    void ApplyResourcesToStaticMeshComponents();
    void SetupEventSubscriptions();
};
