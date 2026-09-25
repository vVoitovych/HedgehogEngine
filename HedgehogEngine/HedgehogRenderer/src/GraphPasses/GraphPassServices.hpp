#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

#include "RHI/api/RHITypes.hpp"

#include <array>
#include <cstdint>
#include <memory>
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

namespace Renderer
{
    // The renderer-owned GPU objects behind IGraphPassServices: the engine pipelines, built for
    // dynamic rendering, and per frame in flight a ring of viewProj uniforms (depth and shadow
    // passes) and a ring of forward view uniforms (camera and lights).
    // The pass builders own none of this; they reach it through the GraphFrameContext.
    //
    // Frame protocol: BeginFrame(frameIndex) rewinds that frame slot's uniforms, which the fence
    // for that slot has already made safe to overwrite. Created and used from the frame-loop
    // cutover onward; nothing constructs it yet.
    class GraphPassServices final : public IGraphPassServices
    {
    public:
        // Uniform allocations one frame may make: a depth prepass per view plus up to four shadow
        // cascades per view, for several views.
        static constexpr uint32_t UNIFORMS_PER_FRAME = 64;
        // Forward view uniforms one frame may make: one per view.
        static constexpr uint32_t FORWARD_UNIFORMS_PER_FRAME = 8;

        GraphPassServices(RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);
        ~GraphPassServices() override;

        GraphPassServices(const GraphPassServices&)            = delete;
        GraphPassServices& operator=(const GraphPassServices&) = delete;
        GraphPassServices(GraphPassServices&&)                 = delete;
        GraphPassServices& operator=(GraphPassServices&&)      = delete;

        void BeginFrame(uint32_t frameIndex);

        const RHI::IRHIPipeline&      GetPipeline(EnginePipeline pipeline) const override;
        const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) override;
        const RHI::IRHIDescriptorSet& AllocateForwardViewUniform(const ForwardViewUniform& uniform) override;

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

        UniformRing m_ViewProjRing;
        UniformRing m_ForwardRing;

        // The forward shader's set 1. Identical to the layout the resource registry allocates
        // material sets from, so those sets bind to these pipelines.
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_MaterialLayout;

        std::unique_ptr<RHI::IRHIPipeline> m_DepthPrepassPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ShadowPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardPipeline;
        std::unique_ptr<RHI::IRHIPipeline> m_ForwardDoubleSidedPipeline;

        uint32_t m_FrameIndex = 0;
    };
}
