

### Phase 3 – Gameplay Integration
6. **Weather & Time Data Channel**
   - Central service exposing current weather, forecast, and time-of-day.
   - Add scripting hooks (Quest system, dialogue triggers).
   - UI readouts (HUD widgets for time/weather alerts).
7. **Simulation Links**
   - Crop growth modifiers (sunlight hours, rainfall accumulation).
   - Animal mood/behavior influenced by temperature, precipitation.
   - Save/load persistence for running forecasts and accumulators.

### Phase 4 – Performance & Tooling
8. **Shadow & Probe Budgets**
   - Tune cascaded shadow maps per time-of-day.
   - Selective updates for light probes / reflection captures (only when sky/weather shifts).
9. **Profiling & Presets**
   - Extend profiling presets targeting sunny, stormy, dusk scenarios.
   - Add CI perf gates or automated capture scripts.
10. **Authoring Tools**
    - Timeline editor for time-of-day curves.
    - Weather scenario editor with test harnesses (trigger specific events).

