#if GM_DEBUG_TOOLS

#include "DebugMenu.hpp"
#include "gm/tooling/DebugConsole.hpp"

#include <imgui.h>

namespace gm::debug {

void DebugMenu::Render(bool& menuVisible) {
    if (!menuVisible) {
        return;
    }

    if (ImGui::BeginMainMenuBar()) {
        RenderMenuBar();
        ImGui::EndMainMenuBar();
    }

    if (m_pendingSaveAs) {
        m_pendingSaveAs = false;
        HandleSaveAs();
    }

    if (m_pendingLoad) {
        m_pendingLoad = false;
        HandleLoad();
    }

    if (m_showSaveAsDialog) {
        RenderSaveAsDialog();
    }

    if (m_showLoadDialog) {
        RenderLoadDialog();
    }

    if (m_showInspector) {
        RenderEditorWindow();
    }
    if (m_showSceneInfo) {
        RenderSceneInfo();
    }

    if (m_showGameObjects) {
        RenderGameObjectLabels();
    }

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

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS

