#include "Sampler.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

namespace Wrappers
{
	Sampler::Sampler(const Device& device)
		: m_Sampler(nullptr)
	{
		VkPhysicalDeviceProperties properties = device.GetPhysicalDeviceProperties();

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		if (vkCreateSampler(device.GetNativeDevice(), &samplerInfo, nullptr, &m_Sampler) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture sampler!");
		}
		LOGINFO("Vulkan texture sampler created");
	}

	Sampler::~Sampler()
	{
		if (m_Sampler != nullptr)
		{
			LOGERROR("Vulkan texture sampler should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	Sampler::Sampler(Sampler&& other) noexcept
		: m_Sampler(other.m_Sampler)
	{
		other.m_Sampler = nullptr;
	}

	Sampler& Sampler::operator=(Sampler&& other) noexcept
	{
		if (&other != this)
		{
			m_Sampler = other.m_Sampler;
			other.m_Sampler = nullptr;
		}
		return *this;
	}

	void Sampler::Cleanup(const Device& device)
	{
		vkDestroySampler(device.GetNativeDevice(), m_Sampler, nullptr);
		m_Sampler = nullptr;
		LOGINFO("Vulkan texture sampler cleaned");
	}

	VkSampler Sampler::GetNativeSampler() const
	{
		return m_Sampler;
	}

}





