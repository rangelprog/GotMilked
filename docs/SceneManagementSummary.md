# Scene Management Summary

This page captures the high-level impact of the scene-management overhaul. For a full how-to, see `docs/SceneManagementManual.md`.

## Highlights

- **Unified Scene Flow** – `gm::Scene` now owns GameObject lifecycle (init/update/cleanup) and batching for render and light collection.
- **Fast Lookups** – Hash maps back `FindGameObjectByName` and tag queries; tags stay in sync through `Scene::TagGameObject`.
- **Deferred Destruction** – Objects mark themselves with `Destroy()` and are removed safely at frame end.
- **SceneManager** – Central singleton for creating, loading, and switching scenes without leaking state.
- **JSON Serialization** – `SceneSerializer` supports save/load for transforms, materials, and lights (extendable for new components). Prefabs piggyback on the same layer via `gm::scene::PrefabLibrary`, so designers can drop pre-configured GameObjects into the scene editor.

## Key APIs (cheat sheet)

```cpp
auto scene = gm::SceneManager::Instance().LoadScene("Level1");
scene->SpawnGameObject("Player");
scene->TagGameObject(player, "player");
scene->FindGameObjectsByTag("enemy");
scene->SaveToFile("assets/scenes/level1.json");
scene->LoadFromFile("assets/scenes/level1.json");
```

## Where to Go Next

- **How-To Guide:** `docs/SceneManagementManual.md` — step-by-step workflows, serialization details, and best practices.
- **Examples & Patterns:** Manual sections “Typical Scene Lifecycle”, “Working with GameObjects”, and “Troubleshooting” were expanded with the former `SceneManagementGuide.hpp` examples.
- **Future Work:** add serialization coverage for gameplay/renderer components, wire save/load into the editor, and build automated scene round-trip tests.

## Status

- Feature set: ✅ GameObject management, tags/layers, multi-scene support, lazy destruction, pause control, JSON save/load.
- Build: ✅ `cmake --build build`
