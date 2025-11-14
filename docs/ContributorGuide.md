# GotMilked Contributor Guide

The goal of this guide is to help new contributors extend the engine and sample game without reintroducing the tight coupling and ad‑hoc tooling we just removed. It covers:

1. How to add/extend schema‑driven game data (quests, vehicles, buildings, etc.).
2. Which subsystems own which responsibilities so features land in the right module.
3. Best practices for extending components, tooling, and tests while keeping CI happy.

Use this alongside the existing `ComponentRegistrationGuide`, `EngineUsageGuide`, and `DebugToolsGuide`.

---

## 1. Data Schemas & Content Ownership

All structured content lives under `assets/content`:

| Path | Purpose |
| ---- | ------- |
| `assets/content/schemas/*.schema.json` | Canonical JSON Schema definitions (Draft 2020-12). |
| `assets/content/data/<type>/` | Records of a specific type (JSON or YAML). Directory name must match a schema filename (e.g. `quests` ⟷ `quests.schema.json`). |

### 1.1 Authoring a New Schema

1. Create `assets/content/schemas/<type>.schema.json`.
   - Expose top-level `id` or `guid` fields for stable references.
   - Add `"additionalProperties": false` to catch typos early.
   - Use `"gmCategory"`/`"description"` metadata so tooling panels can surface friendly names.
2. Populate `assets/content/data/<type>/` with seed records (`.json` or `.yaml`).
3. Add sample data to keep tests meaningful (e.g. `tests/GameResourcesTests.cpp` reads quest data).
4. Run `python scripts/validate_content.py` locally; CI will block merges if schemas or records fail validation.

### 1.2 Ownership & Lifecycles

| System | Responsibilities | Hand-off Checklist |
| ------ | ---------------- | ------------------ |
| `gm::content::ContentDatabase` | Loads schemas, parses data, issues validation events. | Call `Initialize(assetRoot)` during bootstrap, register validation listeners (e.g. `DebugToolingController`), call `Shutdown()` before tearing down file watchers. |
| Game feature teams (quests, vehicles, buildings) | Define schemas + provide canonical samples. Own data updates and validator rules. | Update schema + examples in the same PR. Add unit tests or tooling panels if behaviour changes. |
| Tooling team | Ensure debug panels (`ContentValidationPanel`) visualize schema issues. | Keep panel in sync with new record types; add filters/columns as new metadata appears. |

### 1.3 Hot Reload & Validation Events

- `ContentDatabase` emits `ContentEvent` records describing add/update/remove results.
- Subscribe via `ContentDatabase::RegisterListener`. Tooling already relays these into ImGui to highlight schema violations.
- When adding new content types, extend `ContentValidationPanel` to explain the new category and link to documentation.

---

## 2. Subsystem Ownership Map

Follow this table when deciding where new features belong:

| Subsystem | Module(s) | Owns | Examples |
| --------- | --------- | ---- | -------- |
| Core Runtime | `gm_core`, `gm_runtime` | App shell, logging, input configuration, timing, GameApp context. | New OS integrations, additional window hints, input devices. |
| Rendering | `gm_rendering` | GPU buffer abstractions, batching, shader/material helpers, light manager. | New shader stages, render passes, batching strategies. |
| Scene & Gameplay | `gm_scene`, `gm_animation`, `gm_physics` | Scene graph, systems, scheduler, component serialization, animation, physics bindings. | New SceneSystem types, schema-driven components, animation controllers. |
| Resources & Assets | `gm_utils`, `gm_assets`, `gm_content` | ResourceManager registries, asset catalogs, content schemas, hot reload. | GUID registries, asset importers, content validation logic. |
| Tooling & Debug | `gm_tooling`, `gm_debug`, app `debug_menu/*` | Debug HUD, plugin host, overlay, editor panels, ImGui integrations. | Plugin APIs, layout persistence, inspector improvements. |
| Game/Application | `apps/GotMilked/*` | Feature-specific logic (quests, cows, tractors), gameplay components, high-level loop. | New quests, vehicle behaviours, mission scripting. |

**Rule of thumb:** if a feature is reusable across games, it belongs in an engine module; otherwise keep it in `apps/GotMilked` and document it there.

---

## 3. Extension Best Practices

### 3.1 Components & Scenes

- Use `GM_REGISTER_COMPONENT_*` macros so components self-register and pick up schema metadata automatically.
- Prefer schema-driven serialization (`ComponentDescriptor` fields) before falling back to custom JSON.
- Keep component headers lean; implementation files should include only what they need.
- When adding high-level components (NPC quest givers, vehicle rigs), place reusable logic in `gm_scene` or `gm_animation`, but keep game-specific flavour in `apps/GotMilked`.

### 3.2 Resource & Content Services

- Always pair `ResourceManager::Init()` with `ResourceManager::Cleanup()` in tests/tools; wrap per-thread loads with `ScopedRegistry`.
- Keep asset GUIDs stable. When moving files, update manifests and sample data in the same change.
- Add content validation tests in `tests/GameplayToolingTests.cpp` (or new suites) to model workflows headlessly.

### 3.3 Tooling Plugins

- For optional authoring tools (quest editor, vehicle tuner), ship them as plugins:
  1. Implement `EditorPlugin`.
  2. Export factory functions.
  3. Register dock windows + shortcuts via `EditorPluginHost`.
  4. Store layouts/shortcuts in `assets/tools/layouts/*.json`.
- Use undo/redo helpers (`PushUndoableAction`) for destructive operations. Always mirror scene edits in serialized data (schema-driven) so they survive reloads.

### 3.4 Testing & CI Expectations

- Extend Catch2 suites when touching:
  - Serialization (add scene/component tests).
  - Resource/catalog behaviour (asset reload tests).
  - Tooling/ImGui (headless smoke tests).
- Run `ctest`, `python scripts/validate_content.py`, and `markdownlint` locally; GitHub Actions will run these plus clang-tidy and module builds across OS/tooling combinations.
- For new schemas or tooling features, update docs + samples in the same PR—CI enforces this by linting docs and validating schemas.

---

## 4. Submission Checklist

- [ ] Tests pass locally (`cmake --build ... --target GotMilkedTests && ctest`).
- [ ] `python scripts/validate_content.py` reports “All content files satisfied their schemas.”
- [ ] Documentation updated (`docs/EngineUsageGuide.md`, `docs/DebugToolsGuide.md`, or this guide) if APIs, schemas, or ownership rules changed.
- [ ] New editor tooling either integrates with the plugin host or explains why it must be statically linked.
- [ ] For data changes: schemas + samples + gameplay logic all updated in a single PR.

Following these rules keeps the project modular and makes it easier to add future systems (NPC questlines, tractor controllers, new building types) without backsliding into all-in-one files.

