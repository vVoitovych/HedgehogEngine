#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;

	class TextureSampler
	{
	public:
		TextureSampler(const std::unique_ptr<Device>& device);
		~TextureSampler();

		TextureSampler(const TextureSampler&) = delete;
		TextureSampler& operator=(const TextureSampler&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkSampler GetNativeSampler() const;

	private:
		VkSampler mTextureSampler;
	};

}











