#include "TextureSampler.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"

#include "VulkanEngine/Logger/Logger.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"

namespace Renderer
{
	TextureSampler::TextureSampler(const std::unique_ptr<Device>& device)
		: mTextureSampler(nullptr)
	{
		VkPhysicalDeviceProperties properties = device->GetPhysicalDeviceProperties();

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

		if (vkCreateSampler(device->GetNativeDevice(), &samplerInfo, nullptr, &mTextureSampler) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create texture sampler!");
		}
		LOGINFO("Vulkan texture sampler created");
	}

	TextureSampler::~TextureSampler()
	{
		if (mTextureSampler != nullptr)
		{
			LOGERROR("Vulkan texture sampler should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void TextureSampler::Cleanup(const std::unique_ptr<Device>& device)
	{
		vkDestroySampler(device->GetNativeDevice(), mTextureSampler, nullptr);
		mTextureSampler = nullptr;
		LOGINFO("Vulkan texture sampler cleaned");
	}

	VkSampler TextureSampler::GetNativeSampler() const
	{
		return mTextureSampler;
	}

}





