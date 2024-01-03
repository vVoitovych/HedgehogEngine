#pragma once

#include <memory>

namespace Renderer
{
	class WindowManager;
	class Device;
	class SwapChain;
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

		void HandleInput();
		void Update(float dt);

		void DrawFrame();

		void RecreateSwapChain();

		bool ShouldClose();

		float GetFrameTime();

	private:

		uint32_t currentFrame = 0;
		bool mShouldClose = false;

		std::unique_ptr<Device> mDevice;
		std::unique_ptr<SwapChain> mSwapChain;

		std::unique_ptr<RenderContext> mRenderContext;
		std::unique_ptr<RenderQueue> mRenderQueue;

	};
}




