#include "VulkanRenderPass.hpp"

#include "VulkanDevice.hpp"
#include "VulkanTypes.hpp"

#include <cassert>
#include <vector>

namespace RHI
{

VulkanRenderPass::VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc)
    : m_Device(device)
{
    std::vector<VkAttachmentDescription> attachments;
    std::vector<VkAttachmentReference>   colorRefs;

    for (uint32_t i = 0; i < static_cast<uint32_t>(desc.m_ColorAttachments.size()); ++i)
    {
        const auto& att = desc.m_ColorAttachments[i];

        VkAttachmentDescription vkAtt{};
        vkAtt.format         = VulkanTypes::ToVkFormat(att.m_Format);
        vkAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        vkAtt.loadOp         = VulkanTypes::ToVkLoadOp(att.m_LoadOp);
        vkAtt.storeOp        = VulkanTypes::ToVkStoreOp(att.m_StoreOp);
        vkAtt.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        vkAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        vkAtt.initialLayout  = VulkanTypes::ToVkLayout(att.m_InitialLayout);
        vkAtt.finalLayout    = VulkanTypes::ToVkLayout(att.m_FinalLayout);
        attachments.push_back(vkAtt);

        colorRefs.push_back({ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
    }

    VkAttachmentReference depthRef{};
    const bool            hasDepth = desc.m_DepthAttachment.has_value();

    if (hasDepth)
    {
        const auto& att = *desc.m_DepthAttachment;

        VkAttachmentDescription vkAtt{};
        vkAtt.format         = VulkanTypes::ToVkFormat(att.m_Format);
        vkAtt.samples        = VK_SAMPLE_COUNT_1_BIT;
        vkAtt.loadOp         = VulkanTypes::ToVkLoadOp(att.m_LoadOp);
        vkAtt.storeOp        = VulkanTypes::ToVkStoreOp(att.m_StoreOp);
        vkAtt.stencilLoadOp  = VulkanTypes::ToVkLoadOp(att.m_StencilLoadOp);
        vkAtt.stencilStoreOp = VulkanTypes::ToVkStoreOp(att.m_StencilStoreOp);
        vkAtt.initialLayout  = VulkanTypes::ToVkLayout(att.m_InitialLayout);
        vkAtt.finalLayout    = VulkanTypes::ToVkLayout(att.m_FinalLayout);
        attachments.push_back(vkAtt);

        depthRef = {
            static_cast<uint32_t>(attachments.size() - 1),
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        };
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount    = static_cast<uint32_t>(colorRefs.size());
    subpass.pColorAttachments       = colorRefs.data();
    subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;

    // Standard dependency: wait for previous render to finish before
    // writing to color / depth attachments.
    VkSubpassDependency dependency{};
    dependency.srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass      = 0;
    dependency.srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                               | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                               | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask   = 0;
    dependency.dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                               | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{ VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    rpInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    rpInfo.pAttachments    = attachments.data();
    rpInfo.subpassCount    = 1;
    rpInfo.pSubpasses      = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies   = &dependency;

    VkResult result = vkCreateRenderPass(m_Device.GetHandle(), &rpInfo, nullptr, &m_RenderPass);
    assert(result == VK_SUCCESS && "Failed to create VkRenderPass.");
}

VulkanRenderPass::~VulkanRenderPass()
{
    if (m_RenderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(m_Device.GetHandle(), m_RenderPass, nullptr);
}

} // namespace RHI
