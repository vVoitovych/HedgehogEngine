#pragma once

#include "HedgehogEngine/Renderer/Renderer.hpp"

// std lib
#include <memory>

namespace VkEngine
{
	class VkApplication
	{
	public:
		void Run();

	private:
		void InitVulkan();
		void MainLoop();
		void Cleanup();

		float GetFrameTime();

	private:
		Renderer::Renderer mRenderer;
	};
}



