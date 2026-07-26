# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

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
Binaries/windows-x86_64/Debug/RenderGraphTest/RenderGraphTest.exe
```

**Renderer smoke test** — after any renderer/RHI change, run (from the repo root, needs a Vulkan GPU):
```
Binaries\windows-x86_64\Debug\Editor\Editor.exe --smoke-test [frames]
```
Renders N frames (default 120) with the normal editor pipeline (frame graph + Scene/Game views + ImGui composition) and exits nonzero if any Vulkan validation error occurred (Debug builds enable `VK_LAYER_KHRONOS_validation`; messages route through Logger, counters live in `RHI/api/RHIDiagnostics.hpp` and are re-exported via `Renderer.hpp`).

**Headless/game-mode proof** — same validation-error contract as `--smoke-test`, but with exactly one view and no ImGui at all (proves the renderer core is editor-free):
```
Binaries\windows-x86_64\Debug\Editor\Editor.exe --game-mode [frames]
```

**Regenerating solution** after modifying any `Build.lua` files:
```
Vendor\Binaries\Premake\Windows\premake5.exe --file=Build.lua vs2022
```

**CI:** `.github/workflows/build.yml` builds Debug+Release and runs all test exes on every PR (the smoke test is local-only — CI runners have no Vulkan GPU).

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

### Key Modules

| Module | Type | Role |
|--------|------|------|
| `HedgehogMath` | static lib | Vectors, matrices, AABB/OBB/Plane/Frustum primitives |
| `HedgehogCommon` | DLL | Shared renderer constants (`MAX_FRAMES_IN_FLIGHT`, `MAX_LIGHTS_COUNT`, …), Camera |
| `HedgehogWindow` | DLL | GLFW window wrapper, input handling (namespace `HW`) |
| `HedgehogSettings` | DLL | YAML-based engine configuration |
| `HedgehogEngine` | DLL | Engine/Frame/Thread context; resource containers (DrawList, Light, Material, Mesh, Texture); ECS integration |
| `RHI` | static lib | Graphics abstraction: `IRHIDevice`, `IRHICommandList`, `IRHITexture`, … — Vulkan backend under `src/Vulkan/` |
| `HedgehogRenderer` | static lib | Multi-pass Vulkan renderer (see structure and passes below) |
| `ECS` | static lib | Entity Component System (EntityManager, ComponentManager, SystemManager, Coordinator) |
| `EcsSerialization` | DLL | ECS serialization; `IHierarchyProvider` interface decoupled from engine |
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
│       └── Renderer.hpp          ← sole public header (ViewHandle-based: CreateView,
│                                    SetFramePipeline, SetCompositionPipeline, SetMainView, …)
├── assets/
│   └── Graphs/                   ← *.rgq pipeline-composition assets (see below)
└── src/
    ├── Renderer/Renderer.cpp     ← owns RHIContext, ThreadContext, ResourceRegistry,
    │                                PassResourceCache, RenderResourceLedger, RenderPassRegistry,
    │                                ViewRegistry, the frame RenderPipeline and the composition
    │                                RenderPipeline; DrawFrame = frame graph → per-view graphs →
    │                                composition graph
    ├── RHIContext/               ← owns IRHIDevice + IRHISwapchain
    ├── ThreadContext/            ← per-frame command lists, fences, semaphores
    ├── ResourceRegistry/         ← mesh/material GPU buffers and descriptor sets
    ├── RenderGraph/              ← the graph runtime + the .rgq loader/factory:
    │   ├── RenderGraph, RenderGraphBuilder, RenderGraphResourcePool, RenderResourceLedger,
    │   │   IRenderPass — handle/pool-based transient textures, auto barriers, cross-graph
    │   │   imports via a shared name→texture ledger
    │   ├── RenderGraphDesc, RenderGraphVocabulary, RenderGraphLoader — parse + validate a .rgq
    │   │   file into a device-free GraphAssetDesc (pure function, unit-tested — see
    │   │   RenderGraphTest below); resource declarations are validated but NOT mechanically
    │   │   wired to texture creation — each pass still declares its own transients in Setup()
    │   ├── RenderPassRegistry — type-string → pass-constructor factory, populated once in
    │   │   Renderer's constructor with the 7 built-in pass types
    │   └── RenderGraphInstantiator — turns a parsed GraphAssetDesc + the registry into pass
    │       instances, in file order
    ├── RenderPipeline/           ← owns one RenderGraph + the pass instances registered on it;
    │                                used for the frame graph, each view graph, and the
    │                                composition graph
    ├── View/                     ← RenderView (one view's own RenderPipeline + camera/gizmo
    │                                payload + desired/compiled size) and ViewRegistry
    │                                (handle→view storage, ordered iteration)
    └── RenderPasses/
        ├── PassInitContext, PassResourceCache  ← shared, ref-counted immutable pass resources
        │                                          (render pass object, pipeline, layouts), so N
        │                                          per-view pass instances share one GPU object
        ├── InitPass/       (frame graph)
        ├── ShadowmapPass/  (frame graph)
        ├── DepthPrepass/   (view graph)
        ├── ForwardPass/    (view graph)
        ├── GizmoPass/      (view graph, editor-only pipelines)
        ├── GuiPass/        (composition graph, editor-only pipelines)
        └── PresentPass/    (composition graph)
```

`api/` is the public include root (added to dependents' include paths).  
`src/` is private — never included from outside the module.

### Render Graph Architecture (HedgehogRenderer)

Three kinds of render-graph instance execute per frame, each a `RenderPipeline` (one `RenderGraph`
+ the pass instances driving it), sharing one frame-level `RenderResourceLedger`:

1. **Frame graph** (once per frame, first) — `InitPass` (acquires the swapchain image, begins the
   shared command buffer), `ShadowmapPass` (writes a `Fixed`-size shadow map, cascades fit to the
   *main view*'s camera).
2. **View graphs** (one instance per registered `RenderView`, application-defined) — e.g. the
   editor's Scene view: `DepthPrePass` → `ForwardPass` → `GizmoPass` into that view's own
   `viewColor`/`viewDepth` (`ViewRelative`-sized); the Game view is identical minus `GizmoPass`.
   Each view owns its own pass instances, framebuffers, and per-frame UBOs/descriptor sets.
3. **Composition graph** (once per frame, last) — e.g. the editor's: `GuiPass` (samples every
   view's colour output, draws ImGui — including the viewport images themselves — into its own
   `guiColor`) → `PresentPass` (blits its declared source to the swapchain, ends and submits the
   command buffer, presents). A headless/game build's composition has no `GuiPass` at all —
   `PresentPass` blits a named view's colour straight to the swapchain (see `present_direct.rgq`).

Which passes exist, in which graph, is declared by a `.rgq` YAML asset under `assets/Graphs/`
(schema doc: `workflow/current-plan.md`, ".rgq schema (version 1)"), loaded through
`Renderer::SetFramePipeline`/`CreateView(desc.PipelineAsset)`/`SetCompositionPipeline` — a bad or
missing asset logs and fails that one call gracefully, it does not crash the process. The five
shipped assets: `frame_default.rgq`, `scene_view.rgq`, `game_view.rgq` (scene minus `GizmoPass`),
`composition_editor.rgq` (GuiPass + PresentPass), `present_direct.rgq` (PresentPass only — the
headless/game composition, exercised by `Editor.exe --game-mode`).

### Project Configuration Files

Each module has its own `Build-[ModuleName].lua` file included from the root `Build.lua`. Global third-party paths and library names are centralized in `Dependencies.lua`. Modifying either requires regenerating the solution.

### Testing

Unit tests use the **doctest** framework. Test projects (each `<Module>/tests/` with its own `Build-<Module>Test.lua`):
- `HedgehogMathTest` — vectors, matrices; `NearlyEqual` helpers with configurable epsilon
- `FileSystemTest` — virtual file system and mounts; provides the `TempDir` RAII helper (`FileSystem/tests/test_helpers.hpp`), reused by other test projects
- `ECSTest` — entity lifecycle, component storage integrity, system signature membership
- `EcsSerializationTest` — scene YAML round-trip plus failure paths (missing/corrupt files)
- `ContentLoaderTest` — OBJ mesh loading with hermetic temp-dir fixtures
- `RenderGraphTest` — `RenderGraphLoader::Parse` and every `.rgq` schema validation rule (V1–V8),
  malformed YAML, and a round-trip parse of all five shipped `assets/Graphs/*.rgq` files off disk
  (assumes the repo root as working directory — true for `Scripts\RunTests.bat`, not for launching
  the exe directly from another directory)

DLL test dependencies are copied to each test's output dir by the owning module's `postbuildcommands` — when adding a test project that links a `SharedLib` module, add a MKDIR/COPY pair to that module's `Build-*.lua`.

The render graph *runtime* has no unit tests by design (it needs a live Vulkan device); it is covered by validation layers + `Editor.exe --smoke-test`/`--game-mode` (see Build System). The `.rgq` *parser and validator* are pure functions over text with no device or filesystem dependency, so they get real unit tests (`RenderGraphTest`, above) — same reasoning as `EcsSerializationTest`'s round-trip/corrupt-file coverage.

### Third-Party Dependencies (git submodules)

glfw, ImGui, yaml-cpp, tinygltf, doctest, Lua — all under `ThirdParty/`. Vulkan SDK headers/libs are also under `ThirdParty/vulkan/`.
