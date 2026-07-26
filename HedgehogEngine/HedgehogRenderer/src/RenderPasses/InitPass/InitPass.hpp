#pragma once

#include "RenderGraph/IRenderPass.hpp"

namespace Renderer
{
    // Frame-graph pass: acquires the next swapchain image and begins the shared command buffer
    // that every later pass this frame records into (see PresentPass, which ends and submits it).
    // Declares no graph resources — it produces ctx.BackBufferIndex directly on the context, not
    // a named texture.
    class InitPass : public IRenderPass
    {
    public:
        InitPass()  = default;
        ~InitPass() override = default;

        InitPass(const InitPass&)            = delete;
        InitPass& operator=(const InitPass&) = delete;

        const char* GetName() const override { return "InitPass"; }

        void Setup(RenderGraphBuilder& builder) override { (void)builder; }
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override { (void)device; (void)graph; }
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override { (void)device; }
    };
}
