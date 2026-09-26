# Performance

## The rule

**No optimization is accepted without a before/after number from the benchmark.**
Run the benchmark on the current branch before the change and after it; a
performance PR must quote both numbers. Changes that don't move the relevant
number get rejected, no matter how clever they look.

## Running the benchmark

```
Binaries\windows-x86_64\Release\Editor\Editor.exe --benchmark [frames] [scene.yaml]
```

- Always **Release** — Debug numbers are meaningless and validation layers skew timings.
- Loads `Assets/Scenes/benchmark.yaml` (5×5 grid of DamagedHelmet instances,
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

When a change intentionally alters performance, re-run the benchmark and update
this table (keep the old row set; add a dated entry below it so history accumulates).
