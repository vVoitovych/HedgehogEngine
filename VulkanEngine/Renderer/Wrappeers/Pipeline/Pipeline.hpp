#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace Renderer
{
	class Device;
	class RenderPass;
	class DescriptorSetLayout;
	class PipelineInfo;

	class Pipeline
	{
	public:
		Pipeline(
			const std::unique_ptr<Device>& device,
			const std::unique_ptr<RenderPass>& renderPass,
			const std::vector<VkDescriptorSetLayout>& layouts,
			const std::vector<VkPushConstantRange>& pushConstantRanges,
			const std::unique_ptr<PipelineInfo>& info);
		~Pipeline();

		Pipeline(const Pipeline&) = delete;
		Pipeline& operator=(const Pipeline&) = delete;

		void Cleanup(const std::unique_ptr<Device>& device);

		VkPipeline GetNativePipeline() const;
		VkPipelineLayout GetNativePipelineLayout();

	private:
		VkPipeline mPipeline;
		VkPipelineLayout mGraphycsPipelineLayout;

	};
}