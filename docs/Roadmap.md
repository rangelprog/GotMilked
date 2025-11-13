# GotMilked Engine & Game Roadmap

## Vision
- Deliver a moddable farming sandbox where cows and dairy economics anchor progression, while the underlying engine remains reusable for future simulations.
- Empower designers with fast iteration tools (scene editor, content browser, hot reload) so gameplay and world-building can evolve without programmer intervention.

## Guiding Principles
- **One Codebase, Two Products**: treat the engine and the GotMilked game as separate shippable products sharing a common foundation.
- **Data-First Workflows**: prioritize pipelines where assets, quests, and tuning live outside of code and can be reloaded at runtime.
- **Instrumentation Everywhere**: design systems with profiling, logging, and debug vis in mind to de-risk late-game performance and gameplay tuning.
- **Incremental Validation**: every milestone closes with a playtestable slice and automated coverage to lock in quality.

## Milestone Overview

| Phase | Focus | Target Duration | Exit Criteria |
| --- | --- | --- | --- |
| 1 | Engine Foundations | 4–6 weeks | Stable rendering/input loop, automated resource discovery, CI builds |
| 2 | Tooling & Workflow | 4 weeks | Scene editor + content browser pipeline, save/load authoring, designer docs |
| 3 | Core Gameplay Loop | 6 weeks | Day/night cycle, cow milking economy, baseline AI behaviours |
| 4 | World & Systems Expansion | 6–8 weeks | NPC quests, vehicles, farm infrastructure, quest scripting |
| 5 | Production Quality | 4 weeks | Performance targets met, comprehensive test harness, telemetry hooks |
| 6 | Launch Prep & Operations | 2–4 weeks | Packaging, release automation, live-ops tooling, player support plan |

## Phase Detail

### Phase 1 – Engine Foundations
- **Rendering & Scene Graph**: finalize camera path, culling strategy, render state cache, and shader management; document extension points.
- **Resource Pipeline Rework**:
  - Replace manual `resources.json` entries with automated asset discovery (per-type folder scans, deterministic GUID generation).
  - Unify `ResourceRegistry` into a thread-safe `ResourceDatabase` service with versioned descriptors and hot-reload notifications.
  - Maintain optional manifest overrides for curated materials or metadata.
- **Platform & Build**: enforce /W4, sanitizers where possible, static analysis gates in CI/CD; produce reproducible Debug/Release builds.
- **Validation**: smoke tests for window lifecycle, asset loading, and scene serialization; baseline performance capture.

### Phase 2 – Tooling & Workflow
- **Debug Tools**: mature the ImGui scene editor, ensure docking layouts and gizmos cover common authoring tasks, surface asset metadata from the new database.
- **Content Browser**: support folder navigation, previews, tagging, and drag/drop assignment with automatic refresh on new files.
- **Authoring Pipelines**:
  - JSON/YAML schemas for quests, NPC schedules, vehicle specs, and building definitions, validated pre-runtime.
  - CLI utilities to regenerate manifests/cache files when asset folders change.
- **Hot Reload & Undo**: extend hot reload coverage (materials, prefabs, scripts) and add undo/redo stacks for scene edits.
- **Documentation Assets**: produce designer-focused guides for asset import, quest scripting, and debug tooling usage.

### Phase 3 – Core Gameplay Loop
- **Simulation Systems**: implement time-of-day, crop growth, animal needs, and dairy production; integrate with save/load and debug overlays.
- **Economy & UI**: create barn inventory, milk selling interface, and feedback loops that reward player progression.
- **AI & Interaction**: baseline cow behaviour trees/state machines, player tools (milking, feeding), and input-driven actions.
- **Telemetry Hooks**: emit metrics for session length, resource usage, and AI state to support future balancing.
- **Validation**: automated playtest scenarios (headless where possible) to verify daily loop stability.

### Phase 4 – World & Systems Expansion
- **NPC Quest Framework**: dialogue, quest state machines, reward pipelines; author sample quest arcs tied to farming progression.
- **Vehicles & Machinery**: implement tractors, trucks, and associated physics/animations; ensure accessibility via tooltips and customization.
- **Building Systems**: construction/upgrade loop, placement editing, resource costs, and condition/durability mechanics.
- **Content Scalability**: profiling for large save files, asynchronous streaming for distant assets, and data-driven spawn tables.
- **Designer Empowerment**: extend editors with quest graph visualization, AI behaviour tweaking, and localization placeholders.

### Phase 5 – Production Quality
- **Performance Optimization**: GPU/CPU profiling, batching strategies, physics tuning, and memory budgeting.
- **Quality Assurance Infrastructure**: regression suite spanning serialization, AI logic, economy balance, and tooling; nightly builds with automated smoke playthroughs.
- **Accessibility & UX Polish**: input remapping, UI scaling, colourblind palettes, and controller support.
- **Security & Stability**: harden file IO, sandbox scripting, and validate user-generated content.

### Phase 6 – Launch Prep & Operations
- **Packaging & Deployment**: produce installers, Steam/Xbox-style manifests, and optional mod SDK packaging.
- **Live Ops Tooling**: remote logging, crash telemetry, patch diffing, and save migration scripts.
- **Support Playbooks**: create triage docs, FAQ, and response templates; establish bug prioritization workflow.
- **Post-Launch Backlog**: DLC hooks, multiplayer experimentation, seasonal events, and community-requested features.

## Cross-Cutting Initiatives
- **Localization**: introduce string tables early, integrate pipeline with CI checks for missing translations.
- **Art & Audio Integration**: define naming conventions, compression targets, LOD expectations, and audio middleware handoff.
- **Modding Support**: plan asset sandboxing, script APIs, and packaging format; ensure tools can operate in mod mode.
- **Risk Management**: maintain risk register (tech debt, dependency updates, schedule slips) with mitigation owners.

## Next Action Checklist
- Stand up the automated asset discovery service and backfill tests for the resource database.
- Update debug tooling to consume live asset catalogs and expose folder structure.
- Define schema + validation pipeline for quests, vehicles, and buildings.
- Kick off CI tasks: asset scan verification, manifest generation, and nightly regression runs.

