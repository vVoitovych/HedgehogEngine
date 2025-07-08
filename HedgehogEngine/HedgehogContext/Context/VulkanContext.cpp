#include "VulkanContext.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogCommon/Common/RendererSettings.hpp"
#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

#include "HedgehogEngine/HedgehogCommon/CpuProfiler/CpuProfiler.hpp"
#include "ContentLoader/TextureLoader.hpp"

#include <vector>
#include <vulkan/vulkan.h>


namespace Context
{
	VulkanContext::VulkanContext()
	{
		m_WindowManager = std::make_unique<WinManager::WindowManager>(WinManager::WindowState::GetDefaultState());
		ContentLoader::TextureLoader texLoader;
		texLoader.LoadTexture("Textures\\Logo\\logo1.png");
		m_WindowManager->SetIcon(texLoader.GetWidth(), texLoader.GetHeight(), static_cast<unsigned char*>(texLoader.GetData()));

		m_Device = std::make_unique<Wrappers::Device>(*m_WindowManager);
		m_SwapChain = std::make_unique<Wrappers::SwapChain>(*m_Device, *m_WindowManager);
	}

	VulkanContext::~VulkanContext()
	{
	}

	void VulkanContext::Cleanup()
	{
		m_SwapChain->Cleanup(*m_Device);
		m_Device->Cleanup();
	}

	void VulkanContext::HandleInput()
	{
		START_TIME_STAMP("Handle input");
		m_WindowManager->HandleInput();
		END_TIME_STAMP("Handle input");
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

