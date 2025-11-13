#include "GameBootstrapper.hpp"

#include "Game.hpp"
#include "DebugToolingController.hpp"
#include "GameEvents.hpp"
#include "gm/core/Logger.hpp"
#include "gm/core/Event.hpp"

GameBootstrapper::GameBootstrapper(Game& game)
    : m_game(game) {}

bool GameBootstrapper::Initialize(GLFWwindow* window, gm::SceneManager& sceneManager) {
    m_game.m_window = window;
    m_game.m_sceneManager = &sceneManager;
    m_game.m_vsyncEnabled = m_game.m_config.window.vsync;
    m_game.SetupLogging();
    if (!m_game.m_window) {
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

    if (m_game.m_debugTooling && !m_game.m_debugTooling->Initialize()) {
        gm::core::Logger::Warning("[Game] Some debug tools failed to initialize, continuing anyway");
    }

    m_game.SetupResourceHotReload();
    m_game.SetupEventSubscriptions();

    gm::core::Event::Trigger(gotmilked::GameEvents::GameInitialized);
    return true;
}

