#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <vector>

namespace Wrappers
{
	class Device;
	class Shader;

	class PipelineInfo
	{
	public:
		PipelineInfo(const Device& device) {}
		virtual ~PipelineInfo() {}

		virtual void Cleanup(const Device& device) = 0;

		virtual const uint32_t GetStagesCount() const = 0;
		virtual const VkPipelineShaderStageCreateInfo* GetStages() const = 0;
		virtual const VkPipelineVertexInputStateCreateInfo* GetVertexInputInfo() const = 0;
		virtual const VkPipelineInputAssemblyStateCreateInfo* GetInputAssemblyInfo() const = 0;
		virtual const VkPipelineViewportStateCreateInfo* GetViewportInfo() const = 0;
		virtual const VkPipelineRasterizationStateCreateInfo* GetRasterizationInfo() const = 0;
		virtual const VkPipelineMultisampleStateCreateInfo* GetMultisamplingInfo() const = 0;
		virtual const VkPipelineDepthStencilStateCreateInfo* GetDepthStencilInfo() const = 0;
		virtual const VkPipelineColorBlendStateCreateInfo* GetColorBlendingInfo() const = 0;
		virtual const VkPipelineDynamicStateCreateInfo* GetDynamicStateInfo() const = 0;

	};
}


