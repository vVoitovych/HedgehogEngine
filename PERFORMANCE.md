# Performance

## The rule

**No optimization is accepted without a before/after number from the benchmark.**
Run the benchmark on the current branch before the change and after it; a
performance PR must quote both numbers. Changes that don't move the relevant
number get rejected, no matter how clever they look.

## Running the benchmark

```
Binaries\windows-x86_64\Release\Editor\Editor.exe --benchmark [frames]
```

- Always **Release** — Debug numbers are meaningless and validation layers skew timings.
- Loads `Assets/Scenes/benchmark.yaml` (5×5 grid of DamagedHelmet instances,
  ~364k vertices per geometry pass, one directional light), warms up 120 frames,
  measures 600 (or `[frames]`), then logs a `FrameStats` table and exits.
- Don't touch the window while it runs; close other GPU-heavy apps.
- Run it 2–3 times and compare medians; single runs can swing a few percent.

**What the numbers are:** CPU-side timings. Per-pass rows measure command-list
*recording* cost; `InitPass` includes the fence wait + swapchain acquire and
`PresentPass` includes queue submit + present, so those two absorb most
GPU-bound waiting. GPU pass durations need a Tracy capture (below) or future
GPU timestamp queries.

## Deep profiling (Tracy)

Release builds link the [Tracy](https://github.com/wolfpld/tracy) 0.13.1 client
(`TRACY_ENABLE` + `TRACY_ON_DEMAND` — dormant until a server connects). Start
the Tracy server GUI, run the Editor, connect, and you get per-frame zones for
every render pass (`HH_PROFILE_ZONE` in `HedgehogRenderer/src/`) plus frame
marks. Add zones to new code via `Profiling/Profiler.hpp`.

## Baseline

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

When a change intentionally alters performance, re-run the benchmark and update
this table (keep the old row set; add a dated entry below it so history accumulates).
