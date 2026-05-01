#pragma once

#include <cstdint>

namespace RHI
{
    class IRHISwapchain;
    class IRHIFence;
    class IRHISemaphore;
    class IRHICommandList;
}

namespace Renderer
{
    class InitPass
    {
    public:
        InitPass()  = default;
        ~InitPass() = default;

        InitPass(const InitPass&)            = delete;
        InitPass& operator=(const InitPass&) = delete;

        uint32_t Render(RHI::IRHISwapchain&  swapchain,
                        RHI::IRHIFence&      fence,
                        RHI::IRHISemaphore&  imageAvailableSemaphore,
                        RHI::IRHICommandList& cmd);

        void Cleanup();
    };
}
