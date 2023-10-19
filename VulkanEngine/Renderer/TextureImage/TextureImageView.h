#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;
	class TextureImage;

	class TextureImageView
	{
	public:
		TextureImageView();
		~TextureImageView();

		TextureImageView(const TextureImageView&) = delete;
		TextureImageView& operator=(const TextureImageView&) = delete;

		void Initialize(const Device& device, const TextureImage& textureImage, VkFormat format);
		void Cleanup(const Device& device);
	private:
		VkImageView mTextureImageView;

	};
}




