#pragma once

#include "RenderGraph/IRenderPass.hpp"

#include <string>

namespace RHI
{
    class IRHITexture;
}

namespace Renderer
{
    struct NodeDesc;

    // Composition-graph pass: blits its source colour target to the acquired swapchain image, ends
    // and submits the shared command buffer (see InitPass, which begins it), and presents. The
    // source is the single "presentSource" input the constructing .rgq node declares — a local
    // name (e.g. "guiColor", read from the same composition graph, as `composition_editor.rgq`
    // does) or an import (e.g. "views:main", straight from a view with no GuiPass at all, as
    // `present_direct.rgq` does for a headless/game build) — see workflow/current-plan.md,
    // "PresentPass takes its blit source from a declared input handle". This is the one pass whose
    // Setup() is genuinely data-driven by the YAML rather than hardcoded, since which source it
    // reads is the entire difference between the editor and game composition pipelines.
    class PresentPass : public IRenderPass
    {
    public:
        explicit PresentPass(const NodeDesc& node);
        ~PresentPass() override = default;

        PresentPass(const PresentPass&)            = delete;
        PresentPass& operator=(const PresentPass&) = delete;

        const char* GetName() const override { return "PresentPass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override { (void)device; }

    private:
        std::string m_SourceName; // local name, or a fully qualified "<scope>/<name>" import key
        bool        m_IsImport = false;

        RHI::IRHITexture* m_ColorBuffer = nullptr; // resolved from the graph; non-owning
    };
}
