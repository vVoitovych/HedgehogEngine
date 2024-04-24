#pragma once

#include "ThirdParty/VulkanMemoryAllocator/vk_mem_alloc.h"

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;
	class CommandPool;

	class Image
	{
	public:
		Image(
			const std::unique_ptr<Device>& device,
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

		void Cleanup(const std::unique_ptr<Device>& device);

		void TransitionImageLayout(VkImageLayout oldLayout, VkImageLayout newLayout, const std::unique_ptr<CommandPool>& commandPool);
		void CreateImageView(const std::unique_ptr<Device>& device, VkFormat format, VkImageAspectFlags aspectFlags);

		const VkImage& GetNativeImage() const;
		const VkImageView& GetNativeView() const;

	private:
		VkImage mImage;
		VmaAllocation mAllocation;

		VkImageView mImageView;

	};

}


