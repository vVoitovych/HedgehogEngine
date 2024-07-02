#pragma once 

#include <memory>

namespace Renderer
{
	class RenderContext;
	class ResourceManager;

	class InitPass;
	class ForwardPass;
	class PresentPass;
	class GuiPass;

	class RenderQueue
	{
	public:
		RenderQueue(const RenderContext& context, const ResourceManager& resourceManager);
		~RenderQueue();

		RenderQueue(const RenderQueue&) = delete;
		RenderQueue(RenderQueue&&) = delete;
		RenderQueue& operator=(const RenderQueue&) = delete;
		RenderQueue& operator=(RenderQueue&&) = delete;


		void Cleanup(const RenderContext& context);
		void Render(RenderContext& context, const ResourceManager& resourceManager);

		void ResizeResources(const RenderContext& context, const ResourceManager& resourceManager);

	private:
		std::unique_ptr<InitPass> mInitPass;
		std::unique_ptr<ForwardPass> mForwardPass;
		std::unique_ptr<PresentPass> mPresentPass;
		std::unique_ptr<GuiPass> mGuiPass;
	};


}



