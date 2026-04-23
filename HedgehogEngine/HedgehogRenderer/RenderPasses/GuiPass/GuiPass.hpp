#pragma once

#include <Volk/volk.h>
#include <memory>

namespace HW
{
    class Window;
}

namespace RHI
{
    class IRHIDevice;
    class IRHIRenderPass;
    class IRHIFramebuffer;
    class IRHICommandList;
}

namespace Renderer
{
    class ResourceManager;

    class GuiPass
    {
    public:
        GuiPass(HW::Window& window, RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        ~GuiPass();

        GuiPass(const GuiPass&)            = delete;
        GuiPass& operator=(const GuiPass&) = delete;

        void BeginFrame();
        void Render(RHI::IRHICommandList& cmd, const ResourceManager& resourceManager);
        void Cleanup(RHI::IRHIDevice& device);

        void ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager);

        static bool IsCursorPositionInGUI();

    private:
        void UploadFonts();

    private:
        std::unique_ptr<RHI::IRHIRenderPass>  m_RenderPass;
        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;
        VkDescriptorPool                      m_ImGuiPool = VK_NULL_HANDLE;
    };
}
