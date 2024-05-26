#include "VulkanContext.hpp"

#include "Renderer/WindowManagment/WindowManager.hpp"
#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Common/RendererSettings.hpp"

#include "Renderer/RenderPasses/GuiPass.hpp"

#include <vector>

namespace Renderer
{
	VulkanContext::VulkanContext()
	{
		mWindowManager = std::make_unique<WindowManager>(WindowState::GetDefaultState());
		mDevice = std::make_unique<Device>(*mWindowManager);
		mSwapChain = std::make_unique<SwapChain>(*mDevice, *mWindowManager);
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

	WindowManager& VulkanContext::GetWindowManager() 
	{
		return *mWindowManager;
	}

	const Device& VulkanContext::GetDevice() const
	{
		return *mDevice;
	}

	const SwapChain& VulkanContext::GetSwapChain() const
	{
		return *mSwapChain;
	}

	SwapChain& VulkanContext::GetSwapChain()
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

