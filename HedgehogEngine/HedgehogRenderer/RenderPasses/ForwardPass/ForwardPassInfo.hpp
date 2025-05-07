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
		VkAttachmentDescription m_ColorAttachment{};
		VkAttachmentDescription m_DepthAttachment{};
		VkAttachmentReference m_ColorAttachmentRef{};
		VkAttachmentReference m_DepthAttachmentRef{};
		VkSubpassDescription m_Subpass{};
		VkSubpassDependency m_Dependency{};
		std::array<VkAttachmentDescription, 2> m_Attachments;
		VkRenderPassCreateInfo m_RenderPassInfo{};
	};


}


