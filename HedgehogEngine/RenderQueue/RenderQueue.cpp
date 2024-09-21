#include "RenderQueue.hpp"

#include "Context/Context.hpp"
#include "Context/VulkanContext.hpp"
#include "ResourceManager/ResourceManager.hpp"
#include "RenderPasses/InitPass/InitPass.hpp"
#include "RenderPasses/ForwardPass/ForwardPass.hpp"
#include "RenderPasses/PresentPass/PresentPass.hpp"
#include "RenderPasses/GuiPass/GuiPass.hpp"

namespace Renderer
{
	RenderQueue::RenderQueue(const Context::Context& context, const ResourceManager& resourceManager)
	{
		mInitPass = std::make_unique<InitPass>(context);
		mForwardPass = std::make_unique<ForwardPass>(context, resourceManager);
		mGuiPass = std::make_unique<GuiPass>(context, resourceManager);
		mPresentPass = std::make_unique<PresentPass>(context);
	}

	RenderQueue::~RenderQueue()
	{
	}

	void RenderQueue::Cleanup(const Context::Context& context)
	{
		mInitPass->Cleanup(context);
		mForwardPass->Cleanup(context);
		mGuiPass->Cleanup(context);
		mPresentPass->Cleanup(context);
	}

	void RenderQueue::Render(Context::Context& context, const ResourceManager& resourceManager)
	{
		auto& vulkanContext = context.GetVulkanContext();

		mInitPass->Render(context);
		if (vulkanContext.IsWindowResized())
			return;
		mForwardPass->Render(context, resourceManager);
		mGuiPass->Render(context, resourceManager);
		mPresentPass->Render(context, resourceManager);

	}

	void RenderQueue::ResizeResources(const Context::Context& context, const ResourceManager& resourceManager)
	{
		mForwardPass->ResizeResources(context, resourceManager);
		mGuiPass->ResizeResources(context, resourceManager);
	}



}




