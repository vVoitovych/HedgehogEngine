#pragma once

#include "RenderGraph/IRenderPass.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

namespace RHI
{
    class IRHITexture;
}

namespace Renderer
{
    // Reads guiColor and blits it to the swapchain image named by the frame context's
    // backBufferIndex. The swapchain itself is imported (not graph-owned), so its transitions
    // stay hand-rolled here rather than going through the graph's generic barrier step.
    class PresentPass : public IRenderPass
    {
    public:
        PresentPass()  = default;
        ~PresentPass() override = default;

        PresentPass(const PresentPass&)            = delete;
        PresentPass& operator=(const PresentPass&) = delete;

        const char* GetName() const override { return "PresentPass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        ResourceHandle     m_GuiColorHandle  = INVALID_RESOURCE_HANDLE;
        RHI::IRHITexture*  m_GuiColorTexture = nullptr;
    };
}
