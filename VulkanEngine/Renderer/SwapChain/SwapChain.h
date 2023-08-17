#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"
#include "VulkanEngine/Renderer/Device/Device.h"

namespace Renderer
{
	class WindowsManager;

	class SwapChain
	{
	public:
		SwapChain();
		~SwapChain();

		SwapChain(const SwapChain&) = delete;
		SwapChain& operator=(const SwapChain&) = delete;

		void Initialize(Device& device, WindowManager& windowManager);
		void Cleanup();

		VkSwapchainKHR GetNativeSwapChain() const;

		VkFormat GetFormat();
		VkExtent2D GetSwapChainExtend();
		size_t GetSwapChainImagesSize() const;

		VkImageView GetNativeSwapChainImageView(size_t index) const;

	private:
		void CreateSwapChain();
		void CreateImageViews();

		SwapChainSupportDetails QuerySwapChainSupport() const;
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
		VkExtent2D ChooseSwapExtend(const VkSurfaceCapabilitiesKHR& capabilities) const;

	private:
		VkDevice mDevice;
		VkSurfaceKHR mSurface;
		VkPhysicalDevice mPhysicalDevice;
		QueueFamilyIndices mIndicies;
		GLFWwindow* mWindow;

		VkSwapchainKHR mSwapChain;

		std::vector<VkImage> mSwapChainImages;
		VkFormat mSwapChainImageFormat;
		VkExtent2D mSwapChainExtent;

		std::vector<VkImageView> mSwapChainImageViews;

	};
}
