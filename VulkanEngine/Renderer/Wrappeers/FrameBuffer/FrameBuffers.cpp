#include "FrameBuffers.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "VulkanEngine/Renderer/Resources/DepthBuffer/DepthBuffer.hpp"
#include "VulkanEngine/Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"
#include "VulkanEngine/Logger/Logger.hpp"

#include <array>

namespace Renderer
{
	FrameBuffers::FrameBuffers()
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

	void FrameBuffers::Initialize(const Device& device, SwapChain& swapChain, const DepthBuffer& depthBuffer, RenderPass& renderPass)
	{
		size_t swapChainImagesSize = swapChain.GetSwapChainImagesSize();
		mFrameBuffers.resize(swapChainImagesSize);

		for (size_t i = 0; i < swapChainImagesSize; ++i)
		{
			std::array<VkImageView, 2> attachments = { swapChain.GetNativeSwapChainImageView(i), depthBuffer.GetNativeView() };

			VkFramebufferCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			createInfo.renderPass = renderPass.GetNativeRenderPass();
			createInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			createInfo.pAttachments = attachments.data();
			auto swapChainExtend = swapChain.GetSwapChainExtend();
			createInfo.width = swapChainExtend.width;
			createInfo.height = swapChainExtend.height;
			createInfo.layers = 1;

			if (vkCreateFramebuffer(device.GetNativeDevice(), &createInfo, nullptr, &mFrameBuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create frame buffer!");
			}
		}
		LOGINFO("Frame buffer created. Created ", swapChainImagesSize, " frame buffers.");
	}

	void FrameBuffers::Cleanup(const Device& device)
	{
		for (auto& frameBuffer : mFrameBuffers)
		{
			vkDestroyFramebuffer(device.GetNativeDevice(), frameBuffer, nullptr);
		}
		mFrameBuffers.clear();
		LOGINFO("Frame buffers cleaned");
	}

	VkFramebuffer FrameBuffers::GetNativeFrameBuffer(size_t index) const
	{
		return mFrameBuffers[index];
	}

}

