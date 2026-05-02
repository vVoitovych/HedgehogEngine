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

namespace Renderer
{
    class ResourceManager;

    // Carries all per-frame data through the node execution list.
    // AcquireNode writes m_BackBufferIndex; PresentNode reads it.
    // All other fields are set once in RenderQueue::Render() before the loop.
    struct RenderContext
    {
        const HedgehogEngine::FrameData* m_FrameData       = nullptr;
        RHI::IRHIDevice*                 m_Device          = nullptr;
        RHI::IRHISwapchain*              m_Swapchain       = nullptr;
        RHI::IRHICommandList*            m_Cmd             = nullptr;
        RHI::IRHIFence*                  m_Fence           = nullptr;
        RHI::IRHISemaphore*              m_ImageAvailable  = nullptr;
        RHI::IRHISemaphore*              m_RenderFinished  = nullptr;
        const ResourceManager*           m_ResourceManager = nullptr;
        uint32_t                         m_FrameIndex      = 0;

        uint32_t                         m_BackBufferIndex = 0;
    };

} // namespace Renderer
