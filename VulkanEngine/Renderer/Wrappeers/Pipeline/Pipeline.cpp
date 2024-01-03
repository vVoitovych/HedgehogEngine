#include "Pipeline.hpp"
#include "PipelineInfo.hpp"

#include "VulkanEngine/Renderer/Wrappeers/Device/Device.hpp"
#include "VulkanEngine/Renderer/Wrappeers/RenderPass/RenderPass.hpp"
#include "VulkanEngine/Renderer/Wrappeers/Descriptors/DescriptorSetLayout.hpp"
#include "VulkanEngine/Renderer/Containers/VertexDescription.hpp"
#include "VulkanEngine/Logger/Logger.hpp"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.hpp"

#include <fstream>

namespace Renderer
{

	Pipeline::Pipeline(
		const std::unique_ptr<Device>& device, 
		const std::unique_ptr<RenderPass>& renderPass, 
		const std::unique_ptr<DescriptorSetLayout>& layout,
		const std::unique_ptr<PipelineInfo>& info)
		: mPipeline(nullptr)
		, mGraphycsPipelineLayout(nullptr)
	{
		VkPipelineLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = 1;
		layoutCreateInfo.pSetLayouts = layout->GetNativeLayout();
		layoutCreateInfo.pushConstantRangeCount = 0;
		layoutCreateInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(device->GetNativeDevice(), &layoutCreateInfo, nullptr, &mGraphycsPipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = info->GetStagesCount();
		pipelineInfo.pStages = info->GetStages();
		pipelineInfo.pVertexInputState = info->GetVertexInputInfo();
		pipelineInfo.pInputAssemblyState = info->GetInputAssemblyInfo();
		pipelineInfo.pViewportState = info->GetViewportInfo();
		pipelineInfo.pRasterizationState = info->GetRasterizationInfo();
		pipelineInfo.pMultisampleState = info->GetMultisamplingInfo();
		pipelineInfo.pDepthStencilState = info->GetDepthStencilInfo();
		pipelineInfo.pColorBlendState = info->GetColorBlendingInfo();
		pipelineInfo.pDynamicState = info->GetDynamicStateInfo();
		pipelineInfo.layout = mGraphycsPipelineLayout;
		pipelineInfo.renderPass = renderPass->GetNativeRenderPass();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(device->GetNativeDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline) != VK_SUCCESS)
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


	void Pipeline::Cleanup(const std::unique_ptr<Device>& device)
	{
		vkDestroyPipeline(device->GetNativeDevice(), mPipeline, nullptr);
		vkDestroyPipelineLayout(device->GetNativeDevice(), mGraphycsPipelineLayout, nullptr);

		mPipeline = nullptr;
		mGraphycsPipelineLayout = nullptr;
		LOGINFO("Pipeline cleaned");
	}

	VkPipeline Pipeline::GetNativePipeline() const
	{
		return mPipeline;
	}

	VkPipelineLayout Pipeline::GetNativePipelineLayout()
	{
		return mGraphycsPipelineLayout;
	}


}


