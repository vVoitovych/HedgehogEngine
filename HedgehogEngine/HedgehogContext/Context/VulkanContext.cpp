#include "VulkanContext.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogCommon/api/RendererSettings.hpp"
#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

#include "RHI/api/IRHIDevice.hpp"
#include "RHI/api/IRHISwapchain.hpp"

#include "ContentLoader/api/TextureLoader.hpp"

#include <GLFW/glfw3.h>
#include <vector>


namespace Context
{
    VulkanContext::VulkanContext()
    {
        m_WindowManager = std::make_unique<WinManager::WindowManager>(WinManager::WindowState::GetDefaultState());
        ContentLoader::TextureLoader texLoader;
        texLoader.LoadTexture("Textures\\Logo\\logo1.png");
        m_WindowManager->SetIcon(texLoader.GetWidth(), texLoader.GetHeight(), static_cast<unsigned char*>(texLoader.GetData()));

        // Legacy Wrappers device + swapchain (used by un-migrated render passes).
        m_Device = std::make_unique<Wrappers::Device>(*m_WindowManager);
        m_SwapChain = std::make_unique<Wrappers::SwapChain>(*m_Device, *m_WindowManager);

        // New RHI device + swapchain (used by migrated render passes).
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
        // Destroy RHI resources first (they are independent of legacy Wrappers objects).
        m_RHIDevice->WaitIdle();
        m_RHISwapchain.reset();
        m_RHIDevice.reset();

        // Then destroy legacy Wrappers resources.
        m_SwapChain->Cleanup(*m_Device);
        m_Device->Cleanup();
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

    const Wrappers::Device& VulkanContext::GetDevice() const
    {
        return *m_Device;
    }

    const Wrappers::SwapChain& VulkanContext::GetSwapChain() const
    {
        return *m_SwapChain;
    }

    Wrappers::SwapChain& VulkanContext::GetSwapChain()
    {
        return *m_SwapChain;
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

}

