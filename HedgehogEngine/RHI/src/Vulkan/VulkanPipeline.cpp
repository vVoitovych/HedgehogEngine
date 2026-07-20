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
    assert(desc.VertexShader && "GraphicsPipelineDesc::VertexShader must not be null.");
    assert(desc.RenderPass   && "GraphicsPipelineDesc::RenderPass must not be null.");

    // ── Shader stages ─────────────────────────────────────────────────────────

    const auto& vertShader = static_cast<const VulkanShader&>(*desc.VertexShader);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(2);

    {
        VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stage.module = vertShader.GetHandle();
        stage.pName  = "main";
        stages.push_back(stage);
    }

    if (desc.FragmentShader)
    {
        const auto& fragShader = static_cast<const VulkanShader&>(*desc.FragmentShader);
        VkPipelineShaderStageCreateInfo stage{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO };
        stage.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage.module = fragShader.GetHandle();
        stage.pName  = "main";
        stages.push_back(stage);
    }

    // ── Vertex input ──────────────────────────────────────────────────────────

    std::vector<VkVertexInputBindingDescription> bindings;
    bindings.reserve(desc.VertexBindings.size());
    for (const auto& b : desc.VertexBindings)
        bindings.push_back({ b.Binding, b.Stride, VulkanTypes::ToVkInputRate(b.InputRate) });

    std::vector<VkVertexInputAttributeDescription> attributes;
    attributes.reserve(desc.VertexAttributes.size());
    for (const auto& a : desc.VertexAttributes)
        attributes.push_back({ a.Location, a.Binding,
                                VulkanTypes::ToVkFormat(a.Format), a.Offset });

    VkPipelineVertexInputStateCreateInfo vertexInput{ VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInput.vertexBindingDescriptionCount   = static_cast<uint32_t>(bindings.size());
    vertexInput.pVertexBindingDescriptions      = bindings.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions    = attributes.data();

    // ── Input assembly ────────────────────────────────────────────────────────

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{ VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    inputAssembly.topology               = VulkanTypes::ToVkTopology(desc.Topology);
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
    rasterization.polygonMode             = VulkanTypes::ToVkFillMode(desc.FillMode);
    rasterization.cullMode                = VulkanTypes::ToVkCullMode(desc.CullMode);
    rasterization.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthBiasEnable         = VK_FALSE;
    rasterization.lineWidth               = desc.LineWidth;

    // ── Multisampling ─────────────────────────────────────────────────────────

    VkPipelineMultisampleStateCreateInfo multisampling{ VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // ── Depth / stencil ───────────────────────────────────────────────────────

    VkPipelineDepthStencilStateCreateInfo depthStencil{ VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
    depthStencil.depthTestEnable       = desc.DepthTestEnable  ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable      = desc.DepthWriteEnable ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp        = VulkanTypes::ToVkCompareOp(desc.DepthCompareOp);
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable     = desc.StencilTestEnable ? VK_TRUE : VK_FALSE;

    // ── Color blend ───────────────────────────────────────────────────────────

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    blendAttachments.reserve(desc.ColorBlendAttachments.size());

    for (const auto& blend : desc.ColorBlendAttachments)
    {
        VkPipelineColorBlendAttachmentState state{};
        state.blendEnable         = blend.BlendEnable ? VK_TRUE : VK_FALSE;
        state.srcColorBlendFactor = VulkanTypes::ToVkBlendFactor(blend.SrcColorFactor);
        state.dstColorBlendFactor = VulkanTypes::ToVkBlendFactor(blend.DstColorFactor);
        state.colorBlendOp        = VulkanTypes::ToVkBlendOp(blend.ColorOp);
        state.srcAlphaBlendFactor = VulkanTypes::ToVkBlendFactor(blend.SrcAlphaFactor);
        state.dstAlphaBlendFactor = VulkanTypes::ToVkBlendFactor(blend.DstAlphaFactor);
        state.alphaBlendOp        = VulkanTypes::ToVkBlendOp(blend.AlphaOp);
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
    setLayouts.reserve(desc.DescriptorSetLayouts.size());
    for (const auto* layout : desc.DescriptorSetLayouts)
    {
        assert(layout && "Null descriptor set layout in GraphicsPipelineDesc.");
        setLayouts.push_back(static_cast<const VulkanDescriptorSetLayout&>(*layout).GetHandle());
    }

    std::vector<VkPushConstantRange> pushRanges;
    pushRanges.reserve(desc.PushConstantRanges.size());
    for (const auto& range : desc.PushConstantRanges)
        pushRanges.push_back({ VulkanTypes::ToVkShaderStage(range.Stages),
                                range.Offset, range.Size });

    VkPipelineLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    layoutInfo.setLayoutCount         = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts            = setLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushRanges.size());
    layoutInfo.pPushConstantRanges    = pushRanges.data();

    VkResult result = vkCreatePipelineLayout(m_Device.GetHandle(), &layoutInfo, nullptr, &m_Layout);
    assert(result == VK_SUCCESS && "Failed to create VkPipelineLayout.");

    // ── Graphics pipeline ─────────────────────────────────────────────────────

    const auto& vkPass = static_cast<const VulkanRenderPass&>(*desc.RenderPass);

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
    pipelineInfo.subpass             = desc.Subpass;

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
