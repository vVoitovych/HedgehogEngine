#pragma once

#include "Renderer/Wrappeers/Pipeline/PipelineInfo.hpp"

#include <vulkan/vulkan.h>
#include <memory>
#include <array>
#include <vector>

namespace Renderer
{
	class Device;
	class Shader;

	class ForwardPipelineInfo : public PipelineInfo
	{
	public:
		ForwardPipelineInfo(const std::unique_ptr<Device>& device);
		~ForwardPipelineInfo();

		void Cleanup(const std::unique_ptr<Device>& device) override;

		uint32_t GetStagesCount() override;
		VkPipelineShaderStageCreateInfo* GetStages() override;
		VkPipelineVertexInputStateCreateInfo* GetVertexInputInfo() override;
		VkPipelineInputAssemblyStateCreateInfo* GetInputAssemblyInfo() override;
		VkPipelineViewportStateCreateInfo* GetViewportInfo() override;
		VkPipelineRasterizationStateCreateInfo* GetRasterizationInfo() override;
		VkPipelineMultisampleStateCreateInfo* GetMultisamplingInfo() override;
		VkPipelineDepthStencilStateCreateInfo* GetDepthStencilInfo() override;
		VkPipelineColorBlendStateCreateInfo* GetColorBlendingInfo() override;
		VkPipelineDynamicStateCreateInfo* GetDynamicStateInfo() override;

	private:
		std::unique_ptr<Shader> mVertexShader;
		std::unique_ptr<Shader> mFragmentShader;
		std::array<VkPipelineShaderStageCreateInfo, 2> mStages;

		VkVertexInputBindingDescription mBindingDesc;
		std::array<VkVertexInputAttributeDescription, 4>  mAttributeDesc;
		VkPipelineVertexInputStateCreateInfo mVertexInputInfo;

		VkPipelineInputAssemblyStateCreateInfo mInputAssembly;
		VkPipelineViewportStateCreateInfo mViewportInfo;
		VkPipelineRasterizationStateCreateInfo mRasterizerInfo;
		VkPipelineMultisampleStateCreateInfo mMultisampling;
		VkPipelineDepthStencilStateCreateInfo mDepthStencil;
		VkPipelineColorBlendAttachmentState mColorBlendAttachmentState;
		VkPipelineColorBlendStateCreateInfo mColorBlending;
		std::vector<VkDynamicState> mDynamicStates;
		VkPipelineDynamicStateCreateInfo mDynamicStateInfo;

	};
}




