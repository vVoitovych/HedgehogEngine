#pragma once

#include "RenderGraph/IRenderPass.hpp"

#include "HedgehogCommon/api/Frame/FrameData.hpp"

#include <HedgehogMath/api/Matrix.hpp>

#include <memory>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHIFramebuffer;
    class IRHIDescriptorPool;
    class IRHIDescriptorSet;
    class IRHIBuffer;
}

namespace Renderer
{
    class DepthPrePassResources;
    struct PassInitContext;

    // View-stage pass: early-Z depth prepass into this view's own viewDepth target. One instance
    // per view (constructed fresh for each RenderView — see Renderer::CreateView); the shared,
    // immutable half (render pass/pipeline/layout) still comes from the cache (PassResourceCache),
    // so N view instances share one pipeline without re-registering anything.
    class DepthPrePass : public IRenderPass
    {
    public:
        explicit DepthPrePass(const PassInitContext& init);
        ~DepthPrePass() override;

        const char* GetName() const override { return "DepthPrePass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        struct DepthPrepassFrameUniform
        {
            alignas(16) HM::Matrix4x4 ViewProj;
        };

    private:
        // Shared, immutable half (render pass, pipeline, frame descriptor-set layout) — see
        // PassResourceCache.
        std::shared_ptr<const DepthPrePassResources> m_Resources;

        std::unique_ptr<RHI::IRHIDescriptorPool> m_FramePool;
        std::unique_ptr<RHI::IRHIFramebuffer>    m_FrameBuffer;

        std::vector<std::unique_ptr<RHI::IRHIBuffer>>        m_FrameUniforms; // [frame]
        std::vector<std::unique_ptr<RHI::IRHIDescriptorSet>> m_FrameSets;     // [frame]
    };

}
