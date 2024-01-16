#include "RenderQueue.hpp"

#include "Renderer/Wrappeers/Device/Device.hpp"
#include "Renderer/Wrappeers/SwapChain/SwapChain.hpp"
#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/EngineContext.hpp"

#include "Renderer/RenderPasses/InitPass.hpp"
#include "Renderer/RenderPasses/ForwardPass.hpp"
#include "Renderer/RenderPasses/PresentPass.hpp"

namespace Renderer
{
	RenderQueue::RenderQueue(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain)
	{
		mInitPass = std::make_unique<InitPass>(device, swapChain);
		mForwardPass = std::make_unique<ForwardPass>(device, swapChain);
		mPresentPass = std::make_unique<PresentPass>(device, swapChain);
	}

	RenderQueue::~RenderQueue()
	{
	}

	void RenderQueue::Cleanup(const std::unique_ptr<Device>& device)
	{
		mInitPass->Cleanup(device);
		mForwardPass->Cleanup(device);
		mPresentPass->Cleanup(device);
	}

	void RenderQueue::CreateSizedResources(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain)
	{
		mInitPass->SetSwapChain(swapChain);
		mForwardPass->CreateSizedResources(device, swapChain);
		mPresentPass->SetSwapChain(swapChain);

	}

	void RenderQueue::Render(std::unique_ptr<RenderContext>& context)
	{
		auto [engineContext, frameContext, threadContext] = context->GetContexts();

		mInitPass->Render(context);
		if (engineContext->IsWindowResized())
			return;
		mForwardPass->Render(context);
		mPresentPass->Render(context);

	}

	void RenderQueue::CleanSizedResources(const std::unique_ptr<Device>& device)
	{
		mForwardPass->CleanSizedResources(device);
	}



}




