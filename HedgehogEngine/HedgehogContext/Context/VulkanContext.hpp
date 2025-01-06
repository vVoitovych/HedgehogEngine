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

		const Wrappers::Device& GetDevice() const;
		const Wrappers::SwapChain& GetSwapChain() const;
		Wrappers::SwapChain& GetSwapChain();

	private:
		std::unique_ptr<WinManager::WindowManager> mWindowManager;
		bool mWindowResized = false;

		std::unique_ptr<Wrappers::Device> mDevice;
		std::unique_ptr<Wrappers::SwapChain> mSwapChain;
	};

}




