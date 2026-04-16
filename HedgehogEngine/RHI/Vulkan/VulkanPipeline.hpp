#pragma once

#include "../RHI/IRHIPipeline.hpp"

#include "volk.h"

namespace RHI
{

class VulkanDevice;

class VulkanPipeline final : public IRHIPipeline
{
public:
    VulkanPipeline(VulkanDevice& device, const GraphicsPipelineDesc& desc);
    ~VulkanPipeline() override;

    VulkanPipeline(const VulkanPipeline&)            = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;
    VulkanPipeline(VulkanPipeline&&)                 = delete;
    VulkanPipeline& operator=(VulkanPipeline&&)      = delete;

    VkPipeline       GetHandle() const  { return m_Pipeline; }
    VkPipelineLayout GetLayout() const  { return m_Layout;   }

private:
    VulkanDevice&    m_Device;
    VkPipelineLayout m_Layout   = VK_NULL_HANDLE;
    VkPipeline       m_Pipeline = VK_NULL_HANDLE;
};

} // namespace RHI
