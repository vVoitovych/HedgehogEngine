#include "FrameBuffer.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"
#include "VulkanEngine/Logger/Logger.hpp"

#include <array>

namespace Renderer
{
	FrameBuffer::FrameBuffer(
		const std::unique_ptr<Device>& device, 
		std::vector<VkImageView> attachments, 
		VkExtent2D extent, 
		const std::unique_ptr<RenderPass>& renderPass)
		: mFrameBuffer(nullptr)
	{
		VkFramebufferCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		createInfo.renderPass = renderPass->GetNativeRenderPass();
		createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		createInfo.pAttachments = attachments.data();
		createInfo.width = extent.width;
		createInfo.height = extent.height;
		createInfo.layers = 1;

		if (vkCreateFramebuffer(device->GetNativeDevice(), &createInfo, nullptr, &mFrameBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create frame buffer!");
		}

		LOGINFO("Frame buffer created");
	}

	FrameBuffer::~FrameBuffer()
	{
		if (mFrameBuffer != nullptr)
		{
			LOGERROR("Vulkan frame buffer should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	FrameBuffer::FrameBuffer(FrameBuffer&& other)
		: mFrameBuffer(other.mFrameBuffer)
	{
		other.mFrameBuffer = nullptr;
	}

	FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other)
	{
		if (this != &other)
		{
			mFrameBuffer = other.mFrameBuffer;

			other.mFrameBuffer = nullptr;
		}
		return *this;
	}

	void FrameBuffer::Cleanup(const std::unique_ptr<Device>& device)
	{
		vkDestroyFramebuffer(device->GetNativeDevice(), mFrameBuffer, nullptr);
		mFrameBuffer = nullptr;
		LOGINFO("Frame buffer cleaned");
	}

	VkFramebuffer FrameBuffer::GetNativeFrameBuffer() const
	{
		return mFrameBuffer;
	}

}





