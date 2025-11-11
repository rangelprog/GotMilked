# GotMilked Sandbox

The sandbox demonstrates the engine’s mesh-spinner demo scene and provides a convenient playground for experimenting with scene serialization.

## Controls

- `WASD` – Move camera on the XZ plane
- `Space` / `Ctrl` – Move camera up/down
- `Right Mouse Button` – Hold to capture mouse look; release to restore cursor
- `Mouse Wheel` – Adjust camera FOV
- `F` – Toggle wireframe mode
- `Esc` – Quit

## Scene Workflow

The sandbox starts by loading `GameScene` via `gm::SceneManager`. If no scene exists on disk it seeds the scene with three mesh spinners (see `SandboxSceneHelpers.cpp`). You can interact with the scene using the serializer keybinds built into your app layer; calling `Game::SetupScene()` after a load will rehydrate saved components using `SandboxResources`.

## Testing

The unit/smoke tests reuse the same mesh-spinner helpers:

- `SandboxResources` encapsulates the shared shader/mesh/texture handles.
- `SandboxSceneHelpers` seeds a scene or rehydrates components after load.
- Test assets are generated at runtime in `tests/TestAssetHelpers.cpp`, so the smoke test exercises the same creation path as the sandbox.

