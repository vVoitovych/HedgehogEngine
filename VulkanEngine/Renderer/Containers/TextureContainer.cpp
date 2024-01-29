#include "TextureContainer.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Commands/CommandPool.hpp"
#include "Renderer/Wrappeers/Resources/Image/Image.hpp"
#include "Renderer/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "ContentLoader/TextureLoader.hpp"
#include "Logger/Logger.hpp"

namespace Renderer
{
	TextureContaineer::TextureContaineer()
	{
	}

	TextureContaineer::~TextureContaineer()
	{
	}

	void TextureContaineer::AddTexture(std::string filePath, VkFormat format)
	{
		auto it = std::find_if(mTexturesList.begin(), mTexturesList.end(), [&filePath](const TextureInfo& info) { return info.filePath == filePath; });
		if (it == mTexturesList.end())
		{
			mTexturesList.push_back({ filePath, format });
		}
		else
		{
			LOGWARNING("Mesh ", filePath, " already added");
		}
	}

	void TextureContaineer::ClearFileList()
	{
		mTexturesList.clear();
	}

	void TextureContaineer::Initialize(const std::unique_ptr<Device>& device, const std::unique_ptr<CommandPool>& commandPool)
	{
		for (size_t i = 0; i < mTexturesList.size(); ++i)
		{
			ContentLoader::TextureLoader textureLoader;
			textureLoader.LoadTexture(mTexturesList[i].filePath);
			int texWidth = textureLoader.GetWidth();
			int texHeight = textureLoader.GetHeight();
			VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight * 4);

			Buffer stageBuffer(
				device,
				imageSize,
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

			stageBuffer.CopyDataToBufferMemory(textureLoader.GetData(), static_cast<size_t>(imageSize));

			Image image(
				device,
				texWidth, texHeight,
				mTexturesList[i].format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

			image.TransitionImageLayout(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandPool);
			stageBuffer.CopyBufferToImage(image.GetNativeImage(), static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), commandPool);
			image.TransitionImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandPool);

			stageBuffer.DestroyBuffer();

			image.CreateImageView(mTexturesList[i].format, VK_IMAGE_ASPECT_COLOR_BIT);

			mImages.push_back(std::move(image));
			LOGINFO("Vulkan texture ", mTexturesList[i].filePath, " loaded and initialized");
		}
	}

	void TextureContaineer::Cleanup()
	{
		for (size_t i = 0; i < mTexturesList.size(); ++i)
		{
			mImages[i].Cleanup();
		}
		mImages.clear();
	}

	const Image& TextureContaineer::GetImage(size_t index) const
	{
		return mImages[index];
	}

}



