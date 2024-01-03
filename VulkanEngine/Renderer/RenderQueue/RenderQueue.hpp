#pragma once 

#include "VulkanEngine/Renderer/RenderPasses/InitPass.hpp"
#include "VulkanEngine/Renderer/RenderPasses/ForwardPass.hpp"
#include "VulkanEngine/Renderer/RenderPasses/PresentPass.hpp"

#include <memory>

namespace Renderer
{
	class Device;
	class SwapChain;
	class RenderContext;

	class RenderQueue
	{
	public:
		RenderQueue(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain);
		~RenderQueue();

		RenderQueue(const RenderQueue&) = delete;
		RenderQueue& operator=(const RenderQueue&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);
		void Render(std::unique_ptr<RenderContext>& context);

		void CleanSizedResources(const std::unique_ptr<Device>& device);
		void CreateSizedResources(const std::unique_ptr<Device>& device, const std::unique_ptr<SwapChain>& swapChain);

	private:
		std::unique_ptr<InitPass> mInitPass;
		std::unique_ptr<ForwardPass> mForwardPass;
		std::unique_ptr<PresentPass> mPresentPass;

	};


}



