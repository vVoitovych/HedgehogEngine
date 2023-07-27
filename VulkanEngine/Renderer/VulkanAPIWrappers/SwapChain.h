#pragma once

#include "../Common/pch.h"
#include "Device.h"
#include "../WindowManagment/WindowManager.h"

namespace Renderer
{

	class SwapChain
	{
	public:
		SwapChain();
		~SwapChain();

		SwapChain(const SwapChain&) = delete;
		SwapChain& operator=(const SwapChain&) = delete;

		void Initialize(Device& device, WindowManager& windowManager);
		void Cleanup(Device& device);

		VkSwapchainKHR GetSwapChain();

		VkFormat GetFormat();
		VkExtent2D GetSwapChainExtend();
		size_t GetSwapChainImagesSize() const;

		VkImageView GetSwapChainImageView(size_t index) const;

	private:
		void CreateSwapChain(Device& device, WindowManager& windowManager);
		void CreateImageViews(Device& device);

		SwapChainSupportDetails QuerySwapChainSupport(Device& device) const;
		VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
		VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const;
		VkExtent2D ChooseSwapExtend(const VkSurfaceCapabilitiesKHR& capabilities, WindowManager& windowManager) const;

	private:
		VkSwapchainKHR mSwapChain;

		std::vector<VkImage> mSwapChainImages;
		VkFormat mSwapChainImageFormat;
		VkExtent2D mSwapChainExtent;

		std::vector<VkImageView> mSwapChainImageViews;

	};
}
