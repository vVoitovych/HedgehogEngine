#include "ForwardPipelineInfo.hpp"

#include "HedgehogWrappers/Wrappeers/Shaders/VertexShader.hpp"
#include "HedgehogWrappers/Wrappeers/Shaders/FragmentShader.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	ForwardPipelineInfo::ForwardPipelineInfo(const Wrappers::Device& device)
		: Wrappers::PipelineInfo(device)
	{
		m_VertexShader = std::make_unique<Wrappers::VertexShader>(device, "ForwardPass/Base.vert");
		m_FragmentShader = std::make_unique<Wrappers::FragmentShader>(device, "ForwardPass/Base.frag");
		m_Stages = {m_VertexShader->GetCreateInfo(), m_FragmentShader->GetCreateInfo()};

		m_BindingDesc[0].binding = 0;
		m_BindingDesc[0].stride = 3 * sizeof(float);
		m_BindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		m_BindingDesc[1].binding = 1;
		m_BindingDesc[1].stride = 2 * sizeof(float);
		m_BindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		m_BindingDesc[2].binding = 2;
		m_BindingDesc[2].stride = 3 * sizeof(float);
		m_BindingDesc[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		m_AttributeDesc[0].binding = 0;
		m_AttributeDesc[0].location = 0;
		m_AttributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		m_AttributeDesc[0].offset = 0;

		m_AttributeDesc[1].binding = 1;
		m_AttributeDesc[1].location = 1;
		m_AttributeDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
		m_AttributeDesc[1].offset = 0;

		m_AttributeDesc[2].binding = 2;
		m_AttributeDesc[2].location = 2;
		m_AttributeDesc[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		m_AttributeDesc[2].offset = 0;


		m_VertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		m_VertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(m_BindingDesc.size());
		m_VertexInputInfo.pVertexBindingDescriptions = m_BindingDesc.data();
		m_VertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_AttributeDesc.size());
		m_VertexInputInfo.pVertexAttributeDescriptions = m_AttributeDesc.data();
		m_VertexInputInfo.flags = 0;
		m_VertexInputInfo.pNext = nullptr;

		m_InputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		m_InputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		m_InputAssembly.primitiveRestartEnable = VK_FALSE;
		m_InputAssembly.pNext = nullptr;
		m_InputAssembly.flags = 0;

		m_ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		m_ViewportInfo.viewportCount = 1;
		m_ViewportInfo.pViewports = nullptr;
		m_ViewportInfo.scissorCount = 1;
		m_ViewportInfo.pScissors = nullptr;
		m_ViewportInfo.flags = 0;
		m_ViewportInfo.pNext = nullptr;

		m_RasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		m_RasterizerInfo.depthClampEnable = VK_FALSE;
		m_RasterizerInfo.depthBiasClamp = 0.0f;
		m_RasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
		m_RasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
		m_RasterizerInfo.lineWidth = 1.0f;
		m_RasterizerInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
		m_RasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		m_RasterizerInfo.depthBiasEnable = VK_FALSE;
		m_RasterizerInfo.flags = 0;
		m_RasterizerInfo.pNext = nullptr;

		m_Multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		m_Multisampling.sampleShadingEnable = VK_FALSE;
		m_Multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		m_Multisampling.minSampleShading = 1.0f;
		m_Multisampling.pSampleMask = nullptr;
		m_Multisampling.alphaToCoverageEnable = VK_FALSE;
		m_Multisampling.alphaToOneEnable = VK_FALSE;
		m_Multisampling.flags = 0;
		m_Multisampling.pNext = nullptr;

		m_DepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		m_DepthStencil.depthTestEnable = VK_TRUE;
		m_DepthStencil.depthWriteEnable = VK_FALSE;
		m_DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		m_DepthStencil.depthBoundsTestEnable = VK_FALSE;
		m_DepthStencil.stencilTestEnable = VK_FALSE;
		m_DepthStencil.front.failOp = VK_STENCIL_OP_KEEP;
		m_DepthStencil.front.passOp = VK_STENCIL_OP_KEEP;
		m_DepthStencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
		m_DepthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
		m_DepthStencil.back = m_DepthStencil.front;

		m_DepthStencil.flags = 0;
		m_DepthStencil.pNext = nullptr;

		m_ColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		m_ColorBlendAttachmentState.blendEnable = VK_FALSE;
		m_ColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		m_ColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		m_ColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		m_ColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		m_ColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		m_ColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		m_ColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		m_ColorBlending.logicOpEnable = VK_FALSE;
		m_ColorBlending.logicOp = VK_LOGIC_OP_COPY;
		m_ColorBlending.attachmentCount = 1;
		m_ColorBlending.pAttachments = &m_ColorBlendAttachmentState;
		m_ColorBlending.blendConstants[0] = 0.0f;
		m_ColorBlending.blendConstants[1] = 0.0f;
		m_ColorBlending.blendConstants[2] = 0.0f;
		m_ColorBlending.blendConstants[3] = 0.0f;
		m_ColorBlending.flags = 0;
		m_ColorBlending.pNext = nullptr;

		m_DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };

		m_DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		m_DynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(m_DynamicStates.size());
		m_DynamicStateInfo.pDynamicStates = m_DynamicStates.data();
		m_DynamicStateInfo.pNext = nullptr;
		m_DynamicStateInfo.flags = 0;
	}

	ForwardPipelineInfo::~ForwardPipelineInfo()
	{
	}

	void ForwardPipelineInfo::Cleanup(const Wrappers::Device& device)
	{
		m_VertexShader->Cleanup(device);
		m_FragmentShader->Cleanup(device);
	}

	const uint32_t ForwardPipelineInfo::GetStagesCount() const
	{
		return static_cast<uint32_t>(m_Stages.size());
	}
	const VkPipelineShaderStageCreateInfo* ForwardPipelineInfo::GetStages() const
	{
		return m_Stages.data();
	}
	const VkPipelineVertexInputStateCreateInfo* ForwardPipelineInfo::GetVertexInputInfo() const
	{
		return &m_VertexInputInfo;
	}
	const VkPipelineInputAssemblyStateCreateInfo* ForwardPipelineInfo::GetInputAssemblyInfo() const
	{
		return &m_InputAssembly;
	}
	const VkPipelineViewportStateCreateInfo* ForwardPipelineInfo::GetViewportInfo() const
	{
		return &m_ViewportInfo;
	}
	const VkPipelineRasterizationStateCreateInfo* ForwardPipelineInfo::GetRasterizationInfo() const
	{
		return &m_RasterizerInfo;
	}
	const VkPipelineMultisampleStateCreateInfo* ForwardPipelineInfo::GetMultisamplingInfo() const
	{
		return &m_Multisampling;
	}
	const VkPipelineDepthStencilStateCreateInfo* ForwardPipelineInfo::GetDepthStencilInfo() const
	{
		return &m_DepthStencil;
	}
	const VkPipelineColorBlendStateCreateInfo* ForwardPipelineInfo::GetColorBlendingInfo() const
	{
		return &m_ColorBlending;
	}
	const VkPipelineDynamicStateCreateInfo* ForwardPipelineInfo::GetDynamicStateInfo() const
	{
		return &m_DynamicStateInfo;
	}
}


