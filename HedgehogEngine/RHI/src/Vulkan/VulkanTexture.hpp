#pragma once

#include "RHI/api/IRHITexture.hpp"
#include "RHI/api/RHITypes.hpp"

#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>

#include <cstdint>

namespace RHI
{

class VulkanDevice;

class VulkanTexture final : public IRHITexture
{
public:
    // Regular texture — VMA managed.
    VulkanTexture(VulkanDevice& device, const TextureDesc& desc);

    // Swapchain image — externally managed (no VMA, no destroy on death).
    VulkanTexture(VkImage image, VkImageView view, Format format,
                  uint32_t width, uint32_t height);

    ~VulkanTexture() override;

    VulkanTexture(const VulkanTexture&)            = delete;
    VulkanTexture& operator=(const VulkanTexture&) = delete;
    VulkanTexture(VulkanTexture&&)                 = delete;
    VulkanTexture& operator=(VulkanTexture&&)      = delete;

    // IRHITexture
    uint32_t GetWidth()  const override { return m_Width;  }
    uint32_t GetHeight() const override { return m_Height; }
    Format   GetFormat() const override { return m_Format; }

    // Internal Vulkan accessors
    VkImage     GetHandle()     const { return m_Image; }
    VkImageView GetViewHandle() const { return m_View;  }

private:
    VulkanDevice* m_Device   = nullptr;   // nullptr for swapchain textures

    uint32_t      m_Width    = 0;
    uint32_t      m_Height   = 0;
    Format        m_Format   = Format::Undefined;

    VkImage       m_Image      = VK_NULL_HANDLE;
    VkImageView   m_View       = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;

    bool          m_OwnedBySwapchain = false;
};

} // namespace RHI
