#include "Pipeline.h"
#include <Windows.h>

#include "VulkanEngine/Renderer/Device/Device.h"
#include "VulkanEngine/Renderer/SwapChain/SwapChain.h"
#include "VulkanEngine/Renderer/RenderPass/RenderPass.h"
#include "VulkanEngine/Renderer/Descriptors/DescriptorSetLayout.h"
#include "VulkanEngine/Renderer/Mesh/Vertex.h"
#include "VulkanEngine/Logger/Logger.h"
#include "VulkanEngine/Renderer/Common/EngineDebugBreak.h"

#include <fstream>

namespace Renderer
{
	Pipeline::Pipeline()
		: mPipeline(nullptr)
		, mGraphycsPipelineLayout(nullptr)
		, mDevice(nullptr)
	{
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

	std::string GetCurrentDirectory()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(NULL, buffer, MAX_PATH);
		std::string::size_type pos = std::string(buffer).find_last_of("\\/");

		return std::string(buffer).substr(0, pos);
	}

	std::vector<char> ReadFile(const std::string& filename)
	{
		std::string fullName = GetCurrentDirectory() + "\\" + filename;
		LOGINFO(fullName);

		std::ifstream file(fullName, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file!");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);

		file.close();

		return buffer;
	}

	VkShaderModule CreateShaderModule(VkDevice device, std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shadeer module!");
		}
		return shaderModule;
	}

	void Pipeline::Initialize(Device& device, SwapChain& swapChain, RenderPass& renderPass, DescriptorSetLayout& layout)
	{
		mDevice = device.GetNativeDevice();

		auto vertexShaderCode = ReadFile("CompiledShaders\\Shaders\\SimpleShader.vert.spv");
		auto fragmentShaderCode = ReadFile("CompiledShaders\\Shaders\\SimpleShader.frag.spv");

		VkShaderModule vertShaderModule = CreateShaderModule(mDevice, vertexShaderCode);
		VkShaderModule fragShaderModule = CreateShaderModule(mDevice, fragmentShaderCode);

		VkPipelineShaderStageCreateInfo vertShadereStageCreateInfo{};
		vertShadereStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShadereStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShadereStageCreateInfo.module = vertShaderModule;
		vertShadereStageCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShadereStageCreateInfo{};
		fragShadereStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShadereStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShadereStageCreateInfo.module = fragShaderModule;
		fragShadereStageCreateInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShadereStageCreateInfo , fragShadereStageCreateInfo };

		auto bindingDesc = Vertex::GetBindingDescription();
		auto atributeDesc = Vertex::GetAttributeDescription();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(atributeDesc.size());
		vertexInputInfo.pVertexAttributeDescriptions = atributeDesc.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicStateInfo{};
		dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicStateInfo.pDynamicStates = dynamicStates.data();

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChain.GetSwapChainExtend().width;
		viewport.height = (float)swapChain.GetSwapChainExtend().height;
		viewport.minDepth = 0.0;
		viewport.maxDepth = 1.0f;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = swapChain.GetSwapChainExtend();

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizerInfo{};
		rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizerInfo.depthClampEnable = VK_FALSE;
		rasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
		rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizerInfo.lineWidth = 1.0f;
		rasterizerInfo.cullMode = VK_CULL_MODE_NONE;
		rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizerInfo.depthBiasEnable = VK_FALSE;
		rasterizerInfo.depthBiasConstantFactor = 0.0f;
		rasterizerInfo.depthBiasClamp = 0.0f;
		rasterizerInfo.depthBiasSlopeFactor = 0.0f;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
		colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachmentState.blendEnable = VK_FALSE;
		colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachmentState;
		colorBlending.blendConstants[0] = 0.0f;
		colorBlending.blendConstants[1] = 0.0f;
		colorBlending.blendConstants[2] = 0.0f;
		colorBlending.blendConstants[3] = 0.0f;


		VkPipelineLayoutCreateInfo layoutCreateInfo{};
		layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutCreateInfo.setLayoutCount = 1;
		layoutCreateInfo.pSetLayouts = layout.GetNativeLayout();
		layoutCreateInfo.pushConstantRangeCount = 0;
		layoutCreateInfo.pPushConstantRanges = nullptr;

		if (vkCreatePipelineLayout(mDevice, &layoutCreateInfo, nullptr, &mGraphycsPipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline layout!");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizerInfo;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = &dynamicStateInfo;
		pipelineInfo.layout = mGraphycsPipelineLayout;
		pipelineInfo.renderPass = renderPass.GetNativeRenderPass();
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		if (vkCreateGraphicsPipelines(mDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create pipeline");
		}

		vkDestroyShaderModule(mDevice, vertShaderModule, nullptr);
		vkDestroyShaderModule(mDevice, fragShaderModule, nullptr);

		LOGINFO("Pipeline created");
	}

	void Pipeline::Cleanup()
	{
		vkDestroyPipeline(mDevice, mPipeline, nullptr);
		vkDestroyPipelineLayout(mDevice, mGraphycsPipelineLayout, nullptr);

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


