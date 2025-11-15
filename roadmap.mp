Engine/Game Split
1. Catalog shared engine subsystems (rendering, scene, physics, tooling) under `src/` and `include/gm/`; mark GotMilked-specific code in `apps/GotMilked/src`.
2. Move reusable systems (`GameResources`, `SceneSerializerExtensions`, `WeatherService`, `ScriptingHooks`) into `GotMilkedEngine`; keep `Game.cpp`, quest/dialogue systems, and farming gameplay in the app.
3. Introduce an explicit engine interface layer (`include/gm/runtime/`) that exposes services (event bus, save system, weather) via pure virtual facades; update `Game` to depend on those abstractions.
4. Split the build: produce `GotMilkedEngine` (static lib) and `GotMilkedGame` (exe) by updating root `CMakeLists.txt`; ensure tests link only against the engine where possible.
5. Document the boundary (assets, hot-reload, tooling) and add CI checks that block engine regressions when only game code changes.

Optimizations
1. Profile CPU/GPU cost of weather, terrain, and animal systems using Tracy + RenderDoc on scenes from `assets/scenes/`; capture baselines for sunny, stormy, dusk presets.
2. Optimize terrain and mesh rendering: merge material bindings in `EditableTerrainComponent`, add instancing for repeated props, and cache shader parameters in `GameRenderer`.
3. Parallelize gameplay updates (`QuestTriggerSystem`, `DialogueTriggerSystem`, cow AI) using the engine task graph so weather accumulation and animation evaluate on worker threads.
4. Rework resource streaming: load textures/meshes asynchronously in `GameResources`, precompile shaders, and double-buffer large uploads to minimize stalls.
5. Reduce save/load overhead by diffing weather and narrative data before writing; integrate a memory budget checker into the debug overlay to spot leaks.

Cleanups
1. Normalize code style via clang-format/clang-tidy, especially in new modules (`WeatherScenarioPanel`, `DebugToolingController`, `Game.cpp`).
2. Remove deprecated fog/weather prototypes and unused feature flags from configs and shader assets.
3. Consolidate duplicate math/util helpers under `gm::core`; eliminate redundant logging wrappers now that fmt-based logger exists.
4. Add regression tests for serialization, scripting hooks, and weather service to `tests/` to protect the new systems.
5. Expand docs: capture engine/game split guidelines, describe tooling (timeline editor, weather scenario harness), and record the new perf capture workflow in `/docs`.
