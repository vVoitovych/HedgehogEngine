#pragma once

#include <memory>

namespace Context
{
	class Context;
}

namespace Renderer
{
	class ResourceManager;
	class RenderQueue;

	class Renderer
	{
	public:
		Renderer(const Context::Context& context);
		~Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		void Cleanup(const Context::Context& context);

		void DrawFrame(Context::Context& context);

		void RecreateSwapChain(Context::Context& context);

	private:
		std::unique_ptr< ResourceManager> m_ResourceManager;
		std::unique_ptr<RenderQueue> m_RenderQueue;

	};
}




