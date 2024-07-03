#include "Application.hpp"

#include "Logger/Logger.hpp"

#include <chrono>

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
			float dt = GetFrameTime();
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

	float VkApplication::GetFrameTime()
	{
		static auto prevTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();
		prevTime = currentTime;
		return deltaTime;

	}

}

