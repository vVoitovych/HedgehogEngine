#pragma once

#include "GraphDescription.hpp"
#include "RGTypes.hpp"

#include <functional>
#include <string>

namespace Renderer
{
    class RGPassBuilder;

    // Declares a graph (RENDERING.md section 5.1). Create/Import register resources; AddOutputSlot
    // declares the graph's typed, ordered output contract (section 5.2); AddPass runs a setup
    // callback against a fresh RGPassBuilder to record that pass's reads/writes. Nothing here
    // touches the RHI or a device — GetDescription() is the whole result, ready for a (later,
    // separately ticketed) compiler to turn into a DAG.
    class GraphBuilder
    {
    public:
        GraphBuilder()  = default;
        ~GraphBuilder() = default;

        GraphBuilder(const GraphBuilder&)            = delete;
        GraphBuilder& operator=(const GraphBuilder&) = delete;
        GraphBuilder(GraphBuilder&&)                 = delete;
        GraphBuilder& operator=(GraphBuilder&&)      = delete;

        // Declares a new transient resource, owned by the graph for this frame. Returns a
        // handle at version 0 — nothing has written it yet.
        RGTexture CreateTexture(const RGTextureDesc& desc);
        RGBuffer  CreateBuffer(const RGBufferDesc& desc);

        // Registers an externally-owned resource under graph tracking, also at version 0.
        // isReadOnly marks it as never writable — Compile (a later ticket) rejects a write to
        // one, per RENDERING.md section 5.3's validation list.
        RGTexture ImportTexture(const std::string& name, RHI::Format format, bool isReadOnly = false);
        RGBuffer  ImportBuffer(const std::string& name, size_t size, bool isReadOnly = false);

        // Appends an output slot; returns its index, which is what a view will later bind to,
        // never Name (RENDERING.md section 5.2).
        uint32_t AddOutputSlot(const std::string& name, RHI::Format format, const RGSizePolicy& size);

        // Attaches the resource version that actually produces a slot's content. A slot left
        // unbound is a Compile() validation failure.
        void BindOutput(uint32_t slotIndex, RGTexture texture);

        // Records a pass: creates its RGPassRecord, then calls setup(passBuilder) synchronously
        // so every dependency the pass declares lands against that record before AddPass returns.
        void AddPass(const std::string& name, const std::function<void(RGPassBuilder&)>& setup);

        const GraphDescription& GetDescription() const { return m_Description; }

        // Clears back to a fresh, empty graph — for a long-lived builder a caller reuses frame
        // to frame (RenderGraphRuntime) instead of constructing a new one. Every RGTexture/
        // RGBuffer/slot index from before this call is meaningless afterwards.
        void Reset() { m_Description = GraphDescription{}; }

    private:
        friend class RGPassBuilder;

        RGResourceRecord& GetResourceRecordMutable(RGResourceId id);
        RGPassRecord&     GetPassRecordMutable(size_t passIndex);

        RGResourceId AllocateResourceId() { return static_cast<RGResourceId>(m_Description.Resources.size()); }

        GraphDescription m_Description;
    };
}
