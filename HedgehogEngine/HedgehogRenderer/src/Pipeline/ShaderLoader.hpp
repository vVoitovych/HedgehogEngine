#pragma once

#include "PipelineLoader.hpp"
#include "VertexDescLoader.hpp"

#include "RHI/api/IRHIShader.hpp"

#include <memory>
#include <string>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{

// All data derived from a single .shader file: pipeline layout, vertex description,
// and compiled shader stage objects. Shader objects are valid until this struct is
// destroyed — the graphics pipeline must be created before that happens.
struct ShaderPipelineDesc
{
    PipelineFileDesc m_Layout;
    VertexFileDesc   m_VertexDesc;

    std::unique_ptr<RHI::IRHIShader> m_VertexShader;    // always present
    std::unique_ptr<RHI::IRHIShader> m_FragmentShader;  // null for depth-only passes
};

class ShaderLoader
{
public:
    // Load a .shader file and all assets it references (layout, vertex desc, SPIR-V).
    // shaderPath must be repo-root-relative and begin with '/'.
    // e.g. "/HedgehogEngine/HedgehogRenderer/Assets/Shaders/ForwardPass.shader"
    //
    // Paths inside the .shader file are resolved relative to the .shader file's directory.
    static ShaderPipelineDesc Load(RHI::IRHIDevice& device, const std::string& shaderPath);

private:
    static RHI::ShaderStage ParseStage(const std::string& s);
};

} // namespace Renderer
