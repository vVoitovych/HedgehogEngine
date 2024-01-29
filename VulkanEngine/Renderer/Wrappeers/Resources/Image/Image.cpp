#include "Image.hpp"
#include "ImageManagement.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Commands/CommandPool.hpp"
#include "Logger/Logger.hpp"
#include "Renderer/Common/EngineDebugBreak.hpp"

#include <stdexcept>

namespace Renderer
{
	Image::Image(
		const std::unique_ptr<Device>& device, 
		uint32_t width, 
		uint32_t height, 
		VkFormat format, 
		VkImageTiling tiling, 
		VkImageUsageFlags usage, 
		VkMemoryPropertyFlags properties)
		: mImage(nullptr)
		, mImageMemory(nullptr)
		, mImageView(nullptr)
		, mDevice(nullptr)
	{
		mDevice = device->GetNativeDevice();
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

		if (vkCreateImage(mDevice, &imageInfo, nullptr, &mImage) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create image!");
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(mDevice, mImage, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = device->FindMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(mDevice, &allocInfo, nullptr, &mImageMemory) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate image memory!");
		}

		vkBindImageMemory(mDevice, mImage, mImageMemory, 0);
	}

	Image::~Image()
	{
		if (mImage != nullptr)
		{
			LOGERROR("Vulkan image should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mImageMemory != nullptr)
		{
			LOGERROR("Vulkan image memory should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mImageView != nullptr)
		{
			LOGERROR("Vulkan image view should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	Image::Image(Image&& other) noexcept
		: mImage(other.mImage)
		, mImageMemory(other.mImageMemory)
		, mImageView(other.mImageView)
		, mDevice(other.mDevice)
	{
		other.mImage = nullptr;
		other.mImageMemory = nullptr;
		other.mImageView = nullptr;
	}

	Image& Image::operator=(Image&& other) noexcept
	{
		if (this != &other)
		{
			mImage = other.mImage;
			mImageMemory = other.mImageMemory;
			mImageView = other.mImageView;
			mDevice = other.mDevice;

			other.mImage = nullptr;
			other.mImageMemory = nullptr;
			other.mImageView = nullptr;
		}
		return *this;
	}

	void Image::Cleanup()
	{
		vkDestroyImage(mDevice, mImage, nullptr);
		vkFreeMemory(mDevice, mImageMemory, nullptr);
		vkDestroyImageView(mDevice, mImageView, nullptr);
		mImage = nullptr;
		mImageMemory = nullptr;
		mImageView = nullptr;

	}

	void Image::TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, const std::unique_ptr<CommandPool>& commandPool)
	{
		VkCommandBuffer commandBuffer = commandPool->BeginSingleTimeCommands();
		ImageManagement::RecordTransitionImageLayout(1, oldLayout, newLayout, false, commandBuffer, mImage);
		commandPool->EndSingleTimeCommands(commandBuffer);
	}

	void Image::CreateImageView(VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = mImage;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(mDevice, &viewInfo, nullptr, &mImageView) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture image view!");
		}

	}

	const VkImage& Image::GetNativeImage() const
	{
		return mImage;
	}

	const VkImageView& Image::GetNativeView() const
	{
		return mImageView;
	}



}

