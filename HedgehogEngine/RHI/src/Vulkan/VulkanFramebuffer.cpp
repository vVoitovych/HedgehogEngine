#include "VulkanFramebuffer.hpp"

#include "VulkanDevice.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanTexture.hpp"

#include <cassert>
#include <vector>

namespace RHI
{

VulkanFramebuffer::VulkanFramebuffer(VulkanDevice& device, const FramebufferDesc& desc)
    : m_Device(device)
    , m_Width(desc.Width)
    , m_Height(desc.Height)
{
    assert(desc.RenderPass && "FramebufferDesc::RenderPass must not be null.");

    const auto& vkPass = static_cast<const VulkanRenderPass&>(*desc.RenderPass);

    std::vector<VkImageView> views;
    views.reserve(desc.ColorAttachments.size() + (desc.DepthAttachment ? 1 : 0));

    for (const auto* att : desc.ColorAttachments)
    {
        assert(att && "Null color attachment in FramebufferDesc.");
        views.push_back(static_cast<const VulkanTexture&>(*att).GetViewHandle());
    }

    if (desc.DepthAttachment)
        views.push_back(static_cast<const VulkanTexture&>(*desc.DepthAttachment).GetViewHandle());

    VkFramebufferCreateInfo fbInfo{ VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    fbInfo.renderPass      = vkPass.GetHandle();
    fbInfo.attachmentCount = static_cast<uint32_t>(views.size());
    fbInfo.pAttachments    = views.data();
    fbInfo.width           = desc.Width;
    fbInfo.height          = desc.Height;
    fbInfo.layers          = 1;

    VkResult result = vkCreateFramebuffer(m_Device.GetHandle(), &fbInfo, nullptr, &m_Framebuffer);
    assert(result == VK_SUCCESS && "Failed to create VkFramebuffer.");
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    if (m_Framebuffer != VK_NULL_HANDLE)
        vkDestroyFramebuffer(m_Device.GetHandle(), m_Framebuffer, nullptr);
}

} // namespace RHI
