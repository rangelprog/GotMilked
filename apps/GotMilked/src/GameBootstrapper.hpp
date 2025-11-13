#pragma once

struct GLFWwindow;

namespace gm {
class SceneManager;
}

class Game;

class GameBootstrapper {
public:
    explicit GameBootstrapper(Game& game);

    bool Initialize(GLFWwindow* window, gm::SceneManager& sceneManager);

private:
    Game& m_game;
};

