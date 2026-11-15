#pragma once

#include "RHI/api/IRHIPipeline.hpp"

#include <Volk/volk.h>

namespace RHI
{

class VulkanDevice;

class VulkanPipeline final : public IRHIPipeline
{
public:
    VulkanPipeline(VulkanDevice& device, const GraphicsPipelineDesc& desc);
    VulkanPipeline(VulkanDevice& device, const ComputePipelineDesc& desc);
    ~VulkanPipeline() override;

    VulkanPipeline(const VulkanPipeline&)            = delete;
    VulkanPipeline& operator=(const VulkanPipeline&) = delete;
    VulkanPipeline(VulkanPipeline&&)                 = delete;
    VulkanPipeline& operator=(VulkanPipeline&&)      = delete;

    VkPipeline       GetHandle() const  { return m_Pipeline; }
    VkPipelineLayout GetLayout() const  { return m_Layout;   }
    // Graphics or compute: where it, its sets and its push constants are bound.
    VkPipelineBindPoint GetBindPoint() const { return m_BindPoint; }

private:
    void CreateLayout(const std::vector<const IRHIDescriptorSetLayout*>& setLayouts,
                      const std::vector<PushConstantRange>&              pushConstantRanges);

    VulkanDevice&       m_Device;
    VkPipelineBindPoint m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    VkPipelineLayout m_Layout   = VK_NULL_HANDLE;
    VkPipeline       m_Pipeline = VK_NULL_HANDLE;
};

} // namespace RHI
