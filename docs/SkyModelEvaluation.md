# Sky Model Evaluation: Hosek-Wilkie vs Precomputed LUT

Date: 2025‑11‑15  
Author: GotMilked Rendering Task Force

## Goals

1. Achieve believable daylight gradients (sunrise → noon → dusk) that respond to the new time-of-day system.
2. Keep shader cost acceptable for mid-tier GPUs and allow future volumetric/fog interactions.
3. Support tooling workflows (curve preview, manual overrides) without recompiling the renderer.

## Candidate Approaches

### 1. Hosek-Wilkie Analytic Model

| Aspect | Notes |
| ------ | ----- |
| Accuracy | Physically-based daylight with spectral parameters (turbidity, albedo). Great for outdoor scenes. |
| Runtime Cost | Requires evaluating 9 coefficients per channel and computing the directional term per pixel. GLSL implementation is light (~30 ALU) but adds trigs/exponentials. |
| Inputs | Sun direction, turbidity, ground albedo; optional atmosphere tweaks. Integrates nicely with current `SunMoonState`. |
| Tooling | Parameters are intuitive (turbidity 1–10). Easy to expose in the Celestial Debugger. |
| Dependencies | Open-source reference code available (Armand's implementation, UE4). No new external library needed. |

**Pros:** High visual fidelity, smooth transitions; minimal texture memory.  
**Cons:** Harder to extend to custom atmospheric layers without digging into the math.

### 2. Precomputed LUT (Bruneton-like)

| Aspect | Notes |
| ------ | ----- |
| Accuracy | Excellent if LUT dimensionality is high enough (height, view angle, sun angle). |
| Runtime Cost | Single texture fetch per pixel. Additional inscattering table needed for advanced fog. |
| Memory | 3D texture(s) → 8–64 MB depending on resolution/precision. |
| Tooling | Editing LUTs requires offline generator. Harder to iterate quickly. |
| Dependencies | Need offline baking pipeline (CUDA/CPU). No such tooling exists in repo yet. |

**Pros:** Extremely fast shading once baked; extensible to multiple atmospheres if we invest in tooling.  
**Cons:** Requires significant upfront tooling work; content iteration loop is slow; asset size concerns on consoles.

## Decision

For Phase 1 we will implement **Hosek-Wilkie** directly in the sky shader:

1. Integrate the public-domain Hosek implementation (RGB coefficients) into `apps/GotMilked/assets/shaders/sky_header.glsl`.
2. Feed turbidity/albedo values from `celestial.json` and/or Celestial Debugger.
3. Render the sky dome via a lightweight full-screen pass before world geometry.

Rationale:

- No new tooling requirements.
- Matches the scope of “extend tests/tooling for time-of-day” already underway.
- Keeps VRAM cost negligible.

## Follow-Up Tasks

1. **Sky Pass:** Create `SkyRenderer` (GL fullscreen triangle) that evaluates Hosek. Hook into `GameRenderer::Render` before drawing world geometry.
2. **Shader Inputs:** Add uniforms for `SunMoonState.sunDirection`, turbidity, exposure compensation.
3. **Tooling:** Extend Celestial Debugger with turbidity slider + preset buttons (Clear, Hazy, Overcast). ✅ (Celestial Debugger now edits turbidity/albedo/exposure/air density.)
4. **Unit Tests:** Validate coefficient generation on CPU versus reference values (small Catch2 test). _(TODO)_
5. **Future Evaluation:** Once weather/fog is in place, revisit LUT-based approach for advanced scattering (store this doc link in roadmap).

## Usage Tips

- **Turbidity:** Higher = hazier sky. Values between 1.8 and 6.0 cover clear → overcast.
- **Ground Albedo:** Controls the horizon tint; farmland scenes prefer warmer 0.2–0.3 values.
- **Exposure:** Scales the Reinhard tonemap inside the sky shader; synchronize with global exposure curves later.
- **Air Density:** Temporary scalar to fake thicker atmospheres; keep near 1.0 until volumetric fog arrives.
- **Use Gradient Sky:** Toggle to fall back to a cheap gradient skybox on low-end hardware; disables the Hosek pass.
- **Midday Lux / Exposure Reference / Min/Max:** Control the physical lux target for noon, the reference lux used to compute EV, and the clamp range (min/max) for the exposure multiplier — see `Game::UpdateExposure`.

Adjust these in `apps/GotMilked/assets/config/celestial.json` or via the Celestial Debugger (View → Celestial Debugger). Changes propagate live without recompiling.

## References

- Hosek-Wilkie paper & sample code: http://cgg.mff.cuni.cz/projects/SkylightModelling/
- Unreal Engine sky/atmosphere implementation (for coefficient layout inspiration).
- Eric Bruneton’s Precomputed Atmospheric Scattering for potential future LUT tooling.


