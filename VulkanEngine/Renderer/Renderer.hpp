#pragma once

#include <memory>

namespace Renderer
{
	class RenderContext;
	class RenderQueue;

	class Renderer
	{
	public:
		Renderer();
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		void Cleanup();
		bool ShouldClose();
		void HandleInput();
		void Update(float dt);

		void DrawFrame();

		void RecreateSwapChain();

		float GetFrameTime();

	private:

		std::unique_ptr<RenderContext> mRenderContext;
		std::unique_ptr<RenderQueue> mRenderQueue;

	};
}




