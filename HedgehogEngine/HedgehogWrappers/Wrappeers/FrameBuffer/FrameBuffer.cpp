#include "FrameBuffer.hpp"
#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/RenderPass/RenderPass.hpp"
#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

#include <array>

namespace Wrappers
{
	FrameBuffer::FrameBuffer(
		const Device& device, 
		std::vector<VkImageView> attachments, 
		VkExtent2D extent, 
		const RenderPass& renderPass)
		: mFrameBuffer(nullptr)
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

	FrameBuffer::~FrameBuffer()
	{
		if (mFrameBuffer != nullptr)
		{
			LOGERROR("Vulkan frame buffer should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept
		: mFrameBuffer(other.mFrameBuffer)
	{
		other.mFrameBuffer = nullptr;
	}

	FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept
	{
		if (this != &other)
		{
			mFrameBuffer = other.mFrameBuffer;

			other.mFrameBuffer = nullptr;
		}
		return *this;
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





