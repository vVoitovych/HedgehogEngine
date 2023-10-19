#pragma once

#include "VulkanEngine/Renderer/Device/Device.h"
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <vector>

namespace Renderer
{
	class Device;
	class WindowsManager;

	class SwapChain
	{
	public:
		SwapChain();
		~SwapChain();

		SwapChain(const SwapChain&) = delete;
		SwapChain& operator=(const SwapChain&) = delete;

		void Initialize(const Device& device, WindowManager& windowManager);
		void Cleanup(const Device& device);

		VkSwapchainKHR GetNativeSwapChain() const;

		VkFormat GetFormat();
		VkExtent2D GetSwapChainExtend();
		size_t GetSwapChainImagesSize() const;

		VkImageView GetNativeSwapChainImageView(size_t index) const;

	private:
		void CreateSwapChain(const Device& device);
		void CreateImageViews(const Device& device);

		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
		VkExtent2D ChooseSwapExtend(const VkSurfaceCapabilitiesKHR& capabilities) const;

	private:
		GLFWwindow* mWindow;

		VkSwapchainKHR mSwapChain;

		std::vector<VkImage> mSwapChainImages;
		VkFormat mSwapChainImageFormat;
		VkExtent2D mSwapChainExtent;

		std::vector<VkImageView> mSwapChainImageViews;

	};
}
