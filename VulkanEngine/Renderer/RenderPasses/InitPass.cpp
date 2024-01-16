#include "InitPass.hpp"
#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/ThreadContext.hpp"
#include "Renderer/Context/EngineContext.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Wrappeers/Commands/CommandBuffer.hpp"

#include <stdexcept>

namespace Renderer
{
	InitPass::InitPass(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain)
	{
		mDevice = device->GetNativeDevice();
		mSwapChain = swapChain->GetNativeSwapChain();
	}

	void InitPass::Render(std::unique_ptr<RenderContext>& renderContext)
	{
		if (!mDevice.has_value() || !mSwapChain.has_value())
		{
			throw std::runtime_error("Init pass doesn't initialized!");
		}

		auto [engineContext, frameContext, threadContext] = renderContext->GetContexts();
		auto& syncObject = threadContext->GetSyncObject();
		syncObject.WaitforInFlightFence();

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(
			mDevice.value(), 
			mSwapChain.value(),
			UINT64_MAX, 
			syncObject.GetImageAvailableSemaphore(), 
			VK_NULL_HANDLE, 
			&imageIndex);
		engineContext->UpdateBackBufferIdex(imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			engineContext->ResizeWindow();
			mSwapChain.reset();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		syncObject.ResetInFlightFence();
		auto& commandBuffer = threadContext->GetCommandBuffer();
		commandBuffer.BeginCommandBuffer(0);
	}

	void InitPass::Cleanup(const std::unique_ptr<Device>& device)
	{
		mDevice.reset();
		mSwapChain.reset();
	}

	void InitPass::SetSwapChain(const std::unique_ptr<SwapChain>& swapChain)
	{
		mSwapChain = swapChain->GetNativeSwapChain();
	}


}

