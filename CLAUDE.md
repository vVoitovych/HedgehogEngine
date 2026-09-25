# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Development Workflow

**Every change starts as a Jira ticket in project `HE` and lands as one pull request under ~1000 changed lines.** Read `WORKFLOW.md` before starting any work — it defines the ticket model, the sizing rule, branch and commit conventions, and the Jira/GitHub automation.

The short form:
- Branch `HE-<n>-<slug>` off `master`; no long-lived integration branches.
- Commit subjects and PR titles start `HE-<n>: `. A `commit-msg` hook and the `ticket-check` CI job enforce this; Jira uses it to move the ticket to **In Review** on PR creation and **Done** on merge.
- Break a problem into tickets with `/jira-tickets <problem>` before writing code.

## Build System

HedgehogEngine uses **Premake5** to generate Visual Studio 2022 solution files. There is no CMake.

**Initial setup** (run once, or after submodule changes):
```bat
Scripts\SetupWindows.bat
```
This initializes git submodules recursively and generates `HedgehogEngine.sln`.

**Building (CLI — always build after code changes, a change is not done until it compiles):**
```
Scripts\Build.bat [Debug|Release]        (default: Debug; locates MSBuild via vswhere)
```
**Build + run all tests in one step:**
```
Scripts\RunTests.bat [Debug|Release]     (exits nonzero if the build or any test fails)
```
Alternatively invoke MSBuild directly (`MSBuild.exe HedgehogEngine.sln /p:Configuration=Debug /p:Platform=x64 /m /v:m`) or open `HedgehogEngine.sln` in Visual Studio 2022. Configurations: `Debug` and `Release`, platform: `x64`.

**Shader compilation** is automatic via a Shaders project pre-build command that calls `ThirdParty/glslc/CompileShaders.bat`, compiling `.vert`/`.frag`/`.comp` GLSL sources to SPIR-V (`.spv`).

**Build output:** `Binaries/windows-x86_64/[Debug|Release]/[ProjectName]/`

**Running tests** (doctest; each exe exits nonzero on failure):
```
Binaries/windows-x86_64/Debug/HedgehogMathTest/HedgehogMathTest.exe
Binaries/windows-x86_64/Debug/FileSystemTest/FileSystemTest.exe
Binaries/windows-x86_64/Debug/ECSTest/ECSTest.exe
Binaries/windows-x86_64/Debug/EcsSerializationTest/EcsSerializationTest.exe
Binaries/windows-x86_64/Debug/ContentLoaderTest/ContentLoaderTest.exe
Binaries/windows-x86_64/Debug/HedgehogExtractTest/HedgehogExtractTest.exe
Binaries/windows-x86_64/Debug/RenderGraphTest/RenderGraphTest.exe
```

**Renderer smoke test** — after any renderer/RHI change, run (from the repo root, needs a Vulkan GPU):
```
Binaries\windows-x86_64\Debug\Editor\Editor.exe --smoke-test [frames]
```
Renders N frames (default 120) and exits nonzero if any Vulkan validation error occurred (Debug builds enable `VK_LAYER_KHRONOS_validation`; messages route through Logger, counters live in `RHI/api/RHIDiagnostics.hpp` and are re-exported via `Renderer.hpp`).

**Game mode** — the render-graph path as a game build runs it, with no editor:
```
Binaries\windows-x86_64\Debug\Editor\Editor.exe --game-mode [frames]
```
Loads `Assets/Scenes/Default.yaml`, builds a `Renderer` with `RendererPaths::RenderGraphOnly` (no legacy pass, so no ImGui), forces `rendering.use_render_graph` on for the run without saving it, and renders N frames (default 120) with `RenderFrame` from views derived from the scene's camera components. Exits nonzero on any Vulkan validation error **or if an ImGui context ever exists** — the renderer not depending on ImGui is checked, not assumed. Run it after any render-graph or renderer change, alongside `--smoke-test`; like it, it needs a GPU and runs locally only.

**Regenerating solution** after modifying any `Build.lua` files:
```
Vendor\Binaries\Premake\Windows\premake5.exe --file=Build.lua vs2022
```

**CI:** `.github/workflows/ticket-check.yml` fails any PR whose title carries no `HE-<number>` key. There is currently **no build workflow** — Debug/Release builds and the test exes must be run locally before opening a PR (the smoke test is local-only in any case: CI runners have no Vulkan GPU).

## Performance

- **Frame budget: 16.6 ms (60 FPS) in Release** at default window size on the baseline machine. Weigh this in any plan touching the frame loop, render passes, ECS iteration, or per-frame allocations.
- **No optimization without a before/after number.** Measure with:
  ```
  Binaries\windows-x86_64\Release\Editor\Editor.exe --benchmark [frames]
  ```
  It loads `Assets/Scenes/benchmark.yaml`, warms up 120 frames, measures 600, and logs per-pass CPU timings (avg/min/max/p95) plus wall frame time and FPS. Always Release; run before and after the change and quote both numbers. Baseline table and methodology: `PERFORMANCE.md`.
- Per-pass rows are CPU record times; GPU-bound waiting shows up in `InitPass` (fence/acquire) and `PresentPass` (submit/present). For GPU-side detail, connect the Tracy 0.13.1 server (client is linked in Release, `TRACY_ON_DEMAND`); zones via `HH_PROFILE_ZONE` in `HedgehogRenderer/src/Profiling/Profiler.hpp`.

## Architecture

The engine is a set of C++20 libraries (mix of static and shared) with an `Editor` executable as the entry point. Dependencies flow strictly upward:

```
Editor (ConsoleApp)
  └── HedgehogEngine + HedgehogRenderer + HedgehogWindow + HedgehogSettings + Logger
        ├── HedgehogEngine  (DLL) → HedgehogCommon, HedgehogSettings, HedgehogWindow,
        │                           ContentLoader, ECS, EcsSerialization, yaml-cpp, ImGui
        ├── HedgehogRenderer (static lib) → RHI, HedgehogEngine, HedgehogCommon,
        │                                   HedgehogSettings, HedgehogWindow,
        │                                   HedgehogMath, ContentLoader, Shaders, imgui
        ├── HedgehogWindow  (DLL) → HedgehogMath, GLFW, Vulkan
        ├── HedgehogSettings (DLL) → yaml-cpp
        ├── HedgehogCommon  (DLL) → HedgehogMath
        └── RHI             (static lib) → Vulkan (Volk + VMA)
```

**The renderer is being rewritten.** `RENDERING.md` at the repository root is the design: a camera-and-view architecture where a `CameraComponent` carries scene data, a `View` is the render request, and each view owns a render graph. It describes the *target*, not what is on master today — the structures below (the six fixed render passes, `ResourceManager`, `RenderQueue`) are what exists now. Epic [HE-63](https://viktoravoitovych.atlassian.net/browse/HE-63) lands it as 28 sub-1000-line pull requests.

### Key Modules

| Module | Type | Role |
|--------|------|------|
| `HedgehogMath` | DLL | Vectors, matrices, AABB/OBB/Plane/Frustum primitives |
| `HedgehogCommon` | DLL | Shared renderer constants (`MAX_FRAMES_IN_FLIGHT`, `MAX_LIGHTS_COUNT`, …), Camera |
| `HedgehogWindow` | DLL | GLFW window wrapper, input handling (namespace `HW`) |
| `HedgehogSettings` | DLL | YAML-based engine configuration |
| `HedgehogEngine` | DLL | Engine/Frame/Thread context; resource containers (DrawList, Light, Material, Mesh, Texture); ECS integration |
| `RHI` | static lib | Graphics abstraction: `IRHIDevice`, `IRHICommandList`, `IRHITexture`, … — Vulkan backend under `src/Vulkan/` |
| `HedgehogRenderer` | static lib | Multi-pass Vulkan renderer (see structure and passes below) |
| `ECS` | static lib | Entity Component System (EntityManager, ComponentManager, SystemManager, Coordinator) |
| `EcsSerialization` | DLL | ECS serialization; `IHierarchyProvider` interface decoupled from engine |
| `HedgehogExtract` | static lib | `RenderScene` + `SceneExtractor` (namespace `HX`): reads the ECS read-only, produces the immutable per-frame scene every view will consume (RENDERING.md section 3.1) |
| `ContentLoader` | static lib | glTF/glb, OBJ, and texture loading (stb_image) |
| `DialogueWindows` | static lib | ImGui-based dialogs for materials, meshes, scenes, textures |
| `Logger` | static lib | Colorized console logging, no dependencies |
| `Editor` | executable | H-form 5-panel editor; Play/Pause/Stop mode; ConsolePanel captures Logger |

### HedgehogRenderer Structure

The renderer follows a strict `api` / `src` split:

```
HedgehogRenderer/
├── api/
│   └── HedgehogRenderer/
│       └── Renderer.hpp          ← sole public header
└── src/
    ├── Renderer/Renderer.cpp
    ├── RHIContext/               ← owns IRHIDevice + IRHISwapchain
    ├── ThreadContext/            ← per-frame command lists, fences, semaphores
    ├── ResourceManager/          ← GPU textures (color, depth, shadow map, scene)
    ├── ResourceRegistry/         ← mesh/material GPU buffers and descriptor sets
    └── RenderPasses/
        ├── InitPass/
        ├── DepthPrepass/
        ├── ShadowmapPass/
        ├── ForwardPass/
        ├── GuiPass/
        └── PresentPass/
```

`api/` is the public include root (added to dependents' include paths).  
`src/` is private — never included from outside the module.

### Render Passes (HedgehogRenderer)

Forward-rendering pipeline executed in order:
1. **InitPass** — acquires the next swapchain image
2. **DepthPrepass** — early-Z depth pass into scene depth buffer
3. **ShadowmapPass** — directional shadow map generation
4. **ForwardPass** — main lit geometry pass into scene color buffer
5. **GuiPass** — runs the application's `UiCallback` (the Editor's ImGui) into the RHI color buffer
6. **PresentPass** — blits scene + GUI to swapchain and submits

**The render-graph path** runs instead when `rendering.use_render_graph: true` is set in `engine_settings.yaml` (`HedgehogSettings::RenderingSettings`, default off): the Editor extracts a `RenderScene` and calls `Renderer::RenderFrame`, which owns the frame lifecycle inline (fence wait, resize before acquire, shared phase, per-view graphs, one execute, present) in `src/Renderer/FrameRenderer`. The editor has three views (RENDERING.md section 7): **scene** (the flycam into the `scene` target), **game** (the scene's camera, redirected to the `game` target with `SetCameraTargetOverride`), and **result** (no camera; the `result` graph's `Ui` pass draws the editor into `main`, sampling both panel targets). A hidden panel tab is sized 0x0, so its view is dropped and its passes never declared (`GetLastFramePassCount`). Both paths are always built. Run `--smoke-test` with the flag both off and on after renderer changes.

**ImGui belongs to the Editor, never the renderer.** `Editor/ImGuiLayer` owns the context, the GLFW platform backend and the RHI GUI backend (`IRHIGuiBackend`, dynamic rendering into any texture of one colour format, created with `Renderer::CreateGuiBackend`, rebuilt when the path changes). Both paths hand the editor a colour target through a `UiCallback`. `HedgehogRenderer` has no ImGui include path or link, so it cannot include an ImGui header.

### Project Configuration Files

Each module has its own `Build-[ModuleName].lua` file included from the root `Build.lua`. Global third-party paths and library names are centralized in `Dependencies.lua`. Modifying either requires regenerating the solution.

### Testing

Unit tests use the **doctest** framework. Test projects (each `<Module>/tests/` with its own `Build-<Module>Test.lua`):
- `HedgehogMathTest` — vectors, matrices; `NearlyEqual` helpers with configurable epsilon
- `FileSystemTest` — virtual file system and mounts; provides the `TempDir` RAII helper (`FileSystem/tests/test_helpers.hpp`), reused by other test projects
- `ECSTest` — entity lifecycle, component storage integrity, system signature membership
- `EcsSerializationTest` — scene YAML round-trip plus failure paths (missing/corrupt files)
- `ContentLoaderTest` — OBJ mesh loading with hermetic temp-dir fixtures
- `HedgehogExtractTest` — ECS-to-`RenderScene` extraction: instance/light/camera extraction, world-bounds correctness, `SourceId` round-trip, layer propagation, zero-allocation re-extraction
- `RenderGraphTest` (`HedgehogEngine/HedgehogRenderer/tests/`) — the whole render graph (namespace `Renderer`), run at runtime only by `Renderer::RenderFrame` behind the settings flag: declaration (`RGTypes`/`GraphDescription`/`GraphBuilder`/`RGPassBuilder` — handle versioning, dependency-edge recording, size-policy resolution), compilation (`GraphCompiler` — culling, topological sort, barrier derivation, all four validation failure modes), and execution (`FrameArena`, `ResourcePool`, `RenderGraphRuntime` — pass-data/closure arena allocation, descriptor-hash pool reuse, persistent-resource identity, relative sizes resolved against `SetSizeReferences`), graph assets (`GraphAsset`/`GraphAssetVocabulary`/`GraphAssetParser` — one-to-one vocabulary, strict rejection of unknown strings, malformed-document messages that name the offender), and instantiation (`PassBuilderRegistry`/`PassInvocation`/`GraphInstantiator` — semantic validation naming pass and slot, the view output contract, outputs written into caller-supplied target textures, and the **C++ equivalence oracle**: `GraphPlanOracle.hpp` renders a compiled plan as name-keyed text so an asset-instantiated graph and its hand-written twin must compile identically), and the shipped graphs (`assets/Graphs/scene|game|result.graph` against `RegisterEnginePassTypes`, each with a hand-written twin, plus `GraphAssetLibrary` hot reload and last-known-good fallback on a broken edit), and the render-target registry (`Targets/RenderTargetRegistry` — `main` and declared targets, all three size policies across a swapchain resize, end-of-frame resize with fence-deferred destruction, zero-area reporting), and views (`Views/ViewManager` — derivation from `RenderScene` cameras with stable ids, application views untouched by reconciliation, target overrides that leave the source camera byte-identical, build-time drops), and view ordering (`Views/ViewOrdering` — writer-before-reader order from render-target dependencies, `Priority` only as a tie-break, cycles reported naming every view and resolved by dropping one), and engine pass recording (`EnginePassTypes` depth prepass, shadow, forward and Ui execute bodies — the Ui pass runs the frame's `UiCallback` after sampling the targets its view reads, or clears without one — driven through a fake `IGraphPassServices` and the counting `RecordingCommandList`; `ShadowCascades` math; `MakeForwardViewUniform`/`MakeSceneLightsUniform` packing and std140 layout), and the shared frame phase (`Frame/SharedPhase` — the shadow atlas declared once and imported by any number of view graphs through schema-v2 `imports`, ordered and barriered by the compiler, culled when no view reads it; casters chosen by `ShadowCasterMask`, never the view's layer mask; scene lights uploaded once per frame; `SelectShadowView`). The real `GraphPassServices` (pipelines + per-frame viewProj, forward-view and scene-light uniform rings) lives in `src/GraphPasses/`, outside what the test compiles. Links `HedgehogMath` (a DLL, copied in by its post-build step) for `RenderScene`. Compiles the graph's sources directly rather than linking the `HedgehogRenderer` static lib and links only `Logger` and the static `yaml-cpp` beyond that, so it needs no Vulkan SDK, GLFW, or ImGui — a fake `IRHIDevice`/`IRHICommandList` pair in `TestRHIDoubles.hpp` (zero Vulkan dependency, since those interfaces have none of their own) stands in for a real device.

DLL test dependencies are copied to each test's output dir by the owning module's `postbuildcommands` — when adding a test project that links a `SharedLib` module, add a MKDIR/COPY pair to that module's `Build-*.lua`.

Everything else in the renderer (the RHI-backed render passes) has no unit tests by design; it is covered by validation layers + `Editor.exe --smoke-test` (see Build System). The render graph's declaration layer above is the deliberate exception — it is device-free by construction (RENDERING.md section 5.3), so it is unit-tested like any other module.

### Third-Party Dependencies (git submodules)

glfw, ImGui, yaml-cpp, tinygltf, doctest, Lua — all under `ThirdParty/`. Vulkan SDK headers/libs are also under `ThirdParty/vulkan/`.
