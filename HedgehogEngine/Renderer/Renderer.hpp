#pragma once

#include <memory>

namespace Renderer
{
	class RenderContext;
	class ResourceManager;
	class RenderQueue;

	class Renderer
	{
	public:
		Renderer(const RenderContext& context);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		void Cleanup(const RenderContext& context);

		void DrawFrame(RenderContext& context);

		void RecreateSwapChain(RenderContext& context);

	private:
		std::unique_ptr< ResourceManager> mResourceManager;
		std::unique_ptr<RenderQueue> mRenderQueue;

	};
}




