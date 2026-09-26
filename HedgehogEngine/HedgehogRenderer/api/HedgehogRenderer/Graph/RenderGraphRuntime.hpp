#pragma once

#include "FrameArena.hpp"
#include "GraphBuilder.hpp"
#include "GraphCompiler.hpp"
#include "ResourcePool.hpp"
#include "RGPassBuilder.hpp"

#include "RHI/api/IRHIBuffer.hpp"
#include "RHI/api/IRHICommandList.hpp"
#include "RHI/api/IRHIDevice.hpp"

#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

// RENDERING.md section 5.4: the execution side. RenderGraphRuntime owns one frame's GraphBuilder,
// compiles it, and issues each surviving pass's derived barriers followed by its closure.
// Everything AddPass hands it — the pass's own data and the execute closure's captures — comes
// from a FrameArena, never the heap, and every pointer it returns is invalid after the next
// Execute() resets that arena: the graph is rebuilt from scratch every frame (no owning pointer
// survives across frames).
namespace Renderer
{
    struct GraphFrameContext;

    class RenderGraphRuntime
    {
    public:
        RenderGraphRuntime(RHI::IRHIDevice& device, size_t arenaCapacityBytes);
        ~RenderGraphRuntime() = default;

        RenderGraphRuntime(const RenderGraphRuntime&)            = delete;
        RenderGraphRuntime& operator=(const RenderGraphRuntime&) = delete;
        RenderGraphRuntime(RenderGraphRuntime&&)                 = delete;
        RenderGraphRuntime& operator=(RenderGraphRuntime&&)      = delete;

        // Declaration passthrough (GraphBuilder.hpp) — building the graph for this frame
        // happens directly against these, exactly as it would against a bare GraphBuilder.
        // A relative size policy resolves to Absolute here once SetSizeReferences has given it a
        // reference; without one it stays relative, as in the headless tests.
        RGTexture CreateTexture(const RGTextureDesc& desc) { return m_Builder.CreateTexture(ResolveSize(desc)); }
        RGBuffer  CreateBuffer(const RGBufferDesc& desc)   { return m_Builder.CreateBuffer(desc); }
        RGTexture ImportTexture(const std::string& name, RHI::Format format, bool isReadOnly = false)
        {
            return m_Builder.ImportTexture(name, format, isReadOnly);
        }
        RGBuffer ImportBuffer(const std::string& name, size_t size, bool isReadOnly = false)
        {
            return m_Builder.ImportBuffer(name, size, isReadOnly);
        }
        uint32_t AddOutputSlot(const std::string& name, RHI::Format format, const RGSizePolicy& size)
        {
            return m_Builder.AddOutputSlot(name, format, size);
        }
        void BindOutput(uint32_t slotIndex, RGTexture texture) { m_Builder.BindOutput(slotIndex, texture); }

        // The extents that RelativeToResult and RelativeToSwapchain resolve against for textures
        // created from now on (RENDERING.md section 4): the result target of the view being
        // declared, and the swapchain. The frame loop sets them before each view's graph;
        // Execute() clears them.
        void SetSizeReferences(uint32_t resultWidth, uint32_t resultHeight,
                               uint32_t swapchainWidth, uint32_t swapchainHeight);

        // Attaches the real, externally-owned resource behind an imported handle — required
        // before Execute() for every resource ImportTexture/ImportBuffer declared, since the
        // pool only ever manages transient textures (RENDERING.md section 5.4's persistence
        // exclusion; buffers aren't pooled at all).
        void BindImportedTexture(RGTexture imported, RHI::IRHITexture* real);
        void BindImportedBuffer(RGBuffer imported, RHI::IRHIBuffer* real);

        // The frame data and pass services the engine passes read (GraphFrameContext.hpp). Attach
        // before declaring passes; builders capture the pointer. Cleared by Execute().
        void                     SetFrameContext(const GraphFrameContext* context) { m_FrameContext = context; }
        const GraphFrameContext* GetFrameContext() const { return m_FrameContext; }

        // The real texture behind a handle, for an execute closure to render into. Valid only
        // while Execute() is running passes: transients are only resolved then.
        [[nodiscard]] RHI::IRHITexture* GetTexture(RGTexture texture);

        // Declares a pass: PassData is default-constructed in the frame arena, setup(passBuilder,
        // passData) runs synchronously against it (declare dependencies AND fill in whatever the
        // execute closure will need), and execute is itself moved into the frame arena as a
        // closure Execute() will call after issuing that pass's barriers. Both static_asserts
        // in FrameArena::Create apply here too: PassData and execute's captures must be
        // trivially destructible.
        template<typename PassData, typename SetupFn, typename ExecuteFn>
        void AddPass(const std::string& name, SetupFn&& setup, ExecuteFn&& execute)
        {
            PassData* passData = m_Arena.Create<PassData>();

            m_Builder.AddPass(name, [&](RGPassBuilder& passBuilder)
            {
                setup(passBuilder, *passData);
            });

            using ExecuteState = std::decay_t<ExecuteFn>;
            static_assert(std::is_trivially_destructible_v<ExecuteState>,
                          "AddPass execute closures are arena-allocated and never destructed — "
                          "captures must be trivially destructible (capture pointers/handles, "
                          "not owning containers).");
            ExecuteState* executeState = m_Arena.Create<ExecuteState>(std::forward<ExecuteFn>(execute));

            PassExecutionRecord record;
            record.PassData     = passData;
            record.ExecuteState = executeState;
            record.Invoke = [](void* passDataPtr, void* executeStatePtr, RHI::IRHICommandList& cmdList)
            {
                (*static_cast<ExecuteState*>(executeStatePtr))(
                    *static_cast<PassData*>(passDataPtr), cmdList);
            };
            m_PassExecutions.push_back(record);
        }

        // Compiles the graph declared since the last Execute() (or construction). On success,
        // issues each surviving pass's derived barriers (resolved against real RHI resources —
        // pooled for transients, bound for imports) then its closure, in compiled order; on
        // failure, logs every CompileError and runs nothing. Either way, resets the frame arena,
        // retires this frame's pooled textures for reuse, and clears pass/import bookkeeping —
        // the next frame declares its graph from scratch.
        bool Execute(RHI::IRHICommandList& cmdList);

        // How many passes the last Execute() ran after culling; 0 if it failed to compile. A hidden
        // view or an unread pass shows up here as fewer passes.
        size_t GetLastExecutedPassCount() const { return m_LastExecutedPassCount; }

        // CPU time each executed pass took to record (its barriers and its closure), by pass name
        // in execution order, for the Editor's --benchmark. Measured only while enabled; the last
        // Execute() with timing off leaves the list empty.
        struct PassTiming
        {
            std::string Name;
            double      CpuMilliseconds = 0.0;
        };
        void SetPassTimingEnabled(bool enabled) { m_PassTimingEnabled = enabled; }
        const std::vector<PassTiming>& GetLastPassTimings() const { return m_LastPassTimings; }

        // Exposed for tests/diagnostics — verifying AddPass's "no heap churn" claim needs a way
        // to check whether a pointer actually came from this arena.
        const FrameArena& GetArena() const { return m_Arena; }

        // What has been declared since the last Execute(). Exposed so the C++ equivalence oracle
        // (RENDERING.md section 6, Rule 2) can compile an instantiated graph without executing it.
        const GraphDescription& GetDescription() const { return m_Builder.GetDescription(); }

    private:
        struct PassExecutionRecord
        {
            void* PassData     = nullptr;
            void* ExecuteState = nullptr;
            void (*Invoke)(void* passData, void* executeState, RHI::IRHICommandList& cmdList) = nullptr;
        };

        RGTextureDesc     ResolveSize(const RGTextureDesc& desc) const;
        RHI::IRHITexture* ResolveTexture(const GraphDescription& description, RGResourceId id);
        RHI::IRHIBuffer*  ResolveBuffer(RGResourceId id);
        void               ResetForNextFrame();

        RHI::IRHIDevice&                                       m_Device;
        FrameArena                                              m_Arena;
        GraphBuilder                                             m_Builder;
        GraphCompiler                                             m_Compiler;
        ResourcePool                                                m_Pool;

        std::vector<PassExecutionRecord>                        m_PassExecutions;
        std::unordered_map<RGResourceId, RHI::IRHITexture*>     m_ImportedTextures;
        std::unordered_map<RGResourceId, RHI::IRHIBuffer*>      m_ImportedBuffers;
        std::unordered_map<RGResourceId, RHI::IRHITexture*>     m_AcquiredTransients; // this-frame cache

        uint32_t m_ResultWidth     = 0;
        uint32_t m_ResultHeight    = 0;
        uint32_t m_SwapchainWidth  = 0;
        uint32_t m_SwapchainHeight = 0;

        size_t m_LastExecutedPassCount = 0;

        bool                    m_PassTimingEnabled = false;
        std::vector<PassTiming> m_LastPassTimings; // reused: names fit the small-string buffer

        const GraphFrameContext* m_FrameContext = nullptr;
        const GraphDescription*  m_Executing    = nullptr; // set only while Execute() runs passes
    };
}
