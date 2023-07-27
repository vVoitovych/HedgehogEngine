#include "Application.h"

#include <iostream>

namespace VkEngine
{
	void VkApplication::Run()
	{
		InitVulkan();
		MainLoop();
	}

	void VkApplication::InitVulkan()
	{
		mRenderer.Initialize();
		std::cout << "Vulkan initialized" << std::endl;
	}

	void VkApplication::MainLoop()
	{
		while (!mRenderer.ShouldClose())
		{
			glfwPollEvents();
			mRenderer.DrawFrame();
		}

		Cleanup();
	}

	void VkApplication::Cleanup()
	{
		mRenderer.Cleanup();
	}
}

