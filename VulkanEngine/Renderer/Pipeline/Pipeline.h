#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"

namespace Renderer
{
	class Device;
	class SwapChain;
	class RenderPass;

	class Pipeline
	{
	public:
		Pipeline();
		~Pipeline();

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		void Initialize(Device& device, SwapChain& swapChain, RenderPass& renderPass);
		void Cleanup();

		VkPipeline GetNativePipeline() const;
	private:
		VkDevice mDevice;
		VkPipeline mPipeline;
		VkPipelineLayout mGraphycsPipelineLayout;

	};
}