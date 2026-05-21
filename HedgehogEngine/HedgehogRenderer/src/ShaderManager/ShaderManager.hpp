#pragma once

#include "RHI/api/IRHIPipeline.hpp"
#include "RHI/api/IRHIShader.hpp"
#include "RHI/api/RHITypes.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RHI
{
    class IRHIDevice;
}

namespace Renderer
{

// Parsed content of a .pl pipeline layout file.
struct PipelineFileDesc
{
    std::vector<std::vector<RHI::DescriptorBinding>> m_DescriptorSets;
    std::vector<RHI::PushConstantRange>              m_PushConstants;
};

// Describes a loaded .shader file. Shader pointers are non-owning — lifetime is
// managed by ShaderManager's cache and valid until ShaderManager::Cleanup().
struct ShaderPipelineDesc
{
    PipelineFileDesc           m_Layout;
    RHI::GraphicsPipelineDesc  m_Pipeline;

    RHI::IRHIShader*           m_VertexShader   = nullptr;
    RHI::IRHIShader*           m_FragmentShader = nullptr;
};

class ShaderManager
{
public:
    explicit ShaderManager(RHI::IRHIDevice& device);
    void Cleanup();

    // Returns a cached shader or loads it from disk. The reference is valid until Cleanup().
    RHI::IRHIShader& GetOrLoad(const std::string& spirvPath, RHI::ShaderStage stage);

    // Returns a pointer to a cached shader, or nullptr if it has not been loaded yet.
    const RHI::IRHIShader* TryGet(const std::string& spirvPath, RHI::ShaderStage stage) const;

    // Parse a .shader YAML file and return a fully-populated ShaderPipelineDesc.
    // shaderPath must begin with '/' and be repo-root-relative.
    ShaderPipelineDesc LoadShaderFile(const std::string& shaderPath);

private:
    static std::string MakeCacheKey(const std::string& path, RHI::ShaderStage stage);

    static RHI::ShaderStage       ParseStage(const std::string& s);
    static RHI::PrimitiveTopology ParseTopology(const std::string& s);
    static RHI::CullMode          ParseCullMode(const std::string& s);
    static RHI::FillMode          ParseFillMode(const std::string& s);
    static RHI::CompareOp         ParseCompareOp(const std::string& s);
    static RHI::BlendFactor       ParseBlendFactor(const std::string& s);
    static RHI::BlendOp           ParseBlendOp(const std::string& s);

    static RHI::DescriptorType ParseDescriptorType(const std::string& s);
    static RHI::ShaderStage    ParseDescriptorStage(const std::string& s);
    static RHI::Format         ParseVertexFormat(const std::string& s);
    static RHI::VertexInputRate ParseVertexInputRate(const std::string& s);

    PipelineFileDesc LoadPipelineLayout(const std::string& layoutPath);

    RHI::IRHIDevice&                                               m_Device;
    std::unordered_map<std::string, std::unique_ptr<RHI::IRHIShader>> m_Cache;
};

} // namespace Renderer
