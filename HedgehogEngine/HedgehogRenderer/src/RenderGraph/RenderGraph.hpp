#pragma once

#include "RenderGraphTypes.hpp"

#include "RHI/api/RHITypes.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RHI
{
    class IRHIDevice;
    class IRHITexture;
    class IRHICommandList;
}

namespace Renderer
{
    class IRenderPass;
    class RenderGraphResourcePool;
    class FrameStats;

    // Orchestrates a small, linear (non-scheduling) render graph: passes run in registration
    // order, the graph owns every transient texture (keyed by size class) and the current
    // layout of each, and a single Invalidate(sizeClass) call recreates that class's textures
    // and rebuilds exactly the passes that reference them.
    class RenderGraph
    {
    public:
        RenderGraph();
        ~RenderGraph();

        RenderGraph(const RenderGraph&)            = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;
        RenderGraph(RenderGraph&&)                 = delete;
        RenderGraph& operator=(RenderGraph&&)      = delete;

        // Passes execute in the order they are added. Ownership stays with the caller
        // (RenderQueue owns the passes via unique_ptr); the graph only holds observer pointers.
        void AddPass(IRenderPass* pass);

        // Runs Setup() on every pass to record the resource graph, creates every declared
        // transient at its size class, then calls CreateFramebuffers() on every pass.
        void Compile(RHI::IRHIDevice& device,
                     uint32_t swapchainWidth, uint32_t swapchainHeight,
                     uint32_t sceneViewWidth, uint32_t sceneViewHeight);

        // Registers a single pass like AddPass, then immediately runs its
        // Setup()+CreateFramebuffers() without touching already-created transients — used
        // because the graph's transients are declared+allocated up front by an earlier
        // zero-pass Compile() call (see Renderer::Renderer), so each pass's Setup() only
        // resolves existing handles via Read/Write/ReadSampled, never CreateTexture.
        void AddAndCompilePass(IRenderPass* pass, RHI::IRHIDevice& device);

        void Update(const RenderGraphContext& ctx);

        // Runs every registered pass in order (see ExecutePass for the per-pass barrier/tracking/
        // profiling steps this applies to each one).
        void Execute(RenderGraphContext& ctx, FrameStats& stats);

        // Migration helper (Group C): runs the same pre-Execute barrier, post-Execute
        // write-layout tracking, and HH_PROFILE_ZONE/FrameStats sampling (keyed by
        // IRenderPass::GetName(), so zone names stay identical to the old hand-written
        // RenderQueue::Render blocks) as Execute()'s loop body, for one already-registered pass.
        // Lets RenderQueue::Render call migrated passes individually, interleaved with legacy
        // passes that haven't converted yet, while still getting correct automatic barriers.
        void ExecutePass(IRenderPass* pass, RenderGraphContext& ctx, FrameStats& stats);

        void SetSwapchainSize(uint32_t width, uint32_t height);
        void SetSceneViewSize(uint32_t width, uint32_t height);

        // Updates a Fixed-class texture's target dimensions (e.g. shadow map size driven by
        // settings); call Invalidate(SizeClass::Fixed, device) afterward to recreate it.
        void SetFixedSize(const std::string& name, uint32_t width, uint32_t height);

        // Recreates every transient in sizeClass, then calls CreateFramebuffers() on every pass
        // that reads or writes a handle in that class.
        void Invalidate(SizeClass sizeClass, RHI::IRHIDevice& device);

        RHI::IRHITexture& GetTexture(ResourceHandle handle) const;
        RHI::IRHITexture& GetTexture(const std::string& name) const;

        void Cleanup(RHI::IRHIDevice& device);

        // ── Builder-facing API (called only through RenderGraphBuilder) ─────────────────────
        ResourceHandle DeclareTexture(const std::string& name, const GraphTextureDesc& desc);
        ResourceHandle DeclareWrite(size_t passIndex, const std::string& name, RHI::ImageLayout finalLayoutAfterExecute);
        ResourceHandle DeclareRead(size_t passIndex, const std::string& name);
        ResourceHandle DeclareReadSampled(size_t passIndex, const std::string& name);

    private:
        ResourceHandle resolveHandle(const std::string& name) const;
        void transitionSampledReads(size_t passIndex, RHI::IRHICommandList& cmd);
        void createFramebuffersForHandles(const std::vector<ResourceHandle>& handles, RHI::IRHIDevice& device);
        void executePassAt(size_t passIndex, RenderGraphContext& ctx, FrameStats& stats);

    private:
        struct PassDeps
        {
            std::vector<ResourceHandle> m_Writes;
            std::vector<ResourceHandle> m_ReadSampled;
            std::vector<ResourceHandle> m_AllHandles; // reads + sampled reads + writes, for invalidation lookup
        };

        std::vector<IRenderPass*> m_Passes;
        std::vector<PassDeps>     m_PassDeps;
        std::unordered_map<IRenderPass*, size_t> m_PassIndexByPointer;

        std::unordered_map<std::string, ResourceHandle>      m_NameToHandle;
        std::unordered_map<ResourceHandle, RHI::ImageLayout> m_WriteFinalLayout;
        std::vector<RHI::ImageLayout>                        m_CurrentLayouts;

        std::unique_ptr<RenderGraphResourcePool> m_Pool;
    };
}
