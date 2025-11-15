#pragma once

#if GM_DEBUG_TOOLS

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <filesystem>
#include <cstdint>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <unordered_map>
#include <nlohmann/json_fwd.hpp>

#include <imgui.h>

#include "gm/animation/AnimationPose.hpp"
#include "gm/animation/AnimationClip.hpp"
#include "gm/animation/AnimationPoseEvaluator.hpp"
#include "gm/animation/Skeleton.hpp"
#include "gm/debug/EditorPlugin.hpp"
#include "gm/content/ContentDatabase.hpp"
#include "gm/scene/TimeOfDayController.hpp"
#include "WeatherTypes.hpp"

class WeatherParticleSystem;

namespace gm {
class Scene;
class GameObject;
}

class GameResources;

namespace gm::scene {
class PrefabLibrary;
class AnimatorComponent;
struct PrefabDefinition;
}

namespace gm::save {
class SaveManager;
}

struct ImGuiPayload;
struct ImVec2;

namespace gm::debug {
class DebugConsole;
class EditableTerrainComponent;

class DebugMenu : public EditorPluginHost {
public:
    DebugMenu();
    ~DebugMenu();

    struct Callbacks {
        std::function<void()> quickSave;
        std::function<void()> quickLoad;
        std::function<void()> reloadResources;
        std::function<void()> onSceneLoaded;  // Called after scene is loaded
        
        // Camera getters/setters for scene save/load
        std::function<glm::vec3()> getCameraPosition;
        std::function<glm::vec3()> getCameraForward;
        std::function<float()> getCameraFov;
        std::function<void(const glm::vec3&, const glm::vec3&, float)> setCamera;
        
        // World time getter for save format compatibility
        std::function<double()> getWorldTime;
        
        // Rendering callbacks for GameObject labels
        std::function<glm::mat4()> getViewMatrix;
        std::function<glm::mat4()> getProjectionMatrix;
        std::function<void(int&, int&)> getViewportSize;

        // Celestial/time-of-day controls
        std::function<float()> getTimeOfDayNormalized;
        std::function<void(float)> setTimeOfDayNormalized;
        std::function<gm::scene::CelestialConfig()> getCelestialConfig;
        std::function<void(const gm::scene::CelestialConfig&)> setCelestialConfig;
        std::function<gm::scene::SunMoonState()> getSunMoonState;
        std::function<WeatherState()> getWeatherState;
        std::function<std::vector<std::string>()> getWeatherProfileNames;
        std::function<void(const std::string&)> setWeatherProfile;
        std::function<WeatherForecast()> getWeatherForecast;
        std::function<void(const WeatherForecast&)> setWeatherForecast;
        std::function<void(const WeatherState&, bool broadcastEvent)> setWeatherState;
        std::function<void(bool captureLightProbes, bool captureReflections)> requestEnvironmentCapture;
        std::function<void()> triggerWeatherEvent;
    };

    struct WeatherScenarioStep {
        std::string label{"Step"};
        std::string profile{"default"};
        float durationSeconds = 15.0f;
        float wetness = 0.0f;
        float puddles = 0.0f;
        float darkening = 0.0f;
        float windSpeed = 4.0f;
        glm::vec3 windDirection{0.2f, 0.0f, 0.8f};
        bool triggerWeatherEvent = true;
        bool requestLightProbes = false;
        bool requestReflections = false;
        std::vector<std::string> customEvents;
    };

    struct WeatherScenario {
        std::string name{"Scenario"};
        std::string description;
        std::vector<WeatherScenarioStep> steps;
        bool loopPlayback = true;
        bool pendingStepApply = false;
        bool playbackActive = false;
        int currentStep = 0;
        float stepElapsed = 0.0f;
    };

    struct WeatherScenarioHarnessState {
        char customEvent[128] = "";
        bool captureLightProbes = true;
        bool captureReflections = false;
    };

    struct TimeOfDayTimelineKeyframe {
        float timeSeconds = 0.0f;
        float normalizedValue = 0.0f;
    };

    struct TimeOfDayTimelineState {
        std::vector<TimeOfDayTimelineKeyframe> keyframes;
        float durationSeconds = 120.0f;
        float playbackCursor = 0.0f;
        bool playing = false;
        bool loop = true;
        int selectedIndex = -1;
        bool needsSort = false;
    };

    void SetCallbacks(Callbacks callbacks) { m_callbacks = std::move(callbacks); }
    void SetSaveManager(gm::save::SaveManager* manager) { m_saveManager = manager; }
    void SetScene(const std::shared_ptr<gm::Scene>& scene) { m_scene = scene; }
    void SetTerrainComponent(EditableTerrainComponent* terrain) { m_terrainComponent = terrain; }
    void SetWindowHandle(void* hwnd) { m_windowHandle = hwnd; }
    void SetGLFWWindow(void* window) { m_glfwWindow = window; }
    void SetDebugConsole(DebugConsole* console) { m_debugConsole = console; }
    void SetPrefabLibrary(gm::scene::PrefabLibrary* library) { m_prefabLibrary = library; }
    void SetGameResources(::GameResources* resources) { m_gameResources = resources; }
    void SetContentDatabase(gm::content::ContentDatabase* database) { m_contentDatabase = database; }
    void SetWeatherDiagnosticsSource(const WeatherParticleSystem* system) { m_weatherDiagnosticsSystem = system; }
    void SetLayoutProfilePath(const std::filesystem::path& path);
    void SetPluginManifestPath(const std::filesystem::path& path);
    void SetApplyResourcesCallback(std::function<void()> callback) { m_applyResourcesCallback = std::move(callback); }
    void SetConsoleVisible(bool visible);
    bool IsConsoleVisible() const;
    void SetOverlayToggleCallbacks(std::function<bool()> getter, std::function<void(bool)> setter);
    void ProcessGlobalShortcuts();
    bool ShouldBlockCameraInput() const;
    bool HasSelection() const;
    void ClearSelection();
    void BeginSceneReload();
    void EndSceneReload();
    bool ShouldDelaySceneUI();

    void Render(bool& menuVisible);
    
    // Public methods to trigger file dialogs (for keyboard shortcuts)
    void TriggerSaveAs() { m_pendingSaveAs = true; }
    void TriggerLoad() { m_pendingLoad = true; }
    
    // Load recent files from disk (call after construction)
    void LoadRecentFilesFromDisk();

    // Plugin controls
    void ReloadPlugins();

    // EditorPluginHost
    GameResources* GetGameResources() const override { return m_gameResources; }
    std::shared_ptr<gm::Scene> GetActiveScene() const override { return m_scene.lock(); }
    void RegisterDockWindow(const std::string& id,
                            const std::string& title,
                            const std::function<void()>& renderFn,
                            bool* visibilityFlag = nullptr) override;
    void RegisterShortcut(const ShortcutDesc& desc,
                          const std::function<void()>& handler) override;
    void PushUndoableAction(const std::string& description,
                            const std::function<void()>& redo,
                            const std::function<void()>& undo) override;

private:
    struct ShortcutBinding {
        ImGuiKey key = ImGuiKey_None;
        bool ctrl = false;
        bool shift = false;
        bool alt = false;
    };

    struct ShortcutHandler {
        ShortcutBinding binding;
        std::function<void()> callback;
        bool fromPlugin = false;
        EditorPlugin* owner = nullptr;
    };

    struct EditorAction {
        std::function<void()> redo;
        std::function<void()> undo;
        std::string description;
    };

    struct PluginWindow {
        std::string id;
        std::string title;
        std::function<void()> renderFn;
        bool* externalVisibility = nullptr;
        bool visible = true;
        EditorPlugin* owner = nullptr;
    };

    struct LoadedPlugin {
        std::string name;
        std::filesystem::path path;
        void* handle = nullptr;
        EditorPlugin* instance = nullptr;
        DestroyEditorPluginFn destroy = nullptr;
    };

    struct AnimationAssetEntry {
        std::filesystem::path absolutePath;
        std::string displayName;
    };

    void RenderMenuBar();
    void RenderFileMenu();
    void RenderEditMenu();
    void RenderOptionsMenu();
    void RenderSaveAsDialog();
    void RenderLoadDialog();
    void RenderImportModelDialog();
    void RenderSceneHierarchy();
    void RenderSceneHierarchyTree(const std::vector<std::shared_ptr<gm::GameObject>>& roots, const std::string& filter);
    void RenderSceneHierarchyNode(const std::shared_ptr<gm::GameObject>& gameObject, const std::string& filter);
    void RenderSceneHierarchyFiltered(const std::vector<std::shared_ptr<gm::GameObject>>& objects, const std::string& filter);
    void RenderSceneHierarchyRootDropTarget();
    std::shared_ptr<gm::GameObject> ResolvePayloadGameObject(const ImGuiPayload* payload);
    void DeleteGameObject(const std::shared_ptr<gm::GameObject>& gameObject);
    void RenderInspector();
    void RenderSceneExplorerWindow();
    void RenderSceneInfo();
    void RenderPrefabBrowser();
    void RenderContentBrowser();
    void RenderTransformGizmo();
    void RenderGameObjectOverlay();
    void RenderAnimationDebugger();
    void RenderContentValidationWindow();
    void RenderCelestialDebugger();
    float RenderTimeOfDayTimeline(float normalizedTime);
    void RenderWeatherScenarioEditor();
    void RenderFogDebugger();
    void RenderWeatherPanel(const WeatherParticleSystem& system);
    void RenderDockspace();
    void HandleSaveAs();
    void HandleLoad();
    void AddRecentFile(const std::string& filePath);
    void LoadRecentFile(const std::string& filePath);
    void SaveRecentFilesToDisk();
    void EnsureSelectionWindowsVisible();
    void FocusCameraOnGameObject(const std::shared_ptr<gm::GameObject>& gameObject);
    void DrawAnimatorLayerEditor(const std::shared_ptr<gm::scene::AnimatorComponent>& animator);
    void DrawPreviewSkeleton(const ImVec2& canvasSize);
    void EnsureAnimationAssetCache();
    void RefreshAnimationPreviewPose();
    bool LoadPreviewSkeleton(const AnimationAssetEntry& entry);
    bool LoadPreviewClip(const AnimationAssetEntry& entry);
    void RemapPreviewClip();
    std::string RelativeAssetLabel(const std::filesystem::path& absolute) const;
    void AssignSkeletonFromAsset(gm::scene::AnimatorComponent& animator, const AnimationAssetEntry& entry);
    void AssignClipToLayer(gm::scene::AnimatorComponent& animator, const std::string& slot, const AnimationAssetEntry& entry);
    void SpawnCowHerd(int columns, int rows, float spacing, const glm::vec3& origin);
    void DrawPrefabDetails(const gm::scene::PrefabDefinition& prefab);
    std::filesystem::path ResolveAssimpImporterExecutable() const;
    void TriggerGlbReimport(const std::string& meshGuid);
    void EnsureWeatherScenarioDefaults();
    void AdvanceWeatherScenarioPlayback(WeatherScenario& scenario, float deltaTime);
    void ApplyWeatherScenarioStep(WeatherScenario& scenario, WeatherScenarioStep& step, bool fromPlayback);
public:
    void HandleFileDrop(const std::vector<std::string>& paths);
private:
    void StartModelImport(const std::filesystem::path& filePath);
    bool ExecuteModelImport(const std::filesystem::path& inputPath,
                            const std::filesystem::path& outputDir,
                            const std::string& baseName);

    // Shortcut helpers
    void InitializeShortcutDefaults();
    void RegisterShortcutHandler(const std::string& id,
                                 ShortcutBinding binding,
                                 const std::function<void()>& handler,
                                 bool fromPlugin = false,
                                 EditorPlugin* owner = nullptr);
    bool IsShortcutPressed(const ShortcutBinding& binding) const;
    bool IsShortcutPressed(const std::string& id) const;
    std::string FormatShortcutLabel(const ShortcutBinding& binding) const;
    void ApplyShortcutOverrides();
    ShortcutBinding ShortcutFromDesc(const ShortcutDesc& desc) const;
    ShortcutBinding ShortcutFromJson(const nlohmann::json& data) const;
    nlohmann::json ShortcutToJson(const ShortcutBinding& binding) const;

    // Window/layout persistence
    std::vector<std::pair<std::string, bool*>> GetWindowBindings();
    void MarkLayoutDirty();
    void AutosaveLayout(float deltaTime);
    bool SaveLayoutProfileInternal(const std::filesystem::path& path) const;
    bool LoadLayoutProfileInternal(const std::filesystem::path& path);
    void ApplyWindowStateOverrides();

    // Undo/redo
    bool UndoLastAction();
    bool RedoLastAction();

    // Plugins
    void HandlePluginMenu();
    void RenderPluginWindows();
    void LoadPluginsFromManifest();
    void RemovePluginArtifacts(EditorPlugin* plugin);
    void UnloadPlugins();
    void* LoadLibraryHandle(const std::filesystem::path& path);
    void UnloadLibraryHandle(void* handle);
    void* ResolveSymbol(void* handle, const char* name);


    Callbacks m_callbacks;
    gm::save::SaveManager* m_saveManager = nullptr;
    std::weak_ptr<gm::Scene> m_scene;
    EditableTerrainComponent* m_terrainComponent = nullptr;
    void* m_windowHandle = nullptr;
    void* m_glfwWindow = nullptr;

    bool m_fileMenuOpen = false;
    bool m_editMenuOpen = false;
    bool m_optionsMenuOpen = false;
    bool m_showSceneExplorer = false;
    bool m_showSceneInfo = false;
    bool m_showDebugConsole = false;
    bool m_showPrefabBrowser = false;
    bool m_showContentBrowser = false;
    bool m_showAnimationDebugger = false;
    bool m_showContentValidation = false;
    bool m_showCelestialDebugger = false;
    bool m_showFogDebugger = false;
    bool m_showWeatherPanel = false;
    bool m_showWeatherScenarioEditor = false;
    const WeatherParticleSystem* m_weatherDiagnosticsSystem = nullptr;
    std::vector<WeatherScenario> m_weatherScenarios;
    int m_selectedWeatherScenario = 0;
    WeatherScenarioHarnessState m_weatherHarness{};

    TimeOfDayTimelineState m_timeOfDayTimeline;

    // Layout control
    bool m_resetDockLayout = false;

    // Selection
    std::weak_ptr<gm::GameObject> m_selectedGameObject;

    // Prefabs
    gm::scene::PrefabLibrary* m_prefabLibrary = nullptr;
    std::string m_pendingPrefabToSpawn;

    // Gizmo state
    int m_gizmoOperation = 0; // 0=translate,1=rotate,2=scale
    int m_gizmoMode = 0; // 0=world,1=local

    // File dialogs
    bool m_showSaveAsDialog = false;
    bool m_showLoadDialog = false;
    bool m_showImportDialog = false;
    bool m_pendingSaveAs = false;
    bool m_pendingLoad = false;
    bool m_pendingImport = false;
    char m_filePathBuffer[512] = {0};
    std::string m_defaultScenePath = "assets/scenes/";
    char m_quickLoadBuffer[512] = {0};
    std::string m_lastQuickLoadPath;

    // Recent files (max 10)
    static constexpr size_t kMaxRecentFiles = 10;
    std::vector<std::string> m_recentFiles;
    std::string m_recentFilesPath = "assets/scenes/.recent_files.txt";
    DebugConsole* m_debugConsole = nullptr;
    std::function<bool()> m_overlayGetter;
    std::function<void(bool)> m_overlaySetter;
    bool m_suppressCameraInput = false;
    bool m_sceneReloadInProgress = false;
    bool m_sceneReloadPendingResume = false;
    int m_sceneReloadFramesToSkip = 0;
    std::uint64_t m_lastSeenSceneVersion = 0;
    ::GameResources* m_gameResources = nullptr;
    gm::content::ContentDatabase* m_contentDatabase = nullptr;

    std::string m_pendingContentBrowserFocusPath;

    // Animation tooling state
    bool m_enableBoneOverlay = false;
    bool m_showBoneNames = false;
    bool m_boneOverlayAllObjects = false;
    bool m_showAnimationDebugOverlay = false;

    struct FogDebugOptions {
        bool overlayEnabled = true;
        bool overlayShowLabels = true;
        bool overlayOnlySelected = false;
        float overlayOpacity = 0.55f;
        float densityColorScale = 80.0f;
        float densityMultiplier = 1.0f;
    } m_fogDebug;
    float m_boneOverlayLineThickness = 2.0f;
    float m_boneOverlayNodeRadius = 4.0f;

    bool m_animationAssetsDirty = true;
    std::vector<AnimationAssetEntry> m_animationSkeletonAssets;
    std::vector<AnimationAssetEntry> m_animationClipAssets;
    std::string m_selectedSkeletonAsset;
    std::string m_selectedClipAsset;
    std::array<char, 128> m_animationFilterBuffer{};

    std::shared_ptr<gm::animation::Skeleton> m_previewSkeleton;
    std::unique_ptr<gm::animation::AnimationClip> m_previewClip;
    std::unique_ptr<gm::animation::AnimationPoseEvaluator> m_previewEvaluator;
    gm::animation::AnimationPose m_previewPose;
    double m_previewTimeSeconds = 0.0;
    bool m_previewPlaying = false;
    bool m_previewLoop = true;
    std::vector<glm::mat4> m_previewBoneMatrices;
    float m_previewYaw = glm::radians(90.0f);
    float m_previewPitch = glm::radians(-15.0f);
    float m_previewZoom = 1.0f;

    std::function<void()> m_applyResourcesCallback;

    // Model import state
    struct ImportSettings {
        std::filesystem::path inputPath;
        std::filesystem::path outputDir;
        std::string baseName;
        bool generatePrefab = true;
        bool overwriteExisting = false;
    };
    ImportSettings m_importSettings;
    bool m_importInProgress = false;
    std::string m_importStatusMessage;

    // Layout persistence
    std::filesystem::path m_layoutProfilePath;
    mutable std::string m_cachedDockspaceLayout;
    bool m_pendingDockRestore = false;
    bool m_layoutDirty = false;
    float m_layoutAutosaveTimer = 0.0f;
    float m_layoutAutosaveInterval = 2.0f;
    std::unordered_map<std::string, bool> m_windowStateOverrides;
    std::unordered_map<std::string, ShortcutBinding> m_shortcutOverrides;
    std::unordered_map<std::string, ShortcutHandler> m_shortcutHandlers;

    // Undo stack
    std::vector<EditorAction> m_undoStack;
    std::vector<EditorAction> m_redoStack;
    size_t m_maxUndoDepth = 256;

    // Plugins
    std::filesystem::path m_pluginManifestPath;
    std::vector<LoadedPlugin> m_plugins;
    std::vector<PluginWindow> m_pluginWindows;
    EditorPlugin* m_activePlugin = nullptr;
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
