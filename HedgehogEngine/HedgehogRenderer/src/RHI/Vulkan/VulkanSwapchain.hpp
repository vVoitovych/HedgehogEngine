#pragma once

#include "HedgehogRenderer/api/RHI/IRHISwapchain.hpp"
#include "VulkanTexture.hpp"

#include "volk.h"

#include <vector>

namespace RHI
{

class VulkanDevice;

class VulkanSwapchain final : public IRHISwapchain
{
public:
    VulkanSwapchain(VulkanDevice& device, uint32_t width, uint32_t height);
    ~VulkanSwapchain() override;

    VulkanSwapchain(const VulkanSwapchain&)            = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;
    VulkanSwapchain(VulkanSwapchain&&)                 = delete;
    VulkanSwapchain& operator=(VulkanSwapchain&&)      = delete;

    // IRHISwapchain
    uint32_t     GetImageCount() const override { return static_cast<uint32_t>(m_Textures.size()); }
    Format       GetFormat()     const override { return m_Format; }
    uint32_t     GetWidth()      const override { return m_Width;  }
    uint32_t     GetHeight()     const override { return m_Height; }

    IRHITexture& GetTexture(uint32_t index) override;

    uint32_t AcquireNextImage(IRHISemaphore& signalSemaphore) override;
    void     Present(uint32_t imageIndex, IRHISemaphore& waitSemaphore) override;
    void     Resize(uint32_t width, uint32_t height) override;

    VkSwapchainKHR GetHandle() const { return m_Swapchain; }

private:
    void Create(uint32_t width, uint32_t height);
    void Destroy();

    VulkanDevice&              m_Device;
    VkSwapchainKHR             m_Swapchain = VK_NULL_HANDLE;

    Format                     m_Format = Format::Undefined;
    uint32_t                   m_Width  = 0;
    uint32_t                   m_Height = 0;

    std::vector<VkImageView>   m_ImageViews;
    std::vector<VulkanTexture> m_Textures;
};

} // namespace RHI
