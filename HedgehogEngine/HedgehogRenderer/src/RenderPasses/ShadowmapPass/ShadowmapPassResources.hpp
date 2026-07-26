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

    // Shared, immutable half of ShadowmapPass (see PassResourceCache): the depth-only render
    // pass, the pipeline, and the per-cascade descriptor-set layout. Constructed once via
    // PassResourceCache::GetOrCreate<ShadowmapPassResources>().
    class ShadowmapPassResources
    {
    public:
        explicit ShadowmapPassResources(const PassInitContext& init);
        ~ShadowmapPassResources();

        ShadowmapPassResources(const ShadowmapPassResources&)            = delete;
        ShadowmapPassResources& operator=(const ShadowmapPassResources&) = delete;
        ShadowmapPassResources(ShadowmapPassResources&&)                 = delete;
        ShadowmapPassResources& operator=(ShadowmapPassResources&&)      = delete;

        RHI::IRHIRenderPass&          GetRenderPass()    const { return *m_RenderPass; }
        RHI::IRHIPipeline&            GetPipeline()      const { return *m_Pipeline; }
        RHI::IRHIDescriptorSetLayout& GetShadowLayout()  const { return *m_ShadowmapLayout; }

        // Binding list for the per-cascade set, needed by each ShadowmapPass instance to size
        // its own descriptor pool (PipelineLoader::MakePoolSizes).
        const std::vector<RHI::DescriptorBinding>& GetShadowBindings() const { return m_ShadowBindings; }

        // Format the render pass was created with, cached so ShadowmapPass::Setup() can declare
        // its shadowDepth graph texture with a matching format.
        RHI::Format GetShadowFormat() const { return m_ShadowFormat; }

    private:
        std::unique_ptr<RHI::IRHIRenderPass>          m_RenderPass;
        std::unique_ptr<RHI::IRHIPipeline>            m_Pipeline;
        std::unique_ptr<RHI::IRHIDescriptorSetLayout> m_ShadowmapLayout;

        std::vector<RHI::DescriptorBinding> m_ShadowBindings;

        RHI::Format m_ShadowFormat = RHI::Format::Undefined;
    };
}
