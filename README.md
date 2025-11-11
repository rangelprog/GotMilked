# GotMilked

> TIP: Replace `<owner>` with your GitHub user or organization name.

![CI](https://github.com/<owner>/GotMilked/actions/workflows/ci.yml/badge.svg)

GotMilked is a C++ game engine and sandbox demo showcasing a component-based scene system, modern OpenGL rendering, and JSON scene serialization.

## Documentation

- [Engine Usage Guide](docs/EngineUsageGuide.md) – build/run instructions, module overview, workflow summary, and component lifecycle overview.
- [Scene Management Manual](docs/SceneManagementManual.md) – deep dive into scenes, custom serializers, and extension points.
- [.github/workflows/ci.yml](.github/workflows/ci.yml) – CI pipeline that builds the engine and runs the unit/smoke tests on every push and PR.

## Quick Start

1. Configure the project: `cmake -S . -B build`
2. Build the engine and sandbox: `cmake --build build`
3. Launch the sandbox: `build/apps/GotMilkedSandbox/Debug/GotMilkedSandbox.exe`

The sandbox demonstrates the engine in action with generic mesh spinner components that serialize to JSON and rehydrate assets on load. Use it as a template for your own scenes or gameplay experiments.

## Testing

- Enable tests (default ON in CI): `cmake -S . -B build -DGM_BUILD_TESTS=ON`
- Build tests: `cmake --build build --config Debug`
- Run tests: `ctest --test-dir build -C Debug --output-on-failure`

The test suite includes coverage for scene serialization, resource caching/reloading, and a headless OpenGL smoke test that exercises `Scene::Draw`.

## Continuous Integration

- GitHub Actions workflow: `.github/workflows/ci.yml`
  - Configures CMake with `GM_BUILD_TESTS=ON` and disables the sandbox for faster builds.
  - Builds the engine in Debug configuration.
  - Executes `ctest -C Debug --output-on-failure`, ensuring every PR runs unit and smoke tests automatically.
- Update the badge URL above once the repository lives under your GitHub account.

## Debug Logging

`Logger::Debug` output is compiled in for debug builds but muted automatically in release builds. To control verbosity at runtime:

- `GM_LOG_DEBUG=1` (or `true/on/yes`) – enable debug logs
- `GM_LOG_DEBUG=0` (or `false/off/no`) – suppress debug logs

You can also toggle programmatically via `gm::core::Logger::SetDebugEnabled(bool)`.
