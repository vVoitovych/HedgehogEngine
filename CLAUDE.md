# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

HedgehogEngine uses **Premake5** to generate Visual Studio 2022 solution files. There is no CMake.

**Initial setup** (run once, or after submodule changes):
```bat
Scripts\SetupWindows.bat
```
This initializes git submodules recursively and generates `HedgehogEngine.sln`.

**Building:** Open `HedgehogEngine.sln` in Visual Studio 2022 and build. Configurations: `Debug` and `Release`, platform: `Windows x64`.

**Shader compilation** is automatic via a Shaders project pre-build command that calls `ThirdParty/glslc/CompileShaders.bat`, compiling `.vert`/`.frag`/`.comp` GLSL sources to SPIR-V (`.spv`).

**Build output:** `Binaries/Windows-x64/[Debug|Release]/[ProjectName]/`

**Running tests:**
```
Binaries/Windows-x64/Debug/HedgehogMathTest/HedgehogMathTest.exe
```

**Regenerating solution** after modifying any `Build.lua` files:
```bat
cd Scripts && premake5 --file=..\Build.lua vs2022
```

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
| `Shaders` | static lib | GLSL sources; compiled to SPIR-V by pre-build step |
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
5. **GuiPass** — ImGui overlay (renders into RHI color buffer)
6. **PresentPass** — blits scene + GUI to swapchain and submits

### Project Configuration Files

Each module has its own `Build-[ModuleName].lua` file included from the root `Build.lua`. Global third-party paths and library names are centralized in `Dependencies.lua`. Modifying either requires regenerating the solution.

### Testing

Only `HedgehogMath` has unit tests (`HedgehogMathTest`), using the **doctest** framework. Test files: `test_vector.cpp`, `test_matrix.cpp`, `test_common.cpp`. Floating-point comparisons use `NearlyEqual` helpers with configurable epsilon.

### Third-Party Dependencies (git submodules)

glfw, ImGui, yaml-cpp, tinygltf, doctest, Lua — all under `ThirdParty/`. Vulkan SDK headers/libs are also under `ThirdParty/vulkan/`.
