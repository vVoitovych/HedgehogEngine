#pragma once

#include <memory>

namespace Context
{
	class VulkanContext;
	class EngineContext;
	class FrameContext;
	class ThreadContext;

	class Context
	{
	public:
		Context();
		~Context();

		void UpdateContext(float dt);

		void Cleanup();

		Context(const Context&) = delete;
		Context& operator=(const Context&) = delete;
		Context(Context&&) = delete;
		Context& operator=(Context&&) = delete;

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


