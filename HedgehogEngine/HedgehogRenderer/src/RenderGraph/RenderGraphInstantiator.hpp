#pragma once

#include <memory>
#include <vector>

namespace Renderer
{
    class IRenderPass;
    class RenderPassRegistry;
    struct PassInitContext;
    struct GraphAssetDesc;

    // Turns a parsed GraphAssetDesc into pass instances. Deliberately does not touch a RenderGraph
    // or RenderGraphBuilder: resource declarations stay pass-owned (each pass's own Setup() still
    // calls builder.CreateTexture/Write/Read/ReadSampled exactly as before Phase 5 — see
    // RenderGraphDesc.hpp) rather than being driven generically from `desc.Resources`. Instantiate
    // only decides WHICH passes exist and in WHAT ORDER; the caller (Renderer) still does
    // `pipeline->AddPass(std::move(pass))` for each and then `pipeline->Compile(...)`.
    class RenderGraphInstantiator
    {
    public:
        // Constructs one pass per node in `desc.Nodes`, in file order, appending each to
        // outPasses (any passes already in outPasses are left untouched). Returns false + LOGERROR
        // if a node's type fails to construct (a defensive backstop — RenderGraphLoader's V4
        // already rejects an unregistered type at parse time, so this should only fire if a pass
        // constructor itself fails against the real device, e.g. a shader compile error). On
        // failure, outPasses may contain the passes constructed before the failing node; the
        // caller is responsible for cleaning them up (they haven't been added to any graph yet).
        [[nodiscard]] static bool Instantiate(const GraphAssetDesc& desc,
                                              const RenderPassRegistry& registry,
                                              const PassInitContext& initCtx,
                                              std::vector<std::unique_ptr<IRenderPass>>& outPasses);
    };
}
