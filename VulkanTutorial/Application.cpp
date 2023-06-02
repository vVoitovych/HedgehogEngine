#include "Application.h"

#include <iostream>

namespace VkEngine
{
	void VkApplication::Run()
	{
		InitWindow(sWindowWidth, sWindowHeight, "Vulkan App");
		InitVulkan();
		MainLoop();
		Cleanup();
	}

	void VkApplication::InitWindow(int inWidth, int inHeight, std::string inName)
	{
		std::cout << "Window initialization!" << std::endl;
		mWindow = std::make_shared<VkWindow>(inWidth, inHeight, inName);
		std::cout << "Window initialized!" << std::endl;
	}

	void VkApplication::InitVulkan()
	{
		std::cout << "Vulkan initialization!" << std::endl;
		mVulkanWrapper = std::make_unique<Renderer::VulkanWrapper>(mWindow);
		std::cout << "Vulkan initialized!" << std::endl;
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

