#pragma once

class Game;

class GameRenderer {
public:
    explicit GameRenderer(Game& game);

    void Render();

private:
    Game& m_game;
};

