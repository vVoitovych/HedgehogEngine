#pragma once

#include <memory>

namespace Renderer
{
	class VulkanContext;
	class EngineContext;
	class FrameContext;
	class ThreadContext;

	class RenderContext
	{
	public:
		RenderContext();
		~RenderContext();

		void UpdateContext(float dt);

		void Cleanup();

		RenderContext(const RenderContext&) = delete;
		RenderContext& operator=(const RenderContext&) = delete;
		RenderContext(RenderContext&&) = delete;
		RenderContext& operator=(RenderContext&&) = delete;

		std::unique_ptr<VulkanContext>& GetVulkanContext();
		std::unique_ptr<EngineContext>& GetEngineContext();
		std::unique_ptr<FrameContext>& GetFrameContext();
		std::unique_ptr<ThreadContext>& GetThreadContext();

		const std::unique_ptr<VulkanContext>& GetVulkanContext() const;
		const std::unique_ptr<EngineContext>& GetEngineContext() const;
		const std::unique_ptr<FrameContext>& GetFrameContext() const;
		const std::unique_ptr<ThreadContext>& GetThreadContext() const;

	private:
		std::unique_ptr<VulkanContext> mVulkanContext;
		std::unique_ptr<EngineContext> mEngineContext;
		std::unique_ptr<FrameContext> mFrameContext;
		std::unique_ptr<ThreadContext> mThreadContext;

	};

}


