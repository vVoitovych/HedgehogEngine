#pragma once

#include "Renderer/WindowManagment/WindowManager.h"
#include "Renderer/Renderer.h"

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
		Renderer::WindowManager mWindowManager;
		Renderer::Renderer mRenderer;
	};
}



