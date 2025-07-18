#include "ShadowmapPassInfo.hpp"

namespace Renderer
{
	ShadowmapPassInfo::ShadowmapPassInfo(VkFormat depthFormat)
	{
		m_DepthAttachment.format = depthFormat;
		m_DepthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		m_DepthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		m_DepthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		m_DepthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		m_DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		m_DepthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		m_DepthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

		m_DepthAttachmentRef.attachment = 0;
		m_DepthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		m_Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		m_Subpass.colorAttachmentCount = 0;
		m_Subpass.pColorAttachments = nullptr;
		m_Subpass.pDepthStencilAttachment = &m_DepthAttachmentRef;

		m_Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		m_Dependency.dstSubpass = 0;
		m_Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		m_Dependency.srcAccessMask = 0;
		m_Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		m_Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		m_Attachments = { m_DepthAttachment };
		
		m_RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		m_RenderPassInfo.attachmentCount = static_cast<uint32_t>(m_Attachments.size());
		m_RenderPassInfo.pAttachments = m_Attachments.data();
		m_RenderPassInfo.subpassCount = 1;
		m_RenderPassInfo.pSubpasses = &m_Subpass;
		m_RenderPassInfo.dependencyCount = 1;
		m_RenderPassInfo.pDependencies = &m_Dependency;
	}

	VkRenderPassCreateInfo* ShadowmapPassInfo::GetInfo()
	{
		return &m_RenderPassInfo;
	}

}



