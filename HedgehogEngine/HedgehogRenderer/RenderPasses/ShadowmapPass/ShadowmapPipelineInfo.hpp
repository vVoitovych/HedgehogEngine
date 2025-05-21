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
	class ShadowmapPipelineInfo : public Wrappers::PipelineInfo
	{
	public:
		ShadowmapPipelineInfo(const Wrappers::Device& device);
		~ShadowmapPipelineInfo();

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
		std::unique_ptr<Wrappers::VertexShader> m_VertexShader;
		std::array<VkPipelineShaderStageCreateInfo, 1> m_Stages;

		std::array<VkVertexInputBindingDescription, 3> m_BindingDesc;
		std::array<VkVertexInputAttributeDescription, 3>  m_AttributeDesc;
		VkPipelineVertexInputStateCreateInfo m_VertexInputInfo;

		VkPipelineInputAssemblyStateCreateInfo m_InputAssembly;
		VkPipelineViewportStateCreateInfo m_ViewportInfo;
		VkPipelineRasterizationStateCreateInfo m_RasterizerInfo;
		VkPipelineMultisampleStateCreateInfo m_Multisampling;
		VkPipelineDepthStencilStateCreateInfo m_DepthStencil;
		VkPipelineColorBlendAttachmentState m_ColorBlendAttachmentState;
		VkPipelineColorBlendStateCreateInfo m_ColorBlending;
		std::vector<VkDynamicState> m_DynamicStates;
		VkPipelineDynamicStateCreateInfo m_DynamicStateInfo;

	};
}




