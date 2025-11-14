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

### Plugin Architecture & Extension Points

The debug menu now exposes a runtime plugin system so authoring tools can ship as DLLs/SOs without rebuilding the editor:

1. **Plugin descriptors** live in `assets/tools/plugins.json`. Each entry contains `name`, `path`, and optional shortcut/window metadata. `DebugToolingController` passes this file to `DebugMenu::SetPluginManifestPath`, and `ReloadPlugins()` rebuilds the catalog at runtime.
2. **Editor interface**: Implement `gm::debug::EditorPlugin` (see `include/gm/debug/EditorPlugin.hpp`). Provide `Name()`, `Initialize(EditorPluginHost&)`, `Render(EditorPluginHost&, bool& isVisible)`, and `Shutdown(EditorPluginHost&)`. Export `CreateEditorPlugin` / `DestroyEditorPlugin` functions with C linkage so the host can load the plugin.
3. **Host services** (`EditorPluginHost`): request engine state via helpers such as `GetGameResources()`, `GetActiveScene()`, `RegisterDockWindow()`, `RegisterShortcut()`, and `PushUndoableAction()`. These keep plugins sandboxed—no direct globals.
4. **Docking & layout**: Layouts persist to JSON (default `assets/tools/layouts/default.json`). Call `DebugMenu::SetLayoutProfilePath` to point at a different file and `MarkLayoutDirty()` whenever a plugin reconfigures dockspace nodes. Users can switch layouts at runtime via the Layouts submenu; autosave runs every few seconds when dirty.
5. **Shortcut bindings**: `DebugMenu` converts JSON descriptors into `ShortcutBinding` structs (key + modifiers). Plugins can call `RegisterShortcut(name, ShortcutDesc, callback)` to participate. Bindings serialize back into the layout profile so teams can share authored bindings.
6. **Undo/redo**: Tools that mutate scene/content data should wrap operations in `EditorAction` objects. The host exposes `PushUndoableAction(EditorAction action)`; undo/redo menu items call back into the registered lambdas.

When authoring new tooling, prefer the plugin route (DLL) for optional features and static linkage (`apps/GotMilked/src/debug_menu/*.cpp`) for always-on panels such as the Scene Outliner.

---

## Existing Components

| Component | Responsibility |
|-----------|----------------|
| `DebugMenu` | Main ImGui menu bar, file/save actions, console toggle, overlay toggle, scene inspector. |
| `DebugConsole` | Live log view that subscribes to `gm::core::Logger`. |
| `EditableTerrainComponent` | Terrain sculpting panel and in-view editing logic. Resolution changes now resample the existing heightmap rather than resetting it. |
| `gm::tooling::Overlay` | Global tooling panel (hot reload controls, quick save/load, diagnostics). |
| Grid Overlay | Toggleable world-space grid (press `G` while the HUD is visible). Useful for positioning assets in debug builds. |

All of these live under the `gm::debug` namespace (or `gm::tooling` for the overlay) and are guarded by `GM_DEBUG_TOOLS`.

---

## Scene Explorer Enhancements

The Scene Explorer window is the primary way to inspect and edit game objects at runtime:

* A hierarchical tree shows parent/child relationships. Drag a node onto another to reparent while preserving world transforms.
* Drop a node onto the blank area at the bottom of the list to unparent it and make it a root.
* The inspector displays both world and local transform values. Adjust local position/rotation/scale to work relative to the current parent.
* Parent metadata is surfaced in the inspector, with quick actions to focus the camera on the parent or unparent the selection.

When a text filter is active the hierarchy flattens into a list, but drag-and-drop parenting still works in the filtered view.

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


