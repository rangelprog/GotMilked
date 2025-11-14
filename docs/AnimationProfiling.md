# Animation Profiling & Stress Testing

This note captures the current workflow for validating the animation stack (importer → pose evaluation → GPU skinning) as well as the stress budgets we monitor when introducing new animated characters.

## 1. Stress Scene Workflow

1. Build and run the **Debug** configuration so the editor UI is available.
2. Open `View → Animation Preview`. A **Stress Tools** section now lives at the top of that window.
3. Configure the herd dimensions (columns/rows), spacing, and ground offset, then click **Spawn Cow Herd**.
   - The tool instantiates the `Cow` prefab via the prefab library, so each instance carries the `SkinnedMeshComponent`, `AnimatorComponent`, and controller.
   - Herd origin is centered around `(0, 0, 0)` to keep the cluster inside the main camera frustum.
4. Use the regular scene tools (gizmos, hierarchy) to duplicate or delete the herd as needed.

This workflow makes it trivial to stage 25, 64, or 144 animated cows without hand-placing prefabs.

## 2. Profiling Steps

### CPU (Animation System & Controllers)

- Enable the in-game profiler overlay (Tools → Tooling Overlay) and reset counters.
- Capture one minute of gameplay with the herd wandering or idling.
- The key metric is `AnimationSystem::Update`, which encompasses pose evaluation *and* palette generation.
- For deeper inspection:
  1. Build with `/PROFILE`, attach Visual Studio profiler, and capture two 5‑second traces.
  2. Filter by `AnimationPoseEvaluator::EvaluateLayers` and `AnimatorComponent::GetSkinningPalette` to spot regressions.

### GPU (Skinning UBO + Draw Calls)

- Use RenderDoc or Nsight to capture a frame while the herd is on screen.
- Inspect the `simple_skinned` shader pass:
  - Confirm `SkinnedMeshComponent` palette uploads remain ≤128 matrices.
  - Verify the herd batches into as few draws as possible (currently 1 draw per prefab instance; instancing planned later).
- Record GPU timings from the graphics debugger or from the in-game overlay if supported by the driver (on NVIDIA, turn on Aftermath metrics).

## 3. Current Budgets & Observations

Measurements were taken on a Ryzen 7 5800X + RTX 3070 @1080p, using the new herd spawner.

| Scenario | Count | CPU Animation (ms) | GPU Skinning (ms) | Notes |
|----------|-------|--------------------|-------------------|-------|
| Light Herd | 25 cows (5x5) | 0.8 – 1.0 | 1.5 – 1.8 | Fits within budget for story scenes. |
| Target Herd | 64 cows (8x8) | 1.9 – 2.3 | 3.2 – 3.6 | Default QA perf test. |
| Stress Herd | 144 cows (12x12) | 4.5 – 4.9 | 6.8 – 7.5 | Upper bound before LOD/impostors kick in. |

**Budgets**

- CPU: keep AnimationSystem ≤3 ms for the mainline herd (64 cows). Exceeding this triggers warnings in the profiler overlay.
- GPU: reserve ≤4 ms for skinning + shading of animated actors; beyond that we start culling or swapping to impostors.

## 4. Follow-up

- Add automation to record CPU/GPU timings after nightly builds using the herd spawner presets (32 / 64 / 128 cows) to catch regressions.
- Extend the stress tool with presets for NPCs once their prefabs adopt the animation pipeline.

