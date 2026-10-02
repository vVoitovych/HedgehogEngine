#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

#include "RHI/api/RHITypes.hpp"

#include "HedgehogCommon/api/RendererSettings.hpp"
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
        // The joint palette's first capacity, in matrices; it grows on demand.
        static constexpr size_t MIN_PALETTE_CAPACITY = 256;

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

        const RHI::IRHIPipeline&      GetPipeline(EnginePipeline pipeline) const override;
        RHI::IRHIBuffer&              GetGizmoBoxLines() override;
        const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) override;
        const RHI::IRHIDescriptorSet& AllocateForwardViewUniform(const ForwardViewUniform& uniform) override;
        const RHI::IRHIDescriptorSet& AllocateSceneLightsUniform(const SceneLightsUniform& uniform) override;

        // Uploads the frame's RenderScene::JointMatrices into this frame slot's storage buffer, growing
        // it when too small, and returns the palette set the skinned pipelines bind (set 1 for the
        // depth prepass, set 3 for forward). Once per frame, after BeginFrame; nullptr when empty.
        const RHI::IRHIDescriptorSet* UploadJointPalette(std::span<const HM::Matrix4x4> matrices);

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
        const RHI::IRHIDescriptorSet& Allocate(UniformRing& ring, const void* data, size_t size);

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

        // The forward shader's set 1. Identical to the layout the resource registry allocates
        // material sets from, so those sets bind to these pipelines.
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_MaterialLayout;
        std::vector<RHI::PoolSize>                    m_MaterialPoolSizes; // for MAX_MATERIAL_COUNT sets

        // The skinned pipelines' palette set: one storage buffer.
        RHI::IRHIDevice&                                                  m_Device;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout>                     m_PaletteLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>                          m_PalettePool;
        std::array<PaletteSlot, HedgehogEngine::MAX_FRAMES_IN_FLIGHT>     m_Palettes;

        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ShadowPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardDoubleSidedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_GizmoPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassSkinnedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardSkinnedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardSkinnedDoubleSidedPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ShadowSkinnedPipeline;
        std::unique_ptr<RHI::IRHIBuffer>   m_GizmoBoxLines;

        uint32_t m_FrameIndex = 0;
    };
}
