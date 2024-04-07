#pragma once

#include "VulkanEngine/Renderer/Renderer.hpp"

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



