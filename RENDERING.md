# Rendering Architecture

**Status:** design, not yet implemented. Written 2026-09-20 against `master` at `df8bf99`.
Derived from the camera-centric overview in Google Docs, expanded and reconciled with what is
actually on disk. It replaces an earlier rewrite design that was rejected and deleted on
2026-09-20 — nothing here inherits from it.

The three decisions that shape everything below, taken deliberately:

1. **`CameraComponent` and `View` are separate concepts.** A camera is scene data; a view is a
   render request. See §1.
2. **View order is derived from render-target dependencies**, not hardcoded and not priority
   alone. See §3.3.
3. **Graphs are data assets from the start.** This carries real risk — you are bringing up a new
   graph runtime and a new asset pipeline at the same time — so §6 defines a structural
   mitigation (an equivalence oracle against hand-written C++) rather than leaving it to care.

---

## 1. The model

Three nouns. Keeping them distinct is what makes the editor and the game share one code path.

### Camera — scene data, lives on an entity

```
CameraComponent
    ProjectionType      Perspective | Orthographic
    Fov / OrthoSize, NearPlane, FarPlane
    LayerMask           uint32, which layers this camera renders
    TargetIntent        Main | Texture(name)
    GraphName           which graph asset to instantiate
    Priority            int, tie-break only (see §3.3)
    Enabled             bool
```

It holds no GPU resource, no target pointer and no graph instance. It is serialized into the
scene file and edited in the inspector like any other component. Its transform comes from
`TransformComponent`, as lights already do.

### View — a render request, not an entity

```
View
    Camera        optional — absent for pure composites
    Targets       ordered slots, resolved to concrete render targets
    Graph         instantiated graph for this frame
    LayerMask     copied from the camera, or set directly
    Viewport      rect within the target
    Priority      int
```

Views come from two places and **never** from both at once for the same thing:

- **Derived** from every enabled `CameraComponent` during extraction. This is all a game build
  ever has.
- **Created directly** by the application through `Renderer::CreateView(...)`. This is how the
  editor gets its scene and result views. They are not entities, so they are never written to
  the user's scene file.

This split is the answer to the "result camera" problem. A composite has no projection, no
frustum and no culling; forcing it to be a camera would mean every camera carries meaningless
projection fields, and would push the editor's own cameras into the serialized scene. As a view
with no camera, it is simply a graph with bound targets.

### Graph — the pass DAG for one view

Per view, not global. One global graph would force every view through the same pass sequence,
which is exactly what a scene view and a UI composite must not share.

---

## 2. Layers

**An object is in exactly one layer. A camera has a mask of layers.** That asymmetry is
deliberate — it is what makes "show gizmos in the scene view, not in the game view" a one-bit
change rather than a set operation.

- `RenderComponent` gains `Layer` (a `uint32` index, 0–31). Today it holds only `IsVisible` and
  `Material`; there is no layer field anywhere in the engine.
- **Layer names are engine settings.** `HedgehogSettings` owns a fixed table of 32 layer names,
  persisted in the settings YAML and editable from an editor settings panel — so the inspector
  shows `Editor Gizmos` rather than `bit 7`, and a camera's mask is a list of checkboxes with
  real names rather than a hex field. Unnamed slots display as `Layer 12` and stay selectable.
  Layer 0 is `Default`; every existing scene file deserializes to it. Renaming a layer changes
  only the label — the scene stores the index, never the name, so a rename can never silently
  re-bucket objects.
- Culling tests `(1u << instance.Layer) & view.LayerMask`.

**Shadow casting does not use the view's mask.** A wall on a hidden layer must still shadow the
visible scene, and an editor gizmo must never cast a shadow. The shared shadow pass (§3.4) uses
its own `ShadowCasterMask` from settings, defaulting to every layer except the editor ones.

Lights are unlayered in v1. Per-light culling masks are a later addition and need no interface
change; recorded in §10.

---

## 3. The frame

### 3.1 Phases

```
Simulate      gameplay, transforms, animation           (outside the renderer)
Extract       ECS  ->  immutable RenderScene            once per frame
Build views   cameras + app views -> View set           once per frame
Order views   topological sort by RT dependency         once per frame
Shared        scene upload, shadow atlas                once per frame
Per view      build graph, compile                      per view
Execute       one recorded stream, derived barriers     once per frame
Present
```

`Extract` runs once and produces an immutable `RenderScene` that every view reads. Instances
carry precomputed world bounds so culling never touches mesh data, and a `SourceId` (the
`ECS::Entity`) used only by editor picking. The renderer never reads the ECS.

### 3.2 Building the view set

For each enabled `CameraComponent`: resolve `TargetIntent` to a concrete target, instantiate
`GraphName`, copy the layer mask. Then append the application's own views.

Views are dropped here, not later, when: disabled, target unresolvable, or target area is zero.
A collapsed editor panel or a minimized window produces a 0×0 target and must skip the view
entirely rather than create a degenerate resource.

**The component is never mutated.** In editor mode the game camera's target is overridden *on
the view*, not written back to the component. Writing to a serialized component every frame is
how Play/Stop corrupts a scene file.

### 3.3 Ordering

Each view declares what it reads (render targets its graph imports) and writes (its bound
targets). The engine topologically sorts the view set from those edges. `Priority` breaks ties
between views with no dependency between them, and nothing else.

This makes the editor's scene → game → result order emerge rather than being special-cased in
engine code, and it makes a monitor displaying a feed from another monitor work with no
application-level rule.

A cycle — a camera that can see its own target — is reported as a named validation error and
that view is dropped for the frame. It must never deadlock or render garbage.

### 3.4 Shared passes

Work that is not per-view happens once, before any view's graph: GPU scene upload (instance
transforms, lights, materials) and the shadow atlas. Without this, three views seeing one
directional light render that shadow map three times.

Shared outputs are ordinary graph resources that per-view graphs import, so §3.3's ordering and
the compiler's barrier derivation cover them with no special case.

---

## 4. Render targets

A small registry owns every named target:

- **`main`** — the swapchain. Written by exactly one view per frame (§8).
- **Texture targets** — declared as assets (`name`, format, and a size policy). A material
  referencing one samples last frame's contents if the producing view has not run yet this
  frame; that is well-defined, not a bug, and §3.3 makes it rare.

**Size policies**, used both by targets and by graph resources:

| Policy | Meaning |
|---|---|
| `Absolute` | fixed width × height |
| `RelativeToResult(scale)` | a multiple of the view's result target — half-res bloom, quarter-res AO |
| `RelativeToSwapchain(scale)` | tracks the window |

**Resize is always deferred to end of frame**, behind the frame fence. Resizing a resource the
GPU is still reading is a use-after-free, and the editor resizes constantly as panels are
dragged. Graph resources with a relative policy recompute from the new result size on the next
frame's build.

---

## 5. The graph

### 5.1 Declaration

Versioned handles (`RGTexture`, `RGBuffer`). A pass declares intent through a builder —
`Create`, `Import`, `SampleTexture`, `ColorTarget`, `DepthTarget`, `DepthReadOnly`,
`StorageWrite`, `ReadBuffer`, `WriteBuffer`, `SetSideEffect` — and every write produces a new
handle version. Dependencies are never stated directly; they are recovered from versions.

### 5.2 Outputs are slots, not string tags

The overview binds a graph's result by a `"result"` tag. String-keyed binding between graph and
camera is the drift problem that made the previous architecture painful, and it does not say how
several targets map to several tagged resources.

Instead a graph declares an **ordered list of typed outputs**:

```
outputs:
  - slot: 0   name: color   format: RGBA16F   size: RelativeToResult(1.0)
  - slot: 1   name: depth   format: D32       size: RelativeToResult(1.0)
```

The view binds its targets by slot index, validated at instantiation: count, format
compatibility and size policy must match, or the graph is rejected with a message naming the
slot. Names survive for diagnostics only.

### 5.3 Compile

1. Build the DAG from handle versions.
2. Refcount and **cull** every pass whose outputs are never read and which has no
   `SetSideEffect`. A hidden editor panel costs nothing, with no enable/disable plumbing.
3. **Topologically sort**, tie-breaking toward keeping the same render target adjacent.
4. Compute first and last use per physical resource.
5. **Derive and batch barriers.** The graph is the single owner of resource state; no pass
   transitions anything itself.
6. **Validate**: read with no producer, write to an imported read-only resource, cycles,
   unbound slots. Every failure names the pass and the resource.

Compilation is device-free and therefore fully unit-testable headlessly. Resource aliasing
within a frame is a later addition that needs no interface change (§10).

### 5.4 Resource pool

Transients are pooled by descriptor hash and recycled **across views and across frames**. Naive
per-view allocation multiplies memory by the view count — the editor alone is three
full-resolution targets before any graph transients.

Persistent, excluded from pooling: anything sampled by a later frame (temporal effects) and
anything handed to ImGui, which needs a texture id stable across frames.

---

## 6. Graph assets

Graphs are authored as data from the start. Two rules keep that from becoming the trap it was
in the previous design.

### Rule 1 — data may express nothing that C++ cannot

The loader is a thin mapping onto the same builder API from §5.1. Pass *types* are registered in
C++:

```cpp
registry.Register("Forward", &BuildForwardPass);
```

The asset supplies **parameters** (formats, sizes, flags, shader override) and **identity**
(which resource binds to which slot). It never supplies behaviour. A builder owns its own slot
semantics; the asset cannot invent a pass, a binding model, or a capability.

This is the inverse of the usual mistake, where the asset becomes the authority on slot identity
and the pass must be validated against it.

### Rule 2 — every shipped asset has a C++ equivalence oracle

For each graph that ships, a headless test constructs the same graph by hand through the builder
and asserts the **compiled plans are identical** — same surviving passes, same order, same
barriers.

This is what makes bringing up both layers at once tractable. When an image is wrong you can
tell immediately whether the graph runtime or the asset pipeline is at fault, because one is the
control for the other. Without it you are debugging two new systems through a single black
screen.

### Schema v1, deliberately minimal

`version`, `outputs[]` (§5.2), `resources[]` (name, format, size policy), `passes[]` (type,
name, slot bindings, parameters). No expressions, no conditionals, no parameter type system with
coercion. If the schema starts growing one, stop — that is the signal that the thing being
expressed belongs in C++.

### Failure behaviour

Validation runs at instantiation, not at load, because a graph is meant to be editable in
between. A malformed asset logs a named error and the view **falls back to its last known-good
instantiation**; a typo must never black-screen the editor. Hot reload is realistic precisely
because passes hold no state — a changed asset rebuilds the graph on the next frame.

---

## 7. Editor integration

The editor creates three views and owns them:

| View | Camera | Target | Graph |
|---|---|---|---|
| Scene | editor flycam, not an entity | `scene` texture | `scene` |
| Game | the scene's main camera, target overridden | `game` texture | `game` |
| Result | none | `main` | `result` |

**ImGui stays in the Editor.** The `result` graph's UI pass takes an execute callback supplied
by the application; the renderer never includes `imgui.h`. Today `GuiPass` lives inside
`HedgehogRenderer` and `Renderer::BeginGui()` is public API — both disappear. A game build then
links no ImGui at all, which is a structural guarantee rather than a matter of not adding a
node.

The scene and game panels draw their view's persistent target by a stable texture id,
invalidated only on resize. This replaces `GetSceneViewTextureId()` and `SetSceneViewSize()`,
which hardcode exactly one offscreen target.

**Picking** reads `RenderScene` — ray against instance world bounds, hit mapped back through
`SourceId`. It does not re-derive visibility from the ECS, and it needs no GPU readback.

---

## 8. Invariants

| Condition | Behaviour |
|---|---|
| Exactly one view targets `main` | normal |
| Zero views target `main` | present a cleared frame; log once, do not crash — this is a legitimate state while loading |
| Two or more target `main` | highest priority wins, others dropped, one warning naming them |
| Target area is zero | view skipped entirely |
| Graph cycle | view dropped, named error, last known-good next frame |
| Unbound or mismatched output slot | instantiation rejected with the slot named |

---

## 9. What this changes on master

| Area | Today | After |
|---|---|---|
| `RenderComponent` | `IsVisible`, `Material` | adds `Layer` |
| Camera | `HedgehogCommon::Camera` class, no component | `CameraComponent` + `CameraSystem` |
| Renderer API | `DrawFrame`, `BeginGui`, `GetSceneViewTextureId`, `SetSceneViewSize` | `RenderFrame(scene, views)`, `CreateView`, `SyncResources` |
| Passes | six fixed classes owning per-frame GPU state | stateless builder functions over a graph |
| Targets | `ResourceManager` owns colour/depth/shadow/scene | target registry + pooled graph transients |
| ImGui | `GuiPass` inside the renderer | editor-supplied callback |
| Extraction | `FrameDataBuilder` → `FrameData` | extraction module → `RenderScene` |

The RHI already requests Vulkan 1.3 with `synchronization2` and `dynamicRendering` enabled, so
no device work is needed — but `IRHICommandList` still exposes only `BeginRenderPass` /
`EndRenderPass` and a single-texture `TransitionTexture`. The interface additions in Phase B are
real work.

---

## 10. Deferred, with no interface change required

- Per-light layer culling masks.
- Resource aliasing within a frame (the compiler already computes the lifetimes it needs).
- Async compute and real queue families.
- Parallel pass recording.
- Area lights — `LightType` has Direction, Point and Spot only.
- Per-instance material parameter overrides. `RenderComponent` selects a material *path*; the
  shader comes from the material asset.

---

## Implementation plan

28 tickets — 5 in phase A, 2 in B, 3 each in C, D and E, 8 in F, 4 in G. Every one leaves
`master` building and passing tests, and none exceeds ~1000 changed lines. Sizes are estimates; treat anything over ~700 as a candidate to split after a spike.

### Phase A — foundations (no renderer change, green by construction)

| # | Work | ~Lines |
|---|---|---|
| A0 | Land this document and reference it from `CLAUDE.md` | 440 |
| A1 | `Layer` index on `RenderComponent`; 32-entry layer-name table in `HedgehogSettings` with YAML persistence and an editor settings panel to rename them; inspector layer dropdown; serialization and default-layer migration for existing scenes | 280 |
| A2 | `CameraComponent` + `CameraSystem`, serialization, inspector including the named layer-mask checkboxes, a camera entity in `Default.yaml` | 320 |
| A3 | `RenderScene` POD types + extraction module, with tests covering bounds, `SourceId` round-trip and zero-allocation re-extraction | 550 |
| A4 | Repair `benchmark.yaml` so it renders real geometry; invalidate and re-measure the `PERFORMANCE.md` baseline table | 110 |

A4 is independent and can land at any point, but it **must** precede F4 — until it does, every
per-pass number in `PERFORMANCE.md` is measuring an empty scene.

### Phase B — RHI additions (purely additive; the old renderer keeps working)

| # | Work | ~Lines | Blocked by |
|---|---|---|---|
| B1 | Dynamic rendering: `BeginRendering`/`EndRendering`, attachment formats on the pipeline desc | 300 | — |
| B2 | Access-based `Barrier(...)`, `ResourceState`, texture views and subresource ranges, `GetDesc()` | 420 | B1 |

### Phase C — graph core (headless, no callers)

| # | Work | ~Lines | Blocked by |
|---|---|---|---|
| C1 | Graph types, versioned handles, builder verbs, graph description, and the headless test project | 600 | B2 |
| C2 | **Compiler** — cull, topological sort, lifetimes, barrier derivation, validation, with headless tests for each failure mode | 700 | C1 |
| C3 | Resource pool keyed by descriptor hash + runtime facade, arena-allocated pass data | 450 | C2 |

C2 is the highest-risk ticket in the plan and the most testable — device-free, no runtime
caller, so a defect surfaces in the test run rather than as a black screen. Review it carefully.

### Phase D — asset layer

| # | Work | ~Lines | Blocked by |
|---|---|---|---|
| D1 | Schema v1 types, parser, vocabulary resolution, validation — pure functions over data, with tests | 500 | C1 |
| D2 | Pass-builder registry, graph instantiation, **the C++ equivalence oracle from §6** | 450 | D1, C3 |
| D3 | Ship the `scene`, `game` and `result` graph assets; hot reload with last-known-good fallback | 300 | D2 |

### Phase E — targets and views

| # | Work | ~Lines | Blocked by |
|---|---|---|---|
| E1 | Render-target registry, size policies, deferred end-of-frame resize, zero-area handling | 400 | C3 |
| E2 | `View`, `ViewManager`, derivation from camera components, target resolution, lifetime | 450 | E1, A3 |
| E3 | Dependency ordering: read/write declaration, topological sort, cycle detection, tests | 300 | E2 |

### Phase F — cutover, flag-gated

| # | Work | ~Lines | Blocked by |
|---|---|---|---|
| F1 | Shadow and depth-prepass builders | 500 | C3, D2 |
| F2 | Forward pass builder | 300 | F1 |
| F3 | Shared phase: GPU scene upload + shadow atlas | 450 | F2 |
| F4 | `RenderFrame` **alongside** `DrawFrame`, behind a default-off setting; single view | 600 | F3, E3, A4 |
| F5 | `Editor --game-mode`: runs the new path headlessly, asserts no ImGui context exists | 300 | F4 |
| F6 | Multi-view loop; editor scene/game/result views; UI callback seam; stable texture ids | 550 | F5 |
| F7 | Layer-aware culling on the new path; gizmos and picking on the scene view only | 400 | F6 |
| F8 | **Flip the default and delete the flag** | 150 | F7 |

F4 is the hinge: both renderers coexist from F4 to F8, so Debug memory rises for that stretch.
F5 exercises the new path in CI from the moment it exists, which is what makes the coexistence
window safe. If coexistence proves untenable, make the flag a compile-time constant so only one
path is built.

### Phase G — remove the old path, strictly in this order

| # | Work | ~Lines |
|---|---|---|
| G1 | Delete `InitPass`, `PresentPass`, `GuiPass`, `ThreadContext` | 500 |
| G2 | Delete the legacy depth, forward and shadow passes and `RenderQueue` | 700 |
| G3 | Delete `ResourceManager` and `DrawFrame` | 500 |
| G4 | Delete the RHI render-pass / framebuffer / GUI-backend layer; `FrameData` and `FrameDataBuilder`; enforce the imgui boundary in CI | 600 |

Each of G1–G4 is green **only** because its predecessor removed the last reference. Do not
reorder them.

### Notes on sequencing

- A, B and the A3/B2 pair are independent tracks and can proceed in parallel.
- Nothing before F4 executes at runtime — C, D and E all land as compiled, tested, uncalled code.
- A4 repairs `benchmark.yaml`, which renders **zero meshes** on master. Until it lands, every
  per-pass number in `PERFORMANCE.md` measures an empty scene and no before/after figure quoted
  during the cutover means anything. It blocks F4 for exactly that reason.
