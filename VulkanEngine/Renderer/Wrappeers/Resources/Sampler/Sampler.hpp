#pragma once

#include <vulkan/vulkan.h>

namespace Renderer
{
	class Device;

	class Sampler
	{
	public:
		Sampler(const Device& device);
		~Sampler();

		Sampler(const Sampler&) = delete;
		Sampler& operator=(const Sampler&) = delete;

		Sampler(Sampler&& other) noexcept;
		Sampler& operator=(Sampler&& other) noexcept;

		void Cleanup(const Device& device);

		VkSampler GetNativeSampler() const;

	private:
		VkSampler mSampler;
	};

}











