#pragma once

#include "PipelineLoader.hpp"

#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIShader.hpp"

#include <memory>
#include <string>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{

// All data derived from a single .shader file.
// m_Pipeline is pre-filled with every field the .shader file can describe
// (vertex input, topology, rasterization, depth, blend, push constants).
// The caller must set m_Pipeline.m_DescriptorSetLayouts and m_Pipeline.m_RenderPass
// before passing it to device.CreateGraphicsPipeline().
// m_VertexShader / m_FragmentShader keep the shader objects alive; their raw pointers
// are already stored in m_Pipeline.m_VertexShader / m_Pipeline.m_FragmentShader.
struct ShaderPipelineDesc
{
    PipelineFileDesc           m_Layout;
    RHI::GraphicsPipelineDesc  m_Pipeline;

    std::unique_ptr<RHI::IRHIShader> m_VertexShader;    // always present
    std::unique_ptr<RHI::IRHIShader> m_FragmentShader;  // null for depth-only passes
};

class ShaderLoader
{
public:
    // Load a .shader file and all assets it references (layout, vertex desc, SPIR-V).
    // shaderPath must be repo-root-relative and begin with '/'.
    static ShaderPipelineDesc Load(RHI::IRHIDevice& device, const std::string& shaderPath);

private:
    static RHI::ShaderStage        ParseStage(const std::string& s);
    static RHI::PrimitiveTopology  ParseTopology(const std::string& s);
    static RHI::CullMode           ParseCullMode(const std::string& s);
    static RHI::FillMode           ParseFillMode(const std::string& s);
    static RHI::CompareOp          ParseCompareOp(const std::string& s);
    static RHI::BlendFactor        ParseBlendFactor(const std::string& s);
    static RHI::BlendOp            ParseBlendOp(const std::string& s);
};

} // namespace Renderer
