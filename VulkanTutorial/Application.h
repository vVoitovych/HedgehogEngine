#pragma once

#include "VkWindow.h"

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
		void MainLoop();
		void Cleanup();

	private:
		std::unique_ptr<VkWindow> mWindow;

	};
}



