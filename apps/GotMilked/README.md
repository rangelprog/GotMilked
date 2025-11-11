# GotMilked

The GotMilked game application built on the GotMilkedEngine.

## Controls

- `WASD` – Move camera on the XZ plane
- `Space` / `Ctrl` – Move camera up/down
- `Right Mouse Button` – Hold to capture mouse look; release to restore cursor
- `Mouse Wheel` – Adjust camera FOV
- `F` – Toggle wireframe mode
- `Esc` – Quit

## Scene Workflow

The game starts by loading `GameScene` via `gm::SceneManager`. If no scene exists on disk it seeds the scene with initial content (see `GameSceneHelpers.cpp`). You can interact with the scene using the serializer keybinds built into your app layer; calling `Game::SetupScene()` after a load will rehydrate saved components using `GameResources`.

