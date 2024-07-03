#include "Renderer.hpp"

#include "Context/RenderContext.hpp"
#include "Context/VulkanContext.hpp"
#include "Context/EngineContext.hpp"
#include "Context/FrameContext.hpp"
#include "Context/ThreadContext.hpp"
#include "RenderQueue/RenderQueue.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	Renderer::Renderer()
	{
		mRenderContext = std::make_unique<RenderContext>();
		mResourceManager = std::make_unique<ResourceManager>(*mRenderContext);
		mRenderQueue = std::make_unique<RenderQueue>(*mRenderContext, *mResourceManager);

	}

	Renderer::~Renderer()
	{
	}
	 
	void Renderer::Cleanup()
	{
		vkQueueWaitIdle(mRenderContext->GetVulkanContext().GetDevice().GetNativeGraphicsQueue());

		mRenderQueue->Cleanup(*mRenderContext);
		mResourceManager->Cleanup(*mRenderContext);
		mRenderContext->Cleanup();
	}

	void Renderer::HandleInput()
	{
		auto& vulkanContext = mRenderContext->GetVulkanContext();
		vulkanContext.HandleInput();
	}

	void Renderer::Update(float dt)
	{
		mRenderContext->UpdateContext(dt);

	}

	void Renderer::DrawFrame()
	{		
		auto& vulkanContext = mRenderContext->GetVulkanContext();
		if (vulkanContext.IsWindowResized())
		{
			RecreateSwapChain();
			vulkanContext.ResetWindowResizeState();
		}

		mRenderQueue->Render(*mRenderContext, *mResourceManager);
	}

	void Renderer::RecreateSwapChain()
	{
		vkDeviceWaitIdle(mRenderContext->GetVulkanContext().GetDevice().GetNativeDevice());
		auto& vulkanContext = mRenderContext->GetVulkanContext();
		vulkanContext.GetSwapChain().Recreate(vulkanContext.GetDevice());

		mResourceManager->ResizeResources(*mRenderContext);
		mRenderQueue->ResizeResources(*mRenderContext, *mResourceManager);
	}

	bool Renderer::ShouldClose()
	{
		auto& vulkanContext = mRenderContext->GetVulkanContext();
		return vulkanContext.ShouldClose();
	}

}



