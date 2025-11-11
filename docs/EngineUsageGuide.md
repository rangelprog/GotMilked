# Engine Usage Guide

This guide summarizes how to build, run, and extend the GotMilked engine. It complements the scene management manual by covering the broader engine modules and typical workflows.

---

## 1. Build & Run

### Prerequisites
- CMake 3.27+
- Visual Studio 2022 (or another compiler supported by CMake)
- Windows 10 SDK (build files target 10.0.26100.0 by default)

### Steps
1. Configure: `cmake -S . -B build`
2. Build: `cmake --build build`
3. Run the game: `build/apps/GotMilked/Debug/GotMilked.exe`

The GotMilked game demonstrates engine features (scene management, rendering, serialization). It provides a clean starting point for building farming game content.

---

### Configuration

- Edit `apps/GotMilked/config.json` to change window defaults, toggle VSync, enable/disable resource hot reload, or relocate asset/save directories. Relative paths resolve against the config file’s directory, so source and build trees stay in sync. Hot reload polls watched files every `pollIntervalSeconds` (default 0.5s) and rebinds assets in the running scene whenever they change.
- Saves are written to the configured `paths.saves` directory. Press `F5` for a quick save and `F9` to load the most recent quick save (stubbed to capture camera and scene state). Slot-based saving lives in `gm::save::SaveManager` for future expansion.
- Press `F1` to toggle the tooling overlay (ImGui) at runtime. The panel exposes quick save/load buttons, resource reload controls, hot-reload settings, and live world/camera stats. Disable hot reload or trigger a manual reload directly from the overlay when debugging assets.

---

## 2. Engine Modules

| Module | Key Responsibilities |
| ------ | -------------------- |
| `gm/core` | Timing, input, events |
| `gm/rendering` | Camera, shader, mesh, texture, material, light manager |
| `gm/scene` | Scene, GameObject, components (transform/material/light), SceneManager, SceneSerializer |
| `gm/utils` | Resource loading (`ObjLoader`, `ResourceManager`), UI helpers |

Each module is built into the `GotMilkedEngine` library. Applications (like the GotMilked game) link against this library and provide their own components, resources, and serialization extensions.

> New in this revision: scenes own a stack of `SceneSystem` instances. The built-in `GameObjectUpdate` system preserves existing behaviour, while custom systems (AI, animation, physics) can slot in and even opt into async execution.

---

## 3. Scene Workflow Summary

The scene system revolves around `gm::Scene` and `gm::GameObject`. The detailed manual lives in `docs/SceneManagementManual.md`. Highlights:

1. Create or load scenes via `gm::SceneManager`.
2. Spawn game objects (`Scene::SpawnGameObject`) and attach components (`TransformComponent`, `MaterialComponent`, `LightComponent`, or custom ones).
3. Use tags and names for lookup (`Scene::FindGameObjectsByTag`, `FindGameObjectByName`).
4. Serialize with `Scene::SaveToFile` / `LoadFromFile`. Core components are handled by `gm::SceneSerializer`.
5. Extend serialization by registering custom component serializers with `gm::SceneSerializer::RegisterComponentSerializer` (the GotMilked game wraps this in `SceneSerializerExtensions::RegisterSerializers`). Store asset paths rather than raw pointers.
6. On load, rehydrate custom components by resolving assets through `gm::ResourceManager` or your own resource system.

> Component lifecycle: `gm::Component` exposes `Init`, `Update`, `Render`, and `OnDestroy`. Override only what you need; these hooks are invoked automatically by `GameObject`/`Scene`.

---

## 4. Resource Management Patterns

The engine supplies basic rendering wrappers but leaves resource ownership to the application:

- Use `gm::Texture::loadOrDie`, `gm::Shader::loadFromFiles`, and `gm::Mesh` (via `ObjLoader`) to load data.
- Use `gm::ResourceManager` for centralized resource loading and caching. Resources are loaded by name and automatically cached, preventing duplicate loads. `Has*` helpers let you check cache state, while `Reload*` APIs force a refresh from disk and log any failures.
- When serializing, store asset identifiers (paths or GUIDs) in the component JSON so resources can be rehydrated on load.
- Register asset GUIDs with `gm::ResourceRegistry` so scenes can resolve resources even if files move between releases.

---

## 5. Game Reference

The GotMilked game illustrates several patterns:

- **Resource management:** Uses `GameResources` to load and cache shared shader/mesh/texture resources by name.
- **Scene population:** `PopulateInitialScene` demonstrates how to spawn objects, attach components, and tag them.
- **Custom components:** Games can create their own components (e.g., crop components, animal components) and register serializers for them.

Use it as a template for building your farming game content.

---

## 6. Extending the Engine

Ideas for future expansion:

- Promote generic components (physics, animation, AI) by moving them from app to engine once they are widely useful. Consider implementing them as `SceneSystem` derivatives for cleaner lifecycle management.
- Expand serialization to cover new component types; ensure each component exposes a unique `GetName()`.
- Implement a resource registry with GUIDs to avoid path-dependent references.
- Add cross-platform build targets (Linux/macOS) by providing the appropriate CMake toolchains.
- Expose runtime configuration via environment variables; e.g., set `GM_LOG_DEBUG=0` to silence debug logging in debug builds or `=1` to re-enable it without recompiling.

### 6.1 Parallel Updates

- Call `scene->SetParallelGameObjectUpdates(true);` to fan out `GameObject::Update` across worker tasks.
- Only opt in once your component code is thread-safe; spawning/destroying objects during update still defers to the main thread at the end of the frame.
- Combine with custom async `SceneSystem` implementations to keep heavy work off the main thread.

For more detailed scene usage, consult `docs/SceneManagementManual.md`. The game code (`apps/GotMilked/`) showcases how the pieces fit together in practice.

