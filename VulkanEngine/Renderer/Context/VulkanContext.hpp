#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class WindowManager;
	class Device;
	class CommandPool;
	class DescriptorPool;
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
		const std::unique_ptr<WindowManager>& GetWindowManager() const;

		bool ShouldClose() const;
		void ResizeWindow();
		bool IsWindowResized();
		void ResetWindowResizeState();

		const std::unique_ptr<Device>& GetDevice() const;
		const std::unique_ptr<SwapChain>& GetSwapChain() const;

		const std::unique_ptr<CommandPool>& GetCommandPool() const;
		const std::unique_ptr<DescriptorPool>& GetDescriptorPool() const;

	private:
		std::unique_ptr<WindowManager> mWindowManager;
		bool mWindowResized = false;

		std::unique_ptr<Device> mDevice;
		std::unique_ptr<SwapChain> mSwapChain;

		std::unique_ptr<CommandPool> mCommandPool;
		std::unique_ptr<DescriptorPool> mDescriptorPool;
	};

}




