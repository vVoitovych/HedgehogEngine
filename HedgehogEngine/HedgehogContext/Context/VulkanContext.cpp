#include "VulkanContext.hpp"

#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/SwapChain/SwapChain.hpp"
#include "HedgehogCommon/Common/RendererSettings.hpp"
#include "HedgehogRenderer/RenderPasses/GuiPass/GuiPass.hpp"

#include <vector>
#include <vulkan/vulkan.h>


namespace Context
{
	VulkanContext::VulkanContext()
	{
		mWindowManager = std::make_unique<WinManager::WindowManager>(WinManager::WindowState::GetDefaultState());
		mDevice = std::make_unique<Wrappers::Device>(*mWindowManager);
		mSwapChain = std::make_unique<Wrappers::SwapChain>(*mDevice, *mWindowManager);
	}

	VulkanContext::~VulkanContext()
	{
	}

	void VulkanContext::Cleanup()
	{
		mSwapChain->Cleanup(*mDevice);
		mDevice->Cleanup();
	}

	void VulkanContext::HandleInput()
	{
		mWindowManager->HandleInput();
	}

	WinManager::WindowManager& VulkanContext::GetWindowManager()
	{
		return *mWindowManager;
	}

	const WinManager::WindowManager& VulkanContext::GetWindowManager() const
	{
		return *mWindowManager;
	}

	const Wrappers::Device& VulkanContext::GetDevice() const
	{
		return *mDevice;
	}

	const Wrappers::SwapChain& VulkanContext::GetSwapChain() const
	{
		return *mSwapChain;
	}

	Wrappers::SwapChain& VulkanContext::GetSwapChain()
	{
		return *mSwapChain;
	}

	bool VulkanContext::ShouldClose() const
	{
		return mWindowManager->ShouldClose();
	}

	void VulkanContext::ResizeWindow()
	{
		mWindowResized = true;
	}

	bool VulkanContext::IsWindowResized()
	{
		return mWindowResized;
	}

	void VulkanContext::ResetWindowResizeState()
	{
		mWindowResized = false;
	}

}

