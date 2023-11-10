#include "Texture2D.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Logger/Logger.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"
#include "VulkanEngine/ContentLoader/TextureLoader.hpp"

namespace Renderer
{
	Texture2D::Texture2D()
		: mImage(nullptr)
		, mImageMemory(nullptr)
		, mImageView(nullptr)
	{
	}

	Texture2D::~Texture2D()
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

	Texture2D::Texture2D(const Texture2D& rhs)
	{
		mImage = rhs.mImage;
		mImageMemory = rhs.mImageMemory;
		mImageView = rhs.mImageView;
	}

	Texture2D& Texture2D::operator=(const Texture2D& rhs)
	{
		if (&rhs == this)
			return *this;
		mImage = rhs.mImage;
		mImageMemory = rhs.mImageMemory;
		mImageView = rhs.mImageView;
		return *this;
	}

	void Texture2D::Initialize(const Device& device, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags propertie)
	{
		device.CreateImage(width, height, format, tiling, usage, propertie,	mImage, mImageMemory);

		LOGINFO("Vulkan texture2D image created");
	}

	void Texture2D::Initialize(const Device& device, std::string fileName, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags propertie)
	{
		ContentLoader::TextureLoader textureLoader;
		textureLoader.LoadTexture(fileName);
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

		device.CreateImage(	texWidth, texHeight, format, tiling, VK_IMAGE_USAGE_TRANSFER_DST_BIT | usage, propertie,
			mImage, mImageMemory);

		device.TransitionImageLayout(mImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		device.CopyBufferToImage(stagingBuffer, mImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		device.TransitionImageLayout(mImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		device.DestroyBuffer(stagingBuffer, nullptr);
		device.FreeMemory(stagingBufferMemory, nullptr);

		LOGINFO("Vulkan texture2D image created");
	}

	void Texture2D::CreateImageView(const Device& device, VkFormat format, VkImageAspectFlags flags)
	{
		mImageView = device.CreateImageView(mImage, format, flags);
	}

	void Texture2D::Cleanup(const Device& device)
	{
		device.DestroyImage(mImage, nullptr);
		device.FreeMemory(mImageMemory, nullptr);
		vkDestroyImageView(device.GetNativeDevice(), mImageView, nullptr);

		mImage = nullptr;
		mImageMemory = nullptr;
		mImageView = nullptr;

		LOGINFO("Vulkan Texture2D cleaned");
	}

	VkImage Texture2D::GetNativeImage() const
	{
		return mImage;
	}

	VkImageView Texture2D::GetImageView() const
	{
		return mImageView;
	}

}


