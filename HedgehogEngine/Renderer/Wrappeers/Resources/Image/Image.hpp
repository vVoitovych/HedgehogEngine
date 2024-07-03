#pragma once

#include "ThirdParty/VulkanMemoryAllocator/vk_mem_alloc.h"

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;
	class CommandPool;

	class Image
	{
	public:
		Image(
			const Device& device,
			uint32_t width, 
			uint32_t height, 
			VkFormat format, 
			VkImageTiling tiling, 
			VkImageUsageFlags usage, 
			VkMemoryPropertyFlags properties);
		~Image();

		Image(const Image&) = delete;
		Image& operator=(const Image&) = delete;

		Image(Image&& other) noexcept;
		Image& operator=(Image&& other) noexcept;

		void Cleanup(const Device& device);

		void CreateImageView(const Device& device, VkFormat format, VkImageAspectFlags aspectFlags);

		const VkImage& GetNativeImage() const;
		const VkImageView& GetNativeView() const;
		VkFormat GetFormat() const;
		VkExtent2D GetExtent() const;

	private:
		VkImage mImage;
		VmaAllocation mAllocation;

		VkImageView mImageView;
		VkFormat mFormat;
		VkExtent2D mExtent;
	};

}


