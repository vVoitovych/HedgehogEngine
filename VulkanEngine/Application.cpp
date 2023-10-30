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
			float dt = mRenderer.GetFrameTime();
			mRenderer.HandleInput();
			mRenderer.Update(dt);
			mRenderer.UpdateUniformBuffer();
			mRenderer.DrawFrame();
		}

		Cleanup();
	}

	void VkApplication::Cleanup()
	{
		mRenderer.Cleanup();
	}

}

