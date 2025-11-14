#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "gm/scene/Scene.hpp"
#include "GameResources.hpp"

#include <imgui.h>

namespace {
#if defined(IMGUI_VERSION_NUM) && IMGUI_VERSION_NUM >= 18700
inline bool IsImGuiKeyPressed(ImGuiKey key) {
    return ImGui::IsKeyPressed(key);
}

inline bool IsImGuiKeyDown(ImGuiKey key) {
    return ImGui::IsKeyDown(key);
}
#else
inline bool IsImGuiKeyPressed(ImGuiKey key) {
    return ImGui::IsKeyPressed(ImGui::GetKeyIndex(key));
}

inline bool IsImGuiKeyDown(ImGuiKey key) {
    return ImGui::IsKeyDown(ImGui::GetKeyIndex(key));
}
#endif
} // namespace

namespace gm::debug {

void DebugMenu::Render(bool& menuVisible) {
    if (!menuVisible) {
        return;
    }

    ProcessGlobalShortcuts();

    if (m_sceneReloadInProgress) {
        if (m_sceneReloadPendingResume) {
            m_sceneReloadInProgress = false;
            m_sceneReloadPendingResume = false;
        }
        return;
    }

    if (ImGui::BeginMainMenuBar()) {
        RenderMenuBar();
        ImGui::EndMainMenuBar();
    }

    RenderDockspace();

    if (m_pendingSaveAs) {
        m_pendingSaveAs = false;
        HandleSaveAs();
    }

    if (m_pendingLoad) {
        m_pendingLoad = false;
        HandleLoad();
    }

    if (m_pendingImport) {
        m_pendingImport = false;
        m_showImportDialog = true;
        // Initialize import settings with defaults
        if (m_importSettings.inputPath.empty() && m_gameResources) {
            std::filesystem::path assetsDir = m_gameResources->GetAssetsDirectory();
            m_importSettings.outputDir = assetsDir / "models";
        }
    }

    if (m_showImportDialog) {
        RenderImportModelDialog();
    }

    if (m_showSceneExplorer) {
        RenderSceneExplorerWindow();
        RenderTransformGizmo();
    }

    if (m_showSaveAsDialog) {
        RenderSaveAsDialog();
    }

    if (m_showLoadDialog) {
        RenderLoadDialog();
    }

    if (m_showSceneInfo) {
        RenderSceneInfo();
    }

    if (m_showPrefabBrowser) {
        RenderPrefabBrowser();
    }

    if (m_showContentBrowser) {
        RenderContentBrowser();
    }

    if (m_showAnimationDebugger) {
        RenderAnimationDebugger();
    }

    RenderGameObjectOverlay();

    if (m_showDebugConsole && m_debugConsole) {
        bool open = m_showDebugConsole;
        m_debugConsole->Render(&open);
        m_showDebugConsole = open;
    }
}

void DebugMenu::SetConsoleVisible(bool visible) {
    m_showDebugConsole = visible;
}

bool DebugMenu::IsConsoleVisible() const {
    return m_showDebugConsole;
}

void DebugMenu::SetOverlayToggleCallbacks(std::function<bool()> getter, std::function<void(bool)> setter) {
    m_overlayGetter = std::move(getter);
    m_overlaySetter = std::move(setter);
}

void DebugMenu::ProcessGlobalShortcuts() {
    ImGuiIO& io = ImGui::GetIO();

    if (m_sceneReloadInProgress) {
        m_suppressCameraInput = false;
        return;
    }

    auto selected = m_selectedGameObject.lock();
    if (selected && IsImGuiKeyPressed(ImGuiKey_Escape)) {
        ClearSelection();
        selected.reset();
    }

    bool hasSelection = static_cast<bool>(selected);
    bool allowHotkeys = !io.WantCaptureKeyboard && !m_sceneReloadInProgress && m_sceneReloadFramesToSkip == 0;

    if (allowHotkeys) {
        if (IsImGuiKeyPressed(ImGuiKey_W)) {
            m_gizmoOperation = 0;
        } else if (IsImGuiKeyPressed(ImGuiKey_E)) {
            m_gizmoOperation = 1;
        } else if (IsImGuiKeyPressed(ImGuiKey_R)) {
            m_gizmoOperation = 2;
        }
    }

    m_suppressCameraInput = hasSelection && !io.WantCaptureKeyboard && !io.WantCaptureMouse && !m_sceneReloadInProgress && m_sceneReloadFramesToSkip == 0;
}

bool DebugMenu::ShouldBlockCameraInput() const {
    return m_suppressCameraInput;
}

bool DebugMenu::HasSelection() const {
    return !m_selectedGameObject.expired();
}

void DebugMenu::ClearSelection() {
    m_selectedGameObject.reset();
    m_suppressCameraInput = false;
}

void DebugMenu::BeginSceneReload() {
    m_sceneReloadInProgress = true;
    m_sceneReloadPendingResume = false;
    ClearSelection();
}

void DebugMenu::EndSceneReload() {
    m_sceneReloadPendingResume = true;
    m_sceneReloadFramesToSkip = 1;
    if (auto scene = m_scene.lock()) {
        scene->BumpReloadVersion();
    }
}

bool DebugMenu::ShouldDelaySceneUI() {
    if (!m_sceneReloadInProgress && !m_sceneReloadPendingResume && m_sceneReloadFramesToSkip == 0) {
        auto scene = m_scene.lock();
        if (scene && scene->GetAllGameObjects().empty()) {
            m_sceneReloadPendingResume = true;
            m_sceneReloadFramesToSkip = 1;
            return true;
        }
        return false;
    }

    if (m_sceneReloadFramesToSkip > 0) {
        --m_sceneReloadFramesToSkip;
        if (m_sceneReloadFramesToSkip == 0) {
            m_sceneReloadPendingResume = false;
            m_sceneReloadInProgress = false;
        }
        return true;
    }

    auto scene = m_scene.lock();
    if (scene && scene->GetAllGameObjects().empty()) {
        m_sceneReloadFramesToSkip = 1;
        m_sceneReloadPendingResume = true;
        return true;
    }

    return false;
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS

