#pragma once

#include "RenderGraphTypes.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstddef>
#include <string>

namespace Renderer
{
    class RenderGraph;

    // Thin recording facade handed to IRenderPass::Setup(). Every call is tagged with the
    // owning pass's index so the graph knows, per size class, which passes need
    // CreateFramebuffers() called again after an Invalidate().
    class RenderGraphBuilder
    {
    public:
        RenderGraphBuilder(RenderGraph& graph, size_t passIndex);

        // Declares a new transient texture, resolved by name. Called once by the pass that
        // owns (creates) the resource, before that same pass calls Write() on it.
        ResourceHandle CreateTexture(const std::string& name, const GraphTextureDesc& desc);

        // Declares that this pass writes the named resource, leaving it in finalLayoutAfterExecute
        // once Execute() returns — the graph uses this to track the texture's real layout so a
        // later ReadSampled() knows the correct barrier source layout.
        ResourceHandle Write(const std::string& name, RHI::ImageLayout finalLayoutAfterExecute);

        // Declares a read dependency for ordering/invalidation purposes only — the pass is
        // responsible for any layout transition it needs itself (e.g. a blit source).
        ResourceHandle Read(const std::string& name);

        // Declares a read dependency that the graph auto-transitions to ShaderReadOnly
        // immediately before this pass's Execute().
        ResourceHandle ReadSampled(const std::string& name);

    private:
        RenderGraph& m_Graph;
        size_t       m_PassIndex;
    };
}
