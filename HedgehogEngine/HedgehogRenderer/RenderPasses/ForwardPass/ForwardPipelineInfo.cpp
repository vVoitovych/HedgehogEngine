#include "ForwardPipelineInfo.hpp"

#include "HedgehogWrappers/Wrappeers/Shaders/VertexShader.hpp"
#include "HedgehogWrappers/Wrappeers/Shaders/FragmentShader.hpp"

#include "Logger/Logger.hpp"

namespace Renderer
{
	ForwardPipelineInfo::ForwardPipelineInfo(const Wrappers::Device& device)
		: Wrappers::PipelineInfo(device)
	{
		mVertexShader = std::make_unique<Wrappers::VertexShader>(device, "ForwardPass/Base.vert");
		mFragmentShader = std::make_unique<Wrappers::FragmentShader>(device, "ForwardPass/Base.frag");
		mStages = {mVertexShader->GetCreateInfo(), mFragmentShader->GetCreateInfo()};

		mBindingDesc[0].binding = 0;
		mBindingDesc[0].stride = 3 * sizeof(float);
		mBindingDesc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		mBindingDesc[1].binding = 1;
		mBindingDesc[1].stride = 2 * sizeof(float);
		mBindingDesc[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		mBindingDesc[2].binding = 2;
		mBindingDesc[2].stride = 3 * sizeof(float);
		mBindingDesc[2].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		mAttributeDesc[0].binding = 0;
		mAttributeDesc[0].location = 0;
		mAttributeDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		mAttributeDesc[0].offset = 0;

		mAttributeDesc[1].binding = 1;
		mAttributeDesc[1].location = 1;
		mAttributeDesc[1].format = VK_FORMAT_R32G32_SFLOAT;
		mAttributeDesc[1].offset = 0;

		mAttributeDesc[2].binding = 2;
		mAttributeDesc[2].location = 2;
		mAttributeDesc[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		mAttributeDesc[2].offset = 0;


		mVertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		mVertexInputInfo.vertexBindingDescriptionCount = mBindingDesc.size();
		mVertexInputInfo.pVertexBindingDescriptions = mBindingDesc.data();
		mVertexInputInfo.vertexAttributeDescriptionCount = mAttributeDesc.size();
		mVertexInputInfo.pVertexAttributeDescriptions = mAttributeDesc.data();
		mVertexInputInfo.flags = 0;
		mVertexInputInfo.pNext = nullptr;

		mInputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		mInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		mInputAssembly.primitiveRestartEnable = VK_FALSE;
		mInputAssembly.pNext = nullptr;
		mInputAssembly.flags = 0;

		mViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		mViewportInfo.viewportCount = 1;
		mViewportInfo.pViewports = nullptr;
		mViewportInfo.scissorCount = 1;
		mViewportInfo.pScissors = nullptr;
		mViewportInfo.flags = 0;
		mViewportInfo.pNext = nullptr;

		mRasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		mRasterizerInfo.depthClampEnable = VK_FALSE;
		mRasterizerInfo.depthBiasClamp = 0.0f;
		mRasterizerInfo.rasterizerDiscardEnable = VK_FALSE;
		mRasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;
		mRasterizerInfo.lineWidth = 1.0f;
		mRasterizerInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
		mRasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
		mRasterizerInfo.depthBiasEnable = VK_FALSE;
		mRasterizerInfo.flags = 0;
		mRasterizerInfo.pNext = nullptr;

		mMultisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		mMultisampling.sampleShadingEnable = VK_FALSE;
		mMultisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		mMultisampling.minSampleShading = 1.0f;
		mMultisampling.pSampleMask = nullptr;
		mMultisampling.alphaToCoverageEnable = VK_FALSE;
		mMultisampling.alphaToOneEnable = VK_FALSE;
		mMultisampling.flags = 0;
		mMultisampling.pNext = nullptr;

		mDepthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		mDepthStencil.depthTestEnable = VK_TRUE;
		mDepthStencil.depthWriteEnable = VK_FALSE;
		mDepthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		mDepthStencil.depthBoundsTestEnable = VK_FALSE;
		mDepthStencil.stencilTestEnable = VK_FALSE;
		mDepthStencil.front.failOp = VK_STENCIL_OP_KEEP;
		mDepthStencil.front.passOp = VK_STENCIL_OP_KEEP;
		mDepthStencil.front.depthFailOp = VK_STENCIL_OP_KEEP;
		mDepthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
		mDepthStencil.back = mDepthStencil.front;

		mDepthStencil.flags = 0;
		mDepthStencil.pNext = nullptr;

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
		mColorBlending.flags = 0;
		mColorBlending.pNext = nullptr;

		mDynamicStates = { VK_DYNAMIC_STATE_VIEWPORT ,VK_DYNAMIC_STATE_SCISSOR };

		mDynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		mDynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(mDynamicStates.size());
		mDynamicStateInfo.pDynamicStates = mDynamicStates.data();
		mDynamicStateInfo.pNext = nullptr;
		mDynamicStateInfo.flags = 0;
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


