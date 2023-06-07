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
		mWindow = std::make_shared<VkWindow>(inWidth, inHeight, inName);
		std::cout << "Window initialized" << std::endl;
	}

	void VkApplication::InitVulkan()
	{
		mVulkanWrapper = std::make_unique<Renderer::VulkanWrapper>(mWindow);
		std::cout << "Vulkan initialized" << std::endl;
	}

	void VkApplication::MainLoop()
	{
		while (!mWindow->shouldClose())
		{
			glfwPollEvents();
			mVulkanWrapper->DrawFrame();
		}
	}

	void VkApplication::Cleanup()
	{
	}
}

