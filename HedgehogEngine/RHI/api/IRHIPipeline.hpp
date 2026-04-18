#pragma once

#include "RHITypes.hpp"

#include <vector>

namespace RHI
{

class IRHIShader;
class IRHIDescriptorSetLayout;
class IRHIRenderPass;

struct GraphicsPipelineDesc
{
    // Vertex shader is required. Fragment shader may be null for depth-only passes.
    IRHIShader*                           m_VertexShader   = nullptr;
    IRHIShader*                           m_FragmentShader = nullptr;

    // Vertex input
    std::vector<VertexBinding>            m_VertexBindings;
    std::vector<VertexAttribute>          m_VertexAttributes;

    // Input assembly
    PrimitiveTopology                     m_Topology  = PrimitiveTopology::TriangleList;

    // Rasterization
    CullMode                              m_CullMode  = CullMode::Back;
    FillMode                              m_FillMode  = FillMode::Solid;
    float                                 m_LineWidth = 1.0f;

    // Depth / stencil
    bool                                  m_DepthTestEnable  = true;
    bool                                  m_DepthWriteEnable = true;
    CompareOp                             m_DepthCompareOp   = CompareOp::Less;
    bool                                  m_StencilTestEnable = false;

    // Color blending (one entry per color attachment in the render pass)
    std::vector<ColorBlendAttachment>     m_ColorBlendAttachments;

    // Resource binding layout
    std::vector<const IRHIDescriptorSetLayout*> m_DescriptorSetLayouts;
    std::vector<PushConstantRange>        m_PushConstantRanges;

    // Render pass this pipeline is compatible with
    const IRHIRenderPass*                 m_RenderPass = nullptr;
    uint32_t                              m_Subpass    = 0;
};

class IRHIPipeline
{
public:
    virtual ~IRHIPipeline() = default;

    IRHIPipeline(const IRHIPipeline&)            = delete;
    IRHIPipeline& operator=(const IRHIPipeline&) = delete;
    IRHIPipeline(IRHIPipeline&&)                 = delete;
    IRHIPipeline& operator=(IRHIPipeline&&)      = delete;

protected:
    IRHIPipeline() = default;
};

} // namespace RHI
