#include "VulkanTextureView.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTexture.hpp"
#include "VulkanTypes.hpp"

#include <cassert>

namespace RHI
{

namespace
{
    // View creation needs concrete counts — REMAINING_MIP_LEVELS/REMAINING_ARRAY_LAYERS (valid
    // for barriers) are resolved here against the source texture's own TextureDesc.
    TextureSubresourceRange ResolveRange(const TextureDesc& textureDesc, const TextureSubresourceRange& range)
    {
        TextureSubresourceRange resolved = range;
        if (resolved.MipLevelCount == REMAINING_MIP_LEVELS)
            resolved.MipLevelCount = textureDesc.MipLevels - resolved.BaseMipLevel;
        if (resolved.ArrayLayerCount == REMAINING_ARRAY_LAYERS)
            resolved.ArrayLayerCount = textureDesc.ArrayLayers - resolved.BaseArrayLayer;
        return resolved;
    }
}

VulkanTextureView::VulkanTextureView(VulkanDevice& device, const VulkanTexture& texture,
                                     const TextureSubresourceRange& range)
    : m_Device(device)
    , m_Range(ResolveRange(texture.GetDesc(), range))
{
    const TextureDesc& textureDesc = texture.GetDesc();

    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image            = texture.GetHandle();
    viewInfo.viewType         = VulkanTypes::ToVkImageViewType(textureDesc.Type);
    viewInfo.format           = VulkanTypes::ToVkFormat(textureDesc.Format);
    viewInfo.subresourceRange = VulkanTypes::ToVkSubresourceRange(
        m_Range, VulkanTypes::GetAspectMask(textureDesc.Format));

    VkResult result = vkCreateImageView(m_Device.GetHandle(), &viewInfo, nullptr, &m_View);
    assert(result == VK_SUCCESS && "Failed to create VulkanTextureView.");
}

VulkanTextureView::~VulkanTextureView()
{
    if (m_View != VK_NULL_HANDLE)
        vkDestroyImageView(m_Device.GetHandle(), m_View, nullptr);
}

} // namespace RHI
