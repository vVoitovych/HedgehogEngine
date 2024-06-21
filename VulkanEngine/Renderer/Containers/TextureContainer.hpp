#pragma once

#include <vulkan/vulkan.h>

#include <string>
#include <unordered_map>

namespace Renderer
{
	class Device;
	class Image;
	class Sampler;

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

		const Image& GetImage(const Device& device, std::string filePath) const;
		const Sampler& GetSampler(const Device& device, SamplerType type) const;

		void Cleanup(const Device& device);
	private:
		const Image& CreateImage(const Device& device, std::string filePath) const;
		const Sampler& CreateSampler(const Device& device, SamplerType type) const;

	private:
		mutable std::unordered_map<SamplerType, Sampler> mSamplersList;
		mutable std::unordered_map<std::string, Image> mImages;
	};



}






