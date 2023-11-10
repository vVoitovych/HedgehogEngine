#include "TextureImage.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"

#include "VulkanEngine/Logger/Logger.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"
#include "VulkanEngine/ContentLoader/TextureLoader.hpp"

namespace Renderer
{
	TextureImage::TextureImage()
		: mTextureImage(nullptr)
		, mTextureImageMemory(nullptr)
		, mTextureImageView(nullptr)
	{
	}

	TextureImage::TextureImage(const std::string fileName)
		: mFileName(fileName)
		, mTextureImage(nullptr)
		, mTextureImageMemory(nullptr)
		, mTextureImageView(nullptr)
	{
	}

	TextureImage::~TextureImage()
	{
		if (mTextureImage != nullptr)
		{
			LOGERROR("Vulkan texture image should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mTextureImageMemory != nullptr)
		{
			LOGERROR("Vulkan texture image memory should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mTextureImageView != nullptr)
		{
			LOGERROR("Vulkan texture image view should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void TextureImage::SetFileName(const std::string fileName)
	{
		mFileName = fileName;
	}

	void TextureImage::Initialize(const Device& device, VkFormat format)
	{
		ContentLoader::TextureLoader textureLoader;
		textureLoader.LoadTexture(mFileName);
		int texWidth = textureLoader.GetWidth();
		int texHeight = textureLoader.GetHeight();
		VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight * 4);

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		device.CreateBuffer(
			imageSize, 
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
			stagingBuffer, stagingBufferMemory);
	
		device.CopyDataToBufferMemory(stagingBufferMemory, textureLoader.GetData(), static_cast<size_t>(imageSize));

		device.CreateImage(
			texWidth, texHeight, 
			VK_FORMAT_R8G8B8A8_SRGB, 
			VK_IMAGE_TILING_OPTIMAL, 
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
			mTextureImage, mTextureImageMemory);

		device.TransitionImageLayout(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		device.CopyBufferToImage(stagingBuffer, mTextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		device.TransitionImageLayout(mTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		device.DestroyBuffer(stagingBuffer, nullptr);
		device.FreeMemory(stagingBufferMemory, nullptr);

		mTextureImageView = device.CreateImageView(mTextureImage, format, VK_IMAGE_ASPECT_COLOR_BIT);

		LOGINFO("Vulkan texture image created");
	}

	void TextureImage::Cleanup(const Device& device)
	{
		device.DestroyImage(mTextureImage, nullptr);
		device.FreeMemory(mTextureImageMemory, nullptr);		
		vkDestroyImageView(device.GetNativeDevice(), mTextureImageView, nullptr);
		
		mTextureImageView = nullptr;
		mTextureImage = nullptr;
		mTextureImageMemory = nullptr;

		LOGINFO("Vulkan texture image cleaned");
	}

	VkImage TextureImage::GetNativeImage() const
	{
		return mTextureImage;
	}

	VkImageView TextureImage::GetNativeImageView() const
	{
		return mTextureImageView;
	}

}


