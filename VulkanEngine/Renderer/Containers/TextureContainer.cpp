#include "TextureContainer.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/Resources/Image/Image.hpp"
#include "Renderer/Wrappeers/Resources/Buffer/Buffer.hpp"
#include "Renderer/Wrappeers/Resources/Sampler/Sampler.hpp"
#include "ContentLoader/TextureLoader.hpp"
#include "Logger/Logger.hpp"

#include <stdexcept>

namespace Renderer
{
	TextureContainer::TextureContainer()
	{

	}

	TextureContainer::~TextureContainer()
	{
	}

	void TextureContainer::Cleanup(const Device& device)
	{
		for (auto& image : mImages)
		{
			image.second.Cleanup(device);
		}
		mImages.clear();
		mTexturePathes.clear();

		for (auto& sampler : mSamplersList)
		{
			sampler.second.Cleanup(device);
		}
		mSamplersList.clear();
	}

	const Image& TextureContainer::CreateImage(const Device& device, std::string filePath) const
	{
		ContentLoader::TextureLoader textureLoader;
		textureLoader.LoadTexture(filePath);
		int texWidth = textureLoader.GetWidth();
		int texHeight = textureLoader.GetHeight();
		VkDeviceSize imageSize = static_cast<VkDeviceSize>(texWidth * texHeight * 4);

		Buffer stageBuffer(
			device,
			imageSize,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU);

		stageBuffer.CopyDataToBufferMemory(device, textureLoader.GetData(), static_cast<size_t>(imageSize));

		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB; //TODO: make different texture formats
		Image image(
			device,
			texWidth, texHeight,
			format,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		device.TransitionImageLayout(image.GetNativeImage(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		device.CopyBufferToImage(stageBuffer.GetNativeBuffer(), image.GetNativeImage(), static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		device.TransitionImageLayout(image.GetNativeImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		stageBuffer.DestroyBuffer(device);
		image.CreateImageView(device, format, VK_IMAGE_ASPECT_COLOR_BIT);

		auto result = mImages.emplace(filePath, std::move(image));
		mTexturePathes.push_back(filePath);

		LOGINFO("Vulkan texture ", filePath, " loaded and initialized");
		auto it = mImages.find(filePath);
		return it->second;
	}

	const Sampler& TextureContainer::CreateSampler(const Device& device, SamplerType type) const
	{
		if (type== Renderer::SamplerType::Linear)
		{
			Sampler linearSampler(device);
			mSamplersList.emplace(SamplerType::Linear, std::move(linearSampler));
			auto it = mSamplersList.find(type);
			return it->second;
		}
		else
		{
			throw std::runtime_error("unsupported sampler type");
		}
	}

	const Image& TextureContainer::GetImage(const Device& device, std::string filePath) const
	{
		auto it = mImages.find(filePath);
		if (it != mImages.end())
		{
			return it->second;
		}
		else
		{
			return CreateImage(device, filePath);
		}
	}

	const Sampler& TextureContainer::GetSampler(const Device& device, SamplerType type) const
	{
		auto it = mSamplersList.find(type);
		if (it != mSamplersList.end())
		{
			return it->second;
		}
		else
		{
			return CreateSampler(device, type);
		}

	}

	const std::vector<std::string>& TextureContainer::GetTexturePathes() const
	{
		return mTexturePathes;
	}

	size_t TextureContainer::GetTextureIndex(std::string name) const
	{
		auto it = std::find(mTexturePathes.begin(), mTexturePathes.end(), name);
		return it - mTexturePathes.begin();
	}

}



