#include "VulkanSwapchain.hpp"

#include "VulkanDevice.hpp"
#include "VulkanSyncPrimitive.hpp"
#include "VulkanTypes.hpp"

#include <algorithm>
#include <cassert>

namespace RHI
{

VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, uint32_t width, uint32_t height)
    : m_Device(device)
{
    Create(width, height);
}

VulkanSwapchain::~VulkanSwapchain()
{
    Destroy();
}

void VulkanSwapchain::Create(uint32_t width, uint32_t height)
{
    VkPhysicalDevice gpu     = m_Device.GetPhysicalDevice();
    VkSurfaceKHR     surface = m_Device.GetSurface();

    // ── Surface capabilities ──────────────────────────────────────────────────

    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps);

    // ── Choose surface format (prefer B8G8R8A8_SRGB) ──────────────────────────

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
    assert(formatCount > 0);
    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, surfaceFormats.data());

    VkSurfaceFormatKHR chosenFormat = surfaceFormats[0];
    for (const auto& f : surfaceFormats)
    {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB
            && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosenFormat = f;
            break;
        }
    }

    // ── Choose present mode (prefer Mailbox) ──────────────────────────────────

    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &modeCount, presentModes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& m : presentModes)
    {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }
    }

    // ── Choose extent ─────────────────────────────────────────────────────────

    VkExtent2D extent{};
    if (caps.currentExtent.width != UINT32_MAX)
    {
        extent = caps.currentExtent;
    }
    else
    {
        extent.width  = std::clamp(width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
        extent.height = std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    m_Width  = extent.width;
    m_Height = extent.height;
    m_Format = VulkanTypes::FromVkFormat(chosenFormat.format);

    // ── Image count ───────────────────────────────────────────────────────────

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);

    // ── Create swapchain ──────────────────────────────────────────────────────

    VkSwapchainCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
    createInfo.surface          = surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = chosenFormat.format;
    createInfo.imageColorSpace  = chosenFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform     = caps.currentTransform;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(m_Device.GetHandle(), &createInfo, nullptr, &m_Swapchain);
    assert(result == VK_SUCCESS && "Failed to create VkSwapchainKHR.");

    // ── Retrieve images and create views ──────────────────────────────────────

    vkGetSwapchainImagesKHR(m_Device.GetHandle(), m_Swapchain, &imageCount, nullptr);
    std::vector<VkImage> images(imageCount);
    vkGetSwapchainImagesKHR(m_Device.GetHandle(), m_Swapchain, &imageCount, images.data());

    m_ImageViews.reserve(imageCount);
    m_Textures.reserve(imageCount);

    for (const VkImage img : images)
    {
        VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image                           = img;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = chosenFormat.format;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VkImageView view = VK_NULL_HANDLE;
        result = vkCreateImageView(m_Device.GetHandle(), &viewInfo, nullptr, &view);
        assert(result == VK_SUCCESS && "Failed to create swapchain image view.");

        m_ImageViews.push_back(view);
        m_Textures.push_back(std::make_unique<VulkanTexture>(img, view, m_Format, m_Width, m_Height));
    }
}

void VulkanSwapchain::Destroy()
{
    for (VkImageView view : m_ImageViews)
        vkDestroyImageView(m_Device.GetHandle(), view, nullptr);
    m_ImageViews.clear();
    m_Textures.clear();

    if (m_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_Device.GetHandle(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

// ── IRHISwapchain ─────────────────────────────────────────────────────────────

IRHITexture& VulkanSwapchain::GetTexture(uint32_t index)
{
    assert(index < m_Textures.size());
    return *m_Textures[index];
}

uint32_t VulkanSwapchain::AcquireNextImage(IRHISemaphore& signalSemaphore)
{
    const auto& vkSem = static_cast<VulkanSemaphore&>(signalSemaphore);

    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(
        m_Device.GetHandle(), m_Swapchain,
        UINT64_MAX, vkSem.GetHandle(), VK_NULL_HANDLE, &imageIndex);

    assert((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
           && "vkAcquireNextImageKHR failed.");

    return imageIndex;
}

void VulkanSwapchain::Present(uint32_t imageIndex, IRHISemaphore& waitSemaphore)
{
    const auto& vkSem = static_cast<VulkanSemaphore&>(waitSemaphore);
    VkSemaphore waitSems[] = { vkSem.GetHandle() };

    VkPresentInfoKHR presentInfo{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = waitSems;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_Swapchain;
    presentInfo.pImageIndices      = &imageIndex;

    vkQueuePresentKHR(m_Device.GetPresentQueue(), &presentInfo);
}

void VulkanSwapchain::Resize(uint32_t width, uint32_t height)
{
    m_Device.WaitIdle();
    Destroy();
    Create(width, height);
}

} // namespace RHI
