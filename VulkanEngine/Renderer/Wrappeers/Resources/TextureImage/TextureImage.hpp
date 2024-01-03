#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <memory>

namespace Renderer
{
	class Device;

	class TextureImage
	{
	public:
		TextureImage(const std::unique_ptr<Device>& device, const std::string fileName, VkFormat format);
		~TextureImage();

		TextureImage(const TextureImage&) = delete;
		TextureImage& operator=(const TextureImage&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkImage GetNativeImage() const;
		VkImageView GetNativeImageView() const;

	private:
		VkImage mTextureImage;
		VkDeviceMemory mTextureImageMemory;

		VkImageView mTextureImageView;
	};

}


