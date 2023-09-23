#include "FrameBuffers.h"
#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Renderer/SwapChain/SwapChain.h"
#include "VulkanEngine/Renderer/RenderPass/RenderPass.h"

#include "VulkanEngine/Logger/Logger.h"

namespace Renderer
{
	FrameBuffers::FrameBuffers()
		: mDevice(nullptr)
	{
	}

	FrameBuffers::~FrameBuffers()
	{
		if (!mFrameBuffers.empty())
		{
			LOGERROR("Vulkan frame buffers should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}

	void FrameBuffers::Initialize(Device& device, SwapChain& swapChain, RenderPass& renderPass)
	{
		mDevice = device.GetNativeDevice();

		size_t swapChainImagesSize = swapChain.GetSwapChainImagesSize();
		mFrameBuffers.resize(swapChainImagesSize);

		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			VkImageView attachments[] = { swapChain.GetNativeSwapChainImageView(i) };

			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass.GetNativeRenderPass();
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = attachments;
			auto swapChainExtend = swapChain.GetSwapChainExtend();
			createInfo.width = swapChainExtend.width;
			createInfo.height = swapChainExtend.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(mDevice, &createInfo, nullptr, &mFrameBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create frame buffer!");
			}
		}
		LOGINFO("Frame buffer created");
	}

	void FrameBuffers::Cleanup()
	{
		for (auto& frameBuffer : mFrameBuffers)
		{
			vkDestroyFramebuffer(mDevice, frameBuffer, nullptr);
		}
		mFrameBuffers.clear();
		LOGINFO("Frame buffers cleaned");
	}

	VkFramebuffer FrameBuffers::GetNativeFrameBuffer(size_t index) const
	{
		return mFrameBuffers[index];
	}

}

