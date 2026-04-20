#include "VulkanContext.hpp"

#include "HedgehogEngine/HedgehogWindow/api/Window.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowDesc.hpp"
#include "HedgehogEngine/HedgehogWindow/api/WindowManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

namespace Context
{
    VulkanContext::VulkanContext()
    {
        m_WindowManager = std::make_unique<HW::WindowManager>();

        HW::WindowDesc desc;
        desc.m_Title  = "Hedgehog Engine";
        desc.m_X      = 100;
        desc.m_Y      = 100;
        desc.m_Width  = 1366;
        desc.m_Height = 768;
        m_Window = &m_WindowManager->CreateWindow(desc);

        ContentLoader::TextureLoader texLoader;
        texLoader.LoadTexture("Textures\\Logo\\logo1.png");
        m_Window->SetIcon(texLoader.GetWidth(), texLoader.GetHeight(),
            static_cast<unsigned char*>(texLoader.GetData()));

        RHI::NativeWindowDesc nativeDesc{};
        nativeDesc.m_NativeHandle    = m_Window->GetNativeOsHandle();
        nativeDesc.m_VkExtensions    = m_Window->GetVulkanExtensions(nativeDesc.m_VkExtensionCount);
        m_RHIDevice = RHI::IRHIDevice::Create(nativeDesc);

        int fbWidth = 0, fbHeight = 0;
        m_Window->GetFramebufferSize(fbWidth, fbHeight);
        m_RHISwapchain = m_RHIDevice->CreateSwapchain(
            static_cast<uint32_t>(fbWidth),
            static_cast<uint32_t>(fbHeight));
    }

    VulkanContext::~VulkanContext()
    {
    }

    void VulkanContext::Cleanup()
    {
        m_RHIDevice->WaitIdle();
        m_RHISwapchain.reset();
        m_RHIDevice.reset();
    }

    void VulkanContext::HandleInput()
    {
        m_WindowManager->PollEvents();
    }

    HW::Window& VulkanContext::GetWindow()
    {
        return *m_Window;
    }

    const HW::Window& VulkanContext::GetWindow() const
    {
        return *m_Window;
    }

    RHI::IRHIDevice& VulkanContext::GetRHIDevice()
    {
        return *m_RHIDevice;
    }

    const RHI::IRHIDevice& VulkanContext::GetRHIDevice() const
    {
        return *m_RHIDevice;
    }

    RHI::IRHISwapchain& VulkanContext::GetRHISwapchain()
    {
        return *m_RHISwapchain;
    }

    const RHI::IRHISwapchain& VulkanContext::GetRHISwapchain() const
    {
        return *m_RHISwapchain;
    }

    bool VulkanContext::ShouldClose() const
    {
        return m_Window->ShouldClose();
    }

    void VulkanContext::ResizeWindow()
    {
        m_WindowResized = true;
    }

    bool VulkanContext::IsWindowResized() const
    {
        return m_WindowResized;
    }

    void VulkanContext::ResetWindowResizeState()
    {
        m_WindowResized = false;
    }

    void VulkanContext::RecreateSwapchain()
    {
        int fbWidth = 0, fbHeight = 0;
        m_Window->GetFramebufferSize(fbWidth, fbHeight);
        while (fbWidth == 0 || fbHeight == 0)
        {
            m_Window->GetFramebufferSize(fbWidth, fbHeight);
            m_WindowManager->WaitEvents();
        }
        m_RHIDevice->WaitIdle();
        m_RHISwapchain->Resize(static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight));
    }

}
