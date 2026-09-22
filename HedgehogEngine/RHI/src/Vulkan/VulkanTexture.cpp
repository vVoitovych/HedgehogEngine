#include "VulkanTexture.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTypes.hpp"

#include <cassert>

namespace RHI
{

// ── VMA-owned texture ─────────────────────────────────────────────────────────

VulkanTexture::VulkanTexture(VulkanDevice& device, const TextureDesc& desc)
    : m_Device(&device)
    , m_Desc(desc)
    , m_OwnedBySwapchain(false)
{
    const VkFormat vkFormat = VulkanTypes::ToVkFormat(desc.Format);

    VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imgInfo.imageType     = VulkanTypes::ToVkImageType(desc.Type);
    imgInfo.extent        = { desc.Width, desc.Height, 1 };
    imgInfo.mipLevels     = desc.MipLevels;
    imgInfo.arrayLayers   = desc.ArrayLayers;
    imgInfo.format        = vkFormat;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage         = VulkanTypes::ToVkImageUsage(desc.Usage);
    imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    if (desc.Type == TextureType::TextureCube)
        imgInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(
        m_Device->GetAllocator(), &imgInfo, &allocInfo,
        &m_Image, &m_Allocation, nullptr);
    assert(result == VK_SUCCESS && "Failed to create VulkanTexture image.");

    // Create a matching image view covering every mip/layer — same default a view-less
    // consumer (sampler, render-pass attachment) has always gotten.
    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image                           = m_Image;
    viewInfo.viewType                        = VulkanTypes::ToVkImageViewType(desc.Type);
    viewInfo.format                          = vkFormat;
    viewInfo.subresourceRange.aspectMask     = VulkanTypes::GetAspectMask(desc.Format);
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = desc.MipLevels;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = desc.ArrayLayers;

    result = vkCreateImageView(m_Device->GetHandle(), &viewInfo, nullptr, &m_View);
    assert(result == VK_SUCCESS && "Failed to create image view.");
}

// ── Swapchain-owned image (externally managed) ────────────────────────────────

namespace
{
    TextureDesc MakeSwapchainTextureDesc(Format format, uint32_t width, uint32_t height)
    {
        TextureDesc desc;
        desc.Width  = width;
        desc.Height = height;
        desc.Format = format;
        desc.Usage  = TextureUsage::ColorAttachment;
        return desc;
    }
}

VulkanTexture::VulkanTexture(VkImage image, VkImageView view, Format format,
                             uint32_t width, uint32_t height)
    : m_Device(nullptr)
    , m_Desc(MakeSwapchainTextureDesc(format, width, height))
    , m_Image(image)
    , m_View(view)
    , m_Allocation(VK_NULL_HANDLE)
    , m_OwnedBySwapchain(true)
{
}

VulkanTexture::~VulkanTexture()
{
    if (m_OwnedBySwapchain || m_Device == nullptr)
        return; // Swapchain cleans up its own images/views.

    if (m_View != VK_NULL_HANDLE)
        vkDestroyImageView(m_Device->GetHandle(), m_View, nullptr);

    if (m_Image != VK_NULL_HANDLE)
        vmaDestroyImage(m_Device->GetAllocator(), m_Image, m_Allocation);
}

} // namespace RHI
