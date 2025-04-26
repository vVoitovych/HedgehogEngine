#include "Pipeline.hpp"
#include "PipelineInfo.hpp"

#include "HedgehogWrappers/Wrappeers/Device/Device.hpp"
#include "HedgehogWrappers/Wrappeers/RenderPass/RenderPass.hpp"
#include "HedgehogWrappers/Wrappeers/Descriptors/DescriptorSetLayout.hpp"

#include "HedgehogCommon/Common/EngineDebugBreak.hpp"

#include "Logger/Logger.hpp"

#include <fstream>

namespace Wrappers
{

	Pipeline::Pipeline(
		const Device& device, 
		const RenderPass& renderPass, 
		const std::vector<VkDescriptorSetLayout>& layouts,
		const std::vector<VkPushConstantRange>& pushConstantRanges,
		const PipelineInfo& info)
		: m_Pipeline(nullptr)
		, m_GraphycsPipelineLayout(nullptr)
	{
		VkPipelineLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
		layoutCreateInfo.pSetLayouts = layouts.data();
		layoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
		layoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

		if (vkCreatePipelineLayout(device.GetNativeDevice(), &layoutCreateInfo, nullptr, &m_GraphycsPipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = info.GetStagesCount();
		pipelineInfo.pStages = info.GetStages();
		pipelineInfo.pVertexInputState = info.GetVertexInputInfo();
		pipelineInfo.pInputAssemblyState = info.GetInputAssemblyInfo();
		pipelineInfo.pViewportState = info.GetViewportInfo();
		pipelineInfo.pRasterizationState = info.GetRasterizationInfo();
		pipelineInfo.pMultisampleState = info.GetMultisamplingInfo();
		pipelineInfo.pDepthStencilState = info.GetDepthStencilInfo();
		pipelineInfo.pColorBlendState = info.GetColorBlendingInfo();
		pipelineInfo.pDynamicState = info.GetDynamicStateInfo();
		pipelineInfo.layout = m_GraphycsPipelineLayout;
		pipelineInfo.renderPass = renderPass.GetNativeRenderPass();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;
		pipelineInfo.pNext = nullptr;

		if (vkCreateGraphicsPipelines(device.GetNativeDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline");
		}

		LOGINFO("Pipeline created");
	}

	Pipeline::~Pipeline()
	{
		if (m_Pipeline != nullptr)
		{
			LOGERROR("Vulkan pipeline should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (m_GraphycsPipelineLayout != nullptr)
		{
			LOGERROR("Vulkan pipeline layout should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}


	Pipeline::Pipeline(Pipeline&& rhs) noexcept
		: m_Pipeline(rhs.m_Pipeline)
		, m_GraphycsPipelineLayout(rhs.m_GraphycsPipelineLayout)
	{
		rhs.m_Pipeline = nullptr;
		rhs.m_GraphycsPipelineLayout = nullptr;
	}

	Pipeline& Pipeline::operator=(Pipeline&& rhs) noexcept
	{
		if (this != &rhs)
		{
			m_Pipeline = rhs.m_Pipeline;
			m_GraphycsPipelineLayout = rhs.m_GraphycsPipelineLayout;

			rhs.m_Pipeline = nullptr;
			rhs.m_GraphycsPipelineLayout = nullptr;
		}
		return *this;
	}

	void Pipeline::Cleanup(const Device& device)
	{
		vkDestroyPipeline(device.GetNativeDevice(), m_Pipeline, nullptr);
		vkDestroyPipelineLayout(device.GetNativeDevice(), m_GraphycsPipelineLayout, nullptr);

		m_Pipeline = nullptr;
		m_GraphycsPipelineLayout = nullptr;
		LOGINFO("Pipeline cleaned");
	}

	VkPipeline Pipeline::GetNativePipeline() const
	{
		return m_Pipeline;
	}

	VkPipelineLayout Pipeline::GetNativePipelineLayout() const
	{
		return m_GraphycsPipelineLayout;
	}


}


