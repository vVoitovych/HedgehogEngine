#pragma once

#include "RHI/api/IRHITextureView.hpp"
#include "RHI/api/RHITypes.hpp"

#include <Volk/volk.h>

namespace RHI
{

class VulkanDevice;
class VulkanTexture;

class VulkanTextureView final : public IRHITextureView
{
public:
    VulkanTextureView(VulkanDevice& device, const VulkanTexture& texture,
                       const TextureSubresourceRange& range);
    ~VulkanTextureView() override;

    VulkanTextureView(const VulkanTextureView&)            = delete;
    VulkanTextureView& operator=(const VulkanTextureView&) = delete;
    VulkanTextureView(VulkanTextureView&&)                 = delete;
    VulkanTextureView& operator=(VulkanTextureView&&)      = delete;

    const TextureSubresourceRange& GetRange() const override { return m_Range; }

    VkImageView GetHandle() const { return m_View; }

private:
    VulkanDevice&            m_Device;
    TextureSubresourceRange  m_Range;
    VkImageView              m_View = VK_NULL_HANDLE;
};

} // namespace RHI
