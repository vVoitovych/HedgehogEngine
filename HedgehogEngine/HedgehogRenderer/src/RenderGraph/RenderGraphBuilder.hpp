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
    // CreateFramebuffers() called again after an Invalidate() or a RefreshImports().
    class RenderGraphBuilder
    {
    public:
        RenderGraphBuilder(RenderGraph& graph, size_t passIndex);

        // Declares a new transient texture, resolved by name within this graph's own scope.
        // Called once by the pass that owns (creates) the resource, before that same pass calls
        // Write() on it.
        ResourceHandle CreateTexture(const std::string& name, const GraphTextureDesc& desc);

        // Declares that this pass writes a resource DECLARED BY THIS GRAPH, leaving it in
        // finalLayoutAfterExecute once Execute() returns — the graph uses this to keep the
        // resource ledger's tracked layout correct so a later ReadSampled()/ImportReadSampled()
        // knows the right barrier source layout.
        ResourceHandle Write(const std::string& name, RHI::ImageLayout finalLayoutAfterExecute);

        // Declares a read dependency (on a resource declared by this graph) for ordering/
        // invalidation purposes only — the pass is responsible for any layout transition it
        // needs itself (e.g. a blit source).
        ResourceHandle Read(const std::string& name);

        // Declares a read dependency (on a resource declared by this graph) that the graph
        // auto-transitions to ShaderReadOnly immediately before this pass's Execute().
        ResourceHandle ReadSampled(const std::string& name);

        // Cross-scope variants: scopedName is the fully qualified ledger key of a resource
        // declared by a DIFFERENT graph instance (e.g. "shared/shadowDepth", "game/viewColor")
        // rather than a name local to this graph — see RenderGraph::SetScope / ImportTexture.
        ResourceHandle ImportWrite(const std::string& scopedName, RHI::ImageLayout finalLayoutAfterExecute);
        ResourceHandle ImportRead(const std::string& scopedName);
        ResourceHandle ImportReadSampled(const std::string& scopedName);

    private:
        RenderGraph& m_Graph;
        size_t       m_PassIndex;
    };
}
