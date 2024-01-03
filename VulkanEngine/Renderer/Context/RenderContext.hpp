#pragma once

#include <memory>

namespace Renderer
{
	class Device;
	class WindowManager;
	class SwapChain;

	class EngineContext;
	class FrameContext;
	class ThreadContext;

	class RenderContext
	{
	public:
		RenderContext(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain, std::unique_ptr<WindowManager>&& windowManager);
		~RenderContext();

		void UpdateContext(float dt);

		void Cleanup(const std::unique_ptr<Device>& device);

		RenderContext(const RenderContext&) = delete;
		RenderContext& operator=(const RenderContext&) = delete;

		std::unique_ptr<EngineContext>& GetEngineContext();
		std::unique_ptr<FrameContext>& GetFrameContext();
		std::unique_ptr<ThreadContext>& GetThreadContext();

		const std::unique_ptr<EngineContext>& GetEngineContext() const;
		const std::unique_ptr<FrameContext>& GetFrameContext() const;
		const std::unique_ptr<ThreadContext>& GetThreadContext() const;

		std::tuple<std::unique_ptr<EngineContext>&, std::unique_ptr<FrameContext>&, std::unique_ptr<ThreadContext>&> GetContexts();
		std::tuple<const std::unique_ptr<EngineContext>&, const std::unique_ptr<FrameContext>&, const std::unique_ptr<ThreadContext>&> GetContexts() const;


	private:
		std::unique_ptr<EngineContext> mEngineContext;
		std::unique_ptr<FrameContext> mFrameContext;
		std::unique_ptr<ThreadContext> mThreadContext;

	};

}


