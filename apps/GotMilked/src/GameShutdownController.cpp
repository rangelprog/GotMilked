#include "GameShutdownController.hpp"

#include "Game.hpp"
#include "ToolingFacade.hpp"
#include "EventRouter.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Event.hpp"
#include "GameEvents.hpp"
#include "SceneSerializerExtensions.hpp"
#include "gm/physics/PhysicsWorld.hpp"
#include "gm/rendering/Camera.hpp"
#include "gm/utils/ImGuiManager.hpp"
#include "gm/tooling/Overlay.hpp"
#include "gm/assets/AssetDatabase.hpp"

#if GM_DEBUG_TOOLS
#include "DebugHudController.hpp"
#include "DebugMenu.hpp"
#include "gm/tooling/DebugConsole.hpp"
#endif

GameShutdownController::GameShutdownController(Game& game)
    : m_game(game) {}

void GameShutdownController::Shutdown() {
    if (m_game.m_eventRouter) {
        m_game.m_eventRouter->Clear();
    }
    m_game.m_gameScene.reset();

    gm::SceneSerializerExtensions::UnregisterSerializers();
    m_game.m_resources.Release();

    m_game.m_cameraRigSystem.reset();
    m_game.m_saveManager.reset();
    if (m_game.m_toolingFacade) {
        m_game.m_toolingFacade->Shutdown();
    }
    m_game.m_camera.reset();
#if GM_DEBUG_TOOLS
    m_game.m_terrainEditingSystem.reset();
#endif
    m_game.m_questSystem.reset();
    m_game.m_dialogueSystem.reset();
    m_game.m_completedQuests.clear();
    m_game.m_completedDialogues.clear();
    m_game.m_narrativeLog.reset();
    m_game.m_scriptingHooks.reset();

    gm::core::Logger::Info("[Game] Shutdown complete");

    gm::physics::PhysicsWorld::Instance().Shutdown();

    gm::core::Event::Trigger(gotmilked::GameEvents::GameShutdown);

    m_game.m_sceneManager = nullptr;
    m_game.m_eventRouter.reset();
    m_game.m_toolingFacade.reset();

    gm::assets::AssetDatabase::Instance().Shutdown();
}

