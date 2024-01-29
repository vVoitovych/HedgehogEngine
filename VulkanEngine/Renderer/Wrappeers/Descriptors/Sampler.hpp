#pragma once

#include <vulkan/vulkan.h>
#include <memory>

namespace Renderer
{
	class Device;

	class Sampler
	{
	public:
		Sampler(const std::unique_ptr<Device>& device);
		~Sampler();

		Sampler(const Sampler&) = delete;
		Sampler& operator=(const Sampler&) = delete;

		Sampler(Sampler&& other) noexcept;
		Sampler& operator=(Sampler&& other) noexcept;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkSampler GetNativeSampler() const;

	private:
		VkSampler mSampler;
	};

}











