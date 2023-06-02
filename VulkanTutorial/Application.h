#pragma once

#include "VkWindow.h"
#include "Renderer/VulkanWrapper.h"

// std lib
#include <memory>

namespace VkEngine
{
	class VkApplication
	{
	public:
		void Run();
	public:
		static constexpr int sWindowWidth = 800;
		static constexpr int sWindowHeight = 600;

	private:
		void InitWindow(int inWidth, int inHeight, std::string inName);
		void InitVulkan();
		void MainLoop();
		void Cleanup();

	private:
		std::shared_ptr<VkWindow> mWindow;
		std::unique_ptr<Renderer::VulkanWrapper> mVulkanWrapper;
	};
}



