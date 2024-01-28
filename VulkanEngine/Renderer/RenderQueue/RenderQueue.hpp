#pragma once 

#include <memory>

namespace Renderer
{
	class RenderContext;

	class InitPass;
	class ForwardPass;
	class PresentPass;
	class GuiPass;

	class RenderQueue
	{
	public:
		RenderQueue(const std::unique_ptr<RenderContext>& context);
		~RenderQueue();

		RenderQueue(const RenderQueue&) = delete;
		RenderQueue(RenderQueue&&) = delete;
		RenderQueue& operator=(const RenderQueue&) = delete;
		RenderQueue& operator=(RenderQueue&&) = delete;


		void Cleanup(const std::unique_ptr<RenderContext>& context);
		void Render(std::unique_ptr<RenderContext>& context);

		void RecreateizedResources(const std::unique_ptr<RenderContext>& context);

	private:
		std::unique_ptr<InitPass> mInitPass;
		std::unique_ptr<ForwardPass> mForwardPass;
		std::unique_ptr<PresentPass> mPresentPass;
		std::unique_ptr<GuiPass> mGuiPass;
	};


}



