#include "GameBootstrapper.hpp"

#include "Game.hpp"
#include "DebugToolingController.hpp"
#include "GameEvents.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Event.hpp"

GameBootstrapper::GameBootstrapper(Game& game)
    : m_game(game) {}

bool GameBootstrapper::Initialize(GLFWwindow* window, gm::SceneManager& sceneManager) {
    m_game.SetWindow(window);
    m_game.SetSceneManager(sceneManager);
    m_game.SetVSyncEnabled(m_game.Config().window.vsync);
    m_game.SetupLogging();
    if (!m_game.Window()) {
        gm::core::Logger::Error("[Game] Invalid window handle");
        return false;
    }

    if (!m_game.SetupPhysics()) {
        return false;
    }

    if (!m_game.SetupRendering()) {
        return false;
    }
    m_game.SetupInput();
    m_game.SetupScene();
    m_game.ApplyResourcesToScene();
    m_game.SetupGameplay();
    m_game.SetupSaveSystem();
    if (!m_game.SetupPrefabs()) {
        gm::core::Logger::Warning("[Game] Prefab library failed to initialize");
    }

    if (m_game.DebugTooling() && !m_game.DebugTooling()->Initialize()) {
        gm::core::Logger::Warning("[Game] Some debug tools failed to initialize, continuing anyway");
    }

    m_game.SetupResourceHotReload();
    m_game.SetupEventSubscriptions();

    gm::core::Event::Trigger(gotmilked::GameEvents::GameInitialized);
    return true;
}

