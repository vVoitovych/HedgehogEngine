#pragma once

#include "RenderGraph/RenderGraph.hpp"
#include "RenderGraph/IRenderPass.hpp"

#include <memory>
#include <string>
#include <vector>

namespace RHI
{
    class IRHIDevice;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    class RenderResourceLedger;

    // Owns one RenderGraph instance AND the pass objects registered on it — the frame graph, each
    // view graph (via RenderView, which composes one of these), and the composition graph are all
    // just a RenderPipeline with a different set of passes and a different scope. See
    // workflow/current-plan.md, "Target Architecture".
    class RenderPipeline
    {
    public:
        explicit RenderPipeline(RenderResourceLedger& ledger);
        ~RenderPipeline();

        RenderPipeline(const RenderPipeline&)            = delete;
        RenderPipeline& operator=(const RenderPipeline&) = delete;
        RenderPipeline(RenderPipeline&&)                 = delete;
        RenderPipeline& operator=(RenderPipeline&&)      = delete;

        RenderGraph& GetGraph() { return m_Graph; }

        // Takes ownership and registers the pass with the graph (RenderGraph::AddPass). Call
        // every AddPass() before Compile() — Setup()/CreateFramebuffers() for all added passes
        // run together, once, inside Compile().
        void AddPass(std::unique_ptr<IRenderPass> pass);

        // Runs Setup() on every added pass, creates their declared transients, then
        // CreateFramebuffers() on every pass — see RenderGraph::Compile(). viewWidth/viewHeight are
        // only meaningful for a pipeline that declares a ViewRelative resource (a view graph); pass
        // 0 for the frame/composition pipelines, which never do.
        void Compile(RHI::IRHIDevice& device, uint32_t swapchainWidth, uint32_t swapchainHeight,
                     uint32_t viewWidth, uint32_t viewHeight);

        void Cleanup(RHI::IRHIDevice& device);

        // Forwarders to IRenderPass's optional per-frame/per-event hooks (see IRenderPass.hpp) —
        // Renderer reaches pass-specific behaviour through these instead of a concrete pass type,
        // since Phase 5 constructs passes through a type-erased RenderPassRegistry factory.
        void BeginFrame();
        void DiscardFrame();
        void NotifyViewsChanged(const std::vector<std::string>& viewOutputNames);
        void NotifySettingsDirty(RHI::IRHIDevice& device, const HedgehogSettings::Settings& settings);
        // Returns the first non-null result among owned passes, or nullptr if none match.
        void* GetViewTextureId(const std::string& viewOutputName) const;

    private:
        RenderGraph m_Graph;
        std::vector<std::unique_ptr<IRenderPass>> m_OwnedPasses;
    };
}
