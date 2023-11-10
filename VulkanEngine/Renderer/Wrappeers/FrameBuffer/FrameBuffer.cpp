#include "FrameBuffer.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"
#include "VulkanEngine/Logger/Logger.hpp"

#include <array>

namespace Renderer
{
	FrameBuffer::FrameBuffer()
		: mFrameBuffer(nullptr)
	{
	}

	FrameBuffer::~FrameBuffer()
	{
		if (mFrameBuffer != nullptr)
		{
			LOGERROR("Vulkan frame buffer should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void FrameBuffer::Initialize(const Device& device, std::vector<VkImageView> attachments, VkExtent2D extent, RenderPass& renderPass)
	{
		VkFramebufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass.GetNativeRenderPass();
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.width = extent.width;
		createInfo.height = extent.height;
		createInfo.layers = 1;

		if (vkCreateFramebuffer(device.GetNativeDevice(), &createInfo, nullptr, &mFrameBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create frame buffer!");
		}

		LOGINFO("Frame buffer created");
	}

	void FrameBuffer::Cleanup(const Device& device)
	{
		vkDestroyFramebuffer(device.GetNativeDevice(), mFrameBuffer, nullptr);
		mFrameBuffer = nullptr;
		LOGINFO("Frame buffer cleaned");
	}

	VkFramebuffer FrameBuffer::GetNativeFrameBuffer() const
	{
		return mFrameBuffer;
	}

}





