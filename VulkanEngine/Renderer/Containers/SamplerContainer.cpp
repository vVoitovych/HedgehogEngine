#include "SamplerContainer.h"

#include "Renderer/Wrappeers/Device/Device.hpp"

#include <stdexcept>

namespace Renderer
{
	SamplerContainer::SamplerContainer()
	{
	}

	SamplerContainer::~SamplerContainer()
	{
	}

	void SamplerContainer::Initialize(const std::unique_ptr<Device>& device)
	{
		mSamplers.clear();
		Sampler linearSampler(device);
		mSamplers.push_back(std::move(linearSampler));
	}

	void SamplerContainer::Cleanup(const std::unique_ptr<Device>& device)
	{
		for (auto& sampler : mSamplers)
		{
			sampler.Cleanup(device);
		}
		mSamplers.clear();
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

	const Sampler& SamplerContainer::GetSampler(SamplerType type) const
	{
		return mSamplers[TypeToIndex(type)];
	}

}


