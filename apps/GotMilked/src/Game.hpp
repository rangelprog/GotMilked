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

#if GM_DEBUG_TOOLS
namespace gm::debug {
    class DebugConsole;
    class DebugMenu;
    class DebugHudController;
    class EditableTerrainComponent;
}
#endif

#include "gm/scene/GameObject.hpp"
#include "gm/utils/Config.hpp"
#include "GameResources.hpp"
#include "gm/gameplay/FlyCameraController.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/scene/PrefabLibrary.hpp"
#if GM_DEBUG_TOOLS
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
#if GM_DEBUG_TOOLS
    std::unique_ptr<gm::debug::DebugMenu> m_debugMenu;
    std::unique_ptr<gm::debug::DebugConsole> m_debugConsole;
    std::unique_ptr<gm::debug::DebugHudController> m_debugHud;
#endif
    std::shared_ptr<gm::scene::PrefabLibrary> m_prefabLibrary;
    bool m_overlayVisible = false;
    bool m_vsyncEnabled = true;  // Track VSync state
    
    void PerformQuickSave();
    void PerformQuickLoad();
    void ForceResourceReload();

    // Initialization helpers
    bool SetupLogging();
    bool SetupPhysics();
    bool SetupRendering();
    void SetupInput();
    void SetupGameplay();
    void SetupSaveSystem();
    bool SetupDebugTools();
#if GM_DEBUG_TOOLS
    void SetupDebugMenu();
#endif
    bool SetupPrefabs();
    
    // Helper methods
    void SetupScene();
    void SetupResourceHotReload();
    void ApplyResourcesToScene();
#if GM_DEBUG_TOOLS
    void ApplyResourcesToTerrain();
#endif
    void ApplyResourcesToStaticMeshComponents();
    void SetupEventSubscriptions();
};
