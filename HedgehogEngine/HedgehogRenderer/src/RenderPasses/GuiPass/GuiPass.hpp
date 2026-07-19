#pragma once

#include "RenderGraph/IRenderPass.hpp"
#include "RenderGraph/RenderGraphTypes.hpp"

#include <memory>

namespace HW
{
    class Window;
}

namespace RHI
{
    class IRHIFramebuffer;
    class IRHIGuiBackend;
}

namespace Renderer
{
    class GuiPass : public IRenderPass
    {
    public:
        GuiPass(HW::Window& window, RHI::IRHIDevice& device);
        ~GuiPass() override;

        GuiPass(const GuiPass&)            = delete;
        GuiPass& operator=(const GuiPass&) = delete;

        void BeginFrame();
        void DiscardFrame();
        void* GetSceneViewTextureId() const;

        const char* GetName() const override { return "GuiPass"; }

        void Setup(RenderGraphBuilder& builder) override;
        void CreateFramebuffers(RHI::IRHIDevice& device, RenderGraph& graph) override;
        void Execute(RenderGraphContext& ctx) override;
        void Cleanup(RHI::IRHIDevice& device) override;

    private:
        ResourceHandle m_SceneColorHandle = INVALID_RESOURCE_HANDLE;
        ResourceHandle m_GuiColorHandle   = INVALID_RESOURCE_HANDLE;

        std::unique_ptr<RHI::IRHIGuiBackend>  m_GuiBackend;
        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;
        void*                                  m_SceneViewId = nullptr;
    };
}
