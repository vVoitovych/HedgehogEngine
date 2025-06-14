#include "SwapChain.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/WindowManagment/WindowManager.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

#include "GLFW/glfw3.h"

#include <limits>
#include <algorithm>

namespace Wrappers
{
	SwapChain::SwapChain(const Device& device, WinManager::WindowManager& windowManager)
		: m_SwapChain(nullptr)
		, m_Window(nullptr)
	{
		m_Window = windowManager.GetGlfwWindow();

		CreateSwapChain(device);
		CreateImageViews(device);
	}

	SwapChain::~SwapChain()
	{
		if (m_SwapChain != nullptr)
		{
			LOGERROR("Vulkan swap chain should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}


	void SwapChain::Cleanup(const Device& device)
	{
		for (auto imageView : m_SwapChainImageViews)
		{
			vkDestroyImageView(device.GetNativeDevice(), imageView, nullptr);
		}
		m_SwapChainImageViews.clear();
		vkDestroySwapchainKHR(device.GetNativeDevice(), m_SwapChain, nullptr);
		m_SwapChain = nullptr;
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
		return m_SwapChain;
	}

	VkFormat SwapChain::GetFormat() const
	{
		return m_SwapChainImageFormat;
	}

	VkExtent2D SwapChain::GetSwapChainExtent() const
	{
		return m_SwapChainExtent;
	}

	uint32_t SwapChain::GetMinImagesCount() const
	{
		return m_MinImageCount;
	}

	size_t SwapChain::GetSwapChainImagesSize() const
	{
		return m_SwapChainImages.size();
	}

	VkImageView SwapChain::GetNativeSwapChainImageView(size_t index) const
	{
		return m_SwapChainImageViews[index];
	}

	VkImage SwapChain::GetSwapChainImage(size_t index) const
	{
		return m_SwapChainImages[index];
	}

	void SwapChain::CreateSwapChain(const Device& device)
	{
		SwapChainSupportDetails swapChainSupport = device.QuerySwapChainSupport();

		VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.precentModes);
		VkExtent2D extent = ChooseSwapExtend(swapChainSupport.capabilities);

		m_MinImageCount = swapChainSupport.capabilities.minImageCount;
		uint32_t imageCount = m_MinImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = device.GetNativeSurface();
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		createInfo.imageArrayLayers = 1;
		createInfo.imageUsage =  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = device.GetIndicies();
		uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
		if (indices.graphicsFamily != indices.presentFamily)
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

		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		createInfo.clipped = VK_TRUE;
		createInfo.oldSwapchain = nullptr;

		if (vkCreateSwapchainKHR(device.GetNativeDevice(), &createInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create swap chain!");
		}

		vkGetSwapchainImagesKHR(device.GetNativeDevice(), m_SwapChain, &imageCount, nullptr);
		m_SwapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device.GetNativeDevice(), m_SwapChain, &imageCount, m_SwapChainImages.data());

		m_SwapChainImageFormat = surfaceFormat.format;
		uint32_t minValue = 1;
		m_SwapChainExtent = extent;
		m_SwapChainExtent.width = std::max(m_SwapChainExtent.width, minValue);
		m_SwapChainExtent.height = std::max(m_SwapChainExtent.height, minValue);

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

				glfwGetFramebufferSize(m_Window, &width, &height);

			VkExtent2D actualExtend = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
			actualExtend.width = std::clamp(actualExtend.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtend.height = std::clamp(actualExtend.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtend;
		}
	}

	void SwapChain::CreateImageViews(const Device& device)
	{
		m_SwapChainImageViews.resize(m_SwapChainImages.size());
		for (size_t i = 0; i < m_SwapChainImages.size(); ++i)
		{
			VkImageViewCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = m_SwapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = m_SwapChainImageFormat;
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;
			
			if (vkCreateImageView(device.GetNativeDevice(), &createInfo, nullptr, &m_SwapChainImageViews[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create image view!");
			}
		}

		LOGINFO("Swap chain image views created");

	}


}

