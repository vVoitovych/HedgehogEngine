#pragma once

#include "RHI/api/IRHISampler.hpp"
#include "RHI/api/RHITypes.hpp"

#include <Volk/volk.h>

namespace RHI
{

class VulkanDevice;

class VulkanSampler final : public IRHISampler
{
public:
    VulkanSampler(VulkanDevice& device, const SamplerDesc& desc);
    ~VulkanSampler() override;

    VulkanSampler(const VulkanSampler&)            = delete;
    VulkanSampler& operator=(const VulkanSampler&) = delete;
    VulkanSampler(VulkanSampler&&)                 = delete;
    VulkanSampler& operator=(VulkanSampler&&)      = delete;

    VkSampler GetHandle() const { return m_Sampler; }

private:
    VulkanDevice& m_Device;
    VkSampler     m_Sampler = VK_NULL_HANDLE;
};

} // namespace RHI
