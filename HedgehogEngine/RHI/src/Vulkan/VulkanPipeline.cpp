#include "VulkanPipeline.hpp"

#include "VulkanDescriptor.hpp"
#include "VulkanDevice.hpp"
#include "VulkanRenderPass.hpp"
#include "VulkanShader.hpp"
#include "VulkanTypes.hpp"

#include <cassert>
#include <vector>

namespace RHI
{

VulkanPipeline::VulkanPipeline(VulkanDevice& device, const GraphicsPipelineDesc& desc)
    : m_Device(device)
{
    assert(desc.m_VertexShader && "GraphicsPipelineDesc::m_VertexShader must not be null.");
    assert(desc.m_RenderPass   && "GraphicsPipelineDesc::m_RenderPass must not be null.");

    // ── Shader stages ─────────────────────────────────────────────────────────

    const auto& vertShader = static_cast<const VulkanShader&>(*desc.m_VertexShader);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(2);

    {
        VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stage.module = vertShader.GetHandle();
        stage.pName  = "main";
        stages.push_back(stage);
    }

    if (desc.m_FragmentShader)
    {
        const auto& fragShader = static_cast<const VulkanShader&>(*desc.m_FragmentShader);
        VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage.module = fragShader.GetHandle();
        stage.pName  = "main";
        stages.push_back(stage);
    }

    // ── Vertex input ──────────────────────────────────────────────────────────

    std::vector<VkVertexInputBindingDescription> bindings;
    bindings.reserve(desc.m_VertexBindings.size());
    for (const auto& b : desc.m_VertexBindings)
        bindings.push_back({ b.m_Binding, b.m_Stride, VulkanTypes::ToVkInputRate(b.m_InputRate) });

    std::vector<VkVertexInputAttributeDescription> attributes;
    attributes.reserve(desc.m_VertexAttributes.size());
    for (const auto& a : desc.m_VertexAttributes)
        attributes.push_back({ a.m_Location, a.m_Binding,
                                VulkanTypes::ToVkFormat(a.m_Format), a.m_Offset });

    VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInput.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions      = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions    = attributes.data();

    // ── Input assembly ────────────────────────────────────────────────────────

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology               = VulkanTypes::ToVkTopology(desc.m_Topology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ── Dynamic viewport / scissor (set at draw time) ─────────────────────────

    VkPipelineViewportStateCreateInfo viewportState{ VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    constexpr VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{ VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates    = dynamicStates;

    // ── Rasterization ─────────────────────────────────────────────────────────

    VkPipelineRasterizationStateCreateInfo rasterization{ VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
    rasterization.depthClampEnable        = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.polygonMode             = VulkanTypes::ToVkFillMode(desc.m_FillMode);
    rasterization.cullMode                = VulkanTypes::ToVkCullMode(desc.m_CullMode);
    rasterization.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable         = VK_FALSE;
    rasterization.lineWidth               = desc.m_LineWidth;

    // ── Multisampling ─────────────────────────────────────────────────────────

    VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ── Depth / stencil ───────────────────────────────────────────────────────

    VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencil.depthTestEnable       = desc.m_DepthTestEnable  ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable      = desc.m_DepthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp        = VulkanTypes::ToVkCompareOp(desc.m_DepthCompareOp);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = desc.m_StencilTestEnable ? VK_TRUE : VK_FALSE;

    // ── Color blend ───────────────────────────────────────────────────────────

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    blendAttachments.reserve(desc.m_ColorBlendAttachments.size());

    for (const auto& blend : desc.m_ColorBlendAttachments)
    {
        VkPipelineColorBlendAttachmentState state{};
        state.blendEnable         = blend.m_BlendEnable ? VK_TRUE : VK_FALSE;
        state.srcColorBlendFactor = VulkanTypes::ToVkBlendFactor(blend.m_SrcColorFactor);
        state.dstColorBlendFactor = VulkanTypes::ToVkBlendFactor(blend.m_DstColorFactor);
        state.colorBlendOp        = VulkanTypes::ToVkBlendOp(blend.m_ColorOp);
        state.srcAlphaBlendFactor = VulkanTypes::ToVkBlendFactor(blend.m_SrcAlphaFactor);
        state.dstAlphaBlendFactor = VulkanTypes::ToVkBlendFactor(blend.m_DstAlphaFactor);
        state.alphaBlendOp        = VulkanTypes::ToVkBlendOp(blend.m_AlphaOp);
        state.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                                  | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachments.push_back(state);
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{ VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    colorBlend.logicOpEnable     = VK_FALSE;
    colorBlend.attachmentCount   = static_cast<uint32_t>(blendAttachments.size());
    colorBlend.pAttachments      = blendAttachments.data();

    // ── Pipeline layout ───────────────────────────────────────────────────────

    std::vector<VkDescriptorSetLayout> setLayouts;
    setLayouts.reserve(desc.m_DescriptorSetLayouts.size());
    for (const auto* layout : desc.m_DescriptorSetLayouts)
    {
        assert(layout && "Null descriptor set layout in GraphicsPipelineDesc.");
        setLayouts.push_back(static_cast<const VulkanDescriptorSetLayout&>(*layout).GetHandle());
    }

    std::vector<VkPushConstantRange> pushRanges;
    pushRanges.reserve(desc.m_PushConstantRanges.size());
    for (const auto& range : desc.m_PushConstantRanges)
        pushRanges.push_back({ VulkanTypes::ToVkShaderStage(range.m_Stages),
                                range.m_Offset, range.m_Size });

    VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts            = setLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushRanges.size());
    layoutInfo.pPushConstantRanges    = pushRanges.data();

    VkResult result = vkCreatePipelineLayout(m_Device.GetHandle(), &layoutInfo, nullptr, &m_Layout);
    assert(result == VK_SUCCESS && "Failed to create VkPipelineLayout.");

    // ── Graphics pipeline ─────────────────────────────────────────────────────

    const auto& vkPass = static_cast<const VulkanRenderPass&>(*desc.m_RenderPass);

    VkGraphicsPipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
    pipelineInfo.stageCount          = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages             = stages.data();
    pipelineInfo.pVertexInputState   = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pDepthStencilState  = &depthStencil;
    pipelineInfo.pColorBlendState    = &colorBlend;
    pipelineInfo.pDynamicState       = &dynamicState;
    pipelineInfo.layout              = m_Layout;
    pipelineInfo.renderPass          = vkPass.GetHandle();
    pipelineInfo.subpass             = desc.m_Subpass;

    result = vkCreateGraphicsPipelines(
        m_Device.GetHandle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_Pipeline);
    assert(result == VK_SUCCESS && "Failed to create VkPipeline.");
}

VulkanPipeline::~VulkanPipeline()
{
    if (m_Pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_Device.GetHandle(), m_Pipeline, nullptr);
    if (m_Layout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(m_Device.GetHandle(), m_Layout, nullptr);
}

} // namespace RHI
