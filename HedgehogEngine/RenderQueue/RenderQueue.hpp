#pragma once 

#include <memory>

namespace Context
{
	class Context;
}

namespace Renderer
{
	class ResourceManager;

	class InitPass;
	class ForwardPass;
	class PresentPass;
	class GuiPass;

	class RenderQueue
	{
	public:
		RenderQueue(const Context::Context& context, const ResourceManager& resourceManager);
		~RenderQueue();

		RenderQueue(const RenderQueue&) = delete;
		RenderQueue(RenderQueue&&) = delete;
		RenderQueue& operator=(const RenderQueue&) = delete;
		RenderQueue& operator=(RenderQueue&&) = delete;


		void Cleanup(const Context::Context& context);
		void Render(Context::Context& context, const ResourceManager& resourceManager);

		void ResizeResources(const Context::Context& context, const ResourceManager& resourceManager);

	private:
		std::unique_ptr<InitPass> mInitPass;
		std::unique_ptr<ForwardPass> mForwardPass;
		std::unique_ptr<PresentPass> mPresentPass;
		std::unique_ptr<GuiPass> mGuiPass;
	};


}



