#include "VulkanTexture.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTypes.hpp"

#include <cassert>

namespace RHI
{

// ── VMA-owned texture ─────────────────────────────────────────────────────────

VulkanTexture::VulkanTexture(VulkanDevice& device, const TextureDesc& desc)
    : m_Device(&device)
    , m_Width(desc.m_Width)
    , m_Height(desc.m_Height)
    , m_Format(desc.m_Format)
    , m_OwnedBySwapchain(false)
{
    const VkFormat vkFormat = VulkanTypes::ToVkFormat(desc.m_Format);

    VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imgInfo.imageType     = VK_IMAGE_TYPE_2D;
    imgInfo.extent        = { desc.m_Width, desc.m_Height, 1 };
    imgInfo.mipLevels     = 1;
    imgInfo.arrayLayers   = 1;
    imgInfo.format        = vkFormat;
    imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.usage         = VulkanTypes::ToVkImageUsage(desc.m_Usage);
    imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkResult result = vmaCreateImage(
        m_Device->GetAllocator(), &imgInfo, &allocInfo,
        &m_Image, &m_Allocation, nullptr);
    assert(result == VK_SUCCESS && "Failed to create VulkanTexture image.");

    // Create a matching image view.
    VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    viewInfo.image                           = m_Image;
    viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format                          = vkFormat;
    viewInfo.subresourceRange.aspectMask     = VulkanTypes::GetAspectMask(desc.m_Format);
    viewInfo.subresourceRange.baseMipLevel   = 0;
    viewInfo.subresourceRange.levelCount     = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount     = 1;

    result = vkCreateImageView(m_Device->GetHandle(), &viewInfo, nullptr, &m_View);
    assert(result == VK_SUCCESS && "Failed to create image view.");
}

// ── Swapchain-owned image (externally managed) ────────────────────────────────

VulkanTexture::VulkanTexture(VkImage image, VkImageView view, Format format,
                             uint32_t width, uint32_t height)
    : m_Device(nullptr)
    , m_Width(width)
    , m_Height(height)
    , m_Format(format)
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
