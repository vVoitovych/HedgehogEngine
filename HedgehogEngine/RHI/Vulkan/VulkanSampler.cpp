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
    samplerInfo.magFilter               = VulkanTypes::ToVkFilter(desc.m_MagFilter);
    samplerInfo.minFilter               = VulkanTypes::ToVkFilter(desc.m_MinFilter);
    samplerInfo.addressModeU            = VulkanTypes::ToVkAddressMode(desc.m_AddressModeU);
    samplerInfo.addressModeV            = VulkanTypes::ToVkAddressMode(desc.m_AddressModeV);
    samplerInfo.addressModeW            = VulkanTypes::ToVkAddressMode(desc.m_AddressModeW);
    samplerInfo.anisotropyEnable        = desc.m_MaxAnisotropy > 1.0f ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy           = std::min(desc.m_MaxAnisotropy,
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
