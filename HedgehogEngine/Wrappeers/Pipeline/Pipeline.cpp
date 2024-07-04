#include "Pipeline.hpp"
#include "PipelineInfo.hpp"

#include "Wrappeers/Device/Device.hpp"
#include "Wrappeers/RenderPass/RenderPass.hpp"
#include "Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "Containers/VertexDescription.hpp"
#include "Common/EngineDebugBreak.hpp"

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
		: mPipeline(nullptr)
		, mGraphycsPipelineLayout(nullptr)
	{
		VkPipelineLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
		layoutCreateInfo.pSetLayouts = layouts.data();
		layoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
		layoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();

		if (vkCreatePipelineLayout(device.GetNativeDevice(), &layoutCreateInfo, nullptr, &mGraphycsPipelineLayout) != VK_SUCCESS)
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
		pipelineInfo.layout = mGraphycsPipelineLayout;
		pipelineInfo.renderPass = renderPass.GetNativeRenderPass();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(device.GetNativeDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline");
		}

		LOGINFO("Pipeline created");
	}

	Pipeline::~Pipeline()
	{
		if (mPipeline != nullptr)
		{
			LOGERROR("Vulkan pipeline should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
		if (mGraphycsPipelineLayout != nullptr)
		{
			LOGERROR("Vulkan pipeline layout should be cleanedup before destruction!");
			ENGINE_DEBUG_BREAK();
		}
	}


	void Pipeline::Cleanup(const Device& device)
	{
		vkDestroyPipeline(device.GetNativeDevice(), mPipeline, nullptr);
		vkDestroyPipelineLayout(device.GetNativeDevice(), mGraphycsPipelineLayout, nullptr);

		mPipeline = nullptr;
		mGraphycsPipelineLayout = nullptr;
		LOGINFO("Pipeline cleaned");
	}

	VkPipeline Pipeline::GetNativePipeline() const
	{
		return mPipeline;
	}

	VkPipelineLayout Pipeline::GetNativePipelineLayout() const
	{
		return mGraphycsPipelineLayout;
	}


}


