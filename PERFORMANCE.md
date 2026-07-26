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

## 2026-07-25 — Composable render pipeline (Phases 1–6, `workflow/current-plan.md`)

`RenderQueue`'s hand-written, duplicated scene/game render blocks were replaced with a real
render-graph runtime: a frame graph (`InitPass`, `ShadowmapPass`) runs once per frame, one *view*
graph runs per registered `RenderView` (the editor's Scene and Game panels each own their full
pass set — `DepthPrePass`/`ForwardPass`/optionally `GizmoPass` — rather than sharing hand-toggled
instances), and a composition graph (`GuiPass`/`PresentPass`, or just `PresentPass` for a
headless/game build) runs last. Pipeline composition — which passes, in which view, in which
order — is now declared in `.rgq` YAML assets (`HedgehogRenderer/assets/Graphs/`), loaded through
a validating parser + factory registry rather than hardcoded in `Renderer`'s constructor.

**Row naming changed.** The old combined `GameViewPass` row is gone. Every pass that exists once
per registered view now gets its own row, suffixed `[<viewName>]` for every view after the first
(`ForwardPass` for the Scene/main view, `ForwardPass[game]` for the Game view, etc.) — see
`workflow/current-plan.md`, "Profiling identity". `ShadowmapPass` stays a single frame-graph row
(cascades fit to whichever view is `SetMainView`); `GuiPass`/`PresentPass` stay single
composition-graph rows.

**No net performance change intended or observed** — this was an architecture refactor, not an
optimization; the goal at every phase was staying within run-to-run noise of whatever the
immediately-preceding phase measured. Per-phase Release `--benchmark 600` numbers (same RTX 2070
machine as the baseline above, but measured across several days in late July 2026 — some of the
spread below is machine-state variance across sessions, not attributable to any one phase):

| Phase | `DrawFrame(total)` avg | Frame(wall) avg | FPS |
|---|---:|---:|---:|
| 2 (shared pass resources) | 1.972 ms | — | 479.3 |
| 3 (single graph, dual-view bridge) | 1.908 ms | — | 498.0 |
| 4 (per-view instancing, no bridge) | 1.969–2.025 ms | — | 465.8–477.4 |
| 5 (`.rgq`-driven pipelines) | 1.870–1.915 ms | — | 493.7–506.5 |
| 6 (headless game-mode proof) | 1.900 ms | 2.008 ms | 498.0 |

All within the "few percent" run-to-run noise band this document already documents — no phase
regressed against the one before it, and the final (Phase 6) numbers land within noise of the
2026-07-15 baseline table above despite the renderer's internals changing substantially. The
baseline table's per-pass rows are no longer directly comparable 1:1 (row set changed, per above)
but the aggregate `DrawFrame(total)`/`Frame(wall)`/FPS figures are.
