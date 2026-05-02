#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace HedgehogEngine
{
    struct FrameData;
}

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
    class IRHICommandList;
    class IRHIFence;
    class IRHISemaphore;
}

namespace HW
{
    class Window;
}

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    class IRenderNode;
    class GuiNode;
    class ResourceManager;

    class RenderQueue
    {
    public:
        RenderQueue(RHI::IRHIDevice&                  device,
                    HW::Window&                       window,
                    const HedgehogSettings::Settings& settings,
                    ResourceManager&                  resourceManager);
        ~RenderQueue();

        RenderQueue(const RenderQueue&)            = delete;
        RenderQueue& operator=(const RenderQueue&) = delete;
        RenderQueue(RenderQueue&&)                 = delete;
        RenderQueue& operator=(RenderQueue&&)      = delete;

        void Cleanup(RHI::IRHIDevice& device);

        void  BeginGui();
        void  DiscardGui();
        void* GetSceneViewTextureId() const;

        void Render(const HedgehogEngine::FrameData& frame,
                    RHI::IRHIDevice&      device,
                    RHI::IRHISwapchain&   swapchain,
                    RHI::IRHICommandList& cmd,
                    RHI::IRHIFence&       fence,
                    RHI::IRHISemaphore&   imageAvailableSemaphore,
                    RHI::IRHISemaphore&   renderFinishedSemaphore,
                    uint32_t              frameIndex,
                    const ResourceManager& resourceManager);

        void UpdateData(const HedgehogEngine::FrameData&   frame,
                        uint32_t                           frameIndex,
                        const HedgehogSettings::Settings&  settings);

        void ResizeResources(RHI::IRHIDevice& device, const ResourceManager& resourceManager);
        void ResizeSceneView(RHI::IRHIDevice& device, const ResourceManager& resourceManager);

        void UpdateResources(RHI::IRHIDevice&                  device,
                             const HedgehogSettings::Settings& settings,
                             const ResourceManager&            resourceManager);

    private:
        std::vector<std::unique_ptr<IRenderNode>> m_Nodes;
        GuiNode*                                  m_GuiNode = nullptr; // non-owning alias
    };

} // namespace Renderer
