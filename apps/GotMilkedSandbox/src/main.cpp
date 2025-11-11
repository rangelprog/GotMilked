#include <string>

#include "Game.hpp"
#include "gm/core/GameApp.hpp"

int main() {
  const std::string assetsDir = std::string(GM_ASSETS_DIR);
  Game game(assetsDir);

  gm::core::GameAppConfig config;
  config.width = 1280;
  config.height = 720;
  config.title = "GotMilkedSandbox";

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
