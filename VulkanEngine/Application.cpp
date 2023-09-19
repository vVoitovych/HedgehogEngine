#include "Application.h"

#include "VulkanEngine/Logger/Logger.h"

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
		LOGINFO("Vulkan initialized");
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

