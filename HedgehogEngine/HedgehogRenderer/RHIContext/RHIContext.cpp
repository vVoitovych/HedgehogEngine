#include "HedgehogRenderer/RHIContext/RHIContext.hpp"

#include "HedgehogEngine/Context/WindowContext.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"

namespace Renderer
{
    RHIContext::RHIContext(HedgehogEngine::WindowContext& windowContext)
    {
        auto& window = windowContext.GetWindow();

        RHI::NativeWindowDesc nativeDesc{};
        nativeDesc.m_NativeHandle = window.GetNativeOsHandle();
        nativeDesc.m_VkExtensions = window.GetVulkanExtensions(nativeDesc.m_VkExtensionCount);
        m_RHIDevice = RHI::IRHIDevice::Create(nativeDesc);

        int fbWidth = 0, fbHeight = 0;
        window.GetFramebufferSize(fbWidth, fbHeight);
        m_RHISwapchain = m_RHIDevice->CreateSwapchain(
            static_cast<uint32_t>(fbWidth),
            static_cast<uint32_t>(fbHeight));
    }

    RHIContext::~RHIContext()
    {
    }

    void RHIContext::Cleanup()
    {
        m_RHIDevice->WaitIdle();
        m_RHISwapchain.reset();
        m_RHIDevice.reset();
    }

    void RHIContext::RecreateSwapchain(HedgehogEngine::WindowContext& windowContext)
    {
        auto& window = windowContext.GetWindow();

        int fbWidth = 0, fbHeight = 0;
        window.GetFramebufferSize(fbWidth, fbHeight);
        while (fbWidth == 0 || fbHeight == 0)
        {
            window.GetFramebufferSize(fbWidth, fbHeight);
            windowContext.WaitEvents();
        }
        m_RHIDevice->WaitIdle();
        m_RHISwapchain->Resize(static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight));
    }

    RHI::IRHIDevice& RHIContext::GetRHIDevice()
    {
        return *m_RHIDevice;
    }

    const RHI::IRHIDevice& RHIContext::GetRHIDevice() const
    {
        return *m_RHIDevice;
    }

    RHI::IRHISwapchain& RHIContext::GetRHISwapchain()
    {
        return *m_RHISwapchain;
    }

    const RHI::IRHISwapchain& RHIContext::GetRHISwapchain() const
    {
        return *m_RHISwapchain;
    }
}
