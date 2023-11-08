#pragma once

#include <vulkan/vulkan.h>

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

		void Initialize(const Device& device, SwapChain& swapChain, RenderPass& renderPass, DescriptorSetLayout& layout);
		void Cleanup(const Device& device);

		VkPipeline GetNativePipeline() const;
		VkPipelineLayout GetNativePipelineLayout();

	private:
		VkPipeline mPipeline;
		VkPipelineLayout mGraphycsPipelineLayout;

	};
}