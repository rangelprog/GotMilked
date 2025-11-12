#if GM_DEBUG_TOOLS

#include "DebugHudController.hpp"
#include "DebugMenu.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "gm/tooling/Overlay.hpp"

namespace gm::debug {

void DebugHudController::SetDebugMenu(DebugMenu* menu) {
    m_menu = menu;
    if (m_menu) {
        m_menu->SetConsoleVisible(m_consoleVisible);
        m_menu->SetOverlayToggleCallbacks(
            [this]() { return m_overlayVisible; },
            [this](bool visible) { SetOverlayVisible(visible); });
    }
}

void DebugHudController::SetDebugConsole(DebugConsole* console) {
    m_console = console;
    (void)m_console;
}

void DebugHudController::SetOverlay(gm::tooling::Overlay* overlay) {
    m_overlay = overlay;
    (void)m_overlay;
}

void DebugHudController::RegisterTerrain(EditableTerrainComponent* terrain) {
    if (!terrain) {
        return;
    }
    if (std::find(m_terrains.begin(), m_terrains.end(), terrain) == m_terrains.end()) {
        m_terrains.push_back(terrain);
    }
    terrain->SetEditingEnabled(m_hudVisible && m_terrainEditingEnabled);
    terrain->SetEditorWindowVisible(m_hudVisible && m_terrainEditingEnabled);
}

void DebugHudController::ToggleHud() {
    SetHudVisible(!m_hudVisible);
}

void DebugHudController::SetHudVisible(bool visible) {
    m_hudVisible = visible;
    m_menuVisible = visible;
    ApplyVisibility();
}

void DebugHudController::RenderHud() {
    if (!m_hudVisible || !m_menu) {
        return;
    }
    m_menu->SetConsoleVisible(m_consoleVisible);
    m_menu->Render(m_menuVisible);
    m_consoleVisible = m_menu->IsConsoleVisible();
}

void DebugHudController::RenderTerrainEditors() {
    if (!m_hudVisible) {
        return;
    }
    for (auto* terrain : m_terrains) {
        if (!terrain) {
            continue;
        }
        if (m_terrainEditingEnabled) {
            terrain->Render();
        }
    }
}

void DebugHudController::SetConsoleVisible(bool visible) {
    m_consoleVisible = visible;
    if (m_menu) {
        m_menu->SetConsoleVisible(visible);
    }
}

void DebugHudController::SetOverlayVisible(bool visible) {
    m_overlayVisible = visible;
}

void DebugHudController::ApplyVisibility() {
    if (m_menu) {
        m_menuVisible = m_hudVisible ? m_menuVisible : false;
        m_menu->SetConsoleVisible(m_consoleVisible);
    }
    SetTerrainEditingEnabled(m_terrainEditingEnabled);
}

void DebugHudController::SetTerrainEditingEnabled(bool enabled) {
    m_terrainEditingEnabled = enabled;
    for (auto* terrain : m_terrains) {
        if (!terrain) {
            continue;
        }
        terrain->SetEditingEnabled(enabled);
        terrain->SetEditorWindowVisible(enabled);
    }
}

void DebugHudController::Refresh() {
    ApplyVisibility();
    SetOverlayVisible(m_overlayVisible);
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


