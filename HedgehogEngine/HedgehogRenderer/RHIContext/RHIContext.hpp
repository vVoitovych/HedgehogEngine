#pragma once

#include <memory>

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
}

namespace Context
{
    class WindowContext;
}

namespace Renderer
{
    class RHIContext
    {
    public:
        explicit RHIContext(Context::WindowContext& windowContext);
        ~RHIContext();

        RHIContext(const RHIContext&)            = delete;
        RHIContext(RHIContext&&)                 = delete;
        RHIContext& operator=(const RHIContext&) = delete;
        RHIContext& operator=(RHIContext&&)      = delete;

        void Cleanup();
        void RecreateSwapchain(Context::WindowContext& windowContext);

        RHI::IRHIDevice&          GetRHIDevice();
        const RHI::IRHIDevice&    GetRHIDevice() const;
        RHI::IRHISwapchain&       GetRHISwapchain();
        const RHI::IRHISwapchain& GetRHISwapchain() const;

    private:
        std::unique_ptr<RHI::IRHIDevice>    m_RHIDevice;
        std::unique_ptr<RHI::IRHISwapchain> m_RHISwapchain;
    };
}
