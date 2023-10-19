#include "TextureImage.h"

#include "VulkanEngine/Renderer/Device/Device.h"

#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"
#include "VulkanEngine/ContentLoader/TextureLoader.h"

namespace Renderer
{
	TextureImage::TextureImage()
	{
	}

	TextureImage::TextureImage(const std::string fileName)
		: mFileName(fileName)
		, mTextureImage(nullptr)
		, mTextureImageMemory(nullptr)
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
	}

	void TextureImage::SetFileName(const std::string fileName)
	{
		mFileName = fileName;
	}

	void TextureImage::Initialize(const Device& device)
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

		LOGINFO("Vulkan texture image created");
	}

	void TextureImage::Cleanup(const Device& device)
	{
		device.DestroyImage(mTextureImage, nullptr);
		device.FreeMemory(mTextureImageMemory, nullptr);
		mTextureImage = nullptr;
		mTextureImageMemory = nullptr;

		LOGINFO("Vulkan texture image cleaned");
	}

	VkImage TextureImage::GetNativeImage() const
	{
		return mTextureImage;
	}

}


