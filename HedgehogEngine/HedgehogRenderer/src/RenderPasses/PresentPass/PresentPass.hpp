#pragma once

#include <cstdint>

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
    class IRHICommandList;
    class IRHITexture;
    class IRHIFence;
    class IRHISemaphore;
}

namespace Renderer
{
    class PresentPass
    {
    public:
        PresentPass()  = default;
        ~PresentPass() = default;

        PresentPass(const PresentPass&)            = delete;
        PresentPass& operator=(const PresentPass&) = delete;

        void Render(RHI::IRHICommandList& cmd,
                    RHI::IRHIDevice&      device,
                    RHI::IRHISwapchain&   swapchain,
                    RHI::IRHITexture&     colorBuffer,
                    uint32_t              backBufferIndex,
                    RHI::IRHISemaphore&   imageAvailableSemaphore,
                    RHI::IRHISemaphore&   renderFinishedSemaphore,
                    RHI::IRHIFence&       fence);

        void Cleanup();
    };
}
