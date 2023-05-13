#include "Application.h"

#include <iostream>

namespace VkEngine
{
	void VkApplication::Run()
	{
		InitWindow(sWindowWidth, sWindowHeight, "Vulkan App");
		MainLoop();
		Cleanup();
	}

	void VkApplication::InitWindow(int inWidth, int inHeight, std::string inName)
	{
		mWindow = std::make_unique<VkWindow>(inWidth, inHeight, inName);
		std::cout << "Window initialed!" << std::endl;
	}

	void VkApplication::MainLoop()
	{
		while (!mWindow->shouldClose())
		{
			glfwPollEvents();
		}
	}

	void VkApplication::Cleanup()
	{
	}
}

