#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <array>
#include <vector>

namespace Renderer
{
	class Device;
	class Shader;

	class PipelineInfo
	{
	public:
		PipelineInfo(const std::unique_ptr<Device>& device) {}
		virtual ~PipelineInfo() {}

		virtual void Cleanup(const std::unique_ptr<Device>& device) = 0;

		virtual uint32_t GetStagesCount() = 0;
		virtual VkPipelineShaderStageCreateInfo* GetStages() = 0;
		virtual VkPipelineVertexInputStateCreateInfo* GetVertexInputInfo() = 0;
		virtual VkPipelineInputAssemblyStateCreateInfo* GetInputAssemblyInfo() = 0;
		virtual VkPipelineViewportStateCreateInfo* GetViewportInfo() = 0;
		virtual VkPipelineRasterizationStateCreateInfo* GetRasterizationInfo() = 0;
		virtual VkPipelineMultisampleStateCreateInfo* GetMultisamplingInfo() = 0;
		virtual VkPipelineDepthStencilStateCreateInfo* GetDepthStencilInfo() = 0;
		virtual VkPipelineColorBlendStateCreateInfo* GetColorBlendingInfo() = 0;
		virtual VkPipelineDynamicStateCreateInfo* GetDynamicStateInfo() = 0;

	};
}


