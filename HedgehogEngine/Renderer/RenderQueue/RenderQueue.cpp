#include "RenderQueue.hpp"

#include "Renderer/Context/RenderContext.hpp"
#include "Renderer/Context/VulkanContext.hpp"
#include "Renderer/ResourceManager/ResourceManager.hpp"
#include "Renderer/RenderPasses/InitPass.hpp"
#include "Renderer/RenderPasses/ForwardPass.hpp"
#include "Renderer/RenderPasses/PresentPass.hpp"
#include "Renderer/RenderPasses/GuiPass.hpp"

namespace Renderer
{
	RenderQueue::RenderQueue(const RenderContext& context, const ResourceManager& resourceManager)
	{
		mInitPass = std::make_unique<InitPass>(context);
		mForwardPass = std::make_unique<ForwardPass>(context, resourceManager);
		mGuiPass = std::make_unique<GuiPass>(context, resourceManager);
		mPresentPass = std::make_unique<PresentPass>(context);
	}

	RenderQueue::~RenderQueue()
	{
	}

	void RenderQueue::Cleanup(const RenderContext& context)
	{
		mInitPass->Cleanup(context);
		mForwardPass->Cleanup(context);
		mGuiPass->Cleanup(context);
		mPresentPass->Cleanup(context);
	}

	void RenderQueue::Render(RenderContext& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();

		mInitPass->Render(context);
		if (vulkanContext.IsWindowResized())
			return;
		mForwardPass->Render(context, resourceManager);
		mGuiPass->Render(context, resourceManager);
		mPresentPass->Render(context, resourceManager);

	}

	void RenderQueue::ResizeResources(const RenderContext& context, const ResourceManager& resourceManager)
	{
		mForwardPass->ResizeResources(context, resourceManager);
		mGuiPass->ResizeResources(context, resourceManager);
	}



}




