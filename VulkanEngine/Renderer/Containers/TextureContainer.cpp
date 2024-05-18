#include "TextureContainer.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Resources/Image/Image.hpp"
#include "Renderer/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "Renderer/Wrappeers/Resources/Sampler/Sampler.hpp"
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
			LOGWARNING("Texture ", filePath, " already added");
		}
	}

	void TextureContaineer::ClearFileList()
	{
		mTexturesList.clear();
	}

	void TextureContaineer::Initialize(const Device& device)
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
				VMA_MEMORY_USAGE_CPU_TO_GPU);

			stageBuffer.CopyDataToBufferMemory(device, textureLoader.GetData(), static_cast<size_t>(imageSize));

			Image image(
				device,
				texWidth, texHeight,
				mTexturesList[i].format,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
			device.TransitionImageLayout(image.GetNativeImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
			device.CopyBufferToImage(stageBuffer.GetNativeBuffer(), image.GetNativeImage(), static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
			device.TransitionImageLayout(image.GetNativeImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			stageBuffer.DestroyBuffer(device);
			image.CreateImageView(device, mTexturesList[i].format, VK_IMAGE_ASPECT_COLOR_BIT);

			mImages.push_back(std::move(image));
			LOGINFO("Vulkan texture ", mTexturesList[i].filePath, " loaded and initialized");
		}

		mSamplersList.clear();
		Sampler linearSampler(device);
		mSamplersList.push_back(std::move(linearSampler));
	}

	void TextureContaineer::Cleanup(const Device& device)
	{
		for (size_t i = 0; i < mTexturesList.size(); ++i)
		{
			mImages[i].Cleanup(device);
		}
		mImages.clear();

		for (auto& sampler : mSamplersList)
		{
			sampler.Cleanup(device);
		}
		mSamplersList.clear();
	}

	const Image& TextureContaineer::GetImage(std::string filePath) const
	{
		auto it = std::find_if(mTexturesList.begin(), mTexturesList.end(), [&filePath](const TextureInfo& info) { return info.filePath == filePath; });
		if (it != mTexturesList.end())
		{
			return mImages[it - mTexturesList.begin()];
		}
		else
		{
			LOGERROR("TYexture ", filePath, " not found");
		}
	}

	const Image& TextureContaineer::GetImage(size_t index) const
	{
		return mImages[index];
	}

	size_t TypeToIndex(SamplerType type)
	{
		switch (type)
		{
		case Renderer::SamplerType::Linear:
			return 0;
			break;
		default:
			throw std::exception("Wrond sampler type");
		}
	}

	const Sampler& TextureContaineer::GetSampler(SamplerType type) const
	{
		return mSamplersList[TypeToIndex(type)];
	}

}



