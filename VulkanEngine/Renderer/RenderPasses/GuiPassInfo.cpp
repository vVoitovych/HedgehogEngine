#include "GuiPassInfo.hpp"

namespace Renderer
{
	GuiPassInfo::GuiPassInfo(VkFormat colorFormat)
	{
		mColorAttachment.format = colorFormat;
		mColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		mColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		mColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		mColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		mColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		mColorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		mColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		mColorAttachmentRef.attachment = 0;
		mColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		mSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		mSubpass.colorAttachmentCount = 1;
		mSubpass.pColorAttachments = &mColorAttachmentRef;
		mSubpass.pDepthStencilAttachment = nullptr;

		mDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		mDependency.dstSubpass = 0;
		mDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		mDependency.srcAccessMask = 0;
		mDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		mDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		mAttachments = { mColorAttachment };

		mRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		mRenderPassInfo.attachmentCount = static_cast<uint32_t>(mAttachments.size());
		mRenderPassInfo.pAttachments = mAttachments.data();
		mRenderPassInfo.subpassCount = 1;
		mRenderPassInfo.pSubpasses = &mSubpass;
		mRenderPassInfo.dependencyCount = 1;
		mRenderPassInfo.pDependencies = &mDependency;
	}

	VkRenderPassCreateInfo* GuiPassInfo::GetInfo()
	{
		return &mRenderPassInfo;
	}
}





