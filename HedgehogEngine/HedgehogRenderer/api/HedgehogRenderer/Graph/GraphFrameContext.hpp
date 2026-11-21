#pragma once

#include "EnvironmentUniform.hpp"
#include "ParticleUniform.hpp"
#include "RGTypes.hpp"
#include "UiCallback.hpp"

#include "HedgehogRenderer/Views/ViewCulling.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogMath/api/Matrix.hpp"
#include "HedgehogMath/api/Vector.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace RHI
{
    class IRHIBuffer;
    class IRHIDescriptorSet;
    class IRHIPipeline;
    class IRHITexture;
}

namespace Renderer
{
    struct ShadowCascades;
    struct ShadowUniform;
    struct ParticleFrameData;

    // The particle pool's buffers as the shared phase's simulation left them in this frame's graph
    // (SharedPhaseOutputs), for the views' Particles passes to read: the states, the alive list the
    // simulation filled, the slot rows with their draw arguments, and the sort list each view's
    // Particles pass sorts its alpha-blended particles into (its latest version is the one to write),
    // and the collision depth the shadow view's Particles pass copies its depth into.
    struct ParticleGraphBuffers
    {
        RGBuffer State;
        RGBuffer AliveOut;
        RGBuffer Slots;
        RGBuffer Sort;
        RGBuffer CollisionDepth;
    };

    // The particles' DepthBuffer collision reads the depth of the shadow view's camera as the frame
    // before saw it: a PARTICLE_COLLISION_DEPTH_SIZE square of depths over that view, copied by its
    // Particles pass, which also records the view-projection it was seen with here.
    inline constexpr uint32_t PARTICLE_COLLISION_DEPTH_SIZE = 512;
    struct ParticleCollisionDepth
    {
        HM::Matrix4x4 ViewProj;
        bool          Written = false; // copied this frame, so the next frame may collide with it
    };

    // The mesh itself and up to three levels of detail.
    inline constexpr uint32_t MAX_MESH_LODS = 4;

    // One level of detail's indices in the shared index buffer.
    struct MeshLodRange
    {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
    };

    // Where one mesh's indices sit in the shared geometry buffers: FirstIndex and IndexCount are LOD 0,
    // the mesh itself (LodRanges[0]), and LodRanges[1 .. LodCount - 1] its levels of detail, all over
    // the same vertices at VertexOffset.
    struct MeshDrawRange
    {
        uint32_t                                  FirstIndex   = 0;
        uint32_t                                  IndexCount   = 0;
        uint32_t                                  VertexOffset = 0;
        std::array<MeshLodRange, MAX_MESH_LODS>   LodRanges{};
        uint32_t                                  LodCount     = 1;
    };

    // A level of detail's range, clamped to the mesh's coarsest level (LOD 0 for a mesh without).
    [[nodiscard]] inline MeshLodRange GetLodRange(const MeshDrawRange& mesh, uint32_t lod)
    {
        if (lod == 0 || mesh.LodCount <= 1)
            return { mesh.FirstIndex, mesh.IndexCount };
        return mesh.LodRanges[std::min(lod, std::min(mesh.LodCount, MAX_MESH_LODS) - 1)];
    }

    // Everything the engine passes read about one view's frame, filled by the frame loop. Plain
    // data: no pass owns it, and nothing here outlives the frame.
    // How the frame is finished after lighting, from RenderScene::PostProcess (these defaults without
    // one: FXAA alone). Nothing reads it yet.
    struct PostProcessSettings
    {
        bool        Fxaa                 = true;
        bool        Ssao                 = false;
        float       SsaoRadius           = 0.5f;
        float       SsaoIntensity        = 1.0f;
        float       SsaoBias             = 0.025f;
        bool        Bloom                = false;
        float       BloomIntensity       = 0.05f;
        float       BloomThreshold       = 1.0f;
        float       BloomKnee            = 0.5f;
        std::string ColorLutPath;        // empty: no grading
        float       ColorLutContribution = 1.0f;
    };

    struct GraphFrameData
    {
        HM::Matrix4x4 View;
        HM::Matrix4x4 Proj;
        HM::Vector3   EyePosition = HM::Vector3(0.0f, 0.0f, 0.0f);
        float         NearPlane = 0.1f;
        float         FarPlane  = 1000.0f;

        // The shadow-casting light's direction; the shadow pass falls back to +X without one.
        std::optional<HM::Vector3> ShadowLightDirection;
        uint32_t                   ShadowCascadeCount       = 1;
        float                      ShadowCascadeSplitLambda = 0.5f;

        // The shadow frame's cascades, computed once by the shared phase, which the Shadow pass
        // renders (it computes them itself when null, as in headless tests).
        const ShadowCascades* Cascades = nullptr;
        // Every view's: how its forward pass reads the sun's shadow from the atlas (the same
        // cascades, SharedPhaseOutputs::Shadow). Null when the frame has no shadow view: unshadowed.
        const ShadowUniform* Shadow = nullptr;

        // Opaque instances only: what the depth prepass, shadow and forward passes draw. For a view,
        // those on a layer in its mask whose bounds its frustum can see (ViewCulling.hpp), and only
        // the rigid ones: a view's skinned instances are in SkinnedInstances. The shadow view keeps
        // every scene instance here, skinned ones too, drawn in their bind pose.
        std::span<const HX::RenderInstance> OpaqueInstances;

        // A view's opaque instances with a joint palette (RenderInstance::JointCount > 0), culled like
        // OpaqueInstances. The depth prepass and forward pass draw them after the rigid ones, with the
        // skinned pipelines and JointPalette.
        std::span<const HX::RenderInstance> SkinnedInstances;

        // A view's instances whose material is Cutoff (alpha-tested), rigid and skinned, culled like
        // OpaqueInstances: the depth prepass draws them with the cutoff pipelines, which discard
        // below the material's cutoff, and the forward pass after the opaque ones.
        std::span<const HX::RenderInstance> CutoffInstances;
        std::span<const HX::RenderInstance> SkinnedCutoffInstances;

        // A view's instances whose material is Transparent, rigid and skinned, back to front from
        // the view (SortBackToFront): only the ForwardTransparent pass draws them, blended.
        std::span<const HX::RenderInstance> TransparentInstances;
        std::span<const HX::RenderInstance> SkinnedTransparentInstances;

        // Each material's draw info, indexed by RenderInstance::MaterialIndex: its alpha mode and
        // whether it is double-sided, which picks the passes' pipelines without back-face culling.
        std::span<const MaterialDrawInfo> Materials;

        // The frame's RenderScene::JointMatrices, uploaded once as a storage buffer (the skinned
        // pipelines' palette set). Null when no instance is skinned.
        const RHI::IRHIDescriptorSet* JointPalette = nullptr;

        // The view's editor-layer instances (HX::EDITOR_LAYER), culled like OpaqueInstances: only the
        // Gizmo pass reads them, drawing their bounds. Never scene geometry, never shadow casters.
        std::span<const HX::RenderInstance> OverlayInstances;

        // A view's terrain tiles (RenderInstance::TerrainIndex >= 0), culled like OpaqueInstances:
        // the depth prepass draws them as opaque instances, the forward pass with the terrain
        // pipeline and TerrainSets[TerrainIndex]. The shadow casts them among its opaque instances.
        std::span<const HX::RenderInstance> TerrainInstances;
        // Each terrain's set (the terrain pipeline's set 1), by TerrainIndex; null for a terrain the
        // registry has no set for. Owned by the resource registry.
        std::span<const RHI::IRHIDescriptorSet* const> TerrainSets;

        // Shared geometry. Meshes is indexed by RenderInstance::MeshIndex. TexCoords, Normals and
        // Tangents (float4: xyz along increasing u, w the handedness) are only read by the forward pass.
        RHI::IRHIBuffer*              Positions = nullptr;
        RHI::IRHIBuffer*              TexCoords = nullptr;
        RHI::IRHIBuffer*              Normals   = nullptr;
        RHI::IRHIBuffer*              Tangents  = nullptr;
        RHI::IRHIBuffer*              Indices   = nullptr;
        // Skinning streams, aligned with Positions: four joint indices (uint4) and four weights
        // (float4) per vertex, zero for static meshes.
        RHI::IRHIBuffer*              Joints    = nullptr;
        RHI::IRHIBuffer*              Weights   = nullptr;
        std::span<const MeshDrawRange> Meshes;

        // Each material's descriptor set (the forward shader's set 1), indexed by
        // RenderInstance::MaterialIndex. Owned by the resource registry.
        std::span<const RHI::IRHIDescriptorSet* const> MaterialSets;

        // The frame's lights (the forward shader's set 2), uploaded once by the shared phase for
        // every view (SharedPhaseOutputs::SceneLights).
        const RHI::IRHIDescriptorSet* SceneLights = nullptr;

        // The Ui pass: the application's callback, and the render targets the view reads (its
        // ViewDesc::Reads, as written earlier this frame). The Ui pass declares each as sampled, so
        // the compiler orders their writers first and makes them readable to the UI.
        UiCallback                 Ui;
        std::span<const RGTexture> UiSampledTargets;

        // The game UI (RenderScene::Ui), drawn by the GameUi pass over the view it is in: the frame's
        // draw commands, its vertices and 16-bit indices uploaded once per frame, a texture set per
        // RenderScene::UiTextures entry (indexed by UiDrawCommand::Texture; null for one not loaded)
        // and the white texture a solid fill samples; and a set per RenderScene::UiFonts entry (indexed
        // by a UI_FONT_TEXTURE command's low bits; null for a font without one, whose text is not
        // drawn). Positions are pixels of a UiTargetSize target, stretched over whatever colour target
        // the pass draws into.
        std::span<const HX::UiDrawCommand>             UiCommands;
        RHI::IRHIBuffer*                               UiVertices = nullptr;
        RHI::IRHIBuffer*                               UiIndices  = nullptr;
        std::span<const RHI::IRHIDescriptorSet* const> UiTextureSets;
        std::span<const RHI::IRHIDescriptorSet* const> UiFontSets;
        const RHI::IRHIDescriptorSet*                  UiSolidTexture = nullptr;
        HM::Vector2                                    UiTargetSize   = HM::Vector2(0.0f, 0.0f);

        // RenderScene::DebugLines, uploaded once per frame (HX::DebugLineVertex, two per line), which
        // the Gizmo pass draws after the overlay boxes. Null and 0 when there are none.
        RHI::IRHIBuffer* DebugLineVertices    = nullptr;
        uint32_t         DebugLineVertexCount = 0;

        // The exposure the ToneMap pass scales the view's HDR radiance by, in EV (2^Exposure):
        // RenderScene::Environment's, or 0 without an environment.
        float Exposure = 0.0f;

        // The scene's post-processing settings.
        PostProcessSettings PostProcess;
        // The colour grading LUT ToneMap reads (ColorLut.hpp's layout): the scene's, or the neutral one
        // without a usable one; null outside the renderer (headless tests), which grades nothing.
        const RHI::IRHITexture* ColorLut = nullptr;

        // The scene's image-based lighting, which the Forward pass binds in set 3 beside the shadow:
        // the baked RenderScene::Environment, or the black fallback without one (no ambient light).
        ForwardEnvironment Environment;

        // The view's screen-space ambient occlusion (R, 1 unoccluded), which the Forward pass multiplies
        // into the ambient light; null for none, which binds white.
        const RHI::IRHITexture* AmbientOcclusion = nullptr;

        // The frame's particles (the shared phase's ParticleFrameData) and its buffers in the graph;
        // null without particles, and the Particles pass then draws nothing.
        const ParticleFrameData* Particles = nullptr;
        ParticleGraphBuffers     ParticleBuffers;
        // Set on the shadow view's frame alone: where its Particles pass copies its depth to, when an
        // emitter collides with the depth buffer.
        ParticleCollisionDepth* ParticleCollision = nullptr;
    };

    // The graph-path forward shader's uniforms (GraphForward/Base.vert and .frag), laid out for
    // std140: set 0 is per view, set 2 is shared by every view.
    struct GpuLight
    {
        alignas(16) HM::Vector3 Position;
        alignas(16) HM::Vector3 Direction;
        alignas(16) HM::Vector3 Color;
        alignas(16) HM::Vector4 Data; // (type, intensity, radius, cos(coneAngle))
    };

    struct ForwardViewUniform
    {
        alignas(16) HM::Matrix4x4 View;
        alignas(16) HM::Matrix4x4 ViewProj;
        alignas(16) HM::Vector3   EyePosition;
        // 1 / the view's width and height in x and y, turning gl_FragCoord into a screen UV.
        alignas(16) HM::Vector4   InverseViewport = HM::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
    };

    struct SceneLightsUniform
    {
        alignas(16) GpuLight Lights[HedgehogEngine::MAX_LIGHTS_COUNT];
        int32_t              LightCount = 0;
    };

    // Packs one view's camera into the forward shader's set 0.
    [[nodiscard]] ForwardViewUniform MakeForwardViewUniform(const GraphFrameData& frame, uint32_t viewportWidth = 0,
                                                            uint32_t viewportHeight = 0);

    // Packs the frame's lights into the forward shader's set 2. Lights past MAX_LIGHTS_COUNT are
    // dropped, as the legacy forward pass did.
    [[nodiscard]] SceneLightsUniform MakeSceneLightsUniform(std::span<const HX::RenderLight> lights);

    // The skinned pipelines' push constants: the rigid ones' model matrix, then where the
    // instance's palette starts in the frame's JointPalette.
    struct SkinnedPushConstants
    {
        float    Model[16];
        uint32_t PaletteOffset = 0;
    };
    static_assert(sizeof(SkinnedPushConstants) == 68, "The skinned shaders' push constant block is 68 bytes.");

    [[nodiscard]] SkinnedPushConstants MakeSkinnedPushConstants(const HX::RenderInstance& instance);

    enum class EnginePipeline
    {
        DepthPrepass,
        Shadow,
        Forward,            // back faces culled
        ForwardDoubleSided, // Forward with cullBackFaces: false
        DepthPrepassSkinned,        // DepthPrepass for SkinnedInstances: palette at set 1
        ForwardSkinned,             // Forward for SkinnedInstances: palette at set 4
        ForwardSkinnedDoubleSided,  // ForwardSkinned with cullBackFaces: false
        ShadowSkinned,              // Shadow for SkinnedInstances: palette at set 1
        GameUi,                     // UiDrawList quads, alpha-blended, no depth: texture at set 0
        DebugLines,                 // DebugLineVertex lines, depth-tested, not written: viewProj at set 0
        ToneMap,                    // a fullscreen triangle, no depth: the sampled HDR texture at set 0
        Skybox,                     // a fullscreen triangle at the far plane, depth-tested, not written: the cube at set 0
        DepthPrepassDoubleSided,        // DepthPrepass without back-face culling
        DepthPrepassSkinnedDoubleSided, // DepthPrepassSkinned without back-face culling
        DepthPrepassCutoff,             // DepthPrepass for CutoffInstances: UVs, the material at set 1, discarding below its cutoff
        DepthPrepassCutoffDoubleSided,  // DepthPrepassCutoff without back-face culling
        DepthPrepassCutoffSkinned,      // DepthPrepassCutoff for SkinnedCutoffInstances: palette at set 1, material at set 2
        DepthPrepassCutoffSkinnedDoubleSided, // DepthPrepassCutoffSkinned without back-face culling
        ShadowDoubleSided,              // Shadow without back-face culling
        ShadowSkinnedDoubleSided,       // ShadowSkinned without back-face culling
        ShadowCutoff,                   // Shadow for CutoffInstances: the depth prepass's cutoff layout and shaders
        ShadowCutoffDoubleSided,        // ShadowCutoff without back-face culling
        ShadowCutoffSkinned,            // ShadowCutoff for SkinnedCutoffInstances: palette at set 1, material at set 2
        ShadowCutoffSkinnedDoubleSided, // ShadowCutoffSkinned without back-face culling
        ForwardTransparent,                 // Forward for TransparentInstances: alpha-blended, depth tested, not written
        ForwardTransparentBackFaces,        // ForwardTransparent culling the front faces: a double-sided one's far side
        ForwardTransparentSkinned,          // ForwardTransparent for SkinnedTransparentInstances: palette at set 4
        ForwardTransparentSkinnedBackFaces, // ForwardTransparentSkinned culling the front faces
        SelectionMask,        // overlay instances as solid white into an R8 mask: no depth, no culling
        SelectionMaskSkinned, // SelectionMask for skinned overlay instances: palette at set 1
        SelectionOutline,     // a fullscreen triangle blending the outline over the colour: the mask at set 0
        ForwardTerrain,       // Forward for TerrainInstances: the terrain's layers at set 1
        ParticlesAlpha,       // particle billboards over the HDR target, alpha-blended, depth tested, not written
        ParticlesAdditive,    // ParticlesAlpha added to what is behind
        Fxaa,                 // a fullscreen triangle, no depth (PostProcess.pl): the tone-mapped image at set 0
        Ssao,                 // a fullscreen triangle into R8 (Ssao.pl): the view's depth at set 0
        SsaoBlur,             // a fullscreen triangle into R8 (PostProcess.pl): the raw occlusion at set 0, depth at 1
        BloomDownsample,      // a fullscreen triangle into HDR (PostProcess.pl): the level below at set 0
        BloomUpsample,        // BloomDownsample's, added one to one onto the level above
    };

    // The particle compute shaders (ParticleCompute.pl), all on one layout.
    enum class EngineComputePipeline
    {
        ParticleReset,    // one emitter instance's slot afresh: every particle dead
        ParticleEmit,     // its SpawnCount newborns into this frame's alive list
        ParticleSimulate, // its alive particles moved on, the living into the next frame's list
        ParticleFinish,   // every emitter's counters and draw arguments for the next frame
        ParticleSortKeys, // a view's sort list for one alpha emitter: its alive particles by distance
        ParticleSortStep, // one step of the bitonic network over that list
        ParticleCopyDepth, // the shadow view's depth (set 1, a sampled texture) into the collision depth
        ParticleSpawnFromDeaths, // a sub-emitter's particles where its parent's particles died
    };
    inline constexpr size_t ENGINE_COMPUTE_PIPELINE_COUNT = 8;

    // The pool's buffers in the order ParticleFrameData::Buffers and ::States list them.
    enum class ParticleBuffer : uint8_t
    {
        State,
        DeadList,
        AliveIn,
        AliveOut,
        Slots,
        Sort, // per alpha emitter, (key, particle) pairs at twice its slot's first particle
        CollisionDepth, // PARTICLE_COLLISION_DEPTH_SIZE squared floats, row 0 the top
    };
    inline constexpr size_t PARTICLE_BUFFER_COUNT = 7;

    // One frame of the particle pool: the pool's buffers (this frame's alive list in AliveIn, the next
    // frame's in AliveOut) and the state the frames before left each in, which the shared phase's
    // ParticleSimulate pass imports them with; and per emitter instance its uniform, whether its slot
    // starts afresh, and its texture's set (null for none: a soft round dot), which the views'
    // Particles passes draw with.
    // One sub-emitter of an emitter instance this frame: Count particles of the uniform Child where
    // each particle of the uniform Parent died. Depth is the parent's emitter's depth in its effect's
    // sub-emitter tree (0 for one no emitter spawns).
    struct ParticleSubEmitterDispatch
    {
        uint32_t Parent = 0;
        uint32_t Child  = 0;
        uint32_t Count  = 1;
        uint32_t Depth  = 0;

        bool operator==(const ParticleSubEmitterDispatch&) const = default;
    };

    struct ParticleFrameData
    {
        std::array<RHI::IRHIBuffer*, PARTICLE_BUFFER_COUNT>    Buffers{};
        std::array<RHI::ResourceState, PARTICLE_BUFFER_COUNT> States{};
        std::span<const ParticleEmitterUniform>               Emitters;
        std::span<const uint8_t>                              Reset; // one per emitter: 1 to start its slot afresh
        std::span<const RHI::IRHIDescriptorSet* const>        Textures;
        // Deepest parent first, so a parent's deaths are read before its own parent spawns into it.
        std::span<const ParticleSubEmitterDispatch>           SubEmitters;

        [[nodiscard]] RHI::IRHIBuffer* Get(ParticleBuffer buffer) const { return Buffers[static_cast<size_t>(buffer)]; }
    };

    // The long-lived GPU objects the engine passes use but do not own: pipelines, and per-frame
    // rings of uniform buffers. Implemented by GraphPassServices (outside src/Graph, so the headless
    // tests never need a device); tests substitute a fake.
    class IGraphPassServices
    {
    public:
        virtual ~IGraphPassServices() = default;

        virtual const RHI::IRHIPipeline& GetPipeline(EnginePipeline pipeline) const = 0;

        // A descriptor set (set 0, binding 0) holding viewProj, valid until this frame slot comes
        // round again. One allocation per draw pass (or per shadow cascade) per frame.
        virtual const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) = 0;

        // The forward pass's set 0: one view's camera. One allocation per forward pass per frame,
        // from its own per-frame ring.
        virtual const RHI::IRHIDescriptorSet& AllocateForwardViewUniform(const ForwardViewUniform& uniform) = 0;

        // The forward pass's set 2: the frame's lights. One allocation per frame, by the shared
        // phase; every view's forward pass binds the same set.
        virtual const RHI::IRHIDescriptorSet& AllocateSceneLightsUniform(const SceneLightsUniform& uniform) = 0;

        // The forward pass's set 3: the shadow uniform and the shadow atlas, read through a comparison
        // sampler, then the environment's uniform, prefiltered radiance cube and BRDF table, then the
        // screen-space ambient occlusion (a 1x1 white texture when null). One allocation per forward
        // pass per frame, from its own per-frame ring.
        virtual const RHI::IRHIDescriptorSet& AllocateForwardLighting(const ShadowUniform& shadow,
                                                                      const RHI::IRHITexture& shadowAtlas,
                                                                      const ForwardEnvironment& environment,
                                                                      const RHI::IRHITexture* occlusion) = 0;

        // A descriptor set (set 0, binding 0) sampling texture linearly, clamped to its edge, valid
        // until this frame slot comes round again: for a texture only known at execute time, as a
        // graph's transients are. One allocation per pass that samples one, per frame.
        virtual const RHI::IRHIDescriptorSet& AllocateSampledTexture(const RHI::IRHITexture& texture) = 0;

        virtual const RHI::IRHIPipeline& GetComputePipeline(EngineComputePipeline pipeline) const = 0;

        // The particle compute shaders' set 0: particles' buffers and its emitters, uploaded into this
        // frame slot's emitter buffer. Once per frame, by the shared phase.
        virtual const RHI::IRHIDescriptorSet& AllocateParticleSet(const ParticleFrameData& particles) = 0;
        // The set AllocateParticleSet made this frame, which the Particles passes draw with; null before.
        virtual const RHI::IRHIDescriptorSet* GetParticleSet() const = 0;
    };

    // Attached to a RenderGraphRuntime for one frame (SetFrameContext). Pass builders capture a
    // pointer to it in their arena pass data; with none attached, as in the headless tests, their
    // execute bodies record nothing, while their declarations are unchanged.
    struct GraphFrameContext
    {
        IGraphPassServices*   Services = nullptr;
        const GraphFrameData* Frame    = nullptr;
    };
}
