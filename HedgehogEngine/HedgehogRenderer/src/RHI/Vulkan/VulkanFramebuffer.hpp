#pragma once

#include "HedgehogRenderer/api/RHI/IRHIFramebuffer.hpp"

#include "volk.h"

namespace RHI
{

class VulkanDevice;

class VulkanFramebuffer final : public IRHIFramebuffer
{
public:
    VulkanFramebuffer(VulkanDevice& device, const FramebufferDesc& desc);
    ~VulkanFramebuffer() override;

    VulkanFramebuffer(const VulkanFramebuffer&)            = delete;
    VulkanFramebuffer& operator=(const VulkanFramebuffer&) = delete;
    VulkanFramebuffer(VulkanFramebuffer&&)                 = delete;
    VulkanFramebuffer& operator=(VulkanFramebuffer&&)      = delete;

    uint32_t GetWidth()  const override { return m_Width;  }
    uint32_t GetHeight() const override { return m_Height; }

    VkFramebuffer GetHandle() const { return m_Framebuffer; }

private:
    VulkanDevice& m_Device;
    VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
    uint32_t      m_Width       = 0;
    uint32_t      m_Height      = 0;
};

} // namespace RHI
