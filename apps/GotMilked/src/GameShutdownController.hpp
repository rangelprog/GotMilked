#pragma once

class Game;

class GameShutdownController {
public:
    explicit GameShutdownController(Game& game);

    void Shutdown();

private:
    Game& m_game;
};

