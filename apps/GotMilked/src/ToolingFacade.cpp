#include "ToolingFacade.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Game.hpp"
#include "GameResources.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/input/InputSystem.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/core/Logger.hpp"
#include <glm/mat4x4.hpp>

#if GM_DEBUG_TOOLS
#include "DebugMenu.hpp"
#include "DebugHudController.hpp"
#include "EditableTerrainComponent.hpp"
#include "gm/tooling/DebugConsole.hpp"
#include "gm/debug/GridRenderer.hpp"
#endif

#include <imgui.h>

ToolingFacade::ToolingFacade(Game& game)
    : m_game(game) {}

bool ToolingFacade::IsImGuiReady() const {
    return m_game.m_imgui && m_game.m_imgui->IsInitialized();
}

bool ToolingFacade::WantsKeyboardInput() const {
    if (!IsImGuiReady()) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard;
}

bool ToolingFacade::WantsAnyInput() const {
    if (!IsImGuiReady()) {
        return false;
    }
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureKeyboard || io.WantCaptureMouse;
}

bool ToolingFacade::HandleOverlayToggle() {
    if (!IsImGuiReady()) {
        gm::core::Logger::Warning("[Game] ImGui not initialized; debug menu not available");
        return false;
    }
#if GM_DEBUG_TOOLS
    if (m_game.m_debugHud) {
        m_game.m_debugHud->ToggleHud();
        m_game.m_overlayVisible = m_game.m_debugHud->GetOverlayVisible();
        return true;
    }
#endif
    m_game.m_overlayVisible = !m_game.m_overlayVisible;
    return true;
}

bool ToolingFacade::IsOverlayActive() const {
#if GM_DEBUG_TOOLS
    if (m_game.m_debugHud) {
        return m_game.m_debugHud->IsHudVisible();
    }
#endif
    return m_game.m_overlayVisible;
}

bool ToolingFacade::DebugMenuHasSelection() const {
#if GM_DEBUG_TOOLS
    if (m_game.m_debugMenu) {
        return m_game.m_debugMenu->HasSelection();
    }
#endif
    return false;
}

bool ToolingFacade::ShouldBlockCameraInput() const {
#if GM_DEBUG_TOOLS
    if (m_game.m_debugMenu) {
        return m_game.m_debugMenu->ShouldBlockCameraInput();
    }
#endif
    return false;
}

void ToolingFacade::AddNotification(const std::string& message) {
    if (m_game.m_tooling) {
        m_game.m_tooling->AddNotification(message);
    }
}

void ToolingFacade::RefreshHud() {
#if GM_DEBUG_TOOLS
    if (m_game.m_debugHud) {
        m_game.m_debugHud->Refresh();
    }
#endif
}

void ToolingFacade::UpdateSceneReference() {
    if (m_game.m_tooling) {
        m_game.m_tooling->SetScene(m_game.m_gameScene);
    }
}

#if GM_DEBUG_TOOLS
void ToolingFacade::RegisterTerrain(gm::debug::EditableTerrainComponent* terrain) {
    if (m_game.m_debugHud && terrain) {
        m_game.m_debugHud->RegisterTerrain(terrain);
    }
}
#endif

void ToolingFacade::HandleDebugShortcuts(gm::core::Input& input) {
#if GM_DEBUG_TOOLS
    if (m_game.m_gridRenderer) {
        bool imguiWantsKeyboard = WantsKeyboardInput();

        bool debugModeActive = false;
        if (m_game.m_debugHud) {
            debugModeActive = m_game.m_debugHud->IsHudVisible();
        } else {
            debugModeActive = m_game.m_overlayVisible;
        }

        if (!debugModeActive) {
            m_game.m_gridVisible = false;
        } else if (!imguiWantsKeyboard && input.IsActionJustPressed("ToggleGrid")) {
            m_game.m_gridVisible = !m_game.m_gridVisible;
        }
    }

    if (m_game.m_debugMenu && IsImGuiReady()) {
        auto* debugInputSys = input.GetInputSystem();
        if (debugInputSys) {
            bool ctrlPressed = debugInputSys->IsKeyPressed(GLFW_KEY_LEFT_CONTROL) ||
                               debugInputSys->IsKeyPressed(GLFW_KEY_RIGHT_CONTROL);
            if (ctrlPressed && debugInputSys->IsKeyJustPressed(GLFW_KEY_S)) {
                ImGuiIO& io = ImGui::GetIO();
                if (!io.WantCaptureKeyboard) {
                    m_game.m_debugMenu->TriggerSaveAs();
                }
            }

            if (ctrlPressed && debugInputSys->IsKeyJustPressed(GLFW_KEY_O)) {
                ImGuiIO& io = ImGui::GetIO();
                if (!io.WantCaptureKeyboard) {
                    m_game.m_debugMenu->TriggerLoad();
                }
            }
        }
    }
#else
    (void)input;
#endif
}

void ToolingFacade::BeginFrame() {
    if (IsImGuiReady()) {
        m_game.m_imgui->NewFrame();
    }
}

void ToolingFacade::RenderGrid(const glm::mat4& view, const glm::mat4& proj) {
#if GM_DEBUG_TOOLS
    if (!m_game.m_gridRenderer) {
        return;
    }
    bool debugModeActive = false;
    if (m_game.m_debugHud) {
        debugModeActive = m_game.m_debugHud->IsHudVisible();
    } else {
        debugModeActive = m_game.m_overlayVisible;
    }
    if (debugModeActive && m_game.m_gridVisible) {
        m_game.m_gridRenderer->Render(view, proj);
    }
#else
    (void)view;
    (void)proj;
#endif
}

void ToolingFacade::RenderUI() {
    if (!IsImGuiReady()) {
        return;
    }
#if GM_DEBUG_TOOLS
    if (m_game.m_debugHud) {
        m_game.m_debugHud->RenderHud();
    } else if (m_game.m_debugMenu) {
        bool visible = true;
        m_game.m_debugMenu->Render(visible);
    }
#endif
    bool overlayPreferred = m_game.m_overlayVisible;
#if GM_DEBUG_TOOLS
    bool hudVisible = true;
    if (m_game.m_debugHud) {
        overlayPreferred = m_game.m_debugHud->GetOverlayVisible();
        hudVisible = m_game.m_debugHud->IsHudVisible();
    }
#endif
    if (m_game.m_tooling) {
#if GM_DEBUG_TOOLS
        if (!m_game.m_debugHud || hudVisible) {
            bool renderOpen = overlayPreferred;
            m_game.m_tooling->Render(renderOpen);
            overlayPreferred = renderOpen;
        }
#else
        bool renderOpen = overlayPreferred;
        m_game.m_tooling->Render(renderOpen);
        overlayPreferred = renderOpen;
#endif
#if GM_DEBUG_TOOLS
        if (m_game.m_debugHud) {
            m_game.m_debugHud->SetOverlayVisible(overlayPreferred);
        }
#endif
        m_game.m_overlayVisible = overlayPreferred;
    }
#if GM_DEBUG_TOOLS
    if (m_game.m_debugHud) {
        m_game.m_debugHud->RenderTerrainEditors();
    }
    {
        const bool viewportActive = IsOverlayActive();
        static bool s_lastViewportState = false;
        if (viewportActive != s_lastViewportState) {
            AddNotification(viewportActive
                ? "[ViewportCam] Debug HUD activated"
                : "[ViewportCam] Debug HUD hidden");
            s_lastViewportState = viewportActive;
        }
        m_game.SetDebugViewportCameraActive(viewportActive);
    }
#endif
    m_game.m_imgui->Render();
}

void ToolingFacade::Shutdown() {
    if (m_game.m_imgui) {
        m_game.m_imgui->Shutdown();
        m_game.m_imgui.reset();
    }
    m_game.m_tooling.reset();
#if GM_DEBUG_TOOLS
    m_game.m_debugHud.reset();
    m_game.m_debugMenu.reset();
    m_game.m_debugConsole.reset();
    m_game.m_gridRenderer.reset();
    m_game.m_gridVisible = false;
#endif
    m_game.m_overlayVisible = false;
}

gm::utils::ImGuiManager* ToolingFacade::ImGui() {
    return m_game.m_imgui.get();
}

gm::tooling::Overlay* ToolingFacade::Overlay() {
    return m_game.m_tooling.get();
}

