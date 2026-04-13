#include "Image.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

#include <stdexcept>

namespace Wrappers
{
	Image::Image(
		const Device& device, 
		uint32_t width, 
		uint32_t height, 
		VkFormat format, 
		VkImageTiling tiling, 
		VkImageUsageFlags usage, 
		VkMemoryPropertyFlags properties)
		: m_Image(nullptr)
		, m_Allocation(nullptr)
		, m_ImageView(nullptr)
		, m_Format(format)
	{
		m_Extent.width = width;
		m_Extent.height = height;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = format;
		imageInfo.tiling = tiling;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		if (vmaCreateImage(device.GetAllocator(), &imageInfo, &allocInfo, &m_Image, &m_Allocation, nullptr) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image!");
		}
	}

	Image::~Image()
	{
		if (m_Image != nullptr)
		{
			LOGERROR("Vulkan image should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_Allocation != nullptr)
		{
			LOGERROR("Vulkan image allocation should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_ImageView != nullptr)
		{
			LOGERROR("Vulkan image view should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	Image::Image(Image&& other) noexcept
		: m_Image(other.m_Image)
		, m_Allocation(other.m_Allocation)
		, m_ImageView(other.m_ImageView)
		, m_Extent(other.m_Extent)
		, m_Format(other.m_Format)
	{
		other.m_Image = nullptr;
		other.m_Allocation = nullptr;
		other.m_ImageView = nullptr;
	}

	Image& Image::operator=(Image&& other) noexcept
	{
		if (this != &other)
		{
			m_Image = other.m_Image;
			m_Allocation = other.m_Allocation;
			m_ImageView = other.m_ImageView;

			other.m_Image = nullptr;
			other.m_Allocation = nullptr;
			other.m_ImageView = nullptr;
		}
		return *this;
	}

	void Image::Cleanup(const Device& device)
	{
		vkDestroyImageView(device.GetNativeDevice(), m_ImageView, nullptr);

		vmaDestroyImage(device.GetAllocator(), m_Image, m_Allocation);
		m_Image = nullptr;
		m_Allocation = nullptr;
		m_ImageView = nullptr;

	}

	void Image::CreateImageView(const Device& device, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_Image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(device.GetNativeDevice(), &viewInfo, nullptr, &m_ImageView) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture image view!");
		}

	}

	const VkImage& Image::GetNativeImage() const
	{
		return m_Image;
	}

	const VkImageView& Image::GetNativeView() const
	{
		return m_ImageView;
	}

	VkFormat Image::GetFormat() const
	{
		return m_Format;
	}

	VkExtent2D Image::GetExtent() const
	{
		return m_Extent;
	}



}

