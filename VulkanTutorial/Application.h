#pragma once

#include "Renderer/Renderer.h"

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

	private:
		Renderer::Renderer mRenderer;
	};
}



