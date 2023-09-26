#pragma once

#include "VulkanEngine/Renderer/Common/pch.h"

namespace Renderer
{
	class Device;
	class SwapChain;
	class RenderPass;
	class DescriptorSetLayout;

	class Pipeline
	{
	public:
		Pipeline();
		~Pipeline();

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		void Initialize(Device& device, SwapChain& swapChain, RenderPass& renderPass, DescriptorSetLayout& layout);
		void Cleanup();

		VkPipeline GetNativePipeline() const;
		VkPipelineLayout GetNativePipelineLayout();

	private:
		VkDevice mDevice;
		VkPipeline mPipeline;
		VkPipelineLayout mGraphycsPipelineLayout;

	};
}