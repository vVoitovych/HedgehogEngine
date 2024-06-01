#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace Hedgehog
{
	namespace Wrappers
	{
		class Device;
		class RenderPass;
		class DescriptorSetLayout;
		class PipelineInfo;

		class Pipeline
		{
		public:
			Pipeline(
				const Device& device,
				const RenderPass& renderPass,
				const std::vector<VkDescriptorSetLayout>& layouts,
				const std::vector<VkPushConstantRange>& pushConstantRanges,
				const PipelineInfo& info);
			~Pipeline();

			Pipeline(const Pipeline&) = delete;
			Pipeline& operator=(const Pipeline&) = delete;

			void Cleanup(const Device& device);

			VkPipeline GetNativePipeline() const;
			VkPipelineLayout GetNativePipelineLayout() const;

		private:
			VkPipeline mPipeline;
			VkPipelineLayout mGraphycsPipelineLayout;

		};
	}
}

