#pragma once

#include <memory>

namespace HW
{
    class Window;
}

namespace RHI
{
    class IRHIDevice;
    class IRHIFramebuffer;
    class IRHICommandList;
    class IRHIGuiBackend;
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
        void DiscardFrame();
        void Render(RHI::IRHICommandList& cmd, const ResourceManager& resourceManager);
        void Cleanup(RHI::IRHIDevice& device);

        void ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        void RecreateSceneDescriptor(const ResourceManager& resourceManager);

        void* GetSceneViewTextureId() const;

    private:
        void CreateSceneViewDescSet(const ResourceManager& resourceManager);

    private:
        std::unique_ptr<RHI::IRHIGuiBackend>  m_GuiBackend;
        std::unique_ptr<RHI::IRHIFramebuffer> m_FrameBuffer;
        void*                                  m_SceneViewId = nullptr;
    };
}
