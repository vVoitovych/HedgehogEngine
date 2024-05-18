#include "InitPass.hpp"
#include "Renderer/Context/VulkanContext.hpp"
#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/ThreadContext.hpp"
#include "Renderer/Context/EngineContext.hpp"
#include "Renderer/Context/FrameContext.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Wrappeers/Commands/CommandBuffer.hpp"
#include "Renderer/Wrappeers/SyncObjects/SyncObject.hpp"

#include <stdexcept>

namespace Renderer
{
	InitPass::InitPass(const std::unique_ptr<RenderContext>& context)
	{
	}

	void InitPass::Render(std::unique_ptr<RenderContext>& context)
	{
		auto& vulkanContext = context->GetVulkanContext();
		auto& frameContext = context->GetFrameContext();
		auto& threadContext = context->GetThreadContext();

		auto& syncObject = threadContext->GetSyncObject();
		syncObject.WaitforInFlightFence(vulkanContext->GetDevice());

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(
			vulkanContext->GetDevice().GetNativeDevice(), 
			vulkanContext->GetSwapChain().GetNativeSwapChain(),
			UINT64_MAX, 
			syncObject.GetImageAvailableSemaphore(), 
			VK_NULL_HANDLE, 
			&imageIndex);
		frameContext->SetBackBufferIndex(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			vulkanContext->ResizeWindow();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		syncObject.ResetInFlightFence(vulkanContext->GetDevice());
		auto& commandBuffer = threadContext->GetCommandBuffer();
		commandBuffer.BeginCommandBuffer(0);
	}

	void InitPass::Cleanup(const std::unique_ptr<RenderContext>& context)
	{

	}



}

