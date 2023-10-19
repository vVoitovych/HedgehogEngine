#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;

	class TextureSampler
	{
	public:
		TextureSampler();
		~TextureSampler();

		TextureSampler(const TextureSampler&) = delete;
		TextureSampler& operator=(const TextureSampler&) = delete;

		void Initialize(const Device& device);
		void Cleanup(const Device& device);

		VkSampler GetNativeSampler() const;

	private:
		VkSampler mTextureSampler;
	};

}











