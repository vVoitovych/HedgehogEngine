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
    class RenderResourceLedger;
    class FrameStats;

    // Orchestrates a small, linear (non-scheduling) render graph: passes run in registration
    // order. The graph owns every LOCALLY DECLARED transient texture (keyed by size class) via
    // its own RenderGraphResourcePool, and publishes each one to a shared RenderResourceLedger
    // under a scoped name ("<scope>/<name>") so other graph instances — a different view, the
    // frame graph, the composition graph — can import it. A resource's current image layout is
    // tracked in the ledger too, including for resources this graph itself declared, so the
    // layout truth for any texture lives in exactly one place regardless of which graph produced
    // it. See workflow/current-plan.md, "RenderResourceLedger — the cross-stage resource mechanism".
    class RenderGraph
    {
    public:
        explicit RenderGraph(RenderResourceLedger& ledger);
        ~RenderGraph();

        RenderGraph(const RenderGraph&)            = delete;
        RenderGraph& operator=(const RenderGraph&) = delete;
        RenderGraph(RenderGraph&&)                 = delete;
        RenderGraph& operator=(RenderGraph&&)      = delete;

        // Every resource this graph declares is published to the ledger as "<scope>/<name>", and
        // every ImportTexture() call resolves a fully qualified ledger key directly. Must be
        // called (if at all — an empty scope is legal but then every declared name must already
        // be globally unique) before the first DeclareTexture/ImportTexture call, i.e.
        // immediately after construction.
        void SetScope(std::string scope);

        // Suffix appended to this graph's FrameStats zone names ("" for the main/only instance of
        // a pass type, "[game]" for an additional view) so multiple views of the same pass type
        // don't collide in the per-pass timing table. Must be called (if at all) before the first
        // AddPass()/AddAndCompilePass() — see workflow/current-plan.md, "Profiling identity".
        void SetStatsSuffix(std::string suffix);

        // Passes execute in the order they are added. Ownership stays with the caller (a
        // RenderPipeline owns the passes via unique_ptr); the graph only holds observer pointers.
        void AddPass(IRenderPass* pass);

        // Runs Setup() on every pass to record the resource graph, creates every locally declared
        // transient at its size class and publishes each to the ledger, then calls
        // CreateFramebuffers() on every pass. Call graphs in dependency order — a graph that
        // imports another graph's resource must Compile() after the owning graph has.
        void Compile(RHI::IRHIDevice& device,
                     uint32_t swapchainWidth, uint32_t swapchainHeight,
                     uint32_t viewWidth, uint32_t viewHeight);

        // Registers a single pass like AddPass, then immediately runs its
        // Setup()+CreateFramebuffers() without touching already-created transients — used
        // because the graph's transients are declared+allocated up front by an earlier
        // zero-pass Compile() call, so each pass's Setup() only resolves existing handles via
        // Read/Write/ReadSampled/Import*, never CreateTexture. Returns the pass's index, which a
        // caller may keep to drive it later via ExecutePassAt() — e.g. registering the SAME pass
        // object twice with different state set beforehand (see ForwardPass::SetActiveTarget) to
        // get two independent PassDeps entries from one C++ object, a Phase 3 bridge that lets a
        // single pass instance serve two render targets before per-view instancing (Phase 4)
        // makes that unnecessary. ExecutePass(IRenderPass*, ...) cannot reach the first of two
        // such registrations (m_PassIndexByPointer only remembers the latest one for a pointer).
        size_t AddAndCompilePass(IRenderPass* pass, RHI::IRHIDevice& device);

        void Update(const RenderGraphContext& ctx);

        // Runs every registered pass in order (see ExecutePass for the per-pass barrier/tracking/
        // profiling steps this applies to each one).
        void Execute(RenderGraphContext& ctx, FrameStats& stats);

        // Runs the same pre-Execute barrier, post-Execute write-layout tracking, and
        // HH_PROFILE_ZONE/FrameStats sampling (keyed by IRenderPass::GetName() plus this graph's
        // stats suffix, so zone names stay identical to the old hand-written RenderQueue::Render
        // blocks) as Execute()'s loop body, for one already-registered pass. Lets a caller drive
        // migrated passes individually, interleaved with legacy passes that haven't converted
        // yet, while still getting correct automatic barriers.
        void ExecutePass(IRenderPass* pass, RenderGraphContext& ctx, FrameStats& stats);

        // Index-addressed variant of ExecutePass, for the dual-registration bridge described on
        // AddAndCompilePass above. passIndex must be a value that AddPass/AddAndCompilePass
        // returned for this graph.
        void ExecutePassAt(size_t passIndex, RenderGraphContext& ctx, FrameStats& stats);

        void SetSwapchainSize(uint32_t width, uint32_t height);

        // Sets the size ViewRelative transients in THIS graph resolve against. Only meaningful
        // for a view-stage graph; a frame/composition-stage graph never declares a ViewRelative
        // texture and never needs to call this.
        void SetViewSize(uint32_t width, uint32_t height);

        // Updates a Fixed-class texture's target dimensions (e.g. shadow map size driven by
        // settings); call Invalidate(SizeClass::Fixed, device) afterward to recreate it.
        void SetFixedSize(const std::string& name, uint32_t width, uint32_t height);

        // Recreates every LOCALLY DECLARED transient in sizeClass, republishes each to the ledger
        // (bumping its generation), then calls CreateFramebuffers() on every pass in THIS graph
        // that reads or writes a handle in that class. Does not touch imported resources — the
        // graph that owns an imported resource invalidates it; this graph picks up the change via
        // RefreshImports().
        void Invalidate(SizeClass sizeClass, RHI::IRHIDevice& device);

        // Walks this graph's imported handles; for any whose ledger generation has advanced since
        // it was last observed, calls CreateFramebuffers() on every pass that depends on it. Call
        // after any other graph's Invalidate() that this graph might import from, before the next
        // Execute() — see workflow/current-plan.md, "Descriptor / framebuffer lifetime on resize".
        void RefreshImports(RHI::IRHIDevice& device);

        RHI::IRHITexture& GetTexture(ResourceHandle handle) const;
        RHI::IRHITexture& GetTexture(const std::string& name) const;

        void Cleanup(RHI::IRHIDevice& device);

        // ── Builder-facing API (called only through RenderGraphBuilder) ─────────────────────
        ResourceHandle DeclareTexture(const std::string& name, const GraphTextureDesc& desc);
        ResourceHandle DeclareWrite(size_t passIndex, const std::string& name, RHI::ImageLayout finalLayoutAfterExecute);
        ResourceHandle DeclareRead(size_t passIndex, const std::string& name);
        ResourceHandle DeclareReadSampled(size_t passIndex, const std::string& name);

        // Cross-scope variants: name is the fully qualified ledger key of a resource declared by
        // a different graph instance rather than a name local to this graph's own declarations.
        ResourceHandle ImportTexture(const std::string& scopedName);
        ResourceHandle DeclareImportWrite(size_t passIndex, const std::string& scopedName, RHI::ImageLayout finalLayoutAfterExecute);
        ResourceHandle DeclareImportRead(size_t passIndex, const std::string& scopedName);
        ResourceHandle DeclareImportReadSampled(size_t passIndex, const std::string& scopedName);

    private:
        struct HandleInfo
        {
            std::string ScopedName;
            bool        IsImported         = false;
            uint32_t    LocalPoolIndex     = 0; // valid only when !IsImported
            uint32_t    LastSeenGeneration = 0; // valid only when IsImported
        };

        struct PassDeps
        {
            std::string                  ZoneName; // pass->GetName() + this graph's stats suffix, cached once
            std::vector<ResourceHandle>  Writes;
            std::vector<ResourceHandle>  ReadSampled;
            std::vector<ResourceHandle>  AllHandles; // reads + sampled reads + writes, for invalidation lookup
        };

    private:
        // Strict, local-only lookup used by DeclareWrite/DeclareRead/DeclareReadSampled — keeping
        // it strict (as opposed to resolveAnyHandle below) means a typo'd local name can never
        // silently resolve to an already-imported handle of the same string.
        ResourceHandle resolveHandle(const std::string& name) const;

        // General lookup used by GetTexture(name): checks locally-declared names first, then
        // imported ones. A pass's CreateFramebuffers() doesn't care which kind a texture is, only
        // that Setup() already established the dependency (via Write/Import*) — see PresentPass.
        ResourceHandle resolveAnyHandle(const std::string& name) const;

        ResourceHandle registerImportHandle(const std::string& scopedName);
        std::string    scopedNameOf(const std::string& localName) const;

        void recordRead(size_t passIndex, ResourceHandle handle);
        void recordReadSampled(size_t passIndex, ResourceHandle handle);
        void recordWrite(size_t passIndex, ResourceHandle handle, RHI::ImageLayout finalLayoutAfterExecute);

        // Publishes every locally-declared handle whose pool index is in changedPoolIndices to
        // the ledger; returns the graph-wide handles that were (re)published.
        std::vector<ResourceHandle> publishRecreated(const std::vector<uint32_t>& changedPoolIndices);

        void transitionSampledReads(size_t passIndex, RHI::IRHICommandList& cmd);
        void createFramebuffersForHandles(const std::vector<ResourceHandle>& handles, RHI::IRHIDevice& device);
        void executePassAt(size_t passIndex, RenderGraphContext& ctx, FrameStats& stats);

    private:
        RenderResourceLedger& m_Ledger;
        std::string           m_Scope;
        std::string           m_StatsSuffix;

        std::vector<IRenderPass*>                m_Passes;
        std::vector<PassDeps>                    m_PassDeps;
        std::unordered_map<IRenderPass*, size_t> m_PassIndexByPointer;

        std::vector<HandleInfo>                            m_Handles; // graph-wide handle space (local + imported)
        std::unordered_map<std::string, ResourceHandle>    m_NameToHandle;       // local, unscoped lookup
        std::unordered_map<std::string, ResourceHandle>    m_ImportNameToHandle; // scoped lookup
        std::unordered_map<ResourceHandle, RHI::ImageLayout> m_WriteFinalLayout;

        std::unique_ptr<RenderGraphResourcePool> m_Pool;
    };
}
