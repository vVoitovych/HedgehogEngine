#include "Renderer.hpp"

#include "WindowManagment/WindowManager.hpp"
#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/SwapChain/SwapChain.hpp"
#include "Context/RenderContext.hpp"
#include "Context/EngineContext.hpp"
#include "Context/FrameContext.hpp"
#include "Context/ThreadContext.hpp"
#include "RenderQueue/RenderQueue.hpp"

#include "Logger/Logger.hpp"

#include <chrono>

namespace Renderer
{
	Renderer::Renderer()
	{
		auto windowManager = std::make_unique<WindowManager>(WindowState::GetDefaultState());
		mDevice = std::make_unique<Device>(windowManager);
		mSwapChain = std::make_unique<SwapChain>(mDevice, windowManager);

		mRenderContext = std::make_unique<RenderContext>(mDevice, mSwapChain, std::move(windowManager));
		mRenderQueue = std::make_unique<RenderQueue>(mDevice, mSwapChain);

	}

	Renderer::~Renderer()
	{
	}
	 
	void Renderer::Cleanup()
	{
		vkQueueWaitIdle(mDevice->GetNativeGraphicsQueue());

		mRenderQueue->Cleanup(mDevice);
		mRenderContext->Cleanup(mDevice);
		mSwapChain->Cleanup(mDevice);
		mDevice->Cleanup();
	}

	void Renderer::HandleInput()
	{
		auto& engineContext = mRenderContext->GetEngineContext();
		engineContext->HandleInput();
	}

	void Renderer::Update(float dt)
	{
		mRenderContext->UpdateContext(dt);

	}

	void Renderer::DrawFrame()
	{
		mRenderQueue->Render(mRenderContext);
		
		auto& engineContext = mRenderContext->GetEngineContext();
		auto dt = GetFrameTime();
		engineContext->UpdateContext(dt);
		if (engineContext->IsWindowResized())
		{
			RecreateSwapChain();
			engineContext->ResetWindowResizeState(mSwapChain->GetSwapChainExtend());
		}
	}

	void Renderer::RecreateSwapChain()
	{
		vkDeviceWaitIdle(mDevice->GetNativeDevice());
		mRenderQueue->CleanSizedResources(mDevice);
		mSwapChain->Recreate(mDevice);
		mRenderQueue->CreateSizedResources(mDevice, mSwapChain);
	}

	bool Renderer::ShouldClose()
	{
		auto& engineContext = mRenderContext->GetEngineContext();
		return engineContext->ShouldClose();
	}

	float Renderer::GetFrameTime()
	{
		static auto prevTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();
		prevTime = currentTime;
		return deltaTime;
	}

}



