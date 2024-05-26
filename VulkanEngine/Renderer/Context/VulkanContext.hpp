#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class WindowManager;
	class Device;
	class SwapChain;

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
		WindowManager& GetWindowManager();

		bool ShouldClose() const;
		void ResizeWindow();
		bool IsWindowResized();
		void ResetWindowResizeState();

		const Device& GetDevice() const;
		const SwapChain& GetSwapChain() const;
		SwapChain& GetSwapChain();

	private:
		std::unique_ptr<WindowManager> mWindowManager;
		bool mWindowResized = false;

		std::unique_ptr<Device> mDevice;
		std::unique_ptr<SwapChain> mSwapChain;
	};

}




