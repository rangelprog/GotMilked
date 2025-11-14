#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/mat4x4.hpp>
#include <unordered_set>
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
    class GridRenderer;
    class EditableTerrainComponent;
}
#include "gm/debug/TerrainEditingSystem.hpp"
#endif

#include "gm/scene/GameObject.hpp"
#include "gm/scene/SceneManager.hpp"
#include "gm/utils/Config.hpp"
#include "GameResources.hpp"
#include "GameConstants.hpp"
#include "gm/gameplay/CameraRigSystem.hpp"
#include "gm/gameplay/QuestTriggerSystem.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/scene/PrefabLibrary.hpp"
#include "gm/core/Event.hpp"
#if GM_DEBUG_TOOLS
#include "DebugMenu.hpp"
#endif

class GameBootstrapper;
class GameRenderer;
class DebugToolingController;
class SceneResourceController;
class GameShutdownController;
class ToolingFacade;
class EventRouter;
class GameLoopController;

#if GM_DEBUG_TOOLS
namespace gm::gameplay {
class FlyCameraController;
}
#endif

class Game {
public:
    Game(const gm::utils::AppConfig& config);
    ~Game();

    bool Init(GLFWwindow* window, gm::SceneManager& sceneManager);
    void Update(float dt);
    void Render();
    void Shutdown();
    ToolingFacade* GetToolingFacade() const { return m_toolingFacade.get(); }
    gm::Camera* GetRenderCamera() const;
    float GetRenderCameraFov() const;
#if GM_DEBUG_TOOLS
    void SetDebugViewportCameraActive(bool enabled);
    bool IsDebugViewportCameraActive() const;
    void UpdateViewportCamera(float deltaTime, bool inputSuppressed);
#endif

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
    std::shared_ptr<gm::gameplay::CameraRigSystem> m_cameraRigSystem;
    std::shared_ptr<gm::gameplay::QuestTriggerSystem> m_questSystem;
    std::unique_ptr<gm::save::SaveManager> m_saveManager;
    std::unique_ptr<gm::utils::ImGuiManager> m_imgui;
    std::unique_ptr<gm::tooling::Overlay> m_tooling;
#if GM_DEBUG_TOOLS
    std::unique_ptr<gm::debug::DebugMenu> m_debugMenu;
    std::unique_ptr<gm::debug::DebugConsole> m_debugConsole;
    std::unique_ptr<gm::debug::DebugHudController> m_debugHud;
    std::unique_ptr<gm::debug::GridRenderer> m_gridRenderer;
    std::shared_ptr<gm::debug::TerrainEditingSystem> m_terrainEditingSystem;
#endif
    std::shared_ptr<gm::scene::PrefabLibrary> m_prefabLibrary;
    bool m_overlayVisible = false;
    bool m_vsyncEnabled = true;  // Track VSync state
#if GM_DEBUG_TOOLS
    bool m_gridVisible = false;
#endif
    std::unordered_set<std::string> m_completedQuests;
    gm::SceneManager* m_sceneManager = nullptr;
    std::unique_ptr<GameBootstrapper> m_bootstrapper;
    std::unique_ptr<GameRenderer> m_renderer;
    std::unique_ptr<DebugToolingController> m_debugTooling;
    std::unique_ptr<SceneResourceController> m_sceneResources;
    std::unique_ptr<GameShutdownController> m_shutdownController;
    std::unique_ptr<ToolingFacade> m_toolingFacade;
    std::unique_ptr<EventRouter> m_eventRouter;
    std::unique_ptr<GameLoopController> m_loopController;
    
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
    void EnsureCameraRig();

    friend class GameBootstrapper;
    friend class GameRenderer;
    friend class DebugToolingController;
    friend class SceneResourceController;
    friend class GameShutdownController;
    friend class ToolingFacade;
    friend class GameLoopController;

#if GM_DEBUG_TOOLS
    std::unique_ptr<gm::Camera> m_viewportCamera;
    std::unique_ptr<gm::gameplay::FlyCameraController> m_viewportCameraController;
    bool m_viewportCameraActive = false;
    bool m_viewportCameraHasSavedPose = false;
    glm::vec3 m_viewportSavedPosition{0.0f};
    glm::vec3 m_viewportSavedForward{0.0f, 0.0f, -1.0f};
    float m_viewportSavedFov = gotmilked::GameConstants::Camera::DefaultFovDegrees;
#endif
};
