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

#include "gm/animation/AnimationPose.hpp"
#include "gm/animation/AnimationClip.hpp"
#include "gm/animation/AnimationPoseEvaluator.hpp"
#include "gm/animation/Skeleton.hpp"

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

class DebugMenu {
public:
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

private:
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
public:
    void HandleFileDrop(const std::vector<std::string>& paths);
private:
    void StartModelImport(const std::filesystem::path& filePath);
    bool ExecuteModelImport(const std::filesystem::path& inputPath,
                            const std::filesystem::path& outputDir,
                            const std::string& baseName);

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

    std::string m_pendingContentBrowserFocusPath;

    // Animation tooling state
    bool m_enableBoneOverlay = false;
    bool m_showBoneNames = false;
    bool m_boneOverlayAllObjects = false;
    bool m_showAnimationDebugOverlay = false;
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
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
