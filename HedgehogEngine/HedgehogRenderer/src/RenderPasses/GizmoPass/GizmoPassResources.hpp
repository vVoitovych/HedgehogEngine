#pragma once

#include <memory>

namespace RHI
{
    class IRHIRenderPass;
    class IRHIPipeline;
}

namespace Renderer
{
    struct PassInitContext;

    // Shared, immutable half of GizmoPass (see PassResourceCache): the colour-only render pass
    // and the pipeline (GizmoPass uses no descriptor sets — the view-projection matrix is a
    // push constant). Constructed once via PassResourceCache::GetOrCreate<GizmoPassResources>().
    class GizmoPassResources
    {
    public:
        explicit GizmoPassResources(const PassInitContext& init);
        ~GizmoPassResources();

        GizmoPassResources(const GizmoPassResources&)            = delete;
        GizmoPassResources& operator=(const GizmoPassResources&) = delete;
        GizmoPassResources(GizmoPassResources&&)                 = delete;
        GizmoPassResources& operator=(GizmoPassResources&&)      = delete;

        RHI::IRHIRenderPass& GetRenderPass() const { return *m_RenderPass; }
        RHI::IRHIPipeline&   GetPipeline()   const { return *m_Pipeline; }

    private:
        std::unique_ptr<RHI::IRHIRenderPass> m_RenderPass;
        std::unique_ptr<RHI::IRHIPipeline>   m_Pipeline;
    };
}
