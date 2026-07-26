#pragma once

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
    class RenderGraph;
    class RenderGraphBuilder;
    struct RenderGraphContext;

    // A pass never touches transient-texture lifetime directly: it declares reads/writes
    // in Setup() and receives ready-to-use textures/framebuffers via CreateFramebuffers(),
    // called once at graph compile and again whenever a size class or an imported resource it
    // depends on changes.
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        // Stable literal zone name (e.g. "ForwardPass") — the executor uses it (plus a per-graph
        // stats suffix, see RenderGraph::SetStatsSuffix) for FrameStats sampling so per-pass
        // timings keep the same identity they had when RenderQueue recorded them by hand around
        // each bespoke Render() call.
        virtual const char* GetName() const = 0;

        virtual void Setup(RenderGraphBuilder& builder) = 0;
        virtual void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) = 0;
        virtual void Update(const RenderGraphContext& ctx) { (void)ctx; }
        virtual void Execute(RenderGraphContext& ctx) = 0;
        virtual void Cleanup(RHI::IRHIDevice& device) = 0;

        // Optional per-frame/per-event hooks, default no-op. Since Phase 5 constructs passes
        // through a type-erased RenderPassRegistry factory (unique_ptr<IRenderPass>, no RTTI —
        // dynamic_cast is unused anywhere in this codebase), Renderer can no longer reach a
        // specific concrete pass type (ShadowmapPass, GuiPass) directly; these hooks are how it
        // reaches pass-specific per-frame behaviour through the interface instead. Each is
        // overridden by exactly one pass today (see workflow/current-plan.md, Phase 5 completion
        // note) — not a speculative extension point, the minimal generalization Phase 5 needs.
        virtual void BeginFrame() { }
        virtual void DiscardFrame() { }
        // Called before this pass's owning pipeline is (re)compiled, with the current set of
        // view output names (see ViewRegistry::GetAllViewOutputNames) — only GuiPass uses this.
        virtual void OnViewsChanged(const std::vector<std::string>& viewOutputNames) { (void)viewOutputNames; }
        // Called once per frame when HedgehogSettings::Settings::IsDirty() — only ShadowmapPass
        // uses this (shadow map resize driven by HedgehogSettings::ShadowmapingSettings).
        virtual void OnSettingsDirty(RenderGraph& graph, RHI::IRHIDevice& device,
                                     const HedgehogSettings::Settings& settings)
        {
            (void)graph; (void)device; (void)settings;
        }
        // ImGui texture id for a view's colour output, keyed by its fully qualified ledger name
        // (RenderView::OutputColorName()); nullptr if this pass doesn't own one under that name.
        // Only GuiPass returns non-null.
        virtual void* GetViewTextureId(const std::string& viewOutputName) const { (void)viewOutputName; return nullptr; }
    };
}
