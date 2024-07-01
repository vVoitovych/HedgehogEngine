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

		VulkanContext& GetVulkanContext();
		EngineContext& GetEngineContext();
		FrameContext& GetFrameContext();
		ThreadContext& GetThreadContext();

		const VulkanContext& GetVulkanContext() const;
		const EngineContext& GetEngineContext() const;
		const FrameContext& GetFrameContext() const;
		const ThreadContext& GetThreadContext() const;

	private:
		std::unique_ptr<VulkanContext> mVulkanContext;
		std::unique_ptr<EngineContext> mEngineContext;
		std::unique_ptr<FrameContext> mFrameContext;
		std::unique_ptr<ThreadContext> mThreadContext;

	};

}


