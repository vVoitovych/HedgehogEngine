#pragma once

#include "RHI/api/RHITypes.hpp"

#include <memory>
#include <vector>

namespace RHI
{
    class IRHIRenderPass;
    class IRHIPipeline;
    class IRHIDescriptorSetLayout;
}

namespace Renderer
{
    struct PassInitContext;

    // Shared, immutable half of DepthPrePass (see PassResourceCache): the depth-only render
    // pass, the pipeline, and the frame descriptor-set layout. Constructed once via
    // PassResourceCache::GetOrCreate<DepthPrePassResources>(), regardless of how many per-view
    // DepthPrePass instances exist.
    class DepthPrePassResources
    {
    public:
        explicit DepthPrePassResources(const PassInitContext& init);
        ~DepthPrePassResources();

        DepthPrePassResources(const DepthPrePassResources&)            = delete;
        DepthPrePassResources& operator=(const DepthPrePassResources&) = delete;
        DepthPrePassResources(DepthPrePassResources&&)                 = delete;
        DepthPrePassResources& operator=(DepthPrePassResources&&)      = delete;

        RHI::IRHIRenderPass&          GetRenderPass()  const { return *m_RenderPass; }
        RHI::IRHIPipeline&            GetPipeline()    const { return *m_Pipeline; }
        RHI::IRHIDescriptorSetLayout& GetFrameLayout() const { return *m_FrameLayout; }

        // Set-0 (per-frame) binding list, needed by each DepthPrePass instance to size its own
        // per-target descriptor pool (PipelineLoader::MakePoolSizes).
        const std::vector<RHI::DescriptorBinding>& GetFrameBindings() const { return m_FrameBindings; }

        // Format the render pass was created with, cached so DepthPrePass::Setup() can declare
        // its viewDepth graph texture with a matching format.
        RHI::Format GetDepthFormat() const { return m_DepthFormat; }

    private:
        std::unique_ptr<RHI::IRHIRenderPass>          m_RenderPass;
        std::unique_ptr<RHI::IRHIPipeline>            m_Pipeline;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_FrameLayout;

        std::vector<RHI::DescriptorBinding> m_FrameBindings;

        RHI::Format m_DepthFormat = RHI::Format::Undefined;
    };
}
