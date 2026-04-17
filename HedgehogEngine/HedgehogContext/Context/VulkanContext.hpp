#pragma once

#include <memory>

namespace WinManager
{
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

        VulkanContext(const VulkanContext&) = delete;
        VulkanContext(VulkanContext&&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;
        VulkanContext& operator=(VulkanContext&&) = delete;

        void Cleanup();

        void HandleInput();
        WinManager::WindowManager& GetWindowManager();
        const WinManager::WindowManager& GetWindowManager() const;

        bool ShouldClose() const;
        void ResizeWindow();
        bool IsWindowResized();
        void ResetWindowResizeState();
        void RecreateSwapchain();

        RHI::IRHIDevice&          GetRHIDevice();
        const RHI::IRHIDevice&    GetRHIDevice() const;
        RHI::IRHISwapchain&       GetRHISwapchain();
        const RHI::IRHISwapchain& GetRHISwapchain() const;

    private:
        std::unique_ptr<WinManager::WindowManager> m_WindowManager;
        bool m_WindowResized = false;

        std::unique_ptr<RHI::IRHIDevice>    m_RHIDevice;
        std::unique_ptr<RHI::IRHISwapchain> m_RHISwapchain;
    };

}
