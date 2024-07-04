#include "Application.hpp"

#include "HedgehogEngine/Context/Context.hpp"
#include "HedgehogEngine/Context/VulkanContext.hpp"

#include "HedgehogEngine/Renderer/Renderer.hpp"

#include "Logger/Logger.hpp"

#include <chrono>

namespace HedgehogClient
{
	HedgehogClient::HedgehogClient()
	{
	}

	HedgehogClient::~HedgehogClient()
	{
	}

	void HedgehogClient::Run()
	{
		InitVulkan();
		MainLoop();
	}

	void HedgehogClient::InitVulkan()
	{
		mContext = std::make_unique<Context::Context>();
		mRenderer = std::make_unique<Renderer::Renderer>(*mContext);
		LOGINFO("Vulkan initialized");
	}

	void HedgehogClient::MainLoop()
	{

		while (!mContext->GetVulkanContext().ShouldClose())
		{
			float dt = GetFrameTime();
			mContext->GetVulkanContext().HandleInput();
			mContext->UpdateContext(dt);
			mRenderer->DrawFrame(*mContext);
		}

		Cleanup();
	}

	void HedgehogClient::Cleanup()
	{
		mRenderer->Cleanup(*mContext);
		mContext->Cleanup();
	}

	float HedgehogClient::GetFrameTime()
	{
		static auto prevTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();
		prevTime = currentTime;
		return deltaTime;

	}

}

