# Assets

What a HedgehogEngine project is made of: the asset types the engine knows today (section 1) and
the file formats each one reads (section 2). The facts below come from the code as of
2026-10-24; each type names where it lives, so the table can be checked against it.

Paths are virtual: `assets://` is the project's `Assets/` folder, `engine://` the engine's root
(its own files under `Content/`), `project://` the project's root and `user://` the editor's
per-user folder.

## 1. Asset types

### 1.1 What exists

Columns:

- **Loaded by**: the module and the call that reads it.
- **Hot reload**: whether a saved change shows without restarting (and how often it is polled).
- **Editor**: what the editor does with it: *create* (a menu or button makes a new one), *edit*
  (a window or the inspector changes it and saves it), *preview* (it can be seen or heard without
  playing the scene), *assign* (the Content panel drops or opens it onto a field).
- **Cooked**: whether the Cooker (`Tools/CookerCore`) packages it, and how it finds it.

#### Content (under `assets://`, shown in the Content panel)

| Type | File | Loaded by | Hot reload | Editor | Cooked |
|------|------|-----------|------------|--------|--------|
| Scene | `.yaml` under `Assets/Scenes` | `SceneManager::LoadScene` (HedgehogEngine, through EcsSerialization's `EcsSerializer`) | No: File > Open again | create (File > New), edit (the whole editor), assign (open, or drop on the hierarchy) | The startup scene and each `--scene`, or every scene with `--all-scenes`; each is the root of a dependency closure |
| Prefab | `.prefab` | `PrefabManager` (HedgehogEngine), a document cache per path | Yes: a changed write time is read again at the next `Instantiate` | create (hierarchy: Create Prefab...), edit (Apply / Revert of an instance's overrides), assign (drop on the hierarchy instantiates it) | Followed from a scene's instance (`Prefab` key) and from nested prefabs |
| Material | `.material` (YAML) | `MaterialContainer` through `MaterialSerializer` (HedgehogEngine) | No | create (Assets > Create material, the Content panel's "+"), edit (the inspector's material fields, Save material), assign | Followed from `RenderComponent::Material`; its maps followed in turn |
| Texture | `.png`, `.jpg`, `.jpeg`, `.tga` | ContentLoader's `LoadTexture` (stb_image), uploaded by the renderer's texture cache with a mip chain | No | assign (onto a material slot) | Followed from materials and `UiImageComponent::Texture` |
| Mesh | `.obj`, `.gltf`, `.glb` | ContentLoader's `LoadMesh` (tinyobjloader for OBJ, tinygltf); `MeshContainer` | No | assign (onto a mesh, or drop on the hierarchy to make an entity) | Followed from `MeshComponent::MeshPath`; a `.gltf`'s external buffers and images followed |
| glTF materials | inside `.gltf`/`.glb` | ContentLoader's `LoadGltfMaterials`, written out by `ImportGltfMaterials` | No (an import never overwrites) | create (Import materials in the Content panel's menu, or dropping a glTF on the hierarchy) | As `.material` files, once imported |
| Animation clips | inside a skinned `.gltf`/`.glb` | ContentLoader's `LoadAnimations`, kept by the `Mesh` | No | preview (the animator's `PreviewTime` in Edit mode), pick (the Clip combo) | With their mesh file |
| Animator controller | `.animctrl` (YAML) | HedgehogAnimation's `ParseAnimatorController`, cached by `AnimationSystem` | Yes: once a second (`CONTROLLER_POLL_INTERVAL`) | create (New in the editor), edit (Window > Animator Controller, with a `.animctrl.layout` beside it), preview (the active state during Play), assign (drag onto an Animator) | Followed from `AnimatorComponent::Controller` |
| Script | `.lua` | HedgehogScripting's `ScriptSystem`, through the sandbox's `LoadScriptFile` | Yes: once a second (`RELOAD_POLL_INTERVAL`), also during Play | assign (onto a script component); edited in an outside editor | Followed from `ScriptComponent::ScriptFile` and its AssetRef properties' values (the code itself is not read) |
| Font | `.ttf`, `.otf` | ContentLoader's `LoadFont` (stb_truetype), baked per size by `FontContainer` | No | assign (onto a UI text) | Followed from `UiTextComponent::Font` |
| Audio clip | `.wav`, `.mp3`, `.flac` | HedgehogAudio's `AudioEngine::LoadClip` (miniaudio), decoded once into a cache | No | preview (opening it plays it as a one-shot), assign (onto an audio source) | Followed from `AudioSourceComponent::Clip` |
| Environment | `.hdr` (equirectangular) | ContentLoader's `BakeEnvironment` (stb_image's float loader), baked once per path by the renderer | No (baked once per session) | assign (onto an environment's Map) | Followed from `EnvironmentComponent::Map` |
| Input actions | `Assets/Input/actions.yaml` | HedgehogInput's `LoadInputActions`, owned by `EngineContext` | Yes: once a second (`INPUT_ACTIONS_POLL_INTERVAL`) | edit (Window > Input Actions, also by opening the file) | Always, as a runtime asset when present |

#### Render assets (under `engine://`, the engine's, but also accepted from a project)

| Type | File | Loaded by | Hot reload | Editor | Cooked |
|------|------|-----------|------------|--------|--------|
| Render graph | `.graph` (YAML) | The renderer's `GraphAssetLibrary` (`GraphAssetParser`) | Yes: polled every frame (`GraphAssetLibrary::Poll`), a broken edit keeping the last good graph | create (New), edit (Window > Render Graph Editor, with a `.graph.layout` beside it), assign (a camera's Graph) | The shipped `scene`, `game` and `result` graphs always; a camera's graph followed, and from it its pass types' shaders |
| Shader | `.shader` (pipeline state) and GLSL `.vert`/`.frag`/`.comp`/`.glsl` | The renderer's pipeline loader; GLSL compiled to `.spv` by `glslc` at build and package time | No | edit (Window > Shader) | The `.shader` followed with its layout, vertex description and `.spv` stages; GLSL sources never ship |
| Pipeline layout | `.pl` | The renderer's pipeline loader | No | edit (Window > Pipeline) | Followed from a `.shader` |
| Vertex description | `.vdes` | The renderer's `VertexDescLoader` | No | edit (Window > Vertex Descriptions) | Followed from a `.shader` |

#### Project and engine files

| Type | File | Loaded by | Hot reload | Editor | Cooked |
|------|------|-----------|------------|--------|--------|
| Project | `project://Project.yaml` | HedgehogSettings' `ProjectSettings` | No | edit (File > Project Settings...) | Always |
| Engine settings | `project://engine_settings.yaml` | HedgehogSettings' `Settings` (shadows, layers, physics, Lua debugger) | No | edit (Edit > Settings...) | Always |
| Plugin | `<name>.dll` beside the engine | HedgehogEngine's `PluginManager` | Yes: once a second in Edit mode, from a shadow copy | edit (the plugin list in Project Settings) | Each enabled plugin's DLL, from the binaries folder |
| Engine content | `engine://Content/...` (default cube and sphere, cells texture, base script, editor font) | The loaders above | As their types | — | The defaults always (`GetEngineRuntimeAssets`), the rest when referenced |
| Template | `engine://Templates/Empty/` | HedgehogSettings' `CreateProject` | — | create (File > New Project...) | Never |
| Save game | `<slot>.save` under LocalAppData | EcsSerialization's `SaveSlotStore`, through `SaveGameManager` | — (player data) | — | Never: made by the game |
| Editor state | `user://editor_settings.yaml`, `user://imgui.ini` | The editor's `EditorSettings`, ImGui | — | (the editor's own layout) | Never |

### 1.2 Observations

- **Only materials can be created from the Content panel.** Everything else is made outside the
  editor (textures, meshes, audio, fonts, HDRs, scripts) or from its own tool (controllers,
  graphs, prefabs).
- **Hot reload covers the text assets people edit while the game runs** (scripts, controllers,
  input actions, graphs, prefabs, plugins). Binary content (textures, meshes, audio, fonts,
  environments) is read once per session, so replacing an exported file needs a restart.
- **There are no import settings.** How a texture is used (sRGB or linear, mipmaps, filtering,
  compression) is decided by where it is used: a material slot's colour space, the UI's clamp to
  the edge. The same file cannot be imported two ways.
- **Previews are few.** Audio plays and animation clips scrub, but the Content panel shows type
  icons, not thumbnails of textures, meshes or materials.
- **Clips live in their mesh.** An animation can only be played on the mesh whose file holds it;
  there is no retargeting and no clip file of its own.

### 1.3 Types to add

Ordered by priority. High: a project of any size needs it soon, or other work waits on it. Medium:
it removes a real limit, but there is a workaround. Low: needed only by some games.

| Priority | Type | Why | What it would hold |
|----------|------|-----|--------------------|
| High | **Texture import settings** (a `.meta` or `.texture` beside the image) | Every texture is used as the slot it sits in decides; there is no way to say a texture is linear data, to turn off mipmaps or filtering for pixel art, to compress, or to cap its size. HE-385's compressed formats need somewhere to record how to compress. | Colour space, mipmaps, filter and wrap, compression format, max size; the Cooker would apply it |
| High | **Data asset** (a typed YAML record a script reads) | Gameplay tuning (weapons, enemies, levels, loot tables) now lives in script properties or in code. A data file the inspector edits and scripts read (`Data.load("assets://Weapons/Sword.data")`) keeps it out of scenes and code. | A schema (a script's declared fields, reusing the `Properties` syntax) and values |
| High | **Particle system** | There is no particle effect at all: no sparks, smoke, dust or magic. Most games need some. | Emitter shape, rate, lifetime, start values, curves over life, a material or texture sheet; a component that plays one |
| Medium | **Physics material** | Friction and restitution are set per collider, so a surface type (ice, rubber, metal) is repeated on every collider and cannot be changed in one place. | Friction, restitution, how they combine; a collider's `Material` AssetRef |
| Medium | **Audio mixer / bus** | Only a master volume exists: music, effects and voice cannot be balanced or muted apart, and there are no effects (reverb, low-pass). | Buses in a tree, their volumes and effects; an audio source's `Bus` |
| Medium | **Sprite atlas** | UI images and any 2D game draw whole textures: many small images mean many textures and draw calls, and there is no sprite sheet animation. | One texture with named rectangles; a UI image or sprite names a rectangle |
| Medium | **Animation clip** (standalone, and an avatar to retarget) | Clips are tied to the mesh file that holds them. A library of motions shared by several characters needs clips of their own and a mapping between skeletons. | The clip's tracks by joint name; an avatar mapping one skeleton's joints to another's |
| Medium | **Localisation table** | UI text is written into components in one language. | Keys to strings per language; a UI text names a key |
| Medium | **Post-process profile** | Exposure lives on the environment; there is no bloom, colour grading, vignette or tone mapping choice to share between scenes. | The tone mapper and its settings, bloom, a colour grading LUT; a camera or volume names one |
| Low | **Terrain** | Large outdoor ground is now a mesh made elsewhere. | A height map, splat maps of materials, its size; editing tools would follow |
| Low | **Navigation mesh** | AI has nothing to find paths on. | A baked walkable surface per scene |
| Low | **Video** | No cutscenes or animated UI from video. | A video file played onto a texture |
| Low | **Shader graph / material template** | A material can only use the one PBR shader; a custom look needs engine code. | A node graph compiled to a `.shader` a material names |

## 2. File formats

For each asset type made outside the engine: the formats it reads today, each with the code that
reads it, then the formats worth adding, with the library that would read them, its licence and a
rough cost (S: a day or two, M: a ticket, L: an epic or a large dependency). Formats the engine
writes itself (scenes, prefabs, materials, controllers, graphs, input actions, project files) are
its own YAML and are not repeated here.

### 2.1 Textures

| Supported | Read by | Notes |
|-----------|---------|-------|
| PNG | stb_image 2.28 (`ContentLoader/src/TextureLoader.cpp`, `stbi_load_from_memory`) | 8 and 16 bits per channel, palettes and grey; always decoded to RGBA8 (`STBI_rgb_alpha`), so 16-bit images lose precision |
| JPEG | stb_image | Baseline and progressive, 8-bit; no 12-bit or arithmetic coding |
| TGA | stb_image | Uncompressed and RLE |
| BMP, PSD, GIF, PNM, PIC | stb_image | Decoded by the same call, but the Content panel calls them Other (`Editor/Panels/ContentTypes.cpp` lists only `.png`, `.jpg`, `.jpeg`, `.tga`), so they cannot be dropped on a material slot; PSD gives only the composited image, GIF only its first frame |

Every texture is uploaded as RGBA8 with a mip chain the GPU generates (`ResourceRegistry`'s texture
cache), so a 4K texture takes 85 MB of video memory and its mips are box-filtered.

| Wanted | Library | Licence | Priority | Cost | Why |
|--------|---------|---------|----------|------|-----|
| KTX2 with Basis Universal (UASTC, ETC1S) | libktx, or basis_universal's transcoder | Apache-2.0 | High | M | GPU-compressed textures transcoded to BC7/BC5/BC4 at load: a quarter of the video memory and much faster loads. Sketchfab and Khronos samples ship it (`KHR_texture_basisu`). Needs texture import settings (section 1.3) to say how to encode |
| DDS (BC1 to BC7) | A small reader (dds-ktx, single header) | BSD-2-Clause | Medium | S | Ready-made compressed textures, mip chains included, from most DCC tools and asset stores |
| EXR (half and float) | tinyexr | BSD-3-Clause | Medium | S | Linear data textures and HDR images; Poly Haven publishes its HDRIs and many PBR maps as EXR |
| BMP, PSD, GIF in the Content panel | (already decoded) | — | Low | S | Add the extensions to `ContentTypes` so the files the loader already reads can be assigned |
| WebP | libwebp | BSD-3-Clause | Low | S | Smaller lossless and lossy images; some online libraries offer it |

### 2.2 Meshes

| Supported | Read by | Notes |
|-----------|---------|-------|
| OBJ | tinyobjloader (`ContentLoader/src/ObjLoader.cpp`) | Positions, UVs (v flipped), normals; tangents generated. Its `.mtl` materials are parsed by tinyobjloader and then ignored. A face corner without a UV gets (0, 0) and one without a normal a smooth normal, generated from the faces around its vertex (since HE-387) |
| glTF 2.0 (`.gltf` with external or `data:` buffers, `.glb`) | tinygltf (`ContentLoader/src/GltfModel.cpp`, `GltfMeshLoader.cpp`) | Meshes (all primitives merged, one material: the first primitive's), `TANGENT` or generated tangents, one skin (JOINTS_0/WEIGHTS_0), skeletal animation (Step and Linear; CUBICSPLINE played as Linear), metallic-roughness materials (`GltfMaterialLoader.cpp`: factors, the five maps, alpha mode and double-sidedness). Ignored: morph targets, a second UV set, cameras and lights, every `KHR_`/`EXT_` extension, images embedded in the file (for materials) |

| Wanted | Library | Licence | Priority | Cost | Why |
|--------|---------|---------|----------|------|-----|
| FBX | ufbx (single file) | MIT or public domain | High | M | Most asset stores (Unity Asset Store, Mixamo, Quaternius, Kenney) ship FBX; ufbx reads meshes, skins and animation without the Autodesk SDK |
| glTF extensions: `KHR_texture_transform`, `KHR_materials_emissive_strength`, `KHR_mesh_quantization`, `KHR_materials_clearcoat`/`transmission` | tinygltf already parses them | — | Medium | S each | Sketchfab and Blender exports use them; without them tiled textures, bright emissives and quantized meshes come out wrong |
| glTF compression: `KHR_draco_mesh_compression`, `EXT_meshopt_compression` | Draco; meshoptimizer | Apache-2.0; MIT | Medium | M | Compressed meshes from Sketchfab and gltfpack; meshoptimizer is also worth having for simplification and LODs |
| Several materials per mesh | (engine work) | — | Medium | M | A glTF or OBJ with several materials now draws with the first; needs submeshes |
| OBJ `.mtl` materials | tinyobjloader already reads them | — | Low | S | Import an OBJ's materials as glTF's are imported |
| USD / USDZ | OpenUSD | Modified Apache-2.0 | Low | L | Film and Apple pipelines; a very large dependency |
| Collada, 3DS and the rest | Assimp | BSD-3-Clause | Low | M | Older formats; Assimp is large and its quality varies by format, so prefer FBX and glTF |

### 2.3 Animation

| Supported | Read by | Notes |
|-----------|---------|-------|
| glTF 2.0 skeletal animation | tinygltf (`ContentLoader/src/GltfAnimationLoader.cpp`) | Translation, rotation and scale channels on the skin's joints; clips live in their mesh's file (section 1.2) |

| Wanted | Library | Licence | Priority | Cost | Why |
|--------|---------|---------|----------|------|-----|
| FBX animation | ufbx | MIT or public domain | High | S (with FBX meshes) | Mixamo's library and most store animations are FBX |
| glTF clips without a mesh | tinygltf | MIT | Medium | S | Animation-only glTF files, once clips have their own asset (section 1.3) |
| CUBICSPLINE interpolation | (engine work) | — | Low | S | Played as Linear today, with a warning |
| BVH motion capture | A small reader | — | Low | S | Free mocap libraries (CMU) publish BVH |

### 2.4 Audio

| Supported | Read by | Notes |
|-----------|---------|-------|
| WAV | miniaudio 0.11.25 (dr_wav), `HedgehogAudio/src/AudioClipCache.cpp`, `ma_decoder_init_memory` | PCM and float; decoded whole into 32-bit floats at load |
| FLAC | miniaudio (dr_flac) | Decoded whole at load |
| MP3 | miniaudio (dr_mp3) | Decoded whole at load |

Every clip is decoded whole into memory: a three-minute stereo track at 48 kHz takes about 70 MB.

| Wanted | Library | Licence | Priority | Cost | Why |
|--------|---------|---------|----------|------|-----|
| Ogg Vorbis | stb_vorbis, which miniaudio already integrates | Public domain or MIT | High | S | The usual format for game music and effects (Kenney, OpenGameArt, freesound exports); smaller than MP3 at the same quality |
| Streaming long clips | miniaudio's streamed `ma_sound` | MIT-0 or public domain | High | M | Music decoded as it plays rather than whole at load |
| Opus | libopus and opusfile | BSD-3-Clause | Low | M | The best quality per byte, mostly for voice |

### 2.5 Fonts

| Supported | Read by | Notes |
|-----------|---------|-------|
| TrueType (`.ttf`) | stb_truetype 1.26 (`ContentLoader/src/FontLoader.cpp`) | Glyph outlines rasterized per pixel size into an atlas; ASCII and Latin-1 only; kerning from `kern` and GPOS |
| OpenType (`.otf`) | stb_truetype | CFF outlines included; no shaping (ligatures, Arabic, Indic) |

| Wanted | Library | Licence | Priority | Cost | Why |
|--------|---------|---------|----------|------|-----|
| More than Latin-1 (Cyrillic, Greek, CJK) | (engine work over stb_truetype) | — | High | M | Bake the codepoints a text uses, not a fixed range; needed by any localisation (section 1.3) |
| Signed-distance-field atlases | msdfgen | MIT | Medium | M | Sharp text at any size and cheap outlines and shadows, without one atlas per size |
| Font collections (`.ttc`) | stb_truetype (`stbtt_GetFontOffsetForIndex` with an index) | — | Low | S | Many system and Noto fonts ship as collections; only the first font is read now |
| WOFF / WOFF2 | woff2 with brotli | MIT | Low | S | Web font files; most fonts are also offered as TTF |
| Text shaping | HarfBuzz | MIT ("Old MIT") | Low | L | Right-to-left and complex scripts |

### 2.6 Environments

| Supported | Read by | Notes |
|-----------|---------|-------|
| Radiance HDR (`.hdr`), equirectangular | stb_image's float loader (`ContentLoader/src/HdrImage.cpp`) | 8-bit images refused; baked into a prefiltered cube and SH at first use (about 190 ms for a 1K sky in Release) |

| Wanted | Library | Licence | Priority | Cost | Why |
|--------|---------|---------|----------|------|-----|
| EXR | tinyexr | BSD-3-Clause | Medium | S | Poly Haven, ambientCG and most HDRI sites offer EXR, often with more range than their `.hdr` |
| A cooked, prefiltered cube (KTX2) | libktx | Apache-2.0 | Medium | M | The Cooker bakes once and the game loads the result, so a scene with an environment starts without the bake |
| Cube faces (six images, or a cross) | stb_image | — | Low | S | Skyboxes made as cube maps rather than panoramas |

### 2.7 Scripts and shaders

| Supported | Read by | Notes |
|-----------|---------|-------|
| Lua 5.4 source (`.lua`) | Lua 5.4.9 with sol2 (`HedgehogScripting`) | Text only: precompiled chunks are refused by the sandbox |
| GLSL (`.vert`, `.frag`, `.comp`, `.glsl`) | glslc (shaderc), at build and package time | Compiled to SPIR-V; only the engine's shaders, through `.shader` files |

| Wanted | Library | Licence | Priority | Cost | Why |
|--------|---------|---------|----------|------|-----|
| Shaders compiled at run time, hot-reloaded | shaderc or glslang | Apache-2.0; BSD-3-Clause and others | Medium | M | Edit a shader and see it without rebuilding, as graphs already reload |
| HLSL | DirectX Shader Compiler (SPIR-V output) | NCSA (University of Illinois, as LLVM) | Low | M | Shaders written for other engines |
