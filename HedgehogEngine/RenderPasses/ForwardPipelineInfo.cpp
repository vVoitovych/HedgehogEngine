#include "ForwardPipelineInfo.hpp"

#include "Wrappeers/Shaders/VertexShader.hpp"
#include "Wrappeers/Shaders/FragmentShader.hpp"
#include "Containers/VertexDescription.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	ForwardPipelineInfo::ForwardPipelineInfo(const Wrappers::Device& device)
		: Wrappers::PipelineInfo(device)
	{
		mVertexShader = std::make_unique<Wrappers::VertexShader>(device, "SimpleShader.vert.spv");
		mFragmentShader = std::make_unique<Wrappers::FragmentShader>(device, "SimpleShader.frag.spv");
		mStages = {mVertexShader->GetCreateInfo(), mFragmentShader->GetCreateInfo()};

		mBindingDesc = VertexDescription::GetBindingDescription();
		mAttributeDesc = VertexDescription::GetAttributeDescription();

		mVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		mVertexInputInfo.vertexBindingDescriptionCount = 1;
		mVertexInputInfo.pVertexBindingDescriptions = &mBindingDesc;
		mVertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(mAttributeDesc.size());
		mVertexInputInfo.pVertexAttributeDescriptions = mAttributeDesc.data();

		mInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		mInputAssembly.primitiveRestartEnable = VK_FALSE;

		mViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		mViewportInfo.viewportCount = 1;
		mViewportInfo.pViewports = nullptr;
		mViewportInfo.scissorCount = 1;
		mViewportInfo.pScissors = nullptr;

		mRasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mRasterizerInfo.depthClampEnable = VK_FALSE;
		mRasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
		mRasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
		mRasterizerInfo.lineWidth = 1.0f;
		mRasterizerInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
		mRasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		mRasterizerInfo.depthBiasEnable = VK_FALSE;

		mMultisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		mMultisampling.sampleShadingEnable = VK_FALSE;
		mMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		mDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		mDepthStencil.depthTestEnable = VK_TRUE;
		mDepthStencil.depthWriteEnable = VK_TRUE;
		mDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		mDepthStencil.depthBoundsTestEnable = VK_FALSE;
		mDepthStencil.stencilTestEnable = VK_FALSE;

		mColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		mColorBlendAttachmentState.blendEnable = VK_FALSE;
		mColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		mColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		mColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
		mColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		mColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		mColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

		mColorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		mColorBlending.logicOpEnable = VK_FALSE;
		mColorBlending.logicOp = VK_LOGIC_OP_COPY;
		mColorBlending.attachmentCount = 1;
		mColorBlending.pAttachments = &mColorBlendAttachmentState;
		mColorBlending.blendConstants[0] = 0.0f;
		mColorBlending.blendConstants[1] = 0.0f;
		mColorBlending.blendConstants[2] = 0.0f;
		mColorBlending.blendConstants[3] = 0.0f;

		mDynamicStates = { VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };

		mDynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		mDynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(mDynamicStates.size());
		mDynamicStateInfo.pDynamicStates = mDynamicStates.data();
	}

	ForwardPipelineInfo::~ForwardPipelineInfo()
	{
	}

	void ForwardPipelineInfo::Cleanup(const Wrappers::Device& device)
	{
		mVertexShader->Cleanup(device);
		mFragmentShader->Cleanup(device);
	}

	const uint32_t ForwardPipelineInfo::GetStagesCount() const
	{
		return static_cast<uint32_t>(mStages.size());
	}
	const VkPipelineShaderStageCreateInfo* ForwardPipelineInfo::GetStages() const
	{
		return mStages.data();
	}
	const VkPipelineVertexInputStateCreateInfo* ForwardPipelineInfo::GetVertexInputInfo() const
	{
		return &mVertexInputInfo;
	}
	const VkPipelineInputAssemblyStateCreateInfo* ForwardPipelineInfo::GetInputAssemblyInfo() const
	{
		return &mInputAssembly;
	}
	const VkPipelineViewportStateCreateInfo* ForwardPipelineInfo::GetViewportInfo() const
	{
		return &mViewportInfo;
	}
	const VkPipelineRasterizationStateCreateInfo* ForwardPipelineInfo::GetRasterizationInfo() const
	{
		return &mRasterizerInfo;
	}
	const VkPipelineMultisampleStateCreateInfo* ForwardPipelineInfo::GetMultisamplingInfo() const
	{
		return &mMultisampling;
	}
	const VkPipelineDepthStencilStateCreateInfo* ForwardPipelineInfo::GetDepthStencilInfo() const
	{
		return &mDepthStencil;
	}
	const VkPipelineColorBlendStateCreateInfo* ForwardPipelineInfo::GetColorBlendingInfo() const
	{
		return &mColorBlending;
	}
	const VkPipelineDynamicStateCreateInfo* ForwardPipelineInfo::GetDynamicStateInfo() const
	{
		return &mDynamicStateInfo;
	}
}


