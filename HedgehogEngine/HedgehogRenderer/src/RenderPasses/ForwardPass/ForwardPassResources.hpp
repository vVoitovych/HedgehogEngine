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

    // Shared, immutable half of ForwardPass (see PassResourceCache): the render pass object, the
    // pipeline, both descriptor-set layouts, and the single ResourceRegistry::SetMaterialLayout
    // registration. Constructed once via PassResourceCache::GetOrCreate<ForwardPassResources>(),
    // regardless of how many per-view ForwardPass instances exist.
    class ForwardPassResources
    {
    public:
        explicit ForwardPassResources(const PassInitContext& init);
        ~ForwardPassResources();

        ForwardPassResources(const ForwardPassResources&)            = delete;
        ForwardPassResources& operator=(const ForwardPassResources&) = delete;
        ForwardPassResources(ForwardPassResources&&)                 = delete;
        ForwardPassResources& operator=(ForwardPassResources&&)      = delete;

        RHI::IRHIRenderPass&          GetRenderPass()     const { return *m_RenderPass; }
        RHI::IRHIPipeline&            GetPipeline()       const { return *m_Pipeline; }
        RHI::IRHIDescriptorSetLayout& GetFrameLayout()    const { return *m_FrameLayout; }

        // Set-0 (per-frame) binding list, needed by each ForwardPass instance to size its own
        // per-target descriptor pool (PipelineLoader::MakePoolSizes) — the pool itself stays
        // per-instance, not shared, so it is not part of this immutable resource set.
        const std::vector<RHI::DescriptorBinding>& GetFrameBindings() const { return m_FrameBindings; }

        // Formats the render pass was created with, cached here (rather than re-derived) so a
        // ForwardPass instance's Setup() can declare its own viewColor/viewDepth graph textures
        // with a format guaranteed to match this render pass's attachment formats.
        RHI::Format GetColorFormat() const { return m_ColorFormat; }
        RHI::Format GetDepthFormat() const { return m_DepthFormat; }

    private:
        std::unique_ptr<RHI::IRHIRenderPass>          m_RenderPass;
        std::unique_ptr<RHI::IRHIPipeline>            m_Pipeline;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_FrameLayout;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_MaterialLayout;

        std::vector<RHI::DescriptorBinding> m_FrameBindings;

        RHI::Format m_ColorFormat = RHI::Format::Undefined;
        RHI::Format m_DepthFormat = RHI::Format::Undefined;
    };
}
