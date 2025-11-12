# GotMilked Engine & Game

GotMilked is a custom C++ farming-game prototype built on a lightweight in-house engine. This repository contains both the reusable engine modules and the "GotMilked" gameplay application.

## Repository Layout

- `include/`, `src/` – core engine systems: rendering, scene graph, physics integration, resource management, tooling.
- `apps/GotMilked/` – the gameplay layer, assets, and debug tooling glue.
- `docs/` – in-depth guides for engine usage, debug tools, profiling, and scene management.
- `tests/` – Catch2-based unit/integration tests for serialization, resource handling, rendering smoke cases, and logger behaviour.

## Building

```
cmake -S . -B build -DGM_BUILD_TESTS=ON
cmake --build build --config Debug
```

Enable the debug tooling overlay by passing `-DGM_ENABLE_DEBUG_TOOLS=ON` (on by default). This compiles the ImGui-based HUD, debug console, and terrain editor.

## Running

Launch the gameplay application after configuring:

```
cmake --build build --target GotMilked --config Debug
./build/apps/GotMilked/Debug/GotMilked.exe
```

Configuration is driven by `apps/GotMilked/config.json`. See `docs/EngineUsageGuide.md` for the available options (window, resource paths, hot reload, etc.).

## Testing

```
cmake --build build --target GotMilkedTests --config Debug
ctest --test-dir build/tests -C Debug --output-on-failure
```

Tests cover scene serialization, resource manager caching/reloads, logger output/listeners, and render smoke cases. They run against the engine library and mock assets.

## Key Documentation

- `docs/EngineUsageGuide.md` – building, configuration, and integration tips.
- `docs/DebugToolsGuide.md` – enabling the debug HUD, extending the menu/console, terrain editor usage.
- `docs/SceneManagementSummary.md` – scene systems, pooling, naming lookups, serialization notes.
- `docs/ProfilingRecommendations.md` – instrumentation advice and performance hotspots.

## Contributing

1. Enable tests (`-DGM_BUILD_TESTS=ON`) and ensure `ctest` passes.
2. Run static analysis / lint (MSVC /W4, clang-tidy where available).
3. Update relevant docs when adding systems (debug tools, renderer, gameplay layers).
4. Keep debug-only code wrapped in `GM_DEBUG_TOOLS` and place debug UI in `gm::debug` namespaces.
