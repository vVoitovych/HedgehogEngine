# Performance

## The rule

**No optimization is accepted without a before/after number from the benchmark.**
Run the benchmark on the current branch before the change and after it; a
performance PR must quote both numbers. Changes that don't move the relevant
number get rejected, no matter how clever they look.

## Running the benchmark

```
Binaries\windows-x86_64\Release\Editor.exe --benchmark [frames] [scene.yaml]
```

- Always **Release** — Debug numbers are meaningless and validation layers skew timings.
- Loads `Assets/Scenes/benchmark.yaml` of the sample project, `Projects/FeatureTest` (5×5 grid of DamagedHelmet instances,
  ~364k vertices per geometry pass, one directional light), warms up 120 frames,
  measures 600 (or `[frames]`), then logs a `FrameStats` table and exits. Pass a
  file name under `Assets/Scenes/` (e.g. `Default.yaml`) to measure another scene.
- Don't touch the window while it runs; close other GPU-heavy apps.
- Run it 2–3 times and compare medians; single runs can swing a few percent.

**What the numbers are:** CPU-side timings. Since HE-87 every frame renders
through the render graph: per-pass rows (`Shadow`, `DepthPrepass`, `Forward`,
`Gizmo`, `Ui`) measure each graph pass's command-list *recording* cost (its
barriers plus its closure), summed over the views that ran it, and
`RenderFrame(total)` includes the fence wait, swapchain acquire, queue submit
and present, so it absorbs most GPU-bound waiting. (Entries before HE-87 are the
legacy path: `InitPass`/`PresentPass` held the waiting there.) GPU pass durations need a Tracy capture (below) or future
GPU timestamp queries.

## Deep profiling (Tracy)

Release builds link the [Tracy](https://github.com/wolfpld/tracy) 0.13.1 client
(`TRACY_ENABLE` + `TRACY_ON_DEMAND` — dormant until a server connects). Start
the Tracy server GUI, run the Editor, connect, and you get per-frame zones for
every render pass (`HH_PROFILE_ZONE` in `HedgehogRenderer/src/`) plus frame
marks. Add zones to new code via `Profiling/Profiler.hpp`.

## Baseline

### INVALID — 2026-07-15 (HE-68)

The table below measured an **empty scene**: every entity in `benchmark.yaml` had a
`MeshComponent` but no `RenderComponent`, so `RenderSystem` never tracked them and the draw
list was always zero objects, regardless of what the "5×5 grid of DamagedHelmet instances"
description claimed. Every number in it is a no-op frame's cost (mostly `PresentPass`
fence/acquire/present overhead), not a measurement of anything rendering. Do not use it for
before/after comparisons. Kept for history, per HE-68, rather than deleted.

Recorded 2026-07-15, commit branch `architecture_improvement`.
Machine: NVIDIA GeForce RTX 2070, Windows 11, Release x64, default window size.

| Zone             | avg ms | min ms | max ms | p95 ms |
|------------------|-------:|-------:|-------:|-------:|
| InitPass         |  0.152 |  0.007 | 13.103 |  0.779 |
| ShadowmapPass    |  0.040 |  0.009 |  3.530 |  0.078 |
| DepthPrePass     |  0.009 |  0.004 |  0.063 |  0.018 |
| ForwardPass      |  0.012 |  0.005 |  0.095 |  0.024 |
| GuiPass          |  0.035 |  0.011 |  0.202 |  0.078 |
| PresentPass      |  2.002 |  0.218 |  6.287 |  3.607 |
| DrawFrame(total) |  2.263 |  0.260 | 15.796 |  4.069 |
| **Frame(wall)**  | **2.418** | 0.321 | 15.958 |  4.325 |

Average: **413.6 FPS**. Frame budget: **16.6 ms (60 FPS)** — currently ~7× headroom.

### 2026-09-21 (HE-68) — first valid baseline

`benchmark.yaml`'s 25 helmet entities now carry a `RenderComponent` (`Materials\test2.material`,
already authored for `DamagedHelmet`'s texture), so this is the first baseline recorded against
a scene that actually draws something. `--benchmark` reports **draw count = 25** (one `DrawObject`
per helmet, all in a single opaque `DrawNode`).

Recorded 2026-09-21, commit branch `HE-68-repair-benchmark-yaml`, median of 3 runs (600 frames
each, 120-frame warmup — see the raw runs below the table).
Machine: NVIDIA GeForce RTX 2070, Windows 11, Release x64, default window size.

| Zone             | avg ms | min ms | max ms | p95 ms |
|------------------|-------:|-------:|-------:|-------:|
| InitPass         |  0.235 |  0.012 |  7.147 |  0.566 |
| ShadowmapPass    |  0.047 |  0.019 |  0.528 |  0.120 |
| DepthPrePass     |  0.012 |  0.006 |  0.079 |  0.027 |
| ForwardPass      |  0.016 |  0.008 |  0.098 |  0.038 |
| GuiPass          |  0.043 |  0.014 |  1.958 |  0.093 |
| PresentPass      |  3.035 |  0.409 | 14.865 | 10.136 |
| DrawFrame(total) |  3.401 |  0.523 | 15.420 | 10.291 |
| **Frame(wall)**  | **3.619** | 0.677 | 15.894 | 10.497 |

Average: **276.3 FPS**. Frame budget: **16.6 ms (60 FPS)** — currently ~4.6× headroom.

Raw `Frame(wall)` avg across the 3 runs (median row used above): 3.819 ms, **3.619 ms**, 2.683 ms
— run-to-run swing is large relative to the sub-4ms frame cost at this scale (25 instances is
still far below the frame budget), consistent with the doc's own "single runs can swing a few
percent" note, just more visible when the absolute numbers are this small.

### 2026-09-26 (HE-87) — the editor renders through the render graph

The editor now always renders with `Renderer::RenderFrame` (scene view with the Scene
tab visible, plus the result view's UI; the hidden game panel costs nothing). Before =
master's legacy `DrawFrame`, after = this change; runs interleaved, 600 frames each.
The machine was noisier than for the entry above, so compare within this entry only.
`--benchmark` now reports **render instances = 25** for `benchmark.yaml`.

| Scene            | Before, `Frame(wall)` avg ms (runs) | After, `Frame(wall)` avg ms (runs) | Median before → after |
|------------------|-------------------------------------|------------------------------------|----------------------:|
| `benchmark.yaml` | 2.555, 2.345, 2.248                 | 2.670, 2.259, 1.889                | 2.345 → 2.259 |
| `Default.yaml`   | 2.256, 2.035, 2.223, 2.230, 2.217   | 2.415, 2.000, 1.722, 2.212, 1.865, 2.220 | 2.223 → 2.106 |

Two `Default.yaml` runs of the legacy build and one of the new build crashed during
warm-up, right after the scene's Lua script starts: a pre-existing crash, not a
rendering one.

Graph-path per-pass rows (`benchmark.yaml`, one run): `Shadow` 0.051, `DepthPrepass`
0.010, `Forward` 0.015, `Gizmo` 0.000 (nothing selected), `Ui` 0.032,
`RenderFrame(total)` 1.680 ms avg.

### 2026-10-05 (HE-262) — MAX_ENTITIES 512 → 4096, MAX_COMPONENTS 16 → 64

Every registered component type keeps a fixed array of `MAX_ENTITIES` components and every
entity a `MAX_COMPONENTS`-bit signature, so this change costs memory, not frame time. Before =
master at cc688f7, after = this change; Release, 600 frames, 3 runs each, same machine (RTX 2070),
runs not interleaved. `RenderFrame(total)` and `Frame(wall)` are averages per run, in ms.

| Scene            | `RenderFrame(total)` before (runs) | after (runs)         | Median before → after |
|------------------|------------------------------------|----------------------|----------------------:|
| `benchmark.yaml` | 1.943, 1.968, 1.936                | 1.897, 1.941, 1.760  | 1.943 → 1.897 |
| `Default.yaml`   | 1.950, 1.944, 1.947                | 1.879, 1.930, 1.973  | 1.947 → 1.930 |

| Scene            | `Frame(wall)` before (runs) | after (runs)        | Median before → after |
|------------------|-----------------------------|---------------------|----------------------:|
| `benchmark.yaml` | 2.404, 2.407, 2.374         | 2.317, 2.400, 2.152 | 2.404 → 2.317 |
| `Default.yaml`   | 2.285, 2.301, 2.280         | 2.211, 2.304, 2.309 | 2.285 → 2.304 |

No frame-time change beyond run-to-run noise (the per-frame paths never loop over
`MAX_ENTITIES`). Memory and test time do grow:

- `Game.exe --frames 300` peak working set (3 runs): 339.5, 339.6, 340.4 MB → 343.7, 342.8,
  344.0 MB, about **+4 MB** for the engine's 16 component arrays at 4096 entries.
- `Scripts\RunTests.bat Debug` wall time, build already up to date: **29 s → 41 s**. Every
  test that builds an `EngineContext` now allocates the larger arrays, and a few test helpers
  scan every id up to `MAX_ENTITIES`.

### 2026-10-09 (HE-325) — HDR target and tone mapping

Every scene and game view now renders Forward into a transient `R16G16B16A16Float` `hdr`
target and adds a `ToneMap` pass: one fullscreen triangle sampling it into the view's colour
output. That is one more pass, one more full-size render target and one more full-screen read
and write per visible view. Before = master at 583464a, after = this change; Release,
`benchmark.yaml`, 600 frames, 3 runs each, same machine (RTX 2070), same session, not
interleaved. Averages per run, in ms.

| Row                  | Before (runs)       | After (runs)        | Median before → after |
|----------------------|---------------------|---------------------|----------------------:|
| `RenderFrame(total)` | 1.876, 1.921, 1.933 | 2.041, 1.987, 1.991 | 1.921 → 1.991 |
| `Frame(wall)`        | 2.100, 2.152, 2.178 | 2.324, 2.263, 2.263 | 2.152 → 2.263 |

The `ToneMap` pass records in 0.009-0.010 ms of CPU; the rest of the ~0.1 ms is GPU time
showing up in `RenderFrame(total)`'s fence wait (the fastest frames went from 0.49-0.52 to
0.57-0.58 ms of wall time). Well inside the 16.6 ms budget; the cost scales with the views'
pixel counts, not the scene.

### 2026-10-09 (HE-326) — Cook-Torrance PBR shading and the tangent stream

The forward shader now evaluates a GGX specular lobe per light and, for normal-mapped materials,
a tangent basis; every vertex gains a 16-byte tangent stream. Before = master at 6a5a9b1, after =
this change; Release, `benchmark.yaml`, 600 frames, 3 runs each, same machine (RTX 2070), same
session, not interleaved. Averages per run, in ms.

| Row                  | Before (runs)       | After (runs)        | Median before → after |
|----------------------|---------------------|---------------------|----------------------:|
| `RenderFrame(total)` | 1.891, 1.961, 1.904 | 1.930, 1.935, 1.896 | 1.904 → 1.930 |
| `Frame(wall)`        | 2.116, 2.206, 2.152 | 2.193, 2.196, 2.142 | 2.152 → 2.193 |

No change beyond run-to-run noise: `benchmark.yaml`'s materials have no maps, so the normal-map
path is skipped, and its few lights keep the per-pixel cost small. The `Forward` pass records in
0.012-0.014 ms either way.

### 2026-10-09 (HE-328) — PCF sun shadows in the forward pass

Every forward pass now binds the shadow set and, for the sun, picks a cascade and averages 9
comparison taps at the default PCF radius of 1 (2x2-filtered each). Before = master at 7d2024e,
after = this change; Release, 600 frames, 3 runs each, same machine (RTX 2070), same session, not
interleaved. `benchmark.yaml` has no shadow-casting light (only the set binding is new);
`Physics.yaml`'s sun casts. `Frame(wall)` averages per run, in ms.

| Scene            | Before (runs)       | After (runs)        | Median before → after |
|------------------|---------------------|---------------------|----------------------:|
| `benchmark.yaml` | 2.129, 2.155, 2.171 | 2.203, 2.192, 2.212 | 2.155 → 2.203 |
| `Physics.yaml`   | 1.954, 2.032, 2.046 | 2.053, 2.023, 2.036 | 2.032 → 2.036 |

At most about 0.05 ms, near run-to-run noise. The `Shadow` pass records in 0.032-0.040 ms and
`Forward` in 0.014-0.016 ms, as before: the shadow pass already ran every frame.

### 2026-10-09 (HE-331) — Image-based lighting from the scene's environment

Every forward pass now binds the environment (its uniform, the prefiltered radiance cube and the
BRDF table) in set 3 and adds the split-sum specular and SH diffuse per pixel; a scene without an
environment binds a black 1x1 cube. Before = master at c3eefa6, after = this change; Release, 600
frames, 3 runs each, same machine (RTX 2070), same session, not interleaved. The third scene is
`Physics.yaml` with an `EnvironmentComponent` on a procedural 256x128 sky (a check scene, not
committed). `Frame(wall)` averages per run, in ms.

| Scene                   | Before (runs)       | After (runs)        | Median before → after |
|-------------------------|---------------------|---------------------|----------------------:|
| `benchmark.yaml`        | 2.270, 2.222, 2.254 | 2.319, 2.250, 2.249 | 2.254 → 2.250 |
| `Physics.yaml`          | 2.130, 2.198, 2.236 | 2.055, 2.099, 2.104 | 2.198 → 2.099 |
| Physics + environment   | 2.176, 2.063, 2.118 | 2.114, 2.165, 2.098 | 2.118 → 2.114 |

No change beyond run-to-run noise. The environment is baked once per map, at its first frame, on
the main thread: that sky took 179 ms in Release (1.7 s in Debug), logged as
`[Environment] Baked <path> ... in <n> ms.`; the BRDF table (64x64, 256 samples) is computed once
when the renderer starts.

### 2026-10-09 (HE-332) — The skybox

A `Skybox` pass now runs in every scene and game view, between `Forward` and `ToneMap`; it draws one
fullscreen triangle only when the scene has a baked environment that shows its skybox. Before =
master at 130e78b, after = this change; Release, 600 frames, 3 runs each, same machine (RTX 2070),
same session, not interleaved. The third scene is `Physics.yaml` with an `EnvironmentComponent` on
a procedural sky (a check scene, not committed), the only one that draws the sky. `Frame(wall)`
averages per run, in ms.

| Scene                   | Before (runs)       | After (runs)        | Median before → after |
|-------------------------|---------------------|---------------------|----------------------:|
| `benchmark.yaml`        | 2.192, 2.185, 2.377 | 2.173, 2.199, 2.205 | 2.192 → 2.199 |
| `Physics.yaml`          | 2.058, 2.052, 2.120 | 2.040, 2.086, 2.080 | 2.058 → 2.080 |
| Physics + environment   | 2.121, 2.158, 2.226 | 2.067, 2.071, 2.112 | 2.158 → 2.071 |

No change beyond run-to-run noise; the `Skybox` pass records in 0.007 ms with the sky drawn.

### 2026-10-09 (HE-336) — baseline after the PBR epic

The end of the PBR rendering epic (HE-319): HDR with ACES tone mapping, Cook-Torrance shading,
PCF sun shadows, image-based lighting and the skybox. `benchmark.yaml`'s 25 helmets now draw with
their imported material (`DamagedHelmet_Material_MR.material`: all five maps, emissive) instead of
`test2.material` (base colour only), and `Pbr.yaml` is the new scene that uses every feature at
once (ten spheres, the helmet, a shadowing sun, a 1K `.hdr` environment and its skybox). Release,
600 frames after a 120-frame warmup, 3 runs each, same machine (NVIDIA GeForce RTX 2070, Windows
11), same session, default window size. Average and p95 per run, in ms.

| Scene | `Frame(wall)` avg (runs) | `Frame(wall)` p95 (runs) | `RenderFrame(total)` avg (runs) |
|---|---|---|---|
| `benchmark.yaml`, `test2.material` (before) | 2.092, 2.093, 2.204 | 2.931, 2.797, 3.565 | 1.879, 1.889, 1.985 |
| `benchmark.yaml`, imported material (after) | 2.249, 2.154, 2.156 | 3.498, 2.933, 3.595 | 1.998, 1.915, 1.921 |
| `Pbr.yaml` | 2.010, 1.957, 1.979 | 2.994, 2.840, 2.553 | 1.826, 1.776, 1.797 |

Medians: `benchmark.yaml` 2.093 → 2.156 ms (all five maps sampled and the emissive added, about
+0.06 ms, near run-to-run noise) and `Pbr.yaml` 1.979 ms, about 505 FPS: **about 8× headroom
under the 16.6 ms budget**. Per-pass recording on `Pbr.yaml` (avg): Shadow 0.034, DepthPrepass
0.008, Forward 0.017, Skybox 0.006, ToneMap 0.006, Ui 0.024 ms; the rest of `RenderFrame(total)`
is the fence wait, acquire, submit and present. The environment's first-frame bake is not in
these numbers (the warmup absorbs it): 1.8 s in Debug for the 1024x512 sky at face 256.

When a change intentionally alters performance, re-run the benchmark and update
this table (keep the old row set; add a dated entry below it so history accumulates).
