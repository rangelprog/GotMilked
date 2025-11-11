#include <filesystem>
#include <string>
#include <exception>
#include <cstdio>

#include "Game.hpp"
#include "gm/core/GameApp.hpp"
#include "gm/utils/Config.hpp"

int main() {
  try {
    const std::filesystem::path configPath = std::filesystem::path(GM_CONFIG_PATH);
    auto configResult = gm::utils::ConfigLoader::Load(configPath);
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
        return game.Init(ctx.window);
      } catch (const std::exception& ex) {
        std::fprintf(stderr, "[main] Exception in onInit: %s\n", ex.what());
        return false;
      } catch (...) {
        std::fprintf(stderr, "[main] Unknown exception in onInit\n");
        return false;
      }
    };
    callbacks.onUpdate = [&](gm::core::GameAppContext&, float dt) {
      try {
        game.Update(dt);
      } catch (const std::exception& ex) {
        std::fprintf(stderr, "[main] Exception in onUpdate: %s\n", ex.what());
      } catch (...) {
        std::fprintf(stderr, "[main] Unknown exception in onUpdate\n");
      }
    };
    callbacks.onRender = [&](gm::core::GameAppContext&) {
      try {
        game.Render();
      } catch (const std::exception& ex) {
        std::fprintf(stderr, "[main] Exception in onRender: %s\n", ex.what());
      } catch (...) {
        std::fprintf(stderr, "[main] Unknown exception in onRender\n");
      }
    };
    callbacks.onShutdown = [&](gm::core::GameAppContext&) {
      try {
        game.Shutdown();
      } catch (const std::exception& ex) {
        std::fprintf(stderr, "[main] Exception in onShutdown: %s\n", ex.what());
      } catch (...) {
        std::fprintf(stderr, "[main] Unknown exception in onShutdown\n");
      }
    };

    return app.Run(callbacks);
  } catch (const std::exception& ex) {
    std::fprintf(stderr, "[main] Fatal exception: %s\n", ex.what());
    return 1;
  } catch (...) {
    std::fprintf(stderr, "[main] Fatal unknown exception\n");
    return 1;
  }
}
