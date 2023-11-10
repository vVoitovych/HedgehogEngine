#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace Renderer
{
	class Device;

	class Texture2D
	{
	public:
		Texture2D();
		~Texture2D();

		Texture2D(const Texture2D& rhs);
		Texture2D& operator=(const Texture2D& rhs);

		void Initialize(
			const Device& device, 
			uint32_t width, 
			uint32_t height, 
			VkFormat format, 
			VkImageTiling tiling,
			VkImageUsageFlags usage, 
			VkMemoryPropertyFlags propertie);

		void Initialize(
			const Device& device,
			std::string fileName,
			VkFormat format,
			VkImageTiling tiling,
			VkImageUsageFlags usage,
			VkMemoryPropertyFlags propertie);

		void CreateImageView(const Device& device, VkFormat format, VkImageAspectFlags flags);

		void Cleanup(const Device& device);

		VkImage GetNativeImage() const;
		VkImageView GetImageView() const;;
	private:
		VkImage mImage;
		VkDeviceMemory mImageMemory;

		VkImageView mImageView;

	};

}


