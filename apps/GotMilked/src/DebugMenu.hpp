#pragma once

#include <functional>
#include <memory>
#include <string>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace gm {
class Scene;
}

namespace gm::save {
class SaveManager;
}

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

    void Render(bool& menuVisible);

private:
    void RenderMenuBar();
    void RenderFileMenu();
    void RenderEditMenu();
    void RenderOptionsMenu();
    void RenderSaveAsDialog();
    void RenderLoadDialog();
    void RenderGameObjectLabels();

    Callbacks m_callbacks;
    gm::save::SaveManager* m_saveManager = nullptr;
    std::weak_ptr<gm::Scene> m_scene;
    EditableTerrainComponent* m_terrainComponent = nullptr;
    void* m_windowHandle = nullptr;

    bool m_fileMenuOpen = false;
    bool m_editMenuOpen = false;
    bool m_optionsMenuOpen = false;
    bool m_showGameObjects = false;

    // File dialogs
    bool m_showSaveAsDialog = false;
    bool m_showLoadDialog = false;
    char m_filePathBuffer[512] = {0};
    std::string m_defaultScenePath = "assets/scenes/";
};

