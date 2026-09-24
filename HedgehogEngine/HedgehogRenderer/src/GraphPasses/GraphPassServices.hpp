#pragma once

#include "HedgehogRenderer/Graph/GraphFrameContext.hpp"

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
    // The renderer-owned GPU objects behind IGraphPassServices: one pipeline per engine pass type,
    // built for dynamic rendering, and a ring of viewProj uniform buffers per frame in flight.
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

        GraphPassServices(RHI::IRHIDevice& device, const FS::FileSystemManager& fileSystem);
        ~GraphPassServices() override;

        GraphPassServices(const GraphPassServices&)            = delete;
        GraphPassServices& operator=(const GraphPassServices&) = delete;
        GraphPassServices(GraphPassServices&&)                 = delete;
        GraphPassServices& operator=(GraphPassServices&&)      = delete;

        void BeginFrame(uint32_t frameIndex);

        const RHI::IRHIPipeline&      GetPipeline(EnginePipeline pipeline) const override;
        const RHI::IRHIDescriptorSet& AllocateViewProjUniform(const HM::Matrix4x4& viewProj) override;

    private:
        struct UniformSlot
        {
            std::unique_ptr<RHI::IRHIBuffer>        Buffer;
            std::unique_ptr<RHI::IRHIDescriptorSet> Set;
        };

        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_ViewProjLayout;
        std::unique_ptr<RHI::IRHIDescriptorPool>      m_Pool;
        std::unique_ptr<RHI::IRHIPipeline>            m_DepthPrepassPipeline;
        std::unique_ptr<RHI::IRHIPipeline>            m_ShadowPipeline;

        std::vector<std::vector<UniformSlot>> m_Uniforms; // [frame in flight][slot]
        uint32_t                              m_FrameIndex = 0;
        uint32_t                              m_NextSlot   = 0;
    };
}
