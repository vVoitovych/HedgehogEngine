#pragma once

#include "vma/vk_mem_alloc.h"

#include <vulkan/vulkan.h>

namespace Wrappers
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
		VkImage m_Image;
		VmaAllocation m_Allocation;

		VkImageView m_ImageView;
		VkFormat m_Format;
		VkExtent2D m_Extent;
	};

}


