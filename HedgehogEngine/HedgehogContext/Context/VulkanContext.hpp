#pragma once

#include <memory>

namespace HW
{
    class Window;
    class WindowManager;
}

namespace RHI
{
    class IRHIDevice;
    class IRHISwapchain;
}

namespace Context
{

    class VulkanContext
    {
    public:
        VulkanContext();
        ~VulkanContext();

        VulkanContext(const VulkanContext&)            = delete;
        VulkanContext(VulkanContext&&)                 = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext& operator=(VulkanContext&&)      = delete;

        void Cleanup();

        void HandleInput();
        HW::Window&       GetWindow();
        const HW::Window& GetWindow() const;

        bool ShouldClose() const;
        void ResizeWindow();
        bool IsWindowResized() const;
        void ResetWindowResizeState();
        void RecreateSwapchain();

        RHI::IRHIDevice&          GetRHIDevice();
        const RHI::IRHIDevice&    GetRHIDevice() const;
        RHI::IRHISwapchain&       GetRHISwapchain();
        const RHI::IRHISwapchain& GetRHISwapchain() const;

    private:
        std::unique_ptr<HW::WindowManager> m_WindowManager;
        HW::Window*                        m_Window        = nullptr;
        bool                               m_WindowResized = false;

        std::unique_ptr<RHI::IRHIDevice>    m_RHIDevice;
        std::unique_ptr<RHI::IRHISwapchain> m_RHISwapchain;
    };

}
