#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace Renderer
{
	class Device;
	class Image;
	class Sampler;

	struct TextureInfo
	{
		std::string filePath;
		VkFormat format;
	};

	enum class SamplerType
	{
		Linear
	};

	class TextureContaineer
	{
	public:
		TextureContaineer();
		~TextureContaineer();

		TextureContaineer(const TextureContaineer&) = delete;
		TextureContaineer(TextureContaineer&&) = delete;
		TextureContaineer& operator=(const TextureContaineer&) = delete;
		TextureContaineer& operator=(TextureContaineer&&) = delete;

		void AddTexture(std::string filePath, VkFormat format);
		void ClearFileList();

		void Initialize(const Device& device);
		void Cleanup(const Device& device);

		const Image& GetImage(std::string filePath) const;
		const Image& GetImage(size_t index) const;
		const Sampler& GetSampler(SamplerType type) const;

	private:
		std::vector<TextureInfo> mTexturesList;
		std::vector<Sampler> mSamplersList;
		std::vector<Image> mImages;
	};



}






