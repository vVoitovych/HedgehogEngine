#include "FrameBuffers.h"

namespace Renderer
{
	FrameBuffers::FrameBuffers()
	{
	}

	FrameBuffers::~FrameBuffers()
	{
		if (!mFrameBuffers.empty())
		{
			throw std::runtime_error("Vulkan frame buffers should be cleanedup before destruction!");
		}
	}

	void FrameBuffers::Initialize(Device& device, SwapChain& swapChain, RenderPass& renderPass)
	{
		size_t swapChainImagesSize = swapChain.GetSwapChainImagesSize();
		mFrameBuffers.resize(swapChainImagesSize);

		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			VkImageView attachments[] = { swapChain.GetSwapChainImageView(i) };

			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass.GetRenderPass();
			createInfo.attachmentCount = 1;
			createInfo.pAttachments = attachments;
			auto swapChainExtend = swapChain.GetSwapChainExtend();
			createInfo.width = swapChainExtend.width;
			createInfo.height = swapChainExtend.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(device.GetDevice(), &createInfo, nullptr, &mFrameBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create frame buffer!");
			}
		}
		std::cout << "Frame buffers created" << std::endl;
	}

	void FrameBuffers::Cleanup(Device& device)
	{
		for (auto frameBuffer : mFrameBuffers)
		{
			vkDestroyFramebuffer(device.GetDevice(), frameBuffer, nullptr);
		}
		mFrameBuffers.clear();
	}

	VkFramebuffer FrameBuffers::GetFrameBuffer(size_t index) const
	{
		return mFrameBuffers[index];
	}

}

