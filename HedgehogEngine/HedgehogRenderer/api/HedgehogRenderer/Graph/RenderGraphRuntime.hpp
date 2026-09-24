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
        RGTexture CreateTexture(const RGTextureDesc& desc) { return m_Builder.CreateTexture(desc); }
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

        // Attaches the real, externally-owned resource behind an imported handle — required
        // before Execute() for every resource ImportTexture/ImportBuffer declared, since the
        // pool only ever manages transient textures (RENDERING.md section 5.4's persistence
        // exclusion; buffers aren't pooled at all).
        void BindImportedTexture(RGTexture imported, RHI::IRHITexture* real);
        void BindImportedBuffer(RGBuffer imported, RHI::IRHIBuffer* real);

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
    };
}
