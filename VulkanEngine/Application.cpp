#include "Application.hpp"

#include "VulkanEngine/Logger/Logger.hpp"

namespace VkEngine
{
	void VkApplication::Run()
	{
		InitVulkan();
		MainLoop();
	}

	void VkApplication::InitVulkan()
	{
		LOGINFO("Vulkan initialized");
	}

	void VkApplication::MainLoop()
	{

		while (!mRenderer.ShouldClose())
		{
			float dt = mRenderer.GetFrameTime();
			mRenderer.HandleInput();
			mRenderer.Update(dt);
			mRenderer.DrawFrame();
		}

		Cleanup();
	}

	void VkApplication::Cleanup()
	{
		mRenderer.Cleanup();
	}

}

