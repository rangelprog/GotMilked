#pragma once

class Game;

class GameLoopController {
public:
    explicit GameLoopController(Game& game);

    void Update(float dt);

private:
    Game& m_game;

    void HandleResourceCatalogChanges();
    void UpdatePhysics(float dt);
    void HandleGlobalInputs();
    void HandleDebugShortcuts();
    void UpdateGameplay(float dt);
    void UpdateHotReloader(float dt);
};

