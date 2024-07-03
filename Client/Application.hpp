#pragma once

#include <memory>

namespace Renderer
{
	class Renderer;
	class RenderContext;
}

namespace HedgehogClient
{
	class HedgehogClient
	{
	public:
		HedgehogClient();
		~HedgehogClient();

		void Run();

	private:
		void InitVulkan();
		void MainLoop();
		void Cleanup();

		float GetFrameTime();

	private:
		std::unique_ptr<Renderer::RenderContext> mContext;
		std::unique_ptr<Renderer::Renderer> mRenderer;
	};
}



