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
		VkAttachmentDescription mColorAttachment{};
		VkAttachmentReference mColorAttachmentRef{};
		VkSubpassDescription mSubpass{};
		VkSubpassDependency mDependency{};
		std::array<VkAttachmentDescription, 1> mAttachments;
		VkRenderPassCreateInfo mRenderPassInfo{};

	};
}


