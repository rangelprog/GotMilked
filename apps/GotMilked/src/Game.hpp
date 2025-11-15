#pragma once

#include <memory>
#include <string>
#include <filesystem>
#include <vector>
#include <glm/mat4x4.hpp>
#include <glm/mat4x4.hpp>
#include <unordered_set>
#include <unordered_map>
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
#include "gameplay/CameraRigSystem.hpp"
#include "gameplay/QuestTriggerSystem.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/save/SaveManager.hpp"
#include "gm/save/SaveSnapshotHelpers.hpp"
#include "gm/content/ContentDatabase.hpp"
#include "gm/scene/PrefabLibrary.hpp"
#include "gm/scene/TimeOfDayController.hpp"
#include "gm/scene/LightComponent.hpp"
#include "gm/core/Event.hpp"
#include "WeatherTypes.hpp"
#include <nlohmann/json_fwd.hpp>
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

namespace gm::core {
struct GameAppContext;
}

namespace gm::rendering {
class CascadeShadowMap;
}

class Game {
public:
    Game(const gm::utils::AppConfig& config);
    ~Game();

    bool Init(GLFWwindow* window, gm::SceneManager& sceneManager);
    void BindAppContext(gm::core::GameAppContext& context);
    void Update(float dt);
    void Render();
    void Shutdown();
    ToolingFacade* GetToolingFacade() const { return m_toolingFacade.get(); }
    DebugToolingController* DebugTooling() { return m_debugTooling.get(); }
    const DebugToolingController* DebugTooling() const { return m_debugTooling.get(); }
    GLFWwindow* Window() const { return m_window; }
    void SetWindow(GLFWwindow* window) { m_window = window; }
    gm::SceneManager* SceneManager() const { return m_sceneManager; }
    void SetSceneManager(gm::SceneManager& manager) { m_sceneManager = &manager; }
    gm::content::ContentDatabase* ContentDatabase() const { return m_contentDatabase.get(); }
    void SetVSyncEnabled(bool enabled);
    bool IsVSyncEnabled() const;
    void RequestExit() const;
    gm::core::GameAppContext* AppContext() const { return m_appContext; }
    const gm::utils::AppConfig& Config() const { return m_config; }
    gm::Camera* GetRenderCamera() const;
    float GetRenderCameraFov() const;
    const gm::scene::CelestialConfig& GetCelestialConfig() const { return m_timeOfDayController.GetConfig(); }
    gm::scene::SunMoonState GetSunMoonState() const { return m_sunMoonState; }
    float GetTimeOfDayNormalized() const { return m_timeOfDayController.GetNormalizedTime(); }
    const WeatherState& GetWeatherState() const { return m_weatherState; }
    const std::unordered_map<std::string, WeatherProfile>& GetWeatherProfiles() const { return m_weatherProfiles; }
    WeatherQuality GetWeatherQuality() const { return m_weatherQuality; }
    float GetLastDeltaTime() const { return m_lastDeltaTime; }
    void SetCelestialConfig(const gm::scene::CelestialConfig& config);
    void SetTimeOfDayNormalized(float normalized);
    void SetWeatherProfile(const std::string& profileName);
    void SetWeatherQuality(WeatherQuality quality) { m_weatherQuality = quality; }
    void UpdateShadowCascades(const gm::rendering::CascadeShadowMap& cascades);
    void UpdateExposure(float dt);
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
    gm::scene::TimeOfDayController m_timeOfDayController;
    gm::scene::SunMoonState m_sunMoonState{};
    float m_exposureAccumulator = 1.0f;
    std::weak_ptr<gm::LightComponent> m_sunLight;
    std::weak_ptr<gm::LightComponent> m_moonLight;
    bool m_overlayVisible = false;
    bool m_vsyncEnabled = true;  // Track VSync state
#if GM_DEBUG_TOOLS
    bool m_gridVisible = false;
#endif
    std::unordered_set<std::string> m_completedQuests;
    WeatherQuality m_weatherQuality = WeatherQuality::High;
    WeatherState m_weatherState{};
    std::unordered_map<std::string, WeatherProfile> m_weatherProfiles;
    float m_lastDeltaTime = 0.016f;
    float m_weatherClock = 0.0f;
    WeatherStateEventPayload m_weatherEventPayload;
    gm::SceneManager* m_sceneManager = nullptr;
    std::unique_ptr<GameBootstrapper> m_bootstrapper;
    std::unique_ptr<GameRenderer> m_renderer;
    std::unique_ptr<DebugToolingController> m_debugTooling;
    std::unique_ptr<SceneResourceController> m_sceneResources;
    std::unique_ptr<GameShutdownController> m_shutdownController;
    std::unique_ptr<ToolingFacade> m_toolingFacade;
    std::unique_ptr<EventRouter> m_eventRouter;
    std::unique_ptr<GameLoopController> m_loopController;
    std::unique_ptr<gm::content::ContentDatabase> m_contentDatabase;
    gm::core::GameAppContext* m_appContext = nullptr;
    
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
    void LoadCelestialConfig();
    void CaptureCelestialLights();
    void UpdateCelestialLights();
    bool LoadWeatherProfiles();
    void UpdateWeather(float dt);
    WeatherProfile ParseProfile(const nlohmann::json& entry) const;
    WeatherState ParseInitialWeather(const nlohmann::json& data) const;
    const WeatherProfile& ResolveWeatherProfile(const std::string& name) const;
    void BroadcastWeatherEvent();
    
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
