#pragma once

#include "RenderGraph/IRenderPass.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace HW
{
    class Window;
}

namespace RHI
{
    class IRHIDevice;
    class IRHIFramebuffer;
    class IRHIGuiBackend;
}

namespace Renderer
{
    // Composition-graph pass: draws ImGui (including every view's colour output, sampled as an
    // ImGui image per view) into its own guiColor target. Which views it samples is data, not
    // structure — set via SetViewSources() (the fully qualified ledger names returned by
    // ViewRegistry::GetAllViewOutputNames(), e.g. "scene/viewColor") before this pass's graph is
    // compiled, so Setup() can declare an ImportReadSampled per source. guiColor lives in the
    // composition graph rather than the frame graph — a deliberate deviation from the plan's
    // prose (see workflow/current-plan.md, Phase 4 completion note): InitPass never touches it,
    // and GuiPass/PresentPass are the only readers/writers, so the composition graph is where the
    // architecture diagram already puts it.
    class GuiPass : public IRenderPass
    {
    public:
        GuiPass(HW::Window& window, RHI::IRHIDevice& device);
        ~GuiPass() override;

        GuiPass(const GuiPass&)            = delete;
        GuiPass& operator=(const GuiPass&) = delete;

        const char* GetName() const override { return "GuiPass"; }

        // Must be called (if the source list changes) before the next Compile()/AddPass() that
        // registers this pass — Setup() reads it to declare this frame's ImportReadSampled calls.
        void OnViewsChanged(const std::vector<std::string>& viewOutputNames) override { m_ViewSources = viewOutputNames; }

        void BeginFrame() override;
        void DiscardFrame() override;

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

        // ImGui texture id for a view's colour output, keyed by its fully qualified ledger name
        // (RenderView::OutputColorName()); nullptr if that name isn't among the current sources.
        void* GetViewTextureId(const std::string& viewOutputName) const override;

    private:
        std::unique_ptr<RHI::IRHIGuiBackend>  m_GuiBackend;
        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;

        std::vector<std::string>               m_ViewSources;
        std::unordered_map<std::string, void*> m_ViewTextureIds;
    };
}
