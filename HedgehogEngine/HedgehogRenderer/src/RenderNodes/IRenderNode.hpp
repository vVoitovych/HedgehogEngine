#pragma once

#include <cstdint>

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

namespace HedgehogSettings
{
    class Settings;
}

namespace Renderer
{
    class ResourceManager;

    // All execution-time state passed to every node's Execute().
    // backBufferIndex is written by InitNode and read by PresentNode.
    struct NodeContext
    {
        const HedgehogEngine::FrameData& frame;
        RHI::IRHIDevice&                 device;
        RHI::IRHISwapchain&              swapchain;
        RHI::IRHICommandList&            cmd;
        RHI::IRHIFence&                  fence;
        RHI::IRHISemaphore&              imageAvailableSemaphore;
        RHI::IRHISemaphore&              renderFinishedSemaphore;
        uint32_t                         frameIndex;
        const ResourceManager&           resourceManager;
        uint32_t                         backBufferIndex = 0;
    };

    class IRenderNode
    {
    public:
        virtual ~IRenderNode() = default;

        virtual void Execute(NodeContext& ctx)         = 0;
        virtual void Cleanup(RHI::IRHIDevice& device)  = 0;

        virtual void OnWindowResize(RHI::IRHIDevice& device,
                                    const ResourceManager& resourceManager) {}
        virtual void OnSceneViewResize(RHI::IRHIDevice& device,
                                       const ResourceManager& resourceManager) {}
        virtual void OnSettingsChanged(RHI::IRHIDevice& device,
                                       const HedgehogSettings::Settings& settings,
                                       const ResourceManager& resourceManager) {}
        virtual void UpdateData(const HedgehogEngine::FrameData& frame,
                                uint32_t frameIndex,
                                const HedgehogSettings::Settings& settings) {}

        virtual void  BeginFrame()                  {}
        virtual void  DiscardFrame()                {}
        virtual void* GetSceneViewTextureId() const { return nullptr; }

        bool IsEnabled() const  { return m_Enabled; }
        void SetEnabled(bool v) { m_Enabled = v; }

    private:
        bool m_Enabled = true;
    };
}
