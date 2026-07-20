#include "VulkanSampler.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTypes.hpp"

#include <algorithm>
#include <cassert>

namespace RHI
{

VulkanSampler::VulkanSampler(VulkanDevice& device, const SamplerDesc& desc)
    : m_Device(device)
{
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_Device.GetPhysicalDevice(), &props);

    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter               = VulkanTypes::ToVkFilter(desc.MagFilter);
    samplerInfo.minFilter               = VulkanTypes::ToVkFilter(desc.MinFilter);
    samplerInfo.addressModeU            = VulkanTypes::ToVkAddressMode(desc.AddressModeU);
    samplerInfo.addressModeV            = VulkanTypes::ToVkAddressMode(desc.AddressModeV);
    samplerInfo.addressModeW            = VulkanTypes::ToVkAddressMode(desc.AddressModeW);
    samplerInfo.anisotropyEnable        = desc.MaxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy           = std::min(desc.MaxAnisotropy,
                                                    props.limits.maxSamplerAnisotropy);
    samplerInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable           = VK_FALSE;
    samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias              = 0.0f;
    samplerInfo.minLod                  = 0.0f;
    samplerInfo.maxLod                  = VK_LOD_CLAMP_NONE;

    VkResult result = vkCreateSampler(m_Device.GetHandle(), &samplerInfo, nullptr, &m_Sampler);
    assert(result == VK_SUCCESS && "Failed to create VkSampler.");
}

VulkanSampler::~VulkanSampler()
{
    if (m_Sampler != VK_NULL_HANDLE)
        vkDestroySampler(m_Device.GetHandle(), m_Sampler, nullptr);
}

} // namespace RHI
