#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace Wrappers
{
	class Device;
	class Image;
	class Sampler;
}

namespace Context
{
	enum class SamplerType
	{
		Linear
	};

	class TextureContainer
	{
	public:
		TextureContainer();
		~TextureContainer();

		TextureContainer(const TextureContainer&) = delete;
		TextureContainer(TextureContainer&&) = delete;
		TextureContainer& operator=(const TextureContainer&) = delete;
		TextureContainer& operator=(TextureContainer&&) = delete;

		const Wrappers::Image& GetImage(const Wrappers::Device& device, std::string filePath) const;
		const Wrappers::Sampler& GetSampler(const Wrappers::Device& device, SamplerType type) const;
		const std::vector<std::string>& GetTexturePathes() const;
		size_t GetTextureIndex(std::string name) const;

		void Cleanup(const Wrappers::Device& device);
	private:
		const Wrappers::Image& CreateImage(const Wrappers::Device& device, std::string filePath) const;
		const Wrappers::Sampler& CreateSampler(const Wrappers::Device& device, SamplerType type) const;

	private:
		mutable std::unordered_map<SamplerType, Wrappers::Sampler> mSamplersList;
		mutable std::unordered_map<std::string, Wrappers::Image> mImages;
		mutable std::vector<std::string> mTexturePathes;
	};



}






