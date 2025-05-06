#include "DepthPrePassPipelineInfo.hpp"

#include "HedgehogWrappers/Wrappeers/Shaders/VertexShader.hpp"

namespace Renderer
{
	DepthPrePassPipelineInfo::DepthPrePassPipelineInfo(const Wrappers::Device& device)
		: Wrappers::PipelineInfo(device)
	{
		m_VertexShader = std::make_unique<Wrappers::VertexShader>(device, "DepthPrepass/Base.vert");
		m_Stages = { m_VertexShader->GetCreateInfo() };

		m_BindingDesc[0].binding = 0;
		m_BindingDesc[0].stride = 3 * sizeof(float);
		m_BindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		m_AttributeDesc[0].binding = 0;
		m_AttributeDesc[0].location = 0;
		m_AttributeDesc[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		m_AttributeDesc[0].offset = 0;

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
		m_DepthStencil.depthWriteEnable = VK_TRUE;
		m_DepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		m_DepthStencil.depthBoundsTestEnable = VK_FALSE;
		m_DepthStencil.stencilTestEnable = VK_FALSE;
		m_DepthStencil.front.failOp = VK_STENCIL_OP_KEEP;
		m_DepthStencil.front.passOp = VK_STENCIL_OP_KEEP;
		m_DepthStencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
		m_DepthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
		m_DepthStencil.back = m_DepthStencil.front;

		m_DepthStencil.flags = 0;
		m_DepthStencil.pNext = nullptr;


		m_DynamicStates = { VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };

		m_DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		m_DynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(m_DynamicStates.size());
		m_DynamicStateInfo.pDynamicStates = m_DynamicStates.data();
		m_DynamicStateInfo.pNext = nullptr;
		m_DynamicStateInfo.flags = 0;
	}

	DepthPrePassPipelineInfo::~DepthPrePassPipelineInfo()
	{
	}

	void DepthPrePassPipelineInfo::Cleanup(const Wrappers::Device& device)
	{
		m_VertexShader->Cleanup(device);
	}

	const uint32_t DepthPrePassPipelineInfo::GetStagesCount() const
	{
		return static_cast<uint32_t>(m_Stages.size());
	}
	const VkPipelineShaderStageCreateInfo* DepthPrePassPipelineInfo::GetStages() const
	{
		return m_Stages.data();
	}
	const VkPipelineVertexInputStateCreateInfo* DepthPrePassPipelineInfo::GetVertexInputInfo() const
	{
		return &m_VertexInputInfo;
	}
	const VkPipelineInputAssemblyStateCreateInfo* DepthPrePassPipelineInfo::GetInputAssemblyInfo() const
	{
		return &m_InputAssembly;
	}
	const VkPipelineViewportStateCreateInfo* DepthPrePassPipelineInfo::GetViewportInfo() const
	{
		return &m_ViewportInfo;
	}
	const VkPipelineRasterizationStateCreateInfo* DepthPrePassPipelineInfo::GetRasterizationInfo() const
	{
		return &m_RasterizerInfo;
	}
	const VkPipelineMultisampleStateCreateInfo* DepthPrePassPipelineInfo::GetMultisamplingInfo() const
	{
		return &m_Multisampling;
	}
	const VkPipelineDepthStencilStateCreateInfo* DepthPrePassPipelineInfo::GetDepthStencilInfo() const
	{
		return &m_DepthStencil;
	}
	const VkPipelineColorBlendStateCreateInfo* DepthPrePassPipelineInfo::GetColorBlendingInfo() const
	{
		return nullptr;
	}
	const VkPipelineDynamicStateCreateInfo* DepthPrePassPipelineInfo::GetDynamicStateInfo() const
	{
		return &m_DynamicStateInfo;
	}
}


