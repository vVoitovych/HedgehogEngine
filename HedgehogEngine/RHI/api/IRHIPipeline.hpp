#pragma once

#include "RHITypes.hpp"

#include <vector>

namespace RHI
{

class IRHIShader;
class IRHIDescriptorSetLayout;

struct GraphicsPipelineDesc
{
    // Vertex shader is required. Fragment shader may be null for depth-only passes.
    IRHIShader*                           VertexShader   = nullptr;
    IRHIShader*                           FragmentShader = nullptr;

    // Vertex input
    std::vector<VertexBinding>            VertexBindings;
    std::vector<VertexAttribute>          VertexAttributes;

    // Input assembly
    PrimitiveTopology                     Topology  = PrimitiveTopology::TriangleList;

    // Rasterization
    RHI::CullMode                         CullMode  = RHI::CullMode::Back;
    RHI::FillMode                         FillMode  = RHI::FillMode::Solid;
    float                                 LineWidth = 1.0f;

    // Depth / stencil
    bool                                  DepthTestEnable  = true;
    bool                                  DepthWriteEnable = true;
    CompareOp                             DepthCompareOp   = CompareOp::Less;
    bool                                  StencilTestEnable = false;

    // Color blending (one entry per color attachment in the render pass)
    std::vector<ColorBlendAttachment>     ColorBlendAttachments;

    // Resource binding layout
    std::vector<const IRHIDescriptorSetLayout*> DescriptorSetLayouts;
    std::vector<PushConstantRange>        PushConstantRanges;

    // The attachments of the BeginRendering scope the pipeline is used in: ColorAttachmentFormats,
    // and DepthAttachmentFormat if that scope has a depth attachment.
    std::vector<RHI::Format>              ColorAttachmentFormats;
    RHI::Format                           DepthAttachmentFormat = RHI::Format::Undefined;
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
