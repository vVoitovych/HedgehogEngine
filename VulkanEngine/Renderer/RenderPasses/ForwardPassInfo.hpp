#pragma once

#include <vulkan/vulkan.h>
#include <array>

namespace Renderer
{
	class ForwardPassInfo
	{
	public:
		ForwardPassInfo(VkFormat colorFormat, VkFormat depthFormat);
		VkRenderPassCreateInfo* GetInfo();
	private:
		VkAttachmentDescription mColorAttachment{};
		VkAttachmentDescription mDepthAttachment{};
		VkAttachmentReference mColorAttachmentRef{};
		VkAttachmentReference mDepthAttachmentRef{};
		VkSubpassDescription mSubpass{};
		VkSubpassDependency mDependency{};
		std::array<VkAttachmentDescription, 2> mAttachments;
		VkRenderPassCreateInfo mRenderPassInfo{};
	};


}


