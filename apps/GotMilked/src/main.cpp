#include <filesystem>
#include <string>
#include <exception>

#include "Game.hpp"
#include "gm/core/GameApp.hpp"
#include "gm/core/Logger.hpp"
#include "gm/utils/Config.hpp"

int main() {
  try {
    const std::filesystem::path configPath = std::filesystem::path(GM_CONFIG_PATH);
    auto configResult = gm::utils::ConfigLoader::Load(configPath);
    
    // Check for critical config errors
    if (configResult.HasErrors()) {
      gm::core::Logger::Error("[main] Configuration errors detected. Please fix the following:");
      for (const auto& error : configResult.errors) {
        gm::core::Logger::Error("[main]   - {}", error);
      }
      return 1;
    }
    
    gm::utils::AppConfig appConfig = configResult.config;

    Game game(appConfig);

    gm::core::GameAppConfig config;
    config.width = appConfig.window.width;
    config.height = appConfig.window.height;
    config.title = appConfig.window.title;
    config.enableVsync = appConfig.window.vsync;
    config.enableDepthTest = appConfig.window.depthTest;
    config.showFpsInTitle = appConfig.window.showFpsInTitle;
    config.fpsTitleUpdateIntervalSeconds = appConfig.window.fpsTitleUpdateIntervalSeconds;

    gm::core::GameApp app(config);

    gm::core::GameAppCallbacks callbacks;
    callbacks.onInit = [&](gm::core::GameAppContext& ctx) {
      try {
        if (!ctx.sceneManager) {
          gm::core::Logger::Error("[main] GameAppContext missing SceneManager");
          return false;
        }
        return game.Init(ctx.window, *ctx.sceneManager);
      } catch (const std::exception& ex) {
        gm::core::Logger::Error("[main] Exception in onInit: {}", ex.what());
        return false;
      } catch (...) {
        gm::core::Logger::Error("[main] Unknown exception in onInit");
        return false;
      }
    };
    callbacks.onUpdate = [&](gm::core::GameAppContext&, float dt) {
      try {
        game.Update(dt);
      } catch (const std::exception& ex) {
        gm::core::Logger::Error("[main] Exception in onUpdate: {}", ex.what());
      } catch (...) {
        gm::core::Logger::Error("[main] Unknown exception in onUpdate");
      }
    };
    callbacks.onRender = [&](gm::core::GameAppContext&) {
      try {
        game.Render();
      } catch (const std::exception& ex) {
        gm::core::Logger::Error("[main] Exception in onRender: {}", ex.what());
      } catch (...) {
        gm::core::Logger::Error("[main] Unknown exception in onRender");
      }
    };
    callbacks.onShutdown = [&](gm::core::GameAppContext&) {
      try {
        game.Shutdown();
      } catch (const std::exception& ex) {
        gm::core::Logger::Error("[main] Exception in onShutdown: {}", ex.what());
      } catch (...) {
        gm::core::Logger::Error("[main] Unknown exception in onShutdown");
      }
    };

    return app.Run(callbacks);
  } catch (const std::exception& ex) {
    gm::core::Logger::Error("[main] Fatal exception: {}", ex.what());
    return 1;
  } catch (...) {
    gm::core::Logger::Error("[main] Fatal unknown exception");
    return 1;
  }
}
