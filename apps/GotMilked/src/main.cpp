#include <filesystem>
#include <string>

#include "Game.hpp"
#include "gm/core/GameApp.hpp"
#include "gm/utils/Config.hpp"

int main() {
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
    return game.Init(ctx.window);
  };
  callbacks.onUpdate = [&](gm::core::GameAppContext&, float dt) {
    game.Update(dt);
  };
  callbacks.onRender = [&](gm::core::GameAppContext&) {
    game.Render();
  };
  callbacks.onShutdown = [&](gm::core::GameAppContext&) {
    game.Shutdown();
  };

  return app.Run(callbacks);
}
