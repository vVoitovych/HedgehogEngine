#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Renderer
{
    class RenderView;
    class RenderResourceLedger;

    // Owns every RenderView, keyed by a stable handle (uint32_t; Renderer::ViewHandle at the
    // public API is the same underlying type — see api/HedgehogRenderer/Renderer.hpp). Iteration
    // order is insertion order, which is also render order (matters for e.g. the main-view
    // "no stats suffix" convention — see RenderView::SetStatsSuffix).
    class ViewRegistry
    {
    public:
        using Handle                          = uint32_t;
        static constexpr Handle InvalidHandle = 0;

        ViewRegistry()  = default;
        ~ViewRegistry() = default;

        ViewRegistry(const ViewRegistry&)            = delete;
        ViewRegistry& operator=(const ViewRegistry&) = delete;
        ViewRegistry(ViewRegistry&&)                 = delete;
        ViewRegistry& operator=(ViewRegistry&&)      = delete;

        // Constructs and stores a new RenderView; the caller still has to AddPass()+Compile() it.
        Handle CreateView(std::string name, RenderResourceLedger& ledger);

        // Removes and destroys a view. Caller must have already called view->Cleanup(device) (and
        // device.WaitIdle() beforehand) — this method only drops ownership and bookkeeping.
        void DestroyView(Handle handle);

        [[nodiscard]] std::optional<Handle> FindByName(std::string_view name) const;

        RenderView*       Get(Handle handle);
        const RenderView* Get(Handle handle) const;

        // Insertion order == render order.
        const std::vector<Handle>& GetOrderedHandles() const { return m_Order; }

        // Scoped output-colour name (see RenderView::OutputColorName) of every view that exists,
        // in render order — what GuiPass declares via ImportReadSampled at graph-COMPILE time
        // (see GuiPass::Setup). Deliberately not filtered by IsEnabled(): a view's enabled flag is
        // a PER-FRAME concept, but Setup() runs once at compile time, so a view disabled at
        // compile time and enabled later would otherwise never get a texture-id slot. The
        // per-frame "disabled view" behaviour instead falls out of the resource ledger itself — a
        // never-rendered view's colour stays whatever layout it last had, and
        // RenderGraph::transitionSampledReads no-ops when that's already ShaderReadOnly. A
        // hand-composed stand-in for the "views:*" wildcard expansion the YAML loader will do in
        // Phase 5 (see workflow/current-plan.md, "Name reference syntax").
        std::vector<std::string> GetAllViewOutputNames() const;

    private:
        std::unordered_map<Handle, std::unique_ptr<RenderView>> m_Views;
        std::vector<Handle>                                     m_Order;
        Handle                                                  m_NextHandle = 1; // 0 is InvalidHandle
    };
}
