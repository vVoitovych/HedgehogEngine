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
		std::unique_ptr<WinManager::WindowManager> m_WindowManager;
		bool m_WindowResized = false;

		std::unique_ptr<Wrappers::Device> m_Device;
		std::unique_ptr<Wrappers::SwapChain> m_SwapChain;
	};

}




