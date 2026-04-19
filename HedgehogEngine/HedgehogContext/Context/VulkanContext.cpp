#include "VulkanContext.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

#include <GLFW/glfw3.h>

namespace Context
{
    VulkanContext::VulkanContext()
    {
        m_WindowManager = std::make_unique<WinManager::WindowManager>(WinManager::WindowState::GetDefaultState());
        ContentLoader::TextureLoader texLoader;
        texLoader.LoadTexture("Textures\\Logo\\logo1.png");
        m_WindowManager->SetIcon(texLoader.GetWidth(), texLoader.GetHeight(), static_cast<unsigned char*>(texLoader.GetData()));

        m_RHIDevice = RHI::IRHIDevice::Create(m_WindowManager->GetGlfwWindow());
        int fbWidth = 0, fbHeight = 0;
        glfwGetFramebufferSize(m_WindowManager->GetGlfwWindow(), &fbWidth, &fbHeight);
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
        m_WindowManager->HandleInput();
    }

    WinManager::WindowManager& VulkanContext::GetWindowManager()
    {
        return *m_WindowManager;
    }

    const WinManager::WindowManager& VulkanContext::GetWindowManager() const
    {
        return *m_WindowManager;
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
        return m_WindowManager->ShouldClose();
    }

    void VulkanContext::ResizeWindow()
    {
        m_WindowResized = true;
    }

    bool VulkanContext::IsWindowResized()
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
        glfwGetFramebufferSize(m_WindowManager->GetGlfwWindow(), &fbWidth, &fbHeight);
        while (fbWidth == 0 || fbHeight == 0)
        {
            glfwGetFramebufferSize(m_WindowManager->GetGlfwWindow(), &fbWidth, &fbHeight);
            glfwWaitEvents();
        }
        m_RHIDevice->WaitIdle();
        m_RHISwapchain->Resize(static_cast<uint32_t>(fbWidth), static_cast<uint32_t>(fbHeight));
    }

}
