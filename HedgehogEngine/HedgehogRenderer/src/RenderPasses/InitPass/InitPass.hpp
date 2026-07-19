#pragma once

#include "RenderGraph/IRenderPass.hpp"

namespace Renderer
{
    // No attachments — acquires the next swapchain image and writes it into the frame
    // context's backBufferIndex for downstream passes (PresentPass) to consume.
    class InitPass : public IRenderPass
    {
    public:
        InitPass()  = default;
        ~InitPass() override = default;

        InitPass(const InitPass&)            = delete;
        InitPass& operator=(const InitPass&) = delete;

        const char* GetName() const override { return "InitPass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;
    };
}
