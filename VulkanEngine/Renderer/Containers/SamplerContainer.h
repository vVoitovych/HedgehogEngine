#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <memory>

#include "Renderer/Wrappeers/Descriptors/Sampler.hpp"

namespace Renderer
{
	class Device;

	enum class SamplerType
	{
		Linear
	};

	class SamplerContainer
	{
	public:
		SamplerContainer();
		~SamplerContainer();

		SamplerContainer(const SamplerContainer&) = delete;
		SamplerContainer(SamplerContainer&&) = delete;
		SamplerContainer& operator=(const SamplerContainer&) = delete;
		SamplerContainer& operator=(SamplerContainer&&) = delete;

		void Initialize(const std::unique_ptr<Device>& device);
		void Cleanup(const std::unique_ptr<Device>& device);

		const Sampler& GetSampler(SamplerType type) const;

	private:
		std::vector<Sampler> mSamplers;

	};
}



