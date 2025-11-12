#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace gm {
class Scene;
class GameObject;
}

namespace gm::save {
class SaveManager;
}

#ifdef _DEBUG
namespace gm::tooling {
class DebugConsole;
}
#endif

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
#ifdef _DEBUG
    void SetDebugConsole(gm::tooling::DebugConsole* console) { m_debugConsole = console; }
#endif

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
    void RenderGameObjectLabels();
    void RenderEditorWindow();
    void RenderSceneHierarchy();
    void RenderInspector();
    void RenderSceneInfo();
    void HandleSaveAs();
    void HandleLoad();
    void AddRecentFile(const std::string& filePath);
    void LoadRecentFile(const std::string& filePath);
    void SaveRecentFilesToDisk();

    Callbacks m_callbacks;
    gm::save::SaveManager* m_saveManager = nullptr;
    std::weak_ptr<gm::Scene> m_scene;
    EditableTerrainComponent* m_terrainComponent = nullptr;
    void* m_windowHandle = nullptr;

    bool m_fileMenuOpen = false;
    bool m_editMenuOpen = false;
    bool m_optionsMenuOpen = false;
    bool m_showGameObjects = false;

    // Editor windows
    bool m_showInspector = false;
    bool m_showSceneInfo = false;
#ifdef _DEBUG
    bool m_showDebugConsole = false;
#endif

    // Selection
    std::weak_ptr<gm::GameObject> m_selectedGameObject;

    // File dialogs
    bool m_showSaveAsDialog = false;
    bool m_showLoadDialog = false;
    bool m_pendingSaveAs = false;
    bool m_pendingLoad = false;
    char m_filePathBuffer[512] = {0};
    std::string m_defaultScenePath = "assets/scenes/";

    // Recent files (max 10)
    static constexpr size_t kMaxRecentFiles = 10;
    std::vector<std::string> m_recentFiles;
    std::string m_recentFilesPath = "assets/scenes/.recent_files.txt";
#ifdef _DEBUG
    gm::tooling::DebugConsole* m_debugConsole = nullptr;
#endif
};
