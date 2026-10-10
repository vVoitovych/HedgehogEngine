#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

#include "RHI/api/RHITypes.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogExtract/api/RenderScene.hpp"
#include "HedgehogExtract/api/UiDrawList.hpp"
#include "HedgehogMath/api/Matrix.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHIBuffer;
    class IRHIDescriptorPool;
    class IRHIDescriptorSetLayout;
    class IRHISampler;
}

namespace FS
{
    class FileSystemManager;
}

namespace HR
{
    class ResourceRegistry;
}

namespace Renderer
{
    // The renderer-owned GPU objects behind IGraphPassServices: the engine pipelines, built for
    // dynamic rendering, and per frame in flight a ring of viewProj uniforms (depth and shadow
    // passes), a ring of forward view uniforms (one camera each), the frame's scene lights and
    // the frame's joint palette.
    // The pass builders own none of this; they reach it through the GraphFrameContext.
    //
    // Frame protocol: BeginFrame(frameIndex) rewinds that frame slot's uniforms, which the fence
    // for that slot has already made safe to overwrite. Created and used from the frame-loop
    // cutover onward; nothing constructs it yet.
    class GraphPassServices final : public IGraphPassServices
    {
    public:
        // Uniform allocations one frame may make: a depth prepass and a gizmo pass per view plus up
        // to four shadow cascades, for several views.
        static constexpr uint32_t UNIFORMS_PER_FRAME = 64;
        // Forward view uniforms one frame may make: one per view.
        static constexpr uint32_t FORWARD_UNIFORMS_PER_FRAME = 8;
        // Scene-light uploads one frame makes: one, by the shared phase, for every view.
        static constexpr uint32_t SCENE_LIGHTS_PER_FRAME = 1;
        // Sampled-texture sets one frame may make: a tone map and a skybox per view, for several views.
        static constexpr uint32_t SAMPLED_TEXTURES_PER_FRAME = 16;
        // The joint palette's first capacity, in matrices; it grows on demand.
        static constexpr size_t MIN_PALETTE_CAPACITY = 256;
        // The game UI buffers' first capacities, in vertices and indices; they grow on demand.
        static constexpr size_t MIN_UI_VERTEX_CAPACITY = 4096;
        static constexpr size_t MIN_DEBUG_LINE_VERTEX_CAPACITY = 4096;
        static constexpr size_t MIN_UI_INDEX_CAPACITY  = 6144;

        GraphPassServices(RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);
        ~GraphPassServices() override;

        GraphPassServices(const GraphPassServices&)            = delete;
        GraphPassServices& operator=(const GraphPassServices&) = delete;
        GraphPassServices(GraphPassServices&&)                 = delete;
        GraphPassServices& operator=(GraphPassServices&&)      = delete;

        void BeginFrame(uint32_t frameIndex);

        // Gives the resource registry the material layout (the forward shader's set 1) to allocate
        // material sets from.
        void ProvideMaterialLayout(RHI::IRHIDevice& device, HR::ResourceRegistry& registry) const;

        // Gives the resource registry the GameUi shader's set 0 (one texture) to allocate UI texture
        // sets from.
        void ProvideUiTextureLayout(HR::ResourceRegistry& registry) const;

        const RHI::IRHIPipeline&      GetPipeline(EnginePipeline pipeline) const override;
        RHI::IRHIBuffer&              GetGizmoBoxLines() override;
        const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) override;
        const RHI::IRHIDescriptorSet& AllocateForwardViewUniform(const ForwardViewUniform& uniform) override;
        const RHI::IRHIDescriptorSet& AllocateSceneLightsUniform(const SceneLightsUniform& uniform) override;
        const RHI::IRHIDescriptorSet& AllocateSampledTexture(const RHI::IRHITexture& texture) override;
        const RHI::IRHIDescriptorSet& AllocateForwardLighting(const ShadowUniform& shadow,
                                                              const RHI::IRHITexture& shadowAtlas,
                                                              const ForwardEnvironment& environment) override;

        // Uploads the frame's RenderScene::JointMatrices into this frame slot's storage buffer, growing
        // it when too small, and returns the palette set the skinned pipelines bind (set 1 for the
        // depth prepass, set 4 for forward). Once per frame, after BeginFrame; nullptr when empty.
        const RHI::IRHIDescriptorSet* UploadJointPalette(std::span<const HM::Matrix4x4> matrices);

        // The frame's game UI geometry in this frame slot's buffers (GraphFrameData::UiVertices and
        // UiIndices). Both null when the list has no indices.
        struct UiGeometry
        {
            RHI::IRHIBuffer* Vertices = nullptr;
            RHI::IRHIBuffer* Indices  = nullptr;
        };

        // Uploads list's vertices and indices into this frame slot's buffers, growing them when too
        // small. Once per frame, after BeginFrame.
        UiGeometry UploadUiGeometry(const HX::UiDrawList& list);

        // Uploads the frame's debug lines into this frame slot's vertex buffer, growing it when too
        // small. Once per frame, after BeginFrame; null when there are none.
        RHI::IRHIBuffer* UploadDebugLines(std::span<const HX::DebugLineVertex> vertices);

    private:
        struct UniformSlot
        {
            std::unique_ptr<RHI::IRHIBuffer>        Buffer;
            std::unique_ptr<RHI::IRHIDescriptorSet> Set;
        };

        // A ring of uniform buffers per frame in flight, each with a descriptor set for its layout.
        struct UniformRing
        {
            std::unique_ptr<RHI::IRHIDescriptorSetLayout> Layout;
            std::unique_ptr<RHI::IRHIDescriptorPool>      Pool;
            std::vector<std::vector<UniformSlot>>         Slots; // [frame in flight][slot]
            uint32_t                                      Next = 0;
        };

        static void CreateRing(RHI::IRHIDevice& device, UniformRing& ring, const std::vector<RHI::DescriptorBinding>& layout,
                               uint32_t slotsPerFrame, size_t uniformSize);
        RHI::IRHIDescriptorSet& Allocate(UniformRing& ring, const void* data, size_t size);

        // One frame in flight's joint palette: a storage buffer of Capacity matrices and its set.
        struct PaletteSlot
        {
            std::unique_ptr<RHI::IRHIBuffer>        Buffer;
            std::unique_ptr<RHI::IRHIDescriptorSet> Set;
            size_t                                  Capacity = 0;
        };

        UniformRing m_ViewProjRing;
        UniformRing m_ForwardRing;
        UniformRing m_SceneLightsRing;
        // The forward shader's set 3: the shadow uniform, the atlas, the environment uniform, the
        // radiance cube and the BRDF table (the textures written as handed out).
        UniformRing                       m_LightingRing;
        std::unique_ptr<RHI::IRHISampler> m_ShadowSampler; // comparison, linear, clamped

        // Per frame in flight, SAMPLED_TEXTURES_PER_FRAME sets of one combined image sampler (the
        // ToneMap shader's set 0), rewritten as they are handed out, and the sampler they use.
        std::unique_ptr<RHI::IRHIDescriptorSetLayout>                     m_SampledTextureLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>                          m_SampledTexturePool;
        std::vector<std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>>> m_SampledTextureSets; // [frame][slot]
        uint32_t                                                          m_NextSampledTexture = 0;
        std::unique_ptr<RHI::IRHISampler>                                 m_LinearClampSampler;

        // The forward shader's set 1. Identical to the layout the resource registry allocates
        // material sets from, so those sets bind to these pipelines.
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_MaterialLayout;
        std::vector<RHI::PoolSize>                    m_MaterialPoolSizes; // for MAX_MATERIAL_COUNT sets

        // The skinned pipelines' palette set: one storage buffer.
        RHI::IRHIDevice&                                                  m_Device;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout>                     m_PaletteLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>                          m_PalettePool;
        std::array<PaletteSlot, HedgehogEngine::MAX_FRAMES_IN_FLIGHT>     m_Palettes;

        // One frame in flight's game UI vertex and index buffers.
        struct UiGeometrySlot
        {
            std::unique_ptr<RHI::IRHIBuffer> Vertices;
            std::unique_ptr<RHI::IRHIBuffer> Indices;
            size_t                           VertexCapacity = 0;
            size_t                           IndexCapacity  = 0;
        };

        std::unique_ptr<RHI::IRHIDescriptorSetLayout>                    m_UiTextureLayout; // GameUi's set 0
        std::array<UiGeometrySlot, HedgehogEngine::MAX_FRAMES_IN_FLIGHT> m_UiGeometry;

        // One frame in flight's debug line vertices.
        struct DebugLineSlot
        {
            std::unique_ptr<RHI::IRHIBuffer> Vertices;
            size_t                           Capacity = 0;
        };
        std::array<DebugLineSlot, HedgehogEngine::MAX_FRAMES_IN_FLIGHT> m_DebugLines;

        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ShadowPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardDoubleSidedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_GizmoPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassSkinnedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardSkinnedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardSkinnedDoubleSidedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ShadowSkinnedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_GameUiPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DebugLinesPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ToneMapPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_SkyboxPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassDoubleSidedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassSkinnedDoubleSidedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassCutoffPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassCutoffDoubleSidedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassCutoffSkinnedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassCutoffSkinnedDoubleSidedPipeline;
        std::unique_ptr<RHI::IRHIBuffer>   m_GizmoBoxLines;

        uint32_t m_FrameIndex = 0;
    };
}
