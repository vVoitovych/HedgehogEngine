#pragma once

#include "../RHI/IRHIRenderPass.hpp"

#include "volk.h"

namespace RHI
{

class VulkanDevice;

class VulkanRenderPass final : public IRHIRenderPass
{
public:
    VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc);
    ~VulkanRenderPass() override;

    VulkanRenderPass(const VulkanRenderPass&)            = delete;
    VulkanRenderPass& operator=(const VulkanRenderPass&) = delete;
    VulkanRenderPass(VulkanRenderPass&&)                 = delete;
    VulkanRenderPass& operator=(VulkanRenderPass&&)      = delete;

    VkRenderPass GetHandle() const { return m_RenderPass; }

private:
    VulkanDevice& m_Device;
    VkRenderPass  m_RenderPass = VK_NULL_HANDLE;
};

} // namespace RHI
