# GotMilked

The GotMilked gameplay application built on the GotMilkedEngine.

## Controls

- `WASD` – Move camera on the XZ plane
- `Space` / `Ctrl` – Move camera up/down
- `Right Mouse Button` – Hold to capture mouse look; release to restore cursor
- `Mouse Wheel` – Adjust camera FOV
- `F` – Toggle wireframe mode
- `F1` – Toggle the debug HUD (ImGui menu, console, terrain editor)
- `F5` – Quick save (writes to the configured saves directory)
- `F9` – Quick load the most recent quick save
- `Esc` – Quit

## Scene Workflow

The game starts by loading `GameScene` via `gm::SceneManager`. If no scene exists it seeds a default layout (sun light + editable terrain) via `GameSceneHelpers.cpp`.

Quick save / load go through `gm::save::SaveManager`. Save files live under the configured `paths.saves` directory; when unset the engine defaults to `~/Documents/GotMilked/saves` (or the Windows equivalent).

## Debug HUD & Terrain Editor

The debug HUD is only available when the project is built with `-DGM_ENABLE_DEBUG_TOOLS=ON` (the default for debug builds).

- Use the **Debug** menu to show the console or tooling overlay.
- The **Scene Editor** window offers a hierarchy view and inspector.
- Editable terrain exposes brush radius, strength, min/max heights, and a **Resolution** slider. Changing resolution now resamples the existing heightmap, so any sculpted detail is preserved when you increase/decrease the grid density.
- Toggle terrain editing with `T` (when the HUD has focus). LMB raises, RMB lowers.

## Configuration

Game configuration lives in `apps/GotMilked/config.json`.

- Window: width/height, vsync, FPS counter update interval.
- Paths: override assets/saves directories.
- Hot reload: toggle and adjust polling interval.

See `docs/EngineUsageGuide.md` and `docs/DebugToolsGuide.md` for more detail on configuration and debug tooling.

