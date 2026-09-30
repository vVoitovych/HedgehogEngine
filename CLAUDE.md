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

**Build output:** every executable and DLL of a configuration goes to one directory, `Binaries/windows-x86_64/[Debug|Release]/`, so each program finds its DLLs next to it with no copy step. Static libraries and object files go to `Binaries/Intermediates/windows-x86_64/[Debug|Release]/[ProjectName]/`. The two directories are `BinariesDir` and `IntermediatesDir` in the root `Build.lua`.

**Running tests** (doctest; each exe exits nonzero on failure):
```
Binaries/windows-x86_64/Debug/HedgehogMathTest.exe
Binaries/windows-x86_64/Debug/FileSystemTest.exe
Binaries/windows-x86_64/Debug/ECSTest.exe
Binaries/windows-x86_64/Debug/EcsSerializationTest.exe
Binaries/windows-x86_64/Debug/ContentLoaderTest.exe
Binaries/windows-x86_64/Debug/HedgehogExtractTest.exe
Binaries/windows-x86_64/Debug/RenderGraphTest.exe
Binaries/windows-x86_64/Debug/HedgehogEngineTest.exe
Binaries/windows-x86_64/Debug/ScriptingTest.exe
```

**Renderer smoke test** — after any renderer/RHI change, run (from the repo root, needs a Vulkan GPU):
```
Binaries\windows-x86_64\Debug\Editor.exe --smoke-test [frames]
```
Renders N frames (default 120) and exits nonzero if any Vulkan validation error occurred (Debug builds enable `VK_LAYER_KHRONOS_validation`; messages route through Logger, counters live in `RHI/api/RHIDiagnostics.hpp` and are re-exported via `Renderer.hpp`).

**Game mode** — the render-graph path as a game build runs it, with no editor:
```
Binaries\windows-x86_64\Debug\Editor.exe --game-mode [frames]
```
Loads `Assets/Scenes/Default.yaml`, starts Play (`Simulation::Play`, so the scene's scripts run) and stops it before teardown, builds a `Renderer` exactly as the editor does, but creates no ImGui, and renders N frames (default 120) with `RenderFrame` from views derived from the scene's camera components. Exits nonzero on any Vulkan validation error **or if an ImGui context ever exists** — the renderer not depending on ImGui is checked, not assumed. Run it after any render-graph or renderer change, alongside `--smoke-test`; like it, it needs a GPU and runs locally only.

**Regenerating solution** after modifying any `Build.lua` files:
```
Vendor\Binaries\Premake\Windows\premake5.exe --file=Build.lua vs2022
```

**CI:** `.github/workflows/ticket-check.yml` fails any PR whose title carries no `HE-<number>` key, and `module-boundaries.yml` runs `Scripts\CheckModuleBoundaries.ps1` (see *Architecture*). There is currently **no build workflow** — Debug/Release builds and the test exes must be run locally before opening a PR (the smoke test is local-only in any case: CI runners have no Vulkan GPU).

## Performance

- **Frame budget: 16.6 ms (60 FPS) in Release** at default window size on the baseline machine. Weigh this in any plan touching the frame loop, render passes, ECS iteration, or per-frame allocations.
- **No optimization without a before/after number.** Measure with:
  ```
  Binaries\windows-x86_64\Release\Editor.exe --benchmark [frames] [scene.yaml]
  ```
  It loads `Assets/Scenes/benchmark.yaml` (or the named file under `Assets/Scenes/`, e.g. `Default.yaml`), warms up 120 frames, measures 600, and logs CPU timings (avg/min/max/p95) for `RenderFrame(total)` and each render-graph pass (summed over the views that ran it) plus wall frame time and FPS. Always Release; run before and after the change and quote both numbers. Baseline table and methodology: `PERFORMANCE.md`.
- Per-pass rows are CPU record times (barriers plus the pass's closure, `RenderGraphRuntime::GetLastPassTimings`); GPU-bound waiting shows up in `RenderFrame(total)`, which includes the fence wait, acquire, submit and present. For GPU-side detail, connect the Tracy 0.13.1 server (client is linked in Release, `TRACY_ON_DEMAND`); zones via `HH_PROFILE_ZONE` in `HedgehogRenderer/src/Profiling/Profiler.hpp`.

## Architecture

The engine is a set of C++20 libraries (mix of static and shared) with an `Editor` executable as the entry point. Dependencies flow strictly upward:

```
Editor (ConsoleApp)            owns ImGui: context, GLFW backend, GUI renderer (RHIImGui)
  └── HedgehogEngine + HedgehogExtract + HedgehogScripting + HedgehogRenderer + RHIImGui + HedgehogWindow
      + HedgehogSettings + HedgehogCommon + DialogueWindows + ECS + FileSystem + Logger + imgui
        ├── HedgehogExtract  (static lib) → ECS, HedgehogEngine, HedgehogMath
        ├── HedgehogScripting (static lib) → HedgehogEngine, ECS, HedgehogMath, FileSystem, Logger,
        │                                   Lua, sol2 (headers) — the Editor and game mode own its ScriptRuntime
        ├── HedgehogEngine   (DLL)        → HedgehogCommon, HedgehogSettings, HedgehogWindow,
        │                                   ContentLoader, HedgehogMath, ECS, EcsSerialization,
        │                                   FileSystem, yaml-cpp, Logger (no Lua)
        ├── HedgehogRenderer (static lib) → RHI, HedgehogMath, HedgehogSettings, HedgehogWindow,
        │                                   ContentLoader, FileSystem, Logger, yaml-cpp, Tracy
        │                                   (headers only: HedgehogCommon, HedgehogExtract's
        │                                   RenderScene; never the ECS, HedgehogEngine or ImGui)
        ├── RHIImGui         (static lib) → RHI, imgui (ImGui's Vulkan renderer, via
        │                                   RHI/api/Vulkan/VulkanNative.hpp)
        ├── HedgehogWindow   (DLL)        → HedgehogMath, GLFW, Logger
        ├── HedgehogSettings (DLL)        → FileSystem, yaml-cpp, Logger
        ├── HedgehogCommon   (DLL)        → HedgehogMath
        └── RHI              (static lib) → Vulkan (Volk + VMA), Logger — no ImGui
```

`Scripts\CheckModuleBoundaries.ps1` enforces three rules on `#include` strings: no include reaches into another module's `src/`; nothing under `HedgehogRenderer/` includes `ECS/`, the engine module (`HedgehogEngine/HedgehogEngine/`, `HedgehogEngine/api/`), `imgui.h` or `imgui_impl_*`; and nothing under `HedgehogEngine/HedgehogEngine/` includes Lua (`ThirdParty/Lua/`, `lua.h`, `lualib.h`, `lauxlib.h`, `lua.hpp`), sol2 (`sol/`), Jolt (`Jolt/`) or `HedgehogScripting/`. Each include is reported once. `-SelfTest` plants each kind of violation in a throwaway tree and checks that each is caught and the tree passes without them. Both run first in `RunTests.bat` and in CI (`module-boundaries.yml`).

**The renderer is being rewritten.** `RENDERING.md` at the repository root is the design: a camera-and-view architecture where a `CameraComponent` carries scene data, a `View` is the render request, and each view owns a render graph. Every frame now renders through it (see *Rendering a frame* below), and the legacy fixed-pass renderer is deleted. Epic [HE-63](https://viktoravoitovych.atlassian.net/browse/HE-63) lands it as 28 sub-1000-line pull requests.

### Key Modules

| Module | Type | Role |
|--------|------|------|
| `HedgehogMath` | DLL | Vectors, matrices, AABB/OBB/Plane/Frustum primitives; `Quaternion` (identity, axis-angle and Euler in degrees, multiply, rotate, normalize, inverse, `Slerp`, `ToMatrix`, `LookRotation` turning local -Z to forward with +Y up). `FromEuler(e).ToMatrix()` equals TransformSystem's `GetRotationX * GetRotationY * GetRotationZ`, so a `TransformComponent` keeps storing Euler degrees; `ToEuler` gives y in [-90, 90] and z = 0 at gimbal lock |
| `HedgehogCommon` | DLL | Shared renderer constants (`MAX_FRAMES_IN_FLIGHT`, `MAX_LIGHTS_COUNT`, …), Camera |
| `HedgehogWindow` | DLL | GLFW window wrapper, input handling (namespace `HW`) |
| `HedgehogSettings` | DLL | YAML-based engine configuration |
| `HedgehogEngine` | DLL | Engine context; resource catalog (Material, Mesh, Texture containers); ECS systems and components; Play mode (`Simulation/Simulation`, owned by `EngineContext`: Edit/Playing/Paused; `Play` snapshots the scene and calls `ISimulationSystem::OnPlayStart` in registration order, `Tick` runs the `SimulationClock`'s fixed steps of `FixedUpdate` then one `Update` only while playing, `Stop` calls `OnPlayStop` in reverse and restores the snapshot; gameplay systems register with `GetSimulation().AddSystem`, the engine registers none itself: the application adds the `HedgehogScripting` `ScriptRuntime`, so scripts run only in Play; `EngineContext::UpdateContext` ticks the simulation before Transform, Hierarchy and Light, which run in every mode; scene deserialization only reads a `ScriptComponent`'s path and Params and creates no Lua state; `ScriptComponent` is pure data, and the engine neither links nor includes Lua); scenes (`SceneManager`: file load/save, and `CaptureSnapshot`/`RestoreSnapshot`, an in-memory `SceneSnapshot` of the YAML, scene name and game-object index that restores entities under their captured ids); `Simulation/SimulationClock` (fixed-step accumulator: steps and interpolation alpha per frame, time scale, pause, max substeps with excess time dropped) |
| `RHI` | static lib | Graphics abstraction: `IRHIDevice`, `IRHICommandList`, `IRHITexture`, … — dynamic rendering and `Barrier()` only (no render-pass/framebuffer objects); Vulkan backend under `src/Vulkan/`, native handles for integrations in `api/Vulkan/VulkanNative.hpp` |
| `RHIImGui` | static lib | ImGui's GPU renderer on the RHI (`RHIImGui::IGuiRenderer`), used only by the Editor |
| `HedgehogRenderer` | static lib | Multi-pass Vulkan renderer (see structure and passes below) |
| `ECS` | static lib | Entity Component System (EntityManager, ComponentManager, SystemManager, Coordinator) |
| `EcsSerialization` | DLL | ECS serialization; `IHierarchyProvider` interface decoupled from engine |
| `HedgehogScripting` | static lib | Script runtime on sol2 (namespace `HedgehogScripting`). `ScriptVM`: one sandboxed `sol::state` with only base, math, string, table, coroutine and utf8, plus `os.clock`/`os.time`; `load` takes text chunks only, `collectgarbage` only `"count"`; no `io`, `os`, `debug`, `package`, `require`, `dofile`, `loadfile`. Source is read through `FS::FileSystemManager` as chunk `@assets://...`; every call is protected with a `debug.traceback` handler and returns `bool`/`std::optional`, logging `[Lua Error]` and keeping `GetLastError()`. Include sol2 only through `HedgehogScripting/api/Sol.hpp`, which sets `SOL_USING_CXX_LUA 0`, `SOL_ALL_SAFETIES_ON` in Debug and `SOL_SAFE_FUNCTION_CALLS`/`SOL_SAFE_USERTYPE` in Release (a wrong argument to a bound function is a script error, never a crash), and opts `HM::Vector3` out of sol2's container detection. Every `ScriptVM` registers the engine globals in `src/Bindings/`: `Vector3` (`Vector3(x, y, z)`, read-write `x`/`y`/`z`, `+ - * /`, unary minus, `==`, `dot`, `cross`, `length`, `normalized` (zero stays zero), `lerp`, `tostring`) and `Quat` (`Quat.identity`, `fromEuler` (three numbers or a Vector3, degrees), `axisAngle`, `lookRotation`, `slerp`, `q:euler()`, `inverse`, `normalized`, read-only `x`/`y`/`z`/`w`, `*` with a Quat or a Vector3); and `Log.info`/`Log.warn`/`Log.error`, which write one Logger line `[Script] <chunk>:<line>: <args>` (arguments through `tostring`, joined by spaces), with `print` routed to `Log.info` so script output reaches the Editor's console instead of stdout. `ScriptClassCache`: compiles each script file once in the one VM (class named by file stem, base `Scripts/Base/ActorScript.lua`), keeps the file's top-level globals as class defaults, and `CreateInstance(path, entity, name)` gives a `ScriptInstance` with its own environment (a shallow copy of the defaults) and `self`; class files run under a proxy `_ENV` that forwards to the instance being called, so methods are shared but globals are per instance (a coroutine resumed inside another instance's call sees that instance's globals). `ScriptInstance::Call(method, args...)` logs errors once as `[Script] <entity> (<file>): <message>` with a traceback and faults the instance on an OnStart/OnUpdate error (later calls skipped). `ScriptRuntime(ECS&, EventBus&, FileSystemManager&)` is an `ISimulationSystem` driving the scripts of entities with a `ScriptComponent` and a `TransformComponent` (its `ScriptedEntities` ECS system, backfilled): instances made on `OnPlayStart` (or on the first Update after a script is added or enabled), OnStart then OnEnable before the first OnUpdate, OnDisable/OnEnable from `Enable`/`NewEnable`, OnDestroy from the `ScriptComponent` removal callback (which it owns while alive, replacing any other) and for every instance on `OnPlayStop`; each instance's `self.entity` is an `Entity` handle (`src/Bindings/EntityBindings`, registered per runtime since it reaches into the runtime's ECS): `id`, `generation`, `isValid()`, `name`, `parent` (nil at top level), `children` (a new array), `==`, `tostring`, and `transform` with `position`, `eulerAngles` (the stored Euler degrees), `rotation` (a `Quat`; writing it stores its Euler angles), `scale`, read-only `forward` (-Z)/`right`/`up` in the parent's space, `translate(v)` and `rotate(q)`/`rotate(x, y, z)` about the entity's own axes. A handle keeps the ECS generation it was made with, so once its entity is destroyed it is invalid for good, even after the id is reused; reading anything but `id`, `generation` or `isValid` then raises a Lua error naming the id (a C++ exception sol2 turns into the error). Transform setters publish `TransformChangedEvent`, so the world matrix updates the same frame (TransformSystem runs after `Simulation::Tick`). The legacy `GetPosition`/`SetPosition`/`GetRotation`/`SetRotation` (`{x, y, z}` tables for the instance's own entity) remain for one epic as wrappers in `src/Bindings/LegacyBindings.cpp`, each logging a deprecation warning the first time a script file uses it; the shipped `PlayerScript.lua` uses `self.entity.transform.eulerAngles`. The component's `Params` are globals; `DescribeScript(path)` lists a script's number/boolean globals without an instance. The Editor (`EditorApplication`, before `EditorGui`, handed over with `SetScriptRuntime`) and `--game-mode` each own one and add it to the Simulation; assigning a script in the inspector or from the Content panel (Edit mode only) sets `ScriptPath` and refreshes `Params` with `DescribeScript`, running no script code. `ScriptClassCache::Reload()` runs on every Play and before describing, so edited script files are read afresh |
| `HedgehogExtract` | static lib | `RenderScene` + `SceneExtractor` (namespace `HX`): reads the ECS read-only, produces the immutable per-frame scene every view will consume (RENDERING.md section 3.1); `MeshBoundsCache` (per-mesh local bounds), `CameraMath` (shared view/projection/pick-ray math), `ScenePicker` (editor picking against `RenderScene` only) |
| `ContentLoader` | static lib | glTF/glb, OBJ, and texture loading (stb_image) |
| `DialogueWindows` | static lib | ImGui-based dialogs for materials, meshes, scenes, textures |
| `Logger` | DLL | Colorized console logging, no dependencies; each run also writes `Logs/<exe>_<YYYY-MM-DD>_<HH-MM-SS>.txt` beside the executable (timestamped lines, flushed per line, created on the first message; console-only if the file cannot be opened) |
| `Editor` | executable | H-form 5-panel editor; Play/Pause/Stop toolbar driving `Simulation` (File New/Open/Save and opening a scene from the Content panel only in Edit; Stop clears a selection whose entity did not survive); ConsolePanel captures Logger; ContentPanel browses `assets://` (folder tree, breadcrumb, search, a grid of type icons from one extension table in `Panels/ContentTypes`, drawn as pictures from `Editor/Resources/Icons` (`Panels/ContentIcons`, uploaded once the device is ready; a type without one is a coloured tile, and the folder also holds pictures for types not added yet), listings re-read at most once a second; double-click or Open acts by type in `EditorGuiContent.cpp`: a scene loads, a mesh, material, texture or script goes onto the selected entity, a shader, pipeline, vertex description or graph opens in its tool, anything else in the OS default app; the context menu adds Show in Explorer and Copy path, virtual or physical, through `Platform/ShellActions`; entries drag as an `HH_ASSET` payload (`Panels/AssetDragDrop`) onto the inspector's mesh, material, base-colour, script and camera-graph fields, each taking only its type, and onto the hierarchy, where a mesh becomes a new entity under the entity it is dropped on (or the root) and a scene asks to open; drops apply after the frame's panels are drawn) |

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
    ├── Renderer/FrameRenderer    ← the frame: per-frame command lists, fences and semaphores,
    │                               views, shared phase, per-view graphs, present
    ├── Graph/                    ← the render graph, graph assets, engine pass types
    │   └── Passes/               ← one .hpp/.cpp per engine pass type, plus PassCommon
    ├── GraphPasses/              ← GraphPassServices: pipelines and uniform rings for the passes
    ├── Views/, Targets/, Frame/  ← ViewManager and culling, RenderTargetRegistry, SharedPhase
    ├── ResourceRegistry/         ← mesh/material GPU buffers and descriptor sets
    └── Pipeline/, Profiling/     ← .shader/.pl loading; FrameStats and Tracy zones
```

`api/` is the public include root (added to dependents' include paths).  
`src/` is private — never included from outside the module.

### Rendering a frame (HedgehogRenderer)

Every frame goes through the render graph: the Editor (and `--game-mode`) builds a `Renderer`, simulates, extracts a `RenderScene`, calls `SyncResources`, then `Renderer::RenderFrame`, which owns the frame lifecycle inline (fence wait, resize before acquire, shared phase, per-view graphs, one execute, present) in `src/Renderer/FrameRenderer`. The editor has three views (RENDERING.md section 7): **scene** (the flycam into the `scene` target), **game** (the scene's camera, redirected to the `game` target with `SetCameraTargetOverride`), and **result** (no camera; the `result` graph's `Ui` pass draws the editor into `main`, sampling both panel targets). A hidden panel tab is sized 0x0, so its view is dropped and its passes never declared (`GetLastFramePassCount`).

Graph passes (`assets/Graphs/*.graph`; each pass type is one `.hpp`/`.cpp` pair in `src/Graph/Passes/` exposing `Get<Name>PassType()`, its slots, parameters and build function, and `EnginePassTypes` registers them): the shared phase's **Shadow** (the cascaded shadow atlas, once per frame), then per view **DepthPrepass** → **Forward** (lit, samples the atlas) → **Gizmo** (scene view only: editor-layer bounds), and the result view's **Ui** (the application's `UiCallback` into `main`). Views are culled by layer mask and frustum.

The legacy fixed-pass renderer is gone: `DrawFrame`, every legacy pass, `RenderQueue` and `ResourceManager` with its textures. Render order comes from the graph compiler's topological sort, render targets are either pooled graph transients or view-owned targets in the render-target registry, the `Renderer` owns the mesh/material `ResourceRegistry` directly, and `FrameRenderer` owns the per-frame command lists and synchronization objects. `Renderer.hpp`'s public surface is `RenderFrame`, `SyncResources`, `Cleanup`, the frame-stats pair and the view accessors (`CreateView`/`UpdateView`/`DestroyView`, `SetCameraTargetOverride`, `DeclareTarget`/`ResizeTarget`/`GetTargetTexture`, `GetLastFramePassCount`) and, for tools, the graph assets and pass types (`GetGraphNames`, `FindGraphAsset`, `GetGraphFile`, `FindPassType`, `GetPassTypeNames`, `GetGraphError`, `DiagnoseGraph`). The Editor's render graph editor (`Editor/Tools/RenderGraphEditor/`) draws and edits a graph with them: its edits are plain `GraphAsset` operations in `GraphEditing`, it checks every edit with `DiagnoseGraph` (problems in red on their node, and in a list under the canvas), it saves through `GraphAssetWriter` and the renderer hot-reloads the file, and node positions live in a `<name>.graph.layout` file beside the graph. New starts an empty untitled graph, Open... and Save As... pick any `.graph` file (`DialogueWindows::RenderGraphOpenDialogue`/`RenderGraphSaveDialogue`), and a picked file is named by `MakeGraphReference` (`Tools/RenderGraphEditor/GraphFileReference`): an engine graph's name, else a virtual path under the most specific mount, else an absolute path, normalized as the renderer lists it, and registered with `Renderer::LoadGraph`. The camera inspector (`EditorGui::DrawCameraGraph`) names the same references: its drop-down shows engine graphs by name and files by file name, Browse... picks a file through the same dialogue and `MakeGraphReference`, Edit opens the camera's graph with `RenderGraphEditorWindow::OpenGraph`, and a reference with no usable graph shows in red with the renderer's reason.

**ImGui belongs to the Editor, never the renderer.** `Editor/ImGuiLayer` owns the context, the GLFW platform backend and the GUI renderer (`RHIImGui::IGuiRenderer`, ImGui's Vulkan backend with dynamic rendering into any texture of one colour format, created in the `Renderer`'s `DeviceReadyCallback`, which hands the application the device and the swapchain's format once at construction). The result view's `Ui` pass hands the editor its colour target through a `UiCallback`. Neither `HedgehogRenderer` nor `RHI` has an ImGui include path or link, and the boundary check keeps it that way.

### Project Configuration Files

Each module has its own `Build-[ModuleName].lua` file included from the root `Build.lua`. Global third-party paths and library names are centralized in `Dependencies.lua`. Modifying either requires regenerating the solution.

### Testing

Unit tests use the **doctest** framework. Test projects (each `<Module>/tests/` with its own `Build-<Module>Test.lua`):
- `HedgehogMathTest` — vectors, matrices, quaternions (1,000 seeded Euler triples matching TransformSystem's rotation product within 1e-5 and round-tripping through `ToEuler` up to sign, gimbal lock, basis-vector rotation, composition order, inverse, `Slerp` endpoints, midpoint and shorter way, `LookRotation`); `NearlyEqual` helpers with configurable epsilon
- `FileSystemTest` — virtual file system and mounts, directory listing (`ListDirectory`: folders first, then files, by name); provides the `TempDir` RAII helper (`FileSystem/tests/test_helpers.hpp`), reused by other test projects
- `ECSTest` — entity lifecycle, component storage integrity, system signature membership
- `EcsSerializationTest` — scene YAML round-trip plus failure paths (missing/corrupt files); `SerializeToString`/`DeserializeFromString` byte-identical round trip, the file version writing exactly the string
- `ContentLoaderTest` — OBJ mesh loading with hermetic temp-dir fixtures
- `HedgehogExtractTest` — ECS-to-`RenderScene` extraction: instance/light/camera extraction, world-bounds correctness from real mesh bounds (unit-cube fallback), `SourceId` round-trip, layer propagation, zero-allocation re-extraction, ray-AABB picking (nearest hit, layer mask, editor layer skipped) and pick rays for both projections
- `ScriptingTest` (`HedgehogScripting/tests/`) — the Entity API (an `EngineContext`'s ECS and transform systems with a runtime over a temp `assets://`): setting `transform.position` moves the world matrix the same frame, id/name/parent/children/equality, every transform property and method, a handle to a destroyed entity staying invalid after its id is reused and raising an error naming the id with a traceback, the legacy functions still working and warning once per script file (`test_log_capture.hpp` captures Logger's console output); bindings: Lua `Vector3` and `Quat` results equal HedgehogMath's in C++, wrong arguments are script errors in Debug and Release, `Log.warn` gives exactly one Logger warning prefixed with the script file and line, `print`/`Log.info` are info lines and `Log.error` an error line (captured from `std::cout`), a failing `__tostring` inside a log call is a script error that logs nothing; `ScriptVM`: the allowed libraries work; `io`, `debug`, `package`, `require`, `dofile`, `loadfile`, `os.execute` and friends are gone and calling them fails as a script error; binary chunks refused both from `load` and from the host; runtime and syntax errors name the `@assets://` chunk and line, with a traceback; files load through the engine file system (reuses `TempDir`); script classes: two instances share the class but not their globals, one compile per file, one VM (`ScriptVM::GetOpenStateCount`), OnUpdate errors name the entity, `assets://` file and line with a traceback and fault the instance, missing methods are not errors, missing files or classes make no instance, and the shipped `ActorScript`/`PlayerScript` load and `PlayerScript` turns a stand-in entity; the runtime, in a small world of ECS, `SceneManager` and `Simulation` over a temp `assets://`: exact callback order across Play, toggles, removal, entity deletion and Stop, nothing in Edit or Paused, scripts added during Play, a speed-2 `PlayerScript` turning by 2*dt through its `Entity` handle, `Params` overriding globals (dirty ones mid-Play), a faulted instance not stopping others, Play again after Stop, and `DescribeScript`; and the engine as the Editor runs it (an `EngineContext` with the runtime added): Default.yaml's object turning only in Play at its saved speed 7.45 and `clockWise: false` with no deprecation warning, freezing on Pause and restored by Stop with its Params, describing a script in Edit running nothing, and 50 Play/Stop cycles keeping one VM and no instances between Plays
- `HedgehogEngineTest` (`HedgehogEngine/HedgehogEngine/tests/`, excluded from the engine DLL's own file list) — links the `HedgehogEngine` DLL; `SimulationClock`: steps and alpha from frame time, substep cap dropping excess time, time scale, pause, bad frame times, reset; scene snapshots: capture, mutate (move, delete, add, rename), restore gives byte-identical YAML with ids, scene name and game-object index preserved, repeatable; `Simulation`: no ticks in Edit or Paused, fixed steps per Tick matching the clock, start/stop order, Stop restoring a mutated scene, a `ScriptComponent`'s path and Params surviving Play/Stop and loading from Default.yaml. `EngineContext`'s constructor creates no window (only `UpdateContext` takes one), so scene and simulation behaviour can be tested here too
- `RenderGraphTest` (`HedgehogEngine/HedgehogRenderer/tests/`) — the whole render graph (namespace `Renderer`), run at runtime by `Renderer::RenderFrame`: declaration (`RGTypes`/`GraphDescription`/`GraphBuilder`/`RGPassBuilder` — handle versioning, dependency-edge recording, size-policy resolution), compilation (`GraphCompiler` — culling, topological sort, barrier derivation, all four validation failure modes), and execution (`FrameArena`, `ResourcePool`, `RenderGraphRuntime` — pass-data/closure arena allocation, descriptor-hash pool reuse, persistent-resource identity, relative sizes resolved against `SetSizeReferences`), graph assets (`GraphAsset`/`GraphAssetVocabulary`/`GraphAssetParser`/`GraphAssetWriter` — one-to-one vocabulary in both directions, strict rejection of unknown strings, malformed-document messages that name the offender, and the writer's exact block layout, write-parse-write round trips and whole-file replacement), graph diagnostics (`GraphDiagnostics` — validation and compile problems of an asset found without a GPU, each tagged with its pass, slot or resource), and instantiation (`PassBuilderRegistry`/`PassInvocation`/`GraphInstantiator` — semantic validation naming pass and slot, the view output contract, outputs written into caller-supplied target textures, and the **C++ equivalence oracle**: `GraphPlanOracle.hpp` renders a compiled plan as name-keyed text so an asset-instantiated graph and its hand-written twin must compile identically), and the shipped graphs (`assets/Graphs/scene|game|result.graph` against `RegisterEnginePassTypes`, each with a hand-written twin, plus `GraphAssetLibrary` hot reload and last-known-good fallback on a broken edit, and directory registration: every `*.graph` under its stem, sorted names, files added later picked up by `Poll()`, and graph references (`GraphReference` — a view's `GraphName` is a registered name or a `.graph` file by virtual or absolute path, normalized so every spelling of one file registers once) loaded by `FindOrLoad` on first use, hot-reloaded like the rest, a missing file reported and picked up once it exists), and the render-target registry (`Targets/RenderTargetRegistry` — `main` and declared targets, all three size policies across a swapchain resize, end-of-frame resize with fence-deferred destruction, zero-area reporting), and views (`Views/ViewManager` — derivation from `RenderScene` cameras with stable ids, application views untouched by reconciliation, target overrides that leave the source camera byte-identical, build-time drops, derived views never including the editor layer), and per-view culling (`Views/ViewCulling` — layer mask and frustum, editor-layer instances split out as the Gizmo pass's overlay), and view ordering (`Views/ViewOrdering` — writer-before-reader order from render-target dependencies, `Priority` only as a tie-break, cycles reported naming every view and resolved by dropping one), and engine pass recording (the `src/Graph/Passes/` depth prepass, shadow, forward and Ui execute bodies — the Ui pass runs the frame's `UiCallback` after sampling the targets its view reads, or clears without one; the Gizmo pass draws the overlay instances' bounds over the view, recording nothing without any — driven through a fake `IGraphPassServices` and the counting `RecordingCommandList`; `ShadowCascades` math; `MakeForwardViewUniform`/`MakeSceneLightsUniform` packing and std140 layout), and the shared frame phase (`Frame/SharedPhase` — the shadow atlas declared once and imported by any number of view graphs through schema-v2 `imports`, ordered and barriered by the compiler, culled when no view reads it; casters chosen by `ShadowCasterMask`, never the view's layer mask; scene lights uploaded once per frame; `SelectShadowView`). The real `GraphPassServices` (pipelines + per-frame viewProj, forward-view and scene-light uniform rings) lives in `src/GraphPasses/`, outside what the test compiles. Links `HedgehogMath` (a DLL, found next to the test in the shared output directory) for `RenderScene`. Compiles the graph's sources directly rather than linking the `HedgehogRenderer` static lib and links only `Logger` and the static `yaml-cpp` beyond that, so it needs no Vulkan SDK, GLFW, or ImGui — a fake `IRHIDevice`/`IRHICommandList` pair in `TestRHIDoubles.hpp` (zero Vulkan dependency, since those interfaces have none of their own) stands in for a real device.

Every executable and DLL builds into the one output directory (`BinariesDir`), so a new test project that links a `SharedLib` module needs nothing beyond `links`: no copy step. A new project uses `targetdir (BinariesDir)` if it is an executable or DLL, `targetdir (IntermediatesDir)` if it is a static library, and `objdir (IntermediatesDir)` either way.

Everything else in the renderer (the RHI-backed render passes) has no unit tests by design; it is covered by validation layers + `Editor.exe --smoke-test` (see Build System). The render graph's declaration layer above is the deliberate exception — it is device-free by construction (RENDERING.md section 5.3), so it is unit-tested like any other module.

### Third-Party Dependencies (git submodules)

glfw, ImGui, imgui-node-editor (the Editor's render graph canvas), yaml-cpp, tinygltf, doctest, Lua, sol2 (header-only, used only by HedgehogScripting), Tracy — all under `ThirdParty/`, each pinned to a commit of a fork under `vVoitovych`. The imgui-node-editor fork carries a small patch for the ImGui version in use; update it there, not in the submodule's working tree. Lua is pinned to the `v5.4.9` release: 5.5 changed `lua_newstate` to take a seed argument, which sol2 does not handle, so stay on the 5.4 line until sol2 supports 5.5. sol2 is pinned to the `v3.5.0` release commit (9190880c); the fork carries no tags, so the pin is by commit. Vulkan SDK headers/libs are also under `ThirdParty/vulkan/`.
