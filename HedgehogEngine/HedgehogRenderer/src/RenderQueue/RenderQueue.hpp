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
    class IRHICommandList;
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
                    RHI::IRHIDevice&                 device,
                    RHI::IRHICommandList&            cmd,
                    uint32_t                         frameIndex,
                    const ResourceManager&           resourceManager);

        void PreExecuteFrame(const HedgehogEngine::FrameData&   frame,
                             uint32_t                           frameIndex,
                             const HedgehogSettings::Settings&  settings);

    private:
        std::vector<std::unique_ptr<IRenderNode>> m_Nodes;
        GuiNode*                                  m_GuiNode = nullptr; // non-owning alias
    };

} // namespace Renderer
