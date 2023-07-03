#include "Application.h"

#include <iostream>

namespace VkEngine
{
	void VkApplication::Run()
	{
		InitWindow(sWindowWidth, sWindowHeight, "Vulkan App");
		InitVulkan();
		MainLoop();
	}

	void VkApplication::InitWindow(int inWidth, int inHeight, std::string inName)
	{
		mWindowManager.Initialize(Renderer::WindowState::GetDefaultState());
		std::cout << "Window initialized" << std::endl;
	}

	void VkApplication::InitVulkan()
	{
		mRenderer.Initialize(mWindowManager);
		std::cout << "Vulkan initialized" << std::endl;
	}

	void VkApplication::MainLoop()
	{
		while (!mWindowManager.ShouldClose())
		{
			glfwPollEvents();
			mRenderer.DrawFrame();
		}

		Cleanup();
	}

	void VkApplication::Cleanup()
	{
		mRenderer.Cleanup();
		mWindowManager.Cleanup();
	}
}

