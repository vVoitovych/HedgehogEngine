#include "RenderQueue.hpp"

#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/VulkanContext.hpp"

#include "Renderer/RenderPasses/InitPass.hpp"
#include "Renderer/RenderPasses/ForwardPass.hpp"
#include "Renderer/RenderPasses/PresentPass.hpp"
#include "Renderer/RenderPasses/GuiPass.h"

namespace Renderer
{
	RenderQueue::RenderQueue(const std::unique_ptr<RenderContext>& context)
	{
		mInitPass = std::make_unique<InitPass>(context);
		mForwardPass = std::make_unique<ForwardPass>(context);
		mGuiPass = std::make_unique<GuiPass>(context);
		mPresentPass = std::make_unique<PresentPass>(context);
	}

	RenderQueue::~RenderQueue()
	{
	}

	void RenderQueue::Cleanup(const std::unique_ptr<RenderContext>& context)
	{
		mInitPass->Cleanup(context);
		mForwardPass->Cleanup(context);
		mGuiPass->Cleanup(context);
		mPresentPass->Cleanup(context);
	}

	void RenderQueue::Render(std::unique_ptr<RenderContext>& context)
	{
		auto& vulkanContext = context->GetVulkanContext();

		mInitPass->Render(context);
		if (vulkanContext->IsWindowResized())
			return;
		mForwardPass->Render(context);
		mGuiPass->Render(context);
		mPresentPass->Render(context);

	}

	void RenderQueue::ResizeResources(const std::unique_ptr<RenderContext>& context)
	{
		mForwardPass->ResizeResources(context);
		mGuiPass->ResizeResources(context);
	}



}




