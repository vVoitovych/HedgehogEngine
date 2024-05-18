#include "SwapChain.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/WindowManagment/WindowManager.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"
#include "Logger/Logger.hpp"

#include <limits>
#include <algorithm>

namespace Renderer
{
	SwapChain::SwapChain(const Device& device, WindowManager& windowManager)
		: mSwapChain(nullptr)
		, mWindow(nullptr)
	{
		mWindow = windowManager.GetGlfwWindow();

		CreateSwapChain(device);
		CreateImageViews(device);
	}

	SwapChain::~SwapChain()
	{
		if (mSwapChain != nullptr)
		{
			LOGERROR("Vulkan swap chain should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}


	void SwapChain::Cleanup(const Device& device)
	{
		for (auto imageView : mSwapChainImageViews)
		{
			vkDestroyImageView(device.GetNativeDevice(), imageView, nullptr);
		}
		mSwapChainImageViews.clear();
		vkDestroySwapchainKHR(device.GetNativeDevice(), mSwapChain, nullptr);
		mSwapChain = nullptr;
		LOGINFO("Swap chain cleaned");
	}

	void SwapChain::Recreate(const Device& device)
	{
		Cleanup(device);

		CreateSwapChain(device);
		CreateImageViews(device);
	}

	VkSwapchainKHR SwapChain::GetNativeSwapChain() const
	{
		return mSwapChain;
	}

	VkFormat SwapChain::GetFormat() const
	{
		return mSwapChainImageFormat;
	}

	VkExtent2D SwapChain::GetSwapChainExtent() const
	{
		return mSwapChainExtent;
	}

	uint32_t SwapChain::GetMinImagesCount() const
	{
		return mMinImageCount;
	}

	size_t SwapChain::GetSwapChainImagesSize() const
	{
		return mSwapChainImages.size();
	}

	VkImageView SwapChain::GetNativeSwapChainImageView(size_t index) const
	{
		return mSwapChainImageViews[index];
	}

	VkImage SwapChain::GetSwapChainImage(size_t index) const
	{
		return mSwapChainImages[index];
	}

	void SwapChain::CreateSwapChain(const Device& device)
	{
		SwapChainSupportDetails swapChainSupport = device.QuerySwapChainSupport();

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.mFormats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.mPrecentModes);
		VkExtent2D extent = ChooseSwapExtend(swapChainSupport.mCapabilities);

		mMinImageCount = swapChainSupport.mCapabilities.minImageCount;
		uint32_t imageCount = mMinImageCount + 1;
		if (swapChainSupport.mCapabilities.maxImageCount > 0 && imageCount > swapChainSupport.mCapabilities.maxImageCount)
		{
			imageCount = swapChainSupport.mCapabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = device.GetNativeSurface();
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage =  VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		QueueFamilyIndices indices = device.GetIndicies();
		uint32_t queueFamilyIndices[] = { indices.mGraphicsFamily.value(), indices.mPresentFamily.value() };
		if (indices.mGraphicsFamily != indices.mPresentFamily)
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;
			createInfo.pQueueFamilyIndices = nullptr;
		}

		createInfo.preTransform = swapChainSupport.mCapabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = nullptr;

		if (vkCreateSwapchainKHR(device.GetNativeDevice(), &createInfo, nullptr, &mSwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device.GetNativeDevice(), mSwapChain, &imageCount, nullptr);
		mSwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device.GetNativeDevice(), mSwapChain, &imageCount, mSwapChainImages.data());

		mSwapChainImageFormat = surfaceFormat.format;
		uint32_t minValue = 1;
		mSwapChainExtent = extent;
		mSwapChainExtent.width = std::max(mSwapChainExtent.width, minValue);
		mSwapChainExtent.height = std::max(mSwapChainExtent.height, minValue);

		LOGINFO("Swap chain created with ", imageCount, " back buffers. Swap chain extent: (", extent.width, ", ", extent.height, ")");
	}

	VkSurfaceFormatKHR SwapChain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) const
	{
		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}
		return availableFormats[0];
	}

	VkPresentModeKHR SwapChain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) const
	{
		for (const auto& availablePresentMode : availablePresentModes)
		{
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
				return availablePresentMode;
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D SwapChain::ChooseSwapExtend(const VkSurfaceCapabilitiesKHR& capabilities) const
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width;
			int height;

				glfwGetFramebufferSize(mWindow, &width, &height);

			VkExtent2D actualExtend = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
			actualExtend.width = std::clamp(actualExtend.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtend.height = std::clamp(actualExtend.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtend;
		}
	}

	void SwapChain::CreateImageViews(const Device& device)
	{
		mSwapChainImageViews.resize(mSwapChainImages.size());
		for (size_t i = 0; i < mSwapChainImages.size(); ++i)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = mSwapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = mSwapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device.GetNativeDevice(), &createInfo, nullptr, &mSwapChainImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image view!");
			}
		}

		LOGINFO("Swap chain image views created");

	}


}

