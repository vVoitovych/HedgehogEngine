#pragma once

#include <memory>

namespace Wrappers
{
    class Device;
    class SwapChain;
}

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

        // Legacy Wrappers API (kept until all render passes are migrated).
        const Wrappers::Device& GetDevice() const;
        const Wrappers::SwapChain& GetSwapChain() const;
        Wrappers::SwapChain& GetSwapChain();

        // New RHI API.
        RHI::IRHIDevice&          GetRHIDevice();
        const RHI::IRHIDevice&    GetRHIDevice() const;
        RHI::IRHISwapchain&       GetRHISwapchain();
        const RHI::IRHISwapchain& GetRHISwapchain() const;

    private:
        std::unique_ptr<WinManager::WindowManager> m_WindowManager;
        bool m_WindowResized = false;

        // Legacy Wrappers objects (removed once all callers are migrated).
        std::unique_ptr<Wrappers::Device>    m_Device;
        std::unique_ptr<Wrappers::SwapChain> m_SwapChain;

        // RHI objects — authoritative device/swapchain going forward.
        std::unique_ptr<RHI::IRHIDevice>    m_RHIDevice;
        std::unique_ptr<RHI::IRHISwapchain> m_RHISwapchain;
    };

}




