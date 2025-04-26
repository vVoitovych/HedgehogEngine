#pragma once

#include <vulkan/vulkan.h>
#include <vector>

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

		Pipeline(Pipeline&& rhs) noexcept;
		Pipeline& operator=(Pipeline&& rhs) noexcept;

		void Cleanup(const Device& device);

		VkPipeline GetNativePipeline() const;
		VkPipelineLayout GetNativePipelineLayout() const;

	private:
		VkPipeline m_Pipeline;
		VkPipelineLayout m_GraphycsPipelineLayout;

	};
}