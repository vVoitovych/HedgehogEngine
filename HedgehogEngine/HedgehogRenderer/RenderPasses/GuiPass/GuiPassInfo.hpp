#pragma once

#include <vulkan/vulkan.h>
#include <array>

namespace Renderer
{
	class GuiPassInfo
	{
	public:
		GuiPassInfo(VkFormat colorFormat);
		VkRenderPassCreateInfo* GetInfo();

	private:
		VkAttachmentDescription m_ColorAttachment{};
		VkAttachmentReference m_ColorAttachmentRef{};
		VkSubpassDescription m_Subpass{};
		VkSubpassDependency m_Dependency{};
		std::array<VkAttachmentDescription, 1> m_Attachments;
		VkRenderPassCreateInfo m_RenderPassInfo{};

	};
}


