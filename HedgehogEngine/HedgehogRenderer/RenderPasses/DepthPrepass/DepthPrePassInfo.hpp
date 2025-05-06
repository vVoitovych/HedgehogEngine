#pragma once

#include <vulkan/vulkan.h>
#include <array>

namespace Renderer
{
	class DepthPrePassInfo
	{
	public:
		DepthPrePassInfo(VkFormat depthFormat);
		VkRenderPassCreateInfo* GetInfo();
	private:
		VkAttachmentDescription m_DepthAttachment{};
		VkAttachmentReference m_DepthAttachmentRef{};
		VkSubpassDescription m_Subpass{};
		VkSubpassDependency m_Dependency{};
		std::array<VkAttachmentDescription, 1> m_Attachments;
		VkRenderPassCreateInfo m_RenderPassInfo{};
	};


}


