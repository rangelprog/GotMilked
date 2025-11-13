#pragma once

#include <memory>
#include <optional>

struct GLFWwindow;

namespace gm {
class Scene;
namespace debug {
class DebugMenu;
class DebugConsole;
class DebugHudController;
class GridRenderer;
class EditableTerrainComponent;
} // namespace debug
namespace tooling {
class Overlay;
} // namespace tooling
namespace utils {
class ImGuiManager;
} // namespace utils
} // namespace gm

class Game;

class DebugToolingController {
public:
    explicit DebugToolingController(Game& game);

    bool Initialize();

#if GM_DEBUG_TOOLS
    void ConfigureDebugMenu();
#endif

private:
    Game& m_game;
};

