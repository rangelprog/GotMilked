# Working with Debug Tools

The GotMilked game ships with an ImGui-based debug HUD that can be compiled in or out. This page explains how to enable it, where to extend it, and which components provide the existing functionality.

---

## Enabling / Disabling the HUD

Debug tooling is controlled by the CMake option `GM_ENABLE_DEBUG_TOOLS`. In debug builds the option defaults to `ON`; in other configurations you can set it explicitly:

```bash
cmake -S . -B build -DGM_ENABLE_DEBUG_TOOLS=ON   # enable HUD
cmake -S . -B build -DGM_ENABLE_DEBUG_TOOLS=OFF  # strip HUD from the build
```

When disabled, all HUD sources are removed from the target and the macro `GM_DEBUG_TOOLS` evaluates to `0`, so release builds avoid the debug headers entirely.

---

## Extending the HUD

### Entry Points

* `gm::debug::DebugHudController` (`apps/GotMilked/src/DebugHudController.*`) orchestrates menu/console visibility, the tooling overlay, and any terrain editors. Add new windows by registering them with the controller or having the controller toggle their visibility.
* `gm::debug::DebugMenu` is split across `DebugMenu.cpp`, `DebugMenu_Menus.cpp`, `DebugMenu_Windows.cpp`, and `DebugMenu_RecentFiles.cpp`. Use the appropriate file when adding menu items, editor windows, or recent-file plumbing.
* `gm::tooling::Overlay` (`src/tooling/Overlay.*`) is the game-wide tooling panel (hot reload, quick save/load, diagnostics). Extend its sections when you need runtime controls tied to the broader engine state.
* `gm::debug::EditableTerrainComponent` (`apps/GotMilked/src/EditableTerrainComponent.*`) implements the terrain editor widget and in-scene sculpting. Use it as a template for other editor-style components that need access to the HUD.

### Hooking Into the Controller

1. Use `DebugHudController::RegisterTerrain` (or add similar registration points) so the controller can drive visibility for new editor components.
2. Provide toggle callbacks to the menu via `DebugMenu::SetOverlayToggleCallbacks` (or related helpers) so state changes route through the controller instead of touching individual widgets.
3. Call `DebugHudController::Refresh()` if a resource reload or hot-reload path needs to reapply visibility/editing flags (e.g., after shaders recompile).

---

## Existing Components

| Component | Responsibility |
|-----------|----------------|
| `DebugMenu` | Main ImGui menu bar, file/save actions, console toggle, overlay toggle, scene inspector. |
| `DebugConsole` | Live log view that subscribes to `gm::core::Logger`. |
| `EditableTerrainComponent` | Terrain sculpting panel and in-view editing logic. Resolution changes now resample the existing heightmap rather than resetting it. |
| `gm::tooling::Overlay` | Global tooling panel (hot reload controls, quick save/load, diagnostics). |

All of these live under the `gm::debug` namespace (or `gm::tooling` for the overlay) and are guarded by `GM_DEBUG_TOOLS`.

---

## Terrain Editor Tips

* The inspector window exposes sliders for brush radius, strength, min/max heights, and resolution.
* Adjusting the resolution resamples the current heightmap via bilinear interpolation, preserving sculpted detail when you increase/halve the grid.
* Toggle editing with `T` (when the HUD is active) and use LMB/RMB to raise/lower terrain.

---

## Debug Namespace & Project Notes

* Keep debug-only headers and sources wrapped with `#if GM_DEBUG_TOOLS` and under the `gm::debug` namespace to avoid pulling them into release builds.
* Update this guide whenever new tooling modules are added (camera controllers, profiler overlays, etc.).
* Tests covering logger/listener behaviour and resource reloads live in `tests/`. Run `ctest` after modifying debug tooling to ensure you haven’t regressed log output or serialization.


