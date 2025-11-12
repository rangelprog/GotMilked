#pragma once

#if GM_DEBUG_TOOLS

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace gm {
class Scene;
class GameObject;
}

namespace gm::scene {
class PrefabLibrary;
}

namespace gm::save {
class SaveManager;
}

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
    void SetDebugConsole(DebugConsole* console) { m_debugConsole = console; }
    void SetPrefabLibrary(gm::scene::PrefabLibrary* library) { m_prefabLibrary = library; }
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
    void RenderMenuBar();
    void RenderFileMenu();
    void RenderEditMenu();
    void RenderOptionsMenu();
    void RenderSaveAsDialog();
    void RenderLoadDialog();
    void RenderSceneHierarchy();
    void RenderInspector();
    void RenderSceneExplorerWindow();
    void RenderSceneInfo();
    void RenderPrefabBrowser();
    void RenderTransformGizmo();
    void RenderGameObjectOverlay();
    void RenderDockspace();
    void HandleSaveAs();
    void HandleLoad();
    void AddRecentFile(const std::string& filePath);
    void LoadRecentFile(const std::string& filePath);
    void SaveRecentFilesToDisk();
    void EnsureSelectionWindowsVisible();
    void FocusCameraOnGameObject(const std::shared_ptr<gm::GameObject>& gameObject);

    Callbacks m_callbacks;
    gm::save::SaveManager* m_saveManager = nullptr;
    std::weak_ptr<gm::Scene> m_scene;
    EditableTerrainComponent* m_terrainComponent = nullptr;
    void* m_windowHandle = nullptr;

    bool m_fileMenuOpen = false;
    bool m_editMenuOpen = false;
    bool m_optionsMenuOpen = false;
    bool m_showSceneExplorer = false;
    bool m_showSceneInfo = false;
    bool m_showDebugConsole = false;
    bool m_showPrefabBrowser = false;

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
    bool m_pendingSaveAs = false;
    bool m_pendingLoad = false;
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
};

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS
