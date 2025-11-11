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
3. Run the sandbox: `build/apps/GotMilkedSandbox/Debug/GotMilkedSandbox.exe`

The sandbox demonstrates engine features (scene management, rendering, serialization). It now uses generic `MeshSpinnerComponent` instances, avoiding asset-specific code.

---

## 2. Engine Modules

| Module | Key Responsibilities |
| ------ | -------------------- |
| `gm/core` | Timing, input, events |
| `gm/rendering` | Camera, shader, mesh, texture, material, light manager |
| `gm/scene` | Scene, GameObject, components (transform/material/light), SceneManager, SceneSerializer |
| `gm/utils` | Resource loading (`ObjLoader`, `ResourceManager`), UI helpers |

Each module is built into the `GotMilkedEngine` library. Applications (like the sandbox) link against this library and provide their own components, resources, and serialization extensions.

---

## 3. Scene Workflow Summary

The scene system revolves around `gm::Scene` and `gm::GameObject`. The detailed manual lives in `docs/SceneManagementManual.md`. Highlights:

1. Create or load scenes via `gm::SceneManager`.
2. Spawn game objects (`Scene::SpawnGameObject`) and attach components (`TransformComponent`, `MaterialComponent`, `LightComponent`, or custom ones).
3. Use tags and names for lookup (`Scene::FindGameObjectsByTag`, `FindGameObjectByName`).
4. Serialize with `Scene::SaveToFile` / `LoadFromFile`. Core components are handled by `gm::SceneSerializer`.
5. Extend serialization by registering custom component serializers with `gm::SceneSerializer::RegisterComponentSerializer` (the sandbox wraps this in `SceneSerializerExtensions::RegisterSerializers`). Store asset paths rather than raw pointers.
6. On load, rehydrate custom components by resolving assets through `gm::ResourceManager` or your own resource system.

> Component lifecycle: `gm::Component` exposes `Init`, `Update`, `Render`, and `OnDestroy`. Override only what you need; these hooks are invoked automatically by `GameObject`/`Scene`.

---

## 4. Resource Management Patterns

The engine supplies basic rendering wrappers but leaves resource ownership to the application:

- Use `gm::Texture::loadOrDie`, `gm::Shader::loadFromFiles`, and `gm::Mesh` (via `ObjLoader`) to load data.
- Use `gm::ResourceManager` for centralized resource loading and caching. Resources are loaded by name and automatically cached, preventing duplicate loads. `Has*` helpers let you check cache state, while `Reload*` APIs force a refresh from disk and log any failures.
- When serializing, store asset identifiers (paths or GUIDs) in the component JSON so resources can be rehydrated on load.

---

## 5. Sandbox Reference

The sandbox illustrates several patterns:

- **Resource management:** Uses `gm::ResourceManager` to load and cache shared shader/mesh/texture resources by name.
- **Generic renderer:** `MeshSpinnerComponent` spins a mesh and renders with the shared resources.
- **Scene population:** `PopulateSandboxScene` demonstrates how to spawn objects, attach components, and tag them.
- **Rehydration:** Upon loading a scene, `RehydrateMeshSpinnerComponents` restores asset pointers from `ResourceManager` using stored paths.

Use it as a template for your own demo or gameplay scenes.

---

## 6. Extending the Engine

Ideas for future expansion:

- Promote generic components (physics, animation, AI) by moving them from app to engine once they are widely useful.
- Expand serialization to cover new component types; ensure each component exposes a unique `GetName()`.
- Implement a resource registry with GUIDs to avoid path-dependent references.
- Add cross-platform build targets (Linux/macOS) by providing the appropriate CMake toolchains.
- Expose runtime configuration via environment variables; e.g., set `GM_LOG_DEBUG=0` to silence debug logging in debug builds or `=1` to re-enable it without recompiling.

For more detailed scene usage, consult `docs/SceneManagementManual.md`. The sandbox code (`apps/GotMilkedSandbox/`) showcases how the pieces fit together in practice.

