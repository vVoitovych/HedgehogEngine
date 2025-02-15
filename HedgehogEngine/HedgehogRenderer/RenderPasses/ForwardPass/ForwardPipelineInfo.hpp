#pragma once

#include "HedgehogWrappers/Wrappeers/Pipeline/PipelineInfo.hpp"

#include <vulkan/vulkan.h>
#include <memory>
#include <array>
#include <vector>

namespace Wrappers
{
	class Device;
	class VertexShader;
	class FragmentShader;
}

namespace Renderer
{
	class ForwardPipelineInfo : public Wrappers::PipelineInfo
	{
	public:
		ForwardPipelineInfo(const Wrappers::Device& device);
		~ForwardPipelineInfo();

		void Cleanup(const Wrappers::Device& device) override;

		const uint32_t GetStagesCount() const override;
		const VkPipelineShaderStageCreateInfo* GetStages() const override;
		const VkPipelineVertexInputStateCreateInfo* GetVertexInputInfo() const override;
		const VkPipelineInputAssemblyStateCreateInfo* GetInputAssemblyInfo() const override;
		const VkPipelineViewportStateCreateInfo* GetViewportInfo() const override;
		const VkPipelineRasterizationStateCreateInfo* GetRasterizationInfo() const override;
		const VkPipelineMultisampleStateCreateInfo* GetMultisamplingInfo() const override;
		const VkPipelineDepthStencilStateCreateInfo* GetDepthStencilInfo() const override;
		const VkPipelineColorBlendStateCreateInfo* GetColorBlendingInfo() const override;
		const VkPipelineDynamicStateCreateInfo* GetDynamicStateInfo() const override;

	private:
		std::unique_ptr<Wrappers::VertexShader> mVertexShader;
		std::unique_ptr<Wrappers::FragmentShader> mFragmentShader;
		std::array<VkPipelineShaderStageCreateInfo, 2> mStages;

		VkVertexInputBindingDescription mBindingDesc;
		std::array<VkVertexInputAttributeDescription, 5>  mAttributeDesc;
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




