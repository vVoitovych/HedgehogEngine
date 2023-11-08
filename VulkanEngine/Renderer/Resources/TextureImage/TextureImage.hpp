#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace Renderer
{
	class Device;

	class TextureImage
	{
	public:
		TextureImage();
		TextureImage(const std::string fileName);
		~TextureImage();

		TextureImage(const TextureImage&) = delete;
		TextureImage& operator=(const TextureImage&) = delete;

		void SetFileName(const std::string fileName);

		void Initialize(const Device& device, VkFormat format);
		void Cleanup(const Device& device);

		VkImage GetNativeImage() const;
		VkImageView GetNativeImageView() const;

	private:
		std::string mFileName;

		VkImage mTextureImage;
		VkDeviceMemory mTextureImageMemory;

		VkImageView mTextureImageView;
	};

}


