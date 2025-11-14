# Engine Usage Guide

This guide summarizes how to build, run, and extend the GotMilked engine. It complements the scene management manual by covering the broader engine modules and typical workflows.

---

## 1. Build & Run

### Prerequisites
- CMake 3.21+
- Visual Studio 2022 (or another compiler supported by CMake)
- Windows 10 SDK (build files target 10.0.26100.0 by default)

### Steps
1. Configure (preset recommended): `cmake --preset windows-msvc` (Linux: `cmake --preset linux-clang`)
2. Build: `cmake --build --preset windows-msvc`
3. Run the game: `build/windows-msvc-debug/apps/GotMilked/GotMilked.exe`

### Run Tests

The repository ships with a Catch2-based test suite that validates serialization, resource loading, and rendering smoke paths.

1. Build the aggregated test binary: `cmake --build --preset windows-msvc --target GotMilkedTests`
2. Execute all tests: `ctest --preset windows-msvc`

Useful flags:

- Filter by test name or tag: `ctest --preset windows-msvc -R GameResources` or run a single case via `build/windows-msvc-debug/tests/GotMilkedTests.exe "Scene draws without errors"`.
- Emit full failure logs: `ctest --preset windows-msvc --output-on-failure`.

Catch2 discovers tests automatically (`catch_discover_tests`), so adding a new `TEST_CASE` in `tests/` is enough for CMake to register it the next time you configure the project.

The GotMilked game demonstrates engine features (scene management, rendering, serialization). It provides a clean starting point for building farming game content.

---

### Configuration

- Edit `apps/GotMilked/config.json` to change window defaults, toggle VSync, enable/disable resource hot reload, or relocate asset/save directories. Relative paths resolve against the config file’s directory, so source and build trees stay in sync. Hot reload polls watched files every `pollIntervalSeconds` (default 0.5s) and rebinds assets in the running scene whenever they change.
- Saves are written to the configured `paths.saves` directory. Press `F5` for a quick save and `F9` to load the most recent quick save (stubbed to capture camera and scene state). Slot-based saving lives in `gm::save::SaveManager` for future expansion. If no saves path is specified, the engine now defaults to the user documents folder (`~/Documents/GotMilked/saves` or `%USERPROFILE%\Documents\GotMilked\saves`).
- Press `F1` to toggle the debug HUD (ImGui). Build the HUD by configuring CMake with `-DGM_ENABLE_DEBUG_TOOLS=ON`. Once the HUD is visible, open the Debug menu to toggle the tooling overlay, console, or terrain editor. Terrain resolution changes now resample the existing heightmap instead of wiping it, so you can refine sculpted terrain without losing detail. Disable hot reload or trigger a manual reload directly from the overlay when debugging assets.
- Run `cmake --build --preset windows-msvc --target lock-deps` to prefetch third-party dependencies into `external/`, avoiding network fetches during normal builds.
- Sanitizer presets (`windows-asan`, `linux-clang-asan`) enable AddressSanitizer via toolchain files; use them to catch memory issues early when working on engine code.

---

## 2. Engine Modules & Boundaries

All engine code now compiles into focused static libraries that the umbrella `GotMilkedEngine` INTERFACE target re-exports. Keep new symbols inside the module that owns their domain to avoid the monolithic include chains we just retired.

| Module | Responsibilities | Depends On |
| ------ | ---------------- | ---------- |
| `gm_core` | Entry (`GameApp`), logging, timing, input, config plumbing | n/a |
| `gm_rendering` | GPU abstractions: `Shader`, `Mesh`, `Material`, `Camera`, `LightManager`, batch helpers | `gm_core` |
| `gm_animation` | `Skeleton`, `AnimationClip`, `AnimatorComponent`, animation graph helpers | `gm_rendering`, `gm_scene` |
| `gm_scene` | `Scene`, `SceneManager`, `GameObjectScheduler`, `RenderBatcher`, schema-driven serialization | `gm_core`, `gm_rendering`, `gm_utils` |
| `gm_utils` | `ResourceManager`, threading, profilers, OBJ/YAML helpers | `gm_core` |
| `gm_physics` | `PhysicsWorld`, `RigidBodyComponent`, Jolt bindings | `gm_scene` |
| `gm_save` | Save/load infrastructure, diffing, snapshots | `gm_utils` |
| `gm_tooling` | Overlay plumbing, catalog listeners, asset watch helpers | `gm_utils`, `gm_scene` |
| `gm_assets` | Asset database, catalog, importers | `gm_utils` |
| `gm_content` | `ContentDatabase`, schema registry, validation | `gm_assets` |
| `gm_runtime` | Thin runtime glue, hot reload plumbing shared between editor/game | `gm_scene`, `gm_utils` |
| `gm_debug` (optional) | Debug HUD, ImGui bridge, plugin host | depends on `gm_tooling`, `gm_scene` |

> `apps/GotMilked` only accesses services through their published headers. If you need a type from another module, include its header directly rather than reaching through `gm/Engine.hpp`.

---

## 3. Scene Workflow Summary

The scene system revolves around `gm::Scene` and `gm::GameObject`. The detailed manual lives in `docs/SceneManagementManual.md`. Highlights:

1. Create or load scenes via `gm::SceneManager`.
2. Spawn game objects (`Scene::SpawnGameObject`) and attach components (`TransformComponent`, `MaterialComponent`, `LightComponent`, or custom ones).
3. Use tags and names for lookup (`Scene::FindGameObjectsByTag`, `FindGameObjectByName`).
4. Serialize with `Scene::SaveToFile` / `LoadFromFile`. Core components are handled by `gm::SceneSerializer`.
5. Extend serialization by registering custom component serializers with `gm::SceneSerializer::RegisterComponentSerializer` (the GotMilked game wraps this in `SceneSerializerExtensions::RegisterSerializers`). Store asset paths rather than raw pointers.
6. On load, rehydrate custom components by resolving assets through `gm::ResourceManager` or your own resource system. The scene serializer now clears tag/name caches before recycling objects, preventing orphaned references during repeated load/unload cycles when pooling is active.

> Component lifecycle: `gm::Component` exposes `Init`, `Update`, `Render`, and `OnDestroy`. Override only what you need; these hooks are invoked automatically by `GameObject`/`Scene`.

---

## 4. Resource Management Patterns

The engine supplies basic rendering wrappers but leaves resource ownership to the application:

- Use `gm::Texture::loadOrThrow`, `gm::Shader::loadFromFiles`, and `gm::Mesh` (via `ObjLoader`) to load data.
- Use `gm::ResourceManager` for centralized resource loading and caching. The manager now provides a per-thread `ScopedRegistry` so background jobs can stage loads without trampling the global registry lock. Call `gm::ResourceManager::Init()` once at startup (typically in `GameApp::onInit`), wrap bulk loads in `ScopedRegistry` instances, and always call `Cleanup()` during shutdown to release GPU/CPU allocations deterministically.
- When serializing, store asset identifiers (paths or GUIDs) in the component JSON so resources can be rehydrated on load.
- Register asset GUIDs with `gm::ResourceRegistry` so scenes can resolve resources even if files move between releases.

### Registry & Database Lifecycles

| Service | Initialize | Use | Shutdown |
| ------- | ---------- | --- | -------- |
| `gm::ResourceManager` | `ResourceManager::Init(config)` once per process. Set an explicit registry (`SetRegistry`) when bootstrapping headless tools. | Acquire handles via `LoadOrStoreResource<T>(guid, loader)`. Use `ScopedRegistry` to give worker threads their own caches. | `ResourceManager::Cleanup()` to flush caches and join outstanding notifications. |
| `gm::scene::ComponentSchemaRegistry` | Auto-populated via `GM_REGISTER_COMPONENT_*` macros when each TU initializes. Call `ComponentSchemaRegistry::Instance().Finalize()` from app startup if you need to assert on missing schemas. | `ComponentDescriptor` objects supply serializer/deserializer lambdas that `SceneSerializer` consumes. | No explicit shutdown; lives until process exit. |
| `gm::content::ContentDatabase` | `contentDb.Initialize(assetRoot)` after the asset manifest and schema directories exist. Provide a callback interface for validation events (see `DebugToolingController`). | `FindRecord(type, id)` during gameplay/editor sessions. Subscribe to `OnContentChanged` to react to hot reload. | `contentDb.Shutdown()` before destroying file watchers or asset catalogs. |
| `gm::content::ContentSchemaRegistry` | `LoadSchemas(schemaDir)` (happens inside `ContentDatabase::Initialize`). | `GetSchema(type)` feeds serializer/editor tooling. | No explicit shutdown beyond releasing owning `ContentDatabase`. |

The GotMilked game wires these services inside `Game::SetupResources` and `DebugToolingController`, but the pattern is the same for standalone tools:

```cpp
gm::ResourceManager::Init({ .assetRoot = assetsPath });
gm::content::ContentDatabase contentDb;
contentDb.Initialize(assetsPath / "content");
auto scoped = gm::ResourceManager::ScopedRegistry::Create();
// Load assets / scenes
contentDb.Shutdown();
gm::ResourceManager::Cleanup();
```

---

## 5. Game Reference

The GotMilked game illustrates several patterns:

- **Resource management:** Uses `GameResources` to load and cache shared shader/mesh/texture resources by name.
- **Scene population:** `PopulateInitialScene` demonstrates how to spawn objects, attach components, and tag them.
- **Custom components:** Games can create their own components (e.g., crop components, animal components) and register serializers for them.
- **Terrain editing:** The built-in `EditableTerrainComponent` supports brush sculpting and on-the-fly resolution changes. The inspector exposes sliders for size, min/max heights, brush radius/strength, and resolution; changing resolution resamples the current height data.

Use it as a template for building your farming game content.

---

## 6. Extending the Engine

Ideas for future expansion:

- Promote generic components (physics, animation, AI) by moving them from app to engine once they are widely useful. Consider implementing them as `SceneSystem` derivatives for cleaner lifecycle management.
- Expand serialization to cover new component types; ensure each component exposes a unique `GetName()`.
- Implement a resource registry with GUIDs to avoid path-dependent references.
- Add cross-platform build targets (Linux/macOS) by providing the appropriate CMake toolchains.
- Expose runtime configuration via environment variables; e.g., set `GM_LOG_DEBUG=0` to silence debug logging in debug builds or `=1` to re-enable it without recompiling.
- Add unit tests for new utilities (resource manager, logger, serialization). Catch2 suites already cover these areas; add new cases in `tests/` so CI runs them alongside existing checks.

### 6.1 Parallel Updates

- Call `