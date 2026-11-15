#pragma once

#include "PipelineLoader.hpp"

#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIShader.hpp"

#include "FileSystem/api/FileSystemManager.hpp"

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
// The caller must set m_Pipeline.DescriptorSetLayouts and the attachment formats
// before passing it to device.CreateGraphicsPipeline().
// m_VertexShader / m_FragmentShader keep the shader objects alive; their raw pointers
// are already stored in m_Pipeline.VertexShader / m_Pipeline.FragmentShader.
struct ShaderPipelineDesc
{
    PipelineFileDesc           Layout;
    RHI::GraphicsPipelineDesc  Pipeline;

    std::unique_ptr<RHI::IRHIShader> VertexShader;    // always present
    std::unique_ptr<RHI::IRHIShader> FragmentShader;  // null for depth-only passes
};

// All data derived from a compute .shader file (ComputeShaderFile.hpp). Pipeline holds the push
// constants and the shader; the caller sets DescriptorSetLayouts from Layout before passing it to
// device.CreateComputePipeline(). ComputeShader is null when the file was refused (logged).
struct ComputeShaderDesc
{
    PipelineFileDesc                 Layout;
    RHI::ComputePipelineDesc         Pipeline;
    std::unique_ptr<RHI::IRHIShader> ComputeShader;
};

class ShaderLoader
{
public:
    // Load a .shader file and all assets it references (layout, vertex desc, SPIR-V).
    // shaderVirtualPath must be a virtual path, e.g. "engine://HedgehogEngine/.../GraphForward.shader".
    static ShaderPipelineDesc Load(RHI::IRHIDevice& device,
                                   const std::string& shaderVirtualPath,
                                   const FS::FileSystemManager& fileSystem);

    // Load a compute .shader file, its layout and its SPIR-V; one error naming the file, and no
    // shader, when it cannot be read or is not a compute shader.
    static ComputeShaderDesc LoadCompute(RHI::IRHIDevice& device,
                                         const std::string& shaderVirtualPath,
                                         const FS::FileSystemManager& fileSystem);

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
