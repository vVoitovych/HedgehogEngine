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
		RenderQueue(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);
		~RenderQueue();

		RenderQueue(const RenderQueue&) = delete;
		RenderQueue(RenderQueue&&) = delete;
		RenderQueue& operator=(const RenderQueue&) = delete;
		RenderQueue& operator=(RenderQueue&&) = delete;


		void Cleanup(const std::unique_ptr<RenderContext>& context);
		void Render(std::unique_ptr<RenderContext>& context);

		void ResizeResources(const std::unique_ptr<RenderContext>& context, const std::unique_ptr<ResourceManager>& resourceManager);

	private:
		std::unique_ptr<InitPass> mInitPass;
		std::unique_ptr<ForwardPass> mForwardPass;
		std::unique_ptr<PresentPass> mPresentPass;
		std::unique_ptr<GuiPass> mGuiPass;
	};


}



