#include "GameLoopController.hpp"

#include "Game.hpp"
#include "GameResources.hpp"
#include "gm/physics/PhysicsWorld.hpp"
#include "gm/scene/Scene.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/utils/HotReloader.hpp"
#include "gm/utils/Profiler.hpp"
#include "gm/core/Input.hpp"
#include "gm/core/InputBindings.hpp"
#include "gm/core/input/InputSystem.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Event.hpp"
#include "ToolingFacade.hpp"
#if GM_DEBUG_TOOLS
#include "DebugHudController.hpp"
#endif

#include <GLFW/glfw3.h>
#include <filesystem>

GameLoopController::GameLoopController(Game& game)
    : m_game(game) {}

void GameLoopController::Update(float dt) {
    if (!m_game.Window()) {
        return;
    }

    gm::utils::Profiler::ScopedTimer frameTimer("GameLoopController::Update");

    {
        gm::utils::Profiler::ScopedTimer timer("GameLoopController::HandleResourceCatalogChanges");
        HandleResourceCatalogChanges();
    }
    {
        gm::utils::Profiler::ScopedTimer timer("GameLoopController::UpdatePhysics");
        UpdatePhysics(dt);
    }
    {
        gm::utils::Profiler::ScopedTimer timer("GameLoopController::HandleGlobalInputs");
        HandleGlobalInputs();
    }
    {
        gm::utils::Profiler::ScopedTimer timer("GameLoopController::HandleDebugShortcuts");
        HandleDebugShortcuts();
    }
    {
        gm::utils::Profiler::ScopedTimer timer("GameLoopController::UpdateGameplay");
        UpdateGameplay(dt);
    }
    {
        gm::utils::Profiler::ScopedTimer timer("GameLoopController::UpdateHotReloader");
        UpdateHotReloader(dt);
    }
}

void GameLoopController::HandleResourceCatalogChanges() {
    if (auto catalogUpdate = m_game.m_resources.ProcessCatalogEvents(); catalogUpdate) {
        if (catalogUpdate.reloadSucceeded) {
            m_game.ApplyResourcesToScene();
            if (m_game.m_toolingFacade) {
                m_game.m_toolingFacade->RefreshHud();
            }
        }

        if (catalogUpdate.reloadSucceeded && catalogUpdate.prefabsChanged && m_game.m_prefabLibrary) {
            std::filesystem::path prefabRoot = m_game.m_assetsDir / "prefabs";
            if (!m_game.m_prefabLibrary->LoadDirectory(prefabRoot)) {
                gm::core::Logger::Info("[Game] Prefab library refreshed; no prefabs found in {}", prefabRoot.string());
            } else {
                gm::core::Logger::Info("[Game] Prefab library refreshed after catalog change");
            }
        }
    }
}

void GameLoopController::UpdatePhysics(float dt) {
    auto& physics = gm::physics::PhysicsWorld::Instance();
    if (physics.IsInitialized()) {
        physics.FlushPendingOperations();
        physics.Step(dt);
    }
}

void GameLoopController::HandleGlobalInputs() {
    auto& input = gm::core::Input::Instance();

    auto* inputSys = input.GetInputSystem();
    if (inputSys && inputSys->IsKeyJustPressed(GLFW_KEY_V)) {
        bool imguiWantsKeyboard = m_game.m_toolingFacade && m_game.m_toolingFacade->WantsKeyboardInput();
        if (!imguiWantsKeyboard) {
            bool enabled = !m_game.IsVSyncEnabled();
            m_game.SetVSyncEnabled(enabled);
            gm::core::Logger::Info("[Game] VSync {}", enabled ? "enabled" : "disabled");
        }
    }

    if (input.IsActionJustPressed("Exit")) {
        bool shouldExit = true;
        if (m_game.m_toolingFacade && m_game.m_toolingFacade->DebugMenuHasSelection()) {
            shouldExit = false;
        }
        if (shouldExit) {
            m_game.RequestExit();
        }
    }

    if (input.IsActionJustPressed("QuickSave")) {
        m_game.PerformQuickSave();
    }

    if (input.IsActionJustPressed("QuickLoad")) {
        m_game.PerformQuickLoad();
    }

    if (input.IsActionJustPressed("ToggleOverlay")) {
        if (!m_game.m_toolingFacade || !m_game.m_toolingFacade->HandleOverlayToggle()) {
            // ToolingFacade will log if toggle failed
        }
    }

#if GM_DEBUG_TOOLS
    if (inputSys && inputSys->IsKeyJustPressed(GLFW_KEY_F7)) {
        if (m_game.m_debugHud) {
            m_game.m_debugHud->ToggleProfiler();
        }
    }
#endif
}

void GameLoopController::HandleDebugShortcuts() {
    auto& input = gm::core::Input::Instance();
    if (m_game.m_toolingFacade) {
        m_game.m_toolingFacade->HandleDebugShortcuts(input);
    }
}

void GameLoopController::UpdateGameplay(float dt) {
    if (m_game.m_cameraRigSystem) {
        m_game.m_cameraRigSystem->SetWindow(m_game.Window());
    }

    bool imguiWantsInput = m_game.m_toolingFacade && m_game.m_toolingFacade->WantsAnyInput();
    bool overlayActive = m_game.m_toolingFacade ? m_game.m_toolingFacade->IsOverlayActive() : m_game.m_overlayVisible;
    bool debugSelectionBlocksInput = m_game.m_toolingFacade && m_game.m_toolingFacade->ShouldBlockCameraInput();
    if (m_game.m_cameraRigSystem) {
        m_game.m_cameraRigSystem->SetInputSuppressed(imguiWantsInput || overlayActive || debugSelectionBlocksInput);
    }

#if GM_DEBUG_TOOLS
    if (m_game.IsDebugViewportCameraActive()) {
        bool suppressDebugCameraInput = imguiWantsInput || debugSelectionBlocksInput;
        m_game.UpdateViewportCamera(dt, suppressDebugCameraInput);
    }
#endif

    if (m_game.m_questSystem) {
        m_game.m_questSystem->SetInputSuppressed(imguiWantsInput || overlayActive);
    }
    if (m_game.m_dialogueSystem) {
        m_game.m_dialogueSystem->SetInputSuppressed(imguiWantsInput || overlayActive);
    }

    if (m_game.m_gameScene) {
        m_game.m_gameScene->Update(dt);
    }
}

void GameLoopController::UpdateHotReloader(float dt) {
        m_game.m_hotReloader.Update(dt);
}

