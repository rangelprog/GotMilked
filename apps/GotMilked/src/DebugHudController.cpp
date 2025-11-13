#if GM_DEBUG_TOOLS

#include "DebugHudController.hpp"
#include "DebugMenu.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/utils/Profiler.hpp"

#include <imgui.h>

namespace gm::debug {

namespace {
void RenderProfilerOverlay() {
    auto profile = gm::utils::Profiler::Instance().GetLastFrame();
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoMove;
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    if (ImGui::Begin("Frame Profiler", nullptr, flags)) {
        const double fps = profile.frameTimeMs > 0.0 ? 1000.0 / profile.frameTimeMs : 0.0;
        ImGui::Text("Frame: %.2f ms (%.0f FPS)", profile.frameTimeMs, fps);
        ImGui::Separator();
        for (const auto& sample : profile.samples) {
            ImGui::Text("%-32s %.2f ms", sample.name.c_str(), sample.durationMs);
        }
    }
    ImGui::End();
}
} // namespace

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
    if (m_profilerVisible) {
        RenderProfilerOverlay();
    }
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

void DebugHudController::ToggleProfiler() {
    m_profilerVisible = !m_profilerVisible;
}

} // namespace gm::debug

#endif // GM_DEBUG_TOOLS


