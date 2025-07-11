#pragma once

#include <memory>

namespace Context
{
	class VulkanContext;
	class EngineContext;
	class RendererContext;

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
		RendererContext& GetRendererContext();

		const VulkanContext& GetVulkanContext() const;
		const EngineContext& GetEngineContext() const;
		const RendererContext& GetRendererContext() const;

	private:
		std::unique_ptr<VulkanContext> m_VulkanContext;
		std::unique_ptr<EngineContext> m_EngineContext;
		std::unique_ptr<RendererContext> m_RendererContext;

	};

}


